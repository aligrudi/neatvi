#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vi.h"

int xrow, xoff, xtop;		/* current row, column, and top row */
int xleft;			/* the first visible column */
int xquit;			/* exit if set */
int xvis;			/* visual mode */
int xai = 1;			/* autoindent option */
int xic = 1;			/* ignorecase option */
int xaw;			/* autowrite option */
int xhl = 1;			/* syntax highlight option */
int xhll;			/* highlight current line */
int xled = 1;			/* use the line editor */
int xtd = +1;			/* current text direction */
int xshape = 1;			/* perform letter shaping */
int xorder = 1;			/* change the order of characters */
int xkmap = 0;			/* the current keymap */
int xkmap_alt = 1;		/* the alternate keymap */
static char xkwd[EXLEN];	/* the last searched keyword */
static char xrep[EXLEN];	/* the last replacement */
static int xkwddir;		/* the last search direction */
static int xgdep;		/* global command recursion depth */

static struct buf {
	char ft[32];
	char *path;
	struct lbuf *lb;
	int row, off, top, td;
	long mtime;		/* modification time */
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

static long mtime(char *path)
{
	struct stat st;
	if (!stat(path, &st))
		return st.st_mtime;
	return -1;
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
	bufs[i].off = 0;
	bufs[i].top = 0;
	bufs[i].td = +1;
	bufs[i].mtime = -1;
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
	xoff = bufs[0].off;
	xtop = bufs[0].top;
	xtd = bufs[0].td;
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
		if (*s)
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
	while (*s && !isspace((unsigned char) *s)) {
		if (*s == '\\' && s[1])
			s++;
		*arg++ = *s++;
	}
	*arg = '\0';
	return s;
}

/* read ex file argument */
static char *ex_filearg(char *s, char *arg, int spaceallowed)
{
	s = ex_cmd(s, arg);
	while (isspace((unsigned char) *s))
		s++;
	while (*s && (spaceallowed || !isspace((unsigned char) *s))) {
		int c = (unsigned char) *s++;
		if (c == '%') {
			if (!bufs[0].path || !bufs[0].path[0]) {
				ex_show("\"%\" is unset\n");
				return NULL;
			}
			strcpy(arg, bufs[0].path);
			arg = strchr(arg, '\0');
			continue;
		}
		if (c == '#') {
			if (!bufs[1].path || !bufs[1].path[0]) {
				ex_show("\"#\" is unset\n");
				return NULL;
			}
			strcpy(arg, bufs[1].path);
			arg = strchr(arg, '\0');
			continue;
		}
		if (c == '\\')
			c = *s++;
		*arg++ = c;
	}
	*arg = '\0';
	return s;
}


static char *ex_argeol(char *ec)
{
	char arg[EXLEN];
	char *s = ex_cmd(ec, arg);
	while (isspace((unsigned char) *s))
		s++;
	return s;
}

/* the previous search keyword */
int ex_kwd(char **kwd, int *dir)
{
	if (kwd)
		*kwd = xkwd;
	if (dir)
		*dir = xkwddir;
	return xkwddir == 0;
}

/* set the previous search keyword */
void ex_kwdset(char *kwd, int dir)
{
	if (kwd) {
		snprintf(xkwd, sizeof(xkwd), "%s", kwd);
		reg_put('/', kwd, 0);
	}
	xkwddir = dir;
}

static int ex_search(char **pat)
{
	struct sbuf *kw;
	char *b = *pat;
	char *e = b;
	char *pats[1];
	struct rset *re;
	int dir, row;
	kw = sbuf_make();
	while (*++e) {
		if (*e == **pat)
			break;
		sbuf_chr(kw, (unsigned char) *e);
		if (*e == '\\' && e[1])
			e++;
	}
	if (sbuf_len(kw))
		ex_kwdset(sbuf_buf(kw), **pat == '/' ? 1 : -1);
	sbuf_free(kw);
	*pat = *e ? e + 1 : e;
	if (ex_kwd(&pats[0], &dir))
		return -1;
	re = rset_make(1, pats, xic ? RE_ICASE : 0);
	if (!re)
		return -1;
	row = xrow + dir;
	while (row >= 0 && row < lbuf_len(xb)) {
		if (rset_find(re, lbuf_get(xb, row), 0, NULL, 0) >= 0)
			break;
		row += dir;
	}
	rset_free(re);
	return row >= 0 && row < lbuf_len(xb) ? row : -1;
}

static int ex_lineno(char **num)
{
	int n = xrow;
	switch ((unsigned char) **num) {
	case '.':
		*num += 1;
		break;
	case '$':
		n = lbuf_len(xb) - 1;
		*num += 1;
		break;
	case '\'':
		if (lbuf_jump(xb, (unsigned char) *++(*num), &n, NULL))
			return -1;
		*num += 1;
		break;
	case '/':
	case '?':
		n = ex_search(num);
		break;
	default:
		if (isdigit((unsigned char) **num)) {
			n = atoi(*num) - 1;
			while (isdigit((unsigned char) **num))
				*num += 1;
		}
	}
	while (**num == '-' || **num == '+') {
		n += atoi((*num)++);
		while (isdigit((unsigned char) **num))
			(*num)++;
	}
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
		*end = ex_lineno(&loc) + 1;
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

static int ec_edit(char *ec)
{
	char msg[128];
	char cmd[EXLEN];
	char path[EXLEN];
	int fd;
	ex_cmd(ec, cmd);
	if (!ex_filearg(ec, path, 0))
		return 1;
	if (!strchr(cmd, '!'))
		if (xb && ex_modifiedbuffer("buffer modified\n"))
			return 1;
	bufs[0].row = xrow;
	bufs[0].off = xoff;
	bufs[0].top = xtop;
	bufs[0].td = xtd;
	if (path[0] && bufs_find(path) >= 0) {
		bufs_switch(bufs_find(path));
		return 0;
	}
	if (path[0] || !bufs[0].path)
		bufs_switch(bufs_open(path));
	fd = open(ex_path(), O_RDONLY);
	if (fd >= 0) {
		int rd = lbuf_rd(xb, fd, 0, lbuf_len(xb));
		close(fd);
		snprintf(msg, sizeof(msg), "\"%s\"  %d lines  [r]\n",
				ex_path(), lbuf_len(xb));
		if (rd)
			ex_show("read failed\n");
		else
			ex_show(msg);
	}
	lbuf_saved(xb, path[0] != '\0');
	bufs[0].mtime = mtime(ex_path());
	xrow = MAX(0, MIN(xrow, lbuf_len(xb) - 1));
	xoff = 0;
	xtop = MAX(0, MIN(xtop, lbuf_len(xb) - 1));
	return 0;
}

static int ec_read(char *ec)
{
	char arg[EXLEN], loc[EXLEN];
	char msg[128];
	int beg, end;
	char *path;
	char *obuf;
	int n = lbuf_len(xb);
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	path = arg[0] ? arg : ex_path();
	if (ex_region(loc, &beg, &end))
		return 1;
	if (arg[0] == '!') {
		int pos = MIN(xrow + 1, lbuf_len(xb));
		if (!ex_filearg(ec, arg, 1))
			return 1;
		obuf = cmd_pipe(arg + 1, NULL, 0, 1);
		if (obuf)
			lbuf_edit(xb, obuf, pos, pos);
		free(obuf);
	} else {
		int fd = open(path, O_RDONLY);
		int pos = lbuf_len(xb) ? end : 0;
		if (fd < 0) {
			ex_show("read failed\n");
			return 1;
		}
		if (lbuf_rd(xb, fd, pos, pos)) {
			ex_show("read failed\n");
			close(fd);
			return 1;
		}
		close(fd);
	}
	xrow = end + lbuf_len(xb) - n - 1;
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
	char *ibuf;
	int beg, end;
	ex_cmd(ec, cmd);
	ex_arg(ec, arg);
	ex_loc(ec, loc);
	path = arg[0] ? arg : ex_path();
	if (cmd[0] == 'x' && !lbuf_modified(xb))
		return ec_quit(cmd);
	if (ex_region(loc, &beg, &end))
		return 1;
	if (!loc[0]) {
		beg = 0;
		end = lbuf_len(xb);
	}
	if (arg[0] == '!') {
		if (!ex_filearg(ec, arg, 1))
			return 1;
		ibuf = lbuf_cp(xb, beg, end);
		ex_print(NULL);
		cmd_pipe(arg + 1, ibuf, 1, 0);
		free(ibuf);
	} else {
		int fd;
		if (!strchr(cmd, '!') && bufs[0].path &&
				!strcmp(bufs[0].path, path) &&
				mtime(bufs[0].path) > bufs[0].mtime) {
			ex_show("write failed: file changed\n");
			return 1;
		}
		if (!strchr(cmd, '!') && arg[0] && mtime(arg) >= 0) {
			ex_show("write failed: file exists\n");
			return 1;
		}
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, conf_mode());
		if (fd < 0) {
			ex_show("write failed: cannot create file\n");
			return 1;
		}
		if (lbuf_wr(xb, fd, beg, end)) {
			ex_show("write failed\n");
			close(fd);
			return 1;
		}
		close(fd);
	}
	snprintf(msg, sizeof(msg), "\"%s\"  %d lines  [w]\n",
			path, end - beg);
	ex_show(msg);
	if (!ex_path()[0]) {
		free(bufs[0].path);
		bufs[0].path = uc_dup(path);
	}
	if (!strcmp(ex_path(), path))
		lbuf_saved(xb, 0);
	if (!strcmp(ex_path(), path))
		bufs[0].mtime = mtime(path);
	if (cmd[0] == 'x' || (cmd[0] == 'w' && cmd[1] == 'q'))
		ec_quit(cmd);
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
		if (beg + 1 <= lbuf_len(xb))
			beg++;
	if (cmd[0] != 'c')
		end = beg;
	n = lbuf_len(xb);
	lbuf_edit(xb, sbuf_buf(sb), beg, end);
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
		ex_print(lbuf_get(xb, i));
	xrow = end;
	xoff = 0;
	return 0;
}

static int ec_null(char *ec)
{
	char loc[EXLEN];
	int beg, end;
	if (!xvis)
		return ec_print(ec);
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end))
		return 1;
	xrow = MAX(beg, end - 1);
	xoff = 0;
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
	lbuf_edit(xb, NULL, beg, end);
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
	lbuf_edit(xb, buf, end, end);
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
	ex_print(msg);
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

static void replace(struct sbuf *dst, char *rep, char *ln, int *offs)
{
	while (rep[0]) {
		if (rep[0] == '\\' && rep[1]) {
			if (rep[1] >= '0' && rep[1] <= '9') {
				int grp = (rep[1] - '0') * 2;
				int len = offs[grp + 1] - offs[grp];
				sbuf_mem(dst, ln + offs[grp], len);
			} else {
				sbuf_chr(dst, (unsigned char) rep[1]);
			}
			rep++;
		} else {
			sbuf_chr(dst, (unsigned char) rep[0]);
		}
		rep++;
	}
}

static int ec_substitute(char *ec)
{
	char loc[EXLEN];
	struct rset *re;
	int offs[32];
	int beg, end;
	char *pats[1];
	char *pat = NULL, *rep = NULL;
	char *s;
	int i;
	ex_loc(ec, loc);
	if (ex_region(loc, &beg, &end))
		return 1;
	s = ex_argeol(ec);
	pat = re_read(&s);
	if (pat && pat[0])
		ex_kwdset(pat, +1);
	if (pat && *s) {
		s--;
		rep = re_read(&s);
	}
	if (!rep)
		rep = uc_dup(pat ? "" : xrep);
	snprintf(xrep, sizeof(xrep), "%s", rep);
	free(pat);
	if (ex_kwd(&pats[0], NULL))
		return 1;
	re = rset_make(1, pats, xic ? RE_ICASE : 0);
	if (!re) {
		free(rep);
		return 1;
	}
	for (i = beg; i < end; i++) {
		char *ln = lbuf_get(xb, i);
		struct sbuf *r = sbuf_make();
		while (rset_find(re, ln, LEN(offs) / 2, offs, 0) >= 0) {
			sbuf_mem(r, ln, offs[0]);
			replace(r, rep, ln, offs);
			ln += offs[1];
			if (!*ln || !strchr(s, 'g'))
				break;
			if (offs[1] <= 0)	/* zero-length match */
				sbuf_chr(r, (unsigned char) *ln++);
		}
		sbuf_str(r, ln);
		lbuf_edit(xb, sbuf_buf(r), i, i + 1);
		sbuf_free(r);
	}
	rset_free(re);
	free(rep);
	return 0;
}

static int ec_exec(char *ec)
{
	char loc[EXLEN];
	char arg[EXLEN];
	int beg, end;
	char *text;
	char *rep;
	ex_modifiedbuffer(NULL);
	ex_loc(ec, loc);
	if (!ex_filearg(ec, arg, 1))
		return 1;
	if (!loc[0]) {
		ex_print(NULL);
		return cmd_exec(arg);
	}
	if (ex_region(loc, &beg, &end))
		return 1;
	text = lbuf_cp(xb, beg, end);
	rep = cmd_pipe(arg, text, 1, 1);
	if (rep)
		lbuf_edit(xb, rep, beg, end);
	free(text);
	free(rep);
	return 0;
}

static int ec_make(char *ec)
{
	char arg[EXLEN];
	char make[EXLEN];
	ex_modifiedbuffer(NULL);
	if (!ex_filearg(ec, arg, 1))
		return 1;
	sprintf(make, "make %s", arg);
	ex_print(NULL);
	if (cmd_exec(make))
		return 1;
	return 0;
}

static int ec_ft(char *ec)
{
	char arg[EXLEN];
	ex_arg(ec, arg);
	if (arg[0])
		snprintf(bufs[0].ft, sizeof(bufs[0].ft), "%s", arg);
	else
		ex_print(ex_filetype());
	return 0;
}

static int ec_cmap(char *ec)
{
	char cmd[EXLEN];
	char arg[EXLEN];
	ex_cmd(ec, cmd);
	ex_arg(ec, arg);
	if (arg[0])
		xkmap_alt = conf_kmapfind(arg);
	else
		ex_print(conf_kmap(xkmap)[0]);
	if (arg[0] && !strchr(cmd, '!'))
		xkmap = xkmap_alt;
	return 0;
}

static int ex_exec(char *ln);

static int ec_glob(char *ec)
{
	char loc[EXLEN], cmd[EXLEN];
	struct rset *re;
	int offs[32];
	int beg, end, not;
	char *pats[1];
	char *pat, *s;
	int i;
	ex_cmd(ec, cmd);
	ex_loc(ec, loc);
	if (!loc[0] && !xgdep)
		strcpy(loc, "%");
	if (ex_region(loc, &beg, &end))
		return 1;
	not = strchr(cmd, '!') || cmd[0] == 'v';
	s = ex_argeol(ec);
	pat = re_read(&s);
	if (pat && pat[0])
		ex_kwdset(pat, +1);
	free(pat);
	if (ex_kwd(&pats[0], NULL))
		return 1;
	if (!(re = rset_make(1, pats, xic ? RE_ICASE : 0)))
		return 1;
	xgdep++;
	for (i = beg + 1; i < end; i++)
		lbuf_globset(xb, i, xgdep);
	i = beg;
	while (i < lbuf_len(xb)) {
		char *ln = lbuf_get(xb, i);
		if ((rset_find(re, ln, LEN(offs) / 2, offs, 0) < 0) == not) {
			xrow = i;
			if (ex_exec(s))
				break;
			i = MIN(i, xrow);
		}
		while (i < lbuf_len(xb) && !lbuf_globget(xb, i, xgdep))
			i++;
	}
	for (i = 0; i < lbuf_len(xb); i++)
		lbuf_globget(xb, i, xgdep);
	xgdep--;
	rset_free(re);
	return 0;
}

static struct option {
	char *abbr;
	char *name;
	int *var;
} options[] = {
	{"ai", "autoindent", &xai},
	{"aw", "autowrite", &xaw},
	{"ic", "ignorecase", &xic},
	{"td", "textdirection", &xtd},
	{"shape", "shape", &xshape},
	{"order", "xorder", &xorder},
	{"hl", "highlight", &xhl},
	{"hll", "highlightline", &xhll},
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
	{"g", "global", ec_glob},
	{"g!", "global!", ec_glob},
	{"=", "=", ec_lnum},
	{"k", "mark", ec_mark},
	{"pu", "put", ec_put},
	{"q", "quit", ec_quit},
	{"q!", "quit!", ec_quit},
	{"r", "read", ec_read},
	{"v", "vglobal", ec_glob},
	{"w", "write", ec_write},
	{"w!", "write!", ec_write},
	{"wq", "wq", ec_write},
	{"wq!", "wq!", ec_write},
	{"u", "undo", ec_undo},
	{"r", "redo", ec_redo},
	{"se", "set", ec_set},
	{"s", "substitute", ec_substitute},
	{"x", "xit", ec_write},
	{"x!", "xit!", ec_write},
	{"ya", "yank", ec_yank},
	{"!", "!", ec_exec},
	{"make", "make", ec_make},
	{"ft", "filetype", ec_ft},
	{"cm", "cmap", ec_cmap},
	{"cm!", "cmap!", ec_cmap},
	{"", "", ec_null},
};

/* read an ex command and its arguments from src into dst */
static void ex_line(int (*ec)(char *s), char *dst, char **src)
{
	if (!ec || (ec != ec_glob && ec != ec_exec)) {
		while (**src && **src != '|' && **src != '\n')
			*dst++ = *(*src)++;
		*dst = '\0';
		if (**src)
			(*src)++;
	} else {	/* the rest of the line for :g and :! */
		strcpy(dst, *src);
		*src = strchr(*src, '\0');
	}
}

/* execute a single ex command */
static int ex_exec(char *ln)
{
	char ec[EXLEN];
	char cmd[EXLEN];
	int i;
	int ret = 0;
	while (*ln) {
		ex_cmd(ln, cmd);
		for (i = 0; i < LEN(excmds); i++) {
			if (!strcmp(excmds[i].abbr, cmd) ||
					!strcmp(excmds[i].name, cmd)) {
				ex_line(excmds[i].ec, ec, &ln);
				ret = excmds[i].ec(ec);
				break;
			}
		}
		if (i == LEN(excmds))
			ex_line(NULL, ec, &ln);
	}
	return ret;
}

/* execute a single ex command */
void ex_command(char *ln)
{
	ex_exec(ln);
	lbuf_modified(xb);
	reg_put(':', ln, 0);
}

/* ex main loop */
void ex(void)
{
	while (!xquit) {
		char *ln = ex_read(":");
		if (ln)
			ex_command(ln);
		free(ln);
	}
}

int ex_init(char **files)
{
	char cmd[EXLEN];
	char *s = cmd;
	char *r = files[0] ? files[0] : "";
	*s++ = 'e';
	*s++ = ' ';
	while (*r && s + 2 < cmd + sizeof(cmd)) {
		if (*r == ' ' || *r == '%' || *r == '#')
			*s++ = '\\';
		*s++ = *r++;
	}
	*s = '\0';
	if (ec_edit(cmd))
		return 1;
	if (getenv("EXINIT"))
		ex_command(getenv("EXINIT"));
	return 0;
}

void ex_done(void)
{
	int i;
	for (i = 0; i < LEN(bufs); i++)
		bufs_free(i);
}
