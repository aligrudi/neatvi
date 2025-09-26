/* neatvi configuration file */

/* access mode of new files */
#define MKFILE_MODE	0600

/* map file names to file types */
static struct filetype {
	char *ft;		/* file type */
	char *pat;		/* file name pattern */
	char *def;		/* pattern for global definitions (for gd command) */
	char *sec;		/* section start pattern (for [[ and ]] commands) */
} filetypes[] = {
	{"c", "\\.[hc]$", "^([a-zA-Z_].*)?\\<%s\\>"},
	{"roff", "\\.(ms|me|mom|tr|roff|tmac|[1-9])$", "^\\.(de|nr|ds) +%s\\>"},
	{"tex", "\\.tex$"},
	{"msg", "letter$|mbox$|mail$"},
	{"mk", "Makefile$|makefile$|\\.mk$", "^%s:"},
	{"sh", "\\.sh$", "^(function +)?%s(\\(\\))? *\\{", "^(function +)?[a-zA-Z_0-9]+(\\(\\))? *\\{"},
	{"go", "\\.go$", "^(func|var|const|type)( +\\(.*\\))? +%s\\>", "^(func|type)\\>.*\\{$"},
	{"py", "\\.py$", "^(def|class) +\\<%s\\>", "^(def|class) "},
	{"bib", "bib$"},
	{"nm", "\\.nm$"},
	{"diff", "\\.(patch|diff)$"},
	{"ls", "ls$"},
	{"txt", "$"},		/* matches everything; must be the last pattern */
};

/* colours used in highlights[] for programming languages */
#define CKWD	(3 | SYN_BD)	/* general keywords */
#define CCON	(2)		/* control flow keywords */
#define CPRE	(2 | SYN_BD)	/* preprocessor directives */
#define CIMP	(4 | SYN_BD)	/* imported packages */
#define CTYP	(3)		/* built-in types and values */
#define CBIN	(3)		/* built-in functions */
#define CCMT	(4 | SYN_IT)	/* comments */
#define CDEF	(4 | SYN_BD)	/* top-level definition */
#define CFUN	(SYN_BD)	/* called functions */
#define CNUM	5		/* numerical constants */
#define CSTR	5		/* string literals */
#define CVAR	3		/* macros */
#define CIDN	0		/* identifiers */

/* syntax highlighting patterns */
static struct highlight {
	char *ft;		/* the filetype of this pattern */
	int att[16];		/* attributes of the matched groups */
	char *pat;		/* regular expression */
	int end;		/* the group ending this pattern */
} highlights[] = {
	/* status bar */
	{"---", {SYN_BGMK(0) | 7 | SYN_BD, 2, 1}, "^(\".*\").*(\\[[wr]\\]).*$"},
	{"---", {SYN_BGMK(0) | 7 | SYN_BD, 2, 5, 7}, "^(\".*\").*=.*(L[0-9]+) +(C[0-9]+).*$"},
	{"---", {SYN_BGMK(0) | 7}, "^(\".*\").*-.*(L[0-9]+) +(C[0-9]+).*$"},
	{"---", {SYN_BGMK(7) | SYN_FGMK(0), 1 | SYN_BD, 0, SYN_BD}, "^\\[([0-9])\\](.*/)*([^/]*)$"},
	{"---", {SYN_BGMK(0) | 2 | SYN_BD, 3}, "^QUICK LEAP +\\[([^]]+)\\]$\n?"},
	{"---", {SYN_BGMK(0) | 2 | SYN_BD}, "^.*$\n?"},
	/* ex mode */
	{"-ex", {SYN_BGMK(0) | 7 | SYN_BD}, "^[:/?!].*$"},
	{"-ex", {SYN_BGMK(0) | 7}, "^.*$\n?"},

	/* C */
	{"c", {CTYP}, "\\<(signed|unsigned|char|short|int|long|float|double|void|struct|enum|union|typedef)\\>"},
	{"c", {CKWD}, "\\<(static|extern|register)\\>"},
	{"c", {CCON}, "\\<(return|for|while|if|else|do|sizeof|goto|switch|case|default|break|continue)\\>"},
	{"c", {CCMT}, "//.*$"},
	{"c", {CCMT}, "/\\*([^*]|\\*+[^*/])*\\*+/"},
	{"c", {CIMP, CPRE}, "^#([ \t]*include).*"},
	{"c", {CIMP, CPRE}, "^#([ \t]*[a-zA-Z0-9_]+)"},
	{"c", {0, CFUN}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"c", {CSTR}, "\"([^\"\\]|\\\\.)*\""},
	{"c", {CNUM}, "'([^\\]|\\\\.)'"},
	{"c", {CNUM}, "[-+]?\\<(0[xX][0-9a-fA-F]+|[0-9]+)\\>"},
	{"c", {CCMT}, "^\t*(/\\*.*|\t* \\*.*|\t* \\*\\//)$"},
	{"c", {CIDN}, "[a-zA-Z][a-zA-Z0-9_]*"},

	/* troff */
	{"roff", {0, CKWD, CDEF}, "^[.'][ \t]*(SH)(.*)$"},
	{"roff", {0, CKWD, CDEF}, "^[.'][ \t]*de (.*)$"},
	{"roff", {0, CBIN}, "^[.'][ \t]*([^ \t\\]*)", 1},
	{"roff", {CSTR}, "\"([^\"]|\"\")*\"?"},
	{"roff", {CCMT}, "\\\\\".*$"},
	{"roff", {CBIN}, "\\\\{1,2}[*$fgkmns]([^[(]|\\(..|\\[[^]]*\\])"},
	{"roff", {CVAR}, "\\\\([^[(*$fgkmns]|\\(..|\\[[^]]*\\])"},
	{"roff", {CSTR}, "\\$[^$]+\\$"},
	{"roff", {CNUM}, "[-+]?\\<([0-9]+(\.[0-9]+)?)\\>"},

	/* tex */
	{"tex", {0, CKWD, 0, 0, CIMP, 0, CFUN},
		"\\\\([^[{ \t]+)((\\[([^][]+)\\])|(\\{([^}]*)\\}))*"},
	{"tex", {CSTR}, "\\$[^$]+\\$"},
	{"tex", {CCMT}, "%.*$"},

	/* mail */
	{"msg", {6 | SYN_BD}, "^From .*20..$"},
	{"msg", {6 | SYN_BD, 4 | SYN_BD}, "^Subject: (.*)$"},
	{"msg", {6 | SYN_BD, 2 | SYN_BD}, "^From: (.*)$"},
	{"msg", {6 | SYN_BD, 5 | SYN_BD}, "^To: (.*)$"},
	{"msg", {6 | SYN_BD, 5 | SYN_BD}, "^Cc: (.*)$"},
	{"msg", {6 | SYN_BD}, "^[A-Z][-A-Za-z]+: .+$"},
	{"msg", {2 | SYN_IT}, "^> .*$"},

	/* makefile */
	{"mk", {0, CDEF}, "([A-Za-z_][A-Za-z0-9_]*)[ \t]*="},
	{"mk", {CVAR}, "\\$\\([a-zA-Z0-9_]+\\)"},
	{"mk", {CCMT}, "#.*$"},
	{"mk", {0, CFUN}, "([A-Za-z_%.]+):"},

	/* shell script */
	{"sh", {0, CKWD, CDEF}, "^(function +)?([a-zA-Z_0-9]+) *(\\(\\))? *\\{"},
	{"sh", {CCON}, "\\<(break|case|continue|do|done|elif|else|esac|fi|for|if|in|then|until|while|return)\\>"},
	{"sh", {CSTR}, "\"([^\"\\]|\\\\.)*\""},
	{"sh", {CSTR}, "'[^']*'"},
	{"sh", {CSTR}, "`([^`\\]|\\\\.)*`"},
	{"sh", {CVAR}, "\\$(\\{[^}]+\\}|[a-zA-Z_0-9]+|[!#$?*@-])"},
	{"sh", {CVAR}, "\\$\\([^()]+\\)"},
	{"sh", {CFUN}, "^\\. .*$"},
	{"sh", {CCMT}, "#.*$"},

	/* go */
	{"go", {0, CKWD, CIDN, CDEF}, "^\\<(func) (\\([^()]+\\) )?([a-zA-Z0-9_]+)\\>"},
	{"go", {CKWD}, "\\<(func|type|var|const|package)\\>"},
	{"go", {CPRE, CIMP}, "^import[ \t]+([^ ]+)"},
	{"go", {CKWD}, "\\<(import|interface|struct)\\>"},
	{"go", {CCON}, "\\<(break|case|chan|continue|default|defer|else|fallthrough|for|go|goto|if|map|range|return|select|switch)\\>"},
	{"go", {0, CBIN}, "\\<(append|copy|delete|len|cap|make|new|complex|real|imag|close|panic|recover|print|println|int|int8|int16|int32|int64|uint|uint8|uint16|uint32|uint64|uintptr|float32|float64|complex128|complex64|bool|byte|rune|string|error)\\>\\("},
	{"go", {CTYP}, "\\<(true|false|iota|nil|int8|int16|int32|int64|int|uint8|uint16|uint32|uint64|uint|uintptr|float32|float64|complex128|complex64|bool|byte|rune|string|error)\\>"},
	{"go", {CCMT}, "//.*$"},
	{"go", {CCMT}, "/\\*([^*]|\\*+[^*/])*\\*+/"},
	{"go", {0, CFUN}, "([a-zA-Z][a-zA-Z0-9_]*)\\(", 1},
	{"go", {CIDN}, "[a-zA-Z][a-zA-Z0-9_]*"},
	{"go", {CSTR}, "\"([^\"\\]|\\\\.)*\""},
	{"go", {CNUM}, "'([^']|\\\\')*'"},
	{"go", {CSTR}, "`([^`]|\\\\`)*`"},
	{"go", {CNUM}, "[-+]?\\<(0[xX][0-9a-fA-F]+|[0-9.]+)\\>"},

	/* refer */
	{"bib", {0, SYN_BD, SYN_BGMK(3) | SYN_BD}, "^(%L) +(.*)$", 1},
	{"bib", {0, SYN_BD, 4 | SYN_BD}, "^(%A) (.*)$", 1},
	{"bib", {0, SYN_BD, 5 | SYN_BD}, "^(%T) (.*)$", 1},
	{"bib", {0, SYN_BD, 2 | SYN_BD}, "^(%[JB]) (.*)$", 1},
	{"bib", {0, SYN_BD, 5 | SYN_BD}, "^(%D) (.*)$", 1},
	{"bib", {0, SYN_BD, 7}, "^(%O) (.*)$", 1},
	{"bib", {0, SYN_BD, SYN_BD}, "^(%[A-Z]) (.*)$", 1},
	{"bib", {4}, "^#.*$", 1},

	/* python */
	{"py", {CCMT}, "#.*$"},
	{"py", {CKWD}, "\\<(class|def)\\>"},
	{"py", {CKWD}, "\\<(and|or|not|is|in)\\>"},
	{"py", {0, 0, CPRE, CIDN, CPRE, CIMP}, "((from)[ \t]+([^ ]+)[ \t]+)?(import)[ \t]+([^ ]+)"},
	{"py", {CKWD}, "\\<(import|from|global|lambda|del)\\>"},
	{"py", {CCON}, "\\<(for|while|if|elif|else|pass|return|break|continue)\\>"},
	{"py", {CCON}, "\\<(try|except|as|raise|finally|with)\\>"},
	{"py", {0, CFUN}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"py", {CSTR}, "[\"']([^\"'\\]|\\\\.)*[\"']"},
	{"py", {CIDN}, "[a-zA-Z][a-zA-Z0-9_]*"},

	/* neatmail listing */
	{"nm", {0, 6 | SYN_BD, 4 | SYN_BD, 3, 5, SYN_BD},
		"^([ROU][^ABCDHML0-9]?)(0*)([0-9]+)(@[^ \t]*)? *\t\\[([^\t]*)\\]\t *\\[([^\t]*)\\].*"},
	{"nm", {0 | SYN_BD | SYN_BGMK(6)}, "^[N].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(5)}, "^[A-Z][HA].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(3)}, "^[A-Z][MB].*$"},
	{"nm", {7}, "^[A-Z][LC].*$"},
	{"nm", {0 | SYN_BD | SYN_BGMK(7)}, "^[F].*$"},
	{"nm", {7 | SYN_IT}, "^\t.*$"},
	{"nm", {SYN_BD}, "^:.*$"},

	/* diff */
	{"diff", {1}, "^-.*$"},
	{"diff", {2}, "^\\+.*$"},
	{"diff", {6}, "^@.*$"},
	{"diff", {SYN_BD}, "^diff .*$"},

	/* directory listing */
	{"ls", {0, 0, 5 | SYN_BD, 0, 4}, "^/?([-a-zA-Z0-9_.]+/)*([-a-zA-Z0-9_.]+)\\>(:[0-9]*:)?(.*)$"},
	{"ls", {CCMT}, "^#.*$"},
};

/* how to highlight current line (hll option) */
#define SYN_LINE	(SYN_BGMK(11))

/* how to highlight text in the reverse direction */
#define SYN_REVDIR	(SYN_RV)

/* define it as "\33[8l" to disable BiDi in vte-based terminals */
#define LNPREF		""

/* right-to-left characters (used only in dircontexts[] and dirmarks[]) */
#define CR2L		"ءآأؤإئابةتثجحخدذرزسشصضطظعغـفقكلمنهوىييپچژکگی‌‍؛،»«؟ًٌٍَُِّْٔ"
/* neutral characters (used only in dircontexts[] and dirmarks[]) */
#define CNEUT		"-!\"#$%&'()*+,./:;<=>?@^_`{|}~ "

/* direction context; specifies the base direction of lines */
static struct dircontext {
	int dir;
	char *pat;
} dircontexts[] = {
	{-1, "^[" CR2L "]"},
	{+1, "^[a-zA-Z_0-9]"},
};

/* direction marks; the direction of contiguous characters in a line */
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
	{"ً", "ـً", 1},
	{"ٌ", "ـٌ", 1},
	{"ٍ", "ـٍ", 1},
	{"َ", "ـَ", 1},
	{"ُ", "ـُ", 1},
	{"ِ", "ـِ", 1},
	{"ّ", "ـّ", 1},
	{"ْ", "ـْ", 1},
	{"ٓ", "ـٓ", 1},
	{"ٔ", "ـٔ", 1},
	{"ٕ", "ـٕ", 1},
	{"ٰ", "ـٰ", 1},
};

/* external commands */
#define ECMD	"neatvi.sh"
