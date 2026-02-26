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
	int n;
	int *pos;
	if (!s || !p)
		return 0;
	n = uc_slen(s);
	pos = ren_position(s);
	p = pos_prev(pos, n, p, 1);
	if (uc_code(uc_chr(s, ren_off(s, p))) == '\n')
		p = pos_prev(pos, n, p, 0);
	free(pos);
	return p >= 0 ? p : 0;
}

/* obtain cursor position when inserting a character before offset */
int ren_insert(char *ln, int off)
{
	struct sbuf *sb;
	char *cur;
	int *pos;
	int ret;
	if (off == 0)
		return 0;
	sb = sbuf_make();
	cur = uc_chr(ln, off - 1);
	sbuf_mem(sb, ln, cur - ln);
	sbuf_mem(sb, cur, uc_len(cur));
	sbuf_str(sb, cur);
	pos = ren_position(sbuf_buf(sb));
	ret = pos[off] - (pos[off] < pos[off - 1]);
	sbuf_free(sb);
	free(pos);
	return ret;
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

int ren_cwid(char *s, int pos)
{
	int wid;
	if (s[0] == '\t') {
		int ts = xts > 0 ? xts : 8;
		return ts - (pos % ts);
	}
	if (mapch_get(s, &wid))
		return wid;
	return uc_wid(s);
}

char *ren_translate(char *s, char *ln)
{
	char *p = s[0] != '\t' ? mapch_get(s, NULL) : NULL;
	return p || !xshape ? p : uc_shape(ln, s);
}
