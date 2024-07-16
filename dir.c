#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

static struct rset *dir_rslr;	/* pattern of marks for left-to-right strings */
static struct rset *dir_rsrl;	/* pattern of marks for right-to-left strings */
static struct rset *dir_rsctx;	/* direction context patterns */

static int dir_match(char **chrs, int beg, int end, int ctx, int *rec,
		int *r_beg, int *r_end, int *c_beg, int *c_end, int *dir)
{
	int subs[16 * 2];
	struct rset *rs = ctx < 0 ? dir_rsrl : dir_rslr;
	struct sbuf *str = sbuf_make();
	int grp;
	int flg = (beg ? RE_NOTBOL : 0) | (chrs[end][0] ? RE_NOTEOL : 0);
	int found = -1;
	sbuf_mem(str, chrs[beg], chrs[end] - chrs[beg]);
	if (rs)
		found = rset_find(rs, sbuf_buf(str), LEN(subs) / 2, subs, flg);
	if (found >= 0 && r_beg && r_end && c_beg && c_end) {
		char *s = sbuf_buf(str);
		conf_dirmark(found, NULL, NULL, dir, &grp);
		*r_beg = beg + uc_off(s, subs[0]);
		*r_end = beg + uc_off(s, subs[1]);
		*c_beg = subs[grp * 2 + 0] >= 0 ?
			beg + uc_off(s, subs[grp * 2 + 0]) : *r_beg;
		*c_end = subs[grp * 2 + 1] >= 0 ?
			beg + uc_off(s, subs[grp * 2 + 1]) : *r_end;
		*rec = grp > 0;
	}
	sbuf_free(str);
	return found < 0;
}

static void dir_reverse(int *ord, int beg, int end)
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

/* reorder the characters based on direction marks and characters */
static void dir_fix(char **chrs, int *ord, int dir, int beg, int end)
{
	int r_beg, r_end, c_beg, c_end;
	int c_dir, c_rec;
	while (beg < end && !dir_match(chrs, beg, end, dir, &c_rec,
				&r_beg, &r_end, &c_beg, &c_end, &c_dir)) {
		if (dir < 0)
			dir_reverse(ord, r_beg, r_end);
		if (c_dir < 0)
			dir_reverse(ord, c_beg, c_end);
		if (c_beg == r_beg)
			c_beg++;
		if (c_rec)
			dir_fix(chrs, ord, c_dir, c_beg, c_end);
		beg = r_end;
	}
}

/* return the direction context of the given line */
int dir_context(char *s)
{
	int found = -1;
	int dir;
	if (xtd > +1)
		return +1;
	if (xtd < -1)
		return -1;
	if (xtd == 0 && ~((unsigned char) *s) & 0x80)
		return +1;
	if (dir_rsctx)
		found = rset_find(dir_rsctx, s ? s : "", 0, NULL, 0);
	if (!conf_dircontext(found, NULL, &dir))
		return dir;
	return xtd < 0 ? -1 : +1;
}

/* reorder the characters in s */
void dir_reorder(char *s, int *ord)
{
	int n;
	char **chrs = uc_chop(s, &n);
	int dir = dir_context(s);
	if (n && chrs[n - 1][0] == '\n') {
		ord[n - 1] = n - 1;
		n--;
	}
	dir_fix(chrs, ord, dir, 0, n);
	free(chrs);
}

void dir_init(void)
{
	char *relr[128];
	char *rerl[128];
	char *ctx[128];
	int curctx, i;
	char *pat;
	for (i = 0; !conf_dirmark(i, &pat, &curctx, NULL, NULL); i++) {
		relr[i] = curctx >= 0 ? pat : NULL;
		rerl[i] = curctx <= 0 ? pat : NULL;
	}
	dir_rslr = rset_make(i, relr, 0);
	dir_rsrl = rset_make(i, rerl, 0);
	for (i = 0; !conf_dircontext(i, &pat, NULL); i++)
		ctx[i] = pat;
	dir_rsctx = rset_make(i, ctx, 0);
}

void dir_done(void)
{
	if (dir_rslr)
		rset_free(dir_rslr);
	if (dir_rsrl)
		rset_free(dir_rsrl);
	if (dir_rsctx)
		rset_free(dir_rsctx);
}
