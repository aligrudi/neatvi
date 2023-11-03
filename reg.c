#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vi.h"

static char *bufs[256];
static int lnmode[256];

char *reg_get(int c, int *ln)
{
	*ln = lnmode[c];
	return bufs[c];
}

static void reg_putraw(int c, char *s, int ln)
{
	unsigned char c_lower = tolower(c);
	char *pre = isupper(c) && bufs[c_lower] ? bufs[c_lower] : "";
	size_t size = strlen(pre) + strlen(s) + 1;
	char *buf = malloc(size);
	snprintf(buf, size, "%s%s", pre, s);
	free(bufs[c_lower]);
	bufs[c_lower] = buf;
	lnmode[c_lower] = ln;
}

void reg_put(int c, char *s, int ln)
{
	int i, i_ln;
	char *i_s;
	if (ln || strchr(s, '\n')) {
		for (i = 8; i > 0; i--)
			if ((i_s = reg_get('0' + i, &i_ln)))
				reg_putraw('0' + i + 1, i_s, i_ln);
		reg_putraw('1', s, ln);
	}
	reg_putraw(c, s, ln);
}

void reg_done(void)
{
	int i;
	for (i = 0; i < LEN(bufs); i++)
		free(bufs[i]);
}
