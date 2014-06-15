#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ffmpeg_fft.h"
//#include "libfft/fft-pgm.h"
int fdprintf(int fd, const char * __restrict format, ...);

/**
 * 0th order modified bessel function of the first kind.
 */
static float I0(float x){
    float v=1;
    float lastv=0;
    float t=1;
    int i;

    x= x*x/4;
    for(i=1; v != lastv; i++){
        lastv=v;
        t *= x/(i*i);
        v += t;
    }
    return v;
}

static void create_kb_window(float *windowbuf, unsigned int n, float alpha)
{
    unsigned int k, n2 = (n >> 1);
    float alpha_i0inv = 1.0f / I0(alpha);
    for(k=0; k<n2; k++) {
        float tmp = fabsf((float)k/(float)n);
        tmp = sqrtf(1.0f - tmp*tmp);
        windowbuf[k] = I0(alpha * tmp)*alpha_i0inv;
    }
    for(k=n2; k<n; k++) {
        float tmp = fabsf((float)(n-k)/(float)n);
        tmp = sqrtf(1.0f - tmp*tmp);
        windowbuf[k] = I0(alpha * tmp)*alpha_i0inv;
    }
}

static void usage(void) {
    const char *usage_str = "fft-pgm [-a <kaiser-bessel alpha>] [-n <FFT size (2^n)>] mono_RIFF_wavfile.wav\n";
    write(1, usage_str, strlen(usage_str));
}

static uint32_t itoa10(char *__restrict bufend, uint64_t uval)
{
    unsigned int digit;
    unsigned int i = 0, j = 0;
    unsigned char _buf[24];
    do {
        digit = uval % 10;
        uval /= 10;
        _buf[i++] = (digit + '0');
    } while (uval);
    while(i-- > 0) { bufend[j++] = _buf[i]; }
    bufend[j] = '\0';
    return j;
}

static void write_pgm_header(int fd, uint32_t w, uint32_t h) {
  // header
  unsigned char head[32];
  unsigned int head_len = 3;
  head[0] = 'P'; head[1] = '5'; head[2] = '\n';
  head_len += itoa10(head+head_len, w); head[head_len++] = ' ';
  head_len += itoa10(head+head_len, h); head[head_len++] = '\n';
  head[head_len++] = '2'; head[head_len++] = '5'; head[head_len++] = '5';
  head[head_len++] = '\n'; head[head_len] = '\0';
  write(fd, head, head_len);
}

typedef union _f32 {
    float f;
    uint32_t i;
} _f32;

float to_dB(float in){
  _f32 tmp;
  tmp.f = in;
  tmp.i &= 0x7fffffff;
  return ((float)(tmp.i * 3.58855719e-7f - 382.3080943f));
}

int create_rdft_image(float alpha, uint32_t fft_nbits, char *fname_in, int fd_out) {
    struct stat st;
    FFTContext r;
    float buf[65536], tsin[65536];
    float *windowbuf = NULL;
    FFTComplex bufout[65536];
    uint32_t n, n2, n4, rows;
    if (fft_nbits == 0)
        fft_nbits = 10;
    uint32_t i = 0, k, frames;
    if (alpha < 0)
        alpha = 10.0f;
    int c;
    uint8_t *p = NULL, *p_start = NULL;
    int fd_in = -1;

#if 0
    while((c = getopt(argc, argv, "n:a:?")) != -1) {
        switch(c) {
            case 'a':
                alpha = strtod(optarg, NULL);
                break;
            case 'n':
                fft_nbits = strtoul(optarg, NULL, 10);
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
    fd_in = open(fname_in, O_RDONLY, 0644);
    fstat(fd_in, &st);

    n = (1 << fft_nbits);
    n2 = (n >> 1);
    n4 = (n >> 2);
    ff_fft_init(&r, fft_nbits-1, 0);
    ff_rdft_init_sine_table(tsin, fft_nbits-1);

    windowbuf = malloc(n * sizeof(float));
    create_kb_window(windowbuf, n, alpha);

    frames = ((st.st_size-44)/4);
    rows = frames/n4;
    p = malloc(2*frames+127);
    p_start = p;
    write_pgm_header(fd_out, n4, rows/4);
    while(i < frames) {
        unsigned int bytes_to_read = (n << 2);
        if(read(fd_in, buf, bytes_to_read) < bytes_to_read) break;
        for (k = 0; k < n; k++) {
            buf[k] *= windowbuf[k];
        }
        ff_rdft_calc(&r, (float*)bufout, buf, tsin);
        for(k=0; k<n4; k+=1) {
            float tmp2 = ((bufout[k].re*bufout[k].re)+(bufout[k].im*bufout[k].im));
            tmp2 *= 256.0f;
            if(tmp2 > 1.0f) {
                //tmp2 = logf(tmp2)/(float)M_LN10;
                tmp2 = to_dB(tmp2);
                //tmp2 *= 48.0f;
                tmp2 *= 4.0f;
                if(tmp2 > 255.0f) tmp2 = 255.0f;
                *p++ = lrintf(tmp2);
            } else {
                *p++ = 0;
            }
        }
        i++;
    }
    write(fd_out, p_start, (p - p_start));
    close(fd_in);
    return 0;
}

