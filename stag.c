#include <stdio.h>
#include <string.h>
#include "regex.h"

#define LEN(a)	(sizeof(a) / sizeof((a)[0]))
#define TAGRE	"^(func|var|const|type)( +\\([^()]+\\))? +([a-zA-Z_0-9]+)\\>"
#define TAGGRP	3

static struct tag {
	char *ext;	/* file extension */
	char *pat;	/* tag pattern */
	int grp;	/* tag group */
} tags[] = {
	{".go", "^(func|var|const|type)( +\\([^()]+\\))? +([a-zA-Z_0-9]+)\\>", 3},
};

static int tags_find(char *path)
{
	int i;
	for (i = 0; i < LEN(tags); i++) {
		int len = strlen(tags[i].ext);
		if (strlen(path) > len && !strcmp(strchr(path, '\0') - len, tags[i].ext))
			return i;
	}
	return -1;
}

static int mktags(char *path, regex_t *re, int grp)
{
	char ln[128];
	char tag[120];
	int lnum = 0;
	regmatch_t grps[32];
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return 1;
	while (fgets(ln, sizeof(ln), fp) != NULL) {
		if (regexec(re, ln, LEN(grps), grps, 0) == 0) {
			int len = grps[grp].rm_eo - grps[grp].rm_so;
			if (len + 1 > sizeof(tag))
				len = sizeof(tag) - 1;
			memcpy(tag, ln + grps[grp].rm_so, len);
			tag[len] = '\0';
			printf("%s\t%s\t%d\n", tag, path, lnum + 1);
		}
		lnum++;
	}
	fclose(fp);
	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	if (argc == 1) {
		printf("usage: %s files >tags\n", argv[0]);
		return 0;
	}
	for (i = 1; i < argc; i++) {
		int idx = tags_find(argv[i]);
		regex_t re;
		if (idx < 0) {
			fprintf(stderr, "mktags: no pattern for %s\n", argv[i]);
			continue;
		}
		if (regcomp(&re, tags[idx].pat, REG_EXTENDED) != 0) {
			fprintf(stderr, "mktags: bad pattern %s\n", tags[idx].pat);
			continue;
		}
		if (mktags(argv[i], &re, tags[idx].grp))
			fprintf(stderr, "mktags: failed to read %s\n", argv[i]);
		regfree(&re);
	}
	return 0;
}
