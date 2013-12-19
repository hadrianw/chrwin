#!/usr/bin/env ccrun
# -xc -std=c99 -D_BSD_SOURCE -D_GNU_SOURCE -Wall -Wextra -pedantic -lX11

/*
Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define UNUSED(X) (void)(X)
#define MAX(a, b) ((a) < (b) ? (b) : (a))

typedef struct {
	int fd;
	struct sockaddr_un addr;
} Conn;

static Conn ctrl = {
	.addr = {
		AF_UNIX,
		"/tmp/.X11-unix/X123"
	}
};
static Conn client;
static Conn server = {
	.addr = {
		AF_UNIX,
		"/tmp/.X11-unix/X1"
	}
};

static char buf[BUFSIZ];

static size_t children = 0;

void sigchld(int sig) {
	UNUSED(sig);
	children--;
	if(children) {
		shutdown(ctrl.fd, SHUT_RDWR);
	}
}

int runpipe() {
	int retval = EXIT_FAILURE;
	socklen_t len = sizeof(server.addr.sun_family) + strlen(server.addr.sun_path);
	server.fd = socket(server.addr.sun_family, SOCK_STREAM, 0);
	if(connect(server.fd, (struct sockaddr*)&server.addr, len) < 0)
		goto out_server;

	fd_set rfd, wfd;
	socklen_t clen = 0;
	socklen_t slen = 0;
	for(;;) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);

		FD_SET(client.fd, &rfd);
		FD_SET(server.fd, &rfd);
	
		FD_SET(client.fd, &wfd);
		FD_SET(server.fd, &wfd);

		if(select(MAX(client.fd, server.fd)+1, &rfd,
		   &wfd, NULL, NULL) < 0)
			goto out_client;
		if(FD_ISSET(client.fd, &rfd) && clen == 0) {
			printf("client -> ");
			clen = read(client.fd, &buf, sizeof(buf));
			printf("%8d", clen);
			if(clen == 0)
				break;
		}
		if(FD_ISSET(server.fd, &wfd) && clen > 0) {
			puts(" -> server");
			clen -= write(server.fd, &buf, clen);
		}
		if(FD_ISSET(server.fd, &rfd) && slen == 0) {
			printf("server -> ");
			slen = read(server.fd, &buf, sizeof(buf));
			printf("%8d", slen);
			if(slen == 0)
				break;
		}
		if(FD_ISSET(client.fd, &wfd) && slen > 0) {
			puts(" -> client");
			slen -= write(client.fd, &buf, slen);
		}
	}
	
	retval = EXIT_SUCCESS;
out_server:
	close(server.fd);
out_client:
	close(client.fd);
	return retval;
}

int main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	int retval = EXIT_FAILURE;
	socklen_t len;

	len = sizeof(ctrl.addr.sun_family) + strlen(ctrl.addr.sun_path);
	ctrl.fd = socket(ctrl.addr.sun_family, SOCK_STREAM, 0);
	unlink(ctrl.addr.sun_path);
	if(bind(ctrl.fd, (struct sockaddr*)&ctrl.addr, len) < 0) {
		puts("bind failed");
		goto out_ctrl;
	}

	listen(ctrl.fd, 4);
	puts("listen");

	struct sigaction act = {
		.sa_handler = sigchld,
		.sa_flags = SA_NOCLDSTOP
	};
	sigaction(SIGCHLD, &act, NULL);

	do {
		len = sizeof(client.addr);
		client.fd = accept(ctrl.fd, &client.addr, &len);
		if(client.fd == -1)
			continue;
		
		pid_t pid = fork();
		if(pid == -1) {
			if(client.fd)
				close(client.fd);
		} else if(pid == 0) {
			sigaction(SIGCHLD, NULL, NULL);
			close(ctrl.fd);
			puts("client connected");
			retval = runpipe();
			goto out;
		} else {
			children++;
		}
	} while(children);

	int status;
	do {
		status = wait(NULL);
	} while(status > 0);
	if(errno != ECHILD) {
		puts("error during wait()");
		goto out_ctrl;
	}

	retval = EXIT_SUCCESS;
out_ctrl:
	close(ctrl.fd);
out:
	puts("\ndone");
	return retval;
}

