SRCS=$(filter-out src/main.c, $(wildcard src/*.c) $(wildcard src/app/*.c))
TESTS=$(wildcard test/*.c)
OBJS=$(SRCS:.c=.o) src/main.o
TESTOBJS=$(TESTS:.c=.o) $(SRCS:.c=.o)

TARGET=ctf-server
TESTTARGET=test-ctf-server

DB_TEST_WRAPS=calloc malloc mysql_init mysql_real_connect mysql_close mysql_thread_end \
 mysql_stmt_init mysql_stmt_prepare mysql_stmt_field_count mysql_stmt_param_count \
 mysql_stmt_bind_param mysql_stmt_execute mysql_stmt_close mysql_stmt_error \
 mysql_stmt_affected_rows mysql_stmt_insert_id mysql_query mysql_store_result mysql_affected_rows \
 mysql_num_fields mysql_fetch_row mysql_fetch_lengths mysql_free_result
TEST_LDFLAGS=$(foreach symbol,$(DB_TEST_WRAPS),-Wl,--wrap=$(symbol))

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
	$(CC) $(CFLAGS) -o $(TESTTARGET) $^ $(LDFLAGS) $(TEST_LDFLAGS)

clean:
	rm -f $(OBJS) $(TESTOBJS) $(TARGET) $(TESTTARGET)

fmt:
	clang-format-19 --style=file -i ./src/*.c
	clang-format-19 --style=file -i ./src/*.h
	clang-format-19 --style=file -i ./src/app/*.c
	clang-format-19 --style=file -i ./src/app/*.h
	clang-format-19 --style=file -i ./test/*.c
	clang-format-19 --style=file -i ./test/*.h