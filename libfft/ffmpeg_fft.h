/*
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_FFT_H
#define AVCODEC_FFT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Useful macros. */
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define DECLARE_ALIGNED(n,t,v) t __attribute__ ((aligned (n))) v

/* FFT computation */
typedef struct FFTComplex {
    float re, im;
} FFTComplex;

typedef struct FFTContext {
    uint32_t nbits;
    uint32_t inverse;
    uint16_t *revtab_fwd;
    uint16_t *revtab_inv;
    float *tsin;
} FFTContext;

#define COSTABLE(size) \
    DECLARE_ALIGNED(16, float, ff_cos_##size)[size/2]
extern COSTABLE(16);
extern COSTABLE(32);
extern COSTABLE(64);
extern COSTABLE(128);
extern COSTABLE(256);
extern COSTABLE(512);
extern COSTABLE(1024);
extern COSTABLE(2048);
extern COSTABLE(4096);
extern COSTABLE(8192);
extern COSTABLE(16384);
extern COSTABLE(32768);
extern COSTABLE(65536);
extern float* const ff_cos_tabs[17];

#if defined(__amd64__) || defined(__x86_64__) || defined(__i386__)
static inline unsigned int av_log2(unsigned int x) {
    unsigned int ret;
    __asm__ volatile("or $1, %1 \n\t"
             "bsr %1, %0 \n\t"
             :"=a"(ret) : "D"(x));
    return ret;
}
#else
#if defined(__arm__)
static inline unsigned int av_log2(unsigned int x)
{
    unsigned int ret;
    __asm__ __volatile__("clz\t%0, %1" : "=r" (ret) : "r" (x));
    ret = 31 - ret;
    return ret;
}
#else
#if defined(__alpha__)
static inline unsigned long av_log2(unsigned long x) {
    unsigned long ret;
    __asm__("ctlz %1,%0" : "=r"(ret) : "r"(x));
    return ret^63;
}
#else
static const uint8_t av_log2_tab[256]={
    0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
static inline unsigned int av_log2(unsigned int v)
{
    unsigned int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += av_log2_tab[v];
    return n;
}
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simple pool-based allocator
 */
void *mempool_alloc_small(size_t sizeofobject, unsigned int pool_id);
void mempool_free_small(unsigned int pool_id);
void *mempool_alloc_large(size_t sizeofobject);
void *mempool_realloc_large (void *ptr, size_t old_size, size_t new_size);
void mempool_free_large(void *ptr, size_t sizeofobject);

/**
 * Initialize the cosine table in ff_cos_tabs[index]
 * \param index index in ff_cos_tabs array of the table to initialize
 */
void ff_init_ff_cos_tabs(int index);

/**
 * Set up a complex FFT.
 * @param nbits           log2 of the length of the input array
 * @param inverse         if 0 perform the forward transform, if 1 perform the inverse
 */
float fft_cosf(float x);
float fft_sinf(float x);
int ff_fft_init(FFTContext *s, uint32_t nbits);
void ff_fft_cleanup(FFTContext *s);
void ff_rdft_init_sine_table(float *tsin, uint32_t nbits);
void MD5(unsigned char *dst, const unsigned char *src, unsigned int len);
void ff_imdct_postrotate_sse(FFTComplex *z, const float *tcos, unsigned int nbits);
void ff_imdct_postrotate_neon(FFTComplex *z, const float *tcos, unsigned int nbits);
void ff_mdct_postrotate_sse(FFTComplex *z, const float *tcos, unsigned int nbits);
void ff_mdct_postrotate_neon(FFTComplex *z, const float *tcos, unsigned int nbits);
void ff_rdft_transform(uint32_t nbits, float *data, uint32_t inverse, float *tsin);
void ff_rdft_transform_sse(uint32_t nbits, float *data, uint32_t inverse, float *tsin);
void ff_rdft_transform_neon(uint32_t nbits, float *data, uint32_t inverse, float *tsin);
void ff_rdft_calc(FFTContext *s, float *data_out, float *data_in, uint8_t inverse);
void ff_mdct_init(float *tcos, unsigned int nbits, float scale);
void ff_imdct_half(FFTContext *s, float *output, const float *input, float *tcos);
void ff_mdct_calc(FFTContext *s, float *out, const float *input, float *tcos);
void ff_fft_calc_3dn2(FFTComplex *z, uint32_t nbits);
void ff_fft_calc_interleave_3dn2(FFTComplex *z, uint32_t nbits);
void ff_fft_calc_sse(FFTComplex *z, uint32_t nbits);
void ff_fft_calc_interleave_sse(FFTComplex *z, uint32_t nbits);
void ff_fft_calc_c(FFTComplex *z, uint32_t nbits);
void ff_fft_calc_neon(FFTComplex *z, uint32_t nbits);

static inline void ff_rdft_init(FFTContext *s, uint32_t nbits)
{
    unsigned int n = (1 << nbits);
    s->tsin = (float*)mempool_alloc_small(n * sizeof(float), 0);
    ff_fft_init(s, nbits-1);
    ff_rdft_init_sine_table(s->tsin, nbits-1);
}

/**
 * Do a complex FFT with the parameters defined in ff_fft_init(). The
 * input data must be permuted before. No 1.0/sqrt(n) normalization is done.
 */
extern void (*ff_fft_calc)(FFTComplex *z, uint32_t nbits);
extern void (*ff_fft_calc_noninterleaved)(FFTComplex *z, uint32_t nbits);
extern void (*ff_imdct_postrotate)(FFTComplex *z, const float *tcos, unsigned int nbits);
extern void (*ff_mdct_postrotate)(FFTComplex *z, const float *tcos, unsigned int nbits);

/**
 * Special-cased transforms used in MP3 only.
 */
void dct32_sse(float *out, const float *tab);
void imdct36_sse(float *out, float *in);
void imdct36_neon(float *out, float *in);

/**
 * DSP utility functions.
 */
void vector_fmul_window_sse(float *dst, const float *src0, const float *src1, const float *win, int len);
void vector_fmul_sse(float *dst, const float *src0, const float *src1, int len);
void vector_fmul_scalar_sse(float *dst, const float *src, float mul, int len);
void vector_fmul_reverse_sse(float *dst, const float *src0, const float *src1, int len);
float scalarproduct_float_sse(const float *v1, const float *v2, unsigned int len);
void butterflies_float_sse(float *src0, float *src1, unsigned int len);

void vector_fmul_window_neon(float *dst, const float *src0, const float *src1, const float *win, int len);
void vector_fmul_neon(float *dst, const float *src0, const float *src1, int len);
void vector_fmul_scalar_neon(float *dst, const float *src, float mul, int len);
void vector_fmul_reverse_neon(float *dst, const float *src0, const float *src1, int len);
float scalarproduct_float_neon(const float *v1, const float *v2, unsigned int len);
void butterflies_float_neon(float *src0, float *src1, unsigned int len);

void sbr_sum64x5_sse(float *z);
void sbr_qmf_pre_shuffle_sse(float *z);
void sbr_qmf_post_shuffle_sse(float W[32][2], float *z);
void sbr_qmf_deint_bfly_sse(float *v, const float *src0, const float *src1);
void sbr_hf_g_filt_sse(float (*Y)[2], float (*X_high)[40][2],
                       const float *g_filt, size_t m_max, size_t ixh);
void vector_fmul_add_sse(float *dst, const float *src0, const float *src1,
                         const float *src2, int len);
void vector_fmul_copy_sse(float *dst, const float *src, int len);

void sbr_sum64x5_neon(float *z);
void sbr_qmf_pre_shuffle_neon(float *z);
void sbr_qmf_post_shuffle_neon(float W[32][2], const float *z);
void sbr_qmf_deint_bfly_neon(float *v, const float *src0, const float *src1);
void sbr_hf_g_filt_neon(float (*Y)[2], float (*X_high)[40][2],
                        const float *g_filt, size_t m_max, size_t ixh);
void vector_fmul_add_neon(float *dst, const float *src0, const float *src1,
                          const float *src2, int len);
void vector_fmul_copy_neon(float *dst, const float *src, int len);

typedef struct AudioFifo {
    float *buffer;
    float *rptr, *wptr, *end;
    uint32_t bufsize, rndx, wndx;
} AudioFifo;

void fifo_alloc(AudioFifo *f, unsigned int size);
void fifo_free(AudioFifo *f);
void fifo_reset(AudioFifo *f);
unsigned int fifo_write(AudioFifo *f, float *src, unsigned int size);
unsigned int fifo_write_interleave(AudioFifo *f, float *buf[2], unsigned int size, unsigned int ch);
unsigned int fifo_read(AudioFifo *f, void *dest, unsigned int buf_size);

/* Returns how many floats there are available currently for reading in the FIFO. */
static inline unsigned int fifo_size(AudioFifo *f)
{
    return (uint32_t)(f->wndx - f->rndx);
}

/* Returns how many more floats can be written to the FIFO currently. */
static inline unsigned int fifo_space(AudioFifo *f)
{
    return f->end - f->buffer - fifo_size(f);
}

/* Hilariously silly dB <-> linear conversions using floating point hacks. Accurate to ~1% or thereabouts. */
typedef union _f32 {
    float f;
    uint32_t i;
} _f32;

static inline float to_dB(float in){
  _f32 tmp;
  tmp.f = in;
  tmp.i &= 0x7fffffff;
  return ((float)(tmp.i * 3.58855719e-7f - 382.3080943f));
}

static inline float af_from_dB(float in){
  _f32 tmp;
  if (in < -200.0f) return 0.0f;
  tmp.i = (1.39331762961e+06f*(in+764.6161886f));
  return tmp.f;
}

#ifdef __cplusplus
};
#endif

#endif /* AVCODEC_FFT_H */

