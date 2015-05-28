#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

#define LEN(a)		(sizeof(a) / sizeof((a)[0]))

/* return the length of a utf-8 character */
int uc_len(char *s)
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

/* the number of utf-8 characters in s */
int uc_slen(char *s)
{
	char *e = s + strlen(s);
	int i;
	for (i = 0; s < e; i++)
		s += uc_len(s);
	return i;
}

/* the unicode codepoint of the given utf-8 character */
int uc_code(char *s)
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

/* find the beginning of the character at s[i] */
char *uc_beg(char *beg, char *s)
{
	while (s > beg && (((unsigned char) *s) & 0xc0) == 0x80)
		s--;
	return s;
}

/* find the end of the character at s[i] */
char *uc_end(char *s)
{
	if (!*s || !((unsigned char) *s & 0x80))
		return s;
	if (((unsigned char) *s & 0xc0) == 0xc0)
		s++;
	while (((unsigned char) *s & 0xc0) == 0x80)
		s++;
	return s - 1;
}

/* return a pointer to the character following s */
char *uc_next(char *s)
{
	s = uc_end(s);
	return *s ? s + 1 : s;
}

/* return a pointer to the character preceding s */
char *uc_prev(char *beg, char *s)
{
	return s == beg ? beg : uc_beg(beg, s - 1);
}

int uc_wid(char *s)
{
	return 1;
}

char *uc_lastline(char *s)
{
	char *r = strrchr(s, '\n');
	return r ? r + 1 : s;
}

/* allocate and return an array for the characters in s */
char **uc_chop(char *s, int *n)
{
	char **chrs;
	int i;
	*n = uc_slen(s);
	chrs = malloc((*n + 1) * sizeof(chrs[0]));
	for (i = 0; i < *n + 1; i++) {
		chrs[i] = s;
		s = uc_next(s);
	}
	return chrs;
}

char *uc_chr(char *s, int off)
{
	int i = 0;
	while (s && *s) {
		if (i++ == off)
			return s;
		s = uc_next(s);
	}
	return s && (off < 0 || i == off) ? s : "";
}

/* the number of characters between s and s + off*/
int uc_off(char *s, int off)
{
	char *e = s + off;
	int i;
	for (i = 0; s < e && *s; i++)
		s = uc_next(s);
	return i;
}

char *uc_sub(char *s, int beg, int end)
{
	char *sbeg = uc_chr(s, beg);
	char *send = uc_chr(s, end);
	int len = sbeg && send && sbeg <= send ? send - sbeg : 0;
	char *r = malloc(len + 1);
	memcpy(r, sbeg, len);
	r[len] = '\0';
	return r;
}

char *uc_dup(char *s)
{
	char *r = malloc(strlen(s) + 1);
	return r ? strcpy(r, s) : NULL;
}

int uc_isspace(char *s)
{
	int c = s ? (unsigned char) *s : 0;
	return c <= 0x7f && isspace(c);
}

int uc_isprint(char *s)
{
	int c = s ? (unsigned char) *s : 0;
	return c > 0x7f || isprint(c);
}

int uc_isalpha(char *s)
{
	int c = s ? (unsigned char) *s : 0;
	return c > 0x7f || isalpha(c);
}

int uc_isdigit(char *s)
{
	int c = s ? (unsigned char) *s : 0;
	return c <= 0x7f && isdigit(c);
}

int uc_kind(char *c)
{
	if (uc_isspace(c))
		return 0;
	if (uc_isalpha(c) || uc_isdigit(c) || c[0] == '_')
		return 1;
	return 2;
}

#define UC_R2L(ch)	(((ch) & 0xff00) == 0x0600 || \
			((ch) & 0xfffc) == 0x200c || \
			((ch) & 0xff00) == 0xfb00 || \
			((ch) & 0xff00) == 0xfc00 || \
			((ch) & 0xff00) == 0xfe00)

/* sorted list of characters that can be shaped */
static struct achar {
	unsigned c;		/* utf-8 code */
	unsigned s;		/* single form */
	unsigned i;		/* initial form */
	unsigned m;		/* medial form */
	unsigned f;		/* final form */
} achars[] = {
	{0x0621, 0xfe80},				/* hamza */
	{0x0622, 0xfe81, 0, 0, 0xfe82},			/* alef madda */
	{0x0623, 0xfe83, 0, 0, 0xfe84},			/* alef hamza above */
	{0x0624, 0xfe85, 0, 0, 0xfe86},			/* waw hamza */
	{0x0625, 0xfe87, 0, 0, 0xfe88},			/* alef hamza below */
	{0x0626, 0xfe89, 0xfe8b, 0xfe8c, 0xfe8a},	/* yeh hamza */
	{0x0627, 0xfe8d, 0, 0, 0xfe8e},			/* alef */
	{0x0628, 0xfe8f, 0xfe91, 0xfe92, 0xfe90},	/* beh */
	{0x0629, 0xfe93, 0, 0, 0xfe94},			/* teh marbuta */
	{0x062a, 0xfe95, 0xfe97, 0xfe98, 0xfe96},	/* teh */
	{0x062b, 0xfe99, 0xfe9b, 0xfe9c, 0xfe9a},	/* theh */
	{0x062c, 0xfe9d, 0xfe9f, 0xfea0, 0xfe9e},	/* jeem */
	{0x062d, 0xfea1, 0xfea3, 0xfea4, 0xfea2},	/* hah */
	{0x062e, 0xfea5, 0xfea7, 0xfea8, 0xfea6},	/* khah */
	{0x062f, 0xfea9, 0, 0, 0xfeaa},			/* dal */
	{0x0630, 0xfeab, 0, 0, 0xfeac},			/* thal */
	{0x0631, 0xfead, 0, 0, 0xfeae},			/* reh */
	{0x0632, 0xfeaf, 0, 0, 0xfeb0},			/* zain */
	{0x0633, 0xfeb1, 0xfeb3, 0xfeb4, 0xfeb2},	/* seen */
	{0x0634, 0xfeb5, 0xfeb7, 0xfeb8, 0xfeb6},	/* sheen */
	{0x0635, 0xfeb9, 0xfebb, 0xfebc, 0xfeba},	/* sad */
	{0x0636, 0xfebd, 0xfebf, 0xfec0, 0xfebe},	/* dad */
	{0x0637, 0xfec1, 0xfec3, 0xfec4, 0xfec2},	/* tah */
	{0x0638, 0xfec5, 0xfec7, 0xfec8, 0xfec6},	/* zah */
	{0x0639, 0xfec9, 0xfecb, 0xfecc, 0xfeca},	/* ain */
	{0x063a, 0xfecd, 0xfecf, 0xfed0, 0xfece},	/* ghain */
	{0x0640, 0x640, 0x640, 0x640},			/* tatweel */
	{0x0641, 0xfed1, 0xfed3, 0xfed4, 0xfed2},	/* feh */
	{0x0642, 0xfed5, 0xfed7, 0xfed8, 0xfed6},	/* qaf */
	{0x0643, 0xfed9, 0xfedb, 0xfedc, 0xfeda},	/* kaf */
	{0x0644, 0xfedd, 0xfedf, 0xfee0, 0xfede},	/* lam */
	{0x0645, 0xfee1, 0xfee3, 0xfee4, 0xfee2},	/* meem */
	{0x0646, 0xfee5, 0xfee7, 0xfee8, 0xfee6},	/* noon */
	{0x0647, 0xfee9, 0xfeeb, 0xfeec, 0xfeea},	/* heh */
	{0x0648, 0xfeed, 0, 0, 0xfeee},			/* waw */
	{0x0649, 0xfeef, 0, 0, 0xfef0},			/* alef maksura */
	{0x064a, 0xfef1, 0xfef3, 0xfef4, 0xfef2},	/* yeh */
	{0x067e, 0xfb56, 0xfb58, 0xfb59, 0xfb57},	/* peh */
	{0x0686, 0xfb7a, 0xfb7c, 0xfb7d, 0xfb7b},	/* tcheh */
	{0x0698, 0xfb8a, 0, 0, 0xfb8b},			/* jeh */
	{0x06a9, 0xfb8e, 0xfb90, 0xfb91, 0xfb8f},	/* fkaf */
	{0x06af, 0xfb92, 0xfb94, 0xfb95, 0xfb93},	/* gaf */
	{0x06cc, 0xfbfc, 0xfbfe, 0xfbff, 0xfbfd},	/* fyeh */
	{0x200c},					/* ZWNJ */
	{0x200d, 0, 0x200d, 0x200d},			/* ZWJ */
};

static struct achar *find_achar(int c)
{
	int h, m, l;
	h = LEN(achars);
	l = 0;
	/* using binary search to find c */
	while (l < h) {
		m = (h + l) >> 1;
		if (achars[m].c == c)
			return &achars[m];
		if (c < achars[m].c)
			h = m;
		else
			l = m + 1;
	}
	return NULL;
}

static int can_join(int c1, int c2)
{
	struct achar *a1 = find_achar(c1);
	struct achar *a2 = find_achar(c2);
	return a1 && a2 && (a1->i || a1->m) && (a2->f || a2->m);
}

static int uc_cshape(int cur, int prev, int next)
{
	int c = cur;
	int join_prev, join_next;
	struct achar *ac = find_achar(c);
	if (!ac)		/* ignore non-Arabic characters */
		return c;
	join_prev = can_join(prev, c);
	join_next = can_join(c, next);
	if (join_prev && join_next)
		c = ac->m;
	if (join_prev && !join_next)
		c = ac->f;
	if (!join_prev && join_next)
		c = ac->i;
	if (!join_prev && !join_next)
		c = ac->c;	/* some fonts do not have a glyph for ac->s */
	return c ? c : cur;
}

/*
 * return nonzero for Arabic combining characters
 *
 * The standard Arabic diacritics:
 * + 0x064b: fathatan
 * + 0x064c: dammatan
 * + 0x064d: kasratan
 * + 0x064e: fatha
 * + 0x064f: damma
 * + 0x0650: kasra
 * + 0x0651: shadda
 * + 0x0652: sukun
 * + 0x0653: madda above
 * + 0x0654: hamza above
 * + 0x0655: hamza below
 * + 0x0670: superscript alef
 */
static int uc_comb(int c)
{
	return (c >= 0x064b && c <= 0x0655) ||		/* the standard diacritics */
		(c >= 0xfc5e && c <= 0xfc63) ||		/* shadda ligatures */
		c == 0x0670;				/* superscript alef */
}

static void uc_cput(char *d, int c)
{
	int l = 0;
	if (c > 0xffff) {
		*d++ = 0xf0 | (c >> 18);
		l = 3;
	} else if (c > 0x7ff) {
		*d++ = 0xe0 | (c >> 12);
		l = 2;
	} else if (c > 0x7f) {
		*d++ = 0xc0 | (c >> 6);
		l = 1;
	} else {
		*d++ = c;
	}
	while (l--)
		*d++ = 0x80 | ((c >> (l * 6)) & 0x3f);
	*d = '\0';
}

/* shape the given arabic character; returns a static buffer */
char *uc_shape(char *beg, char *s)
{
	static char out[16];
	char *r;
	int prev = 0;
	int next = 0;
	int curr = uc_code(s);
	if (!curr || !UC_R2L(curr))
		return NULL;
	r = s;
	while (r > beg) {
		r = uc_beg(beg, r - 1);
		if (!uc_comb(uc_code(r))) {
			prev = uc_code(r);
			break;
		}
	}
	r = s;
	while (*r) {
		r = uc_next(r);
		if (!uc_comb(uc_code(r))) {
			next = uc_code(r);
			break;
		}
	}
	uc_cput(out, uc_cshape(curr, prev, next));
	return out;
}
