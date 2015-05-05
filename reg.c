#include <stdlib.h>
#include <string.h>
#include "vi.h"

static char *reg;
static int lnmode;

char *reg_get(int c, int *ln)
{
	*ln = lnmode;
	return reg;
}

void reg_put(int c, char *s, int ln)
{
	char *nreg = malloc(strlen(s) + 1);
	strcpy(nreg, s);
	free(reg);
	reg = nreg;
	lnmode = ln;
}

void reg_done(void)
{
	free(reg);
}
