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
};

/* line buffers */
struct lbuf {
	int mark[NMARKS];	/* mark lines */
	int mark_off[NMARKS];	/* mark line offsets */
	struct lopt hist[128];	/* buffer history */
	int undo;		/* current index into hist[] */
	int useq;		/* current operation sequence */
	char **ln;		/* lines */
	int ln_n;		/* number of lbuf in l[] */
	int ln_sz;		/* size of l[] */
	int mark_mod;		/* clear modification marks */
};

struct lbuf *lbuf_make(void)
{
	struct lbuf *lb = malloc(sizeof(*lb));
	int i;
	memset(lb, 0, sizeof(*lb));
	for (i = 0; i < LEN(lb->mark); i++)
		lb->mark[i] = -1;
	return lb;
}

void lbuf_free(struct lbuf *lb)
{
	int i;
	for (i = 0; i < lb->ln_n; i++)
		free(lb->ln[i]);
	for (i = 0; i < LEN(lb->hist); i++)
		free(lb->hist[i].buf);
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
	int len = strlen(s);
	struct sbuf *sb;
	int lb_len = lbuf_len(lb);
	int beg = pos, end;
	int i;
	sb = sbuf_make();
	for (i = 0; i < len; i++) {
		sbuf_chr(sb, (unsigned char) s[i]);
		if (s[i] == '\n') {
			lbuf_insertline(lb, pos++, sbuf_done(sb));
			sb = sbuf_make();
		}
	}
	sbuf_free(sb);
	for (i = 0; i < LEN(lb->mark); i++)	/* updating marks */
		if (lb->mark[i] >= pos)
			lb->mark[i] += lbuf_len(lb) - lb_len;
	end = beg + lbuf_len(lb) - lb_len;
	if (lb->mark_mod || lb->mark['['] < 0 || lb->mark['['] > beg)
		lbuf_mark(lb, '[', beg, 0);
	if (lb->mark_mod || lb->mark[']'] < 0 || lb->mark[']'] < end - 1)
		lbuf_mark(lb, ']', end - 1, 0);
	lb->mark_mod = 0;
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
	if (lb->mark_mod || lb->mark['['] < 0 || lb->mark['['] > beg)
		lbuf_mark(lb, '[', beg, 0);
	if (lb->mark_mod || lb->mark[']'] < 0 || lb->mark[']'] < beg)
		lbuf_mark(lb, ']', beg, 0);
	lb->mark_mod = 0;
}

/* append undo/redo history */
static void lbuf_opt(struct lbuf *lb, int ins, int beg, int end)
{
	struct lopt *lo = &lb->hist[0];
	int n = LEN(lb->hist);
	int i;
	if (lb->undo) {
		for (i = 0; i < lb->undo; i++)
			free(lb->hist[i].buf);
		memmove(lb->hist + 1, lb->hist + lb->undo,
			(n - lb->undo) * sizeof(lb->hist[0]));
		for (i = n - lb->undo + 1; i < n; i++)
			lb->hist[i].buf = NULL;
	} else {
		free(lb->hist[n - 1].buf);
		memmove(lb->hist + 1, lb->hist, (n - 1) * sizeof(lb->hist[0]));
	}
	lo->ins = ins;
	lo->beg = beg;
	lo->end = end;
	lo->buf = lbuf_cp(lb, beg, end);
	lo->seq = lb->useq;
	lb->undo = 0;
}

void lbuf_rd(struct lbuf *lbuf, int fd, int pos)
{
	char buf[1 << 8];
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
	lbuf_opt(lb, 0, beg, end);
	lbuf_delete(lb, beg, end);
}

void lbuf_put(struct lbuf *lb, int pos, char *s)
{
	int lb_len = lbuf_len(lb);
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

static struct lopt *lbuf_lopt(struct lbuf *lb, int i)
{
	struct lopt *lo = &lb->hist[i];
	return i >= 0 && i < LEN(lb->hist) && lo->buf ? lo : NULL;
}

int lbuf_undo(struct lbuf *lb)
{
	struct lopt *lo = lbuf_lopt(lb, lb->undo);
	int useq = lo ? lo->seq : 0;
	if (!lo)
		return 1;
	lb->mark_mod = 1;
	while (lo && lo->seq == useq) {
		lb->undo++;
		if (lo->ins)
			lbuf_delete(lb, lo->beg, lo->end);
		else
			lbuf_insert(lb, lo->beg, lo->buf);
		lo = lbuf_lopt(lb, lb->undo);
	}
	return 0;
}

int lbuf_redo(struct lbuf *lb)
{
	struct lopt *lo = lbuf_lopt(lb, lb->undo - 1);
	int useq = lo ? lo->seq : 0;
	if (!lo)
		return 1;
	lb->mark_mod = 1;
	while (lo && lo->seq == useq) {
		lb->undo--;
		if (lo->ins)
			lbuf_insert(lb, lo->beg, lo->buf);
		else
			lbuf_delete(lb, lo->beg, lo->end);
		lo = lbuf_lopt(lb, lb->undo - 1);
	}
	return 0;
}

void lbuf_undofree(struct lbuf *lb)
{
	int i;
	for (i = 0; i < LEN(lb->hist); i++)
		free(lb->hist[i].buf);
	memset(lb->hist, 0, sizeof(lb->hist));
	lb->undo = 0;
}

void lbuf_undomark(struct lbuf *lbuf)
{
	lbuf->mark_mod = 1;
	lbuf->useq++;
}
