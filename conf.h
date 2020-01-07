/* neatvi configuration file */

/* map file names to file types */
static struct filetype {
	char *ft;		/* file type */
	char *pat;		/* file name pattern */
} filetypes[] = {
	{"c", "\\.[hc]$"},				/* C */
	{"roff", "\\.(ms|tr|roff|tmac|txt|[1-9])$"},	/* troff */
	{"tex", "\\.tex$"},				/* tex */
	{"msg", "letter$|mbox$|mail$"},			/* email */
	{"mk", "Makefile$|makefile$|\\.mk$"},		/* makefile */
	{"sh", "\\.sh$"},				/* shell script */
	{"nm", "\\.nm$"},				/* neatmail */
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
	{"c", {2 | SYN_IT}, "/\\*([^*]|\\*+[^*/])*\\*+/"},
	{"c", {6}, "^#[ \t]*[a-zA-Z0-9_]+"},
	{"c", {0, SYN_BD}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"c", {4}, "\"([^\"]|\\\\\")*\""},
	{"c", {4}, "'([^\\]|\\\\.)'"},
	{"c", {4}, "[-+]?\\<(0[xX][0-9a-fA-F]+|[0-9]+)\\>"},

	{"roff", {4, 0, 5 | SYN_BD, 4 | SYN_BD, 5 | SYN_BD, 4 | SYN_BD},
		"^[.'][ \t]*((SH.*)|(de) (.*)|([^ \t\\]{2,}))?.*$", 1},
	{"roff", {2 | SYN_IT}, "\\\\\".*$"},
	{"roff", {3}, "\\\\{1,2}[*$fgkmns]([^[(]|\\(..|\\[[^]]*\\])"},
	{"roff", {3}, "\\\\([^[(*$fgkmns]|\\(..|\\[[^]]*\\])"},
	{"roff", {3}, "\\$[^$]+\\$"},

	{"tex", {4 | SYN_BD, 0, 3, 0, 5},
		"\\\\[^[{ \t]+(\\[([^]]+)\\])?(\\{([^}]*)\\})?"},
	{"tex", {3}, "\\$[^$]+\\$"},
	{"tex", {2 | SYN_IT}, "%.*$"},

	/* mail */
	{"msg", {6 | SYN_BD}, "^From .*20..$"},
	{"msg", {6 | SYN_BD, 4 | SYN_BD}, "^Subject: (.*)$"},
	{"msg", {6 | SYN_BD, 2 | SYN_BD}, "^From: (.*)$"},
	{"msg", {6 | SYN_BD, 5 | SYN_BD}, "^To: (.*)$"},
	{"msg", {6 | SYN_BD, 5 | SYN_BD}, "^Cc: (.*)$"},
	{"msg", {6 | SYN_BD}, "^[A-Z][-A-Za-z]+: .+$"},
	{"msg", {2 | SYN_IT}, "^> .*$"},

	/* makefile */
	{"mk", {0, 3}, "([A-Za-z_][A-Za-z0-9_]*)[ \t]*="},
	{"mk", {3}, "\\$\\([a-zA-Z0-9_]+\\)"},
	{"mk", {2 | SYN_IT}, "#.*$"},
	{"mk", {0, SYN_BD}, "([A-Za-z_%.]+):"},

	/* shell script */
	{"sh", {2 | SYN_IT}, "#.*$"},
	{"sh", {4}, "\"([^\"]|\\\\\")*\""},
	{"sh", {4}, "\'[^\']*\'"},

	/* neatmail */
	{"nm", {0, 12 | SYN_BD, 12 | SYN_BD, 2, 8 | SYN_BD},
		"^([ROU])([0-9]+)\t([^\t]*)\t([^\t]*)"},
	{"nm", {7}, "^[LJ].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(13)}, "^[HT].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(11)}, "^[MI].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(12)}, "^[N].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(10)}, "^[F].*$"},
	{"nm", {7 | SYN_IT}, "^\t.*$"},
	{"nm", {SYN_BD}, "^:.*$"},

	/* status bar */
	{"---", {8 | SYN_BD, 4, 1}, "^(\".*\").*(\\[[wr]\\]).*$"},
	{"---", {8 | SYN_BD, 4, 4}, "^(\".*\").*(L[0-9]+) +(C[0-9]+).*$"},
	{"---", {8 | SYN_BD}, "^.*$"},
};

/* how to hightlight text in the reverse direction */
#define SYN_REVDIR		(SYN_BGMK(255))

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
	{"\342\200\214", "^-", 2},
	{"\342\200\215", "^-", 2},
	{"\x00", "^@", 2},
	{"\x01", "^A", 2},
	{"\x02", "^B", 2},
	{"\x03", "^C", 2},
	{"\x04", "^D", 2},
	{"\x05", "^E", 2},
	{"\x06", "^F", 2},
	{"\x07", "^G", 2},
	{"\x08", "^H", 2},
	/*{"\x09", "^I", 2},*/
	/*{"\x0A", "$", 1},*/
	{"\x0B", "^K", 2},
	{"\x0C", "^L", 2},
	{"\x0D", "^M", 2},
	{"\x0E", "^N", 2},
	{"\x0F", "^O", 2},
	{"\x10", "^P", 2},
	{"\x11", "^Q", 2},
	{"\x12", "^R", 2},
	{"\x13", "^S", 2},
	{"\x14", "^T", 2},
	{"\x15", "^U", 2},
	{"\x16", "^V", 2},
	{"\x17", "^W", 2},
	{"\x18", "^X", 2},
	{"\x19", "^Y", 2},
	{"\x1A", "^Z", 2},
	{"\x1B", "^[", 2},
	{"\x1C", "^\\", 2},
	{"\x1D", "^]", 2},
	{"\x1E", "^^", 2},
	{"\x1F", "^_", 2},
	{"\x7F", "^?", 2},
};
