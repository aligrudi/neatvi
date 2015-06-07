/* neatvi configuration file */

/* the alternate keymap (^F and ^E in insert mode to switch) */
#define KMAPALT		"fa"

/* right-to-left characters (used only in dircontexts[] and dirmarks[]) */
#define CR2L		"ءآأؤإئابةتثجحخدذرزسشصضطظعغـفقكلمنهوىييپچژکگی‌‍؛،»«؟ًٌٍَُِّْ"
/* neutral characters (used only in dircontexts[] and dirmarks[]) */
#define CNEUT		"-!\"#$%&'()*+,./:;<=>?@^_`{|}~ "

/* direction context patterns; specifies the direction of a whole line */
static struct dircontext {
	int dir;
	char *pat;
} dircontexts[] = {
	{-1, "^[" CR2L "]"},
	{+1, "^[a-zA-Z_0-9]"},
};

/* direction marks; the direction of a few words in a line */
static struct dirmark {
	int ctx;	/* the direction context for this mark; 0 means any */
	int dir;	/* the direction of the matched text */
	int grp;	/* the nested subgroup; 0 means no groups */
	char *pat;
} dirmarks[] = {
	{+0, +1, 1, "\\\\\\*\\[([^]]+)\\]"},
	{+1, -1, 0, "[" CR2L "][" CNEUT CR2L "]*[" CR2L "]"},
	{-1, +1, 0, "[a-zA-Z0-9_][^" CR2L "\\\\`$']*[a-zA-Z0-9_]"},
	{+0, +1, 0, "\\$([^$]+)\\$"},
	{+0, +1, 1, "\\\\[a-zA-Z0-9_]+\\{([^}]+)\\}"},
	{-1, +1, 0, "\\\\[^ \t" CR2L "]+"},
};

/* character placeholders */
static struct placeholder {
	char *s;	/* the source character */
	char *d;	/* the placeholder */
	int wid;	/* the width of the placeholder */
} placeholders[] = {
	{"‌", "-", 1},
	{"‍", "-", 1},
	{"ْ", "ـْ", 1},
	{"ٌ", "ـٌ", 1},
	{"ٍ", "ـٍ", 1},
	{"ً", "ـً", 1},
	{"ُ", "ـُ", 1},
	{"ِ", "ـِ", 1},
	{"َ", "ـَ", 1},
	{"ّ", "ـّ", 1},
};

/* map file names to file types */
static struct filetype {
	char *ft;		/* file type */
	char *pat;		/* file name pattern */
} filetypes[] = {
	{"c", "\\.[hc]$"},
	{"tr", "\\.(ms|tr|roff|tmac)$"},
};

/* syntax highlighting patterns */
static struct highlight {
	char *ft;		/* the filetype of this pattern */
	int att[16];		/* attributes of the matched groups */
	char *pat;		/* regular expression */
	int end;		/* the group ending this pattern */
} highlights[] = {
	{"c", {5}, "\\<(signed|unsigned|char|short|int|long|float|double|void|struct|enum|union|typedef)\\>"},
	{"c", {5}, "\\<(static|extern|register)\\>"},
	{"c", {4}, "\\<(return|for|while|if|else|do|sizeof|goto|switch|case|default|break|continue)\\>"},
	{"c", {2 | SYN_IT}, "//.*$"},
	{"c", {2 | SYN_IT}, "/\\*([^*]|\\*[^/])*\\*/"},
	{"c", {6}, "^#[ \t]*[a-zA-Z0-9_]+"},
	{"c", {0, SYN_BD}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"c", {4}, "\"([^\"]|\\\\\")*\""},
	{"c", {4}, "'([^\\]|\\\\.)'"},

	{"tr", {4, 0, 5 | SYN_BD, 4 | SYN_BD, 5 | SYN_BD, 4 | SYN_BD},
		"^[.'][ \t]*((SH.*)|(de) (.*)|([^ \t\\]{2,}))?.*$", 1},
	{"tr", {2 | SYN_IT}, "\\\\\".*$"},
	{"tr", {3}, "\\\\{1,2}[*$fgkmns]([^[(]|\\(..|\\[[^]]*\\])"},
	{"tr", {3}, "\\\\([^[(*$fgkmns]|\\(..|\\[[^]]*\\])"},
	{"tr", {3}, "\\$[^$]+\\$"},
};

/* how to hightlight text in the reverse direction */
#define SYN_REVDIR		(SYN_BGMK(7))
