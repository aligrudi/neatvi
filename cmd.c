#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include "vi.h"

static int cmd_make(char **argv, int *ifd, int *ofd)
{
	int pid;
	int pipefds0[2];
	int pipefds1[2];
	if (ifd)
		pipe(pipefds0);
	if (ofd)
		pipe(pipefds1);
	if (!(pid = fork())) {
		if (ifd) {		/* setting up stdin */
			dup2(pipefds0[0], 0);
			close(pipefds0[1]);
			close(pipefds0[0]);
		}
		if (ofd) {		/* setting up stdout and stderr */
			dup2(pipefds1[1], 1);
			dup2(pipefds1[1], 2);
			close(pipefds1[0]);
			close(pipefds1[1]);
		}
		execvp(argv[0], argv);
		exit(1);
	}
	if (ifd)
		close(pipefds0[0]);
	if (ofd)
		close(pipefds1[1]);
	if (pid < 0) {
		if (ifd)
			close(pipefds0[1]);
		if (ofd)
			close(pipefds1[0]);
		return -1;
	}
	if (ifd)
		*ifd = pipefds0[1];
	if (ofd)
		*ofd = pipefds1[0];
	return pid;
}

/*
 * Execute a shell command.
 *
 * If ibuf is given, it is passed as standard input to the process.
 * Otherwise, the process reads from the terminal.
 *
 * If oproc is 0, the process writes directly to the terminal.  If it
 * is 1, process' output is saved and returned.  If it is 2, in addition
 * to returning the output, it is written to the terminal.
 */
char *cmd_pipe(char *cmd, char *ibuf, int oproc)
{
	char *argv[] = {"/bin/sh", "-c", cmd, NULL};
	struct pollfd fds[4];
	struct sbuf isb = {0};
	struct sbuf osb = {0};
	char buf[512];
	int ifd = -1, ofd = -1;
	long ipos = 0, opos = 0;
	int pid = cmd_make(argv, &ifd, &ofd);
	if (pid <= 0)
		return NULL;
	memset(fds, 0, sizeof(fds));
	fds[0].fd = ofd;
	fds[1].fd = ifd;
	fds[0].events = POLLIN;
	fds[2].fd = 0;
	fds[3].fd = oproc != 1 ? 1 : -1;
	fds[2].events = POLLIN;
	sbuf_str(&isb, ibuf ? ibuf : "");
	while (fds[0].fd >= 0 || fds[1].fd >= 0 || (fds[3].fd >= 0 && opos < sbuf_len(&osb))) {
		fds[1].events = ipos < sbuf_len(&isb) ? POLLOUT : 0;
		fds[3].events = opos < sbuf_len(&osb) ? POLLOUT : 0;
		if (poll(fds, 4, 200) < 0)
			break;
		if (fds[0].revents & POLLIN) {
			long ret = read(fds[0].fd, buf, sizeof(buf));
			if (ret > 0)
				sbuf_mem(&osb, buf, ret);
			if (ret <= 0) {
				close(fds[0].fd);
				fds[0].fd = -1;
			}
		} else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(fds[0].fd);
			fds[0].fd = -1;
		}
		if (fds[1].revents & POLLOUT) {
			long ret = write(fds[1].fd, sbuf_buf(&isb) + ipos, sbuf_len(&isb) - ipos);
			if (ret > 0)
				ipos += ret;
			if (ibuf && ipos == sbuf_len(&isb)) {
				close(fds[1].fd);
				fds[1].fd = -1;
			}
		} else if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(fds[1].fd);
			fds[1].fd = -1;
		}
		if (fds[2].revents & POLLIN) {
			long ret = read(fds[2].fd, buf, sizeof(buf));
			long i;
			for (i = 0; i < ret; i++)
				if ((unsigned char) buf[i] == TK_CTL('c'))
					kill(pid, SIGINT);
			if (!ibuf && ret > 0)
				sbuf_mem(&isb, buf, ret);
			if (ret <= 0)
				fds[2].fd = -1;
		} else if (fds[2].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[2].fd = -1;
		}
		if (fds[3].revents & POLLOUT) {
			long ret = write(fds[3].fd, sbuf_buf(&osb) + opos, sbuf_len(&osb) - opos);
			if (ret > 0)
				opos += ret;
			if (ret <= 0)
				fds[3].fd = -1;
		} else if (fds[3].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[3].fd = -1;
		}
	}
	close(fds[0].fd);
	close(fds[1].fd);
	waitpid(pid, NULL, 0);
	sbuf_free(&isb);
	if (oproc)
		return sbuf_done(&osb);
	sbuf_free(&osb);
	return NULL;
}

int cmd_exec(char *cmd)
{
	cmd_pipe(cmd, NULL, 0);
	return 0;
}

char *cmd_unix(char *path, char *ibuf)
{
	char buf[512];
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un addr;
	long nw = 0, nc = 0;
	long len = strlen(ibuf);
	struct sbuf sb = {0};
	if (fd < 0)
		return NULL;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(fd);
		return NULL;
	}
	while (nw < len && (nc = write(fd, ibuf + nw, len - nw)) >= 0)
		nw += nc;
	shutdown(fd, SHUT_WR);
	while ((nc = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(&sb, buf, nc);
	close(fd);
	return sbuf_done(&sb);
}
