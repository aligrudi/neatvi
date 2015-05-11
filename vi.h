/* neatvi main header */

#define PATHLEN		512

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
void lbuf_mark(struct lbuf *lbuf, int mark, int pos);
int lbuf_markpos(struct lbuf *lbuf, int mark);
void lbuf_undo(struct lbuf *lbuf);
void lbuf_redo(struct lbuf *lbuf);
void lbuf_undomark(struct lbuf *lbuf);
void lbuf_undofree(struct lbuf *lbuf);

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
struct reset *reset_make(int n, char **pat);
int reset_find(struct reset *re, char *s, int n, int *grps, int flg);
void reset_free(struct reset *re);

/* rendering lines */
char *ren_all(char *s, int wid);
int ren_cursor(char *s, int pos);
int ren_next(char *s, int p, int dir);
int ren_eol(char *s, int dir);
int ren_pos(char *s, int off);
int ren_off(char *s, int pos);
int ren_last(char *s);
int ren_cmp(char *s, int pos1, int pos2);
int ren_region(char *s, int c1, int c2, int *l1, int *l2, int closed);

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
int uc_dir(char *s);
int uc_wid(char *s);
int uc_slen(char *s);
int uc_code(char *s);
char *uc_chr(char *s, int off);
char *uc_sub(char *s, int beg, int end);
char *uc_dup(char *s);
int uc_isspace(char *s);
int uc_isprint(char *s);
int uc_isdigit(char *s);
int uc_isalpha(char *s);
int uc_kind(char *c);
char **uc_chop(char *s, int *n);
char *uc_next(char *s);
char *uc_beg(char *beg, char *s);
char *uc_end(char *beg, char *s);
char *uc_shape(char *beg, char *s);

/* managing the terminal */
#define xrows		(term_rows() - 1)
#define xcols		(term_cols())

void term_init(void);
void term_done(void);
void term_str(char *s);
void term_chr(int ch);
void term_pos(int r, int c);
void term_clear(void);
void term_kill(void);
int term_rows(void);
int term_cols(void);
int term_read(int timeout);
void term_record(void);
void term_commit(void);

#define TERMCTRL(x)	((x) - 96)
#define TERMESC		27

/* line-oriented input and output */
char *led_prompt(char *pref, char *post);
char *led_input(char *pref, char *post, int *row, int *col);
void led_print(char *msg, int row);
char *led_keymap(int c);

/* ex commands */
void ex(void);
void ex_command(char *cmd);

/* global variables */
extern struct lbuf *xb;
extern int xrow;
extern int xcol;
extern int xtop;
extern int xled;
extern char xpath[];
extern int xquit;
extern int xdir;
