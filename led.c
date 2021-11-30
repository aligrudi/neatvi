/* line editing and drawing */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "vi.h"

static char *kmap_map(int kmap, int c)
{
	static char cs[4];
	char **keymap = conf_kmap(kmap);
	cs[0] = c;
	return keymap[c] ? keymap[c] : cs;
}

static int led_posctx(int dir, int pos, int beg, int end)
{
	return dir >= 0 ? pos - beg : end - pos - 1;
}

/* map cursor horizontal position to terminal column number */
int led_pos(char *s, int pos)
{
	return led_posctx(dir_context(s), pos, xleft, xleft + xcols);
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
		int curbeg = led_posctx(ctx, pos[i], cbeg, cend);
		int curend = led_posctx(ctx, pos[i] + curwid - 1, cbeg, cend);
		if (curbeg >= 0 && curbeg < (cend - cbeg) &&
				curend >= 0 && curend < (cend - cbeg))
			for (j = 0; j < curwid; j++)
				off[led_posctx(ctx, pos[i] + j, cbeg, cend)] = i;
	}
	att = syn_highlight(xhl ? syn : "", s0);
	/* the attribute of \n character is used for blanks */
	for (i = 0; i < n; i++)
		if (chrs[i][0] == '\n')
			att_blank = att[i];
	led_markrev(n, chrs, pos, att);
	/* generate term output */
	out = sbuf_make();
	sbuf_str(out, conf_lnpref());
	i = cbeg;
	while (i < cend) {
		int o = off[i - cbeg];
		int att_new = o >= 0 ? att[o] : att_blank;
		sbuf_str(out, term_att(att_new, att_old));
		att_old = att_new;
		if (o >= 0) {
			if (ren_translate(chrs[o], s0))
				sbuf_str(out, ren_translate(chrs[o], s0));
			else if (uc_isprint(chrs[o]))
				sbuf_mem(out, chrs[o], uc_len(chrs[o]));
			else
				for (j = i; j < cend && off[j - cbeg] == o; j++)
					sbuf_chr(out, ' ');
			while (i < cend && off[i - cbeg] == o)
				i++;
		} else {
			sbuf_chr(out, ' ');
			i++;
		}
	}
	sbuf_str(out, term_att(0, att_old));
	free(att);
	free(pos);
	free(off);
	free(chrs);
	return sbuf_done(out);
}

/* print a line on the screen */
void led_print(char *s, int row, char *syn)
{
	char *r = led_render(s, xleft, xleft + xcols, syn);
	term_pos(row, 0);
	term_kill();
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
	char *r = led_render(s, xleft, xleft + xcols, syn);
	td_set(td);
	term_pos(row, 0);
	term_kill();
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

static void led_printparts(char *ai, char *pref, char *main,
		char *post, int kmap, char *syn)
{
	struct sbuf *ln;
	int off, pos;
	int idir = 0;
	ln = sbuf_make();
	sbuf_str(ln, ai);
	sbuf_str(ln, pref);
	sbuf_str(ln, main);
	off = uc_slen(sbuf_buf(ln));
	/* cursor position for inserting the next character */
	if (*pref || *main || *ai) {
		int len = sbuf_len(ln);
		sbuf_str(ln, kmap_map(kmap, 'a'));
		sbuf_str(ln, post);
		idir = ren_pos(sbuf_buf(ln), off) -
			ren_pos(sbuf_buf(ln), off - 1) < 0 ? -1 : +1;
		sbuf_cut(ln, len);
	}
	term_record();
	sbuf_str(ln, post);
	pos = ren_cursor(sbuf_buf(ln), ren_pos(sbuf_buf(ln), MAX(0, off - 1)));
	if (pos >= xleft + xcols)
		xleft = pos - xcols / 2;
	if (pos < xleft)
		xleft = pos < xcols ? 0 : pos - xcols / 2;
	led_print(sbuf_buf(ln), -1, syn);
	term_pos(-1, led_pos(sbuf_buf(ln), pos + idir));
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

/* read a character from the terminal */
char *led_read(int *kmap)
{
	int c = term_read();
	while (!TK_INT(c)) {
		switch (c) {
		case TK_CTL('f'):
			*kmap = xkmap_alt;
			break;
		case TK_CTL('e'):
			*kmap = 0;
			break;
		default:
			return led_readchar(c, *kmap);
		}
		c = term_read();
	}
	return NULL;
}

/* read a line from the terminal */
static char *led_line(char *pref, char *post, char *ai,
		int ai_max, int *key, int *kmap, char *syn)
{
	struct sbuf *sb;
	int ai_len = strlen(ai);
	int c, lnmode;
	char *cs;
	sb = sbuf_make();
	if (!pref)
		pref = "";
	if (!post)
		post = "";
	while (1) {
		led_printparts(ai, pref, sbuf_buf(sb), post, *kmap, syn);
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
		case TK_CTL('t'):
			if (ai_len < ai_max) {
				ai[ai_len++] = '\t';
				ai[ai_len] = '\0';
			}
			break;
		case TK_CTL('d'):
			/* when ai and pref are empty, remove the first space of sb */
			if (ai_len == 0 && !pref[0]) {
				char *buf = sbuf_buf(sb);
				if (buf[0] == ' ' || buf[0] == '\t') {
					char *dup = uc_dup(buf + 1);
					sbuf_cut(sb, 0);
					sbuf_str(sb, dup);
					free(dup);
				}
			}
			if (ai_len > 0)
				ai[--ai_len] = '\0';
			break;
		case TK_CTL('p'):
			if (reg_get(0, &lnmode))
				sbuf_str(sb, reg_get(0, &lnmode));
			break;
		default:
			if (c == '\n' || TK_INT(c))
				break;
			if ((cs = led_readchar(c, *kmap)))
				sbuf_str(sb, cs);
		}
		if (c == '\n' || TK_INT(c))
			break;
	}
	*key = c;
	return sbuf_done(sb);
}

/* read an ex command */
char *led_prompt(char *pref, char *post, int *kmap, char *syn)
{
	int key;
	int td = td_set(+2);
	char *s = led_line(pref, post, "", 0, &key, kmap, syn);
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

/* read visual command input */
char *led_input(char *pref, char *post, int *kmap, char *syn)
{
	struct sbuf *sb = sbuf_make();
	char ai[128];
	int ai_max = sizeof(ai) - 1;
	int n = 0;
	int key;
	while (n < ai_max && (*pref == ' ' || *pref == '\t'))
		ai[n++] = *pref++;
	ai[n] = '\0';
	while (1) {
		char *ln = led_line(pref, post, ai, ai_max, &key, kmap, syn);
		int ln_sp = 0;	/* number of initial spaces in ln */
		while (ln[ln_sp] && (ln[ln_sp] == ' ' || ln[ln_sp] == '\t'))
			ln_sp++;
		/* append the auto-indent only if there are other characters */
		if (ln[ln_sp] || (pref && pref[0]) ||
				(key != '\n' && post[0] && post[0] != '\n'))
			sbuf_str(sb, ai);
		if (pref)
			sbuf_str(sb, pref);
		sbuf_str(sb, ln);
		if (key == '\n')
			sbuf_chr(sb, '\n');
		led_printparts(ai, pref ? pref : "", uc_lastline(ln),
				key == '\n' ? "" : post, *kmap, syn);
		if (key == '\n')
			term_chr('\n');
		if (!pref || !pref[0]) {	/* updating autoindent */
			int ai_len = ai_max ? strlen(ai) : 0;
			int ai_new = ln_sp;
			if (ai_len + ai_new > ai_max)
				ai_new = ai_max - ai_len;
			memcpy(ai + ai_len, ln, ai_new);
			ai[ai_len + ai_new] = '\0';
		}
		if (!xai)
			ai[0] = '\0';
		free(ln);
		if (key != '\n')
			break;
		term_room(1);
		pref = NULL;
		n = 0;
		while (xai && (post[n] == ' ' || post[n] == '\t'))
			n++;
		memmove(post, post + n, strlen(post) - n + 1);
	}
	sbuf_str(sb, post);
	if (TK_INT(key))
		return sbuf_done(sb);
	sbuf_free(sb);
	return NULL;
}
