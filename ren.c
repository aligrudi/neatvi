/* rendering strings */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

/* specify the screen position of the characters in s */
int *ren_position(char *s)
{
	int i, n;
	char **chrs = uc_chop(s, &n);
	int *off, *pos;
	int cpos = 0;
	pos = malloc((n + 1) * sizeof(pos[0]));
	for (i = 0; i < n; i++)
		pos[i] = i;
	dir_reorder(s, pos);
	off = malloc(n * sizeof(off[0]));
	for (i = 0; i < n; i++)
		off[pos[i]] = i;
	for (i = 0; i < n; i++) {
		pos[off[i]] = cpos;
		cpos += ren_cwid(chrs[off[i]], cpos);
	}
	pos[n] = cpos;
	free(chrs);
	free(off);
	return pos;
}

int ren_wid(char *s)
{
	int *pos = ren_position(s);
	int n = uc_slen(s);
	int ret = pos[n];
	free(pos);
	return ret;
}

/* find the next character after visual position p; if cur, start from p itself */
static int pos_next(int *pos, int n, int p, int cur)
{
	int i, ret = -1;
	for (i = 0; i < n; i++)
		if (pos[i] - !cur >= p && (ret < 0 || pos[i] < pos[ret]))
			ret = i;
	return ret >= 0 ? pos[ret] : -1;
}

/* find the previous character after visual position p; if cur, start from p itself */
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
	int *pos = ren_position(s);
	int ret = off < n ? pos[off] : 0;
	free(pos);
	return ret;
}

/* convert visual position to character offset */
int ren_off(char *s, int p)
{
	int off = -1;
	int n = uc_slen(s);
	int *pos = ren_position(s);
	int i;
	p = pos_prev(pos, n, p, 1);
	for (i = 0; i < n; i++)
		if (pos[i] == p)
			off = i;
	free(pos);
	return off >= 0 ? off : 0;
}

/* adjust cursor position */
int ren_cursor(char *s, int p)
{
	int n, next;
	int *pos;
	if (!s)
		return 0;
	n = uc_slen(s);
	pos = ren_position(s);
	p = pos_prev(pos, n, p, 1);
	if (uc_code(uc_chr(s, ren_off(s, p))) == '\n')
		p = pos_prev(pos, n, p, 0);
	next = pos_next(pos, n, p, 0);
	p = (next >= 0 ? next : pos[n]) - 1;
	free(pos);
	return p >= 0 ? p : 0;
}

/* real cursor position; never past EOL */
int ren_noeol(char *s, int p)
{
	int n;
	int *pos;
	if (!s)
		return 0;
	n = uc_slen(s);
	pos = ren_position(s);
	p = pos_prev(pos, n, p, 1);
	if (uc_code(uc_chr(s, ren_off(s, p))) == '\n')
		p = pos_prev(pos, n, p, 0);
	free(pos);
	return p >= 0 ? p : 0;
}

/* the position of the next character */
int ren_next(char *s, int p, int dir)
{
	int n = uc_slen(s);
	int *pos = ren_position(s);
	p = pos_prev(pos, n, p, 1);
	if (dir >= 0)
		p = pos_next(pos, n, p, 0);
	else
		p = pos_prev(pos, n, p, 0);
	free(pos);
	return p;
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

	if (c2 < c1)
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

static char *ren_placeholder(char *s)
{
	char *src, *dst;
	int wid, i;
	int c = uc_code(s);
	for (i = 0; !conf_placeholder(i, &src, &dst, &wid); i++)
		if (uc_code(src) == c)
			return dst;
	return NULL;
}

int ren_cwid(char *s, int pos)
{
	char *src, *dst;
	int wid, i;
	if (s[0] == '\t')
		return 8 - (pos & 7);
	for (i = 0; !conf_placeholder(i, &src, &dst, &wid); i++)
		if (uc_code(src) == uc_code(s))
			return wid;
	return 1;
}

char *ren_translate(char *s, char *ln)
{
	char *p = ren_placeholder(s);
	return p || !xshape ? p : uc_shape(ln, s);
}
