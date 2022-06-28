#ifndef KRK_SERVER_H
#define KRK_SERVER_H

#include <netdb.h>

#include "coro.h"

typedef int(*krk_net_lookup_try_f)(const struct addrinfo* info, void* extra);
typedef void(*krk_net_client_f)(krk_coro_t* coro, int fd);

int krk_net_lookup(
	const char* addr,
	const char* port,
	krk_net_lookup_try_f try,
	void* extra
);

int krk_net_multiServer(const struct addrinfo* info, krk_net_client_f cl);
int krk_net_printAddr  (const struct addrinfo* info, void* unused);

void   krk_net_writeAll(krk_coro_t* coro, int fd, void* buf, size_t len);
void   krk_net_readAll (krk_coro_t* coro, int fd, void* buf, size_t len);
size_t krk_net_readEOF (krk_coro_t* coro, int fd, void* buf, size_t len);
size_t krk_net_readAny (krk_coro_t* coro, int fd, void* buf, size_t len);

#endif
