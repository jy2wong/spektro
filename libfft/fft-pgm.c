#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ffmpeg_fft.h"
int fdprintf(int fd, const char * __restrict format, ...);
void create_kb_window(float *windowbuf, unsigned int n, float alpha);
int (*read_wavfile)(int fd, float *out, size_t sz);

static void usage(void) {
    const char *usage_str = "fft-pgm [-a <kaiser-bessel alpha>] [-n <FFT size (2^n)>] [-o <output PGM file>] infile.wav\n"
                            "If no output filename is specified, the output image is written to standard output.\n";
    write(1, usage_str, strlen(usage_str));
}

typedef struct _WAVHeader {
    uint32_t riff;
    uint8_t pad0[8];
    uint32_t tag;
    uint32_t taglen;
    uint16_t wav_id;
    uint16_t channels;
    uint32_t samplerate;
    uint32_t bitrate;
    uint32_t block_align;
    uint8_t pad1[8];
} __attribute__((packed)) WAVHeader;

static int16_t convbuff[1048576];

int read_wavfile_float_mono(int fd, float *outbuf, size_t outsize)
{
    ssize_t bytes_read = read(fd, outbuf, outsize * sizeof(float));
    return bytes_read;
}

int read_wavfile_float_stereo(int fd, float *outbuf, size_t outsize)
{
    ssize_t i, j = 0, bytes_read = read(fd, outbuf, outsize * 2 * sizeof(float));
    for (i = 0; i < (bytes_read >> 2); i++) {
        outbuf[j++] = 0.5f * (outbuf[2*i] + outbuf[2*i + 1]);
    }
    return bytes_read;
}

int read_wavfile_s16_mono(int fd, float *outbuf, size_t outsize)
{
    ssize_t i, bytes_read = read(fd, convbuff, outsize * sizeof(int16_t));
    bytes_read >>= 1;
    for (i = 0; i < bytes_read; i++) {
        float f = ((float)convbuff[i] / 32768.0f);
        if (f < -1.0f) f = -1.0f;
        if (f > 1.0f) f = 1.0f;
        outbuf[i] = f;
    }
    return bytes_read;
}

int read_wavfile_s16_stereo(int fd, float *outbuf, size_t outsize)
{
    ssize_t i, bytes_read = read(fd, convbuff, outsize * 2 * sizeof(int16_t));
    bytes_read >>= 2;
    for (i = 0; i < bytes_read; i++) {
        float f = ((float)convbuff[2*i] / 32768.0f);
        float g = ((float)convbuff[2*i+1] / 32768.0f);
        f = 0.5f * (f + g);
        if (f < -1.0f) f = -1.0f;
        if (f > 1.0f) f = 1.0f;
        outbuf[i] = f;
    }
    return bytes_read;
}

int create_rdft_image(float alpha, uint32_t fft_nbits, char *fname_in, int fd_out) {
    struct stat st;
    FFTContext r;
    WAVHeader w;
    float buf[65536];
    float *windowbuf = NULL;
    FFTComplex bufout[65536];
    uint32_t n, n2, n4, rows;
    if (fft_nbits == 0)
        fft_nbits = 10;
    uint32_t i = 0, k, frames;
    if (alpha < 0)
        alpha = 10.0f;
    int c, fd_in = -1;
    uint8_t *p = NULL, *p_start = NULL;
    unsigned char head[32];
    unsigned int head_len;

#if 0
    while((c = getopt(argc, argv, "n:a:o:?")) != -1) {
        switch(c) {
            case 'a':
                alpha = strtod(optarg, NULL);
                break;
            case 'n':
                fft_nbits = strtoul(optarg, NULL, 10);
                break;
            case 'o':
                fd_out = open(optarg, O_WRONLY|O_CREAT, 0644);
                break;
            case 'h':
            case '?':
                usage();
                return 0;
            default:
                break;
        }
    }
#endif
    if ((fd_in = open(fname_in, O_RDONLY, 0644)) < 0) {
        fprintf(stderr, "Could not open input file %s, exiting.\n", fname_in);
        return -1;
    }

    fstat(fd_in, &st);
    read(fd_in, &w, sizeof(WAVHeader));

    if (w.wav_id == 1) {
        read_wavfile = ((w.channels == 2) ? read_wavfile_s16_stereo : read_wavfile_s16_mono);
        frames = ((st.st_size-44)/2);
    } else if ((w.wav_id == 3) || (w.wav_id == 0xfffe)) {
        read_wavfile = ((w.channels == 2) ? read_wavfile_float_stereo : read_wavfile_float_mono);
        frames = ((st.st_size-44)/4);
    } else {
        fprintf(stderr, "Could not interpret %s as a WAV file; exiting.\n", fname_in);
        return -1;
    }

    n = (1 << fft_nbits);
    n4 = (n >> 2);
    ff_rdft_init(&r, fft_nbits);

    windowbuf = mempool_alloc_small(n * sizeof(float), 0);
    create_kb_window(windowbuf, n, alpha);

    rows = frames/n4;
    rows /= (4*w.channels);
    p = malloc(2*frames+127);
    p_start = p;
    head_len = snprintf(head, 31, "P5\n%u %u\n255\n", n4, rows);
    write(fd_out, head, head_len);
    while(i < frames) {
        if (read_wavfile(fd_in, buf, n) <= 0) break;
        for (k = 0; k < n; k++) {
            buf[k] *= windowbuf[k];
        }
        ff_rdft_calc(&r, (float*)bufout, buf, 0);
        for(k=0; k<n4; k+=1) {
            float tmp2 = ((bufout[k].re*bufout[k].re)+(bufout[k].im*bufout[k].im));
            tmp2 *= 256.0f;
            if(tmp2 > 1.0f) {
                tmp2 = to_dB(tmp2);
                if(tmp2 > 255.0f) tmp2 = 255.0f;
                *p++ = lrintf(tmp2);
            } else {
                *p++ = 0;
            }
        }
        i++;
    }
    close(fd_in);
    write(fd_out, p_start, (p - p_start));
    if (fd_out != STDOUT_FILENO) {
        close(fd_out);
    }
 
    ff_fft_cleanup(&r);
    return 0;
}

