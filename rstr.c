#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vi.h"

struct rstr {
	struct rset *rs;	/* only for regex patterns */
	char *str;		/* for simple, non-regex patterns  */
	int icase;		/* ignore case */
	int lbeg, lend;		/* match line beg/end */
	int wbeg, wend;		/* match word beg/end */
};

/* return zero if a simple pattern is given */
static int rstr_simple(struct rstr *rs, char *re)
{
	char *beg;
	char *end;
	rs->lbeg = re[0] == '^';
	if (rs->lbeg)
		re++;
	rs->wbeg = re[0] == '\\' && re[1] == '<';
	if (rs->wbeg)
		re += 2;
	beg = re;
	while (re[0] && !strchr("\\.*+?[]{}()$", (unsigned char) re[0]))
		re++;
	end = re;
	rs->wend = re[0] == '\\' && re[1] == '>';
	if (rs->wend)
		re += 2;
	rs->lend = re[0] == '$';
	if (rs->lend)
		re++;
	if (!re[0]) {
		int len = end - beg;
		rs->str = malloc(len + 1);
		memcpy(rs->str, beg, len);
		rs->str[len] = '\0';
		return 0;
	}
	return 1;
}

struct rstr *rstr_make(char *re, int flg)
{
	struct rstr *rs = malloc(sizeof(*rs));
	memset(rs, 0, sizeof(*rs));
	rs->icase = flg & RE_ICASE;
	if (rstr_simple(rs, re))
		rs->rs = rset_make(1, &re, flg);
	if (!rs->rs && !rs->str) {
		free(rs);
		return NULL;
	}
	return rs;
}

static int isword(char *s)
{
	int c = (unsigned char) s[0];
	return isalnum(c) || c == '_' || c > 127;
}

static int match_case(char *s, char *r, int icase)
{
	for (; *r && *s; s++, r++) {
		if (!icase && *s != *r)
			return 1;
		if (icase && tolower((unsigned char) *s) != tolower((unsigned char) *r))
			return 1;
	}
	return *r;
}

/* return zero if an occurrence is found */
int rstr_find(struct rstr *rs, char *s, int n, int *grps, int flg)
{
	int len;
	char *beg, *end;
	char *r;
	if (rs->rs)
		return rset_find(rs->rs, s, n, grps, flg);
	if ((rs->lbeg && (flg & RE_NOTBOL)) || (rs->lend && (flg & RE_NOTEOL)))
		return -1;
	len = strlen(rs->str);
	beg = s;
	end = s + strlen(s) - len - 1;
	if (end < beg)
		return -1;
	if (rs->lend)
		beg = end;
	if (rs->lbeg)
		end = s;
	for (r = beg; r <= end; r++) {
		if (rs->wbeg && r > s && (isword(r - 1) || !isword(r)))
			continue;
		if (rs->wend && r[len] && (!isword(r + len - 1) || isword(r + len)))
			continue;
		if (!match_case(r, rs->str, rs->icase)) {
			if (n >= 1) {
				grps[0] = r - s;
				grps[1] = r - s + len;
			}
			return 0;
		}
	}
	return -1;
}

void rstr_free(struct rstr *rs)
{
	if (rs->rs)
		rset_free(rs->rs);
	free(rs->str);
	free(rs);
}
