#include <poll.h>
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
	newtermios.c_lflag &= ~ICANON;
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &newtermios);
	if (getenv("LINES"))
		rows = atoi(getenv("LINES"));
	if (getenv("COLUMNS"))
		cols = atoi(getenv("COLUMNS"));
	if (!ioctl(0, TIOCGWINSZ, &win)) {
		cols = cols ? cols : win.ws_col;
		rows = rows ? rows : win.ws_row;
	}
	cols = cols ? cols : 80;
	rows = rows ? rows : 25;
}

void term_done(void)
{
	term_commit();
	tcsetattr(0, 0, &termios);
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

void term_pos(int r, int c)
{
	char buf[32] = "\r";
	if (c < 0)
		c = 0;
	if (c >= xcols)
		c = cols - 1;
	if (r < 0)
		sprintf(buf, "\r\33[%d%c", abs(c), c > 0 ? 'C' : 'D');
	else
		sprintf(buf, "\33[%d;%dH", r + 1, c + 1);
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

int term_read(int ms)
{
	struct pollfd ufds[1];
	char b;
	ufds[0].fd = 0;
	ufds[0].events = POLLIN;
	if (poll(ufds, 1, ms * 1000) <= 0)
		return -1;
	if (read(0, &b, 1) <= 0)
		return -1;
	return (unsigned char) b;
}
