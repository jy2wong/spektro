/*
 * FFT/IFFT transforms
 * Copyright (c) 2008 Loren Merritt
 * Copyright (c) 2002 Fabrice Bellard
 * Partly based on libdjbfft by D. J. Bernstein
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

/**
 * @file
 * FFT/IFFT transforms.
 */

#include <stdlib.h>
#include <string.h>
#include "ffmpeg_fft.h"
#define x86_reg uint64_t
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm__ volatile\
        ("cpuid\n\t"\
         : "=a" (eax), "=b" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

/* cos(2*pi*x/n) for 0<=x<=n/4, followed by its reverse */
COSTABLE(16);
COSTABLE(32);
COSTABLE(64);
COSTABLE(128);
COSTABLE(256);
COSTABLE(512);
COSTABLE(1024);
COSTABLE(2048);
COSTABLE(4096);
COSTABLE(8192);
COSTABLE(16384);
COSTABLE(32768);
COSTABLE(65536);
float * const ff_cos_tabs[] = {
    NULL, NULL, NULL, NULL,
    ff_cos_16, ff_cos_32, ff_cos_64, ff_cos_128, ff_cos_256, ff_cos_512, ff_cos_1024,
    ff_cos_2048, ff_cos_4096, ff_cos_8192, ff_cos_16384, ff_cos_32768, ff_cos_65536
};

int split_radix_permutation(int i, int n, int inverse)
{
    int m;
    if(n <= 2) return i&1;
    m = n >> 1;
    if(!(i&m))            return split_radix_permutation(i, m, inverse)*2;
    m >>= 1;
    if(inverse == !(i&m)) return split_radix_permutation(i, m, inverse)*4 + 1;
    else                  return split_radix_permutation(i, m, inverse)*4 - 1;
}

static const float
C1  =  4.1666667908e-02, /* 0x3d2aaaab */
C2  = -1.3888889225e-03, /* 0xbab60b61 */
C3  =  2.4801587642e-05, /* 0x37d00d01 */
C4  = -2.7557314297e-07, /* 0xb493f27c */
C5  =  2.0875723372e-09, /* 0x310f74f6 */
C6  = -1.1359647598e-11; /* 0xad47d74e */

// Differs from libc cosf on [0, pi/2] by at most 0.0000001229f
// Differs from libc cosf on [0, pi] by at most 0.0000035763f
float fft_cosf(float x)
{
    float z,r;
    z  = x*x;
    r  = z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))));
    return 1.0f - z*(0.5f - r);
}

static const float
S1  = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
S2  =  8.33333333332248946124e-03, /* 0x3F811111, 0x1110F8A6 */
S3  = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
S4  =  2.75573137070700676789e-06, /* 0x3EC71DE3, 0x57B1FE7D */
S5  = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
S6  =  1.58969099521155010221e-10; /* 0x3DE5D93A, 0x5ACFD57C */

// Differs from libc sinf on [0, pi/2] by at most 0.0000001192f
// Differs from libc sinf on [0, pi] by at most 0.0000170176f
float fft_sinf(float x)
{
    float z,r;
    z =  x*x;
    r = z*(S1+z*(S2+z*(S3+z*(S4+z*(S5+z*S6)))));
    return x*(1.0f+r);
}   

void ff_init_ff_cos_tabs(int index)
{
    int i;
    int m = 1<<index;
    float freq = 2*(float)M_PI/m;
    float *tab = ff_cos_tabs[index];
    for(i=0; i<=m/4; i++)
        tab[i] = fft_cosf(i*freq);
    for(i=1; i<m/4; i++)
        tab[m/2-i] = tab[i];
}

/* sin(2*pi*x/n) for 0<=x<n/4, followed by n/2<=x<3n/4 */
void ff_rdft_init_sine_table(float *tsin, uint32_t nbits)
{
    uint32_t i, n = 1 << nbits;
    float theta = (float)M_PI/n;
    for (i = 0; i < (n>>1); i++) {
        tsin[i]        = fft_cosf((float)M_PI/2.0f + (i*theta));
        tsin[i+(n>>1)] = fft_cosf((float)M_PI/2.0f - (i*theta));
    }
}

/** Map one real FFT into two parallel real even and odd FFTs. Then interleave
 * the two real FFTs into one complex FFT. Unmangle the results.
 * ref: http://www.engineeringproductivitytools.com/stuff/T0001/PT10.HTM
 */
void ff_rdft_transform(FFTContext *s, float *data, float *tsin)
{
    unsigned int i, i1;
    FFTComplex ev, od;
    const unsigned int n = 1 << (s->nbits+1);
    const float k1 = 0.5f;
    const float k2 = 0.5f - s->inverse;
    const float *tcos = ff_cos_tabs[s->nbits+1];

    /* i=0 is a special case because of packing, the DC term is real, so we
       are going to throw the N/2 term (also real) in with it. */
    ev.re = data[0];
    data[0] = ev.re+data[1];
    data[1] = ev.re-data[1];
    for (i = 1; i < (n>>2); i++) {
        i1 = n-2*i;
        /* Separate even and odd FFTs */
        ev.re =  k1*(data[2*i  ]+data[i1  ]);
        od.im = -k2*(data[2*i  ]-data[i1  ]);
        ev.im =  k1*(data[2*i+1]-data[i1+1]);
        od.re =  k2*(data[2*i+1]+data[i1+1]);
        /* Apply twiddle factors to the odd FFT and add to the even FFT */
        data[2*i  ] =  ev.re + od.re*tcos[i] - od.im*tsin[i];
        data[2*i+1] =  ev.im + od.im*tcos[i] + od.re*tsin[i];
        data[i1   ] =  ev.re - od.re*tcos[i] + od.im*tsin[i];
        data[i1+1 ] = -ev.im + od.im*tcos[i] + od.re*tsin[i];
    }
    data[2*i+1] = -data[2*i+1];
    if (s->inverse) {
        data[0] *= k1;
        data[1] *= k1;
    }
}

void ff_rdft_calc(FFTContext *s, float *data_out, float *data_in, float *tsin)
{
    unsigned int i;
    unsigned int n = (1 << s->nbits), n2 = (n >> 1);
    FFTComplex *z1 = (FFTComplex*)data_in, *z2 = (FFTComplex *)data_out;
    float *data = (s->inverse ? data_in : data_out);
    float *tsin_ptr = (s->inverse ? tsin+n2 : tsin);   
    if (!s->inverse) {
        for(i=0;i<n;i++) z2[s->revtab[i]] = z1[i];
        ff_fft_calc((FFTComplex*)data_out, s->nbits);
    }
    ff_rdft_transform(s, data, tsin_ptr);
    if (s->inverse) {
        for(i=0;i<n;i++) z2[s->revtab[i]] = z1[i];
        ff_fft_calc((FFTComplex*)data_out, s->nbits);
    }
}

/**
 * Init MDCT pre/post rotation tables.
 */
void ff_mdct_init(float *tcos, unsigned int nbits, float scale)
{
    int n, n4, i;
    float alpha, theta;
    n = 1 << nbits;
    n4 = n >> 2;

    theta = 1.0f / 8.0f + (scale < 0 ? n4 : 0);
    scale = sqrtf(fabsf(scale));
    for(i=0;i<n4;i++) {
        alpha = 2.0f * (float)M_PI * (i + theta) / n;
        tcos[2*i] = fft_cosf(alpha) * scale;
    }
    for(i=0;i<n4;i++) {
        alpha = (float)M_PI/2.0f - (2.0f * (float)M_PI * (i + theta) / n);
        tcos[2*i+1] = fft_cosf(alpha) * scale;
    }
}

/**
 * Compute the middle half of the inverse MDCT of size N = 2^nbits,
 * thus excluding the parts that can be derived by symmetry
 * @param output N/2 samples
 * @param input N/2 samples
 */
void ff_imdct_half(FFTContext *s, float *output, const float *input, float *tcos)
{
    int k, n2, n, mdct_bits, j;
    const uint16_t *revtab = s->revtab;
    const float *in1, *in2;
    FFTComplex *z = (FFTComplex *)output;

    mdct_bits = s->nbits + 1;
    n = 1 << mdct_bits;
    n2 = n >> 1;

    /* pre rotation */
    in1 = input;
    in2 = input + n - 1;
    for(k = 0; k < n2; k++) {
        j=revtab[k];
        z[j].re = *in2 * tcos[2*k  ] - *in1 * tcos[2*k+1];
        z[j].im = *in2 * tcos[2*k+1] + *in1 * tcos[2*k];
        in1 += 2;
        in2 -= 2;
    }
    ff_fft_calc_noninterleaved(z, s->nbits);
    ff_imdct_postrotate(z, tcos, n);
}

/* complex multiplication: p = a * b */
#undef CMUL
#define CMUL(pre, pim, are, aim, bre, bim) \
{\
    float _are = (are);\
    float _aim = (aim);\
    float _bre = (bre);\
    float _bim = (bim);\
    (pre) = _are * _bre - _aim * _bim;\
    (pim) = _are * _bim + _aim * _bre;\
}

/**
 * Compute MDCT of size N = 2^nbits
 * @param input N samples
 * @param out N/2 samples
 */
void ff_mdct_calc(FFTContext *s, float *out, const float *input, float *tcos)
{
    int i, j, n, n8, n4, n2, n3;
    float re, im;
    const uint16_t *revtab = s->revtab;
    FFTComplex *x = (FFTComplex *)out;
    int mdct_bits;

    mdct_bits = s->nbits + 2;
    n = 1 << mdct_bits;
    n2 = n >> 1;
    n4 = n >> 2;
    n8 = n >> 3;
    n3 = 3 * n4;

    /* pre rotation */
    for(i=0;i<n8;i++) {
        re = (-input[2*i+n3] - input[n3-1-2*i]);
        im = (-input[n4+2*i] + input[n4-1-2*i]);
        j = revtab[i];
        CMUL(x[j].re, x[j].im, re, im, -tcos[2*i], tcos[2*i+1]);

        re = ( input[2*i]    - input[n2-1-2*i]);
        im = (-input[n2+2*i] - input[ n-1-2*i]);
        j = revtab[n8 + i];
        CMUL(x[j].re, x[j].im, re, im, -tcos[2*(n8 + i)], tcos[2*(n8 + i)+1]);
    }
    ff_fft_calc_noninterleaved(x, s->nbits);
    ff_mdct_postrotate(x, tcos, n2);
}

/* nbits = 4, 6, 9 */
void (*ff_fft_calc)(FFTComplex *z, uint32_t nbits);
void (*ff_fft_calc_noninterleaved)(FFTComplex *z, uint32_t nbits);
void (*ff_imdct_postrotate)(FFTComplex *z, const float *tcos, unsigned int nbits);
void (*ff_mdct_postrotate)(FFTComplex *z, const float *tcos, unsigned int nbits);

int ff_fft_init(FFTContext *s, uint32_t nbits, uint8_t inverse)
{
    int i, j, n;
    //int eax, ebx, ecx;
    //int std_flags = 0, ext_flags = 0;

    s->nbits = nbits;
    s->inverse = inverse;
    n = 1 << nbits;

    //cpuid(1, eax, ebx, ecx, std_flags);
    //cpuid(0x80000001, eax, ebx, ecx, ext_flags);

    //if (std_flags & (1<<25)) {
    	ff_fft_calc = ff_fft_calc_interleave_sse;
    	ff_fft_calc_noninterleaved = ff_fft_calc_sse;
        ff_imdct_postrotate = ff_imdct_postrotate_sse;
        ff_mdct_postrotate = ff_mdct_postrotate_sse;
    /* } else if (ext_flags & (1<<31)) {
    	ff_fft_calc = ff_fft_calc_3dn2;
	} else {
    	ff_fft_calc = ff_fft_calc_c;
	} */

    for(j=4; j<=nbits; j++) {
        ff_init_ff_cos_tabs(j);
    }
    for(i=0; i<n; i++) {
        s->revtab[-split_radix_permutation(i, n, s->inverse) & (n-1)] = i;
	}	
    return 0;
}

