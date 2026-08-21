SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TARGET=ctf-server

CC=gcc
CFLAGS=-Wall -Wextra -Werror -g

all: $(TARGET)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)