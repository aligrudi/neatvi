/* rendering strings */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

/* specify the screen position of the characters in s; reordering version */
static int *ren_position_reorder(char *s)
{
	int i, n;
	char **chrs = uc_chop(s, &n);
	int *off, *pos;
	int cpos = 0;
	pos = malloc((n + 1) * sizeof(pos[0]));
	for (i = 0; i < n; i++)
		pos[i] = i;
	if (xorder)
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

/* specify the screen position of the characters in s; fast version */
int *ren_position(char *s)
{
	int cpos = 0;
	int *pos;
	int i;
	int n = uc_slen(s);
	if (n <= xlim && (xorder == 2 || (xorder == 1 && (n < strlen(s) || dir_context(s) < 0))))
		return ren_position_reorder(s);
	pos = malloc((n + 1) * sizeof(pos[0]));
	for (i = 0; i < n; i++, s += uc_len(s)) {
		pos[i] = cpos;
		cpos += ren_cwid(s, cpos);
	}
	pos[i] = cpos;
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

/* convert character offset to visual position */
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

/* return an offset before EOL */
int ren_noeol(char *s, int o)
{
	int n = s ? uc_slen(s) : 0;
	if (o >= n)
		o = MAX(0, n - 1);
	return o > 0 && uc_chr(s, o)[0] == '\n' ? o - 1 : o;
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
	return s && uc_chr(s, ren_off(s, p))[0] != '\n' ? p : -1;
}

static char *ren_placeholder(char *s, int *wid)
{
	static int bits = 0xffff;	/* common bits in placeholders */
	char *src, *dst;
	int i;
	if (bits == 0xffff) {
		for (i = 0; !conf_placeholder(i, &src, &dst, wid); i++)
			bits &= (unsigned char) *src;
	}
	if ((((unsigned char) *s) & bits) == bits) {
		for (i = 0; !conf_placeholder(i, &src, &dst, wid); i++)
			if (src[0] == s[0] && uc_code(src) == uc_code(s))
				return dst;
	}
	if (wid)
		*wid = 1;
	if (uc_isbell(s))
		return "�";
	return NULL;
}

int ren_cwid(char *s, int pos)
{
	int wid;
	if (s[0] == '\t')
		return 8 - (pos & 7);
	if (ren_placeholder(s, &wid))
		return wid;
	return uc_wid(s);
}

char *ren_translate(char *s, char *ln)
{
	char *p = ren_placeholder(s, NULL);
	return p || !xshape ? p : uc_shape(ln, s);
}
