#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "vi.h"

#define NMARKS_BASE		('z' - 'a' + 3)
#define NMARKS			32

/* line operations */
struct lopt {
	char *ins;		/* inserted text */
	char *del;		/* deleted text */
	int pos, n_ins, n_del;	/* modification location */
	int pos_off;		/* cursor line offset */
	int seq;		/* operation number */
	int *mark, *mark_off;	/* saved marks */
};

/* line buffers */
struct lbuf {
	int mark[NMARKS];	/* mark lines */
	int mark_off[NMARKS];	/* mark line offsets */
	char **ln;		/* buffer lines */
	char *ln_glob;		/* line global mark */
	int ln_n;		/* number of lines in ln[] */
	int ln_sz;		/* size of ln[] */
	int useq;		/* current operation sequence */
	struct lopt *hist;	/* buffer history */
	int hist_sz;		/* size of hist[] */
	int hist_n;		/* current history head in hist[] */
	int hist_u;		/* current undo head in hist[] */
	int useq_zero;		/* useq for lbuf_saved() */
	int useq_last;		/* useq before hist[] */
};

struct lbuf *lbuf_make(void)
{
	struct lbuf *lb = malloc(sizeof(*lb));
	int i;
	memset(lb, 0, sizeof(*lb));
	for (i = 0; i < LEN(lb->mark); i++)
		lb->mark[i] = -1;
	lb->useq = 1;
	return lb;
}

static void lopt_done(struct lopt *lo)
{
	free(lo->ins);
	free(lo->del);
	free(lo->mark);
	free(lo->mark_off);
}

static void lbuf_savemark(struct lbuf *lb, struct lopt *lo, int m)
{
	if (lb->mark[m] >= 0) {
		if (!lo->mark) {
			lo->mark = malloc(sizeof(lb->mark));
			lo->mark_off = malloc(sizeof(lb->mark_off));
			memset(lo->mark, 0xff, sizeof(lb->mark));
		}
		lo->mark[m] = lb->mark[m];
		lo->mark_off[m] = lb->mark_off[m];
	}
}

static void lbuf_loadmark(struct lbuf *lb, struct lopt *lo, int m)
{
	if (lo->mark && lo->mark[m] >= 0) {
		lb->mark[m] = lo->mark[m];
		lb->mark_off[m] = lo->mark_off[m];
	}
}

static int markidx(int mark)
{
	if (islower(mark))
		return mark - 'a';
	if (mark == '\'' || mark == '`')
		return 'z' - 'a' + 1;
	if (mark == '*')
		return 'z' - 'a' + 2;
	if (mark == '[')
		return 'z' - 'a' + 3;
	if (mark == ']')
		return 'z' - 'a' + 4;
	if (mark == '^')
		return 'z' - 'a' + 5;
	return -1;
}

static void lbuf_markcopy(struct lbuf *lb, int dst, int src)
{
	lb->mark[markidx(dst)] = lb->mark[markidx(src)];
	lb->mark_off[markidx(dst)] = lb->mark_off[markidx(src)];
}

static void lbuf_savepos(struct lbuf *lb, struct lopt *lo)
{
	if (lb->mark[markidx('^')] >= 0)
		lo->pos_off = lb->mark_off[markidx('^')];
	lbuf_markcopy(lb, '*', '^');
}

static void lbuf_loadpos(struct lbuf *lb, struct lopt *lo)
{
	lb->mark[markidx('^')] = lo->pos;
	lb->mark_off[markidx('^')] = lo->pos_off;
	lbuf_markcopy(lb, '*', '^');
}

void lbuf_free(struct lbuf *lb)
{
	int i;
	for (i = 0; i < lb->ln_n; i++)
		free(lb->ln[i]);
	for (i = 0; i < lb->hist_n; i++)
		lopt_done(&lb->hist[i]);
	free(lb->hist);
	free(lb->ln);
	free(lb->ln_glob);
	free(lb);
}

static int linelength(char *s)
{
	char *r = strchr(s, '\n');
	return r ? r - s + 1 : strlen(s);
}

static int linecount(char *s)
{
	int n;
	for (n = 0; s && *s; n++)
		s += linelength(s);
	return n;
}


/* low-level line replacement */
static void lbuf_replace(struct lbuf *lb, char *s, int pos, int n_del)
{
	int n_ins = linecount(s);
	int i;
	while (lb->ln_n + n_ins - n_del >= lb->ln_sz) {
		int nsz = lb->ln_sz + (lb->ln_sz ? lb->ln_sz : 512);
		char **nln = malloc(nsz * sizeof(nln[0]));
		char *nln_glob = malloc(nsz * sizeof(nln_glob[0]));
		memcpy(nln, lb->ln, lb->ln_n * sizeof(lb->ln[0]));
		memcpy(nln_glob, lb->ln_glob, lb->ln_n * sizeof(lb->ln_glob[0]));
		free(lb->ln);
		free(lb->ln_glob);
		lb->ln = nln;
		lb->ln_glob = nln_glob;
		lb->ln_sz = nsz;
	}
	for (i = 0; i < n_del; i++)
		free(lb->ln[pos + i]);
	if (n_ins != n_del) {
		memmove(lb->ln + pos + n_ins, lb->ln + pos + n_del,
			(lb->ln_n - pos - n_del) * sizeof(lb->ln[0]));
		memmove(lb->ln_glob + pos + n_ins, lb->ln_glob + pos + n_del,
			(lb->ln_n - pos - n_del) * sizeof(lb->ln_glob[0]));
	}
	lb->ln_n += n_ins - n_del;
	for (i = 0; i < n_ins; i++) {
		int l = s ? linelength(s) : 0;
		int l_nonl = l - (s[l - 1] == '\n');
		char *n = malloc(l_nonl + 2);
		memcpy(n, s, l_nonl);
		n[l_nonl + 0] = '\n';
		n[l_nonl + 1] = '\0';
		lb->ln[pos + i] = n;
		s += l;
	}
	for (i = n_del; i < n_ins; i++)
		lb->ln_glob[pos + i] = 0;
	for (i = 0; i < LEN(lb->mark); i++) {	/* updating marks */
		if (!s && lb->mark[i] >= pos && lb->mark[i] < pos + n_del)
			lb->mark[i] = -1;
		else if (lb->mark[i] >= pos + n_del)
			lb->mark[i] += n_ins - n_del;
		else if (lb->mark[i] >= pos + n_ins)
			lb->mark[i] = pos + n_ins - 1;
	}
	lbuf_mark(lb, '[', pos, 0);
	lbuf_mark(lb, ']', pos + (n_ins ? n_ins - 1 : 0), 0);
}

/* append undo/redo history */
static void lbuf_opt(struct lbuf *lb, char *buf, int pos, int n_del)
{
	struct lopt *lo;
	int i;
	for (i = lb->hist_u; i < lb->hist_n; i++)
		lopt_done(&lb->hist[i]);
	lb->hist_n = lb->hist_u;
	if (lb->hist_n == lb->hist_sz) {
		int sz = lb->hist_sz + (lb->hist_sz ? lb->hist_sz : 128);
		struct lopt *hist = malloc(sz * sizeof(hist[0]));
		memcpy(hist, lb->hist, lb->hist_n * sizeof(hist[0]));
		free(lb->hist);
		lb->hist = hist;
		lb->hist_sz = sz;
	}
	lo = &lb->hist[lb->hist_n];
	lb->hist_n++;
	lb->hist_u = lb->hist_n;
	memset(lo, 0, sizeof(*lo));
	lo->pos = pos;
	lo->n_del = n_del;
	lo->del = n_del ? lbuf_cp(lb, pos, pos + n_del) : NULL;
	lo->n_ins = buf ? linecount(buf) : 0;
	lo->ins = buf ? uc_dup(buf) : NULL;
	lo->seq = lb->useq;
	lbuf_savepos(lb, lo);
	for (i = 0; i < NMARKS_BASE; i++)
		if (lb->mark[i] >= pos && lb->mark[i] < pos + n_del)
			lbuf_savemark(lb, lo, i);
}

int lbuf_rd(struct lbuf *lbuf, int fd, int beg, int end)
{
	char buf[1 << 10];
	struct sbuf *sb;
	long nr;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	if (!nr)
		lbuf_edit(lbuf, sbuf_buf(sb), beg, end);
	sbuf_free(sb);
	return nr != 0;
}

static long write_fully(int fd, void *buf, long sz)
{
	long nw = 0, nc = 0;
	while (nw < sz && (nc = write(fd, buf + nw, sz - nw)) >= 0)
		nw += nc;
	return nc >= 0 ? nw : -1;
}

int lbuf_wr(struct lbuf *lbuf, int fd, int beg, int end)
{
	char buf[4096];
	long buf_len = 0, sz = 0;
	int i;
	for (i = beg; i < end; i++) {
		char *ln = lbuf->ln[i];
		long nl = strlen(ln);
		if (buf_len > 0 && buf_len + nl > sizeof(buf)) {
			if (write_fully(fd, buf, buf_len) < 0)
				return 1;
			buf_len = 0;
		}
		if (nl >= sizeof(buf)) {
			if (write_fully(fd, ln, nl) < 0)
				return 1;
		} else {
			memcpy(buf + buf_len, ln, nl);
			buf_len += nl;
		}
		sz += nl;
	}
	if (buf_len > 0 && write_fully(fd, buf, buf_len) < 0)
		return 1;
	ftruncate(fd, sz);
	return 0;
}

/* replace lines beg through end with buf */
void lbuf_edit(struct lbuf *lb, char *buf, int beg, int end)
{
	if (beg > lb->ln_n)
		beg = lb->ln_n;
	if (end > lb->ln_n)
		end = lb->ln_n;
	if (beg == end && !buf)
		return;
	lbuf_opt(lb, buf, beg, end - beg);
	lbuf_replace(lb, buf, beg, end - beg);
}

char *lbuf_cp(struct lbuf *lb, int beg, int end)
{
	struct sbuf *sb;
	int i;
	sb = sbuf_make();
	for (i = beg; i < end; i++)
		if (i < lb->ln_n)
			sbuf_str(sb, lb->ln[i]);
	return sbuf_done(sb);
}

char *lbuf_get(struct lbuf *lb, int pos)
{
	return pos >= 0 && pos < lb->ln_n ? lb->ln[pos] : NULL;
}

int lbuf_len(struct lbuf *lb)
{
	return lb->ln_n;
}

void lbuf_mark(struct lbuf *lbuf, int mark, int pos, int off)
{
	if (markidx(mark) >= 0) {
		lbuf->mark[markidx(mark)] = pos;
		lbuf->mark_off[markidx(mark)] = off;
	}
}

int lbuf_jump(struct lbuf *lbuf, int mark, int *pos, int *off)
{
	int mk = markidx(mark);
	if (mk < 0 || lbuf->mark[mk] < 0)
		return 1;
	*pos = lbuf->mark[mk];
	if (off)
		*off = lbuf->mark_off[mk];
	return 0;
}

int lbuf_undo(struct lbuf *lb)
{
	int useq, i;
	if (!lb->hist_u)
		return 1;
	useq = lb->hist[lb->hist_u - 1].seq;
	while (lb->hist_u && lb->hist[lb->hist_u - 1].seq == useq) {
		struct lopt *lo = &lb->hist[--(lb->hist_u)];
		lbuf_replace(lb, lo->del, lo->pos, lo->n_ins);
		lbuf_loadpos(lb, lo);
		for (i = 0; i < LEN(lb->mark); i++)
			lbuf_loadmark(lb, lo, i);
	}
	return 0;
}

int lbuf_redo(struct lbuf *lb)
{
	int useq;
	if (lb->hist_u == lb->hist_n)
		return 1;
	useq = lb->hist[lb->hist_u].seq;
	while (lb->hist_u < lb->hist_n && lb->hist[lb->hist_u].seq == useq) {
		struct lopt *lo = &lb->hist[lb->hist_u++];
		lbuf_replace(lb, lo->ins, lo->pos, lo->n_del);
		lbuf_loadpos(lb, lo);
	}
	return 0;
}

static int lbuf_seq(struct lbuf *lb)
{
	return lb->hist_u ? lb->hist[lb->hist_u - 1].seq : lb->useq_last;
}

/* mark buffer as saved and, if clear, clear the undo history */
void lbuf_saved(struct lbuf *lb, int clear)
{
	int i;
	if (clear) {
		for (i = 0; i < lb->hist_n; i++)
			lopt_done(&lb->hist[i]);
		lb->hist_n = 0;
		lb->hist_u = 0;
		lb->useq_last = lb->useq;
	}
	lb->useq_zero = lbuf_seq(lb);
	lbuf_modified(xb);
}

/* was the file modified since the last lbuf_modreset() */
int lbuf_modified(struct lbuf *lb)
{
	lb->useq++;
	return lbuf_seq(lb) != lb->useq_zero;
}

/* mark the line for ex global command */
void lbuf_globset(struct lbuf *lb, int pos, int dep)
{
	lb->ln_glob[pos] |= 1 << dep;
}

/* return and clear ex global command mark */
int lbuf_globget(struct lbuf *lb, int pos, int dep)
{
	int o = lb->ln_glob[pos] & (1 << dep);
	lb->ln_glob[pos] &= ~(1 << dep);
	return o > 0;
}
