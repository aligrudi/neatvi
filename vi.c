/*
 * NEATVI Editor
 *
 * Copyright (C) 2015-2026 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "vi.h"

/* parts of the screen to update; returned from vc_* functions */
#define VC_COL	1	/* only obtain cursor column from xoff */
#define VC_ROW	2	/* only the current line was updated */
#define VC_WIN	4	/* only the current window was updated */
#define VC_ALT	8	/* only the other window was updated */
#define VC_OK	16	/* already updated */
#define VC_ALL	12	/* all windows were updated */

static char vi_msg[EXLEN];	/* current message */
static char vi_charlast[8];	/* the last character searched via f, t, F, or T */
static int vi_charcmd;		/* the character finding command */
static int vi_arg1, vi_arg2;	/* the first and second arguments */
static int vi_ybuf;		/* current yank buffer */
static int vi_pcol;		/* the column requested by | command */
static int vi_printed;		/* ex_print() calls since the last command */
static int vi_scroll;		/* scroll amount for ^f and ^d */
static int vi_soset, vi_so;	/* search offset; 1 in "/kw/1" */
static int vi_insert;		/* insert mode */
static int vi_insoff;		/* insert offset */
static char vi_ai[128];		/* insert indentation for ai option */
static int w_cnt = 1;		/* window count */
static int w_cur;		/* active window identifier */
static int w_tmp;		/* temporary window */
static char *w_path;		/* saved window path */
static int w_row, w_off, w_top, w_left;	/* saved window configuration */
static int glob_id[128];	/* global mark buffer IDs */

static char *vi_ledmod;		/* led_print() data for the status line */
static char *vi_ledins;		/* led_print() data for xrow when vi_insert is one */

static int vc_status(void);

static void vi_wait(void)
{
	if (vi_printed > 1 || vi_printed < 0) {
		if (vi_printed < 0)
			term_window(0, term_rowx() - 1);
		term_pos(xrows, 0);
		free(led_prompt("[enter to continue]", "", &xkmap, xhl ? "---" : "___", NULL));
		vi_msg[0] = '\0';
	}
	vi_printed = 0;
}

static int xtd_set(int td)
{
	int old = xtd;
	xtd = td;
	return old;
}

static void vi_drawmsg(void)
{
	int td = xtd_set(+2);
	syn_context('.', w_tmp ? 'Z' : 0);
	led_print(vi_msg[0] ? vi_msg : "\n", xrows, 0, xcols, xhl ? "---" : "___", &vi_ledmod);
	syn_context('.', 0);
	vi_msg[0] = '\0';
	xtd_set(td);
}

static void vi_drawquick(char *s, int row)
{
	int td = xtd_set(+2);
	led_print(s, row, 0, xcols, xhl ? "---" : "___", NULL);
	xtd_set(td);
}

static void vi_drawrow(int row)
{
	char *s = lbuf_get(xb, row);
	syn_context(s ? '.' : ',', xhll && row == xrow ? '^' : 0);
	led_print(s ? s : "~", row - xtop, xleft, xcols, xhl ? ex_filetype() : "",
		vi_insert && row == xrow ? &vi_ledins : NULL);
	syn_context('.', 0);
}

/* redraw the given row; if row is -1 redraws all rows */
static void vi_drawagain(int xcol, int row)
{
	int i;
	for (i = xtop; i < xtop + xrows; i++)
		if (row < 0 || i == row)
			vi_drawrow(i);
	vi_drawmsg();
}

/* update the screen */
static void vi_drawupdate(int otop)
{
	int i = 0;
	if (otop != xtop) {
		term_pos(0, 0);
		term_room(otop - xtop);
		if (xtop > otop) {
			int n = MIN(xtop - otop, xrows);
			for (i = 0; i < n; i++)
				vi_drawrow(xtop + xrows - n + i);
		} else {
			int n = MIN(otop - xtop, xrows);
			for (i = 0; i < n; i++)
				vi_drawrow(xtop + i);
		}
	}
	vi_drawmsg();
}

/* update the screen by replacing lines r1 to r2 with n lines */
static void vi_drawfix(int r1, int del, int ins)
{
	int i;
	int s1 = MIN(MAX(r1, xtop), xtop + xrows - 1);
	if (ins != del) {
		term_pos(s1 - xtop, 0);
		term_room(ins - del);
	}
	/* new lines are visible */
	if (del > ins && r1 + ins < xtop + xrows) {
		for (i = xtop + xrows - del + ins; i < xtop + xrows; i++)
			if (i >= r1 + ins)
				vi_drawrow(i);
	}
	/* draw replaced lines */
	for (i = s1; i < xtop + xrows; i++)
		if (i < r1 + ins || (!i && !r1 && !ins))
			vi_drawrow(i);
}

static int vi_switch(int id)
{
	int beg = 0;
	int cnt = term_rowx();
	if (id >= w_cnt)
		return 1;
	if (id != w_cur) {
		char cmd[1024];
		char *old = w_path && w_path[0] ? w_path : "/";
		int row = w_row, off = w_off, top = w_top, left = w_left;
		char *ec = "ew";
		if (w_path && strcmp(w_path, ex_path()) == 0)
			ec = "e";
		snprintf(cmd, sizeof(cmd), "%s! %s", ec, old);
		free(w_path);
		w_path = uc_dup(ex_path());
		w_row = xrow, w_off = xoff, w_top = xtop, w_left = xleft;
		ex_command(cmd);
		xrow = row, xoff = off, xtop = top, xleft = left;
	}
	if (w_cnt == 2) {
		int half = cnt / 2;
		beg = id == 0 ? 0 : half;
		cnt = id == 0 ? half : term_rowx() - half;
	}
	term_window(beg, cnt - 1);
	w_cur = id;
	return 0;
}

static int vi_wsplit(void)
{
	if (w_cnt != 1)
		return 1;
	free(w_path);
	w_cnt = 2;
	w_path = uc_dup(ex_path());
	w_row = xrow, w_off = xoff, w_top = xtop, w_left = xleft;
	return vi_switch(0);
}

static int vi_wonly(void)
{
	if (w_cnt != 2)
		return 1;
	w_cnt = 1;
	w_cur = 0;
	return vi_switch(0);
}

static int vi_wclose(void)
{
	if (w_cnt != 2)
		return 1;
	vi_switch(1 - w_cur);
	return vi_wonly();
}

static int vi_wswap(void)
{
	if (w_cnt != 2)
		return 1;
	w_cur = 1 - w_cur;
	return 0;
}

static void vi_wfix(void)
{
	if (xrow < 0 || xrow >= lbuf_len(xb))
		xrow = lbuf_len(xb) ? lbuf_len(xb) - 1 : 0;
	if (xtop > xrow)
		xtop = xtop - xrows / 2 > xrow ?
				MAX(0, xrow - xrows / 2) : xrow;
	if (xtop + xrows <= xrow)
		xtop = xtop + xrows + xrows / 2 <= xrow ?
				xrow - xrows / 2 : xrow - xrows + 1;
	if (!vi_insert)
		xoff = ren_noeol(lbuf_get(xb, xrow), xoff);
}

static int vi_wmirror(void)
{
	vi_wonly();
	return vi_wsplit();
}

static int vi_buf[128];
static int vi_buflen;

static int vi_read(void)
{
	return vi_buflen ? vi_buf[--vi_buflen] : term_read();
}

static void vi_back(int c)
{
	if (vi_buflen < sizeof(vi_buf))
		vi_buf[vi_buflen++] = c;
}

/* map cursor horizontal position to terminal column number */
static int vi_pos(char *s, int pos)
{
	return dir_context(s ? s : "") >= 0 ? pos - xleft : xleft + xcols - pos - 1;
}

static char *vi_prompt(char *msg, int *kmap, char *hist)
{
	char *r, *s;
	int td = xtd_set(+2);
	term_pos(xrows, 0);
	s = led_prompt(msg, "", kmap, xhl ? "-ex" : "___", xhist != 0 ? hist : NULL);
	xtd_set(td);
	led_reset(&vi_ledmod);
	if (!s)
		return NULL;
	r = uc_dup(strlen(s) >= strlen(msg) ? s + strlen(msg) : s);
	free(s);
	return r;
}

/* read an ex input line */
char *ex_read(char *msg)
{
	struct sbuf *sb;
	int c;
	if (xvis)
		term_pos(xrows - 1, 0);
	if (xled) {
		int td = xtd_set(+2);
		char *s = led_prompt(msg, "", &xkmap, xhl ? "-ex" : "___", NULL);
		xtd_set(td);
		if (s)
			term_chr('\n');
		return s;
	}
	sb = sbuf_make();
	while ((c = getchar()) != EOF && c != '\n')
		sbuf_chr(sb, c);
	if (c == EOF) {
		sbuf_free(sb);
		return NULL;
	}
	return sbuf_done(sb);
}

/* show an ex message */
void ex_show(char *msg)
{
	if (xvis) {
		snprintf(vi_msg, sizeof(vi_msg), "%s", msg);
	} else if (xled) {
		led_print(msg, -1, 0, xcols, xhl ? "-ex" : "___", NULL);
		term_chr('\n');
	} else {
		printf("%s", msg);
	}
}

/* print an ex output line */
void ex_print(char *line)
{
	if (vi_insert) {
		led_print(line, xrows, 0, xcols, xhl ? "---" : "___", &vi_ledmod);
	} else if (xvis) {
		if (line && vi_printed == 0)
			snprintf(vi_msg, sizeof(vi_msg), "%s", line);
		if (line && vi_printed == 1)
			led_print(vi_msg, xrows - 1, 0, xcols, xhl ? "-ex" : "", NULL);
		if (vi_printed)
			term_chr('\n');
		if (line && vi_printed)
			led_print(line, xrows - 1, 0, xcols, xhl ? "-ex" : "", NULL);
		vi_printed += line != NULL ? 1 : -1000;
	} else {
		if (line)
			ex_show(line);
	}
}

static char *reg_getln(int h)
{
	return reg_get(0x80 | h, NULL);
}

static void reg_putln(int h, char *s)
{
	char *old = reg_get(0x80 | h, NULL);
	struct sbuf *sb;
	int i = 0;
	if (xhist == 0 || s == NULL || s[0] == '\0' || s[0] == '\n')
		return;
	/* ignore duplicate lines */
	if (old != NULL) {
		while (s[i] && old[i] == s[i])
			i++;
		if (!s[i] && old[i] == '\n')
			return;
	}
	/* omit old lines */
	if (xhist > 0 && old != NULL) {
		char *end = old;
		for (i = 1; end != NULL && i < xhist; i++)
			end = strchr(end == old ? end : end + 1, '\n');
		if (end != NULL)
			end[1] = '\0';
	}
	/* add the new line */
	sb = sbuf_make();
	sbuf_str(sb, s);
	if (strchr(s, '\n') == NULL)
		sbuf_chr(sb, '\n');
	if (old != NULL)
		sbuf_str(sb, old);
	reg_put(0x80 | h, sbuf_buf(sb), 1);
	sbuf_free(sb);
}

static int vi_yankbuf(void)
{
	int c = vi_read();
	if (c == '"')
		return (c = vi_read()) == '\\' ? 0x80 | vi_read() : c;
	vi_back(c);
	return 0;
}

static int vi_prefix(void)
{
	int n = 0;
	int c = vi_read();
	if ((c >= '1' && c <= '9')) {
		while (isdigit(c)) {
			n = n * 10 + c - '0';
			c = vi_read();
		}
	}
	vi_back(c);
	return n;
}

static int vi_col2off(struct lbuf *lb, int row, int col)
{
	char *ln = lbuf_get(lb, row);
	return ln ? ren_off(ln, col) : 0;
}

static int vi_off2col(struct lbuf *lb, int row, int off)
{
	char *ln = lbuf_get(lb, row);
	return ln ? ren_pos(ln, off) : 0;
}

static int vi_nextoff(struct lbuf *lb, int dir, int *row, int *off)
{
	int o = *off + dir;
	if (o < 0 || !lbuf_get(lb, *row) || o >= uc_slen(lbuf_get(lb, *row)))
		return 1;
	*off = o;
	return 0;
}

static int vi_nextcol(struct lbuf *lb, int dir, int *row, int *off)
{
	char *ln = lbuf_get(lb, *row);
	int col = ln ? ren_pos(ln, *off) : 0;
	int o = ln ? ren_next(ln, col, dir) : -1;
	if (o < 0)
		return -1;
	*off = ren_off(ln, o);
	return 0;
}

static int vi_findchar(struct lbuf *lb, char *cs, int cmd, int n, int *row, int *off)
{
	if (cs != vi_charlast)
		strcpy(vi_charlast, cs);
	vi_charcmd = cmd;
	return lbuf_findchar(lb, cs, cmd, n, row, off);
}

static int vi_search(int cmd, int cnt, int *row, int *off)
{
	char *kwd;
	int r = *row;
	int o = *off;
	char *failed = NULL;
	int len = 0;
	int i, dir;
	if (cmd == '/' || cmd == '?') {
		char sign[4] = {cmd};
		struct sbuf *sb;
		char *kw = vi_prompt(sign, &xkmap, reg_getln('/'));
		char *re;
		if (!kw)
			return 1;
		sb = sbuf_make();
		sbuf_chr(sb, cmd);
		sbuf_str(sb, kw);
		free(kw);
		kw = sbuf_buf(sb);
		if ((re = re_read(&kw))) {
			ex_kwdset(re[0] ? re : NULL, cmd == '/' ? +1 : -1);
			if (re[0]) {
				reg_putln('/', re);
				reg_put('/', re, 0);
			}
			while (isspace(*kw))
				kw++;
			vi_soset = !!kw[0];
			vi_so = atoi(kw);
			free(re);
		}
		sbuf_free(sb);
	}
	if (!lbuf_len(xb) || ex_kwd(&kwd, &dir))
		return 1;
	dir = cmd == 'N' ? -dir : dir;
	o = *off;
	for (i = 0; i < cnt; i++) {
		if (lbuf_search(xb, kwd, dir, &r, &o, &len)) {
			failed = " not found";
			break;
		}
		if (i + 1 < cnt && cmd == '/')
			o += len;
	}
	if (!failed) {
		*row = r;
		*off = o;
		if (vi_soset) {
			*off = -1;
			if (*row + vi_so < 0 || *row + vi_so >= lbuf_len(xb))
				failed = " bad offset";
			else
				*row += vi_so;
		}
	}
	if (failed != NULL)
		snprintf(vi_msg, sizeof(vi_msg), "/%s/%s", kwd, failed ? failed : "");
	return failed != NULL;
}

static char *vi_char(int (*next)(void), int kmap)
{
	static char buf[8];
	int c1, c2;
	int i, n;
	int lnmode;
	int c = next();
	switch (c) {
	case TK_CTL('v'):
		buf[0] = next();
		buf[1] = '\0';
		return buf;
	case TK_CTL('k'):
		c1 = next();
		if (TK_INT(c1))
			return NULL;
		if (c1 == TK_CTL('k'))
			return "";
		c2 = next();
		if (TK_INT(c2))
			return NULL;
		return conf_digraph(c1, c2);
	case TK_CTL('p'):
		return reg_get(0, &lnmode);
	case TK_CTL('r'):
		c1 = next();
		return c1 > 0 ? reg_get(c1, &lnmode) : NULL;
	}
	if (TK_INT(c))
		return NULL;
	if ((c & 0xc0) == 0xc0) {
		buf[0] = c;
		n = uc_len(buf);
		for (i = 1; i < n; i++)
			buf[i] = next();
		buf[n] = '\0';
		return buf;
	}
	return kmap_map(kmap, c);
}

/* read a line motion */
static int vi_motionln(int *row, int cmd)
{
	int cnt = (vi_arg1 ? vi_arg1 : 1) * (vi_arg2 ? vi_arg2 : 1);
	int c = vi_read();
	int mark, mark_row, mark_off;
	switch (c) {
	case '\n':
	case '+':
		*row = MIN(*row + cnt, lbuf_len(xb) - 1);
		break;
	case '-':
		*row = MAX(*row - cnt, 0);
		break;
	case '_':
		*row = MIN(*row + cnt - 1, lbuf_len(xb) - 1);
		break;
	case '\'':
		if ((mark = vi_read()) <= 0)
			return -1;
		if (lbuf_jump(xb, mark, &mark_row, &mark_off))
			return -1;
		*row = mark_row;
		break;
	case 'j':
		*row = MIN(*row + cnt, lbuf_len(xb) - 1);
		break;
	case 'k':
		*row = MAX(*row - cnt, 0);
		break;
	case 'G':
		*row = (vi_arg1 || vi_arg2) ? cnt - 1 : lbuf_len(xb) - 1;
		break;
	case 'H':
		*row = MIN(xtop + cnt - 1, lbuf_len(xb) - 1);
		break;
	case 'L':
		*row = MIN(xtop + xrows - 1 - cnt + 1, lbuf_len(xb) - 1);
		break;
	case 'M':
		*row = MIN(xtop + xrows / 2, lbuf_len(xb) - 1);
		break;
	default:
		if (c == cmd) {
			*row = MIN(*row + cnt - 1, lbuf_len(xb) - 1);
			break;
		}
		if (c == '%' && (vi_arg1 || vi_arg2)) {
			if (cnt > 100)
				return -1;
			*row = MAX(0, lbuf_len(xb) - 1) * cnt / 100;
			break;
		}
		vi_back(c);
		return 0;
	}
	if (*row < 0)
		*row = 0;
	return c;
}

static int vi_curword(char *ln, char *dst, int len, int off, char *ext)
{
	char *beg, *end;
	if (!ln)
		return 1;
	beg = uc_chr(ln, ren_noeol(ln, off));
	end = beg;
	while (*end && (uc_kind(end) == 1 ||
			strchr(ext, (unsigned char) end[0]) != NULL))
		end = uc_next(end);
	while (beg > ln && (uc_kind(uc_beg(ln, beg - 1)) == 1 ||
			strchr(ext, (unsigned char) beg[-1]) != NULL))
		beg = uc_beg(ln, beg - 1);
	if (beg >= end)
		return 1;
	len = len - 1 < end - beg ? len - 1 : end - beg;
	dst[len] = '\0';
	memcpy(dst, beg, len);
	return 0;
}

/* read a motion */
static int vi_motion(int *row, int *off)
{
	char cw[120], kw[128];
	int cnt = (vi_arg1 ? vi_arg1 : 1) * (vi_arg2 ? vi_arg2 : 1);
	char *ln = lbuf_get(xb, *row);
	int dir = dir_context(ln ? ln : "");
	int mark, mark_row, mark_off;
	char *cs;
	int mv;
	int i;
	if ((mv = vi_motionln(row, 0))) {
		*off = -1;
		return mv;
	}
	mv = vi_read();
	switch (mv) {
	case 'f':
		if (!(cs = vi_char(vi_read, xkmap)))
			return -1;
		if (vi_findchar(xb, cs, mv, cnt, row, off))
			return -1;
		break;
	case 'F':
		if (!(cs = vi_char(vi_read, xkmap)))
			return -1;
		if (vi_findchar(xb, cs, mv, cnt, row, off))
			return -1;
		break;
	case ';':
		if (!vi_charlast[0])
			return -1;
		if (vi_findchar(xb, vi_charlast, vi_charcmd, cnt, row, off))
			return -1;
		break;
	case ',':
		if (!vi_charlast[0])
			return -1;
		if (vi_findchar(xb, vi_charlast, vi_charcmd, -cnt, row, off))
			return -1;
		break;
	case 'h':
		for (i = 0; i < cnt; i++)
			if (vi_nextcol(xb, -1 * dir, row, off))
				break;
		break;
	case 'l':
		for (i = 0; i < cnt; i++)
			if (vi_nextcol(xb, +1 * dir, row, off))
				break;
		break;
	case 't':
		if (!(cs = vi_char(vi_read, xkmap)))
			return -1;
		if (vi_findchar(xb, cs, mv, cnt, row, off))
			return -1;
		break;
	case 'T':
		if (!(cs = vi_char(vi_read, xkmap)))
			return -1;
		if (vi_findchar(xb, cs, mv, cnt, row, off))
			return -1;
		break;
	case 'B':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordend(xb, 1, -1, row, off))
				break;
		break;
	case 'E':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordend(xb, 1, +1, row, off))
				break;
		break;
	case 'W':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordbeg(xb, 1, +1, row, off))
				break;
		break;
	case 'b':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordend(xb, 0, -1, row, off))
				break;
		break;
	case 'e':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordend(xb, 0, +1, row, off))
				break;
		break;
	case 'w':
		for (i = 0; i < cnt; i++)
			if (lbuf_wordbeg(xb, 0, +1, row, off))
				break;
		break;
	case '{':
		for (i = 0; i < cnt; i++)
			if (lbuf_paragraphbeg(xb, -1, row, off))
				break;
		break;
	case '}':
		for (i = 0; i < cnt; i++)
			if (lbuf_paragraphbeg(xb, +1, row, off))
				break;
		break;
	case '[':
		if (vi_read() != '[')
			return -1;
		for (i = 0; i < cnt; i++)
			if (lbuf_sectionbeg(xb, -1, conf_section(ex_filetype()), row, off))
				break;
		break;
	case ']':
		if (vi_read() != ']')
			return -1;
		for (i = 0; i < cnt; i++)
			if (lbuf_sectionbeg(xb, +1, conf_section(ex_filetype()), row, off))
				break;
		break;
	case '0':
		*off = 0;
		break;
	case '^':
		*off = lbuf_indents(xb, *row);
		break;
	case '$':
		*off = lbuf_eol(xb, *row);
		break;
	case '|':
		*off = vi_col2off(xb, *row, cnt - 1);
		vi_pcol = cnt - 1;
		break;
	case '/':
		if (vi_search(mv, cnt, row, off))
			return -1;
		break;
	case '?':
		if (vi_search(mv, cnt, row, off))
			return -1;
		break;
	case 'n':
		if (vi_search(mv, cnt, row, off))
			return -1;
		break;
	case 'N':
		if (vi_search(mv, cnt, row, off))
			return -1;
		break;
	case TK_CTL('a'):
		if (vi_curword(lbuf_get(xb, *row), cw, sizeof(cw), *off, "") != 0)
			return -1;
		snprintf(kw, sizeof(kw), "\\<%s\\>", cw);
		ex_kwdset(kw, +1);
		vi_soset = 0;
		if (vi_search('n', cnt, row, off))
			return -1;
		break;
	case ' ':
		for (i = 0; i < cnt; i++)
			if (vi_nextoff(xb, +1, row, off))
				break;
		break;
	case 127:
	case TK_CTL('h'):
		for (i = 0; i < cnt; i++)
			if (vi_nextoff(xb, -1, row, off))
				break;
		break;
	case '`':
		if ((mark = vi_read()) <= 0)
			return -1;
		if (lbuf_jump(xb, mark, &mark_row, &mark_off))
			return -1;
		*row = mark_row;
		*off = mark_off;
		break;
	case '%':
		if (lbuf_pair(xb, row, off))
			return -1;
		break;
	default:
		vi_back(mv);
		return 0;
	}
	return mv;
}

static void swap(int *a, int *b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

static char *lbuf_region(struct lbuf *lb, int r1, int o1, int r2, int o2)
{
	struct sbuf *sb;
	char *s1, *s2, *s3;
	if (r1 == r2)
		return uc_sub(lbuf_get(lb, r1), o1, o2);
	sb = sbuf_make();
	s1 = uc_sub(lbuf_get(lb, r1), o1, -1);
	s3 = uc_sub(lbuf_get(lb, r2), 0, o2);
	s2 = lbuf_cp(lb, r1 + 1, r2);
	sbuf_str(sb, s1);
	sbuf_str(sb, s2);
	sbuf_str(sb, s3);
	free(s1);
	free(s2);
	free(s3);
	return sbuf_done(sb);
}

static int vi_yank(int r1, int o1, int r2, int o2, int lnmode)
{
	char *region;
	region = lbuf_region(xb, r1, lnmode ? 0 : o1, r2, lnmode ? -1 : o2);
	reg_put(vi_ybuf, region, lnmode);
	free(region);
	xrow = r1;
	xoff = lnmode ? xoff : o1;
	return 0;
}

static int vi_delete(int r1, int o1, int r2, int o2, int lnmode)
{
	char *region;
	region = lbuf_region(xb, r1, lnmode ? 0 : o1, r2, lnmode ? -1 : o2);
	reg_put(vi_ybuf, region, lnmode);
	free(region);
	if (!lnmode) {
		struct sbuf *sb = sbuf_make();
		char *s1 = lbuf_get(xb, r1);
		char *s2 = lbuf_get(xb, r2);
		sbuf_mem(sb, s1, s1 ? uc_chr(s1, o1) - s1 : 0);
		sbuf_str(sb, s2 ? uc_chr(s2, o2) : "");
		lbuf_edit(xb, sbuf_buf(sb), r1, r2 + 1);
		sbuf_free(sb);
	} else {
		lbuf_edit(xb, NULL, r1, r2 + 1);
	}
	xrow = r1;
	xoff = lnmode ? lbuf_indents(xb, xrow) : o1;
	vi_drawfix(r1, r2 - r1 + 1, !lnmode);
	return VC_OK;
}

static int linecount(char *s)
{
	int n;
	for (n = 0; s; n++)
		if ((s = strchr(s, '\n')))
			s++;
	return n;
}

static char *vi_help(char *ln)
{
	static char ac[128];
	char cmd[128] = "ra \\~";
	char *info = " ";
	char *beg = NULL, *end = NULL;
	int lastkind = 0;
	char *s, *r;
	/* execute \~ register, if defined */
	if (reg_get(0x80 | '~', NULL)) {
		reg_put('~', ln, 0);
		if (!ex_command(cmd)) {
			snprintf(ac, sizeof(ac), "%s", reg_get('~', NULL));
			if (strchr(ac, '\n') != NULL)
				*strchr(ac, '\n') = '\0';
			return ac;
		}
		return NULL;
	}
	/* extract tag information */
	for (s = ln; s && *s; s = uc_next(s)) {
		int kind = uc_kind(s);
		if (lastkind != 1 && kind == 1)
			beg = s;
		if (lastkind == 1 && kind != 1)
			end = s;
		lastkind = kind;
	}
	end = lastkind == 1 ? s : end;
	if (beg != NULL) {
		char tag[128];
		int pos = 0, dir = 0;
		if (beg != NULL && end - beg + 1 > sizeof(tag))
			end = beg + sizeof(tag) - 1;
		memcpy(tag, beg, end - beg);
		tag[end - beg] = '\0';
		if (!tag_find(tag, &pos, dir, NULL, 0, cmd, sizeof(cmd)))
			if ((r = strrchr(cmd, '"')) != NULL)
				info = r + 1;
	}
	led_print(info, xrows, 0, xcols, xhl ? "---" : "___", &vi_ledmod);
	term_pos(xrow - xtop, 0);
	return NULL;
}

static int vi_indents(char *ln, char *in, int size)
{
	int cnt = 0;
	while (cnt < size && ln && (*ln == ' ' || *ln == '\t'))
		in[cnt++] = *ln++;
	in[cnt] = '\0';
	return cnt;
}

static int vi_change(int r1, int o1, int r2, int o2, int lnmode)
{
	char *region;
	struct sbuf *sb = sbuf_make();
	char *s1 = lbuf_get(xb, r1);
	char *s2 = lbuf_get(xb, r2);
	region = lbuf_region(xb, r1, lnmode ? 0 : o1, r2, lnmode ? -1 : o2);
	reg_put(vi_ybuf, region, lnmode);
	free(region);
	if (xai && lnmode)
		vi_indents(s1, vi_ai, sizeof(vi_ai) - 1);
	if (lnmode) {
		sbuf_str(sb, xai ? vi_ai : "");
		sbuf_str(sb, "\n");
	} else {
		sbuf_mem(sb, s1, s1 ? uc_chr(s1, o1) - s1 : 0);
		sbuf_str(sb, s2 ? uc_chr(s2, o2) : "");
	}
	lbuf_edit(xb, sbuf_buf(sb), r1, r2 + 1);
	sbuf_free(sb);
	xrow = r1;
	xoff = xai && lnmode ? lbuf_indents(xb, r1) : o1;
	vi_drawfix(r1, r2 - r1 + 1, 1);
	vi_insoff = xoff;
	vi_insert = 1;
	return VC_OK;
}

static int vi_case(int r1, int o1, int r2, int o2, int lnmode, int cmd)
{
	char *region, *s;
	region = lbuf_region(xb, r1, lnmode ? 0 : o1, r2, lnmode ? -1 : o2);
	s = region;
	while (*s) {
		int c = (unsigned char) s[0];
		if (c <= 0x7f) {
			if (cmd == 'u')
				s[0] = tolower(c);
			if (cmd == 'U')
				s[0] = toupper(c);
			if (cmd == '~')
				s[0] = islower(c) ? toupper(c) : tolower(c);
		}
		s = uc_next(s);
	}
	if (!lnmode) {
		struct sbuf *sb = sbuf_make();
		char *s1 = lbuf_get(xb, r1);
		char *s2 = lbuf_get(xb, r2);
		sbuf_mem(sb, s1, s1 ? uc_chr(s1, o1) - s1 : 0);
		sbuf_str(sb, region);
		sbuf_str(sb, s2 ? uc_chr(s2, o2) : "");
		lbuf_edit(xb, sbuf_buf(sb), r1, r2 + 1);
		sbuf_free(sb);
	} else {
		lbuf_edit(xb, region, r1, r2 + 1);
	}
	xrow = r2;
	xoff = lnmode ? lbuf_indents(xb, r2) : o2;
	free(region);
	vi_drawfix(r1, r2 - r1 + 1, r2 - r1 + 1);
	return VC_OK;
}

static int vi_pipe(int r1, int r2)
{
	char *text;
	char *rep;
	int kmap = 0;
	char *cmd = vi_prompt("!", &kmap, reg_getln('!'));
	if (!cmd)
		return 0;
	reg_put('!', cmd, 1);
	reg_putln('!', cmd);
	text = lbuf_cp(xb, r1, r2 + 1);
	rep = cmd_pipe(cmd, text, 1);
	if (rep)
		lbuf_edit(xb, rep, r1, r2 + 1);
	free(cmd);
	free(text);
	free(rep);
	return VC_WIN;
}

static int vi_shift(int r1, int r2, int dir)
{
	struct sbuf *sb;
	char *ln;
	int i;
	for (i = r1; i <= r2; i++) {
		if (!(ln = lbuf_get(xb, i)))
			continue;
		sb = sbuf_make();
		if (dir > 0) {
			if (ln[0] != '\n')
				sbuf_chr(sb, '\t');
		} else {
			ln = ln[0] == ' ' || ln[0] == '\t' ? ln + 1 : ln;
		}
		sbuf_str(sb, ln);
		lbuf_edit(xb, sbuf_buf(sb), i, i + 1);
		sbuf_free(sb);
	}
	xrow = r1;
	xoff = lbuf_indents(xb, xrow);
	vi_drawfix(r1, r2 - r1 + 1, r2 - r1 + 1);
	return VC_OK;
}

static int vc_motion(int cmd)
{
	int r1 = xrow, r2 = xrow;	/* region rows */
	int o1 = xoff, o2 = xoff;	/* visual region columns */
	int lnmode = 0;			/* line-based region */
	int mv;
	vi_arg2 = vi_prefix();
	if (vi_arg2 < 0)
		return 0;
	o1 = ren_noeol(lbuf_get(xb, r1), o1);
	o2 = o1;
	if ((mv = vi_motionln(&r2, cmd))) {
		o2 = -1;
	} else if (!(mv = vi_motion(&r2, &o2))) {
		vi_read();
		return 0;
	}
	if (mv < 0)
		return 0;
	lnmode = o2 < 0;
	if (lnmode) {
		o1 = 0;
		o2 = lbuf_eol(xb, r2);
	}
	if (r1 > r2) {
		swap(&r1, &r2);
		swap(&o1, &o2);
	}
	if (r1 == r2 && o1 > o2)
		swap(&o1, &o2);
	o1 = ren_noeol(lbuf_get(xb, r1), o1);
	if (!lnmode && strchr("fFtTeE%", mv))
		if (o2 < lbuf_eol(xb, r2))
			o2 = ren_noeol(lbuf_get(xb, r2), o2) + 1;
	if (cmd == 'y')
		return vi_yank(r1, o1, r2, o2, lnmode);
	if (cmd == 'd')
		return vi_delete(r1, o1, r2, o2, lnmode);
	if (cmd == 'c')
		return vi_change(r1, o1, r2, o2, lnmode);
	if (cmd == '~' || cmd == 'u' || cmd == 'U')
		return vi_case(r1, o1, r2, o2, lnmode, cmd);
	if (cmd == '>' || cmd == '<')
		return vi_shift(r1, r2, cmd == '>' ? +1 : -1);
	if (cmd == '!') {
		if (mv == '{' || mv == '}')
			if (lbuf_get(xb, r2) && lbuf_get(xb, r2)[0] == '\n' && r1 < r2)
				r2--;
		return vi_pipe(r1, r2);
	}
	return 0;
}

static int vc_insert(int cmd)
{
	char in[sizeof(vi_ai) + 1];
	char *ln = lbuf_get(xb, xrow);
	if (cmd == 'I')
		xoff = lbuf_indents(xb, xrow);
	if (cmd == 'A')
		xoff = lbuf_eol(xb, xrow);
	if ((cmd == 'o' || cmd == 'O') && xai)
		vi_indents(ln, vi_ai, sizeof(vi_ai) - 1);
	if (cmd == 'o' && ln)
		xrow++;
	if (cmd == 'a')
		xoff++;
	if (!ln || ln[0] == '\n')
		xoff = 0;
	if ((cmd != 'o' && cmd != 'O') && !ln)
		lbuf_edit(xb, "\n", xrow, xrow);
	if (cmd == 'o' || cmd == 'O') {
		snprintf(in, sizeof(in), "%s\n", vi_ai);
		lbuf_edit(xb, xai ? in : "\n", xrow, xrow);
	}
	if (cmd == 'o' || cmd == 'O')
		xoff = xai ? lbuf_indents(xb, xrow) : 0;
	if ((cmd == 'o' || cmd == 'O') && ln)
		vi_drawfix(xrow, 0, 1);
	if (cmd == 'O' && xhll && ln)
		vi_drawrow(xrow + 1);
	if (!ln)
		vi_drawrow(0);
	vi_insoff = xoff;
	vi_insert = 1;
	led_reset(&vi_ledins);
	return VC_OK;
}

static void vi_edit(int off, int del, char *ins)
{
	char *ln = lbuf_get(xb, xrow);
	struct sbuf *sb = sbuf_make();
	char *pos = ln ? uc_chr(ln, off) : NULL;
	char *end = ln ? uc_chr(pos, del) : NULL;
	char *last = strrchr(ins, '\n');
	int lncnt = linecount(ins);
	int row = xrow;
	last = last ? last + 1 : ins;
	sbuf_mem(sb, ln, ln ? pos - ln : 0);
	sbuf_str(sb, ins);
	sbuf_str(sb, end && end[0] ? end : "\n");
	lbuf_edit(xb, sbuf_buf(sb), xrow, xrow + 1);
	vi_drawrow(xrow);
	xrow += lncnt - 1;
	xoff = xoff - del + uc_slen(last) - (lncnt > 1 ? off : 0);
	vi_insoff = lncnt > 1 ? 0 : vi_insoff;
	if (lncnt > 1)
		led_reset(&vi_ledins);
	if (lncnt > 1)
		vi_drawfix(row + 1, 0, lncnt - 1);
	sbuf_free(sb);
}

static int vi_lastword(char *s, int off)
{
	char *r = uc_chr(s, off);
	int kind = -1;
	while (r > s) {
		r = uc_beg(s, r - 1);
		off--;
		if (kind >= 0 && uc_kind(r) != kind)
			return off + 1;
		if (kind < 0 && !uc_isspace(r))
			kind = uc_kind(r);
	}
	return off;
}

static int vc_insertcmd(void)
{
	int c = vi_read();
	char *s = NULL;
	int off;
	switch (c) {
	case TK_CTL('f'):
		xkmap = xkmap_alt;
		return VC_OK;
	case TK_CTL('e'):
		xkmap = 0;
		return VC_OK;
	case TK_CTL('h'):
	case 127:
		if (xoff > vi_insoff)
			vi_edit(xoff - 1, 1, "");
		return VC_OK;
	case TK_CTL('u'):
		if (xoff > vi_insoff)
			vi_edit(vi_insoff, xoff - vi_insoff, "");
		return VC_OK;
	case TK_CTL('w'):
		s = lbuf_get(xb, xrow);
		off = vi_lastword(s, xoff);
		off = MAX(off, vi_insoff);
		if (off < xoff)
			vi_edit(off, xoff - off, "");
		return VC_OK;
	case TK_CTL('t'):
		vi_edit(0, 0, "\t");
		vi_insoff = vi_insoff > 0 ? vi_insoff + 1 : 0;
		return VC_OK;
	case TK_CTL('d'):
		s = lbuf_get(xb, xrow);
		if (s[0] == ' ' || s[0] == '\t') {
			vi_edit(0, 1, "");
			vi_insoff = vi_insoff > 0 ? vi_insoff - 1 : 0;
		}
		return VC_OK;
	case TK_CTL('a'):
		if (1) {
			char cmp[128];
			char *ln = uc_sub(lbuf_get(xb, xrow), 0, xoff);
			char *ac = vi_help(ln);
			if (ac != NULL)
				snprintf(cmp, sizeof(cmp), "%s", ac);
			free(ln);
		}
		return VC_OK;
	case '\n':
		if (xai) {
			char *ln = lbuf_get(xb, xrow);
			char in[LEN(vi_ai) + 1];
			int sz = MIN(LEN(in) - 2, xoff);
			int indents = vi_indents(ln, in, sz);
			int noindents = indents == sz;
			snprintf(vi_ai, sizeof(vi_ai), "%s", in);
			snprintf(in, sizeof(in), "\n%s", vi_ai);
			vi_edit(noindents ? 0 : xoff, noindents ? xoff : 0, in);
		} else {
			vi_edit(xoff, 0, "\n");
		}
		return VC_OK;
	default:
		if (TK_INT(c)) {
			char *ln = lbuf_get(xb, xrow);
			if (xai && ln[lbuf_indents(xb, xrow)] == '\n') {
				lbuf_edit(xb, "\n", xrow, xrow + 1);
				xoff = 0;
			}
			vi_insert = 0;
			xoff = MAX(0, xoff - 1);
			return VC_OK;
		}
	}
	vi_back(c);
	if ((s = vi_char(vi_read, xkmap)))
		vi_edit(xoff, 0, s);
	return VC_OK;
}

static int vc_put(int cmd)
{
	int cnt = MAX(1, vi_arg1);
	int lnmode;
	char *buf = reg_get(vi_ybuf, &lnmode);
	int lncnt = 0;
	int i;
	if (!buf)
		snprintf(vi_msg, sizeof(vi_msg), "yank buffer empty");
	if (!buf || !buf[0])
		return 0;
	if (lnmode) {
		struct sbuf *sb = sbuf_make();
		for (i = 0; i < cnt; i++)
			sbuf_str(sb, buf);
		if (cmd == 'p' && lbuf_len(xb))
			xrow++;
		lbuf_edit(xb, sbuf_buf(sb), xrow, xrow);
		lncnt = linecount(sbuf_buf(sb));
		xoff = lbuf_indents(xb, xrow);
		sbuf_free(sb);
	} else {
		struct sbuf *sb = sbuf_make();
		char *ln = xrow < lbuf_len(xb) ? lbuf_get(xb, xrow) : "\n";
		int off = ren_noeol(ln, xoff) + (ln[0] != '\n' && cmd == 'p');
		char *s = uc_sub(ln, 0, off);
		sbuf_str(sb, s);
		free(s);
		for (i = 0; i < cnt; i++)
			sbuf_str(sb, buf);
		s = uc_sub(ln, off, -1);
		sbuf_str(sb, s);
		free(s);
		lbuf_edit(xb, sbuf_buf(sb), xrow, xrow + 1);
		lncnt = linecount(sbuf_buf(sb)) - 1;
		xoff = off + uc_slen(buf) * cnt - 1;
		sbuf_free(sb);
	}
	vi_drawfix(xrow, 1, lncnt);
	return VC_OK;
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

static int vc_join(void)
{
	struct sbuf *sb;
	int cnt = vi_arg1 <= 1 ? 2 : vi_arg1;
	int beg = xrow;
	int end = xrow + cnt;
	int off = 0;
	int i;
	if (!lbuf_get(xb, beg) || !lbuf_get(xb, end - 1))
		return 0;
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
	lbuf_edit(xb, sbuf_buf(sb), beg, end);
	xoff = off;
	sbuf_free(sb);
	vi_drawfix(xrow, end - beg, 1);
	return VC_OK;
}

static int vi_scrollforward(int cnt)
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

static int vc_status(void)
{
	int col = vi_off2col(xb, xrow, xoff);
	int c = vi_insert ? 'I' : 'N';
	snprintf(vi_msg, sizeof(vi_msg),
		"%c%04d %c %s  %s %d C%d",
		w_tmp ? '_' : c, xrow + 1,
		lbuf_modified(xb) ? 'M' : '-',
		ex_path()[0] ? ex_path() : "unnamed",
		kmap_map(xkmap, 0),
		lbuf_len(xb), ren_cursor(lbuf_get(xb, xrow), col) + 1);
	return 0;
}

static int vc_charinfo(void)
{
	char *c = uc_chr(lbuf_get(xb, xrow), xoff);
	if (c) {
		char cbuf[8] = "";
		memcpy(cbuf, c, uc_len(c));
		snprintf(vi_msg, sizeof(vi_msg), "<%s> %04x", cbuf, uc_code(c));
	}
	return 0;
}

static int vc_replace(void)
{
	int cnt = MAX(1, vi_arg1);
	char *cs = vi_char(vi_read, xkmap);
	char *ln = lbuf_get(xb, xrow);
	struct sbuf *sb;
	char *pos, *end;
	int off, i;
	if (!ln || !cs)
		return 0;
	off = ren_noeol(ln, xoff);
	pos = uc_chr(ln, off);
	end = uc_chr(pos, cnt);
	if (!end || !end[0])
		return 0;
	sb = sbuf_make();
	sbuf_mem(sb, ln, pos - ln);
	for (i = 0; i < cnt; i++)
		sbuf_str(sb, cs);
	sbuf_str(sb, end);
	lbuf_edit(xb, sbuf_buf(sb), xrow, xrow + 1);
	if (cs[0] == '\n') {
		xrow += cnt;
		xoff = 0;
		vi_drawfix(xrow - cnt, 1, cnt + 1);
	} else {
		xoff = off + cnt - 1;
		vi_drawfix(xrow, 1, 1);
	}
	sbuf_free(sb);
	return VC_OK;
}

static void vi_marksave(void)
{
	lbuf_mark(xb, '\'', xrow, xoff);
	lbuf_mark(xb, '`', xrow, xoff);
}

static int vc_definition(char *ln, int off, int newwin)
{
	char cw[256], kw[256];
	char *s;
	int r = 0, o = 0;
	int len = 0;
	if (vi_curword(ln, cw, sizeof(cw), off, "") != 0)
		return 0;
	snprintf(kw, sizeof(kw), conf_definition(ex_filetype()), cw);
	if (lbuf_search(xb, kw, +1, &r, &o, &len) != 0) {
		snprintf(vi_msg, sizeof(vi_msg), "not found <%s>", kw);
		return 0;
	}
	ln = lbuf_get(xb, r);
	if ((s = strstr(ln, cw)) != NULL)
		o = s - ln;
	vi_marksave();
	if (newwin)
		vi_wmirror();
	xrow = r;
	xoff = o;
	return newwin ? VC_ALL : VC_COL;
}

static int vc_openpath(char *ln, int off, int num, int newwin)
{
	char path[250], ex[256];
	char *sep;
	if (vi_curword(ln, path, sizeof(path), off, "-/.:") != 0)
		return 0;
	if ((sep = strchr(path, ':')) != NULL)
		*sep = '\0';
	if (access(path, R_OK) != 0) {
		snprintf(vi_msg, sizeof(vi_msg), "cannot open <%s>", path);
		return 0;
	}
	if (newwin)
		vi_wmirror();
	snprintf(ex, sizeof(ex), "e %s", path);
	if (ex_command(ex))
		return newwin ? VC_ALL : VC_WIN;
	if (num && sep && isdigit((unsigned char) sep[1])) {
		char *col = strchr(sep + 1, ':');
		int lnum = atoi(sep + 1);
		vi_marksave();
		xrow = MIN(MAX(0, lnum - 1), lbuf_len(xb) - 1);
		if (col && isdigit((unsigned char) col[1]))
			xoff = ren_noeol(lbuf_get(xb, xrow), atoi(col + 1) - 1);
	}
	return newwin ? VC_ALL : VC_WIN;
}

static int vc_tag(char *ln, int off, int newwin)
{
	char cw[120], ex[128];
	if (vi_curword(ln, cw, sizeof(cw), off, ""))
		return 0;
	snprintf(ex, sizeof(ex), "ta %s", cw);
	vi_marksave();
	if (ex_command(ex) != 0)
		return 0;
	if (newwin) {
		ex_command("po");
		vi_wmirror();
		ex_command(ex);
	}
	return newwin ? VC_ALL : VC_WIN;
}

static char rep_cmd[4096];	/* the last command */
static int rep_len;

static void vi_repeatset(int save)
{
	int n;
	char *cmd = term_cmd(&n);
	if (save && n + 1 < sizeof(rep_cmd)) {
		memcpy(rep_cmd, cmd, n);
		rep_len = n;
		rep_cmd[n] = '\0';
		reg_put('.', rep_cmd, 0);
	}
}

static void vc_repeat(void)
{
	int i;
	for (i = 0; i < MAX(1, vi_arg1); i++)
		term_push(rep_cmd, rep_len);
}

static void vc_execute(void)
{
	static int reg = -1;
	int lnmode;
	int c = (c = vi_read()) == '\\' ? 0x80 | vi_read() : c;
	char *buf = NULL;
	int i;
	if (TK_INT(c))
		return;
	if (c == '@')
		c = reg;
	reg = c;
	if (reg >= 0)
		buf = reg_get(reg, &lnmode);
	if (buf != NULL) {
		for (i = 0; i < MAX(1, vi_arg1); i++)
			term_push(buf, strlen(buf));
	}
}

static int vc_ecmd(int c, int newwin)
{
	char cmd[256];
	char *out;
	snprintf(cmd, sizeof(cmd), "%s %c %s %d %d",
		conf_ecmd(), c, ex_path(), xrow + 1, xoff + 1);
	if ((out = cmd_pipe(cmd, NULL, 1)) == NULL) {
		snprintf(vi_msg, sizeof(vi_msg), "command failed");
		return 0;
	}
	if (!strchr(out, '\n')) {
		snprintf(vi_msg, sizeof(vi_msg), "no output");
		free(out);
		return 0;
	}
	if (newwin)
		vi_wmirror();
	ex_command(out);
	free(out);
	return VC_ALL;
}

/* format a quick leap item */
static char *vi_leapfmt(int id, char *ln)
{
	static char out[128];
	char *path;
	int mid = 0, des, sep = 48;
	while (ln[mid] && !isspace((unsigned char) ln[mid]))
		mid++;
	des = ln[mid] ? mid + 1 : mid;
	ln[mid] = '\0';
	while (ln[des] && isspace((unsigned char) ln[des]))
		des++;
	if (!ln[des])
		sep = MIN(mid, 80);
	path = mid <= sep ? ln : ln + mid - sep;
	if (mid > sep)
		path[0] = '-';
	snprintf(out, sizeof(out), "[%d] %-*s  %s", id + 1, sep, path, ln + des);
	return out;
}

/* list quick leap items */
static void vi_leaplist(char *list[], int cnt, int matches, int total, char *mod, char *pos)
{
	char cmd[256];
	int i;
	for (i = 0; i < cnt; i++) {
		strcpy(cmd, "[-]");
		if (list[i]) {
			char ln[256];
			snprintf(ln, sizeof(ln), "%s", list[i]);
			snprintf(cmd, sizeof(cmd), "%s", vi_leapfmt(i, ln));
		}
		vi_drawquick(cmd, xrows - cnt + i);
	}
	snprintf(cmd, sizeof(cmd), "LEAP %s (%d/%d) %s", mod, matches, total, pos);
	vi_drawquick(cmd, cnt ? xrows - cnt - 1 : xrows);
	term_pos(-1, 5);
}

/* ret>=0: selection index, ret=-1 interrupted */
static int vi_leap(struct tlist *tls, char *mod, char *pos, int minrows)
{
	char msg[256];
	char kws[256];
	int view[9];
	char *view_s[LEN(view)];
	int i;
	kws[0] = '\0';
	while (1) {
		int view_n = tlist_top(tls, view, LEN(view));
		char *kw;
		minrows = MAX(minrows, view_n);
		for (i = 0; i < LEN(view); i++)
			view_s[i] = i < view_n ? tlist_get(tls, view[i]) : NULL;
		vi_leaplist(view_s, minrows, tlist_matches(tls), tlist_cnt(tls), mod, pos);
		snprintf(msg, sizeof(msg), "Filter (%s): ", kws);
		if (!(kw = vi_prompt(msg, &xkmap, NULL)))
			return -1;
		if (!kw[0]) {
			free(kw);
			return view_n ? view[0] : -1;
		} else if (kw[0] == '\\' && !kw[2] && kw[1] >= '1' && kw[1] <= '9') {
			int idx = kw[1] - '1';
			free(kw);
			return idx < view_n ? view[idx] : -1;
		}
		snprintf(strchr(kws, '\0'), sizeof(kws) - strlen(kws),
			"%s%s", kws[0] ? "|" : "", kw);
		tlist_filt(tls, kw);
		free(kw);
	}
	return -1;
}

static int vc_quick(int newwin)
{
	char cmd[256];
	struct tlist *tls;
	char *ls[32];
	int sel = -1, ls_n = 0;
	int mod = 0, c;
	char *pos = ex_path()[0] ? ex_path() : "unnamed";
	ls_n = ex_list(ls, LEN(ls));
	tls = tlist_make(ls + 1, ls_n - 1);
	vi_leaplist(ls + 1, ls_n - 1, ls_n - 1, ls_n - 1, "BUFF", pos);
	term_commit();
	c = vi_read();
	if (c == ',' || c == ';' || c == '=') {
		char *name = c == ',' ? "FILE" : "BUFF";
		if (c == '=')
			name = "TAGS";
		mod = c;
		tlist_free(tls);
		if (mod == ';')
			tls = tlist_make(ls + 1, ls_n - 1);
		if (mod == ',')
			tls = tlist_from("ls");
		if (mod == '=')
			tls = tlist_tags("tags");
		if (tls)
			sel = vi_leap(tls, name, pos, ls_n - 1);
	} else if (c >= '1' && c <= '9') {
		sel = c - '1';
	}
	led_reset(&vi_ledmod);
	if (sel >= 0)
		snprintf(cmd, sizeof(cmd), "%s", tlist_get(tls, sel));
	if (tls)
		tlist_free(tls);
	if (sel >= 0 && mod == '=')
		return vc_tag(cmd, 0, newwin) | VC_WIN;
	if (sel >= 0)
		return vc_openpath(cmd, 0, 1, newwin) | VC_WIN;
	if (isalpha(c) && reg_get(0x80 | c, NULL) != NULL) {
		char cmd[8] = {'@', '\\', c};
		if (newwin)
			vi_wmirror();
		ex_command(cmd);
		return VC_ALL;
	}
	if (isalpha(c))
		return vc_ecmd(c, newwin) != 0 ? VC_ALL : VC_WIN;
	return mod || ls_n > 1 ? VC_WIN : 0;
}

static int vc_jumpglob(int mark, int lnmode)
{
	char cmd[16];
	int mod = 0;
	int mark_row, mark_off;
	if (mark <= 0 || mark >= LEN(glob_id) || glob_id[mark] <= 0)
		return 0;
	vi_marksave();
	if (glob_id[mark] != ex_id()) {
		snprintf(cmd, sizeof(cmd), "b %d", glob_id[mark]);
		if (ex_command(cmd))
			return 0;
		mod = VC_WIN;
	}
	if (!lbuf_jump(xb, mark, &mark_row, &mark_off)) {
		xrow = mark_row;
		xoff = lnmode ? 0 : mark_off;
	}
	return VC_COL | mod;
}

static void sigwinch(int signo)
{
	vi_back(TK_CTL('l'));
	vi_back(TK_CTL('c'));
}

static void vi(void)
{
	int xcol;
	int mark;
	char *ln;
	int kmap = 0;
	signal(SIGWINCH, sigwinch);
	vi_switch(0);
	xtop = MAX(0, xrow - xrows / 2);
	xoff = 0;
	xcol = vi_off2col(xb, xrow, xoff);
	vi_drawagain(xcol, -1);
	term_pos(xrow - xtop, vi_pos(lbuf_get(xb, xrow), xcol));
	term_commit();
	while (!xquit) {
		int mod = 0;
		int nrow = xrow;
		int noff = ren_noeol(lbuf_get(xb, xrow), xoff);
		int otop = xtop;
		int oleft = xleft;
		int orow = xrow;
		char *opath = ex_path();	/* do not dereference; to detect buffer changes */
		int mv = 0, n, ru;
		if (!vi_insert) {
			term_cmd(&n);
			vi_arg2 = 0;
			vi_ybuf = vi_yankbuf();
			vi_arg1 = vi_prefix();
			if (!vi_ybuf)
				vi_ybuf = vi_yankbuf();
			mv = vi_motion(&nrow, &noff);
		}
		if (vi_insert) {
			mod = vc_insertcmd();
			if (!vi_insert)
				vi_repeatset(1);
			if (!vi_insert)
				glob_id['*'] = ex_id();
		} else if (mv > 0) {
			if (strchr("\'`GHML/?{}[]nN", mv) || (mv == '%' && noff < 0))
				vi_marksave();
			xrow = nrow;
			if (noff < 0 && !strchr("jk", mv))
				noff = lbuf_indents(xb, xrow);
			if (strchr("jk", mv))
				noff = vi_col2off(xb, xrow, xcol);
			xoff = ren_noeol(lbuf_get(xb, xrow), noff);
			if (!strchr("|jk", mv))
				xcol = vi_off2col(xb, xrow, xoff);
			if (mv == '|')
				xcol = vi_pcol;
		} else if (mv == 0) {
			int c = vi_read();
			int k = 0;
			if (c <= 0)
				continue;
			lbuf_mark(xb, '^', xrow, xoff);
			switch (c) {
			case TK_CTL('b'):
				if (vi_scrollbackward(MAX(1, vi_arg1) * (xrows - 1)))
					break;
				xoff = lbuf_indents(xb, xrow);
				mod = VC_COL;
				break;
			case TK_CTL('f'):
				if (vi_scrollforward(MAX(1, vi_arg1) * (xrows - 1)))
					break;
				xoff = lbuf_indents(xb, xrow);
				mod = VC_COL;
				break;
			case TK_CTL('e'):
				if (vi_scrollforward(MAX(1, vi_arg1)))
					break;
				xoff = vi_col2off(xb, xrow, xcol);
				break;
			case TK_CTL('y'):
				if (vi_scrollbackward(MAX(1, vi_arg1)))
					break;
				xoff = vi_col2off(xb, xrow, xcol);
				break;
			case TK_CTL('u'):
				if (xrow == 0)
					break;
				if (vi_arg1)
					vi_scroll = vi_arg1;
				n = vi_scroll ? vi_scroll : xrows / 2;
				xrow = MAX(0, xrow - n);
				if (xtop > 0)
					xtop = MAX(0, xtop - n);
				xoff = lbuf_indents(xb, xrow);
				mod = VC_COL;
				break;
			case TK_CTL('d'):
				if (xrow == lbuf_len(xb) - 1)
					break;
				if (vi_arg1)
					vi_scroll = vi_arg1;
				n = vi_scroll ? vi_scroll : xrows / 2;
				xrow = MIN(MAX(0, lbuf_len(xb) - 1), xrow + n);
				if (xtop < lbuf_len(xb) - xrows)
					xtop = MIN(lbuf_len(xb) - xrows, xtop + n);
				xoff = lbuf_indents(xb, xrow);
				mod = VC_COL;
				break;
			case TK_CTL('z'):
				term_pos(xrows, 0);
				term_suspend();
				mod = VC_ALL;
				break;
			case 'u':
				if (!lbuf_undo(xb)) {
					lbuf_jump(xb, '^', &xrow, &xoff);
					mod = VC_WIN;
				} else {
					snprintf(vi_msg, sizeof(vi_msg), "undo failed");
				}
				break;
			case TK_CTL('r'):
				if (!lbuf_redo(xb)) {
					lbuf_jump(xb, '^', &xrow, &xoff);
					mod = VC_WIN;
				} else {
					snprintf(vi_msg, sizeof(vi_msg), "redo failed");
				}
				break;
			case TK_CTL('g'):
				vc_status();
				break;
			case TK_CTL('^'):
				ex_command("e #");
				mod = VC_WIN;
				break;
			case TK_CTL(']'):
				mod = vc_tag(lbuf_get(xb, xrow), xoff, 0);
				break;
			case TK_CTL('t'):
				if (!ex_command("pop")) {
					vi_marksave();
					mod = VC_WIN;
				}
				break;
			case TK_CTL('w'):
				k = vi_read();
				if (k == 's')
					if (!vi_wsplit())
						mod = VC_ALL;
				if (k == 'j' || k == 'k') {
					if (w_cnt > 1) {
						vi_switch(1 - w_cur);
						mod = VC_ALL;
					}
				}
				if (k == 'o')
					if (!vi_wonly())
						mod = VC_WIN;
				if (k == 'c')
					if (!vi_wclose())
						mod = VC_WIN;
				if (k == 'x')
					if (!vi_wswap())
						mod = VC_ALL;
				if (k == TK_CTL(']') || k == ']')
					mod = vc_tag(lbuf_get(xb, xrow), xoff, 1);
				if (k == 'g') {
					char *ln = lbuf_get(xb, xrow);
					int j = vi_read();
					if (j == 'f' || j == 'l')
						mod = vc_openpath(ln, xoff, j == 'l', 1);
					if (j == 'd')
						mod = vc_definition(ln, xoff, 1);
				}
				if (k == 'q')
					mod = vc_quick(1);
				break;
			case ':':
				ln = vi_prompt(":", &kmap, reg_getln(':'));
				if (ln && ln[0]) {
					reg_putln(':', ln);
					if (ln[0] != ':') {
						char *ln2 = uc_cat(":", ln);
						free(ln);
						ln = ln2;
					}
					if (!ex_command(ln) && strcmp(ln, ":w"))
						mod = VC_ALL;
					reg_put(':', ln, 1);
				}
				free(ln);
				if (xquit)
					continue;
				break;
			case 'c':
			case 'd':
			case 'y':
			case '!':
			case '>':
			case '<':
				mod = vc_motion(c);
				break;
			case 'i':
			case 'I':
			case 'a':
			case 'A':
			case 'o':
			case 'O':
				mod = vc_insert(c);
				break;
			case 'J':
				mod = vc_join();
				break;
			case TK_CTL('l'):
				term_done();
				term_init();
				mod = VC_ALL;
				break;
			case 'm':
				if ((mark = vi_read()) > 0 && isalpha(mark)) {
					if (isupper(mark))
						glob_id[tolower(mark)] = ex_id();
					lbuf_mark(xb, tolower(mark), xrow, xoff);
				}
				break;
			case 'p':
			case 'P':
				mod = vc_put(c);
				break;
			case 'z':
				k = vi_read();
				switch (k) {
				case '\n':
					xtop = vi_arg1 ? vi_arg1 : xrow;
					break;
				case '.':
					n = vi_arg1 ? vi_arg1 : xrow;
					xtop = MAX(0, n - xrows / 2);
					break;
				case '-':
					n = vi_arg1 ? vi_arg1 : xrow;
					xtop = MAX(0, n - xrows + 1);
					break;
				case '>':
				case '<':
					xtd = 0;
					if (vi_arg1 <= 2)
						xtd = k == '>' ? MAX(1, vi_arg1) : -MAX(1, vi_arg1);
					mod = VC_WIN;
					break;
				case 'e':
				case 'f':
					xkmap = k == 'e' ? 0 : xkmap_alt;
					break;
				case 'j':
				case 'k':
					if (!ex_command(k == 'j' ? "b +" : "b -"))
						mod = VC_WIN;
					break;
				case 'J':
				case 'K':
					if (!ex_command(k == 'J' ? "next" : "prev"))
						mod = VC_WIN;
					break;
				case 'D':
					if (!ex_command("b !"))
						mod = VC_WIN;
					break;
				}
				break;
			case 'g':
				ln = lbuf_get(xb, xrow);
				k = vi_read();
				if (k == '~' || k == 'u' || k == 'U')
					mod = vc_motion(k);
				if (k == 'a')
					mod = vc_charinfo();
				if (k == 'd')
					mod = vc_definition(ln, xoff, 0);
				if (k == 'f' || k == 'l')
					mod = vc_openpath(ln, xoff, k == 'l', 0);
				if (k == '\'' || k == '`')
					mod = vc_jumpglob(vi_read(), k == '\'');
				break;
			case 'x':
				vi_back(' ');
				mod = vc_motion('d');
				break;
			case 'X':
				vi_back(TK_CTL('h'));
				mod = vc_motion('d');
				break;
			case 'C':
				vi_back('$');
				mod = vc_motion('c');
				break;
			case 'D':
				vi_back('$');
				mod = vc_motion('d');
				break;
			case 'q':
				mod = vc_quick(0);
				break;
			case 'r':
				mod = vc_replace();
				break;
			case 's':
				vi_back(' ');
				mod = vc_motion('c');
				break;
			case 'S':
				vi_back('c');
				mod = vc_motion('c');
				break;
			case 'Y':
				vi_back('y');
				mod = vc_motion('y');
				break;
			case 'Z':
				k = vi_read();
				if (k == 'Z')
					if (!ex_command("x"))
						mod = VC_WIN;
				break;
			case '~':
				vi_back(' ');
				mod = vc_motion('~');
				break;
			case '.':
				vc_repeat();
				break;
			case '@':
				vc_execute();
				break;
			default:
				continue;
			}
			if (!vi_insert) {
				int changecmd = strchr("!<>ACDIJOPRSXYacdioprsxy~", c) ||
					(c == 'g' && strchr("uU~", k));
				vi_repeatset(changecmd);
				if (changecmd)
					glob_id['*'] = ex_id();
			}
		}
		if (mod & VC_OK)
			otop = xtop;
		vi_wfix();
		if (mod)
			xcol = vi_off2col(xb, xrow, xoff);
		if (xcol >= xleft + xcols)
			xleft = xcol - xcols / 2;
		if (xcol < xleft)
			xleft = xcol < xcols ? 0 : xcol - xcols / 2;
		vi_wait();
		ru = (xru & 1) || ((xru & 2) && w_cnt > 1) || ((xru & 4) && opath != ex_path());
		if (mod & VC_ALT && w_cnt == 1)
			vi_switch(w_cur);
		if (mod & VC_ALT && w_cnt > 1) {
			char msg[sizeof(vi_msg)];
			int id = w_cur;
			w_tmp = 1;
			vi_switch(1 - id);
			vi_wfix();
			strcpy(msg, vi_msg);
			led_reset(&vi_ledmod);
			if (ru)
				vc_status();
			led_reset(&vi_ledmod);
			vi_drawagain(vi_off2col(xb, xrow, xoff), -1);
			strcpy(vi_msg, msg);
			w_tmp = 0;
			vi_switch(id);
		}
		if (mod & VC_WIN)
			led_reset(&vi_ledmod);
		if (ru && !vi_msg[0])
			vc_status();
		if (mod & (VC_ROW | VC_WIN) || xleft != oleft) {
			int lineonly = mod & VC_ROW && xleft == oleft && xtop == otop;
			vi_drawagain(xcol, lineonly ? xrow : -1);
			if (lineonly && xrow != orow)
				vi_drawagain(xcol, orow);
		} else {
			if (xtop != otop)
				vi_drawupdate(otop);
			if (xhll && xrow != orow && orow >= xtop && orow < xtop + xrows)
				vi_drawrow(orow);
			if (xhll && xrow != orow)
				vi_drawrow(xrow);
			if (vi_msg[0])
				vi_drawmsg();
		}
		ln = lbuf_get(xb, xrow);
		if (vi_insert)
			term_pos(xrow - xtop, vi_pos(ln, ren_insert(ln, xoff)));
		else
			term_pos(xrow - xtop, vi_pos(ln, ren_cursor(ln, xcol)));
		term_commit();
		if (!vi_insert)
			lbuf_tx(xb);
	}
	led_reset(&vi_ledmod);
	led_reset(&vi_ledins);
}

int main(int argc, char *argv[])
{
	int i;
	char *prog = strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
	xvis = strcmp("ex", prog) && strcmp("neatex", prog);
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (argv[i][1] == 's')
			xled = 0;
		if (argv[i][1] == 'e')
			xvis = 0;
		if (argv[i][1] == 'v')
			xvis = 1;
		if (argv[i][1] == 'h') {
			printf("usage: %s [options] [file...]\n\n", argv[0]);
			printf("options:\n");
			printf("  -v    start in vi mode\n");
			printf("  -e    start in ex mode\n");
			printf("  -s    silent mode (for ex mode only)\n");
			return 0;
		}
	}
	dir_init();
	syn_init();
	tag_init();
	if (!ex_init(argv + i)) {
		if (xled || xvis)
			term_init();
		if (xvis)
			vi();
		else
			ex();
		if (xled || xvis)
			term_done();
		ex_done();
	}
	free(w_path);
	reg_done();
	syn_done();
	dir_done();
	tag_done();
	return 0;
}
