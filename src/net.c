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
#include "eventloop.h"
#include "pushRD.h"

#define check(cond, ...) if(cond) err(1, __VA_ARGS__);

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

static void multiServerAccept(
	krk_coro_t* coro,
	krk_eventloop_t* loop,
	int sock
) {
	for(;;) {
		int client = accept(sock, NULL, 0);
		if(client < 0) krk_coro_error(coro);
		else {
			krk_eventloop_addFd(loop, client, coro->extra, NULL);
			krk_coro_yield(coro, NULL);
		}
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

	krk_eventloop_t loop = {0};
	krk_eventloop_addFd(&loop, sock, multiServerAccept, cl);
	return krk_eventloop_run(&loop);
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

typedef ssize_t (*doAll_f)(int, void*, size_t);

static size_t doAll(
	doAll_f fn,
	int eofIsError,
	krk_coro_t* coro,
	int fd,
	void* buf,
	size_t len
) {
	for(size_t i = 0; i < len;) {
		ssize_t num = fn(fd, buf + i, len - i);
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

void krk_net_writeAll(krk_coro_t* coro, int fd, void* buf, size_t len) {
	doAll((doAll_f) write, 1, coro, fd, buf, len);
}

void krk_net_readAll(krk_coro_t* coro, int fd, void* buf, size_t len) {
	doAll(read, 1, coro, fd, buf, len);
}

size_t krk_net_readEOF(krk_coro_t* coro, int fd, void* buf, size_t len) {
	return doAll(read, 0, coro, fd, buf, len);
}

size_t krk_net_readAny(krk_coro_t* coro, int fd, void* buf, size_t len) {
	for(;;) {
		ssize_t got = read(fd, buf, len);
		if(got < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				krk_coro_yield(coro, NULL);
			} else krk_coro_error(coro);
		} else {
			return got;
		}
	}
}
