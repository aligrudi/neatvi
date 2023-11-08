#include <stdio.h>
#include <string.h>
#include "vi.h"

int lbuf_indents(struct lbuf *lb, int r)
{
	char *ln = lbuf_get(lb, r);
	int o;
	if (!ln)
		return 0;
	for (o = 0; uc_isspace(ln); o++)
		ln = uc_next(ln);
	return o;
}

static int uc_nextdir(char **s, char *beg, int dir)
{
	if (dir < 0) {
		if (*s == beg)
			return 1;
		*s = uc_prev(beg, *s);
	} else {
		*s = uc_next(*s);
		if (!(*s)[0])
			return 1;
	}
	return 0;
}

int lbuf_findchar(struct lbuf *lb, char *cs, int cmd, int n, int *row, int *off)
{
	char *ln = lbuf_get(lb, *row);
	char *s;
	int dir = (cmd == 'f' || cmd == 't') ? +1 : -1;
	if (!ln)
		return 1;
	if (n < 0)
		dir = -dir;
	if (n < 0)
		n = -n;
	s = uc_chr(ln, *off);
	while (n > 0 && !uc_nextdir(&s, ln, dir))
		if (uc_code(s) == uc_code(cs))
			n--;
	if (!n && (cmd == 't' || cmd == 'T'))
		uc_nextdir(&s, ln, -dir);
	if (!n)
		*off = uc_off(ln, s - ln);
	return n != 0;
}

int lbuf_search(struct lbuf *lb, char *kw, int dir, int *r, int *o, int *len)
{
	int offs[2];
	int found = 0;
	int r0 = *r, o0 = *o;
	int i;
	struct rstr *re = rstr_make(kw, xic ? RE_ICASE : 0);
	if (!re)
		return 1;
	for (i = r0; !found && i >= 0 && i < lbuf_len(lb); i += dir) {
		char *s = lbuf_get(lb, i);
		int off = dir > 0 && r0 == i ? uc_chr(s, o0 + 1) - s : 0;
		while (rstr_find(re, s + off, 1, offs,
				off ? RE_NOTBOL : 0) >= 0) {
			if (dir < 0 && r0 == i &&
					uc_off(s, off + offs[0]) >= o0)
				break;
			found = 1;
			*o = uc_off(s, off + offs[0]);
			*r = i;
			*len = uc_off(s + off + offs[0], offs[1] - offs[0]);
			off += offs[1] > offs[0] ? offs[1] : offs[1] + 1;
			if (dir > 0 || !s[off] || s[off] == '\n')
				break;
		}
	}
	rstr_free(re);
	return !found;
}

int lbuf_paragraphbeg(struct lbuf *lb, int dir, int *row, int *off)
{
	while (*row >= 0 && *row < lbuf_len(lb) && !strcmp("\n", lbuf_get(lb, *row)))
		*row += dir;
	while (*row >= 0 && *row < lbuf_len(lb) && strcmp("\n", lbuf_get(lb, *row)))
		*row += dir;
	*row = MAX(0, MIN(*row, lbuf_len(lb) - 1));
	*off = 0;
	return 0;
}

int lbuf_sectionbeg(struct lbuf *lb, int dir, char *sec, int *row, int *off)
{
	struct rstr *re = rstr_make(sec, 0);
	*row += dir;
	while (*row >= 0 && *row < lbuf_len(lb)) {
		if (rstr_find(re, lbuf_get(lb, *row), 0, NULL, 0) >= 0)
			break;
		*row += dir;
	}
	rstr_free(re);
	*row = MAX(0, MIN(*row, lbuf_len(lb) - 1));
	*off = 0;
	return 0;
}

static int lbuf_lnnext(struct lbuf *lb, int dir, int *r, int *o)
{
	int off = *o + dir;
	if (off < 0 || !lbuf_get(lb, *r) || off >= uc_slen(lbuf_get(lb, *r)))
		return 1;
	*o = off;
	return 0;
}

int lbuf_eol(struct lbuf *lb, int row)
{
	int len = lbuf_get(lb, row) ? uc_slen(lbuf_get(lb, row)) : 0;
	return len ? len - 1 : 0;
}

static int lbuf_next(struct lbuf *lb, int dir, int *r, int *o)
{
	if (dir < 0 && *r >= lbuf_len(lb))
		*r = MAX(0, lbuf_len(lb) - 1);
	if (lbuf_lnnext(lb, dir, r, o)) {
		if (!lbuf_get(lb, *r + dir))
			return -1;
		*r += dir;
		*o = dir > 0 ? 0 : lbuf_eol(lb, *r);
		return 0;
	}
	return 0;
}

/* return a pointer to the character at visual position c of line r */
static char *lbuf_chr(struct lbuf *lb, int r, int c)
{
	char *ln = lbuf_get(lb, r);
	return ln ? uc_chr(ln, c) : "";
}

/* move to the last character of the word */
static int lbuf_wordlast(struct lbuf *lb, int kind, int dir, int *row, int *off)
{
	if (!kind || !(uc_kind(lbuf_chr(lb, *row, *off)) & kind))
		return 0;
	while (uc_kind(lbuf_chr(lb, *row, *off)) & kind)
		if (lbuf_next(lb, dir, row, off))
			return 1;
	if (!(uc_kind(lbuf_chr(lb, *row, *off)) & kind))
		lbuf_next(lb, -dir, row, off);
	return 0;
}

int lbuf_wordbeg(struct lbuf *lb, int big, int dir, int *row, int *off)
{
	int nl;
	lbuf_wordlast(lb, big ? 3 : uc_kind(lbuf_chr(lb, *row, *off)), dir, row, off);
	nl = uc_code(lbuf_chr(lb, *row, *off)) == '\n';
	if (lbuf_next(lb, dir, row, off))
		return 1;
	while (uc_isspace(lbuf_chr(lb, *row, *off))) {
		nl += uc_code(lbuf_chr(lb, *row, *off)) == '\n';
		if (nl == 2)
			return 0;
		if (lbuf_next(lb, dir, row, off))
			return 1;
	}
	return 0;
}

int lbuf_wordend(struct lbuf *lb, int big, int dir, int *row, int *off)
{
	int nl = 0;
	if (!uc_isspace(lbuf_chr(lb, *row, *off))) {
		if (lbuf_next(lb, dir, row, off))
			return 1;
		nl = dir < 0 && uc_code(lbuf_chr(lb, *row, *off)) == '\n';
	}
	nl += dir > 0 && uc_code(lbuf_chr(lb, *row, *off)) == '\n';
	while (uc_isspace(lbuf_chr(lb, *row, *off))) {
		if (lbuf_next(lb, dir, row, off))
			return 1;
		nl += uc_code(lbuf_chr(lb, *row, *off)) == '\n';
		if (nl == 2) {
			if (dir < 0)
				lbuf_next(lb, -dir, row, off);
			return 0;
		}
	}
	if (lbuf_wordlast(lb, big ? 3 : uc_kind(lbuf_chr(lb, *row, *off)), dir, row, off))
		return 1;
	return 0;
}

/* move to the matching character */
int lbuf_pair(struct lbuf *lb, int *row, int *off)
{
	int r = *row, o = *off;
	char *pairs = "()[]{}";
	int pchr;		/* parenthesis character */
	int pidx;		/* index into pairs[] */
	int dep = 1;		/* parenthesis depth */
	while ((pchr = (unsigned char) lbuf_chr(lb, r, o)[0]) && !strchr(pairs, pchr))
		o++;
	if (!pchr)
		return 1;
	pidx = strchr(pairs, pchr) - pairs;
	while (!lbuf_next(lb, (pidx & 1) ? -1 : +1, &r, &o)) {
		int c = (unsigned char) lbuf_chr(lb, r, o)[0];
		if (c == pairs[pidx ^ 1])
			dep--;
		if (c == pairs[pidx])
			dep++;
		if (!dep) {
			*row = r;
			*off = o;
			return 0;
		}
	}
	return 1;
}
