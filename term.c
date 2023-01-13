#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "vi.h"

static struct sbuf *term_sbuf;
static int rows, cols;
static struct termios termios;

void term_init(void)
{
	struct winsize win;
	struct termios newtermios;
	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~(ICANON | ISIG);
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &newtermios);
	if (getenv("LINES"))
		rows = atoi(getenv("LINES"));
	if (getenv("COLUMNS"))
		cols = atoi(getenv("COLUMNS"));
	if (!ioctl(0, TIOCGWINSZ, &win)) {
		cols = win.ws_col;
		rows = win.ws_row;
	}
	cols = cols ? cols : 80;
	rows = rows ? rows : 25;
	term_str("\33[m");
}

void term_done(void)
{
	term_commit();
	tcsetattr(0, 0, &termios);
}

void term_suspend(void)
{
	term_done();
	kill(getpid(), SIGSTOP);
	term_init();
}

void term_record(void)
{
	if (!term_sbuf)
		term_sbuf = sbuf_make();
}

void term_commit(void)
{
	if (term_sbuf) {
		write(1, sbuf_buf(term_sbuf), sbuf_len(term_sbuf));
		sbuf_free(term_sbuf);
		term_sbuf = NULL;
	}
}

static void term_out(char *s)
{
	if (term_sbuf)
		sbuf_str(term_sbuf, s);
	else
		write(1, s, strlen(s));
}

void term_str(char *s)
{
	term_out(s);
}

void term_chr(int ch)
{
	char s[4] = {ch};
	term_out(s);
}

void term_kill(void)
{
	term_out("\33[K");
}

void term_room(int n)
{
	char cmd[16] = { 0 };
	if (n < 0)
		snprintf(cmd, sizeof(cmd) - 1, "\33[%dM", -n);
	if (n > 0)
		snprintf(cmd, sizeof(cmd) - 1, "\33[%dL", n);
	if (n)
		term_out(cmd);
}

void term_pos(int r, int c)
{
	char buf[32] = "\r";
	if (c < 0)
		c = 0;
	if (c >= xcols)
		c = cols - 1;
	if (r < 0)
		snprintf(buf, sizeof(buf) - 1, "\r\33[%d%c", abs(c),
			 c > 0 ? 'C' : 'D');
	else
		snprintf(buf, sizeof(buf) - 1, "\33[%d;%dH", r + 1, c + 1);
	term_out(buf);
}

int term_rows(void)
{
	return rows;
}

int term_cols(void)
{
	return cols;
}

static char ibuf[4096];		/* input character buffer */
static char icmd[4096];		/* read after the last term_cmd() */
static int ibuf_pos, ibuf_cnt;	/* ibuf[] position and length */
static int icmd_pos;		/* icmd[] position */

/* read s before reading from the terminal */
void term_push(char *s, int n)
{
	n = MIN(n, sizeof(ibuf) - ibuf_cnt);
	memcpy(ibuf + ibuf_cnt, s, n);
	ibuf_cnt += n;
}

/* return a static buffer containing inputs read since the last term_cmd() */
char *term_cmd(int *n)
{
	*n = icmd_pos;
	icmd_pos = 0;
	return icmd;
}

int term_read(void)
{
	struct pollfd ufds[1];
	int n, c;
	if (ibuf_pos >= ibuf_cnt) {
		ufds[0].fd = 0;
		ufds[0].events = POLLIN;
		if (poll(ufds, 1, -1) <= 0)
			return -1;
		/* read a single input character */
		if ((n = read(0, ibuf, 1)) <= 0)
			return -1;
		ibuf_cnt = n;
		ibuf_pos = 0;
	}
	c = ibuf_pos < ibuf_cnt ? (unsigned char) ibuf[ibuf_pos++] : -1;
	if (icmd_pos < sizeof(icmd))
		icmd[icmd_pos++] = c;
	return c;
}

/* return a static string that changes text attributes from old to att */
char *term_att(int att, int old)
{
	static char buf[128];
	char *s = buf;
	int fg = SYN_FG(att);
	int bg = SYN_BG(att);
	if (att == old)
		return "";
	s += snprintf(s, sizeof(buf) - 1 - (s - buf), "\33[");
	if (att & SYN_BD)
		s += snprintf(s, sizeof(buf) - 1 - (s - buf), ";1");
	if (att & SYN_IT)
		s += snprintf(s, sizeof(buf) - 1 - (s - buf), ";3");
	else if (att & SYN_RV)
		s += snprintf(s, sizeof(buf) - 1 - (s - buf), ";7");
	if (SYN_FGSET(att)) {
		if ((fg & 0xff) < 8)
			s += snprintf(s, sizeof(buf) - 1 - (s - buf), ";%d",
				      30 + (fg & 0xff));
		else
			s += snprintf(s, sizeof(buf) - 1 - (s - buf),
				      ";38;5;%d", (fg & 0xff));
	}
	if (SYN_BGSET(att)) {
		if ((bg & 0xff) < 8)
			s += snprintf(s, sizeof(buf) - 1 - (s - buf), ";%d",
				      40 + (bg & 0xff));
		else
			s += snprintf(s, sizeof(buf) - 1 - (s - buf),
				      ";48;5;%d", (bg & 0xff));
	}
	s += snprintf(s, sizeof(buf) - 1 - (s - buf), "m");
	return buf;
}
