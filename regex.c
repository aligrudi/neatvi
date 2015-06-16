#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regex.h"

#define NGRPS		64
#define NREPS		128

#define MAX(a, b)	((a) < (b) ? (b) : (a))
#define LEN(a)		(sizeof(a) / sizeof((a)[0]))

/* regular expressions atoms */
#define RA_CHR		'\0'	/* character literal */
#define RA_BEG		'^'	/* string start */
#define RA_END		'$'	/* string end */
#define RA_ANY		'.'	/* any character */
#define RA_BRK		'['	/* bracket expression */
#define RA_WBEG		'<'	/* word start */
#define RA_WEND		'>'	/* word end */

/* regular expression node types */
#define RN_ATOM		'\0'	/* regular expression */
#define RN_CAT		'c'	/* concatenation */
#define RN_ALT		'|'	/* alternate expressions */
#define RN_GRP		'('	/* pattern group */

/* regular expression program instructions */
#define RI_ATOM		'\0'	/* regular expression */
#define RI_FORK		'f'	/* fork the execution */
#define RI_JUMP		'j'	/* jump to the given instruction */
#define RI_MARK		'm'	/* mark the current position */
#define RI_MATCH	'q'	/* the pattern is matched */

/* regular expression atom */
struct ratom {
	int ra;			/* atom type (RA_*) */
	char *s;		/* atom argument */
};

/* regular expression instruction */
struct rinst {
	int ri;			/* instruction type (RI_*) */
	struct ratom ra;	/* regular expression atom (RI_ATOM) */
	int a1, a2;		/* destination of RE_FORK and RI_JUMP */
	int mark;		/* mark (RI_MARK) */
};

/* regular expression program */
struct regex {
	struct rinst *p;	/* the program */
	int n;			/* number of instructions */
	int grpcnt;		/* number of groups */
	int flg;		/* regcomp() flags */
};

/* regular expression matching state */
struct rstate {
	int mark[NGRPS * 2];	/* marks for RI_MARK */
	int pc;			/* program counter */
	char *s;		/* the current position in the string */
	char *o;		/* the beginning of the string */
	int flg;		/* flags passed to regcomp() and regexec() */
};

/* regular expression tree; used for parsing */
struct rnode {
	int rn;			/* node type (RN_*) */
	struct ratom ra;	/* regular expression atom (RN_ATOM) */
	int mincnt, maxcnt;	/* number of repetitions */
	struct rnode *c1, *c2;	/* children */
};

static struct rnode *rnode_make(int rn, struct rnode *c1, struct rnode *c2)
{
	struct rnode *rnode = malloc(sizeof(*rnode));
	memset(rnode, 0, sizeof(*rnode));
	rnode->rn = rn;
	rnode->c1 = c1;
	rnode->c2 = c2;
	rnode->mincnt = 1;
	rnode->maxcnt = 1;
	return rnode;
}

static void rnode_free(struct rnode *rnode)
{
	if (rnode->c1)
		rnode_free(rnode->c1);
	if (rnode->c2)
		rnode_free(rnode->c2);
	free(rnode->ra.s);
	free(rnode);
}

static int uc_len(char *s)
{
	int c = (unsigned char) s[0];
	if (c > 0 && c <= 0x7f)
		return 1;
	if (c >= 0xfc)
		return 6;
	if (c >= 0xf8)
		return 5;
	if (c >= 0xf0)
		return 4;
	if (c >= 0xe0)
		return 3;
	if (c >= 0xc0)
		return 2;
	return c != 0;
}

static int uc_dec(char *s)
{
	int result;
	int l = uc_len(s);
	if (l <= 1)
		return (unsigned char) *s;
	result = (0x3f >> --l) & (unsigned char) *s++;
	while (l--)
		result = (result << 6) | ((unsigned char) *s++ & 0x3f);
	return result;
}

static void ratom_copy(struct ratom *dst, struct ratom *src)
{
	dst->ra = src->ra;
	dst->s = NULL;
	if (src->s) {
		int len = strlen(src->s);
		dst->s = malloc(len + 1);
		memcpy(dst->s, src->s, len + 1);
	}
}

static int brk_len(char *s)
{
	int n = 1;
	if (s[n] == '^')	/* exclusion mark */
		n++;
	if (s[n] == ']')	/* handling []a] */
		n++;
	while (s[n] && s[n] != ']') {
		if (s[n] == '[' && (s[n + 1] == ':' || s[n + 1] == '='))
			while (s[n] && s[n] != ']')
				n++;
		if (s[n])
			n++;
	}
	return s[n] == ']' ? n + 1 : n;
}

static void ratom_readbrk(struct ratom *ra, char **pat)
{
	int len = brk_len(*pat);
	ra->ra = RA_BRK;
	ra->s = malloc(len + 1);
	memcpy(ra->s, *pat, len);
	ra->s[len] = '\0';
	*pat += len;
}

static void ratom_read(struct ratom *ra, char **pat)
{
	int len;
	switch ((unsigned char) **pat) {
	case '.':
		ra->ra = RA_ANY;
		(*pat)++;
		break;
	case '^':
		ra->ra = RA_BEG;
		(*pat)++;
		break;
	case '$':
		ra->ra = RA_END;
		(*pat)++;
		break;
	case '[':
		ratom_readbrk(ra, pat);
		break;
	case '\\':
		if ((*pat)[1] == '<' || (*pat)[1] == '>') {
			ra->ra = (*pat)[1] == '<' ? RA_WBEG : RA_WEND;
			*pat += 2;
			break;
		}
		(*pat)++;
	default:
		ra->ra = RA_CHR;
		len = uc_len(*pat);
		ra->s = malloc(8);
		memcpy(ra->s, *pat, len);
		ra->s[len] = '\0';
		*pat += len;
	}
}

static char *uc_beg(char *beg, char *s)
{
	while (s > beg && (((unsigned char) *s) & 0xc0) == 0x80)
		s--;
	return s;
}

static int isword(char *s)
{
	int c = (unsigned char) s[0];
	return isalnum(c) || c == '_' || c > 127;
}

static char *brk_classes[][2] = {
	{":alnum:", "a-zA-Z0-9"},
	{":alpha:", "a-zA-Z"},
	{":blank:", " \t"},
	{":digit:", "0-9"},
	{":lower:", "a-z"},
	{":print:", "\x20-\x7e"},
	{":punct:", "][!\"#$%&'()*+,./:;<=>?@\\^_`{|}~-"},
	{":space:", " \t\r\n\v\f"},
	{":upper:", "A-Z"},
	{":word:", "a-zA-Z0-9_"},
	{":xdigit:", "a-fA-F0-9"},
};

static int brk_match(char *brk, int c, int flg)
{
	int beg, end;
	int i;
	int not = brk[0] == '^';
	char *p = not ? brk + 1 : brk;
	char *p0 = p;
	if (flg & REG_ICASE && c < 128 && isupper(c))
		c = tolower(c);
	while (*p && (p == p0 || *p != ']')) {
		if (p[0] == '[' && p[1] == ':') {
			for (i = 0; i < LEN(brk_classes); i++) {
				char *cc = brk_classes[i][0];
				char *cp = brk_classes[i][1];
				if (!strncmp(cc, p + 1, strlen(cc)))
					if (!brk_match(cp, c, flg))
						return not;
			}
			p += brk_len(p);
			continue;
		}
		beg = uc_dec(p);
		p += uc_len(p);
		end = beg;
		if (p[0] == '-' && p[1] && p[1] != ']') {
			p++;
			end = uc_dec(p);
			p += uc_len(p);
		}
		if (flg & REG_ICASE && beg < 128 && isupper(beg))
			beg = tolower(beg);
		if (flg & REG_ICASE && end < 128 && isupper(end))
			end = tolower(end);
		if (c >= beg && c <= end)
			return not;
	}
	return !not;
}

static int ratom_match(struct ratom *ra, struct rstate *rs)
{
	if (ra->ra == RA_CHR) {
		int c1 = uc_dec(ra->s);
		int c2 = uc_dec(rs->s);
		if (rs->flg & REG_ICASE && c1 < 128 && isupper(c1))
			c1 = tolower(c1);
		if (rs->flg & REG_ICASE && c2 < 128 && isupper(c2))
			c2 = tolower(c2);
		if (c1 != c2)
			return 1;
		rs->s += uc_len(ra->s);
		return 0;
	}
	if (ra->ra == RA_ANY) {
		if (!rs->s[0])
			return 1;
		rs->s += uc_len(rs->s);
		return 0;
	}
	if (ra->ra == RA_BRK) {
		int c = uc_dec(rs->s);
		if (!c)
			return 1;
		rs->s += uc_len(rs->s);
		return brk_match(ra->s + 1, c, rs->flg);
	}
	if (ra->ra == RA_BEG && !(rs->flg & REG_NOTBOL))
		return rs->s != rs->o;
	if (ra->ra == RA_END && !(rs->flg & REG_NOTEOL))
		return rs->s[0] != '\0';
	if (ra->ra == RA_WBEG)
		return !((rs->s == rs->o || !isword(uc_beg(rs->o, rs->s - 1))) &&
			isword(rs->s));
	if (ra->ra == RA_WEND)
		return !(rs->s != rs->o && isword(uc_beg(rs->o, rs->s - 1)) &&
			(!rs->s[0] || !isword(rs->s)));
	return 1;
}

static struct rnode *rnode_parse(char **pat);

static struct rnode *rnode_grp(char **pat)
{
	struct rnode *rnode;
	if ((*pat)[0] != '(')
		return NULL;
	*pat += 1;
	rnode = rnode_parse(pat);
	if ((*pat)[0] != ')') {
		rnode_free(rnode);
		return NULL;
	}
	*pat += 1;
	return rnode_make(RN_GRP, rnode, NULL);
}

static struct rnode *rnode_atom(char **pat)
{
	struct rnode *rnode;
	if (!**pat)
		return NULL;
	if ((*pat)[0] == '|' || (*pat)[0] == ')')
		return NULL;
	if ((*pat)[0] == '(') {
		rnode = rnode_grp(pat);
	} else {
		rnode = rnode_make(RN_ATOM, NULL, NULL);
		ratom_read(&rnode->ra, pat);
	}
	if ((*pat)[0] == '*' || (*pat)[0] == '?') {
		rnode->mincnt = 0;
		rnode->maxcnt = (*pat)[0] == '*' ? -1 : 1;
		(*pat)++;
	}
	if ((*pat)[0] == '+') {
		rnode->mincnt = 1;
		rnode->maxcnt = -1;
		*pat += 1;
	}
	if ((*pat)[0] == '{') {
		rnode->mincnt = 0;
		rnode->maxcnt = 0;
		*pat += 1;
		while (isdigit((unsigned char) **pat))
			rnode->mincnt = rnode->mincnt * 10 + *(*pat)++ - '0';
		if (**pat == ',') {
			(*pat)++;
			if ((*pat)[0] == '}')
				rnode->maxcnt = -1;
			while (isdigit((unsigned char) **pat))
				rnode->maxcnt = rnode->maxcnt * 10 + *(*pat)++ - '0';
		} else {
			rnode->maxcnt = rnode->mincnt;
		}
		*pat += 1;
		if (rnode->mincnt > NREPS || rnode->maxcnt > NREPS) {
			rnode_free(rnode);
			return NULL;
		}
	}
	return rnode;
}

static struct rnode *rnode_seq(char **pat)
{
	struct rnode *c1 = rnode_atom(pat);
	struct rnode *c2;
	if (!c1)
		return NULL;
	c2 = rnode_seq(pat);
	return c2 ? rnode_make(RN_CAT, c1, c2) : c1;
}

static struct rnode *rnode_parse(char **pat)
{
	struct rnode *c1 = rnode_seq(pat);
	struct rnode *c2;
	if ((*pat)[0] != '|')
		return c1;
	*pat += 1;
	c2 = rnode_parse(pat);
	return c2 ? rnode_make(RN_ALT, c1, c2) : c1;
}

static int rnode_count(struct rnode *rnode)
{
	int n = 1;
	if (!rnode)
		return 0;
	if (rnode->rn == RN_CAT)
		n = rnode_count(rnode->c1) + rnode_count(rnode->c2);
	if (rnode->rn == RN_ALT)
		n = rnode_count(rnode->c1) + rnode_count(rnode->c2) + 2;
	if (rnode->rn == RN_GRP)
		n = rnode_count(rnode->c1) + 2;
	if (rnode->mincnt == 0 && rnode->maxcnt == 0)
		return 0;
	if (rnode->mincnt == 1 && rnode->maxcnt == 1)
		return n;
	if (rnode->maxcnt < 0) {
		n = (rnode->mincnt + 1) * n + 1;
	} else {
		n = (rnode->mincnt + rnode->maxcnt) * n +
			rnode->maxcnt - rnode->mincnt;
	}
	if (!rnode->mincnt)
		n++;
	return n;
}

static int re_insert(struct regex *p, int ri)
{
	p->p[p->n++].ri = ri;
	return p->n - 1;
}

static void rnode_emit(struct rnode *n, struct regex *p);

static void rnode_emitnorep(struct rnode *n, struct regex *p)
{
	int fork, done, mark;
	if (n->rn == RN_ALT) {
		fork = re_insert(p, RI_FORK);
		p->p[fork].a1 = p->n;
		rnode_emit(n->c1, p);
		done = re_insert(p, RI_JUMP);
		p->p[fork].a2 = p->n;
		rnode_emit(n->c2, p);
		p->p[done].a1 = p->n;
	}
	if (n->rn == RN_CAT) {
		rnode_emit(n->c1, p);
		rnode_emit(n->c2, p);
	}
	if (n->rn == RN_GRP) {
		int grp = p->grpcnt++ + 1;
		mark = re_insert(p, RI_MARK);
		p->p[mark].mark = 2 * grp;
		rnode_emit(n->c1, p);
		mark = re_insert(p, RI_MARK);
		p->p[mark].mark = 2 * grp + 1;
	}
	if (n->rn == RN_ATOM) {
		int atom = re_insert(p, RI_ATOM);
		ratom_copy(&p->p[atom].ra, &n->ra);
	}
}

static void rnode_emit(struct rnode *n, struct regex *p)
{
	int last;
	int jmpend[NREPS];
	int jmpend_cnt = 0;
	int i;
	if (!n)
		return;
	if (n->mincnt == 0 && n->maxcnt == 0)
		return;
	if (n->mincnt == 1 && n->maxcnt == 1) {
		rnode_emitnorep(n, p);
		return;
	}
	if (n->mincnt == 0) {
		int fork = re_insert(p, RI_FORK);
		p->p[fork].a1 = p->n;
		jmpend[jmpend_cnt++] = fork;
	}
	for (i = 0; i < MAX(1, n->mincnt); i++) {
		last = p->n;
		rnode_emitnorep(n, p);
	}
	if (n->maxcnt < 0) {
		int fork;
		fork = re_insert(p, RI_FORK);
		p->p[fork].a1 = last;
		p->p[fork].a2 = p->n;
	}
	for (i = MAX(1, n->mincnt); i < n->maxcnt; i++) {
		int fork = re_insert(p, RI_FORK);
		p->p[fork].a1 = p->n;
		jmpend[jmpend_cnt++] = fork;
		rnode_emitnorep(n, p);
	}
	for (i = 0; i < jmpend_cnt; i++)
		p->p[jmpend[i]].a2 = p->n;
}

int regcomp(regex_t *preg, char *pat, int flg)
{
	struct rnode *rnode = rnode_parse(&pat);
	struct regex *re = malloc(sizeof(*re));
	int n = rnode_count(rnode) + 3;
	int mark;
	memset(re, 0, sizeof(*re));
	re->p = malloc(n * sizeof(re->p[0]));
	memset(re->p, 0, n * sizeof(re->p[0]));
	mark = re_insert(re, RI_MARK);
	re->p[mark].mark = 0;
	rnode_emit(rnode, re);
	mark = re_insert(re, RI_MARK);
	re->p[mark].mark = 1;
	mark = re_insert(re, RI_MATCH);
	rnode_free(rnode);
	re->flg = flg;
	*preg = re;
	return 0;
}

void regfree(regex_t *preg)
{
	struct regex *re = *preg;
	int i;
	for (i = 0; i < re->n; i++)
		if (re->p[i].ri == RI_ATOM)
			free(re->p[i].ra.s);
	free(re->p);
	free(re);
}

static int re_rec(struct regex *re, struct rstate *rs)
{
	struct rinst *ri = &re->p[rs->pc];
	if (ri->ri == RI_ATOM) {
		if (ratom_match(&ri->ra, rs))
			return 1;
		rs->pc++;
		return re_rec(re, rs);
	}
	if (ri->ri == RI_MARK) {
		if (ri->mark < NGRPS)
			rs->mark[ri->mark] = rs->s - rs->o;
		rs->pc++;
		return re_rec(re, rs);
	}
	if (ri->ri == RI_JUMP) {
		rs->pc = ri->a1;
		return re_rec(re, rs);
	}
	if (ri->ri == RI_FORK) {
		struct rstate base = *rs;
		rs->pc = ri->a1;
		if (!re_rec(re, rs))
			return 0;
		*rs = base;
		rs->pc = ri->a2;
		return re_rec(re, rs);
	}
	if (ri->ri == RI_MATCH)
		return 0;
	return 1;
}

static int re_recmatch(struct regex *re, struct rstate *rs, int nsub, regmatch_t *psub)
{
	int i;
	rs->pc = 0;
	for (i = 0; i < LEN(rs->mark); i++)
		rs->mark[i] = -1;
	if (!re_rec(re, rs)) {
		for (i = 0; i < nsub; i++) {
			psub[i].rm_so = i * 2 < LEN(rs->mark) ? rs->mark[i * 2] : -1;
			psub[i].rm_eo = i * 2 < LEN(rs->mark) ? rs->mark[i * 2 + 1] : -1;
		}
		return 0;
	}
	return 1;
}

int regexec(regex_t *preg, char *s, int nsub, regmatch_t psub[], int flg)
{
	struct regex *re = *preg;
	struct rstate rs;
	memset(&rs, 0, sizeof(rs));
	rs.flg = re->flg | flg;
	rs.o = s;
	while (*s) {
		rs.s = s++;
		if (!re_recmatch(re, &rs, flg & REG_NOSUB ? 0 : nsub, psub))
			return 0;
	}
	return 1;
}

int regerror(int errcode, regex_t *preg, char *errbuf, int errbuf_size)
{
	return 0;
}
