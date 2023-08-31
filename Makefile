program_NAME := sema

SRCS = sema.c
OBJS := ${SRCS:.c=.o}

.PHONY: all

CFLAGS = -Wall -ggdb
CC := gcc

all: $(program_NAME)

$(program_NAME): $(OBJS)
	@$(CC) -c $(CFLAGS) $(OBJS)
	ar rcs libsema.a sema.o
	gcc -o $(program_NAME) test.c -L. -lsema -lpthread

clean:
	rm -rf *.o $(program_NAME) libsema.a
