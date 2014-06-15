CC=clang
CFLAGS=-g
FFT_CFLAGS=-g -Wall -O2 -fno-math-errno -fno-omit-frame-pointer -fno-asynchronous-unwind-tables -fwrapv
PKGS="gtk+-3.0 libavformat libavutil libavcodec libavfilter"
.PHONY: all clean

all: spektro

spektro: main.o spektro-audio.o libfft/fft-pgm.o libfft/fft.o libfft/fft_sse.o
	${CC} ${CFLAGS} `pkg-config --cflags ${PKGS}` -o "$@" $^ -lm `pkg-config --libs ${PKGS}`

libfft/fft_sse.o: libfft/fft_sse.asm
	yasm -DARCH_X86_64=1 -f elf64 -o "$@" "$^"

libfft/%.o: libfft/%.c
	${CC} ${FFT_CFLAGS} -c -o "$@" $^

%.o: %.c
	${CC} ${CFLAGS} -c `pkg-config --cflags ${PKGS}` -o "$@" "$^"

clean:
	@rm -f spektro *.o libfft/*.o
