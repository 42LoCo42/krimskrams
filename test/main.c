#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "eventloop.h"
#include "net.h"

static int stdinAdded = 0;

void stdinReader(krk_coro_t* coro, krk_eventloop_t* loop, int fd) {
	for(;;) {
		char buf[16] = {0};
		ssize_t got = read(fd, buf, sizeof(buf));
		if(got < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				krk_coro_yield(coro, NULL);
		} else if(got > 0) {
			for(size_t i = 0; i < loop->pollfds.len; ++i) {
				int target = loop->pollfds.buf[i].fd;
				if(i == 0 || target == fd) continue;
				write(target, buf, got);
			}
		}
		krk_coro_yield(coro, NULL);
	}
}

void client(krk_coro_t* coro, krk_eventloop_t* loop, int fd) {
	if(!stdinAdded) {
		puts("adding stdin");
		printf("%d\n", krk_eventloop_addFd(loop, STDIN_FILENO, stdinReader, NULL));
		stdinAdded = 1;
	}

	char buf[9] = {0};
	size_t got = krk_net_recvEOF(coro, fd, buf, sizeof(buf) - 1);
	buf[got] = 0;

	if(strncmp(buf, "quit", 4) == 0) krk_coro_finish(coro, (void*) 1);

	krk_net_sendAll(coro, fd, buf, got);
	krk_coro_finish(coro, NULL);
}

int main() {
	return krk_net_lookup(
		"localhost",
		"37812",
		(krk_net_lookup_try_f) krk_net_multiServer,
		client
	);
}
