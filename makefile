# Author: 		Ran Bao
# Date:			16/Sept/2015
# Description:	Light weighted http server

# Definitions for compiler and other flags
CC = clang
CFLAGS = -Os -Wall -Wextra -Wstrict-prototypes -Wno-unused-parameter -g -std=c99 -D_GNU_SOURCE
INCLUDE = -I./include -I./src
LD_LIBS = lib/http_parser.o -lpthread
DEL = rm

# Definitions for object
PROG_OBJ = src/main.o src/config.o src/parser.o src/datetime.o src/client.o

# Targets
default: src/webhttpd.out
test: test/parser.out test/cgi.out test/datetime.out
all: default test


# Compile Source
src/%.o: src/%.c $(DEPS)
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

# Compile Testcases
test/%.o: test/%.c $(DEPS)
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

# Link
src/webhttpd.out: $(PROG_OBJ)
	$(CC) $(CFLAGS) $(LD_LIBS) $^ -o $@

test/parser.out: test/parser.o
	$(CC) $(CFLAGS) $(LD_LIBS) $^ -o $@

test/cgi.out: test/cgi.o
	$(CC) $(CFLAGS) $(LD_LIBS) $^ -o $@

test/datetime.out: test/datetime.o
	$(CC) $(CFLAGS) $(LD_LIBS) $^ -o $@

# Others
.PHONY: clean
clean:
	-$(DEL) src/*.o src/*.out test/*.o test/*.out