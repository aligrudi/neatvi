#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "vi.h"

#define NMARKS		128

/* line operations */
struct lopt {
	char *buf;		/* text inserted or deleted */
	int ins;		/* insertion operation if non-zero */
	int beg, end;
	int seq;		/* operation number */
	int *mark, *mark_off;	/* saved marks */
};

/* line buffers */
struct lbuf {
	int mark[NMARKS];	/* mark lines */
	int mark_off[NMARKS];	/* mark line offsets */
	char **ln;		/* buffer lines */
	int ln_n;		/* number of lines in ln[] */
	int ln_sz;		/* size of ln[] */
	int useq;		/* current operation sequence */
	struct lopt *hist;	/* buffer history */
	int hist_sz;		/* size of hist[] */
	int hist_n;		/* current history head in hist[] */
	int hist_u;		/* current undo head in hist[] */
	int mod_new;		/* clear modification marks */
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
	free(lo->buf);
	free(lo->mark);
	free(lo->mark_off);
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
	free(lb);
}

/* insert a line at pos */
static void lbuf_insertline(struct lbuf *lb, int pos, char *s)
{
	if (lb->ln_n == lb->ln_sz) {
		int nsz = lb->ln_sz + 512;
		char **nln = malloc(nsz * sizeof(nln[0]));
		memcpy(nln, lb->ln, lb->ln_n * sizeof(lb->ln[0]));
		free(lb->ln);
		lb->ln = nln;
		lb->ln_sz = nsz;
	}
	memmove(lb->ln + pos + 1, lb->ln + pos,
		(lb->ln_n - pos) * sizeof(lb->ln[0]));
	lb->ln_n++;
	lb->ln[pos] = s;
}

/* low-level insertion */
static void lbuf_insert(struct lbuf *lb, int pos, char *s)
{
	int lb_len = lbuf_len(lb);
	int beg = pos, end;
	char *r;
	int i;
	while ((r = strchr(s, '\n'))) {
		char *n = malloc(r - s + 2);
		memcpy(n, s, r - s + 1);
		n[r - s + 1] = '\0';
		lbuf_insertline(lb, pos++, n);
		s = r + 1;
	}
	for (i = 0; i < LEN(lb->mark); i++)	/* updating marks */
		if (lb->mark[i] >= pos)
			lb->mark[i] += lbuf_len(lb) - lb_len;
	end = beg + lbuf_len(lb) - lb_len;
	if (lb->mod_new || lb->mark['['] < 0 || lb->mark['['] > beg)
		lbuf_mark(lb, '[', beg, 0);
	if (lb->mod_new || lb->mark[']'] < 0 || lb->mark[']'] < end - 1)
		lbuf_mark(lb, ']', end - 1, 0);
	lb->mod_new = 0;
}

/* low-level deletion */
static void lbuf_delete(struct lbuf *lb, int beg, int end)
{
	int i;
	for (i = beg; i < end; i++)
		free(lb->ln[i]);
	memmove(lb->ln + beg, lb->ln + end, (lb->ln_n - end) * sizeof(lb->ln[0]));
	lb->ln_n -= end - beg;
	for (i = 0; i < LEN(lb->mark); i++)	/* updating marks */
		if (lb->mark[i] > beg)
			lb->mark[i] = MAX(beg, lb->mark[i] + beg - end);
	if (lb->mod_new || lb->mark['['] < 0 || lb->mark['['] > beg)
		lbuf_mark(lb, '[', beg, 0);
	if (lb->mod_new || lb->mark[']'] < 0 || lb->mark[']'] < beg)
		lbuf_mark(lb, ']', beg, 0);
	lb->mod_new = 0;
}

/* append undo/redo history */
static void lbuf_opt(struct lbuf *lb, int ins, int beg, int end)
{
	struct lopt *lo;
	int i;
	for (i = lb->hist_u; i < lb->hist_n; i++)
		lopt_done(&lb->hist[i]);
	lb->hist_n = lb->hist_u;
	if (lb->hist_n == lb->hist_sz) {
		int sz = lb->hist_sz + 128;
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
	lo->ins = ins;
	lo->beg = beg;
	lo->end = end;
	lo->buf = lbuf_cp(lb, beg, end);
	lo->seq = lb->useq;
}

void lbuf_rd(struct lbuf *lbuf, int fd, int pos)
{
	char buf[1 << 10];
	struct sbuf *sb;
	int nr;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	lbuf_put(lbuf, pos, sbuf_buf(sb));
	sbuf_free(sb);
}

void lbuf_wr(struct lbuf *lbuf, int fd, int beg, int end)
{
	int i;
	for (i = beg; i < end; i++)
		write(fd, lbuf->ln[i], strlen(lbuf->ln[i]));
}

void lbuf_rm(struct lbuf *lb, int beg, int end)
{
	if (end > lb->ln_n)
		end = lb->ln_n;
	if (beg == end)
		return;
	lbuf_opt(lb, 0, beg, end);
	lbuf_delete(lb, beg, end);
}

void lbuf_put(struct lbuf *lb, int pos, char *s)
{
	int lb_len = lbuf_len(lb);
	if (!*s)
		return;
	lbuf_insert(lb, pos, s);
	lbuf_opt(lb, 1, pos, pos + lbuf_len(lb) - lb_len);
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
	if (mark >= NMARKS)
		return;
	lbuf->mark[mark] = pos;
	lbuf->mark_off[mark] = off;
}

int lbuf_jump(struct lbuf *lbuf, int mark, int *pos, int *off)
{
	if (mark >= NMARKS || lbuf->mark[mark] < 0)
		return 1;
	*pos = lbuf->mark[mark];
	if (off)
		*off = lbuf->mark_off[mark];
	return 0;
}

static void lbuf_savemarks(struct lbuf *lb, struct lopt *lo)
{
	int i;
	lo->mark = malloc(sizeof(lb->mark));
	lo->mark_off = malloc(sizeof(lb->mark_off));
	for (i = 0; i < LEN(lb->mark); i++)
		lo->mark[i] = -1;
	lo->mark['*'] = lb->mark['*'];
	lo->mark_off['*'] = lb->mark_off['*'];
}

static void lbuf_loadmarks(struct lbuf *lb, struct lopt *lo)
{
	int i;
	for (i = 0; lo->mark && i < LEN(lb->mark); i++) {
		if (lo->mark[i] >= 0) {
			lb->mark[i] = lo->mark[i];
			lb->mark_off[i] = lo->mark_off[i];
		}
	}
}

int lbuf_undo(struct lbuf *lb)
{
	int useq;
	if (!lb->hist_u)
		return 1;
	useq = lb->hist[lb->hist_u - 1].seq;
	lb->mod_new = 1;
	while (lb->hist_u && lb->hist[lb->hist_u - 1].seq == useq) {
		struct lopt *lo = &lb->hist[--(lb->hist_u)];
		if (lo->ins)
			lbuf_delete(lb, lo->beg, lo->end);
		else
			lbuf_insert(lb, lo->beg, lo->buf);
		lbuf_loadmarks(lb, lo);
	}
	return 0;
}

int lbuf_redo(struct lbuf *lb)
{
	int useq;
	if (lb->hist_u == lb->hist_n)
		return 1;
	useq = lb->hist[lb->hist_u].seq;
	lb->mod_new = 1;
	while (lb->hist_u < lb->hist_n && lb->hist[lb->hist_u].seq == useq) {
		struct lopt *lo = &lb->hist[lb->hist_u++];
		if (lo->ins)
			lbuf_insert(lb, lo->beg, lo->buf);
		else
			lbuf_delete(lb, lo->beg, lo->end);
		lbuf_loadmarks(lb, lo);
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
	struct lopt *lo = lb->hist_n ? &lb->hist[lb->hist_n - 1] : NULL;
	if (lb->hist_u == lb->hist_n && lo && !lo->mark)
		lbuf_savemarks(lb, lo);
	lb->mod_new = 1;
	lb->useq++;
	return lbuf_seq(lb) != lb->useq_zero;
}
