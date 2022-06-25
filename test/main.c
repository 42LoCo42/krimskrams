#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <string.h>

#include "net.h"

void client(krk_coro_t* coro, int fd) {
	char buf[9] = {0};
	size_t got = krk_net_recvEOF(coro, fd, buf, sizeof(buf) - 1);
	buf[got] = 0;

	strfry(buf);
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
