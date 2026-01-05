#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vi.h"

#define REG(s)	((s)[0] != '\\' ? (unsigned char) (s)[0] : 0x80 | (unsigned char) (s)[1])

int xrow, xoff, xtop;		/* current row, column, and top row */
int xleft;			/* the first visible column */
int xquit;			/* exit if set */
int xvis;			/* visual mode */
int xai = 1;			/* autoindent option */
int xic = 1;			/* ignorecase option */
int xaw;			/* autowrite option */
int xwa;			/* writeany option */
int xhl = 1;			/* syntax highlight option */
int xhll;			/* highlight current line */
int xled = 1;			/* use the line editor */
int xtd = 0;			/* current text direction */
int xshape = 1;			/* perform letter shaping */
int xorder = 1;			/* change the order of characters */
int xkmap = 0;			/* the current keymap */
int xkmap_alt = 1;		/* the alternate keymap */
int xlim = 256;			/* do not process lines longer than this */
int xru = 1;			/* show line number */
int xhist = 0;			/* number of history lines */
static char xkwd[EXLEN];	/* the last searched keyword */
static char xrep[EXLEN];	/* the last replacement */
static int xkwddir;		/* the last search direction */
static int xgdep;		/* global command recursion depth */
static char **next;		/* argument list */
static int next_pos;		/* position in argument list */

static struct buf {
	char ft[32];		/* file type */
	char *path;		/* file path */
	struct lbuf *lb;
	int row, off, top, left;
	short id;		/* buffer number */
	short td;		/* text direction */
	long mtime;		/* modification time */
} bufs[16];

static int bufs_cnt = 0;	/* number of allocated buffers */

static void bufs_free(int idx)
{
	if (bufs[idx].lb) {
		free(bufs[idx].path);
		lbuf_free(bufs[idx].lb);
		memset(&bufs[idx], 0, sizeof(bufs[idx]));
	}
}

static int bufs_find(char *path)
{
	int i;
	path = path[0] == '/' && path[1] == '\0' ? "" : path;
	for (i = 0; i < LEN(bufs); i++)
		if (bufs[i].path && !strcmp(bufs[i].path, path))
			return i;
	return -1;
}

static int bufs_findroom(void)
{
	int i;
	for (i = 0; i < LEN(bufs) - 1; i++)
		if (!bufs[i].lb)
			break;
	return i;
}

static void bufs_init(int idx, char *path)
{
	bufs_free(idx);
	bufs[idx].id = ++bufs_cnt;
	bufs[idx].path = uc_dup(path);
	bufs[idx].lb = lbuf_make();
	bufs[idx].row = 0;
	bufs[idx].off = 0;
	bufs[idx].top = 0;
	bufs[idx].left = 0;
	bufs[idx].td = +1;
	bufs[idx].mtime = -1;
	strcpy(bufs[idx].ft, syn_filetype(path));
}

static int bufs_open(char *path)
{
	int idx = bufs_findroom();
	path = path[0] == '/' && path[1] == '\0' ? "" : path;
	bufs_init(idx, path);
	return idx;
}

static void bufs_save(void)
{
	bufs[0].row = xrow;
	bufs[0].off = xoff;
	bufs[0].top = xtop;
	bufs[0].left = xleft;
	bufs[0].td = xtd;
}

static void bufs_load(void)
{
	xrow = bufs[0].row;
	xoff = bufs[0].off;
	xtop = bufs[0].top;
	xleft = bufs[0].left;
	xtd = bufs[0].td;
	reg_put('%', bufs[0].path ? bufs[0].path : "", 0);
}

static void bufs_shift(void)
{
	bufs_free(0);
	memmove(&bufs[0], &bufs[1], sizeof(bufs) - sizeof(bufs[0]));
	memset(&bufs[LEN(bufs) - 1], 0, sizeof(bufs[0]));
	bufs_load();
}

static void bufs_switch(int idx)
{
	struct buf tmp;
	bufs_save();
	memcpy(&tmp, &bufs[idx], sizeof(tmp));
	memmove(&bufs[1], &bufs[0], sizeof(tmp) * idx);
	memcpy(&bufs[0], &tmp, sizeof(tmp));
	bufs_load();
}

static void bufs_number(void)
{
	int n = 0;
	int i;
	for (i = 0; i < LEN(bufs); i++)
		if (bufs[i].lb != NULL)
			bufs[i].id = ++n;
	bufs_cnt = n;
}

static long mtime(char *path)
{
	struct stat st;
	if (!stat(path, &st))
		return st.st_mtime;
	return -1;
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

/* replace % and # with current and alternate path names; returns a static buffer */
static char *ex_pathexpand(char *src, int spaceallowed)
{
	static char buf[1024];
	char *dst = buf;
	char *end = dst + sizeof(buf);
	while (dst + 1 < end && *src && *src != '\n' &&
			(spaceallowed || (*src != ' ' && *src != '\t'))) {
		if (*src == '%' || *src == '#') {
			int idx = *src == '#';
			if (!bufs[idx].path) {
				ex_show("pathname \"%\" or \"#\" is not set");
				return NULL;
			}
			dst += snprintf(dst, end - dst, "%s",
				bufs[idx].path[0] ? bufs[idx].path : "/");
			src++;
		} else if (dst == buf && *src == '=') {
			char *cur = bufs[0].path;
			char *dir = cur != NULL ? strrchr(cur, '/') : NULL;
			if (cur != NULL && dir != NULL) {
				int len = MIN(dir - cur, end - dst - 2);
				memcpy(dst, cur, len);
				dst += len;
				*dst++ = '/';
			}
			src++;
		} else {
			if (*src == '\\' && src[1])
				src++;
			*dst++ = *src++;
		}
	}
	if (dst + 1 >= end)
		dst = end - 1;
	*dst = '\0';
	return buf;
}

/* read :e +cmd arguments */
static char *ex_plus(char *src, char *dst)
{
	while (*src == ' ')
		src++;
	*dst = '\0';
	if (*src != '+')
		return src;
	while (*src && *src != ' ') {
		if (src[0] == '\\' && src[1])
			src++;
		*dst++ = *src++;
	}
	*dst = '\0';
	while (*src == ' ' || *src == '\t')
		src++;
	return src;
}

/* read register name */
static char *ex_reg(char *src, int *reg)
{
	while (*src == ' ')
		src++;
	if (src[0] == '\\' && src[1] == '"')
		src++;
	*reg = REG(src);
	while (*src && *src != ' ' && *src != '\t')
		src++;
	while (*src == ' ' || *src == '\t')
		src++;
	return src;
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
	if (kwd)
		snprintf(xkwd, sizeof(xkwd), "%s", kwd);
	xkwddir = dir;
}

static int ex_search(char **pat)
{
	char *pat_re;
	struct rstr *re;
	int dir, row;
	int delim = **pat;
	char *kw = re_read(pat);
	if (kw != NULL && *kw)
		ex_kwdset(kw, delim == '/' ? 1 : -1);
	free(kw);
	if (ex_kwd(&pat_re, &dir))
		return -1;
	re = rstr_make(pat_re, xic ? RE_ICASE : 0);
	if (!re)
		return -1;
	row = xrow + dir;
	while (row >= 0 && row < lbuf_len(xb)) {
		if (rstr_find(re, lbuf_get(xb, row), 0, NULL, 0) >= 0)
			break;
		row += dir;
	}
	rstr_free(re);
	return row >= 0 && row < lbuf_len(xb) ? row : -1;
}

static int ex_lineno(char **num)
{
	int n = xrow;
	switch ((unsigned char) **num) {
	case '.':
		++*num;
		break;
	case '$':
		n = lbuf_len(xb) - 1;
		++*num;
		break;
	case '\'':
		if (lbuf_jump(xb, (unsigned char) *++(*num), &n, NULL))
			return -1;
		++*num;
		break;
	case '/':
	case '?':
		n = ex_search(num);
		break;
	default:
		if (isdigit((unsigned char) **num)) {
			n = atoi(*num) - 1;
			while (isdigit((unsigned char) **num))
				++*num;
		}
	}
	while (**num == '-' || **num == '+') {
		n += atoi((*num)++);
		while (isdigit((unsigned char) **num))
			(*num)++;
	}
	return n;
}

/* parse ex command addresses */
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
	if (*beg < 0 && *end == 0)
		*beg = 0;
	if (*beg < 0 || *beg >= lbuf_len(xb))
		return 1;
	if (*end < *beg || *end > lbuf_len(xb))
		return 1;
	return 0;
}

static char *lbuf_save(struct lbuf *lb, int beg, int end, char *path, int force, long ts)
{
	int fd;
	if (end < 0)
		end = lbuf_len(lb);
	if (!force && mtime > 0 && mtime(path) > ts) {
		return "write failed: file changed";
	} else if (!force && ts <= 0 && mtime(path) >= 0) {
		return "write failed: file exists";
	} else if ((fd = open(path, O_WRONLY | O_CREAT, conf_mode())) < 0) {
		return "write failed: cannot create file";
	} else if (lbuf_wr(lb, fd, beg, end) != 0 || close(fd) != 0) {
		close(fd);
		return "write failed";
	}
	return NULL;
}

static int bufs_modified(int idx, char *msg)
{
	struct buf *b = &bufs[idx];
	if (!b->lb || !lbuf_modified(b->lb))
		return 0;
	if (xaw && b->path[0])
		return lbuf_save(b->lb, 0, -1, b->path, 0, b->mtime) != NULL;
	if (msg)
		ex_show(msg);
	return 1;
}

static int ec_buffer(char *loc, char *cmd, char *arg, char *txt)
{
	char *aliases = "%#^";
	char ln[128];
	int i;
	if (!arg[0]) {
		/* print buffer list */
		for (i = 0; i < LEN(bufs) && bufs[i].lb; i++) {
			char c = i < strlen(aliases) ? aliases[i] : ' ';
			char m = lbuf_modified(bufs[i].lb) ? '*' : ' ';
			snprintf(ln, LEN(ln), "%2i %c %s %c",
					(int) bufs[i].id, c, bufs[i].path, m);
			ex_print(ln);
		}
	} else if (arg[0] == '!') {
		/* delete buffer */
		bufs_shift();
		if (bufs[0].lb == NULL)
			bufs_init(0, "");
	} else if (arg[0] == '~') {
		/* reassign buffer ids */
		bufs_number();
	} else {
		int id = arg[0] ? atoi(arg) : 0;
		int idx = -1;
		/* switch to the given buffer */
		if (isdigit((unsigned char) arg[0])) {	/* buffer id given */
			for (idx = 0; idx < LEN(bufs); idx++)
				if (bufs[idx].lb && id == bufs[idx].id)
					break;
		} else if (arg[0] == '-') {		/* previous buffer */
			for (i = 0; i < LEN(bufs); i++)
				if (bufs[i].lb && bufs[i].id < bufs[0].id)
					if (idx < 0 || bufs[i].id > bufs[idx].id)
						idx = i;
		} else if (arg[0] == '+') {		/* next buffer */
			for (i = 0; i < LEN(bufs); i++)
				if (bufs[i].lb && bufs[i].id > bufs[0].id)
					if (idx < 0 || bufs[i].id < bufs[idx].id)
						idx = i;
		} else {				/* buffer alias given */
			char *r = strchr(aliases, (unsigned char) arg[0]);
			idx = r ? r - aliases : -1;
		}
		if (idx >= 0 && idx < LEN(bufs) && bufs[idx].lb) {
			if (!xwa && strchr(cmd, '!') == NULL)
				if (bufs_modified(0, "buffer modified"))
					return 1;
			bufs_switch(idx);
		} else {
			ex_show("no such buffer");
			return 1;
		}
	}
	return 0;
}

int ex_list(char **ls, int size)
{
	int i;
	for (i = 0; i < LEN(bufs) && bufs[i].lb && i < size; i++)
		ls[i] = bufs[i].path;
	return i;
}

static int ec_edit(char *loc, char *cmd, char *arg, char *txt)
{
	char pls[EXLEN];
	char msg[128];
	char *path;
	int fd;
	if (!strchr(cmd, '!'))
		if (xb && !xwa && bufs_modified(0, "buffer modified"))
			return 1;
	arg = ex_plus(arg, pls);
	if (!(path = ex_pathexpand(arg, 0)))
		return 1;
	/* ew: switch buffer without changing # */
	if (path[0] && cmd[0] == 'e' && cmd[1] == 'w' && bufs_find(path) > 1)
		bufs_switch(1);
	/* check if the buffer is already available */
	if (path[0] && bufs_find(path) >= 0) {
		bufs_switch(bufs_find(path));
		if (pls[0] == '+')
			return ex_command(pls + 1);
		return 0;
	}
	if (path[0] || !bufs[0].path)
		bufs_switch(bufs_open(path));
	fd = open(ex_path(), O_RDONLY);
	if (fd >= 0) {
		int rd = lbuf_rd(xb, fd, 0, lbuf_len(xb));
		close(fd);
		snprintf(msg, sizeof(msg), "\"%s\"  [=%d]  [r]",
				ex_path(), lbuf_len(xb));
		if (rd)
			ex_show("read failed");
		else
			ex_show(msg);
	}
	lbuf_saved(xb, path[0] != '\0');
	bufs[0].mtime = mtime(ex_path());
	xrow = MAX(0, MIN(xrow, lbuf_len(xb) - 1));
	xoff = 0;
	xtop = MAX(0, MIN(xtop, lbuf_len(xb) - 1));
	if (pls[0] == '+')
		return ex_command(pls + 1);
	return 0;
}

static int ex_next(char *cmd, int dis)
{
	char arg[EXLEN];
	int old = next_pos;
	int idx = next != NULL && next[old] != NULL ? next_pos + dis : -1;
	char *path = idx >= 0 && next[idx] != NULL ? next[idx] : NULL;
	char *s = arg;
	char *r = path != NULL ? path : "";
	if (dis && path == NULL) {
		ex_show("no more files");
		return 1;
	}
	while (*r && s + 2 < arg + sizeof(arg)) {
		if (*r == ' ' || *r == '%' || *r == '#' || *r == '=')
			*s++ = '\\';
		*s++ = *r++;
	}
	*s = '\0';
	if (ec_edit("", cmd, arg, NULL))
		return 1;
	next_pos = idx;
	return 0;
}

static int ec_next(char *loc, char *cmd, char *arg, char *txt)
{
	return ex_next(cmd, +1);
}

static int ec_prev(char *loc, char *cmd, char *arg, char *txt)
{
	return ex_next(cmd, -1);
}

static int ec_read(char *loc, char *cmd, char *arg, char *txt)
{
	char msg[128];
	int beg, end, pos;
	char *path;
	char *obuf;
	int n = lbuf_len(xb);
	path = arg[0] ? ex_pathexpand(arg, 1) : ex_path();
	if (ex_region(loc, &beg, &end) || path == NULL)
		return 1;
	pos = lbuf_len(xb) ? end : 0;
	if (path[0] == '!') {
		if (!path[1])
			return 1;
		obuf = cmd_pipe(path + 1, NULL, 1);
		if (obuf)
			lbuf_edit(xb, obuf, pos, pos);
		free(obuf);
	} else {
		int fd = open(path, O_RDONLY);
		if (fd < 0) {
			ex_show("read failed");
			return 1;
		}
		if (lbuf_rd(xb, fd, pos, pos)) {
			ex_show("read failed");
			close(fd);
			return 1;
		}
		close(fd);
	}
	xrow = end + lbuf_len(xb) - n - 1;
	snprintf(msg, sizeof(msg), "\"%s\"  [=%d]  [r]",
		path, lbuf_len(xb) - n);
	ex_show(msg);
	return 0;
}

static int ec_write(char *loc, char *cmd, char *arg, char *txt)
{
	char msg[128];
	char *path;
	char *ibuf;
	int beg, end;
	path = arg[0] ? ex_pathexpand(arg, 1) : ex_path();
	if (cmd[0] == 'x' && !lbuf_modified(xb))
		return 0;
	if (ex_region(loc, &beg, &end) || path == NULL)
		return 1;
	if (!loc[0]) {
		beg = 0;
		end = lbuf_len(xb);
	}
	if (path[0] == '!') {
		if (!path[1])
			return 1;
		ibuf = lbuf_cp(xb, beg, end);
		ex_print(NULL);
		cmd_pipe(path + 1, ibuf, 0);
		free(ibuf);
	} else {
		long ts = !strcmp(ex_path(), path) ? bufs[0].mtime : 0;
		char *err = lbuf_save(xb, beg, end, path, !!strchr(cmd, '!'), ts);
		if (err != NULL) {
			ex_show(err);
			return 1;
		}
	}
	snprintf(msg, sizeof(msg), "\"%s\"  [=%d]  [w]", path, end - beg);
	ex_show(msg);
	if (!ex_path()[0]) {
		free(bufs[0].path);
		bufs[0].path = uc_dup(path);
		reg_put('%', path, 0);
	}
	if (!strcmp(ex_path(), path))
		lbuf_saved(xb, 0);
	if (!strcmp(ex_path(), path))
		bufs[0].mtime = mtime(path);
	return 0;
}

static int ec_quit(char *loc, char *cmd, char *arg, char *txt)
{
	int i;
	if (cmd[0] == 'w' || cmd[0] == 'x')
		if (ec_write("", cmd, arg, NULL))
			return 1;
	for (i = 0; i < LEN(bufs); i++) {
		if (bufs[i].lb) {
			if (!strchr(cmd, 'a') && !strchr(cmd, '!')) {
				if (bufs_modified(i, "buffer modified")) {
					bufs_switch(i);
					return 0;
				}
			}
			if (strchr(cmd, 'a')) {
				struct buf *b = &bufs[i];
				char *err = lbuf_save(b->lb, 0, -1, b->path,
						!!strchr(cmd, '!'), b->mtime);
				if (err) {
					bufs_switch(i);
					ex_show(err);
					return 0;
				}
			}
		}
	}
	xquit = 1;
	return 0;
}

static int ec_insert(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	int n;
	if (ex_region(loc, &beg, &end) && (beg != 0 || end != 0))
		return 1;
	if (cmd[0] == 'a')
		if (beg + 1 <= lbuf_len(xb))
			beg++;
	if (cmd[0] != 'c')
		end = beg;
	n = lbuf_len(xb);
	lbuf_edit(xb, txt, beg, end);
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - n - 1);
	return 0;
}

static int ec_print(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	int i;
	if (!cmd[0] && !loc[0])
		if (xrow >= lbuf_len(xb))
			return 1;
	if (ex_region(loc, &beg, &end))
		return 1;
	for (i = beg; i < end; i++)
		ex_print(lbuf_get(xb, i));
	xrow = MAX(beg, end - 1);
	xoff = 0;
	return 0;
}

static int ec_null(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	if (!xvis) {
		xrow = xrow + 1 < lbuf_len(xb) ? xrow + 1 : xrow;
		return ec_print(loc, cmd, arg, txt);
	}
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

static int ec_rs(char *loc, char *cmd, char *arg, char *txt)
{
	reg_put(REG(arg), txt, 1);
	return 0;
}

static int ec_delete(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	if (ex_region(loc, &beg, &end) || !lbuf_len(xb))
		return 1;
	ex_yank(REG(arg), beg, end);
	lbuf_edit(xb, NULL, beg, end);
	xrow = beg;
	return 0;
}

static int ec_yank(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	if (ex_region(loc, &beg, &end) || !lbuf_len(xb))
		return 1;
	ex_yank(REG(arg), beg, end);
	return 0;
}

static int ec_put(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	int lnmode;
	char *buf;
	int n = lbuf_len(xb);
	buf = reg_get(REG(arg), &lnmode);
	if (!buf || ex_region(loc, &beg, &end))
		return 1;
	lbuf_edit(xb, buf, end, end);
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - n - 1);
	return 0;
}

static int ec_lnum(char *loc, char *cmd, char *arg, char *txt)
{
	char msg[32];
	int beg, end;
	if (ex_region(loc, &beg, &end))
		return 1;
	sprintf(msg, "%d\n", end);
	ex_print(msg);
	return 0;
}

static int ec_undo(char *loc, char *cmd, char *arg, char *txt)
{
	return lbuf_undo(xb);
}

static int ec_redo(char *loc, char *cmd, char *arg, char *txt)
{
	return lbuf_redo(xb);
}

static int ec_mark(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	if (ex_region(loc, &beg, &end))
		return 1;
	lbuf_mark(xb, (unsigned char) arg[0], end - 1, 0);
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

static int ec_substitute(char *loc, char *cmd, char *arg, char *txt)
{
	struct rstr *re;
	int offs[32];
	int beg, end;
	char *pat = NULL, *rep = NULL;
	char *s = arg;
	int i;
	if (ex_region(loc, &beg, &end))
		return 1;
	pat = re_read(&s);
	if (pat && pat[0])
		ex_kwdset(pat, +1);
	if (pat && *s) {
		s--;
		rep = re_read(&s);
	}
	if (pat || rep)
		snprintf(xrep, sizeof(xrep), "%s", rep ? rep : "");
	free(pat);
	free(rep);
	if (ex_kwd(&pat, NULL))
		return 1;
	re = rstr_make(pat, xic ? RE_ICASE : 0);
	if (!re)
		return 1;
	for (i = beg; i < end; i++) {
		char *ln = lbuf_get(xb, i);
		struct sbuf *r = NULL;
		while (rstr_find(re, ln, LEN(offs) / 2, offs, 0) >= 0) {
			if (!r)
				r = sbuf_make();
			sbuf_mem(r, ln, offs[0]);
			replace(r, xrep, ln, offs);
			ln += offs[1];
			if (offs[1] <= 0)	/* zero-length match */
				sbuf_chr(r, (unsigned char) *ln++);
			if (!*ln || *ln == '\n' || !strchr(s, 'g'))
				break;
		}
		if (r) {
			sbuf_str(r, ln);
			lbuf_edit(xb, sbuf_buf(r), i, i + 1);
			sbuf_free(r);
		}
	}
	rstr_free(re);
	return 0;
}

static int ec_exec(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	char *text;
	char *rep;
	char *ecmd;
	if (!xwa && bufs_modified(0, "buffer modified"))
		return 1;
	if (!(ecmd = ex_pathexpand(arg, 1)))
		return 1;
	if (!loc[0]) {
		ex_print(NULL);
		return cmd_exec(ecmd);
	}
	if (ex_region(loc, &beg, &end))
		return 1;
	text = lbuf_cp(xb, beg, end);
	rep = cmd_pipe(ecmd, text, 1);
	if (rep)
		lbuf_edit(xb, rep, beg, end);
	free(text);
	free(rep);
	return 0;
}

static int ec_rx(char *loc, char *cmd, char *arg, char *txt)
{
	char *rep, *ecmd;
	int reg = 0;
	arg = ex_reg(arg, &reg);
	if (reg <= 0)
		return 1;
	if (!(ecmd = ex_pathexpand(arg, 1)))
		return 1;
	rep = cmd_pipe(ecmd, reg_get(reg, NULL), 1);
	reg_put(reg, rep ? rep : "", 1);
	free(rep);
	return !rep;
}

static int ec_rk(char *loc, char *cmd, char *arg, char *txt)
{
	char *rep, *path;
	int reg = 0;
	arg = ex_reg(arg, &reg);
	if (reg <= 0)
		return 1;
	if (!(path = ex_pathexpand(arg, 1)))
		return 1;
	rep = cmd_unix(path, reg_get(reg, NULL));
	reg_put(reg, rep ? rep : "", 1);
	free(rep);
	return !rep;
}

static int ec_make(char *loc, char *cmd, char *arg, char *txt)
{
	char make[EXLEN];
	char *target;
	if (!xwa && bufs_modified(0, "buffer modified"))
		return 1;
	if (!(target = ex_pathexpand(arg, 0)))
		return 1;
	if (snprintf(make, sizeof(make), "make %s", target) >= sizeof(make))
		return 1;
	ex_print(NULL);
	if (cmd_exec(make))
		return 1;
	return 0;
}

static int ec_ft(char *loc, char *cmd, char *arg, char *txt)
{
	if (arg[0])
		snprintf(bufs[0].ft, sizeof(bufs[0].ft), "%s", arg);
	else
		ex_print(ex_filetype());
	return 0;
}

static int ec_cmap(char *loc, char *cmd, char *arg, char *txt)
{
	if (arg[0])
		xkmap_alt = conf_kmapfind(arg);
	else
		ex_print(conf_kmap(xkmap)[0]);
	if (arg[0] && !strchr(cmd, '!'))
		xkmap = xkmap_alt;
	return 0;
}

static int ex_exec(char *ln);

static int ec_glob(char *loc, char *cmd, char *arg, char *txt)
{
	struct rstr *re;
	int offs[32];
	int beg, end, not;
	char *pat;
	char *s = arg;
	int i;
	if (!loc[0] && !xgdep)
		loc = "%";
	if (ex_region(loc, &beg, &end))
		return 1;
	not = strchr(cmd, '!') || cmd[0] == 'v';
	pat = re_read(&s);
	if (pat && pat[0])
		ex_kwdset(pat, +1);
	free(pat);
	if (ex_kwd(&pat, NULL))
		return 1;
	if (!(re = rstr_make(pat, xic ? RE_ICASE : 0)))
		return 1;
	xgdep++;
	for (i = beg + 1; i < end; i++)
		lbuf_globset(xb, i, xgdep);
	i = beg;
	while (i < lbuf_len(xb)) {
		char *ln = lbuf_get(xb, i);
		if ((rstr_find(re, ln, LEN(offs) / 2, offs, 0) < 0) == not) {
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
	rstr_free(re);
	return 0;
}

#define TAGCNT		32
#define TAGLEN		128

static int tag_row[TAGCNT];
static int tag_off[TAGCNT];
static int tag_pos[TAGCNT];
static char tag_path[TAGCNT][TAGLEN];
static char tag_name[TAGCNT][TAGLEN];
static int tag_cnt = 0;

static void ex_tagput(char *name)
{
	if (tag_cnt < TAGCNT) {
		tag_row[tag_cnt] = xrow;
		tag_off[tag_cnt] = xoff;
		snprintf(tag_path[tag_cnt], TAGLEN, "%s", ex_path());
		snprintf(tag_name[tag_cnt], TAGLEN, "%s", name);
		tag_cnt++;
	}
}

/* go to definition (dir=+1 next, dir=-1 prev, dir=0 first) */
static int tag_goto(char *cw, int dir)
{
	char path[120], cmd[120];
	char *s, *ln;
	int pos = dir == 0 || tag_cnt == 0 ? 0 : tag_pos[tag_cnt - 1];
	if (tag_find(cw, &pos, dir, path, sizeof(path), cmd, sizeof(cmd))) {
		ex_show("not found");
		return 1;
	}
	if (dir == 0)
		ex_tagput(cw);
	tag_pos[tag_cnt - 1] = pos;
	if (strcmp(path, ex_path()) != 0) {
		if (access(path, R_OK) != 0) {
			ex_show("cannot open");
			return 1;
		}
		if (ec_edit("", "e", path, NULL) != 0)
			return 1;
	}
	xrow = 0;
	xoff = 0;
	ex_command(cmd);
	ln = lbuf_get(xb, xrow);
	if (ln && (s = strstr(ln, cw)) != NULL)
		xoff = s - ln;
	return 0;
}

static int ec_tag(char *loc, char *cmd, char *arg, char *txt)
{
	return tag_goto(arg, 0);
}

static int ec_tfree(char *loc, char *cmd, char *arg, char *txt)
{
	tag_done();
	return 0;
}

static int ec_pop(char *loc, char *cmd, char *arg, char *txt)
{
	if (tag_cnt > 0) {
		tag_cnt--;
		if (ex_path() == NULL || strcmp(tag_path[tag_cnt], ex_path()) != 0)
			ec_edit("", "e", tag_path[tag_cnt], txt);
		xrow = tag_row[tag_cnt];
		xoff = tag_off[tag_cnt];
		return 0;
	} else {
		ex_show("not found");
	}
	return 1;
}

static int ec_tnext(char *loc, char *cmd, char *arg, char *txt)
{
	if (tag_cnt > 0)
		return tag_goto(tag_name[tag_cnt - 1], +1);
	return 1;
}

static int ec_tprev(char *loc, char *cmd, char *arg, char *txt)
{
	if (tag_cnt > 0)
		return tag_goto(tag_name[tag_cnt - 1], -1);
	return 1;
}

static int ec_at(char *loc, char *cmd, char *arg, char *txt)
{
	int beg, end;
	int lnmode;
	char *buf = reg_get(REG(arg), &lnmode);
	if (!buf || ex_region(loc, &beg, &end))
		return 1;
	xrow = beg;
	if (cmd[0] == 'r' && cmd[1] == 'a') {
		struct sbuf *r = sbuf_make();
		char *s = buf;
		int ret;
		while (*s) {
			if ((unsigned char) *s == '' && s[1]) {
				char *reg = reg_get((unsigned char) *++s, NULL);
				sbuf_str(r, reg ? reg : "");
				s++;
			} else {
				if ((unsigned char) *s == '' && s[1])
					s++;
				sbuf_chr(r, (unsigned char) *s++);
			}
		}
		ret = ex_command(sbuf_buf(r));
		sbuf_free(r);
		return ret;
	}
	return ex_command(buf);
}

static int ec_source(char *loc, char *cmd, char *arg, char *txt)
{
	char *path = arg[0] ? ex_pathexpand(arg, 1) : ex_path();
	char buf[1 << 10];
	struct sbuf *sb;
	int fd = path[0] ? open(path, O_RDONLY) : -1;
	long nr;
	if (fd < 0)
		return 1;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	ex_command(sbuf_buf(sb));
	sbuf_free(sb);
	return 0;
}

static int ec_echo(char *loc, char *cmd, char *arg, char *txt)
{
	ex_print(arg);
	return 0;
}

static struct option {
	char *abbr;
	char *name;
	int *var;
} options[] = {
	{"ai", "autoindent", &xai},
	{"aw", "autowrite", &xaw},
	{"hist", "history", &xhist},
	{"hl", "highlight", &xhl},
	{"hll", "highlightline", &xhll},
	{"ic", "ignorecase", &xic},
	{"lim", "linelimit", &xlim},
	{"order", "order", &xorder},
	{"ru", "ruler", &xru},
	{"shape", "shape", &xshape},
	{"td", "textdirection", &xtd},
	{"wa", "writeany", &xwa},
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

static int ec_set(char *loc, char *cmd, char *arg, char *txt)
{
	char tok[EXLEN];
	char *opt = tok;
	char *s = arg;
	int val = 0;
	int i;
	if (*s) {
		s = cutword(s, tok);
		if (tok[0] == 'n' && tok[1] == 'o') {
			opt = tok + 2;
		} else {
			char *r = strchr(tok, '=');
			val = 1;
			if (r) {
				*r = '\0';
				val = atoi(r + 1);
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
	int (*ec)(char *loc, char *cmd, char *arg, char *txt);
} excmds[] = {
	{"a", "append", ec_insert},
	{"b", "buffer", ec_buffer},
	{"d", "delete", ec_delete},
	{"c", "change", ec_insert},
	{"cm", "cmap", ec_cmap},
	{"cm!", "cmap!", ec_cmap},
	{"e", "edit", ec_edit},
	{"e!", "edit!", ec_edit},
	{"ec", "echo", ec_echo},
	{"ew", "ew", ec_edit},
	{"ew!", "ew!", ec_edit},
	{"ft", "filetype", ec_ft},
	{"g", "global", ec_glob},
	{"g!", "global!", ec_glob},
	{"i", "insert", ec_insert},
	{"k", "mark", ec_mark},
	{"make", "make", ec_make},
	{"n", "next", ec_next},
	{"p", "print", ec_print},
	{"po", "pop", ec_pop},
	{"pu", "put", ec_put},
	{"prev", "prev", ec_prev},
	{"q", "quit", ec_quit},
	{"q!", "quit!", ec_quit},
	{"r", "read", ec_read},
	{"redo", "redo", ec_redo},
	{"rs", "rs", ec_rs},
	{"rx", "rx", ec_rx},
	{"ra", "ra", ec_at},
	{"rk", "rk", ec_rk},
	{"se", "set", ec_set},
	{"s", "substitute", ec_substitute},
	{"so", "source", ec_source},
	{"ta", "tag", ec_tag},
	{"tn", "tnext", ec_tnext},
	{"tp", "tprev", ec_tprev},
	{"tf", "tfree", ec_tfree},
	{"u", "undo", ec_undo},
	{"v", "vglobal", ec_glob},
	{"w", "write", ec_write},
	{"w!", "write!", ec_write},
	{"wq", "wq", ec_quit},
	{"wq!", "wq!", ec_quit},
	{"x", "xit", ec_quit},
	{"x!", "xit!", ec_quit},
	{"xa", "xa", ec_quit},
	{"xa!", "xa!", ec_quit},
	{"y", "yank", ec_yank},
	{"!", "!", ec_exec},
	{"@", "@", ec_at},
	{"=", "=", ec_lnum},
	{"", "", ec_null},
};

static int ex_idx(char *cmd)
{
	int i;
	for (i = 0; i < LEN(excmds); i++)
		if (!strcmp(excmds[i].abbr, cmd) || !strcmp(excmds[i].name, cmd))
			return i;
	return -1;
}

/* read ex command addresses */
static char *ex_loc(char *src, char *loc)
{
	while (*src == ':' || *src == ' ' || *src == '\t')
		src++;
	while (*src && strchr(".$0123456789'/?+-,;%", (unsigned char) *src) != NULL) {
		if (*src == '\'')
			*loc++ = *src++;
		if (*src == '/' || *src == '?') {
			int d = *src;
			*loc++ = *src++;
			while (*src && *src != d) {
				if (*src == '\\' && src[1])
					*loc++ = *src++;
				*loc++ = *src++;
			}
		}
		if (*src)
			*loc++ = *src++;
	}
	*loc = '\0';
	return src;
}

/* read ex command name */
static char *ex_cmd(char *src, char *cmd)
{
	char *cmd0 = cmd;
	while (*src == ' ' || *src == '\t')
		src++;
	while (isalpha((unsigned char) *src) && cmd < cmd0 + 16)
		if ((*cmd++ = *src++) == 'k' && cmd == cmd0 + 1)
			break;
	if (*src == '!' || *src == '=' || *src == '@')
		*cmd++ = *src++;
	*cmd = '\0';
	return src;
}

/* read ex command argument for excmd command */
static char *ex_arg(char *src, char *dst, char *excmd)
{
	int c0 = excmd[0];
	int c1 = c0 ? excmd[1] : 0;
	while (*src == ' ' || *src == '\t')
		src++;
	if (c0 == '!' || c0 == 'g' || c0 == 'v' ||
			((c0 == 'r' || c0 == 'w') && !c1 && src[0] == '!')) {
		while (*src && *src != '\n') {
			if (*src == '\\' && src[1])
				*dst++ = *src++;
			*dst++ = *src++;
		}
	} else if ((c0 == 's' && c1 != 'e') || c0 == '&' || c0 == '~') {
		int delim = *src;
		int cnt = 2;
		if (delim != '\n' && delim != '|' && delim != '\\' && delim != '"') {
			*dst++ = *src++;
			while (*src && *src != '\n' && cnt > 0) {
				if (*src == delim)
					cnt--;
				if (*src == '\\' && src[1])
					*dst++ = *src++;
				*dst++ = *src++;
			}
		}
	}
	while (*src && *src != '\n' && *src != '|' && *src != '"') {
		if (*src == '\\' && src[1])
			*dst++ = *src++;
		*dst++ = *src++;
	}
	if (*src == '"') {
		while (*src && *src != '\n')
			src++;
	}
	if (*src == '\n' || *src == '|')
		src++;
	*dst = '\0';
	return src;
}

/* read ex text input for excmd command */
static char *ex_txt(char *src, char **dst, char *excmd)
{
	int c0 = excmd[0];
	int c1 = c0 ? excmd[1] : 0;
	*dst = NULL;
	if (c0 == 'r' && c1 == 's' && src[0]) {
		char *beg = src;
		char *res;
		while (src[0] && (src[0] != '\n' || src[1] != '.' || src[2] != '\n'))
			src++;
		res = malloc((src - beg) + 1 + 1);
		memcpy(res, beg, src - beg);
		res[src - beg] = '\n';
		res[src - beg + 1] = '\0';
		*dst = res;
		return src[0] ? src + 3 : src;
	}
	if ((c0 == 'r' && c1 == 's') || (c1 == 0 && (c0 == 'i' || c0 == 'a' || c0 == 'c'))) {
		struct sbuf *sb = sbuf_make();
		char *s;
		while ((s = ex_read(""))) {
			if (!strcmp(".", s)) {
				free(s);
				break;
			}
			sbuf_str(sb, s);
			sbuf_chr(sb, '\n');
			free(s);
		}
		*dst = sbuf_done(sb);
		return src;
	}
	return src;
}

/* execute a single ex command */
static int ex_exec(char *ln)
{
	char loc[EXLEN], cmd[EXLEN], arg[EXLEN];
	int ret = 0;
	if (strlen(ln) >= EXLEN) {
		ex_show("command too long");
		return 1;
	}
	while (*ln) {
		char *txt = NULL;
		int idx;
		ln = ex_loc(ln, loc);
		ln = ex_cmd(ln, cmd);
		idx = ex_idx(cmd);
		ln = ex_arg(ln, arg, idx >= 0 ? excmds[idx].abbr : "unknown");
		ln = ex_txt(ln, &txt, idx >= 0 ? excmds[idx].abbr : "unknown");
		if (idx >= 0)
			ret = excmds[idx].ec(loc, cmd, arg, txt);
		else
			ex_show("unknown command");
		free(txt);
	}
	return ret;
}

/* execute a single ex command */
int ex_command(char *ln)
{
	int ret = ex_exec(ln);
	lbuf_modified(xb);
	return ret;
}

/* ex main loop */
void ex(void)
{
	while (!xquit) {
		char *ln = ex_read(":");
		if (ln) {
			ex_command(ln);
			reg_put(':', ln, 1);
		}
		free(ln);
	}
}

int ex_init(char **files)
{
	next = files;
	if (ex_next("e", 0))
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
