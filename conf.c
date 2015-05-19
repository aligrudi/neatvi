#include "conf.h"
#include "vi.h"

char *conf_kmapalt(void)
{
	return KMAPALT;
}

int conf_dirmark(int idx, char **pat, int *ctx, int *dir, int *grp)
{
	if (idx < 0 || idx >= LEN(dirmarks))
		return 1;
	if (pat)
		*pat = dirmarks[idx].pat;
	if (ctx)
		*ctx = dirmarks[idx].ctx;
	if (dir)
		*dir = dirmarks[idx].dir;
	if (grp)
		*grp = dirmarks[idx].grp;
	return 0;
}

int conf_dircontext(int idx, char **pat, int *ctx)
{
	if (idx < 0 || idx >= LEN(dircontexts))
		return 1;
	if (pat)
		*pat = dircontexts[idx].pat;
	if (ctx)
		*ctx = dircontexts[idx].dir;
	return 0;
}

int conf_placeholder(int idx, char **s, char **d, int *wid)
{
	if (idx < 0 || idx >= LEN(placeholders))
		return 1;
	if (s)
		*s = placeholders[idx].s;
	if (d)
		*d = placeholders[idx].d;
	if (wid)
		*wid = placeholders[idx].wid;
	return 0;
}
