/*
 * neatvi editor
 *
 * Copyright (C) 2015 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * This program is released under the Modified BSD license.
 */
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

char xpath[PATHLEN];	/* current file */
struct lbuf *xb;	/* current buffer */
int xrow, xcol, xtop;	/* current row, column, and top row */
int xled = 1;		/* use the line editor */
int xdir = 'L';		/* current direction context */
int xquit;
static char vi_findlast[256];	/* the last searched keyword */
static int vi_finddir;		/* the last search direction */
static char vi_charlast[8];	/* the last character searched via f, t, F, or T */
static int vi_charcmd;		/* the character finding command */

static void vi_draw(void)
{
	int i;
	term_record();
	for (i = xtop; i < xtop + xrows; i++) {
		char *s = lbuf_get(xb, i);
		led_print(s ? s : "~", i - xtop);
	}
	led_print("", xrows);
	term_pos(xrow, led_pos(lbuf_get(xb, i), xcol));
	term_commit();
}

static int vi_buf[128];
static int vi_buflen;

static int vi_read(void)
{
	return vi_buflen ? vi_buf[--vi_buflen] : term_read(1000);
}

static void vi_back(int c)
{
	if (vi_buflen < sizeof(vi_buf))
		vi_buf[vi_buflen++] = c;
}

static char *vi_char(void)
{
	return led_keymap(vi_read());
}

static int vi_prefix(void)
{
	int pre = 0;
	int c = vi_read();
	if ((c >= '1' && c <= '9')) {
		while (isdigit(c)) {
			pre = pre * 10 + c - '0';
			c = vi_read();
		}
	}
	vi_back(c);
	return pre;
}

static int lbuf_lnnext(struct lbuf *lb, int *r, int *c, int dir)
{
	char *ln = lbuf_get(lb, *r);
	int col = ln ? ren_next(ln, *c, dir) : -1;
	if (col < 0)
		return -1;
	*c = col;
	return 0;
}

static void lbuf_eol(struct lbuf *lb, int *r, int *c, int dir)
{
	char *ln = lbuf_get(lb, *r);
	*c = dir < 0 ? 0 : MAX(0, ren_wid(ln ? ln : "") - 1);
}

static int lbuf_next(struct lbuf *lb, int *r, int *c, int dir)
{
	if (dir < 0 && *r >= lbuf_len(lb))
		*r = MAX(0, lbuf_len(lb) - 1);
	if (lbuf_lnnext(lb, r, c, dir)) {
		if (!lbuf_get(lb, *r + dir))
			return -1;
		*r += dir;
		lbuf_eol(lb, r, c, -dir);
		return 0;
	}
	return 0;
}

/* return a static buffer to the character at visual position c of line r */
static char *lbuf_chr(struct lbuf *lb, int r, int c)
{
	static char chr[8];
	char *ln = lbuf_get(lb, r);
	if (ln) {
		int off = ren_off(ln, c);
		char *s = uc_chr(ln, off);
		if (s) {
			memcpy(chr, s, uc_len(s));
			chr[uc_len(s)] = '\0';
			return chr;
		}
	}
	return "";
}

static void lbuf_postindents(struct lbuf *lb, int *r, int *c)
{
	lbuf_eol(lb, r, c, -1);
	while (uc_isspace(lbuf_chr(lb, *r, *c)))
		if (lbuf_lnnext(lb, r, c, +1))
			break;
}

static void lbuf_findchar(struct lbuf *lb, int *row, int *col, char *cs, int cmd, int n)
{
	int dir = (cmd == 'f' || cmd == 't') ? +1 : -1;
	int c = *col;
	if (n < 0)
		dir = -dir;
	if (n < 0)
		n = -n;
	strcpy(vi_charlast, cs);
	vi_charcmd = cmd;
	while (n > 0 && !lbuf_lnnext(lb, row, &c, dir))
		if (uc_code(lbuf_chr(lb, *row, c)) == uc_code(cs))
			n--;
	if (!n)
		*col = c;
	if (!n && (cmd == 't' || cmd == 'T'))
		lbuf_lnnext(lb, row, col, -dir);
}

static int lbuf_search(struct lbuf *lb, char *kw, int dir, int *r, int *c, int *len)
{
	int offs[2];
	int found = 0;
	int row = *r, col = *c;
	int i;
	struct rset *re = rset_make(1, &kw, 0);
	if (!re)
		return 1;
	for (i = row; !found && i >= 0 && i < lbuf_len(lb); i += dir) {
		char *s = lbuf_get(lb, i);
		int off = dir > 0 && row == i ? uc_chr(s, col + 1) - s : 0;
		int flg = off ? RE_NOTBOL : 0;
		while (rset_find(re, s + off, 1, offs, flg) >= 0) {
			if (dir < 0 && row == i && off + offs[0] >= col)
				break;
			found = 1;
			*c = uc_off(s, off + offs[0]);
			*r = i;
			*len = offs[1] - offs[0];
			off += offs[1];
			if (dir > 0)
				break;
		}
	}
	rset_free(re);
	return !found;
}

static int vi_search(int cmd, int cnt, int *row, int *col)
{
	int r = *row;
	int c = *col;
	int failed = 0;
	int len = 0;
	int i, dir;
	if (cmd == '/' || cmd == '?') {
		char sign[4] = {cmd};
		char *kw;
		term_pos(xrows, led_pos(sign, 0));
		term_kill();
		if (!(kw = led_prompt(sign, "")))
			return 1;
		vi_finddir = cmd == '/' ? +1 : -1;
		if (strchr(kw, cmd))
			*strchr(kw, cmd) = '\0';
		if (kw[0])
			snprintf(vi_findlast, sizeof(vi_findlast), "%s", kw);
		free(kw);
	}
	dir = cmd == 'N' ? -vi_finddir : vi_finddir;
	if (!vi_findlast[0] || !lbuf_len(xb))
		return 1;
	c = ren_off(lbuf_get(xb, *row), *col);
	for (i = 0; i < cnt; i++) {
		if (lbuf_search(xb, vi_findlast, dir, &r, &c, &len)) {
			failed = 1;
			break;
		}
		if (i + 1 < cnt && cmd == '/')
			c += len;
	}
	if (!failed) {
		*row = r;
		*col = ren_pos(lbuf_get(xb, r), c);
	}
	return failed;
}

static int vi_motionln(int *row, int cmd, int pre1, int pre2)
{
	int pre = (pre1 ? pre1 : 1) * (pre2 ? pre2 : 1);
	int c = vi_read();
	int mark;
	switch (c) {
	case '\n':
	case '+':
		*row = MIN(*row + pre, lbuf_len(xb) - 1);
		break;
	case '-':
		*row = MAX(*row - pre, 0);
		break;
	case '_':
		*row = MIN(*row + pre - 1, lbuf_len(xb) - 1);
		break;
	case '\'':
		if ((mark = vi_read()) > 0 && (isalpha(mark) || mark == '\''))
			if (lbuf_markpos(xb, mark) >= 0)
				*row = lbuf_markpos(xb, mark);
		break;
	case 'j':
		*row = MIN(*row + pre, lbuf_len(xb) - 1);
		break;
	case 'k':
		*row = MAX(*row - pre, 0);
		break;
	case 'G':
		*row = (pre1 || pre2) ? pre - 1 : lbuf_len(xb) - 1;
		break;
	case 'H':
		if (lbuf_len(xb))
			*row = MIN(xtop + pre - 1, lbuf_len(xb) - 1);
		else
			*row = 0;
		break;
	case 'L':
		if (lbuf_len(xb))
			*row = MIN(xtop + xrows - 1 - pre + 1, lbuf_len(xb) - 1);
		else
			*row = 0;
		break;
	case 'M':
		if (lbuf_len(xb))
			*row = MIN(xtop + xrows / 2, lbuf_len(xb) - 1);
		else
			*row = 0;
		break;
	default:
		if (c == cmd) {
			*row = MIN(*row + pre - 1, lbuf_len(xb) - 1);
			break;
		}
		vi_back(c);
		return 0;
	}
	return c;
}

/* move to the last character of the word */
static int lbuf_wordlast(struct lbuf *lb, int *row, int *col, int kind, int dir)
{
	if (!kind || !(uc_kind(lbuf_chr(lb, *row, *col)) & kind))
		return 0;
	while (uc_kind(lbuf_chr(lb, *row, *col)) & kind)
		if (lbuf_next(lb, row, col, dir))
			return 1;
	if (!(uc_kind(lbuf_chr(lb, *row, *col)) & kind))
		lbuf_next(lb, row, col, -dir);
	return 0;
}

static int lbuf_wordbeg(struct lbuf *lb, int *row, int *col, int big, int dir)
{
	int nl = 0;
	lbuf_wordlast(lb, row, col, big ? 3 : uc_kind(lbuf_chr(lb, *row, *col)), dir);
	if (lbuf_next(lb, row, col, dir))
		return 1;
	while (uc_isspace(lbuf_chr(lb, *row, *col))) {
		nl = uc_code(lbuf_chr(lb, *row, *col)) == '\n' ? nl + 1 : 0;
		if (nl == 2)
			return 0;
		if (lbuf_next(lb, row, col, dir))
			return 1;
	}
	return 0;
}

static int lbuf_wordend(struct lbuf *lb, int *row, int *col, int big, int dir)
{
	int nl = uc_code(lbuf_chr(lb, *row, *col)) == '\n' ? -1 : 0;
	if (!uc_isspace(lbuf_chr(lb, *row, *col)))
		if (lbuf_next(lb, row, col, dir))
			return 1;
	while (uc_isspace(lbuf_chr(lb, *row, *col))) {
		nl = uc_code(lbuf_chr(lb, *row, *col)) == '\n' ? nl + 1 : 0;
		if (nl == 2) {
			if (dir < 0)
				lbuf_next(lb, row, col, -dir);
			return 0;
		}
		if (lbuf_next(lb, row, col, dir))
			return 1;
	}
	if (lbuf_wordlast(lb, row, col, big ? 3 : uc_kind(lbuf_chr(lb, *row, *col)), dir))
		return 1;
	return 0;
}

static int vi_motion(int *row, int *col, int pre1, int pre2)
{
	int c = vi_read();
	int pre = (pre1 ? pre1 : 1) * (pre2 ? pre2 : 1);
	char *ln = lbuf_get(xb, *row);
	int dir = dir_context(ln ? ln : "");
	char *cs;
	int i;
	switch (c) {
	case ' ':
		for (i = 0; i < pre; i++)
			if (lbuf_lnnext(xb, row, col, 1))
				break;
		break;
	case 'f':
		if ((cs = vi_char()))
			lbuf_findchar(xb, row, col, cs, c, pre);
		break;
	case 'F':
		if ((cs = vi_char()))
			lbuf_findchar(xb, row, col, cs, c, pre);
		break;
	case ';':
		if (vi_charlast[0])
			lbuf_findchar(xb, row, col, vi_charlast, vi_charcmd, pre);
		break;
	case ',':
		if (vi_charlast[0])
			lbuf_findchar(xb, row, col, vi_charlast, vi_charcmd, -pre);
		break;
	case 'h':
		for (i = 0; i < pre; i++)
			if (lbuf_lnnext(xb, row, col, -1 * dir))
				break;
		break;
	case 'l':
		for (i = 0; i < pre; i++)
			if (lbuf_lnnext(xb, row, col, +1 * dir))
				break;
		break;
	case 't':
		if ((cs = vi_char()))
			lbuf_findchar(xb, row, col, cs, c, pre);
		break;
	case 'T':
		if ((cs = vi_char()))
			lbuf_findchar(xb, row, col, cs, c, pre);
		break;
	case 'B':
		for (i = 0; i < pre; i++)
			if (lbuf_wordend(xb, row, col, 1, -1))
				break;
		break;
	case 'E':
		for (i = 0; i < pre; i++)
			if (lbuf_wordend(xb, row, col, 1, +1))
				break;
		break;
	case 'W':
		for (i = 0; i < pre; i++)
			if (lbuf_wordbeg(xb, row, col, 1, +1))
				break;
		break;
	case 'b':
		for (i = 0; i < pre; i++)
			if (lbuf_wordend(xb, row, col, 0, -1))
				break;
		break;
	case 'e':
		for (i = 0; i < pre; i++)
			if (lbuf_wordend(xb, row, col, 0, +1))
				break;
		break;
	case 'w':
		for (i = 0; i < pre; i++)
			if (lbuf_wordbeg(xb, row, col, 0, +1))
				break;
		break;
	case '0':
		lbuf_eol(xb, row, col, -1);
		break;
	case '^':
		lbuf_postindents(xb, row, col);
		break;
	case '$':
		lbuf_eol(xb, row, col, +1);
		lbuf_lnnext(xb, row, col, -1);
		break;
	case '|':
		*col = pre - 1;
		break;
	case '/':
		vi_search(c, pre, row, col);
		break;
	case '?':
		vi_search(c, pre, row, col);
		break;
	case 'n':
		vi_search(c, pre, row, col);
		break;
	case 'N':
		vi_search(c, pre, row, col);
		break;
	case 127:
	case TK_CTL('h'):
		for (i = 0; i < pre; i++)
			if (lbuf_lnnext(xb, row, col, -1))
				break;
		break;
	default:
		vi_back(c);
		return 0;
	}
	return c;
}

static void swap(int *a, int *b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

static char *lbuf_region(struct lbuf *lb, int r1, int l1, int r2, int l2)
{
	struct sbuf *sb;
	char *s1, *s2, *s3;
	if (r1 == r2)
		return uc_sub(lbuf_get(lb, r1), l1, l2);
	sb = sbuf_make();
	s1 = uc_sub(lbuf_get(lb, r1), l1, -1);
	s3 = uc_sub(lbuf_get(lb, r2), 0, l2);
	s2 = lbuf_cp(lb, r1 + 1, r2);
	sbuf_str(sb, s1);
	sbuf_str(sb, s2);
	sbuf_str(sb, s3);
	free(s1);
	free(s2);
	free(s3);
	return sbuf_done(sb);
}

/* insertion offset before or after the given visual position */
static int vi_insertionoffset(char *s, int c1, int before)
{
	int l1, l2, c2;
	c2 = ren_next(s, c1, before ? -1 : +1);
	l2 = c2 >= 0 ? ren_off(s, c2) : 0;
	if (c1 == c2 || c2 < 0 || uc_chr(s, l2)[0] == '\n') {
		c2 = ren_next(s, c1, before ? +1 : -1);
		l1 = ren_off(s, c1);
		l2 = c2 >= 0 ? ren_off(s, c2) : 0;
		if (c1 == c2 || c2 < 0 || uc_chr(s, l2)[0] == '\n')
			return before ? l1 : l1 + 1;
		if (before)
			return l1 < l2 ? l1 : l1 + 1;
		else
			return l2 < l1 ? l1 + 1 : l1;
	}
	ren_region(s, c1, c2, &l1, &l2, 0);
	c1 = ren_pos(s, l1);
	c2 = ren_pos(s, l2);
	if (c1 < c2)
		return l1 < l2 ? l2 : l1;
	else
		return l1 < l2 ? l1 : l2;
}

static void vi_commandregion(int *r1, int *r2, int *c1, int *c2, int *l1, int *l2, int closed)
{
	if (*r2 < *r1 || (*r2 == *r1 && *c2 < *c1)) {
		swap(r1, r2);
		swap(c1, c2);
	}
	*l1 = lbuf_get(xb, *r1) ? vi_insertionoffset(lbuf_get(xb, *r1), *c1, 1) : 0;
	*l2 = lbuf_get(xb, *r2) ? vi_insertionoffset(lbuf_get(xb, *r2), *c2, !closed) : 0;
	if (*r1 == *r2 && lbuf_get(xb, *r1))
		ren_region(lbuf_get(xb, *r1), *c1, *c2, l1, l2, closed);
	if (*r1 == *r2 && *l2 < *l1)
		swap(l1, l2);
}

static void vi_yank(int r1, int c1, int r2, int c2, int lnmode, int closed)
{
	char *region;
	int l1, l2;
	vi_commandregion(&r1, &r2, &c1, &c2, &l1, &l2, closed);
	region = lbuf_region(xb, r1, lnmode ? 0 : l1, r2, lnmode ? -1 : l2);
	reg_put(0, region, lnmode);
	free(region);
	xrow = r1;
	xcol = lnmode ? xcol : c1;
}

static void vi_delete(int r1, int c1, int r2, int c2, int lnmode, int closed)
{
	char *pref, *post;
	char *region;
	int l1, l2;
	vi_commandregion(&r1, &r2, &c1, &c2, &l1, &l2, closed);
	region = lbuf_region(xb, r1, lnmode ? 0 : l1, r2, lnmode ? -1 : l2);
	reg_put(0, region, lnmode);
	free(region);
	pref = lnmode ? uc_dup("") : uc_sub(lbuf_get(xb, r1), 0, l1);
	post = lnmode ? uc_dup("\n") : uc_sub(lbuf_get(xb, r2), l2, -1);
	lbuf_rm(xb, r1, r2 + 1);
	if (!lnmode) {
		struct sbuf *sb = sbuf_make();
		sbuf_str(sb, pref);
		sbuf_str(sb, post);
		lbuf_put(xb, r1, sbuf_buf(sb));
		sbuf_free(sb);
	}
	xrow = r1;
	xcol = c1;
	if (lnmode)
		lbuf_postindents(xb, &xrow, &xcol);
	free(pref);
	free(post);
}

static int lastline(char *str)
{
	char *s = str;
	char *r = s;
	while (s && s[0]) {
		r = s;
		s = strchr(s == str ? s : s + 1, '\n');
	}
	return r - str;
}

static int linecount(char *s)
{
	int n;
	for (n = 0; s; n++)
		if ((s = strchr(s, '\n')))
			s++;
	return n;
}

static char *vi_input(char *pref, char *post, int *row, int *col)
{
	char *rep = led_input(pref, post);
	struct sbuf *sb;
	int last, off;
	if (!rep)
		return NULL;
	sb = sbuf_make();
	sbuf_str(sb, pref);
	sbuf_str(sb, rep);
	last = lastline(sbuf_buf(sb));
	off = uc_slen(sbuf_buf(sb) + last);
	sbuf_str(sb, post);
	*row = linecount(sbuf_buf(sb)) - 1;
	*col = ren_pos(sbuf_buf(sb) + last, MAX(0, off - 1));
	free(rep);
	return sbuf_done(sb);
}

static void vi_change(int r1, int c1, int r2, int c2, int lnmode, int closed)
{
	char *region;
	int l1, l2;
	int row, col;
	char *rep;
	char *pref, *post;
	vi_commandregion(&r1, &r2, &c1, &c2, &l1, &l2, closed);
	region = lbuf_region(xb, r1, lnmode ? 0 : l1, r2, lnmode ? -1 : l2);
	reg_put(0, region, lnmode);
	free(region);
	pref = lnmode ? uc_dup("") : uc_sub(lbuf_get(xb, r1), 0, l1);
	post = lnmode ? uc_dup("\n") : uc_sub(lbuf_get(xb, r2), l2, -1);
	rep = vi_input(pref, post, &row, &col);
	if (rep) {
		lbuf_rm(xb, r1, r2 + 1);
		lbuf_put(xb, r1, rep);
		xrow = r1 + row - 1;
		xcol = col;
		free(rep);
	}
	free(pref);
	free(post);
}

static void vc_motion(int cmd, int pre1)
{
	int r1 = xrow, r2 = xrow;	/* region rows */
	int c1 = xcol, c2 = xcol;	/* visual region columns */
	int lnmode = 0;			/* line-based region */
	int closed = 1;			/* include the last character */
	int mv;
	int pre2 = vi_prefix();
	if (pre2 < 0)
		return;
	if (vi_motionln(&r2, cmd, pre1, pre2)) {
		lnmode = 1;
		lbuf_eol(xb, &r1, &c1, -1);
		lbuf_eol(xb, &r2, &c2, +1);
	} else if ((mv = vi_motion(&r2, &c2, pre1, pre2))) {
		if (!strchr("fFtTeE$", mv))
			closed = 0;
	} else {
		return;
	}
	if (cmd == 'y')
		vi_yank(r1, c1, r2, c2, lnmode, closed);
	if (cmd == 'd')
		vi_delete(r1, c1, r2, c2, lnmode, closed);
	if (cmd == 'c')
		vi_change(r1, c1, r2, c2, lnmode, closed);
}

static void vc_insert(int cmd)
{
	char *pref, *post;
	char *ln = lbuf_get(xb, xrow);
	int row, col, off = 0;
	char *rep;
	if (cmd == 'I')
		lbuf_postindents(xb, &xrow, &xcol);
	if (cmd == 'A') {
		lbuf_eol(xb, &xrow, &xcol, +1);
		lbuf_lnnext(xb, &xrow, &xcol, -1);
	}
	if (cmd == 'o')
		xrow += 1;
	if (cmd == 'o' || cmd == 'O')
		ln = NULL;
	if (cmd == 'i' || cmd == 'I')
		off = ln ? vi_insertionoffset(ln, xcol, 1) : 0;
	if (cmd == 'a' || cmd == 'A')
		off = ln ? vi_insertionoffset(ln, xcol, 0) : 0;
	pref = ln ? uc_sub(ln, 0, off) : uc_dup("");
	post = ln ? uc_sub(ln, off, -1) : uc_dup("\n");
	rep = vi_input(pref, post, &row, &col);
	if ((cmd == 'o' || cmd == 'O') && !lbuf_len(xb))
		lbuf_put(xb, 0, "\n");
	if (rep) {
		if (cmd != 'o' && cmd != 'O')
			lbuf_rm(xb, xrow, xrow + 1);
		lbuf_put(xb, xrow, rep);
		xrow += row - 1;
		xcol = col;
		free(rep);
	}
	free(pref);
	free(post);
}

static void vc_put(int cmd, int cnt)
{
	int lnmode;
	char *ln;
	char *buf = reg_get(0, &lnmode);
	struct sbuf *sb;
	int off;
	int i;
	if (!buf)
		return;
	ln = lnmode ? NULL : lbuf_get(xb, xrow);
	off = ln ? vi_insertionoffset(ln, xcol, cmd == 'P') : 0;
	if (cmd == 'p' && !ln)
		xrow++;
	sb = sbuf_make();
	if (ln) {
		char *s = uc_sub(ln, 0, off);
		sbuf_str(sb, s);
		free(s);
	}
	for (i = 0; i < MAX(cnt, 1); i++)
		sbuf_str(sb, buf);
	if (ln) {
		char *s = uc_sub(ln, off, -1);
		sbuf_str(sb, s);
		free(s);
	}
	if (ln)
		lbuf_rm(xb, xrow, xrow + 1);
	lbuf_put(xb, xrow, sbuf_buf(sb));
	sbuf_free(sb);

}

static int join_spaces(char *prev, char *next)
{
	int prevlen = strlen(prev);
	if (!prev[0])
		return 0;
	if (prev[prevlen - 1] == ' ' || next[0] == ')')
		return 0;
	return prev[prevlen - 1] == '.' ? 2 : 1;
}

static void vc_join(int arg)
{
	struct sbuf *sb;
	int cnt = arg <= 1 ? 2 : arg;
	int beg = xrow;
	int end = xrow + cnt;
	int off = 0;
	int i;
	if (!lbuf_get(xb, beg) || !lbuf_get(xb, end - 1))
		return;
	sb = sbuf_make();
	for (i = beg; i < end; i++) {
		char *ln = lbuf_get(xb, i);
		char *lnend = strchr(ln, '\n');
		int spaces;
		if (i > beg)
			while (ln[0] == ' ' || ln[0] == '\t')
				ln++;
		spaces = i > beg ? join_spaces(sbuf_buf(sb), ln) : 0;
		off = uc_slen(sbuf_buf(sb));
		while (spaces--)
			sbuf_chr(sb, ' ');
		sbuf_mem(sb, ln, lnend - ln);
	}
	sbuf_chr(sb, '\n');
	lbuf_rm(xb, beg, end);
	lbuf_put(xb, beg, sbuf_buf(sb));
	xcol = ren_pos(sbuf_buf(sb), off);
	sbuf_free(sb);
}

static int vi_scrollforeward(int cnt)
{
	if (xtop >= lbuf_len(xb) - 1)
		return 1;
	xtop = MIN(lbuf_len(xb) - 1, xtop + cnt);
	xrow = MAX(xrow, xtop);
	return 0;
}

static int vi_scrollbackward(int cnt)
{
	if (xtop == 0)
		return 1;
	xtop = MAX(0, xtop - cnt);
	xrow = MIN(xrow, xtop + xrows - 1);
	return 0;
}

static void vi_status(void)
{
	char stat[128];
	sprintf(stat, "[%s] %d lines, %d,%d\n",
		xpath[0] ? xpath : "unnamed", lbuf_len(xb), xrow + 1, xcol + 1);
	led_print(stat, xrows);
}

static void vi(void)
{
	int mark;
	char *ln;
	term_init();
	xtop = 0;
	xrow = 0;
	lbuf_eol(xb, &xrow, &xcol, -1);
	vi_draw();
	term_pos(xrow, led_pos(lbuf_get(xb, xrow), xcol));
	while (!xquit) {
		int redraw = 0;
		int orow = xrow;
		int pre1, mv;
		if ((pre1 = vi_prefix()) < 0)
			continue;
		if ((mv = vi_motionln(&xrow, 0, pre1, 0))) {
			if (strchr("\'GHML", mv))
				lbuf_mark(xb, '\'', orow);
			if (!strchr("jk", mv))
				lbuf_postindents(xb, &xrow, &xcol);
		} else if (!vi_motion(&xrow, &xcol, pre1, 0)) {
			int c = vi_read();
			int z;
			if (c <= 0)
				continue;
			switch (c) {
			case 'u':
				lbuf_undo(xb);
				redraw = 1;
				break;
			case TK_CTL('b'):
				if (vi_scrollbackward((pre1 ? pre1 : 1) * (xrows - 1)))
					break;
				lbuf_postindents(xb, &xrow, &xcol);
				redraw = 1;
				break;
			case TK_CTL('f'):
				if (vi_scrollforeward((pre1 ? pre1 : 1) * (xrows - 1)))
					break;
				lbuf_postindents(xb, &xrow, &xcol);
				redraw = 1;
				break;
			case TK_CTL('e'):
				if (vi_scrollforeward((pre1 ? pre1 : 1)))
					break;
				redraw = 1;
				break;
			case TK_CTL('y'):
				if (vi_scrollbackward((pre1 ? pre1 : 1)))
					break;
				redraw = 1;
				break;
			case TK_CTL('r'):
				lbuf_redo(xb);
				redraw = 1;
				break;
			case TK_CTL('g'):
				vi_status();
				break;
			case ':':
				term_pos(xrows, led_pos(":", 0));
				term_kill();
				ln = led_prompt(":", "");
				if (ln && ln[0]) {
					ex_command(ln);
					redraw = 1;
				}
				free(ln);
				if (xquit)
					continue;
				break;
			case 'c':
			case 'd':
			case 'y':
				vc_motion(c, pre1);
				redraw = 1;
				break;
			case 'i':
			case 'I':
			case 'a':
			case 'A':
			case 'o':
			case 'O':
				vc_insert(c);
				redraw = 1;
				break;
			case 'J':
				vc_join(pre1);
				redraw = 1;
				break;
			case 'm':
				if ((mark = vi_read()) > 0 && isalpha(mark))
					lbuf_mark(xb, mark, xrow);
				break;
			case 'p':
			case 'P':
				vc_put(c, pre1);
				redraw = 1;
				break;
			case 'z':
				z = vi_read();
				switch (z) {
				case '\n':
					xtop = pre1 ? pre1 : xrow;
					break;
				case '.':
					xtop = MAX(0, (pre1 ? pre1 : xrow) - xrows / 2);
					break;
				case '-':
					xtop = MAX(0, (pre1 ? pre1 : xrow) - xrows + 1);
					break;
				case 'l':
				case 'r':
				case 'L':
				case 'R':
					xdir = z;
					break;
				}
				redraw = 1;
				break;
			case 'x':
				vi_back(' ');
				vc_motion('d', pre1);
				redraw = 1;
				break;
			case 'X':
				vi_back(TK_CTL('h'));
				vc_motion('d', pre1);
				redraw = 1;
				break;
			case 'C':
				vi_back('$');
				vc_motion('c', pre1);
				redraw = 1;
				break;
			case 'D':
				vi_back('$');
				vc_motion('d', pre1);
				redraw = 1;
				break;
			case 's':
				vi_back(' ');
				vc_motion('c', pre1);
				redraw = 1;
				break;
			case 'S':
				vi_back('c');
				vc_motion('c', pre1);
				redraw = 1;
				break;
			case 'Y':
				vi_back('y');
				vc_motion('y', pre1);
				redraw = 1;
				break;
			default:
				continue;
			}
		}
		if (xrow < 0 || xrow >= lbuf_len(xb))
			xrow = lbuf_len(xb) ? lbuf_len(xb) - 1 : 0;
		if (xrow < xtop || xrow >= xtop + xrows) {
			xtop = xrow < xtop ? xrow : MAX(0, xrow - xrows + 1);
			redraw = 1;
		}
		if (redraw)
			vi_draw();
		term_pos(xrow - xtop, led_pos(lbuf_get(xb, xrow),
				ren_cursor(lbuf_get(xb, xrow), xcol)));
		lbuf_undomark(xb);
	}
	term_pos(xrows, 0);
	term_kill();
	term_done();
}

int main(int argc, char *argv[])
{
	int visual = 1;
	char ecmd[PATHLEN];
	int i;
	xb = lbuf_make();
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (argv[i][1] == 's')
			xled = 0;
		if (argv[i][1] == 'e')
			visual = 0;
		if (argv[i][1] == 'v')
			visual = 1;
	}
	dir_init();
	if (i < argc) {
		snprintf(ecmd, PATHLEN, "e %s", argv[i]);
		ex_command(ecmd);
	}
	if (visual)
		vi();
	else
		ex();
	lbuf_free(xb);
	reg_done();
	dir_done();
	return 0;
}
