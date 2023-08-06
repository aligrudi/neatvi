#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vi.h"

static char *tag;

int tag_init(char *path)
{
	char buf[1 << 10];
	struct sbuf *sb;
	long nr;
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return 1;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	close(fd);
	tag = sbuf_done(sb);
	return 0;
}

int tag_set(void)
{
	return tag != NULL;
}

static char *copypart(char *dst, int dstlen, char *src)
{
	char *end = src;
	int len = dstlen - 1;
	while (*end && *end != '\t' && *end != '\n')
		end++;
	if (end - src < len)
		len = end - src;
	memcpy(dst, src, len);
	dst[len] = '\0';
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
	char *s = tag + *pos;
	int taglen = strlen(name);
	if (dir != 0)
		s = tag_next(s, dir);
	while (s) {
		if (!strncmp(name, s, taglen) && s[taglen] == '\t') {
			char *r = copypart(path, pathlen, s + taglen + 1);
			copypart(cmd, cmdlen, r);
			*pos = s - tag;
			return 0;
		}
		s = tag_next(s, dir);
	}
	return 1;
}

void tag_done(void)
{
	free(tag);
}
