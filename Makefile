CFLAGS := $(CFLAGS) -std=c11 -Wall -Wextra

main: CFLAGS += -I src
main: LDFLAGS += -L. -lkrimskrams -Wl,-rpath,.
main: test/main.c libkrimskrams.so
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

libkrimskrams.so: CFLAGS += -fPIC -shared
libkrimskrams.so: $(wildcard src/*.c)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

valgrind: main
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--suppressions=valgrind.conf \
		./$<
