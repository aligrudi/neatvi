#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "json.h"
#include "vi.h"

static int lsp_pid;		/* LSP server PID */
static int lsp_ifd;		/* LSP server standard input */
static int lsp_ofd;		/* LSP server standard output */
static int lsp_idx;		/* LSP command index */
static struct sbuf *lsp_res;	/* LSP server input stream */
static char lsp_file[1024];	/* the open file */
static long lsp_mtime;		/* the modification time of lsp_file */
static int lsp_version;		/* lsp_file version */

static char *http_request(long sz, char *fmt, ...);
static int http_notify(long sz, char *fmt, ...);
static long http_hlen(char *buf, long len);

/* return the absolute path of project root; returns a static array. */
static char *lsp_path(void)
{
	static char uri[1024];
	strcpy(uri, "file://");
	getcwd(uri + strlen(uri), sizeof(uri) - strlen(uri));
	return uri;
}

static int lsp_initialize(void)
{
	char *res = http_request(512, "{\"jsonrpc\": \"2.0\", \"id\": %d, \"method\": \"initialize\", \"params\": {\"rootUri\": \"%s\", \"capabilities\": {\"general\": {\"positionEncodings\": [\"utf-8\"]}}}}\n",
		++lsp_idx, lsp_path());
	if (!res)
		return 1;
	free(res);
	if (http_notify(128, "{\"jsonrpc\": \"2.0\", \"method\": \"initialized\"}"))
		return 1;
	return 0;
}

static void fds_close(int fds[2])
{
	close(fds[0]);
	close(fds[1]);
}

int lsp_on(void)
{
	return lsp_pid > 0;
}

int lsp_init(char *cmd[])
{
	int ifds[2], ofds[2];
	lsp_done();
	if (pipe(ifds) < 0)
		return 1;
	if (pipe(ofds) < 0) {
		fds_close(ifds);
		return 1;
	}
	lsp_pid = fork();
	if (lsp_pid < 0) {
		fds_close(ifds);
		fds_close(ofds);
		return 1;
	}
	if (lsp_pid == 0) {
		char *log = getenv("LSPLOG");
		dup2(ifds[0], 0);
		dup2(ofds[1], 1);
		fds_close(ifds);
		fds_close(ofds);
		close(2);
		open(log ? log : "/dev/null", O_WRONLY | O_TRUNC | O_CREAT, 0600);
		execvp(cmd[0], cmd);
		exit(1);
	}
	lsp_ifd = ifds[1];
	lsp_ofd = ofds[0];
	lsp_idx = 0;
	close(ifds[0]);
	close(ofds[1]);
	fcntl(lsp_ifd, F_SETFL, fcntl(lsp_ifd, F_GETFL, 0) | O_NONBLOCK | FD_CLOEXEC);
	fcntl(lsp_ofd, F_SETFL, fcntl(lsp_ofd, F_GETFL, 0) | O_NONBLOCK | FD_CLOEXEC);
	lsp_res = sbuf_make();
	if (lsp_initialize()) {
		lsp_done();
		return 1;
	}
	return 0;
}

static char *lsp_filebody(char *path)
{
	char ec[] = {'"', '\\', '\b', '\f', '\n', '\r', '\t', '\0'};
	char er[] = {'"', '\\',  'b',  'f',  'n',  'r',  't', '\0'};
	int fd = open(path, O_RDONLY);
	char buf[1024];
	long nr;
	int i;
	struct sbuf *sb;
	if (fd < 0)
		return NULL;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < nr; i++) {
			char *r = strchr(ec, (unsigned char) buf[i]);
			if (r) {
				sbuf_chr(sb, '\\');
				sbuf_chr(sb, er[r - ec]);
			} else {
				sbuf_chr(sb, (unsigned char) buf[i]);
			}
		}
	}
	return sbuf_done(sb);
}

static char *lsp_lang(char *ft)
{
	if (!strcmp("sh", ft))
		return "shellscript";
	if (!strcmp("py", ft))
		return "python";
	return ft;
}

static long mtime(char *path)
{
	struct stat st;
	if (!stat(path, &st))
		return st.st_mtime;
	return -1;
}

static int lsp_getpos(char *ent, char *dst, int dstlen, int *row, int *off)
{
	char file[1024];
	char *uri = json_dict_get(ent, "uri");
	char *range = json_dict_get(ent, "range");
	char *start = range ? json_dict_get(range, "start") : NULL;
	if (uri && json_str(uri, file, sizeof(file)) >= 0 && strlen(file) > 7) {
		char *pref = lsp_path();
		char *path = file + strlen("file://");
		char *line = start ? json_dict_get(start, "line") : NULL;
		char *character = start ? json_dict_get(start, "character") : NULL;
		if (!strncmp(pref, file, strlen(pref)))
			path = file + strlen(pref) + 1;
		snprintf(dst, dstlen, "%s", path);
		*row = line ? atoi(line) : 0;
		*off = character ? atoi(character) : 0;
		return 0;
	}
	return 1;
}

static int lsp_open(char *path, char *ft)
{
	char *body;
	if (lsp_mtime > 0 && strcmp(lsp_file, path)) {
		http_notify(1024,
			"{\"jsonrpc\": \"2.0\", \"method\": \"textDocument/didClose\","
			" \"params\": {\"textDocument\": {\"uri\": \"%s/%s\"}}}\n",
			lsp_path(), path);
		lsp_mtime = 0;
	}
	if (!(body = lsp_filebody(path)))
		return 1;
	if (!lsp_mtime) {
		snprintf(lsp_file, sizeof(lsp_file), "%s", path);
		http_notify(1024 + strlen(body),
			"{\"jsonrpc\": \"2.0\", \"method\": \"textDocument/didOpen\","
			" \"params\": {\"textDocument\": {\"uri\": \"%s/%s\", \"languageId\": \"%s\", \"text\": \"%s\"}}}\n",
			lsp_path(), path, lsp_lang(ft), body);
		lsp_version = 0;
		lsp_mtime = mtime(path);
	} else if (lsp_mtime < mtime(path)) {
		http_notify(1024 + strlen(body),
			"{\"jsonrpc\": \"2.0\", \"method\": \"textDocument/didChange\","
			" \"params\": {\"textDocument\": {\"uri\": \"%s/%s\", \"version\": %d}, \"contentChanges\": [{\"text\": \"%s\"}]}}\n",
			lsp_path(), path, lsp_lang(ft), lsp_version, body);
		lsp_version++;
		lsp_mtime = mtime(path);
	}
	free(body);
	return 0;
}

int lsp_definition(char *path, int row, int off, char *ft, char *dst, int dstlen, int *drow, int *doff)
{
	char *res, *result;
	if (lsp_pid <= 0)
		return 1;
	if (lsp_open(path, ft))
		return 1;
	res = http_request(1024,
		"{\"jsonrpc\": \"2.0\", \"id\": %d, \"method\": \"textDocument/definition\","
		" \"params\": {\"textDocument\": {\"uri\": \"%s/%s\"}, \"position\": {\"line\": %d, \"character\": %d}}}\n",
		++lsp_idx, lsp_path(), path, row, off);
	if (!res)
		return 1;
	if ((result = json_dict_get(res + http_hlen(res, strlen(res)), "result"))) {
		char *ent = json_dict_get(result, "uri") ? result : json_list_get(result, 0);
		if (ent && !lsp_getpos(ent, dst, dstlen, drow, doff)) {
			free(res);
			return 0;
		}
	}
	free(res);
	return 1;
}

static char *lsp_fileread(char *path)
{
	char buf[1024];
	int fd = open(path, O_RDONLY);
	long nr;
	struct sbuf *sb;
	if (fd < 0)
		return NULL;
	sb = sbuf_make();
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	return sbuf_done(sb);
}

static int lsp_fileline(char *body, int goal, char *dst, int dstlen, int *lastln, long *lastpos)
{
	long ln = *lastln <= goal ? *lastln : 0;
	long pos = ln ? *lastpos : 0;
	char *s = body + pos;
	char *r;
	int i;
	for (i = ln; i < goal && s; i++) {
		r = strchr(s, '\n');
		s = r ? r + 1 : NULL;
	}
	while (s && (*s == ' ' || *s == '\t'))
		s++;
	if (i == goal && s && (r = strchr(s, '\n'))) {
		long len = MIN(dstlen - 1, r - s);
		memcpy(dst, s, len);
		dst[len] = '\0';
		*lastln = i;
		*lastpos = s - body;
		return 0;
	}
	return 1;
}

char *lsp_find(char *path, int row, int off, char *ft)
{
	char *res, *result;
	char **ents;
	int i;
	if (lsp_pid <= 0)
		return NULL;
	if (lsp_open(path, ft))
		return NULL;
	res = http_request(1024,
		"{\"jsonrpc\": \"2.0\", \"id\": %d, \"method\": \"textDocument/references\","
		" \"params\": {\"textDocument\": {\"uri\": \"%s/%s\"}, \"position\": {\"line\": %d, \"character\": %d}, \"context\": {\"includeDeclaration\": true}}}\n",
		++lsp_idx, lsp_path(), path, row, off);
	if (!res)
		return NULL;
	result = json_dict_get(res + http_hlen(res, strlen(res)), "result");
	if (!result) {
		free(res);
		return NULL;
	}
	if ((ents = json_list(result))) {
		struct sbuf *sb = sbuf_make();
		char lastfile[1024];
		char *lastbody = NULL;
		long lastpos;
		int lastrow;
		for (i = 0; ents[i]; i++) {
			int drow, doff;
			char file[1024];
			char line[128];
			if (!lsp_getpos(ents[i], file, sizeof(file), &drow, &doff)) {
				if (!lastbody || strcmp(lastfile, file)) {
					free(lastbody);
					lastbody = lsp_fileread(file);
					lastpos = 0;
					lastrow = 0;
					snprintf(lastfile, sizeof(lastfile), "%s", file);
				}
				line[0] = '\0';
				lsp_fileline(lastbody, drow, line, sizeof(line), &lastrow, &lastpos);
				sbuf_str(sb, file);
				sbuf_printf(sb, ":%d:%d: %s\n", drow + 1, doff + 1, line);
			}
		}
		free(lastbody);
		free(ents);
		free(res);
		return sbuf_done(sb);
	}
	free(res);
	return NULL;
}

void lsp_done(void)
{
	int i;
	if (lsp_pid <= 0)
		return;
	free(http_request(128, "{\"jsonrpc\": \"2.0\", \"id\": %d, \"method\": \"shutdown\"}\n", ++lsp_idx));
	http_notify(128, "{\"jsonrpc\": \"2.0\", \"method\": \"exit\"}");
	if (lsp_ifd)
		close(lsp_ifd);
	if (lsp_ofd)
		close(lsp_ofd);
	for (i = 0; i < 5; i++) {
		struct timespec ts = {.tv_sec = 0, .tv_nsec = i ? 200000000 : 5000000};
		kill(lsp_pid, i < 4 ? SIGTERM : SIGKILL);
		nanosleep(&ts, NULL);
		if (waitpid(lsp_pid, NULL, WNOHANG) == lsp_pid)
			break;
	}
	sbuf_free(lsp_res);
	lsp_res = NULL;
	lsp_pid = 0;
	lsp_ifd = 0;
	lsp_ofd = 0;
	lsp_mtime = 0;
}

static int startswith(char *r, char *s)
{
	while (*s)
		if (tolower((unsigned char) *s++) != tolower((unsigned char) *r++))
			return 0;
	return 1;
}

static long http_hlen(char *buf, long len)
{
	int pos = 0;
	char *r;
	while (pos + 4 <= len && (r = memchr(buf + pos, '\n', len - pos)) != NULL) {
		int cur = r - buf;
		if (cur == 0 || cur + 3 > len)
			continue;
		if (buf[cur + 1] == '\r' && buf[cur + 2] == '\n')
			return cur + 3;
		pos++;
	}
	return -1;
}

static long http_blen(char *buf, long len)
{
	char *r;
	long pos = 0;
	while (pos + 15 < len && (buf[pos] != '\r' || buf[pos + 1] != '\n')) {
		if (startswith(buf + pos, "content-length:")) {
			int val = pos + 15;
			int ret = 0;
			while (val < len && isspace((unsigned char) buf[val]))
				val++;
			while (val < len && buf[val] >= '0' && buf[val] <= '9')
				ret = ret * 10 + (buf[val++] - '0');
			return ret;
		}
		if ((r = memchr(buf + pos, '\n', len - pos)) == NULL)
			break;
		pos = r - buf + 1;
	}
	return 0;
}

static long http_got(void)
{
	char *res = sbuf_buf(lsp_res);
	long len = sbuf_len(lsp_res);
	long hlen = http_hlen(res, len);
	long blen = hlen > 0 ? http_blen(res, hlen) : -1;
	if (hlen >= 0 && blen >= 0 && (len - hlen) >= blen)
		return hlen + blen;
	return 0;
}

static int http_req(char *msg, long msg_sz)
{
	struct pollfd fds[2];
	char buf[512];
	long msg_nw = 0;
	fds[0].fd = lsp_ofd;
	fds[0].events = POLLIN;
	fds[1].fd = msg ? lsp_ifd : -1;
	fds[1].events = POLLOUT;
	while ((fds[0].fd >= 0 || fds[1].fd >= 0) && poll(fds, 2, 200) >= 0) {
		if (fds[0].revents & POLLIN) {
			long ret = read(fds[0].fd, buf, sizeof(buf));
			if (ret > 0)
				sbuf_mem(lsp_res, buf, ret);
			if (ret <= 0)
				fds[0].fd = -1;
		} else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[0].fd = -1;
		}
		if (fds[1].revents & POLLOUT) {
			long ret = write(fds[1].fd, msg + msg_nw, msg_sz - msg_nw);
			if (ret > 0)
				msg_nw += ret;
			if (ret <= 0 || msg_nw == msg_sz)
				fds[1].fd = -1;
		} else if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[1].fd = -1;
		}
		if (msg && msg_nw == msg_sz)
			break;
		if (!msg && http_got())
			break;
	}
	return 0;
}

static int http_fetch(char **res, long *len)
{
	if (!http_got())
		http_req(NULL, 0);
	*len = http_got();
	if (*len) {
		long tot = sbuf_len(lsp_res);
		*res = sbuf_done(lsp_res);
		lsp_res = sbuf_make();
		sbuf_mem(lsp_res, *res + *len, tot - *len);
		return 0;
	}
	return 1;
}

static int http_send(long sz, char *fmt, va_list ap)
{
	char *buf = malloc(sz);
	struct sbuf *sb = sbuf_make();
	vsnprintf(buf, sz, fmt, ap);
	sbuf_printf(sb, "Content-Length: %ld\r\n\r\n", strlen(buf));
	sbuf_str(sb, buf);
	free(buf);
	http_req(sbuf_buf(sb), sbuf_len(sb));
	sbuf_free(sb);
	return 0;
}

static int http_notify(long sz, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	http_send(sz, fmt, ap);
	va_end(ap);
	return 0;
}

static char *http_request(long sz, char *fmt, ...)
{
	char *res;
	long len;
	va_list ap;
	va_start(ap, fmt);
	http_send(sz, fmt, ap);
	va_end(ap);
	while (!http_fetch(&res, &len)) {
		char *body = res + http_hlen(res, len);;
		char *id = json_dict_get(body, "id");
		if (id && atoi(id) == lsp_idx)
			return res;
		free(res);
	}
	return NULL;
}
