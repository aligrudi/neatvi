#include <stdio.h>
#include "vi.h"
#include "conf.h"

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

int conf_highlight(int idx, char **ft, int **att, char **pat, int *end)
{
	if (idx < 0 || idx >= LEN(highlights))
		return 1;
	if (ft)
		*ft = highlights[idx].ft;
	if (att)
		*att = highlights[idx].att;
	if (pat)
		*pat = highlights[idx].pat;
	if (end)
		*end = highlights[idx].end;
	return 0;
}

int conf_filetype(int idx, char **ft, char **pat)
{
	if (idx < 0 || idx >= LEN(filetypes))
		return 1;
	if (ft)
		*ft = filetypes[idx].ft;
	if (pat)
		*pat = filetypes[idx].pat;
	return 0;
}

int conf_highlight_revdir(int *att)
{
	*att = SYN_REVDIR;
	return 0;
}
