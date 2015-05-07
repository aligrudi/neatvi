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

static void vi_draw(void)
{
	int i;
	term_record();
	for (i = xtop; i < xtop + xrows; i++) {
		char *s = lbuf_get(xb, i);
		led_print(s ? s : "~", i - xtop);
	}
	term_pos(xrow, xcol);
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
	*c = ren_eol(ln ? ln : "", dir);
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

static void lbuf_findchar(struct lbuf *lb, int *row, int *col, char *cs, int dir, int n)
{
	int c = *col;
	if (!cs)
		return;
	while (n > 0 && !lbuf_lnnext(lb, row, &c, dir))
		if (uc_code(lbuf_chr(lb, *row, c)) == uc_code(cs))
			n--;
	if (!n)
		*col = c;
}

static void lbuf_tochar(struct lbuf *lb, int *row, int *col, char *cs, int dir, int n)
{
	int c = *col;
	if (!cs)
		return;
	while (n > 0 && !lbuf_lnnext(lb, row, &c, dir))
		if (uc_code(lbuf_chr(lb, *row, c)) == uc_code(cs))
			n--;
	if (!n) {
		*col = c;
		lbuf_lnnext(lb, row, col, -dir);
	}
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
	int dir = ren_dir(ln ? ln : "");
	int i;
	switch (c) {
	case ' ':
		for (i = 0; i < pre; i++)
			if (lbuf_next(xb, row, col, 1))
				break;
		break;
	case 'f':
		lbuf_findchar(xb, row, col, vi_char(), +1, pre);
		break;
	case 'F':
		lbuf_findchar(xb, row, col, vi_char(), -1, pre);
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
		lbuf_tochar(xb, row, col, vi_char(), 1, pre);
		break;
	case 'T':
		lbuf_tochar(xb, row, col, vi_char(), 0, pre);
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
	case 127:
	case TERMCTRL('h'):
		*col = ren_cursor(ln, *col);
		for (i = 0; i < pre; i++)
			if (lbuf_next(xb, row, col, -1))
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

static void vi_commandregion(int *r1, int *r2, int *c1, int *c2, int *l1, int *l2, int closed)
{
	if (*r2 < *r1 || (*r2 == *r1 && ren_cmp(lbuf_get(xb, *r1), *c1, *c2) > 0)) {
		swap(r1, r2);
		swap(c1, c2);
	}
	*l1 = lbuf_get(xb, *r1) ? ren_insertionoffset(lbuf_get(xb, *r1), *c1, 1) : 0;
	*l2 = lbuf_get(xb, *r2) ? ren_insertionoffset(lbuf_get(xb, *r2), *c2, !closed) : 0;
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
	rep = led_input(pref, post, &row, &col);
	if (rep) {
		lbuf_rm(xb, r1, r2 + 1);
		lbuf_put(xb, r1, rep);
		xrow = r1 + row;
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
		off = ln ? ren_insertionoffset(ln, xcol, 1) : 0;
	if (cmd == 'a' || cmd == 'A')
		off = ln ? ren_insertionoffset(ln, xcol, 0) : 0;
	pref = ln ? uc_sub(ln, 0, off) : uc_dup("");
	post = ln ? uc_sub(ln, off, -1) : uc_dup("\n");
	rep = led_input(pref, post, &row, &col);
	if (rep) {
		if (cmd != 'o' && cmd != 'O')
			lbuf_rm(xb, xrow, xrow + 1);
		lbuf_put(xb, xrow, rep);
		xrow += row;
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
	off = ln ? ren_insertionoffset(ln, xcol, cmd == 'P') : 0;
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

static void vi(void)
{
	int mark;
	term_init();
	xtop = 0;
	xrow = 0;
	lbuf_eol(xb, &xrow, &xcol, -1);
	vi_draw();
	term_pos(xrow, xcol);
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
			case TERMCTRL('b'):
				xtop = MAX(0, xtop - xrows + 1);
				xrow = xtop + xrows - 1;
				lbuf_postindents(xb, &xrow, &xcol);
				redraw = 1;
				break;
			case TERMCTRL('f'):
				if (lbuf_len(xb))
					xtop = MIN(lbuf_len(xb) - 1, xtop + xrows - 1);
				else
					xtop = 0;
				xrow = xtop;
				lbuf_postindents(xb, &xrow, &xcol);
				redraw = 1;
				break;
			case TERMCTRL('r'):
				lbuf_redo(xb);
				redraw = 1;
				break;
			case ':':
				term_pos(xrows, 0);
				term_kill();
				ex_command(NULL);
				if (xquit)
					continue;
				redraw = 1;
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
				vi_back('h');
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
		term_pos(xrow - xtop, ren_cursor(lbuf_get(xb, xrow), xcol));
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
	return 0;
}
