#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vi.h"

#define EXLEN		512

int xrow, xoff, xtop;		/* current row, column, and top row */
int xquit;			/* exit if set */
int xvis;			/* visual mode */
int xai = 1;			/* autoindent option */
int xic = 1;			/* ignorecase option */
int xaw;			/* autowrite option */
int xled = 1;			/* use the line editor */
int xdir = +1;			/* current direction context */
int xshape = 1;			/* perform letter shaping */
int xorder = 1;			/* change the order of characters */

static struct buf {
	char ft[32];
	char *path;
	struct lbuf *lb;
	int row;
} bufs[8];

static int bufs_find(char *path)
{
	int i;
	for (i = 0; i < LEN(bufs); i++)
		if (bufs[i].lb && !strcmp(bufs[i].path, path))
			return i;
	return -1;
}

static void bufs_free(int idx)
{
	if (bufs[idx].lb) {
		free(bufs[idx].path);
		lbuf_free(bufs[idx].lb);
		memset(&bufs[idx], 0, sizeof(bufs[idx]));
	}
}

static int bufs_open(char *path)
{
	int i;
	for (i = 0; i < LEN(bufs) - 1; i++)
		if (!bufs[i].lb)
			break;
	bufs_free(i);
	bufs[i].path = uc_dup(path);
	bufs[i].lb = lbuf_make();
	bufs[i].row = 0;
	strcpy(bufs[i].ft, syn_filetype(path));
	return i;
}

static void bufs_swap(int i, int j)
{
	struct buf tmp;
	if (i == j)
		return;
	memcpy(&tmp, &bufs[i], sizeof(tmp));
	memcpy(&bufs[i], &bufs[j], sizeof(tmp));
	memcpy(&bufs[j], &tmp, sizeof(tmp));
}

static void bufs_switch(int idx)
{
	if (idx > 1)
		bufs_swap(0, 1);
	bufs_swap(0, idx);
	xrow = bufs[0].row;
}

char *ex_path(void)
{
	return bufs[0].path;
}

struct lbuf *ex_lbuf(void)
{
	return bufs[0].lb;
}

char *ex_filetype(void)
{
	return bufs[0].ft;
}

/* read ex command location */
static char *ex_loc(char *s, char *loc)
{
	while (*s == ':' || isspace((unsigned char) *s))
		s++;
	while (*s && !isalpha((unsigned char) *s) && *s != '=' && *s != '!') {
		if (*s == '\'')
			*loc++ = *s++;
		if (*s == '/' || *s == '?') {
			int d = *s;
			*loc++ = *s++;
			while (*s && *s != d) {
				if (*s == '\\' && s[1])
					*loc++ = *s++;
				*loc++ = *s++;
			}
		}
		*loc++ = *s++;
	}
	*loc = '\0';
	return s;
}

/* read ex command name */
static char *ex_cmd(char *s, char *cmd)
{
	char *cmd0 = cmd;
	s = ex_loc(s, cmd);
	while (isspace((unsigned char) *s))
		s++;
	while (isalpha((unsigned char) *s))
		if ((*cmd++ = *s++) == 'k' && cmd == cmd0 + 1)
			break;
	if (*s == '!' || *s == '=')
		*cmd++ = *s++;
	*cmd = '\0';
	return s;
}

/* read ex command argument */
static char *ex_arg(char *s, char *arg)
{
	s = ex_cmd(s, arg);
	while (isspace((unsigned char) *s))
		s++;
	while (*s && !isspace((unsigned char) *s))
		*arg++ = *s++;
	*arg = '\0';
	return s;
}

static int ex_search(char *pat)
{
	struct sbuf *kw;
	int dir = *pat == '/' ? 1 : -1;
	char *b = pat;
	char *e = b;
	char *re_kw[1];
	int i = xrow;
	struct rset *re;
	kw = sbuf_make();
	while (*++e) {
		if (*e == *pat)
			break;
		sbuf_chr(kw, (unsigned char) *e);
		if (*e == '\\' && e[1])
			e++;
	}
	re_kw[0] = sbuf_buf(kw);
	re = rset_make(1, re_kw, xic ? RE_ICASE : 0);
	sbuf_free(kw);
	if (!re)
		return i;
	while (i >= 0 && i < lbuf_len(xb)) {
		if (rset_find(re, lbuf_get(xb, i), 0, NULL, 0) >= 0)
			break;
		i += dir;
	}
	rset_free(re);
	return i;
}

static int ex_lineno(char *num)
{
	int n = xrow;
	if (!num[0] || num[0] == '.')
		n = xrow;
	if (isdigit(num[0]))
		n = atoi(num) - 1;
	if (num[0] == '$')
		n = lbuf_len(xb) - 1;
	if (num[0] == '-')
		n = xrow - (num[1] ? ex_lineno(num + 1) : 1);
	if (num[0] == '+')
		n = xrow + (num[1] ? ex_lineno(num + 1) : 1);
	if (num[0] == '\'')
		lbuf_jump(xb, num[1], &n, NULL);
	if (num[0] == '/' && num[1])
		n = ex_search(num);
	if (num[0] == '?' && num[1])
		n = ex_search(num);
	return n;
}

/* parse ex command location */
static int ex_region(char *loc, int *beg, int *end)
{
	int naddr = 0;
	if (!strcmp("%", loc)) {
		*beg = 0;
		*end = MAX(0, lbuf_len(xb));
		return 0;
	}
	if (!*loc) {
		*beg = xrow;
		*end = xrow == lbuf_len(xb) ? xrow : xrow + 1;
		return 0;
	}
	while (*loc) {
		int end0 = *end;
		*end = ex_lineno(loc) + 1;
		*beg = naddr++ ? end0 - 1 : *end - 1;
		if (!naddr++)
			*beg = *end - 1;
		while (*loc && *loc != ';' && *loc != ',')
			loc++;
		if (!*loc)
			break;
		if (*loc == ';')
			xrow = *end - 1;
		loc++;
	}
	if (*beg < 0 || *beg >= lbuf_len(xb))
		return 1;
	if (*end < *beg || *end > lbuf_len(xb))
		return 1;
	return 0;
}

static int ec_write(char *ec);

static int ex_modifiedbuffer(char *msg)
{
	if (!lbuf_modified(xb))
		return 0;
	if (xaw && ex_path()[0])
		return ec_write("w");
	if (msg)
		ex_show(msg);
	return 1;
}

static int ec_quit(char *ec)
{
	char cmd[EXLEN];
	ex_cmd(ec, cmd);
	if (!strchr(cmd, '!'))
		if (ex_modifiedbuffer("buffer modified\n"))
			return 1;
	xquit = 1;
	return 0;
}

static int ex_expand(char *d, char *s)
{
	while (*s) {
		int c = (unsigned char) *s++;
		if (c == '%') {
			if (!bufs[0].path || !bufs[0].path[0]) {
				ex_show("\"%\" is unset\n");
				return 1;
			}
			strcpy(d, bufs[0].path);
			d = strchr(d, '\0');
			continue;
		}
		if (c == '#') {
			if (!bufs[1].path || !bufs[1].path[0]) {
				ex_show("\"#\" is unset\n");
				return 1;
			}
			strcpy(d, bufs[1].path);
			d = strchr(d, '\0');
			continue;
		}
		if (c == '\\' && (*s == '%' || *s == '#'))
			c = *s++;
		*d++ = c;
	}
	*d = '\0';
	return 0;
}

static int ec_edit(char *ec)
{
	char msg[128];
	char arg[EXLEN], cmd[EXLEN];
	char path[PATHLEN];
	int fd;
	ex_cmd(ec, cmd);
	ex_arg(ec, arg);
	if (!strchr(cmd, '!'))
		if (xb && ex_modifiedbuffer("buffer modified\n"))
			return 1;
	if (ex_expand(path, arg))
		return 1;
	bufs[0].row = xrow;
	if (arg[0] && bufs_find(path) >= 0) {
		bufs_switch(bufs_find(path));
		return 0;
	}
	if (path[0] || !bufs[0].path)
		bufs_switch(bufs_open(path));
	fd = open(ex_path(), O_RDONLY);
	if (fd >= 0) {
		lbuf_rm(xb, 0, lbuf_len(xb));
		lbuf_rd(xb, fd, 0);
		close(fd);
		snprintf(msg, sizeof(msg), "\"%s\"  %d lines  [r]\n",
				ex_path(), lbuf_len(xb));
		ex_show(msg);
	}
	xrow = MAX(0, MIN(xrow, lbuf_len(xb) - 1));
	lbuf_modified(xb);
	lbuf_saved(xb, path[0] != '\0');
	return 0;
}

static int ec_read(char *ec)
{
	char arg[EXLEN], loc[EXLEN];
	char msg[128];
	char *path;
	int fd;
	int beg, end;
	int n = lbuf_len(xb);
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	path = arg[0] ? arg : ex_path();
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ex_show("read failed\n");
		return 1;
	}
	if (ex_region(loc, &beg, &end))
		return 1;
	lbuf_rd(xb, fd, lbuf_len(xb) ? end : 0);
	close(fd);
	xrow = end + lbuf_len(xb) - n;
	snprintf(msg, sizeof(msg), "\"%s\"  %d lines  [r]\n",
			path, lbuf_len(xb) - n);
	ex_show(msg);
	return 0;
}

static int ec_write(char *ec)
{
	char cmd[EXLEN], arg[EXLEN], loc[EXLEN];
	char msg[128];
	char *path;
	int beg, end;
	int fd;
	ex_cmd(ec, cmd);
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	path = arg[0] ? arg : ex_path();
	if (ex_region(loc, &beg, &end))
		return 1;
	if (!loc[0]) {
		beg = 0;
		end = lbuf_len(xb);
	}
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		ex_show("write failed\n");
		return 1;
	}
	lbuf_wr(xb, fd, beg, end);
	close(fd);
	snprintf(msg, sizeof(msg), "\"%s\"  %d lines  [w]\n",
			path, end - beg);
	ex_show(msg);
	if (!ex_path()[0]) {
		free(bufs[0].path);
		bufs[0].path = uc_dup(path);
	}
	if (!strcmp(ex_path(), path))
		lbuf_saved(xb, 0);
	if (!strcmp("wq", cmd))
		ec_quit("wq");
	return 0;
}

static int ec_insert(char *ec)
{
	char arg[EXLEN], cmd[EXLEN], loc[EXLEN];
	struct sbuf *sb;
	char *s;
	int beg, end;
	int n;
	ex_arg(ec, arg);
	ex_cmd(ec, cmd);
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end) && (beg != 0 || end != 0))
		return 1;
	if (cmd[0] == 'c') {
		if (lbuf_len(xb))
			lbuf_rm(xb, beg, end);
		end = beg + 1;
	}
	sb = sbuf_make();
	while ((s = ex_read(""))) {
		if (!strcmp(".", s)) {
			free(s);
			break;
		}
		sbuf_str(sb, s);
		sbuf_chr(sb, '\n');
		free(s);
	}
	if (cmd[0] == 'a')
		if (end > lbuf_len(xb))
			end = lbuf_len(xb);
	n = lbuf_len(xb);
	lbuf_put(xb, end, sbuf_buf(sb));
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - n - 1);
	sbuf_free(sb);
	return 0;
}

static int ec_print(char *ec)
{
	char cmd[EXLEN], loc[EXLEN];
	int beg, end;
	int i;
	ex_cmd(ec, cmd);
	ex_loc(ec, loc);
	if (!cmd[0] && !loc[0]) {
		if (xrow >= lbuf_len(xb) - 1)
			return 1;
		xrow = xrow + 1;
	}
	if (ex_region(loc, &beg, &end))
		return 1;
	for (i = beg; i < end; i++)
		ex_show(lbuf_get(xb, i));
	xrow = end;
	return 0;
}

static void ex_yank(int reg, int beg, int end)
{
	char *buf = lbuf_cp(xb, beg, end);
	reg_put(reg, buf, 1);
	free(buf);
}

static int ec_delete(char *ec)
{
	char loc[EXLEN];
	char arg[EXLEN];
	int beg, end;
	ex_loc(ec, loc);
	ex_arg(ec, arg);
	if (ex_region(loc, &beg, &end) || !lbuf_len(xb))
		return 1;
	ex_yank(arg[0], beg, end);
	lbuf_rm(xb, beg, end);
	xrow = beg;
	return 0;
}

static int ec_yank(char *ec)
{
	char loc[EXLEN];
	char arg[EXLEN];
	int beg, end;
	ex_loc(ec, loc);
	ex_arg(ec, arg);
	if (ex_region(loc, &beg, &end) || !lbuf_len(xb))
		return 1;
	ex_yank(arg[0], beg, end);
	return 0;
}

static int ec_put(char *ec)
{
	char loc[EXLEN];
	char arg[EXLEN];
	int beg, end;
	int lnmode;
	char *buf;
	int n = lbuf_len(xb);
	ex_loc(ec, loc);
	ex_arg(ec, arg);
	buf = reg_get(arg[0], &lnmode);
	if (!buf || ex_region(loc, &beg, &end))
		return 1;
	lbuf_put(xb, end, buf);
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - n - 1);
	return 0;
}

static int ec_lnum(char *ec)
{
	char loc[EXLEN];
	char msg[128];
	int beg, end;
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end))
		return 1;
	sprintf(msg, "%d\n", end);
	ex_show(msg);
	return 0;
}

static int ec_undo(char *ec)
{
	return lbuf_undo(xb);
}

static int ec_redo(char *ec)
{
	return lbuf_redo(xb);
}

static int ec_mark(char *ec)
{
	char loc[EXLEN], arg[EXLEN];
	int beg, end;
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end))
		return 1;
	lbuf_mark(xb, arg[0], end - 1, 0);
	return 0;
}

static char *readuntil(char **src, int delim)
{
	struct sbuf *sbuf = sbuf_make();
	char *s = *src;
	/* reading the pattern */
	while (*s && *s != delim) {
		if (s[0] == '\\' && s[1])
			sbuf_chr(sbuf, (unsigned char) *s++);
		sbuf_chr(sbuf, (unsigned char) *s++);
	}
	if (*s)			/* skipping the delimiter */
		s++;
	*src = s;
	return sbuf_done(sbuf);
}

static int ec_substitute(char *ec)
{
	char loc[EXLEN], arg[EXLEN];
	struct rset *re;
	int offs[32];
	int beg, end;
	char *pat, *rep;
	char *s = arg;
	int delim;
	int i;
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end))
		return 1;
	delim = (unsigned char) *s++;
	pat = readuntil(&s, delim);
	rep = readuntil(&s, delim);
	re = rset_make(1, &pat, xic ? RE_ICASE : 0);
	if (!re)
		return 1;
	for (i = beg; i < end; i++) {
		char *ln = lbuf_get(xb, i);
		struct sbuf *r = sbuf_make();
		while (rset_find(re, ln, LEN(offs) / 2, offs, 0) >= 0) {
			sbuf_mem(r, ln, offs[0]);
			sbuf_str(r, rep);
			ln += offs[1];
			if (!strchr(s, 'g'))
				break;
		}
		sbuf_str(r, ln);
		lbuf_rm(xb, i, i + 1);
		lbuf_put(xb, i, sbuf_buf(r));
		sbuf_free(r);
	}
	rset_free(re);
	free(pat);
	free(rep);
	return 0;
}

static int ec_exec(char *ec)
{
	char cmd[EXLEN];
	char arg[EXLEN];
	ex_modifiedbuffer(NULL);
	if (ex_expand(arg, ex_cmd(ec, cmd)))
		return 1;
	return cmd_exec(arg);
}

static int ec_make(char *ec)
{
	char cmd[EXLEN];
	char arg[EXLEN];
	char make[EXLEN];
	ex_modifiedbuffer(NULL);
	if (ex_expand(arg, ex_cmd(ec, cmd)))
		return 1;
	sprintf(make, "make %s", arg);
	return cmd_exec(make);
}

static struct option {
	char *abbr;
	char *name;
	int *var;
} options[] = {
	{"ai", "autoindent", &xai},
	{"aw", "autowrite", &xaw},
	{"ic", "ignorecase", &xic},
	{"td", "textdirection", &xdir},
	{"shape", "shape", &xshape},
	{"order", "xorder", &xorder},
};

static char *cutword(char *s, char *d)
{
	while (isspace(*s))
		s++;
	while (*s && !isspace(*s))
		*d++ = *s++;
	while (isspace(*s))
		s++;
	*d = '\0';
	return s;
}

static int ec_set(char *ec)
{
	char arg[EXLEN];
	char tok[EXLEN];
	char opt[EXLEN];
	char *s = arg;
	int val = 0;
	int i;
	ex_arg(ec, arg);
	if (*s) {
		s = cutword(s, tok);
		if (tok[0] == 'n' && tok[1] == 'o') {
			strcpy(opt, tok + 2);
			val = 0;
		} else {
			char *r = strchr(tok, '=');
			if (r) {
				*r = '\0';
				strcpy(opt, tok);
				val = atoi(r + 1);
			} else {
				strcpy(opt, tok);
				val = 1;
			}
		}
		for (i = 0; i < LEN(options); i++) {
			struct option *o = &options[i];
			if (!strcmp(o->abbr, opt) || !strcmp(o->name, opt)) {
				*o->var = val;
				return 0;
			}
		}
		ex_show("unknown option");
		return 1;
	}
	return 0;
}

static struct excmd {
	char *abbr;
	char *name;
	int (*ec)(char *s);
} excmds[] = {
	{"p", "print", ec_print},
	{"a", "append", ec_insert},
	{"i", "insert", ec_insert},
	{"d", "delete", ec_delete},
	{"c", "change", ec_insert},
	{"e", "edit", ec_edit},
	{"e!", "edit!", ec_edit},
	{"=", "=", ec_lnum},
	{"k", "mark", ec_mark},
	{"pu", "put", ec_put},
	{"q", "quit", ec_quit},
	{"q!", "quit!", ec_quit},
	{"r", "read", ec_read},
	{"w", "write", ec_write},
	{"wq", "wq", ec_write},
	{"u", "undo", ec_undo},
	{"r", "redo", ec_redo},
	{"se", "set", ec_set},
	{"s", "substitute", ec_substitute},
	{"ya", "yank", ec_yank},
	{"!", "!", ec_exec},
	{"make", "make", ec_make},
	{"", "", ec_print},
};

/* execute a single ex command */
void ex_command(char *ln)
{
	char cmd[EXLEN];
	int i;
	ex_cmd(ln, cmd);
	for (i = 0; i < LEN(excmds); i++) {
		if (!strcmp(excmds[i].abbr, cmd) || !strcmp(excmds[i].name, cmd)) {
			excmds[i].ec(ln);
			break;
		}
	}
	lbuf_modified(xb);
}

/* ex main loop */
void ex(void)
{
	if (xled)
		term_init();
	while (!xquit) {
		char *ln = ex_read(":");
		if (ln)
			ex_command(ln);
		free(ln);
	}
	if (xled)
		term_done();
}

void ex_init(char **files)
{
	char cmd[EXLEN];
	snprintf(cmd, sizeof(cmd), "e %s", files[0] ? files[0] : "");
	ec_edit(cmd);
}

void ex_done(void)
{
	int i;
	for (i = 0; i < LEN(bufs); i++)
		bufs_free(i);
}
