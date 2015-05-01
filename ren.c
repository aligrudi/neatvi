/* rendering strings */
/*
 * Overview:
 * + ren_translate() replaces the characters if necessary.
 * + ren_position() specifies the position of characters on the screen.
 * + ren_reorder() is called by ren_position() and changes the order of characters.
 * + ren_highlight() performs syntax highlighting.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

static int bidi_maximalregion(char *s, int n, int dir, char **chrs, int idx, int *beg, int *end)
{
	while (idx < n && uc_dir(chrs[idx]) * dir >= 0)
		idx++;
	*beg = idx;
	*end = idx;
	while (idx < n && uc_dir(chrs[idx]) * dir <= 0) {
		if (uc_dir(chrs[idx]) * dir < 0)
			*end = idx + 1;
		idx++;
	}
	return *beg >= *end;
}

static void bidi_reverse(int *ord, int beg, int end)
{
	end--;
	while (beg < end) {
		int tmp = ord[beg];
		ord[beg] = ord[end];
		ord[end] = tmp;
		beg++;
		end--;
	}
}

int ren_dir(char *s)
{
	return +1;
}

/* reorder the characters in s */
static void ren_reorder(char *s, int *ord)
{
	int beg = 0, end = 0, n;
	char **chrs = uc_chop(s, &n);
	int dir = ren_dir(s);
	while (!bidi_maximalregion(s, n, dir, chrs, end, &beg, &end))
		bidi_reverse(ord, beg, end);
	free(chrs);
}

/* specify the screen position of the characters in s */
static int *ren_position(char *s, int *beg, int *end)
{
	int i, n;
	char **chrs = uc_chop(s, &n);
	int *off, *pos;
	int diff = 0;
	int dir = ren_dir(s);
	pos = malloc(n * sizeof(pos[0]));
	for (i = 0; i < n; i++)
		pos[i] = i;
	ren_reorder(s, pos);
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
	pos = ren_position(s1, NULL, NULL);
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
	if (ren_dir(s) >= 0)
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
	int dir = ren_dir(s ? s : "");
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
	if (ren_dir(s ? s : "") >= 0)
		p = pos_prev(pos, n, p, 1);
	else
		p = pos_next(pos, n, p, 1);
	if (dir * ren_dir(s ? s : "") >= 0)
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
	return dir * ren_dir(s) >= 0 ? end : beg;
}

/* compare two visual positions */
int ren_cmp(char *s, int pos1, int pos2)
{
	return ren_dir(s ? s : "") >= 0 ? pos1 - pos2 : pos2 - pos1;
}

/*
 * insertion offset before or after the given visual position
 *
 * When pre is nonzero, the return value indicates an offset of s,
 * which, if a character is inserted at that position, it appears
 * just before the character at pos.  If pre is zero, the inserted
 * character should appear just after the character at pos.
 */
int ren_insertionoffset(char *s, int pos, int pre)
{
	int l1;
	if (!s)
		return 0;
	l1 = ren_off(s, pos);
	return pre ? l1 : l1 + 1;
}
