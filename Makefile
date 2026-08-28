SRCS=$(filter-out src/main.c, $(wildcard src/*.c))
TESTS=$(wildcard test/*.c)
OBJS=$(SRCS:.c=.o) src/main.o
TESTOBJS=$(TESTS:.c=.o) $(SRCS:.c=.o)

TARGET=ctf-server
TESTTARGET=test-ctf-server

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -g 

all: $(TARGET) test

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -Isrc -o $@

.PHONY: test fmt clean
test: $(TESTOBJS)
	$(CC) $(CFLAGS) -o $(TESTTARGET) $^

clean:
	rm -f $(OBJS) $(TESTOBJS) $(TARGET) $(TESTTARGET)

fmt:
	clang-format-19 --style=file -i ./src/*.c
	clang-format-19 --style=file -i ./test/*.c