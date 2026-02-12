#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regex.h"

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
#define RI_REPT		'r'	/* repetition */
#define RI_JUMP		'j'	/* jump to the given instruction */
#define RI_MARK		'm'	/* mark the current position */
#define RI_MATCH	'q'	/* pattern or sub-pattern is matched */

/* regular expression atom */
struct ratom {
	int ra;			/* atom type (RA_*) */
	char *s;		/* atom argument */
};

/* regular expression instruction */
struct rinst {
	struct ratom ra;	/* regular expression atom (RI_ATOM) */
	int ri;			/* instruction type (RI_*) */
	int a1, a2;		/* destination of RI_FORK and RI_JUMP */
	int mincnt, maxcnt;	/* repetitions (RI_REPT) */
	int mark;		/* mark (RI_MARK) */
};

/* regular expression program */
struct regex {
	struct rinst *p;	/* the program */
	int n;			/* number of instructions */
	int flg;		/* regcomp() flags */
};

/* regular expression matching state */
struct rstate {
	char *s;			/* the current position in the string */
	char *o;			/* the beginning of the string */
	int pc;				/* program counter */
	int flg;			/* flags passed to regcomp() and regexec() */
	int subcnt;			/* number of groups to return */
	int *mark_num, *mark_val;	/* mark number and value */
	int mark_pos, mark_len;		/* last item in mark_num[] and mark_val[] */
	int *past_s, *past_mpos;	/* previous values of s and mark_pos */
	int past_pos, past_len;		/* last pushed value in past_s and past_mpos[] */
	/* before heap allocations, these buffers are used */
	int _mark_val[128], _mark_num[128];
	int _past_s[128], _past_mpos[128];
};

static void rstate_init(struct rstate *rs, char *s, int flg, int subcnt)
{
	rs->o = s;
	rs->s = s;
	rs->flg = flg;
	rs->mark_num = rs->_mark_num;
	rs->mark_val = rs->_mark_val;
	rs->mark_pos = 0;
	rs->mark_len = LEN(rs->_mark_num);
	rs->past_s = rs->_past_s;
	rs->past_mpos = rs->_past_mpos;
	rs->past_pos = 0;
	rs->past_len = LEN(rs->_past_s);
	rs->subcnt = subcnt;
}

static void rstate_done(struct rstate *rs)
{
	if (rs->mark_num != rs->_mark_num) {
		free(rs->mark_num);
		free(rs->mark_val);
	}
	if (rs->past_s != rs->_past_s) {
		free(rs->past_s);
		free(rs->past_mpos);
	}
}

static int rstate_push(struct rstate *rs)
{
	if (rs->past_pos >= rs->past_len) {
		int past_len = rs->past_len * 2;
		int *past_s = malloc(past_len * sizeof(past_s[0]));
		int *past_mpos = malloc(past_len * sizeof(past_mpos[0]));
		if (!past_s || !past_mpos) {
			free(past_s);
			free(past_mpos);
			return 1;
		}
		memcpy(past_s, rs->past_s, rs->past_len * sizeof(past_s[0]));
		memcpy(past_mpos, rs->past_mpos, rs->past_len * sizeof(past_mpos[0]));
		if (rs->past_s != rs->_past_s) {
			free(rs->past_s);
			free(rs->past_mpos);
		}
		rs->past_s = past_s;
		rs->past_mpos = past_mpos;
		rs->past_len = past_len;
	}
	rs->past_s[rs->past_pos] = rs->s - rs->o;
	rs->past_mpos[rs->past_pos] = rs->mark_pos;
	rs->past_pos++;
	return 0;
}

static void rstate_pop(struct rstate *rs)
{
	rs->past_pos--;
	rs->s = rs->o + rs->past_s[rs->past_pos];
	rs->mark_pos = rs->past_mpos[rs->past_pos];
}

static int rstate_mark(struct rstate *rs, int mark)
{
	if (mark >= rs->subcnt * 2)
		return 0;
	if (rs->mark_pos >= rs->mark_len) {
		int mark_len = rs->mark_len * 2;
		int *mark_num = malloc(mark_len * sizeof(mark_num[0]));
		int *mark_val = malloc(mark_len * sizeof(mark_val[0]));
		if (!mark_num || !mark_val) {
			free(mark_num);
			free(mark_val);
			return 1;
		}
		memcpy(mark_num, rs->mark_num, rs->mark_len * sizeof(mark_num[0]));
		memcpy(mark_val, rs->mark_val, rs->mark_len * sizeof(mark_val[0]));
		if (rs->mark_num != rs->_mark_num) {
			free(rs->mark_num);
			free(rs->mark_val);
		}
		rs->mark_num = mark_num;
		rs->mark_val = mark_val;
		rs->mark_len = mark_len;
	}
	rs->mark_num[rs->mark_pos] = mark;
	rs->mark_val[rs->mark_pos] = rs->s - rs->o;
	rs->mark_pos++;
	return 0;
}

static void rstate_marks(struct rstate *rs, regmatch_t sub[])
{
	int i;
	for (i = 0; i < rs->mark_pos; i++) {
		int mark = rs->mark_num[i];
		if (mark & 1)
			sub[mark >> 1].rm_eo = rs->mark_val[i];
		else
			sub[mark >> 1].rm_so = rs->mark_val[i];
	}
}

/* regular expression tree; used for parsing */
struct rnode {
	struct ratom ra;	/* regular expression atom (RN_ATOM) */
	struct rnode *c1, *c2;	/* children */
	int mincnt, maxcnt;	/* number of repetitions */
	int grp;		/* group number */
	int rn;			/* node type (RN_*) */
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
	if (~c & 0xc0)		/* ASCII or invalid */
		return c > 0;
	if (~c & 0x20)
		return 2;
	if (~c & 0x10)
		return 3;
	if (~c & 0x08)
		return 4;
	return 1;
}

static int uc_dec(char *s)
{
	int c = (unsigned char) s[0];
	if (~c & 0xc0)		/* ASCII or invalid */
		return c;
	if (~c & 0x20)
		return ((c & 0x1f) << 6) | (s[1] & 0x3f);
	if (~c & 0x10)
		return ((c & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
	if (~c & 0x08)
		return ((c & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
	return c;
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
	char *s;
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
		s = *pat;
		while ((s == *pat) || !strchr(".^$[(|)*?+{\\", (unsigned char) s[0])) {
			int l = uc_len(s);
			if (s != *pat && s[l] != '\0' && strchr("*?+{", (unsigned char) s[l]))
				break;
			s += uc_len(s);
		}
		len = s - *pat;
		ra->s = malloc(len + 1);
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
	if (ra->ra == RA_CHR && !(rs->flg & REG_ICASE)) {
		char *s = ra->s;
		char *r = rs->s;
		while (*s && *s == *r)
			s++, r++;
		if (*s)
			return 1;
		rs->s = r;
		return 0;
	}
	if (ra->ra == RA_CHR) {
		int pos = 0;
		while (ra->s[pos]) {
			int c1 = uc_dec(ra->s + pos);
			int c2 = uc_dec(rs->s + pos);
			if (rs->flg & REG_ICASE && c1 < 128 && isupper(c1))
				c1 = tolower(c1);
			if (rs->flg & REG_ICASE && c2 < 128 && isupper(c2))
				c2 = tolower(c2);
			if (c1 != c2)
				return 1;
			pos += uc_len(ra->s + pos);
		}
		rs->s += pos;
		return 0;
	}
	if (ra->ra == RA_ANY) {
		if (!rs->s[0] || (rs->s[0] == '\n' && !!(rs->flg & REG_NEWLINE)))
			return 1;
		rs->s += uc_len(rs->s);
		return 0;
	}
	if (ra->ra == RA_BRK) {
		int c = uc_dec(rs->s);
		if (!c || (c == '\n' && !!(rs->flg & REG_NEWLINE) && ra->s[1] == '^'))
			return 1;
		rs->s += uc_len(rs->s);
		return brk_match(ra->s + 1, c, rs->flg);
	}
	if (ra->ra == RA_BEG && rs->s == rs->o)
		return !!(rs->flg & REG_NOTBOL);
	if (ra->ra == RA_BEG && rs->s > rs->o && rs->s[-1] == '\n')
		return !(rs->flg & REG_NEWLINE);
	if (ra->ra == RA_END && rs->s[0] == '\0')
		return !!(rs->flg & REG_NOTEOL);
	if (ra->ra == RA_END && rs->s[0] == '\n')
		return !(rs->flg & REG_NEWLINE);
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
	struct rnode *rnode = NULL;
	if ((*pat)[0] != '(')
		return NULL;
	++*pat;
	if ((*pat)[0] != ')') {
		rnode = rnode_parse(pat);
		if (!rnode)
			return NULL;
	}
	if ((*pat)[0] != ')') {
		rnode_free(rnode);
		return NULL;
	}
	++*pat;
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
	if (!rnode)
		return NULL;
	if ((*pat)[0] == '*' || (*pat)[0] == '?') {
		rnode->mincnt = 0;
		rnode->maxcnt = (*pat)[0] == '*' ? -1 : 1;
		++*pat;
	}
	if ((*pat)[0] == '+') {
		rnode->mincnt = 1;
		rnode->maxcnt = -1;
		++*pat;
	}
	if ((*pat)[0] == '{') {
		rnode->mincnt = 0;
		rnode->maxcnt = 0;
		++*pat;
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
		++*pat;
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
	++*pat;
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

static int rnode_grpnum(struct rnode *rnode, int num)
{
	int cur = 0;
	if (!rnode)
		return 0;
	if (rnode->rn == RN_GRP)
		rnode->grp = num + cur++;
	cur += rnode_grpnum(rnode->c1, num + cur);
	cur += rnode_grpnum(rnode->c2, num + cur);
	return cur;
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
		mark = re_insert(p, RI_MARK);
		p->p[mark].mark = 2 * n->grp;
		rnode_emit(n->c1, p);
		mark = re_insert(p, RI_MARK);
		p->p[mark].mark = 2 * n->grp + 1;
	}
	if (n->rn == RN_ATOM) {
		int atom = re_insert(p, RI_ATOM);
		ratom_copy(&p->p[atom].ra, &n->ra);
	}
}

static void rnode_emit(struct rnode *n, struct regex *p)
{
	int rept;
	if (!n)
		return;
	if (n->mincnt == 0 && n->maxcnt == 0)
		return;
	if (n->mincnt == 1 && n->maxcnt == 1) {
		rnode_emitnorep(n, p);
		return;
	}
	rept = re_insert(p, RI_REPT);
	p->p[rept].mincnt = n->mincnt;
	p->p[rept].maxcnt = n->maxcnt;
	rnode_emitnorep(n, p);
	re_insert(p, RI_MATCH);
	p->p[rept].a1 = p->n;
}

int regcomp(regex_t *preg, char *pat, int flg)
{
	struct rnode *rnode = rnode_parse(&pat);
	struct regex *re;
	int n = rnode_count(rnode) + 3;
	int mark;
	if (!rnode)
		return 1;
	rnode_grpnum(rnode, 1);
	re = malloc(sizeof(*re));
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
	struct rinst *ri = NULL;
	while (1) {
		ri = &re->p[rs->pc];
		if (ri->ri == RI_ATOM) {
			if (ratom_match(&ri->ra, rs))
				return 1;
			rs->pc++;
			continue;
		}
		if (ri->ri == RI_MARK) {
			rstate_mark(rs, ri->mark);
			rs->pc++;
			continue;
		}
		if (ri->ri == RI_JUMP) {
			rs->pc = ri->a1;
			continue;
		}
		if (ri->ri == RI_REPT) {
			int cnt2 = ri->maxcnt > 0 ? ri->maxcnt - ri->mincnt : 1024;
			int body = rs->pc + 1;
			int i;
			for (i = 0; i < ri->mincnt; i++) {
				rs->pc = body;
				if (re_rec(re, rs))
					return 1;
			}
			for (i = 0; i < cnt2; i++) {
				if (rstate_push(rs))
					break;
				rs->pc = body;
				if (re_rec(re, rs)) {
					rstate_pop(rs);
					break;
				}
			}
			for (; i > 0; i--) {
				rs->pc = ri->a1;
				if (!re_rec(re, rs))
					return 0;
				rstate_pop(rs);
			}
			rs->pc = ri->a1;
			return re_rec(re, rs);
		}
		if (ri->ri == RI_FORK) {
			if (rstate_push(rs))
				return 1;
			rs->pc = ri->a1;
			if (!re_rec(re, rs))
				return 0;
			rstate_pop(rs);
			rs->pc = ri->a2;
			continue;
		}
		break;
	}
	return ri->ri != RI_MATCH;
}

static int re_recmatch(struct regex *re, struct rstate *rs)
{
	rs->pc = 0;
	rs->mark_pos = 0;
	rs->past_pos = 0;
	if (!re_rec(re, rs))
		return 0;
	return 1;
}

int regexec(regex_t *preg, char *s, int nsub, regmatch_t psub[], int flg)
{
	struct regex *re = *preg;
	struct rstate rs;
	char *o = s;
	int i;
	rstate_init(&rs, s, re->flg | flg, flg & REG_NOSUB ? 0 : nsub);
	for (i = 0; i < nsub; i++) {
		psub[i].rm_so = -1;
		psub[i].rm_eo = -1;
	}
	while (*o) {
		rs.s = o = s;
		s += uc_len(s);
		if (!re_recmatch(re, &rs)) {
			rstate_marks(&rs, psub);
			rstate_done(&rs);
			return 0;
		}
	}
	rstate_done(&rs);
	return 1;
}

int regerror(int errcode, regex_t *preg, char *errbuf, int errbuf_size)
{
	return 0;
}
