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
	{"c", "\\.[hc]$", "^([a-zA-Z_].*)?\\<%s\\>.*([^;]|[^)];)$"},
	{"roff", "\\.(ms|me|mom|tr|roff|tmac|[1-9])$", "^\\.(de|nr|ds) +%s\\>"},
	{"tex", "\\.tex$"},
	{"msg", "letter$|mbox$|mail$"},
	{"mk", "Makefile$|makefile$|\\.mk$", "^%s:"},
	{"sh", "\\.sh$", "^(function +)?%s(\\(\\))? *\\{", "^(function +)?[a-zA-Z_0-9]+(\\(\\))? *\\{$"},
	{"go", "\\.go$", "^(func|var|const|type)( +\\(.*\\))? +%s\\>", "^(func|type)\\>.*\\{$"},
	{"py", "\\.py$", "^(def|class) +\\<%s\\>", "^(def|class) "},
	{"bib", "bib$"},
	{"nm", "\\.nm$"},
	{"diff", "\\.(patch|diff)$"},
	{"ls", "ls$"},
	{"txt", "$"},		/* matches everything; must be the last pattern */
};

#define SM(c)		(128 + c)
#define SX(c)		(256 + c)

/* syntax highlighting patterns */
static struct highlight {
	char *ft;		/* the filetype of this pattern */
	int att[16];		/* attributes of the matched groups */
	char *pat;		/* regular expression */
	int end;		/* the group ending this pattern */
} highlights[] = {
	/* status bar */
	{"---", {'_', 'F', 'W'}, "^(\"[^\"]*\").*(\\[[wr]\\]).*$"},
	{"---", {'_', 'F', 'N', 0}, "^(\"[^\"]*\").*=.*(L[0-9]+) +(C[0-9]+).*$"},
	{"---", {'Z'}, "^(\"[^\"]*\").*-.*(L[0-9]+) +(C[0-9]+).*$"},
	{"---", {'L', 'I', 'D', 'B'}, "^\\[([0-9])\\](.*/)*([^/]*)$"},
	{"---", {'Q', 'O', 'F'}, "^LEAP ([A-Z]+) .+\\[([^]]+)\\]$\n?"},
	{"---", {'_'}, "^.*$\n?"},
	/* ex mode */
	{"-ex", {':'}, "^[:/?!].*$"},
	{"-ex", {'X'}, "^.*$\n?"},

	/* C */
	{"c", {'t'}, "\\<(signed|unsigned|char|short|int|long|float|double|void|struct|enum|union|typedef)\\>"},
	{"c", {'k'}, "\\<(static|extern|register)\\>"},
	{"c", {'r'}, "\\<(return|for|while|if|else|do|sizeof|goto|switch|case|default|break|continue)\\>"},
	{"c", {'c'}, "//.*$"},
	{"c", {'c'}, "/\\*([^*]|\\*+[^*/])*\\*+/"},
	{"c", {'m', 'p'}, "^#([ \t]*include).*"},
	{"c", {'m', 'p'}, "^#([ \t]*[a-zA-Z0-9_]+)"},
	{"c", {0, 'f'}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"c", {'s'}, "\"([^\"\\]|\\\\.)*\""},
	{"c", {'h'}, "'([^\\]|\\\\.)'"},
	{"c", {'0'}, "[-+]?\\<(0[xX][0-9a-fA-F]+|[0-9]+)\\>"},
	{"c", {'c'}, "^\t*(/\\*.*| \\*.*| \\*\\//)$"},
	{"c", {'i'}, "[a-zA-Z][a-zA-Z0-9_]*"},

	/* troff */
	{"roff", {0, 'k', 'd'}, "^[.'][ \t]*(SH)(.*)$"},
	{"roff", {0, 'k', 'd'}, "^[.'][ \t]*de (.*)$"},
	{"roff", {0, 'b'}, "^[.'][ \t]*([^ \t\\]*)", 1},
	{"roff", {'s'}, "\"([^\"]|\"\")*\"?"},
	{"roff", {'c'}, "\\\\\".*$"},
	{"roff", {'b'}, "\\\\{1,2}[*$fgkmns]([^[(]|\\(..|\\[[^]]*\\])"},
	{"roff", {'v'}, "\\\\([^[(*$fgkmns]|\\(..|\\[[^]]*\\])"},
	{"roff", {'s'}, "\\$[^$]+\\$"},
	{"roff", {'0'}, "[-+]?\\<([0-9]+(\\.[0-9]+)?)\\>"},

	/* tex */
	{"tex", {0, 'k', 0, 0, 'm', 0, 'f'},
		"\\\\([^[{ \t]+)((\\[([^][]+)\\])|(\\{([^}]*)\\}))*"},
	{"tex", {'s'}, "\\$[^$]+\\$"},
	{"tex", {'c'}, "%.*$"},

	/* mail */
	{"msg", {SM('h')}, "^From .*20..$"},
	{"msg", {SM('h'), SM('s')}, "^Subject: (.*)$"},
	{"msg", {SM('h'), SM('f')}, "^From: (.*)$"},
	{"msg", {SM('h'), SM('t')}, "^To: (.*)$"},
	{"msg", {SM('h'), SM('c')}, "^Cc: (.*)$"},
	{"msg", {SM('h')}, "^[A-Z][-A-Za-z]+: .+$"},
	{"msg", {SM('r')}, "^> .*$"},

	/* makefile */
	{"mk", {0, 'd'}, "^([A-Za-z_][A-Za-z0-9_]*)[ \t]*="},
	{"mk", {'v'}, "\\$\\([a-zA-Z0-9_]+\\)"},
	{"mk", {'c'}, "#.*$"},
	{"mk", {0, 'f'}, "([A-Za-z_%.]+):"},

	/* shell script */
	{"sh", {0, 'k', 'd'}, "^(function +)?([a-zA-Z_0-9]+) *(\\(\\))? *\\{"},
	{"sh", {'r'}, "\\<(break|case|continue|do|done|elif|else|esac|fi|for|if|in|then|until|while|return)\\>"},
	{"sh", {'s'}, "\"([^\"\\]|\\\\.)*\""},
	{"sh", {'s'}, "'[^']*'"},
	{"sh", {'s'}, "`([^`\\]|\\\\.)*`"},
	{"sh", {'v'}, "\\$(\\{[^}]+\\}|[a-zA-Z_0-9]+|[!#$?*@-])"},
	{"sh", {'v'}, "\\$\\([^()]+\\)"},
	{"sh", {'f'}, "^\\. .*$"},
	{"sh", {'c'}, "#.*$"},

	/* go */
	{"go", {0, 'k', 'i', 'd'}, "^\\<(func) (\\([^()]+\\) )?([a-zA-Z0-9_]+)\\>"},
	{"go", {'k'}, "\\<(func|type|var|const|package)\\>"},
	{"go", {'p', 'm'}, "^import[ \t]+([^ ]+)"},
	{"go", {'k'}, "\\<(import|interface|struct)\\>"},
	{"go", {'r'}, "\\<(break|case|chan|continue|default|defer|else|fallthrough|for|go|goto|if|map|range|return|select|switch)\\>"},
	{"go", {0, 'b'}, "\\<(append|copy|delete|len|cap|make|new|complex|real|imag|close|panic|recover|print|println|int|int8|int16|int32|int64|uint|uint8|uint16|uint32|uint64|uintptr|float32|float64|complex128|complex64|bool|byte|rune|string|error)\\>\\("},
	{"go", {'t'}, "\\<(true|false|iota|nil|int8|int16|int32|int64|int|uint8|uint16|uint32|uint64|uint|uintptr|float32|float64|complex128|complex64|bool|byte|rune|string|error)\\>"},
	{"go", {'c'}, "//.*$"},
	{"go", {'c'}, "/\\*([^*]|\\*+[^*/])*\\*+/"},
	{"go", {0, 'f'}, "([a-zA-Z][a-zA-Z0-9_]*)\\(", 1},
	{"go", {'i'}, "[a-zA-Z][a-zA-Z0-9_]*"},
	{"go", {'s'}, "\"([^\"\\]|\\\\.)*\""},
	{"go", {'h'}, "'([^']|\\\\')*'"},
	{"go", {'s'}, "`([^`]|\\\\`)*`"},
	{"go", {'0'}, "[-+]?\\<(0[xX][0-9a-fA-F]+|[0-9.]+)\\>"},
	{"go", {'c'}, "^\t*(/\\*.*| \\*.*| \\*\\//)$"},

	/* refer */
	{"bib", {0, SM('h'), SM('l')}, "^(%L) +(.*)$", 1},
	{"bib", {0, SM('h'), SM('f')}, "^(%A) (.*)$", 1},
	{"bib", {0, SM('h'), SM('s')}, "^(%T) (.*)$", 1},
	{"bib", {0, SM('h'), SM('t')}, "^(%[JB]) (.*)$", 1},
	{"bib", {0, SM('h')}, "^(%[A-Z]) (.*)$", 1},
	{"bib", {'c'}, "^#.*$", 1},

	/* python */
	{"py", {'c'}, "#.*$"},
	{"py", {'k'}, "\\<(class|def)\\>"},
	{"py", {'k'}, "\\<(and|or|not|is|in)\\>"},
	{"py", {0, 0, 'p', 'i', 'p', 'm'}, "^((from)[ \t]+([^ ]+)[ \t]+)?(import)[ \t]+([^ ]+)"},
	{"py", {'k'}, "\\<(import|from|global|lambda|del)\\>"},
	{"py", {'r'}, "\\<(for|while|if|elif|else|pass|return|break|continue)\\>"},
	{"py", {'r'}, "\\<(try|except|as|raise|finally|with)\\>"},
	{"py", {0, 'f'}, "([a-zA-Z][a-zA-Z0-9_]+)\\(", 1},
	{"py", {'s'}, "[\"']([^\"'\\]|\\\\.)*[\"']"},
	{"py", {'i'}, "[a-zA-Z][a-zA-Z0-9_]*"},

	/* neatmail listing */
	{"nm", {0, SM('S'), SM('0'), SM('M'), SM('1'), SM('R'), SM('U')},
		"^([ROU][^ABCDHML0-9]?)(0*)([0-9]+)(@[^ \t]*)? *\t\\[([^\t]*)\\]\t *\\[([^\t]*)\\].*"},
	{"nm", {SM('N')}, "^[N].*$"},
	{"nm", {SM('A')}, "^[A-Z][HA].*$"},
	{"nm", {SM('B')}, "^[A-Z][MB].*$"},
	{"nm", {SM('C')}, "^[A-Z][C].*$"},
	{"nm", {SM('D')}, "^[A-Z][LD].*$"},
	{"nm", {SM('F')}, "^[F].*$"},
	{"nm", {SM('I')}, "^\t.*$"},
	{"nm", {SM('X')}, "^:.*$"},

	/* diff */
	{"diff", {SX('-')}, "^-.*$"},
	{"diff", {SX('+')}, "^\\+.*$"},
	{"diff", {SX('@')}, "^@.*$"},
	{"diff", {SX('D')}, "^diff .*$"},

	/* directory listing */
	{"ls", {SX('l'), SX('d'), SX('b'), SX('n'), SX('s')},
		"^/?([-a-zA-Z0-9_.]+/)*([-a-zA-Z0-9_.]+)\\>(:[0-9]*:)?([^\t]+\t)?(.*)$"},
	{"ls", {SX('c')}, "^#.*$"},
};

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
