CC = gcc
CFLAGS = -std=c99 -O3 -g -Wall -Wextra -Wpedantic -masm=intel
CFLAGS += -Iinclude -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS += -Wno-unused-result
LDFLAGS = -Tlinker.ld -no-pie

SRC = $(wildcard *.c **/*.c)
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: app

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

app: $(OBJ)
	$(CC) -o app $^ $(LDFLAGS)

clean:
	rm -rf $(OBJ) app
