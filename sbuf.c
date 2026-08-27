/* variable length string buffer */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

#define SBUFSZ		128
#define ALIGN(n, a)	(((n) + (a) - 1) & ~((a) - 1))
#define NEXTSZ(o, r)	ALIGN(MAX((o) * 2, (o) + (r)), SBUFSZ)

static int sbuf_extend(struct sbuf *sbuf, long newsz)
{
	char *s;
	if (sbuf->s && !sbuf->owns)
		return 1;
	if (!(s = malloc(newsz)))
		return 1;
	if (sbuf->s_n)
		memcpy(s, sbuf->s, sbuf->s_n);
	free(sbuf->s);
	sbuf->s_sz = newsz;
	sbuf->s = s;
	sbuf->owns = 1;
	return 0;
}

char *sbuf_buf(struct sbuf *sb)
{
	if (!sb->s && sbuf_extend(sb, 1))
		return NULL;
	sb->s[sb->s_n] = '\0';
	return sb->s;
}

char *sbuf_done(struct sbuf *sb)
{
	char *res = sbuf_buf(sb);
	memset(sb, 0, sizeof(*sb));
	return res;
}

void sbuf_free(struct sbuf *sb)
{
	if (sb->owns)
		free(sb->s);
	memset(sb, 0, sizeof(*sb));
}

int sbuf_chr(struct sbuf *sbuf, int c)
{
	if (sbuf->s_n + 2 >= sbuf->s_sz) {
		if (sbuf_extend(sbuf, NEXTSZ(sbuf->s_sz, 1)))
			return 1;
	}
	sbuf->s[sbuf->s_n++] = c;
	return 0;
}

int sbuf_mem(struct sbuf *sbuf, void *s, long len)
{
	if (sbuf->s_n + len + 1 >= sbuf->s_sz) {
		if (sbuf_extend(sbuf, NEXTSZ(sbuf->s_sz, len + 1)))
			return 1;
	}
	memcpy(sbuf->s + sbuf->s_n, s, len);
	sbuf->s_n += len;
	return 0;
}

int sbuf_str(struct sbuf *sbuf, char *s)
{
	return sbuf_mem(sbuf, s, strlen(s));
}

long sbuf_len(struct sbuf *sbuf)
{
	return sbuf->s_n;
}

void sbuf_cut(struct sbuf *sb, long len)
{
	if (sb->s_n > len)
		sb->s_n = len;
}

int sbuf_printf(struct sbuf *sbuf, char *s, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, s);
	vsnprintf(buf, sizeof(buf), s, ap);
	va_end(ap);
	return sbuf_str(sbuf, buf);
}
