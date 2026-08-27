SRCS=$(filter-out src/main.c, $(wildcard src/*.c))
TESTS=$(wildcard test/*.c)
OBJS=$(SRCS:.c=.o) src/main.o
TESTOBJS=$(TESTS:.c=.o) $(SRCS:.c=.o)

TARGET=ctf-server

CC=gcc
CFLAGS=-Wall -Wextra -Werror -g

all: $(TARGET) $(TESTTARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test fmt clean
test: $(TESTOBJS)
	$(CC) $(CFLAGS) -o test-ctf-server $^

clean:
	rm -f $(OBJS) $(TARGET)

fmt:
	clang-format-19 --style=file -i ./src/*.c