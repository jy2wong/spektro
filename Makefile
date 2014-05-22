CC=clang
CFLAGS=-g
.PHONY: all clean

all: spektro

spektro: main.c
	${CC} ${CFLAGS} `pkg-config --cflags gtk+-3.0` -o spektro main.c `pkg-config --libs gtk+-3.0`

clean:
	rm spektro
