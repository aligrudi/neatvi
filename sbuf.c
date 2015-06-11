/* variable length string buffer */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

#define SBUFSZ		128
#define ALIGN(n, a)	(((n) + (a) - 1) & ~((a) - 1))
#define NEXTSZ(o, r)	ALIGN(MAX((o) * 2, (o) + (r)), SBUFSZ)

struct sbuf {
	char *s;		/* allocated buffer */
	int s_n;		/* length of the string stored in s[] */
	int s_sz;		/* size of memory allocated for s[] */
};

static void sbuf_extend(struct sbuf *sbuf, int newsz)
{
	char *s = sbuf->s;
	sbuf->s_sz = newsz;
	sbuf->s = malloc(sbuf->s_sz);
	if (sbuf->s_n)
		memcpy(sbuf->s, s, sbuf->s_n);
	free(s);
}

struct sbuf *sbuf_make(void)
{
	struct sbuf *sb = malloc(sizeof(*sb));
	memset(sb, 0, sizeof(*sb));
	return sb;
}

char *sbuf_buf(struct sbuf *sb)
{
	if (!sb->s)
		sbuf_extend(sb, 1);
	sb->s[sb->s_n] = '\0';
	return sb->s;
}

char *sbuf_done(struct sbuf *sb)
{
	char *s = sbuf_buf(sb);
	free(sb);
	return s;
}

void sbuf_free(struct sbuf *sb)
{
	free(sb->s);
	free(sb);
}

void sbuf_chr(struct sbuf *sbuf, int c)
{
	if (sbuf->s_n + 2 >= sbuf->s_sz)
		sbuf_extend(sbuf, NEXTSZ(sbuf->s_sz, 1));
	sbuf->s[sbuf->s_n++] = c;
}

void sbuf_mem(struct sbuf *sbuf, char *s, int len)
{
	if (sbuf->s_n + len + 1 >= sbuf->s_sz)
		sbuf_extend(sbuf, NEXTSZ(sbuf->s_sz, len + 1));
	memcpy(sbuf->s + sbuf->s_n, s, len);
	sbuf->s_n += len;
}

void sbuf_str(struct sbuf *sbuf, char *s)
{
	sbuf_mem(sbuf, s, strlen(s));
}

int sbuf_len(struct sbuf *sbuf)
{
	return sbuf->s_n;
}

void sbuf_cut(struct sbuf *sb, int len)
{
	if (sb->s_n > len)
		sb->s_n = len;
}

void sbuf_printf(struct sbuf *sbuf, char *s, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, s);
	vsnprintf(buf, sizeof(buf), s, ap);
	va_end(ap);
	sbuf_str(sbuf, buf);
}
