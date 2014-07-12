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
    const char *usage_str = "fft-pgm [-a <kaiser-bessel alpha>] [-l | -L] [-n <FFT size (2^n)>] [-s <stepsize>] [-o <output PGM file>] infile.wav\n"
                            "-l gives a linear scaling, default is logarithmic (can be specified explicitly with -L as well).\n"
                            "Default values are: -n 11 (2048-bit real FFT, so actually a 1024-bit complex FFT)\n"
                            "                    -a 12.0 (Kaiser-Bessel window alpha of 12.0)\n"
                            "                    -s 128 (128 steps per decade in log-frequency plot)\n" 
                            "If no output filename is specified, the output image is written to standard output.\n";
    write(1, usage_str, strlen(usage_str));
}

static void gen_logscale_mapping(uint32_t *lin2log, unsigned int n4, unsigned int stepsize)
{
    float step = expf((float)M_LN10 * (1.0f / (float)stepsize));
    float idx = step;
    uint32_t i = 0;
    for (i = 1; i < n4; i++) {
        lin2log[i] = lrintf(idx);
        idx *= step;
    }
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
    float *tmpbuf = (float*)convbuff;
    ssize_t i, j = 0, bytes_read = read(fd, tmpbuf, outsize * 2 * sizeof(float));
    for (i = 0; i < (bytes_read >> 2); i++) {
        outbuf[j++] = 0.5f * (tmpbuf[2*i] + tmpbuf[2*i + 1]);
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

static inline int32_t get_exptval(float x)
{
    _f32 hx;
    hx.f = x;
    hx.i &= 0x7fffffff;
    return ((hx.i>>23)-126);
}

int create_rdft_image(float alpha, uint32_t fft_nbits, uint32_t use_logscale, char *fname_in, int fd_out) {
    struct stat st;
    FFTContext r;
    WAVHeader w;
    float buf[65536];
    float *windowbuf = NULL;
    FFTComplex bufout[65536];
    uint32_t *lin2log, n_octaves, stepsize = 0;
    uint32_t n, n4, n_lo, n_lo_tmp, n_hi, rows;
    uint32_t i = 0, k, frames;
    float lower_bound = 45.0f, freqstep, f_max;
    int c, fd_in = -1;
    uint8_t *p = NULL, *p_start = NULL;
    unsigned char head[32];
    unsigned int head_len;

#if 0
    while((c = getopt(argc, argv, "a:b:lLn:o:s:?")) != -1) {
        switch(c) {
            case 'a':
                alpha = strtod(optarg, NULL);
                break;
            case 'b':
                lower_bound = strtod(optarg, NULL);
                break;
            case 'l':
                use_logscale = 0;
                break;
            case 'L':
                use_logscale = 1;
                break;
            case 'n':
                fft_nbits = strtoul(optarg, NULL, 10);
                break;
            case 'o':
                fd_out = open(optarg, O_WRONLY|O_CREAT, 0644);
                break;
            case 's':
                stepsize = strtoul(optarg, NULL, 10);
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
        //fprintf(stderr, "Could not open input file %s, exiting.\n", argv[optind]);
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
        return -1;
    }
    //f_max = logf(w.samplerate >> 1) * (float)M_LOG10E;
    //n_octaves = get_exptval(f_max);

    n = (1 << fft_nbits);
    n4 = (n >> 2);
    ff_rdft_init(&r, fft_nbits);

    windowbuf = mempool_alloc_small(n * sizeof(float), 0);
    create_kb_window(windowbuf, n, alpha);

    freqstep = ldexpf((float)w.samplerate, -(int32_t)(fft_nbits-1));
    lower_bound /= freqstep;
    n_lo_tmp = lrintf(lower_bound);

    if (!stepsize) {
        stepsize = (1 << (fft_nbits-4)); // ((1 << (fft_nbits-1)) >> 3)
        stepsize = ((stepsize * 29) >> 5); // stepsize *= 0.9f 
    }
    fprintf(stderr, "stepsize: %u\n", stepsize);

    lin2log = mempool_alloc_small(n4 * sizeof(uint32_t), 1);
    if (use_logscale) {
        gen_logscale_mapping(lin2log, n4, stepsize);
        for (n_lo = 0; n_lo < n4; n_lo++) {
            if (lin2log[n_lo] > n_lo_tmp) break;
        }
        for (n_hi = n_lo; n_hi < n4; n_hi++) {
            if (lin2log[n_hi] > n4) break;
        }
    } else {
        for (k = 0; k < n4; k++) {
            lin2log[k] = k;
        }
        n_hi = n4;
        n_lo = n_lo_tmp;
    }

    rows = frames/n4;
    rows /= (4*w.channels);
    p = malloc(2*frames+127);
    p_start = p;
    head_len = snprintf(head, 31, "P5\n%u %u\n255\n", (n_hi - n_lo), rows);
    write(fd_out, head, head_len);
    //fprintf(stderr, "n4: %u, stepsize: %u\n", n4, stepsize);
    while(i < frames) {
        if (read_wavfile(fd_in, buf, n) <= 0) break;
        vector_fmul_sse(buf, buf, windowbuf, n);
        ff_rdft_calc(&r, (float*)bufout, buf, 0);
        for(k=n_lo; k<n_hi; k+=1) {
            uint32_t k2 = lin2log[k];
            float tmp2 = ((bufout[k2].re*bufout[k2].re)+(bufout[k2].im*bufout[k2].im));
            tmp2 *= 65536.0f;
            if(tmp2 > 1.0f) {
                tmp2 = to_dB(tmp2); tmp2 += tmp2;
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

