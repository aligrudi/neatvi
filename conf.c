#include <ctype.h>
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

static int conf_hldefs[384] = {
	/* general text */
	['.'] = 0,		/* all text */
	['^'] = SYN_BGMK(11),	/* current line highlight (hll option) */
	['~'] = SYN_RV,		/* text in reverse direction */
	/* for programming languages  */
	['k'] = SYN_BD | 3,	/* general keywords */
	['r'] = 2,		/* control flow keywords */
	['o'] = SYN_BD | 3,	/* operators */
	['p'] = SYN_BD | 2,	/* preprocessor directives */
	['n'] = SYN_BD | 4,	/* include directives */
	['m'] = SYN_BD | 4,	/* imported packages */
	['t'] = 3,		/* built-in types and values */
	['b'] = 3,		/* built-in functions */
	['c'] = SYN_IT | 4,	/* comments */
	['d'] = SYN_BD | 4,	/* top-level definitions */
	['f'] = SYN_BD,		/* called functions */
	['0'] = 5,		/* numerical constants */
	['h'] = 5,		/* character constants */
	['s'] = 5,		/* string literals */
	['v'] = 3,		/* macros */
	['i'] = 0,		/* identifiers */
	/* ex-mode and status line */
	['_'] = SYN_BGMK(0) | 7 | SYN_BD,	/* status line */
	['Z'] = SYN_BGMK(0) | 7 | SYN_BD,	/* status line of inactive windows */
	['F'] = 2,		/* status line file name */
	['N'] = 5,		/* status line line number */
	['W'] = 1,		/* status line w/r flags */
	['C'] = 0,		/* status line column number */
	['T'] = 0,		/* status line line count */
	['M'] = 4 | SYN_BD,	/* status line mode */
	['X'] = SYN_BGMK(0) | 7 | SYN_BD,	/* ex-mode messages */
	[':'] = SYN_BGMK(0) | 7,		/* ex-mode prompts */
	['!'] = SYN_BGMK(0) | 7 | SYN_BD,	/* ex-mode warnings */
	['Q'] = SYN_BGMK(0) | 2 | SYN_BD,	/* quick leap header */
	['D'] = 0,		/* quick leap path dirname */
	['B'] = SYN_BD,		/* quick leap path basename */
	['L'] = SYN_BGMK(7) | SYN_FGMK(0),	/* quick leap list */
	['I'] = 1 | SYN_BD,	/* quick leap identifiers */
	/* diff file type */
	[SX('D')] = SYN_BD,		/* diff header */
	[SX('@')] = 6,		/* diff hunk information line */
	[SX('-')] = 1,		/* diff deleted lines  */
	[SX('+')] = 2,		/* diff added lines */
	[SX('=')] = 0,		/* diff preserved lines */
	/* file listings */
	[SX('d')] = 8 | SYN_BD,	/* path dirname */
	[SX('b')] = 1 | SYN_BD,	/* path basename */
	[SX('n')] = 2,		/* path line number */
	[SX('s')] = 6,		/* path symbol name */
	[SX('l')] = 8,		/* the rest of the line */
	[SX('c')] = 4,		/* comments */
	/* messages */
	[SM('h')] = 6 | SYN_BD,	/* mail headers */
	[SM('s')] = 4 | SYN_BD,	/* subject */
	[SM('f')] = 4 | SYN_BD,	/* from */
	[SM('t')] = 5 | SYN_BD,	/* to */
	[SM('c')] = 5 | SYN_BD,	/* cc */
	[SM('r')] = 2 | SYN_IT,	/* replied text */
	[SM('l')] = SYN_BGMK(3) | SYN_BD,	/* label */
	/* neatmail listing */
	[SM('N')] = SYN_BD | SYN_FGMK(0) | SYN_BGMK(6),	/* new mails */
	[SM('A')] = SYN_BD | SYN_FGMK(0) | SYN_BGMK(5),	/* high priority */
	[SM('B')] = SYN_BD | SYN_FGMK(0) | SYN_BGMK(3),	/* priority level 2 */
	[SM('C')] = SYN_BD | SYN_FGMK(0) | SYN_BGMK(3),	/* priority level 3 */
	[SM('D')] = 8,		/* low priority */
	[SM('F')] = SYN_BD | SYN_BGMK(7),	/* flagged */
	[SM('I')] = 7 | SYN_IT,	/* ignored lines */
	[SM('X')] = SYN_BD,	/* commands */
	[SM('S')] = 6 | SYN_BD,	/* status flags */
	[SM('0')] = 12,		/* ignored zeros of message numbers */
	[SM('1')] = 4 | SYN_BD,	/* message numbers */
	[SM('M')] = 3,		/* mbox paths */
	[SM('R')] = 5,		/* message from */
	[SM('U')] = 5,		/* message subject */
};

int conf_hlnum(char *name)
{
	if (name[0] == 'x' && isprint((unsigned char) name[1]))
		return SX(name[1]);
	if (name[0] == 'm' && isprint((unsigned char) name[1]))
		return SM(name[1]);
	return name[0];
}

int conf_hl(int id)
{
	return id >= 0 && id < LEN(conf_hldefs) ? conf_hldefs[id] : 0;
}

void conf_hlset(int id, int att)
{
	if (id >= 0 && id < LEN(conf_hldefs))
		conf_hldefs[id] = att;
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
	return conf_hl('~');
}

int conf_hlline(void)
{
	return conf_hl('^');
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
