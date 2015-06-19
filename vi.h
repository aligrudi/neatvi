/* neatvi main header */

/* helper macros */
#define LEN(a)		(sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))

/* line buffer, managing a number of lines */
struct lbuf *lbuf_make(void);
void lbuf_free(struct lbuf *lbuf);
void lbuf_rd(struct lbuf *lbuf, int fd, int pos);
void lbuf_wr(struct lbuf *lbuf, int fd, int beg, int end);
void lbuf_rm(struct lbuf *lbuf, int beg, int end);
char *lbuf_cp(struct lbuf *lbuf, int beg, int end);
void lbuf_put(struct lbuf *lbuf, int pos, char *s);
char *lbuf_get(struct lbuf *lbuf, int pos);
int lbuf_len(struct lbuf *lbuf);
void lbuf_mark(struct lbuf *lbuf, int mark, int pos, int off);
int lbuf_jump(struct lbuf *lbuf, int mark, int *pos, int *off);
int lbuf_undo(struct lbuf *lbuf);
int lbuf_redo(struct lbuf *lbuf);
int lbuf_modified(struct lbuf *lb);
void lbuf_saved(struct lbuf *lb, int clear);
int lbuf_indents(struct lbuf *lb, int r);
int lbuf_eol(struct lbuf *lb, int r);
/* motions */
int lbuf_findchar(struct lbuf *lb, char *cs, int cmd, int n, int *r, int *o);
int lbuf_search(struct lbuf *lb, char *kw, int dir, int *r, int *o, int *len);
int lbuf_paragraphbeg(struct lbuf *lb, int dir, int *row, int *off);
int lbuf_sectionbeg(struct lbuf *lb, int dir, int *row, int *off);
int lbuf_wordbeg(struct lbuf *lb, int big, int dir, int *row, int *off);
int lbuf_wordend(struct lbuf *lb, int big, int dir, int *row, int *off);
int lbuf_pair(struct lbuf *lb, int *row, int *off);

/* string buffer, variable-sized string */
struct sbuf *sbuf_make(void);
void sbuf_free(struct sbuf *sb);
char *sbuf_done(struct sbuf *sb);
char *sbuf_buf(struct sbuf *sb);
void sbuf_chr(struct sbuf *sb, int c);
void sbuf_str(struct sbuf *sb, char *s);
void sbuf_mem(struct sbuf *sb, char *s, int len);
void sbuf_printf(struct sbuf *sbuf, char *s, ...);
int sbuf_len(struct sbuf *sb);
void sbuf_cut(struct sbuf *s, int len);

/* regular expression sets */
#define RE_ICASE		1
#define RE_NOTBOL		2
#define RE_NOTEOL		4

struct rset *rset_make(int n, char **pat, int flg);
int rset_find(struct rset *re, char *s, int n, int *grps, int flg);
void rset_free(struct rset *re);

/* rendering lines */
int *ren_position(char *s);
int ren_next(char *s, int p, int dir);
int ren_eol(char *s, int dir);
int ren_pos(char *s, int off);
int ren_cursor(char *s, int pos);
int ren_noeol(char *s, int p);
int ren_off(char *s, int pos);
int ren_wid(char *s);
int ren_region(char *s, int c1, int c2, int *l1, int *l2, int closed);
char *ren_translate(char *s, char *ln);
int ren_cwid(char *s, int pos);

/* text direction */
int dir_context(char *s);
void dir_reorder(char *s, int *ord);
void dir_init(void);
void dir_done(void);

/* string registers */
char *reg_get(int c, int *lnmode);
void reg_put(int c, char *s, int lnmode);
void reg_done(void);

/* utf-8 helper functions */
int uc_len(char *s);
int uc_wid(char *s);
int uc_slen(char *s);
int uc_code(char *s);
char *uc_chr(char *s, int off);
int uc_off(char *s, int off);
char *uc_sub(char *s, int beg, int end);
char *uc_dup(char *s);
int uc_isspace(char *s);
int uc_isprint(char *s);
int uc_isdigit(char *s);
int uc_isalpha(char *s);
int uc_kind(char *c);
char **uc_chop(char *s, int *n);
char *uc_next(char *s);
char *uc_prev(char *beg, char *s);
char *uc_beg(char *beg, char *s);
char *uc_end(char *s);
char *uc_shape(char *beg, char *s);
char *uc_lastline(char *s);

/* managing the terminal */
#define xrows		(term_rows() - 1)
#define xcols		(term_cols())

void term_init(void);
void term_done(void);
void term_suspend(void);
void term_str(char *s);
void term_chr(int ch);
void term_pos(int r, int c);
void term_clear(void);
void term_kill(void);
void term_room(int n);
int term_rows(void);
int term_cols(void);
int term_read(void);
void term_record(void);
void term_commit(void);
char *term_att(int att, int old);
void term_push(char *s, int n);
char *term_cmd(int *n);

#define TK_CTL(x)	((x) & 037)
#define TK_INT(c)	((c) < 0 || (c) == TK_ESC || (c) == TK_CTL('c'))
#define TK_ESC		(TK_CTL('['))

/* line-oriented input and output */
char *led_prompt(char *pref, char *post, char **kmap);
char *led_input(char *pref, char *post, char *ai, int ai_max, char **kmap);
char *led_read(char **kmap);
void led_print(char *msg, int row);
int led_pos(char *s, int pos);

/* ex commands */
void ex(void);
void ex_command(char *cmd);
char *ex_read(char *msg);
void ex_print(char *line);
void ex_show(char *msg);
void ex_init(char **files);
void ex_done(void);
char *ex_path(void);
char *ex_filetype(void);
struct lbuf *ex_lbuf(void);

#define xb 	ex_lbuf()

/* process management */
char *cmd_pipe(char *cmd, char *s, int iproc, int oproc);
int cmd_exec(char *cmd);

/* syntax highlighting */
#define SYN_BD		0x100
#define SYN_IT		0x200
#define SYN_RV		0x400
#define SYN_BGMK(b)	((b) << 16)
#define SYN_FG(a)	((a) & 0xffff)
#define SYN_BG(a)	((a) >> 16)

int *syn_highlight(char *ft, char *s);
char *syn_filetype(char *path);
int syn_merge(int old, int new);
void syn_init(void);
void syn_done(void);

/* configuration variables */
char *conf_kmapalt(void);
int conf_dirmark(int idx, char **pat, int *ctx, int *dir, int *grp);
int conf_dircontext(int idx, char **pat, int *ctx);
int conf_placeholder(int idx, char **s, char **d, int *wid);
int conf_highlight(int idx, char **ft, int **att, char **pat, int *end);
int conf_filetype(int idx, char **ft, char **pat);
int conf_highlight_revdir(int *att);

/* global variables */
extern int xrow;
extern int xoff;
extern int xtop;
extern int xvis;
extern int xled;
extern int xquit;
extern int xic;
extern int xai;
extern int xdir;
extern int xshape;
extern int xorder;

#define EXLEN		512	/* ex line length */

extern char xfindkwd[EXLEN];
extern int xfinddir;
