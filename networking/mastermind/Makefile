CC = gcc
CFLAGS = -std=c23 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror -Wno-unused-parameter -g
LDLIBS = -lm

SRCS = $(wildcard *.c)

mastermind: $(SRCS)
	$(CC) $(CFLAGS) -o mastermind $(SRCS) $(LDLIBS)

clean:
	rm -f mastermind
	rm -rf mastermind.dSYM

.PHONY: clean
