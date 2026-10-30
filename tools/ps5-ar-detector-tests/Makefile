CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test check clean

test: build/test_detector
	./build/test_detector

build/test_detector: detector.c tests/test_detector.c detector.h
	mkdir -p build
	$(CC) $(CFLAGS) -I. detector.c tests/test_detector.c -o $@

check:
	$(CC) $(CFLAGS) -Itests/stub -I. -fsyntax-only detector.c http_client.c main.c

clean:
	rm -rf build
