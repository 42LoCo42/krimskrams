#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200112L

#include "net.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "coro.h"
#include "pushRD.h"

#define check(cond, ...) if(cond) err(1, __VA_ARGS__);

typedef krk_coro_t* coro_p;
typedef struct pollfd pollfd;
krk_pushrd_INSTANCE(coro_p);
krk_pushrd_INSTANCE(pollfd);

int krk_net_lookup(
	const char* addr,
	const char* port,
	krk_net_lookup_try_f try,
	void* extra
) {
	struct addrinfo* results = NULL;
	int gai = getaddrinfo(addr, port, NULL, &results);
	if(gai != 0) return gai;

	for(struct addrinfo* i = results; i != NULL; i = i->ai_next) {
		if(try(i, extra) != 0) continue;
		freeaddrinfo(results);
		return 0;
	}

	freeaddrinfo(results);
	return -1;
}

static void multiServerAccept(krk_coro_t* coro, int sock) {
	for(;;) {
		int client = accept(sock, NULL, 0);
		if(client < 0) krk_coro_error(coro);
		else krk_coro_yield(coro, (void*) (long) client);
	}
}

int krk_net_multiServer(const struct addrinfo* info, krk_net_client_f cl) {
	int sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	check(sock < 0, "Could not create server socket");

	int yes = 1;
	check(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0,
		"Could not reuse address");

	check(bind(sock, info->ai_addr, info->ai_addrlen) < 0, "Could not bind");
	check(listen(sock, 1) < 0, "Could not listen");

	krk_pushrd_t$coro_p coros   = {0};
	krk_pushrd_t$pollfd pollfds = {0};

	krk_coro_t* server_coro = malloc(sizeof(krk_coro_t));
	check(server_coro == NULL, "Could not allocate server coroutine");
	check(krk_coro_mk(server_coro, multiServerAccept, 1, sock) < 0,
		"Could not build server coroutine");
	check(krk_pushrd_add$coro_p(&coros,  server_coro) < 0,
		"Could not store server coroutine");

	struct pollfd server_pfd = {
		.fd = sock,
		.events = POLLIN,
		.revents = 0,
	};
	check(krk_pushrd_add$pollfd(&pollfds, server_pfd) < 0,
		"Could not store server pollfd");

	int running = 1;
	while(running) {
		check(poll(pollfds.buf, pollfds.len, -1) < 0, "Could not poll");
		for(size_t i = 0; i < pollfds.len; ++i) {
			struct pollfd fd = pollfds.buf[i];
			if(!fd.revents) continue;

			if(fd.revents & POLLIN) {
				check(krk_coro_run(coros.buf[i]) < 0,
					"Could not switch to coroutine");

				switch(coros.buf[i]->state) {
					case PAUSED:
						if(i != 0) break;
							int client_fd = (long) coros.buf[i]->result;

							krk_coro_t* client_coro = malloc(sizeof(krk_coro_t));
							check(client_coro == NULL,
								"Could not allocate client coroutine");
							check(
								krk_coro_mk(client_coro, cl, 1, client_fd) < 0,
								"Could not build client coroutine"
							);
							check(
								krk_pushrd_add$coro_p(&coros, client_coro) < 0,
								"Could not store client coroutine"
							);

							struct pollfd client_pfd = {
								.fd = (int) (long) coros.buf[i]->result,
								.events = POLLIN,
								.revents = 0,
							};
							check(
								krk_pushrd_add$pollfd(&pollfds, client_pfd) < 0,
								"Could not store client pollfd"
							);
						break;

					case FINISHED:
					case ERRORED:
						if(coros.buf[i]->result != NULL) running = 0;
						close(fd.fd);
						krk_coro_free(coros.buf[i]);
						free(coros.buf[i]);
						krk_pushrd_del$coro_p(&coros, i);
						krk_pushrd_del$pollfd(&pollfds, i);
						break;

					default:
						break;
				}
			} else {
				warnx("unknown event %d on %d\n", fd.revents, fd.fd);
			}
		}
	}
	
	for(size_t i = 0; i < coros.len; ++i) {
		krk_coro_free(coros.buf[i]);
		free(coros.buf[i]);
	}
	free(coros.buf);
	free(pollfds.buf);

	return 0;
}

int krk_net_printAddr(const struct addrinfo* info, void* unused) {
	(void) unused;

	char buf[INET6_ADDRSTRLEN] = {0};
	switch(info->ai_family) {
		case AF_INET:
			inet_ntop(
				info->ai_family,
				&((struct sockaddr_in*) info->ai_addr)->sin_addr.s_addr,
				buf,
				INET6_ADDRSTRLEN
			);
			break;
		case AF_INET6:
			inet_ntop(
				info->ai_family,
				&((struct sockaddr_in6*) info->ai_addr)->sin6_addr.s6_addr,
				buf,
				INET6_ADDRSTRLEN
			);
			break;
		default:
			strncpy(buf, "Unknown AF", INET6_ADDRSTRLEN);
			break;
	}
	printf("%s\n", buf);
	return -1;
}

typedef ssize_t (*doAll_f)(int, void*, size_t, int);

static size_t doAll(
	doAll_f fn,
	int eofIsError,
	krk_coro_t* coro,
	int fd,
	void* buf,
	size_t len
) {
	for(size_t i = 0; i < len;) {
		ssize_t num = fn(fd, buf + i, len - i, MSG_DONTWAIT);
		if(num == 0) {
			if(eofIsError) krk_coro_error(coro); else return i;
		}
		if(num < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				krk_coro_yield(coro, NULL);
			else krk_coro_error(coro);
		} else {
			i += num;
		}
	}
	return len;
}

void krk_net_sendAll(krk_coro_t* coro, int fd, void* buf, size_t len) {
	doAll((doAll_f) send, 1, coro, fd, buf, len);
}

void krk_net_recvAll(krk_coro_t* coro, int fd, void* buf, size_t len) {
	doAll(recv, 1, coro, fd, buf, len);
}

size_t krk_net_recvEOF(krk_coro_t* coro, int fd, void* buf, size_t len) {
	return doAll(recv, 0, coro, fd, buf, len);
}
