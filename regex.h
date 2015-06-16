#define REG_EXTENDED		0x01
#define REG_NOSUB		0x02
#define REG_ICASE		0x04
#define REG_NEWLINE		0x08
#define REG_NOTBOL		0x10
#define REG_NOTEOL		0x20

typedef struct {
	long rm_so;
	long rm_eo;
} regmatch_t;

typedef struct regex *regex_t;

int regcomp(regex_t *preg, char *regex, int cflags);
int regexec(regex_t *preg, char *str, int nmatch, regmatch_t pmatch[], int eflags);
int regerror(int errcode, regex_t *preg, char *errbuf, int errbuf_size);
void regfree(regex_t *preg);
