#ifndef KRK_EVENTLOOP_H
#define KRK_EVENTLOOP_H

#include <poll.h>

#include "coro.h"
#include "pushRD.h"

typedef krk_coro_t* coro_p;
typedef struct pollfd pollfd;
krk_pushrd_t_INSTANCE(coro_p);
krk_pushrd_t_INSTANCE(pollfd);

typedef struct _krk_eventloop_t krk_eventloop_t;

typedef int(*krk_eventloop_errorHandler_f)(
	krk_coro_t* coro,
	krk_eventloop_t* loop
);

struct _krk_eventloop_t {
	int running;
	krk_pushrd_t$coro_p coros;
	krk_pushrd_t$pollfd pollfds;
	krk_eventloop_errorHandler_f errorHandler;
};

typedef void(*krk_eventloop_handler_f)(
	krk_coro_t* coro,
	krk_eventloop_t* loop,
	int fd
);

int krk_eventloop_run(krk_eventloop_t* loop);
int krk_eventloop_addFd(
	krk_eventloop_t* loop,
	int fd,
	krk_eventloop_handler_f handler,
	void* extra
);
void krk_eventloop_delFd(
	krk_eventloop_t* loop,
	int fd
);
void krk_eventloop_delAt(
	krk_eventloop_t* loop,
	size_t index
);

#endif
