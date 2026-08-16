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
	long s_sz;		/* size of memory allocated for s[] */
	long s_n;		/* length of the string stored in s[] */
};

static int sbuf_extend(struct sbuf *sbuf, long newsz)
{
	char *s;
	if (!(s = malloc(newsz)))
		return 1;
	if (sbuf->s_n)
		memcpy(s, sbuf->s, sbuf->s_n);
	free(sbuf->s);
	sbuf->s_sz = newsz;
	sbuf->s = s;
	return 0;
}

struct sbuf *sbuf_make(void)
{
	struct sbuf *sb = malloc(sizeof(*sb));
	memset(sb, 0, sizeof(*sb));
	return sb;
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
	char *s = sbuf_buf(sb);
	free(sb);
	return s;
}

void sbuf_free(struct sbuf *sb)
{
	free(sb->s);
	free(sb);
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

int fbuf_chr(struct fbuf *fb, int c)
{
	if (fb->s_n >= fb->s_sz)
		return 1;
	fb->s[fb->s_n++] = c;
	return 0;
}

int fbuf_mem(struct fbuf *fb, void *s, long len)
{
	long cp = MIN(len, fb->s_sz - fb->s_n);
	memcpy(fb->s + fb->s_n, s, cp);
	fb->s_n += cp;
	return cp < len;
}

int fbuf_str(struct fbuf *fb, char *s)
{
	return fbuf_mem(fb, s, strlen(s));
}

char *fbuf_buf(struct fbuf *fb)
{
	if (fb->s_n < fb->s_sz)
		fb->s[fb->s_n] = '\0';
	return fb->s_n < fb->s_sz ? fb->s : NULL;
}

long fbuf_len(struct fbuf *fb)
{
	return fb->s_n;
}
