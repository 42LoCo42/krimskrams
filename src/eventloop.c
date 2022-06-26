#define _DEFAULT_SOURCE

#include "eventloop.h"

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#define check(cond, ...) if(cond) err(1, __VA_ARGS__);

krk_pushrd_add_INSTANCE(coro_p);
krk_pushrd_del_INSTANCE(coro_p);
krk_pushrd_add_INSTANCE(pollfd);
krk_pushrd_del_INSTANCE(pollfd);

int krk_eventloop_run(krk_eventloop_t* loop) {
	int running = 1;
	while(running) {
		check(poll(
			loop->pollfds.buf,
			loop->pollfds.len,
			-1
		) < 0, "Could not poll");

		for(size_t i = 0; i < loop->pollfds.len; ++i) {
			struct pollfd fd = loop->pollfds.buf[i];
			if(!fd.revents) continue;

			if(fd.revents & POLLIN) {
				check(krk_coro_run(loop->coros.buf[i]) < 0,
					"Could not switch to coroutine");

				switch(loop->coros.buf[i]->state) {
					case FINISHED:
					case ERRORED:
						if(loop->coros.buf[i]->result != NULL) running = 0;
						krk_eventloop_delAt(loop, i);
						break;
					default:
						break;
				}
			} else {
				warnx("unknown event %d on %d\n", fd.revents, fd.fd);
			}
		}
	}

	for(size_t i = 0; i < loop->coros.len; ++i) {
		krk_coro_free(loop->coros.buf[i]);
		free(loop->coros.buf[i]);
	}
	free(loop->coros.buf);
	free(loop->pollfds.buf);

	return 0;
}

int krk_eventloop_addFd(
	krk_eventloop_t* loop,
	int fd,
	krk_eventloop_handler_f handler,
	void* extra
) {
	check(fcntl(fd, F_SETFL, O_NONBLOCK) < 0,
		"Could not set file descriptor %d to non-blocking mode", fd);

	krk_coro_t* coro = malloc(sizeof(krk_coro_t));
	check(coro == NULL, "Could not allocate coroutine");
	check(krk_coro_mk(coro, handler, 2, loop, fd) < 0,
		"Could not build coroutine");
	coro->extra = extra;
	check(krk_pushrd_add$coro_p(&loop->coros,  coro) < 0,
		"Could not store coroutine");

	struct pollfd pfd = {
		.fd      = fd,
		.events  = POLLIN,
		.revents = 0,
	};
	check(krk_pushrd_add$pollfd(&loop->pollfds, pfd) < 0,
		"Could not store pollfd");
	return 0;
}

void krk_eventloop_delFd(
	krk_eventloop_t* loop,
	int fd
) {
	for(size_t i = 0; i < loop->pollfds.len; ++i) {
		if(loop->pollfds.buf[i].fd == fd) {
			krk_eventloop_delAt(loop, i);
			return;
		}
	}
}

void krk_eventloop_delAt(
	krk_eventloop_t* loop,
	size_t i
) {
	close(loop->pollfds.buf[i].fd);
	krk_coro_free(loop->coros.buf[i]);
	free(loop->coros.buf[i]);
	krk_pushrd_del$coro_p(&loop->coros, i);
	krk_pushrd_del$pollfd(&loop->pollfds, i);
}
