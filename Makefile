SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)

TARGET=ctf-server

CC=gcc
CFLAGS=-Wall -Wextra -Werror -g

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

fmt:
	clang-format-19 --style=file -i ./src/*.c