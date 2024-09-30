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
