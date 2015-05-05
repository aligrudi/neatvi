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

void reg_put(int c, char *s, int ln)
{
	char *buf = malloc(strlen(s) + 1);
	strcpy(buf, s);
	free(bufs[c]);
	bufs[c] = buf;
	lnmode[c] = ln;
}

void reg_done(void)
{
	int i;
	for (i = 0; i < LEN(bufs); i++)
		free(bufs[i]);
}
