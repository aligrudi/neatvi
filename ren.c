/* rendering strings */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

/* specify the screen position of the characters in s */
static int *ren_position(char *s, int *beg, int *end)
{
	int i, n;
	char **chrs = uc_chop(s, &n);
	int *off, *pos;
	int diff = 0;
	int dir = dir_context(s);
	pos = malloc(n * sizeof(pos[0]));
	for (i = 0; i < n; i++)
		pos[i] = i;
	dir_reorder(s, pos);
	off = malloc(n * sizeof(off[0]));
	for (i = 0; i < n; i++)
		off[pos[i]] = i;
	for (i = 0; i < n; i++) {
		pos[off[i]] += diff;
		if (*chrs[i] == '\t')
			diff += 8 - (pos[off[i]] & 7);
	}
	if (beg)
		*beg = 0;
	if (end)
		*end = n + diff;
	if (dir < 0) {
		if (beg)
			*beg = xcols - *end - 1;
		if (end)
			*end = xcols - 1;
		for (i = 0; i < n; i++)
			pos[i] = xcols - pos[i] - 1;
	}
	free(chrs);
	free(off);
	return pos;
}

static char *ren_translate(char *s)
{
	struct sbuf *sb = sbuf_make();
	char *r = s;
	while (*r) {
		char *c = uc_shape(s, r);
		if (!strcmp(c, "‌"))
			c = "-";
		if (!strcmp(c, "‍"))
			c = "-";
		sbuf_str(sb, c);
		r = uc_next(r);
	}
	return sbuf_done(sb);
}

char *ren_all(char *s0, int wid)
{
	int n, w = 0;
	int *pos;	/* pos[i]: the screen position of the i-th character */
	int *off;	/* off[i]: the character at screen position i */
	char **chrs;	/* chrs[i]: the i-th character in s1 */
	char *s1;
	struct sbuf *out;
	int i;
	s1 = ren_translate(s0 ? s0 : "");
	chrs = uc_chop(s1, &n);
	pos = ren_position(s0, NULL, NULL);
	for (i = 0; i < n; i++)
		if (w <= pos[i])
			w = pos[i] + 1;
	off = malloc(w * sizeof(off[0]));
	memset(off, 0xff, w * sizeof(off[0]));
	for (i = 0; i < n; i++)
		off[pos[i]] = i;
	out = sbuf_make();
	for (i = 0; i < w; i++) {
		if (off[i] >= 0 && uc_isprint(chrs[off[i]]))
			sbuf_mem(out, chrs[off[i]], uc_len(chrs[off[i]]));
		else
			sbuf_chr(out, ' ');
	}
	free(pos);
	free(off);
	free(chrs);
	free(s1);
	return sbuf_done(out);
}

int ren_last(char *s)
{
	int n = uc_slen(s);
	int *pos = ren_position(s, NULL, NULL);
	int ret = n ? pos[n - 1] : 0;
	free(pos);
	return ret;
}

/* find the next character after visual position p; if cur start from p itself */
static int pos_next(int *pos, int n, int p, int cur)
{
	int i, ret = -1;
	for (i = 0; i < n; i++)
		if (pos[i] - !cur >= p && (ret < 0 || pos[i] < pos[ret]))
			ret = i;
	return ret >= 0 ? pos[ret] : -1;
}

/* find the previous character after visual position p; if cur start from p itself */
static int pos_prev(int *pos, int n, int p, int cur)
{
	int i, ret = -1;
	for (i = 0; i < n; i++)
		if (pos[i] + !cur <= p && (ret < 0 || pos[i] > pos[ret]))
			ret = i;
	return ret >= 0 ? pos[ret] : -1;
}

/* convert visual position to character offset */
int ren_pos(char *s, int off)
{
	int n = uc_slen(s);
	int *pos = ren_position(s, NULL, NULL);
	int ret = off < n ? pos[off] : 0;
	free(pos);
	return ret;
}

/* convert visual position to character offset */
int ren_off(char *s, int p)
{
	int off = -1;
	int n = uc_slen(s);
	int *pos = ren_position(s, NULL, NULL);
	int i;
	if (dir_context(s) >= 0)
		p = pos_prev(pos, n, p, 1);
	else
		p = pos_next(pos, n, p, 1);
	for (i = 0; i < n; i++)
		if (pos[i] == p)
			off = i;
	free(pos);
	return off >= 0 ? off : 0;
}

/* adjust cursor position */
int ren_cursor(char *s, int p)
{
	int dir = dir_context(s ? s : "");
	int n, next;
	int beg, end;
	int *pos;
	if (!s)
		return 0;
	n = uc_slen(s);
	pos = ren_position(s, &beg, &end);
	if (dir >= 0)
		p = pos_prev(pos, n, p, 1);
	else
		p = pos_next(pos, n, p, 1);
	if (dir >= 0)
		next = pos_next(pos, n, p, 0);
	else
		next = pos_prev(pos, n, p, 0);
	p = (next >= 0 ? next : (dir >= 0 ? end : beg)) - dir;
	free(pos);
	return p >= 0 ? p : 0;
}

/* the position of the next character */
int ren_next(char *s, int p, int dir)
{
	int n = uc_slen(s);
	int *pos = ren_position(s, NULL, NULL);
	if (dir_context(s ? s : "") >= 0)
		p = pos_prev(pos, n, p, 1);
	else
		p = pos_next(pos, n, p, 1);
	if (dir * dir_context(s ? s : "") >= 0)
		p = pos_next(pos, n, p, 0);
	else
		p = pos_prev(pos, n, p, 0);
	free(pos);
	return p;
}

int ren_eol(char *s, int dir)
{
	int beg, end;
	int *pos = ren_position(s, &beg, &end);
	free(pos);
	return dir * dir_context(s) >= 0 ? end : beg;
}

/* compare two visual positions */
int ren_cmp(char *s, int pos1, int pos2)
{
	return dir_context(s ? s : "") >= 0 ? pos1 - pos2 : pos2 - pos1;
}

static void swap(int *i1, int *i2)
{
	int t = *i1;
	*i1 = *i2;
	*i2 = t;
}

/* the region specified by two visual positions */
int ren_region(char *s, int c1, int c2, int *l1, int *l2, int closed)
{
	int *ord;		/* ord[i]: the order of the i-th char on the screen */
	int o1, o2;
	int beg, end;
	int n = uc_slen(s);
	int i;
	if (c1 == c2 && !closed) {
		*l1 = ren_off(s, c1);
		*l2 = ren_off(s, c2);
		return 0;
	}
	ord = malloc(n * sizeof(ord[0]));
	for (i = 0; i < n; i++)
		ord[i] = i;
	dir_reorder(s, ord);

	if (ren_cmp(s, c1, c2) > 0)
		swap(&c1, &c2);
	if (!closed)
		c2 = ren_next(s, c2, -1);
	beg = ren_off(s, c1);
	end = ren_off(s, c2);
	if (end < beg)
		swap(&beg, &end);
	o1 = ord[beg];
	o2 = ord[end];
	if (o2 < o1)
		swap(&o1, &o2);
	for (i = beg; i <= end; i++)
		if (ord[i] < o1 || ord[i] > o2)
			break;
	*l1 = beg;
	*l2 = i;
	free(ord);
	return 0;
}
