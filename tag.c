#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vi.h"

static char *tagpath;
static char *tag;
static long taglen;

int tag_init(void)
{
	return 0;
}

static int tag_load(void)
{
	char buf[1 << 10];
	struct sbuf *sb;
	long nr;
	int fd;
	if (tagpath != NULL)
		return tag == NULL;
	tagpath = getenv("TAGPATH") ? getenv("TAGPATH") : "tags";
	if ((fd = open(tagpath, O_RDONLY)) < 0)
		return 1;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	close(fd);
	taglen = sbuf_len(sb);
	tag = sbuf_done(sb);
	return 0;
}

void tag_done(void)
{
	free(tag);
	tag = NULL;
	tagpath = NULL;
}

static char *copypart(char *dst, int dstlen, char *src)
{
	char *end = src;
	int len = dstlen - 1;
	while (*end && *end != '\t' && *end != '\n')
		end++;
	if (end - src < len)
		len = end - src;
	if (dst != NULL && dstlen > 0) {
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return *end ? end + 1 : end;
}

static char *tag_next(char *s, int dir)
{
	if (dir >= 0 && *s) {
		if ((s = strchr(s + 1, '\n')) != NULL)
			return s + 1;
	}
	if (dir < 0 && s > tag) {
		s--;
		while (s > tag && s[-1] != '\n')
			s--;
		return s;
	}
	return NULL;
}

int tag_find(char *name, int *pos, int dir, char *path, int pathlen, char *cmd, int cmdlen)
{
	char *s;
	int len = strlen(name);
	if (tag_load() != 0)
		return 1;
	if (*pos >= taglen)
		*pos = 0;
	s = dir != 0 ? tag_next(tag + *pos, dir) : tag + *pos;
	while (s) {
		if (!strncmp(name, s, len) && s[len] == '\t') {
			char *r = copypart(path, pathlen, s + len + 1);
			copypart(cmd, cmdlen, r);
			*pos = s - tag;
			return 0;
		}
		s = tag_next(s, dir);
	}
	return 1;
}

struct tlist {
	char **ls;
	char *mark;
	char *raw;
	int ls_n;
	int ls_sz;
};

struct tlist *tlist_make(char *ls[], int ls_n)
{
	struct tlist *tls = malloc(sizeof(*tls));
	memset(tls, 0, sizeof(*tls));
	tls->ls = ls;
	tls->ls_n = ls_n;
	tls->ls_sz = 0;
	return tls;
}

static int tlist_put(struct tlist *tls, char *item)
{
	if (tls->ls_n >= tls->ls_sz) {
		char **new;
		int ls_sz = tls->ls_sz + 256;
		if (!(new = malloc(ls_sz * sizeof(tls->ls[0]))))
			return 1;
		memcpy(new, tls->ls, tls->ls_n * sizeof(tls->ls[0]));
		free(tls->ls);
		tls->ls_sz = ls_sz;
		tls->ls = new;
	}
	tls->ls[tls->ls_n++] = item;
	return 0;
}

struct tlist *tlist_from(char *path)
{
	struct tlist *tls;
	char buf[1024];
	long nr;
	char *s;
	struct sbuf *sb;
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return NULL;
	tls = tlist_make(NULL, 0);
	sb = sbuf_make();
	while (sbuf_len(sb) < (1 << 20) && (nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	close(fd);
	tls->raw = sbuf_done(sb);
	for (s = tls->raw; s && *s; s++) {
		char *r = strchr(s, '\n');
		if (r && s[0] != '#' && !isspace((unsigned char) s[0]))
			tlist_put(tls, s);
		if (r)
			*r = '\0';
		s = r;
	}
	return tls;
}

struct tlist *tlist_tags(char *path)
{
	struct tlist *tls;
	char buf[1024];
	long nr;
	char *s;
	struct sbuf *sb;
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return NULL;
	tls = tlist_make(NULL, 0);
	sb = sbuf_make();
	while (sbuf_len(sb) < (1 << 20) && (nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	close(fd);
	tls->raw = sbuf_done(sb);
	for (s = tls->raw; s && *s; s++) {
		char *r = strchr(s, '\n');
		char *t1 = s ? memchr(s, '\t', r - s) : NULL;
		char *t2 = t1 ? memchr(t1 + 1, '\t', r - t1 - 1) : NULL;
		if (r && s[0] != '#' && !isspace((unsigned char) s[0]))
			tlist_put(tls, s);
		if (t2)
			*t2 = '\0';
		if (r)
			*r = '\0';
		s = r;
	}
	return tls;
}

void tlist_free(struct tlist *tls)
{
	if (tls->ls_sz)
		free(tls->ls);
	free(tls->raw);
	free(tls->mark);
	free(tls);
}

void tlist_filt(struct tlist *tls, char *kw)
{
	int i;
	if (!tls->mark) {
		if (!(tls->mark = malloc(tls->ls_n * sizeof(tls->mark[0]))))
			return;
		memset(tls->mark, 1, tls->ls_n * sizeof(tls->mark[0]));
	}
	if (!kw)
		memset(tls->mark, 1, tls->ls_n * sizeof(tls->mark[0]));
	for (i = 0; i < tls->ls_n; i++)
		if (kw && tls->mark[i] && !strstr(tls->ls[i], kw))
			tls->mark[i] = 0;
}

int tlist_cnt(struct tlist *tls)
{
	return tls->ls_n;
}

int tlist_matches(struct tlist *tls)
{
	int i, cnt = 0;
	for (i = 0; i < tls->ls_n; i++)
		if (!tls->mark || tls->mark[i])
			cnt++;
	return cnt;
}

char *tlist_get(struct tlist *tls, int idx)
{
	return tls->ls[idx];
}

int tlist_top(struct tlist *tls, int *view, int view_sz)
{
	int view_n = 0;
	int i;
	for (i = 0; i < tls->ls_n && view_n < view_sz; i++)
		if (!tls->mark || tls->mark[i])
			view[view_n++] = i;
	return view_n;
}
