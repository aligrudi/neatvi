#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "vi.h"
#include "kmap.h"

static char **kmaps[] = {kmap_en, kmap_fa};

static char **kmap_find(char *name)
{
	int i;
	for (i = 0; i < LEN(kmaps); i++)
		if (name && kmaps[i][0] && !strcmp(name, kmaps[i][0]))
			return kmaps[i];
	return kmap_en;
}

static char *kmap_map(char *kmap, int c)
{
	static char cs[4];
	char **keymap = kmap_find(kmap);
	cs[0] = c;
	return keymap[c] ? keymap[c] : cs;
}

/* map cursor horizontal position to terminal column number */
int led_pos(char *s, int pos)
{
	return dir_context(s) >= 0 ? pos : xcols - pos - 1;
}

static int led_posctx(int dir, int pos)
{
	return dir >= 0 ? pos : xcols - pos - 1;
}

static int led_offdir(char **chrs, int *pos, int i)
{
	if (pos[i] + ren_cwid(chrs[i], pos[i]) == pos[i + 1])
		return +1;
	if (pos[i + 1] + ren_cwid(chrs[i + 1], pos[i + 1]) == pos[i])
		return -1;
	return 0;
}

static void led_markrev(int n, char **chrs, int *pos, int *att)
{
	int i = 0, j;
	int hl = 0;
	conf_highlight_revdir(&hl);
	while (i + 1 < n) {
		int dir = led_offdir(chrs, pos, i);
		int beg = i;
		while (i + 1 < n && led_offdir(chrs, pos, i) == dir)
			i++;
		if (dir < 0)
			for (j = beg; j <= i; j++)
				att[j] = syn_merge(att[j], hl);
		if (i == beg)
			i++;
	}
}

static char *led_render(char *s0)
{
	int n, maxcol = 0;
	int *pos;	/* pos[i]: the screen position of the i-th character */
	int *off;	/* off[i]: the character at screen position i */
	int *att;	/* att[i]: the attributes of i-th character */
	char **chrs;	/* chrs[i]: the i-th character in s1 */
	int att_old = 0;
	struct sbuf *out;
	int i, j;
	int ctx = dir_context(s0);
	chrs = uc_chop(s0, &n);
	pos = ren_position(s0);
	off = malloc(xcols * sizeof(off[0]));
	memset(off, 0xff, xcols * sizeof(off[0]));
	for (i = 0; i < n; i++) {
		int curpos = pos[i];
		int curwid = ren_cwid(chrs[i], curpos);
		if (curpos >= 0 && curpos + curwid < xcols) {
			for (j = 0; j < curwid; j++) {
				off[led_posctx(ctx, curpos + j)] = i;
				if (led_posctx(ctx, curpos + j) > maxcol)
					maxcol = led_posctx(ctx, curpos + j);
			}
		}
	}
	att = syn_highlight(ex_filetype(), s0);
	led_markrev(n, chrs, pos, att);
	out = sbuf_make();
	i = 0;
	while (i <= maxcol) {
		int o = off[i];
		int att_new = o >= 0 ? att[o] : 0;
		sbuf_str(out, term_att(att_new, att_old));
		att_old = att_new;
		if (o >= 0) {
			if (ren_translate(chrs[o], s0))
				sbuf_str(out, ren_translate(chrs[o], s0));
			else if (uc_isprint(chrs[o]))
				sbuf_mem(out, chrs[o], uc_len(chrs[o]));
			else
				for (j = i; j <= maxcol && off[j] == o; j++)
					sbuf_chr(out, ' ');
			while (i <= maxcol && off[i] == o)
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

void led_print(char *s, int row)
{
	char *r = led_render(s);
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

static void led_printparts(char *ai, char *pref, char *main, char *post, char *kmap)
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
	led_print(sbuf_buf(ln), -1);
	pos = ren_cursor(sbuf_buf(ln), ren_pos(sbuf_buf(ln), MAX(0, off - 1)));
	term_pos(-1, led_pos(sbuf_buf(ln), pos + idir));
	sbuf_free(ln);
	term_commit();
}

static char *led_digraph(void)
{
	int c1, c2;
	int i;
	c1 = term_read(-1);
	if (TK_INT(c1))
		return NULL;
	c2 = term_read(-1);
	if (TK_INT(c2))
		return NULL;
	for (i = 0; i < LEN(digraphs); i++)
		if (digraphs[i][0][0] == c1 && digraphs[i][0][1] == c2)
			return digraphs[i][1];
	return NULL;
}

char *led_read(char **kmap)
{
	static char buf[8];
	int c = term_read(-1);
	while (!TK_INT(c)) {
		switch (c) {
		case TK_CTL('f'):
			*kmap = conf_kmapalt();
			break;
		case TK_CTL('e'):
			*kmap = kmap_en[0];
			break;
		case TK_CTL('v'):
			buf[0] = term_read(-1);
			buf[1] = '\0';
			return buf;
		case TK_CTL('k'):
			return led_digraph();
		default:
			return kmap_map(*kmap, c);
		}
		c = term_read(-1);
	}
	return NULL;
}

static char *led_line(char *pref, char *post, char *ai, int ai_max, int *key, char **kmap)
{
	struct sbuf *sb;
	int ai_len = strlen(ai);
	int c, lnmode;
	char *dig;
	sb = sbuf_make();
	if (!pref)
		pref = "";
	if (!post)
		post = "";
	while (1) {
		led_printparts(ai, pref, uc_lastline(sbuf_buf(sb)), post, *kmap);
		c = term_read(-1);
		switch (c) {
		case TK_CTL('f'):
			*kmap = conf_kmapalt();
			continue;
		case TK_CTL('e'):
			*kmap = kmap_en[0];
			continue;
		case TK_CTL('h'):
		case 127:
			if (sbuf_len(sb))
				sbuf_cut(sb, led_lastchar(sbuf_buf(sb)));
			break;
		case TK_CTL('u'):
			sbuf_cut(sb, 0);
			break;
		case TK_CTL('k'):
			dig = led_digraph();
			if (dig)
				sbuf_str(sb, dig);
			break;
		case TK_CTL('v'):
			sbuf_chr(sb, term_read(-1));
			break;
		case TK_CTL('w'):
			if (sbuf_len(sb))
				sbuf_cut(sb, led_lastword(sbuf_buf(sb)));
			break;
		case TK_CTL('t'):
			if (ai_len < ai_max)
				ai[ai_len++] = '\t';
			ai[ai_len] = '\0';
			break;
		case TK_CTL('d'):
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
			sbuf_str(sb, kmap_map(*kmap, c));
		}
		if (c == '\n' || TK_INT(c))
			break;
	}
	*key = c;
	return sbuf_done(sb);
}

/* read an ex command */
char *led_prompt(char *pref, char *post, char **kmap)
{
	char *s;
	int key;
	s = led_line(pref, post, "", 0, &key, kmap);
	if (key == '\n')
		return s;
	free(s);
	return NULL;
}

/* read visual command input */
char *led_input(char *pref, char *post, char *ai, int ai_max, char **kmap)
{
	struct sbuf *sb = sbuf_make();
	char *first_ai = NULL;
	int key, i;
	while (1) {
		char *ln = led_line(pref, post, ai, ai_max, &key, kmap);
		if (pref)
			first_ai = uc_dup(ai);
		if (!pref)
			sbuf_str(sb, ai);
		sbuf_str(sb, ln);
		if (key == '\n')
			sbuf_chr(sb, '\n');
		led_printparts(ai, pref ? pref : "", uc_lastline(ln),
				key == '\n' ? "" : post, *kmap);
		if (key == '\n')
			term_chr('\n');
		if (!pref || !pref[0]) {	/* updating autoindent */
			int ai_len = ai_max ? strlen(ai) : 0;
			for (i = 0; isspace((unsigned char) ln[i]); i++)
				if (ai_len < ai_max)
					ai[ai_len++] = ln[i];
			ai[ai_len] = '\0';
		}
		pref = NULL;
		free(ln);
		if (key != '\n')
			break;
		term_room(1);
		while (ai_max && post[0] && (post[0] == ' ' || post[0] == '\t'))
			post++;
	}
	strcpy(ai, first_ai);
	free(first_ai);
	if (TK_INT(key))
		return sbuf_done(sb);
	sbuf_free(sb);
	return NULL;
}
