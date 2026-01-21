/* line editing and drawing */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "vi.h"

static int led_pos(int dir, int pos, int beg, int end)
{
	return dir >= 0 ? pos - beg : end - pos - 1;
}

static int led_offdir(char **chrs, int *pos, int i)
{
	if (pos[i] + ren_cwid(chrs[i], pos[i]) == pos[i + 1])
		return +1;
	if (pos[i + 1] + ren_cwid(chrs[i + 1], pos[i + 1]) == pos[i])
		return -1;
	return 0;
}

/* highlight text in reverse direction */
static void led_markrev(int n, char **chrs, int *pos, int *att)
{
	int i = 0, j;
	int hl = conf_hlrev();
	while (i + 1 < n) {
		int dir = led_offdir(chrs, pos, i);
		int beg = i;
		while (i + 1 < n && led_offdir(chrs, pos, i) == dir)
			i++;
		if (dir < 0)
			for (j = beg; j <= i; j++)
				att[j] = syn_merge(hl, att[j]);
		if (i == beg)
			i++;
	}
}

/* render and highlight a line */
static char *led_render(char *s0, int cbeg, int cend, char *syn)
{
	int n;
	int *pos;	/* pos[i]: the screen position of the i-th character */
	int *off;	/* off[i]: the character at screen position i */
	int *att;	/* att[i]: the attributes of i-th character */
	char **chrs;	/* chrs[i]: the i-th character in s1 */
	int clast = 0;
	int att_old = 0;
	struct sbuf *out;
	int i, j;
	int ctx = dir_context(s0);
	int att_blank = 0;		/* the attribute of blank space */
	chrs = uc_chop(s0, &n);
	pos = ren_position(s0);
	off = malloc((cend - cbeg) * sizeof(off[0]));
	memset(off, 0xff, (cend - cbeg) * sizeof(off[0]));
	/* initialise off[] using pos[] */
	for (i = 0; i < n; i++) {
		int curwid = ren_cwid(chrs[i], pos[i]);
		int curbeg = led_pos(ctx, pos[i], cbeg, cend);
		int curend = led_pos(ctx, pos[i] + curwid - 1, cbeg, cend);
		if (curbeg >= 0 && curbeg < (cend - cbeg) &&
				curend >= 0 && curend < (cend - cbeg)) {
			for (j = 0; j < curwid; j++)
				off[led_pos(ctx, pos[i] + j, cbeg, cend)] = i;
		}
	}
	att = syn_highlight(n <= xlim ? syn : "", s0);
	/* find the last non-empty column */
	for (i = cbeg; i < cend; i++)
		if (off[i - cbeg] >= 0)
			clast = i;
	/* the attribute of the last character is used for blanks */
	att_blank = n > 0 ? att[n - 1] : 0;
	led_markrev(n, chrs, pos, att);
	/* generate term output */
	out = sbuf_make();
	/* disable BiDi in vte-based terminals */
	sbuf_str(out, xvte ? "\33[8l" : "");
	i = cbeg;
	while (i < cend && i <= clast) {
		int o = off[i - cbeg];
		int att_new = o >= 0 ? att[o] : att_blank;
		sbuf_str(out, term_seqattr(att_new, att_old));
		att_old = att_new;
		if (o >= 0) {
			if (ren_translate(chrs[o], s0)) {
				sbuf_str(out, ren_translate(chrs[o], s0));
			} else if (uc_isprint(chrs[o])) {
				sbuf_mem(out, chrs[o], uc_len(chrs[o]));
			} else {
				for (j = i; j < cend && off[j - cbeg] == o; j++)
					sbuf_chr(out, ' ');
			}
			while (i < cend && off[i - cbeg] == o)
				i++;
		} else {
			sbuf_chr(out, ' ');
			i++;
		}
	}
	if (clast < cend - 1)
		sbuf_str(out, term_seqkill());
	sbuf_str(out, term_seqattr(0, att_old));
	free(att);
	free(pos);
	free(off);
	free(chrs);
	return sbuf_done(out);
}

/* print a line on the screen */
void led_print(char *s, int row, int left, char *syn)
{
	char *r = led_render(s, left, left + xcols, syn);
	term_pos(row, 0);
	term_str(r);
	free(r);
}

/* set xtd and return its old value */
static int td_set(int td)
{
	int old = xtd;
	xtd = td;
	return old;
}

/* print a line on the screen; for ex messages */
void led_printmsg(char *s, int row, char *syn)
{
	int td = td_set(+2);
	char *r = led_render(s, 0, xcols, syn);
	td_set(td);
	term_pos(row, 0);
	term_str(r);
	free(r);
}

static int led_lastchar(char *s)
{
	char *r = *s ? strchr(s, '\0') : s;
	if (r != s)
		r = uc_beg(s, r - 1);
	return r - s;
}

static int led_lastword(char *s)
{
	char *r = *s ? uc_beg(s, strchr(s, '\0') - 1) : s;
	int kind;
	while (r > s && uc_isspace(r))
		r = uc_beg(s, r - 1);
	kind = r > s ? uc_kind(r) : 0;
	while (r > s && uc_kind(uc_beg(s, r - 1)) == kind)
		r = uc_beg(s, r - 1);
	return r - s;
}

static void led_printparts(char *pref, char *main, char *post, int *left, int kmap, char *syn)
{
	struct sbuf *ln;
	int off, pos;
	ln = sbuf_make();
	sbuf_str(ln, pref);
	sbuf_str(ln, main);
	off = uc_slen(sbuf_buf(ln));
	term_record();
	sbuf_str(ln, post);
	pos = ren_insert(sbuf_buf(ln), off);
	if (pos >= *left + xcols)
		*left = pos - xcols / 2;
	if (pos < *left)
		*left = pos < xcols ? 0 : pos - xcols / 2;
	led_print(sbuf_buf(ln), -1, *left, syn);
	term_pos(-1, led_pos(dir_context(sbuf_buf(ln)), pos, *left, *left + xcols));
	sbuf_free(ln);
	term_commit();
}

/* continue reading the character starting with c */
static char *led_readchar(int c, int kmap)
{
	static char buf[8];
	int c1, c2;
	int i, n;
	if (c == TK_CTL('v')) {		/* literal character */
		buf[0] = term_read();
		buf[1] = '\0';
		return buf;
	}
	if (c == TK_CTL('k')) {		/* digraph */
		c1 = term_read();
		if (TK_INT(c1))
			return NULL;
		if (c1 == TK_CTL('k'))
			return "";
		c2 = term_read();
		if (TK_INT(c2))
			return NULL;
		return conf_digraph(c1, c2);
	}
	if ((c & 0xc0) == 0xc0) {	/* utf-8 character */
		buf[0] = c;
		n = uc_len(buf);
		for (i = 1; i < n; i++)
			buf[i] = term_read();
		buf[n] = '\0';
		return buf;
	}
	return kmap_map(kmap, c);
}

static int led_match(char *out, int len, char *kwd, char *opt)
{
	while (opt != NULL) {
		int i = 0;
		while (kwd[i] && kwd[i] == opt[i])
			i++;
		if (kwd[i] == '\0')
			break;
		opt = strchr(opt, '\n') == NULL ? NULL : strchr(opt, '\n') + 1;
	}
	out[0] = '\0';
	if (opt != NULL) {
		int i = 0;
		char *beg = opt + strlen(kwd);
		while (beg[i] && beg[i] != '\n' && i + 8 < len)
			i += uc_len(beg + i);
		memcpy(out, beg, i);
		out[i] = '\0';
		return 0;
	}
	return 1;
}

/* read a line from the terminal */
static char *led_line(char *pref, char *post, int *left, int *key, int *kmap, char *syn, char *hist)
{
	struct sbuf *sb;
	int y, lnmode;
	int c = 0;
	char cmp[64] = "";
	char *cs;
	sb = sbuf_make();
	if (pref == NULL)
		pref = "";
	if (post == NULL || !post[0])
		post = cmp;
	while (1) {
		if (hist != NULL)
			led_match(cmp, sizeof(cmp), sbuf_buf(sb), hist);
		led_printparts(pref, sbuf_buf(sb), post, left, *kmap, syn);
		c = term_read();
		switch (c) {
		case TK_CTL('f'):
			*kmap = xkmap_alt;
			continue;
		case TK_CTL('e'):
			*kmap = 0;
			continue;
		case TK_CTL('h'):
		case 127:
			if (sbuf_len(sb))
				sbuf_cut(sb, led_lastchar(sbuf_buf(sb)));
			break;
		case TK_CTL('u'):
			sbuf_cut(sb, 0);
			break;
		case TK_CTL('w'):
			if (sbuf_len(sb))
				sbuf_cut(sb, led_lastword(sbuf_buf(sb)));
			break;
		case TK_CTL('p'):
			if (reg_get(0, &lnmode))
				sbuf_str(sb, reg_get(0, &lnmode));
			break;
		case TK_CTL('r'):
			y = term_read();
			if (y > 0 && reg_get(y, &lnmode))
				sbuf_str(sb, reg_get(y, &lnmode));
			break;
		case TK_CTL('a'):
			sbuf_str(sb, cmp);
			cmp[0] = '\0';
			break;
		default:
			if (c == '\n' || TK_INT(c))
				break;
			if ((cs = led_readchar(c, *kmap)) != NULL)
				sbuf_str(sb, cs);
		}
		if (c == '\n')
			led_printparts(pref, sbuf_buf(sb), "", left, *kmap, syn);
		if (c == '\n' || TK_INT(c))
			break;
	}
	*key = c;
	return sbuf_done(sb);
}

/* read an ex command */
char *led_prompt(char *pref, char *post, int *kmap, char *syn, char *hist)
{
	int key;
	int td = td_set(+2);
	int left = 0;
	char *s = led_line(pref, post, &left, &key, kmap, syn, hist);
	td_set(td);
	if (key == '\n') {
		struct sbuf *sb = sbuf_make();
		if (pref)
			sbuf_str(sb, pref);
		sbuf_str(sb, s);
		if (post)
			sbuf_str(sb, post);
		free(s);
		return sbuf_done(sb);
	}
	free(s);
	return NULL;
}
