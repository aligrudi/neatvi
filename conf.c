#include <stdio.h>
#include <string.h>
#include "vi.h"
#include "conf.h"
#include "kmap.h"

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

int conf_hlrev(void)
{
	return SYN_REVDIR;
}

int conf_hlline(void)
{
	return SYN_LINE;
}

int conf_mode(void)
{
	return MKFILE_MODE;
}

char **conf_kmap(int id)
{
	return kmaps[id];
}

int conf_kmapfind(char *name)
{
	int i;
	for (i = 0; i < LEN(kmaps); i++)
		if (name && kmaps[i][0] && !strcmp(name, kmaps[i][0]))
			return i;
	return 0;
}

char *conf_digraph(int c1, int c2)
{
	int i;
	for (i = 0; i < LEN(digraphs); i++)
		if (digraphs[i][0][0] == c1 && digraphs[i][0][1] == c2)
			return digraphs[i][1];
	return NULL;
}

char *conf_lnpref(void)
{
	return LNPREF;
}

char *conf_definition(char *ft)
{
	int i;
	for (i = 0; i < LEN(filetypes); i++)
		if (!strcmp(ft, filetypes[i].ft) && filetypes[i].def)
			return filetypes[i].def;
	return "^%s\\>";
}

char *conf_section(char *ft)
{
	int i;
	for (i = 0; i < LEN(filetypes); i++)
		if (!strcmp(ft, filetypes[i].ft) && filetypes[i].sec)
			return filetypes[i].sec;
	return "^\\{";
}

char *conf_ecmd(void)
{
	return ECMD;
}
