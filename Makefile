CFLAGS  := $(CFLAGS) -std=c11 -Wall -Wextra
DESTDIR := /

main: CFLAGS += -I src
main: LDFLAGS += -L. -lkrimskrams -Wl,-rpath,.
main: test/main.c libkrimskrams.so
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

libkrimskrams.so: CFLAGS += -fPIC -shared
libkrimskrams.so: $(wildcard src/*.c)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

install: libkrimskrams.so
	install -Dm644 -t $(DESTDIR)/usr/include/krimskrams/ src/*.h
	install -Dm755 $< $(DESTDIR)/usr/lib/$<
	strip $(DESTDIR)/usr/lib/$<

uninstall:
	$(RM) -r -- $(DESTDIR)/usr/include/krimskrams
	$(RM)    -- $(DESTDIR)/usr/lib/libkrimskrams.so

valgrind: main
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--suppressions=valgrind.conf \
		./$<
