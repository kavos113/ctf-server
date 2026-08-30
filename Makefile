SRCS=$(filter-out src/main.c, $(wildcard src/*.c) $(wildcard src/app/*.c))
TESTS=$(wildcard test/*.c)
OBJS=$(SRCS:.c=.o) src/main.o
TESTOBJS=$(TESTS:.c=.o) $(SRCS:.c=.o)

TARGET=ctf-server
TESTTARGET=test-ctf-server

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -g

CFLAGS += $(shell mysql_config --cflags)
LDFLAGS = $(shell mysql_config --libs) -pthread

all: $(TARGET) test

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -Isrc -o $@

.PHONY: test fmt clean
test: $(TESTOBJS)
	$(CC) -o $(TESTTARGET) $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TESTOBJS) $(TARGET) $(TESTTARGET)

fmt:
	clang-format-19 --style=file -i ./src/*.c
	clang-format-19 --style=file -i ./src/*.h
	clang-format-19 --style=file -i ./src/app/*.c
	clang-format-19 --style=file -i ./src/app/*.h
	clang-format-19 --style=file -i ./test/*.c
	clang-format-19 --style=file -i ./test/*.h