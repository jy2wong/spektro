;******************************************************************************
;* FFT transform with SSE/3DNow optimizations
;* Copyright (c) 2008 Loren Merritt
;*
;* This algorithm (though not any of the implementation details) is
;* based on libdjbfft by D. J. Bernstein.
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or
;* modify it under the terms of the GNU Lesser General Public
;* License as published by the Free Software Foundation; either
;* version 2.1 of the License, or (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;* Lesser General Public License for more details.
;*
;* You should have received a copy of the GNU Lesser General Public
;* License along with FFmpeg; if not, write to the Free Software
;* 51, Inc., Foundation Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;******************************************************************************

; These functions are not individually interchangeable with the C versions.
; While C takes arrays of FFTComplex, SSE/3DNow leave intermediate results
; in blocks as conventient to the vector size.
; i.e. {4x real, 4x imaginary, 4x real, ...} (or 2x respectively)

%include "x86inc.asm"

SECTION_RODATA 16  

%define M_SQRT1_2 0.70710678118654752440
ps_root2: times 4 dd M_SQRT1_2
ps_root2mppm: dd -M_SQRT1_2, M_SQRT1_2, M_SQRT1_2, -M_SQRT1_2
ps_80000000: times 4 dd 0x80000000

align 16
ps_cos16: dd 1.0, 0.9238795042, 0.7071067095, 0.3826834559, 0.0, 0.3826834559, 0.7071067095, 0.9238795042

align 16
ps_mask:  dd 0, ~0,  0, ~0
ps_mask2: dd 0,  0,  0, ~0
ps_mask3: dd 0, ~0,  0,  0

ps_imdct36_val:   dd          -0.5,          -0.5, -0.8660254038, -0.8660254038
          dd           1.0,           1.0,  0.8660254038,  0.8660254038
          dd  0.1736481777,  0.1736481777,  0.3420201433,  0.3420201433
          dd -0.7660444431, -0.7660444431,  0.8660254038,  0.8660254038
          dd -0.9396926208, -0.9396926208, -0.9848077530, -0.9848077530
          dd           0.5,           0.5, -0.6427876097, -0.6427876097
          dd           1.0,           1.0, -0.6427876097, -0.6427876097

ps_p1p1m1m1: dd 0,          0, 0x80000000, 0x80000000
ps_p1m1p1m1: dd 0, 0x80000000,          0, 0x80000000

ps_imdct36_cos:  dd 1.0, 0.50190991877167369479, 1.0, 5.73685662283492756461
          dd 1.0, 0.51763809020504152469, 1.0, 1.93185165257813657349
          dd 1.0, 0.55168895948124587824, 1.0, 1.18310079157624925896
          dd 1.0, 0.61038729438072803416, 1.0, 0.87172339781054900991
          dd 1.0, 0.70710678118654752439, 0.0, 0.0

ps_half: dd 0.5

%assign i 16
%rep 13
cextern ff_cos_ %+ i
%assign i i<<1
%endrep

%ifdef ARCH_X86_64
    %define pointer dq
%else
    %define pointer dd
%endif

%macro IF0 1+
%endmacro
%macro IF1 1+
    %1
%endmacro

section .text align=16

; in:  %1={r0,i0,r1,i1} %2={r2,i2,r3,i3}
; out: %1={r0,r1,r2,r3} %2={i0,i1,i2,i3}
%macro T4_SSE 3
    movaps   %3, %1
    shufps   %1, %2, 0x64 ; {r0,i0,r3,i2}
    shufps   %3, %2, 0xce ; {r1,i1,r2,i3}
    movaps   %2, %1
    addps    %1, %3       ; {t1,t2,t6,t5}
    subps    %2, %3       ; {t3,t4,t8,t7}
    movaps   %3, %1
    shufps   %1, %2, 0x44 ; {t1,t2,t3,t4}
    shufps   %3, %2, 0xbe ; {t6,t5,t7,t8}
    movaps   %2, %1
    addps    %1, %3       ; {r0,i0,r1,i1}
    subps    %2, %3       ; {r2,i2,r3,i3}
    movaps   %3, %1
    shufps   %1, %2, 0x88 ; {r0,r1,r2,r3}
    shufps   %3, %2, 0xdd ; {i0,i1,i2,i3}
    SWAP     %2, %3
%endmacro

; in:  %1={r0,i0,r1,i1} %2={r2,i2,r3,i3}
; out: %1={r0,r1,r2,r3} %2={i0,i1,i2,i3}
%macro T4_NONINT_SSE 3
    movaps   %3, %1
    shufps   %1, %2, 0x64 ; {r0,i0,r3,i2}
    shufps   %3, %2, 0xce ; {r1,i1,r2,i3}
    movaps   %2, %1
    addps    %1, %3       ; {t1,t2,t6,t5}
    subps    %2, %3       ; {t3,t4,t8,t7}
    movaps   %3, %1
    shufps   %1, %2, 0x44 ; {t1,t2,t3,t4}
    shufps   %3, %2, 0xbe ; {t6,t5,t7,t8}
    movaps   %2, %1
    addps    %1, %3       ; {r0,i0,r1,i1}
    subps    %2, %3       ; {r2,i2,r3,i3}
%endmacro

%macro T8_SSE 6 ; r0,i0,r1,i1,t0,t1
    movaps   %5, %3
    shufps   %3, %4, 0x44 ; {r4,i4,r6,i6}
    shufps   %5, %4, 0xee ; {r5,i5,r7,i7}
    movaps   %6, %3
    subps    %3, %5       ; {r5,i5,r7,i7}
    addps    %6, %5       ; {t1,t2,t3,t4}
    movaps   %5, %3
    shufps   %5, %5, 0xb1 ; {i5,r5,i7,r7}
    mulps    %3, [ps_root2mppm] ; {-r5,i5,r7,-i7}
    mulps    %5, [ps_root2]
    addps    %3, %5       ; {t8,t7,ta,t9}
    movaps   %5, %6
    shufps   %6, %3, 0x36 ; {t3,t2,t9,t8}
    shufps   %5, %3, 0x9c ; {t1,t4,t7,ta}
    movaps   %3, %6
    addps    %6, %5       ; {t1,t2,t9,ta}
    subps    %3, %5       ; {t6,t5,tc,tb}
    movaps   %5, %6
    shufps   %6, %3, 0xd8 ; {t1,t9,t5,tb}
    shufps   %5, %3, 0x8d ; {t2,ta,t6,tc}
    movaps   %3, %1
    movaps   %4, %2
    addps    %1, %6       ; {r0,r1,r2,r3} ; t5, t6, tb, tc
    addps    %2, %5       ; {i0,i1,i2,i3}
    subps    %3, %6       ; {r4,r5,r6,r7}
    subps    %4, %5       ; {i4,i5,i6,i7}
%endmacro

%macro T8_NONINT_SSE 6 ; r0,i0,r1,i1,t0,t1
    movaps   %5, %3
    shufps   %3, %4, 0x44 ; {r4,i4,r6,i6}
    shufps   %5, %4, 0xee ; {r5,i5,r7,i7}
    movaps   %6, %3
    subps    %3, %5       ; {r5,i5,r7,i7}
    addps    %6, %5       ; {t1,t2,t3,t4}
    movaps   %5, %3
    shufps   %5, %5, 0xb1 ; {i5,r5,i7,r7}
    mulps    %3, [ps_root2mppm] ; {-r5,i5,r7,-i7}
    mulps    %5, [ps_root2]
    addps    %3, %5       ; {t8,t7,ta,t9}
    movaps   %5, %6
    shufps   %6, %3, 0x36 ; {t3,t2,t9,t8}
    shufps   %5, %3, 0x9c ; {t1,t4,t7,ta}
    movaps   %3, %6
    addps    %6, %5       ; {t1,t2,t9,ta}
    subps    %3, %5       ; {t6,t5,tc,tb}
    shufps   %3, %3, 0xb1
    movaps   %5, %3
    movaps   %3, %1
    movaps   %4, %2
    addps    %1, %6       ; {r0,i0,r1,i1}
    addps    %2, %5       ; {r2,i2,r3,i3}
    subps    %3, %6       ; {r4,r5,r6,r7}
    subps    %4, %5       ; {i4,i5,i6,i7}
%endmacro

; scheduled for cpu-bound sizes
%macro PASS_SMALL 3 ; (to load m4-m7), wre, wim
IF%1 mova    m4, Z(4)
IF%1 mova    m5, Z(5)
    mova     m0, %2 ; wre
    mova     m2, m4
    mova     m1, %3 ; wim
    mova     m3, m5
    mulps    m2, m0 ; r2*wre
IF%1 mova    m6, Z(6)
    mulps    m3, m1 ; i2*wim
IF%1 mova    m7, Z(7)
    mulps    m4, m1 ; r2*wim
    mulps    m5, m0 ; i2*wre
    addps    m2, m3 ; r2*wre + i2*wim
    mova     m3, m1
    mulps    m1, m6 ; r3*wim
    subps    m5, m4 ; i2*wre - r2*wim
    mova     m4, m0
    mulps    m3, m7 ; i3*wim
    mulps    m4, m6 ; r3*wre
    mulps    m0, m7 ; i3*wre
    subps    m4, m3 ; r3*wre - i3*wim
    mova     m3, Z(0)
    addps    m0, m1 ; i3*wre + r3*wim
    mova     m1, m4
    addps    m4, m2 ; t5
    subps    m1, m2 ; t3
    subps    m3, m4 ; r2
    addps    m4, Z(0) ; r0
    mova     m6, Z(2)
    mova   Z(4), m3
    mova   Z(0), m4
    mova     m3, m5
    subps    m5, m0 ; t4
    mova     m4, m6
    subps    m6, m5 ; r3
    addps    m5, m4 ; r1
    mova   Z(6), m6
    mova   Z(2), m5
    mova     m2, Z(3)
    addps    m3, m0 ; t6
    subps    m2, m1 ; i3
    mova     m7, Z(1)
    addps    m1, Z(3) ; i1
    mova   Z(7), m2
    mova   Z(3), m1
    mova     m4, m7
    subps    m7, m3 ; i2
    addps    m3, m4 ; i0
    mova   Z(5), m7
    mova   Z(1), m3
%endmacro

; scheduled to avoid store->load aliasing
%macro PASS_BIG 1 ; (!interleave)
    mova     m4, Z(4) ; r2
    mova     m5, Z(5) ; i2
    mova     m2, m4
    mova     m0, [wq] ; wre
    mova     m3, m5
    mova     m1, [wq+o1q] ; wim
    mulps    m2, m0 ; r2*wre
    mova     m6, Z(6) ; r3
    mulps    m3, m1 ; i2*wim
    mova     m7, Z(7) ; i3
    mulps    m4, m1 ; r2*wim
    mulps    m5, m0 ; i2*wre
    addps    m2, m3 ; r2*wre + i2*wim
    mova     m3, m1
    mulps    m1, m6 ; r3*wim
    subps    m5, m4 ; i2*wre - r2*wim
    mova     m4, m0
    mulps    m3, m7 ; i3*wim
    mulps    m4, m6 ; r3*wre
    mulps    m0, m7 ; i3*wre
    subps    m4, m3 ; r3*wre - i3*wim
    mova     m3, Z(0)
    addps    m0, m1 ; i3*wre + r3*wim
    mova     m1, m4
    addps    m4, m2 ; t5
    subps    m1, m2 ; t3
    subps    m3, m4 ; r2
    addps    m4, Z(0) ; r0
    mova     m6, Z(2)
    mova   Z(4), m3
    mova   Z(0), m4
    mova     m3, m5
    subps    m5, m0 ; t4
    mova     m4, m6
    subps    m6, m5 ; r3
    addps    m5, m4 ; r1
IF%1 mova  Z(6), m6
IF%1 mova  Z(2), m5
    mova     m2, Z(3)
    addps    m3, m0 ; t6
    subps    m2, m1 ; i3
    mova     m7, Z(1)
    addps    m1, Z(3) ; i1
IF%1 mova  Z(7), m2
IF%1 mova  Z(3), m1
    mova     m4, m7
    subps    m7, m3 ; i2
    addps    m3, m4 ; i0
IF%1 mova  Z(5), m7
IF%1 mova  Z(1), m3
%if %1==0
    mova     m4, m5 ; r1
    mova     m0, m6 ; r3
    unpcklps m5, m1
    unpckhps m4, m1
    unpcklps m6, m2
    unpckhps m0, m2
    mova     m1, Z(0)
    mova     m2, Z(4)
    mova   Z(2), m5
    mova   Z(3), m4
    mova   Z(6), m6
    mova   Z(7), m0
    mova     m5, m1 ; r0
    mova     m4, m2 ; r2
    unpcklps m1, m3
    unpckhps m5, m3
    unpcklps m2, m7
    unpckhps m4, m7
    mova   Z(0), m1
    mova   Z(1), m5
    mova   Z(4), m2
    mova   Z(5), m4
%endif
%endmacro

%macro PUNPCK 3
    mova      %3, %1
    punpckldq %1, %2
    punpckhdq %3, %2
%endmacro

INIT_XMM
%define mova movaps
%xdefine SUFFIX _sse

%define Z(x) [r0+mmsize*x]

align 16
[global fft4_sse]
fft4_sse:
    movaps   m0, Z(0)
    movaps   m1, Z(1)
    T4_SSE   m0, m1, m2
    movaps Z(0), m0
    movaps Z(1), m1
    ret
cendfunc fft4

align 16
[global fft4_interleave_sse]
fft4_interleave_sse:
    movaps    m0, Z(0)
    movaps    m1, Z(1)
    T4_NONINT_SSE   m0, m1, m2
    movaps Z(0), m0
    movaps Z(1), m1
    ret
cendfunc fft4_interleave

align 16
[global fft8_sse]
fft8_sse:
    movaps   m0, Z(0)
    movaps   m1, Z(1)
    T4_SSE   m0, m1, m2
    movaps   m2, Z(2)
    movaps   m3, Z(3)
    T8_SSE   m0, m1, m2, m3, m4, m5
    movaps Z(0), m0
    movaps Z(1), m1
    movaps Z(2), m2
    movaps Z(3), m3
    ret
cendfunc fft8

align 16
[global fft8_interleave_sse]
fft8_interleave_sse:
    movaps   m0, Z(0)
    movaps   m1, Z(1)
    T4_NONINT_SSE   m0, m1, m2
    movaps   m2, Z(2)
    movaps   m3, Z(3)
    T8_NONINT_SSE   m0, m1, m2, m3, m4, m5
    movaps Z(0), m0
    movaps Z(1), m1
    movaps Z(2), m2
    movaps Z(3), m3
    ret
cendfunc fft8_interleave

align 16
[global fft16_sse]
fft16_sse:
    movaps   m0, Z(0)
    movaps   m1, Z(1)
    T4_SSE   m0, m1, m2
    movaps   m2, Z(2)
    movaps   m3, Z(3)
    T8_SSE   m0, m1, m2, m3, m4, m5
    movaps   m4, Z(4)
    movaps   m5, Z(5)
    movaps Z(0), m0
    movaps Z(1), m1
    movaps Z(2), m2
    movaps Z(3), m3
    T4_SSE   m4, m5, m6
    movaps   m6, Z(6)
    movaps   m7, Z(7)
    T4_SSE   m6, m7, m0
    ;movaps   m0, [ff_cos_16] ; wre
    movaps   m0, [ps_cos16]
    movaps   m2, m4
    ;movaps   m1, [ff_cos_16+16] ; wim
    movaps   m1, [ps_cos16+16]
    movaps   m3, m5
    mulps    m2, m0 ; r2*wre
    mulps    m3, m1 ; i2*wim
    mulps    m4, m1 ; r2*wim
    mulps    m5, m0 ; i2*wre
    addps    m2, m3 ; r2*wre + i2*wim
    movaps   m3, m1
    mulps    m1, m6 ; r3*wim
    subps    m5, m4 ; i2*wre - r2*wim
    movaps   m4, m0
    mulps    m3, m7 ; i3*wim
    mulps    m4, m6 ; r3*wre
    mulps    m0, m7 ; i3*wre
    subps    m4, m3 ; r3*wre - i3*wim
    addps    m0, m1 ; i3*wre + r3*wim
    movaps   m1, m4
    addps    m4, m2 ; t5
    subps    m1, m2 ; t3
    movaps   m3, m5
    subps    m5, m0 ; t4
    addps    m3, m0 ; t6

    movaps   m0, Z(0)
    movaps   m7, Z(1)
    movaps   m6, m0
    subps    m0, m4 ; r2
    addps    m4, m6 ; r0
    movaps   m6, m7
    subps    m7, m3 ; i2
    addps    m3, m6 ; i0
    movaps Z(0), m4
    movaps Z(1), m3
    movaps Z(4), m0
    movaps Z(5), m7

    movaps   m6, Z(2)
    movaps   m2, Z(3)
    movaps   m4, m6
    movaps   m3, m2
    subps    m6, m5 ; r3
    addps    m5, m4 ; r1
    subps    m2, m1 ; i3
    addps    m1, m3 ; i1
    movaps Z(2), m5
    movaps Z(3), m1
    movaps Z(6), m6
    movaps Z(7), m2
    ret
cendfunc fft16

align 16
[global fft16_interleave_sse]
fft16_interleave_sse:
    movaps   m0, Z(0)
    movaps   m1, Z(1)
    T4_SSE   m0, m1, m2
    ;T4_NONINT_SSE   m0, m1, m2
    movaps   m2, Z(2)
    movaps   m3, Z(3)
    T8_SSE   m0, m1, m2, m3, m4, m5
    ;T8_NONINT_SSE   m0, m1, m2, m3, m4, m5
    movaps   m4, Z(4)
    movaps   m5, Z(5)
    movaps Z(0), m0
    movaps Z(1), m1
    movaps Z(2), m2
    movaps Z(3), m3
    T4_SSE   m4, m5, m6
    movaps   m6, Z(6)
    movaps   m7, Z(7)
    T4_SSE   m6, m7, m0
    ;movaps   m0, [ff_cos_16] ; wre
    movaps   m0, [ps_cos16]
    movaps   m2, m4
    ;movaps   m1, [ff_cos_16+16] ; wim
    movaps   m1, [ps_cos16+16]
    movaps   m3, m5
    mulps    m2, m0 ; r2*wre
    mulps    m3, m1 ; i2*wim
    mulps    m4, m1 ; r2*wim
    mulps    m5, m0 ; i2*wre
    addps    m2, m3 ; r2*wre + i2*wim
    movaps   m3, m1
    mulps    m1, m6 ; r3*wim
    subps    m5, m4 ; i2*wre - r2*wim
    movaps   m4, m0
    mulps    m3, m7 ; i3*wim
    mulps    m4, m6 ; r3*wre
    mulps    m0, m7 ; i3*wre
    subps    m4, m3 ; r3*wre - i3*wim
    addps    m0, m1 ; i3*wre + r3*wim
    movaps   m1, m4
    addps    m4, m2 ; t5
    subps    m1, m2 ; t3
    movaps   m3, m5
    subps    m5, m0 ; t4
    addps    m3, m0 ; t6

    movaps   m0, Z(0)
    movaps   m7, Z(1)
    movaps   m6, m0
    subps    m0, m4 ; r2
    addps    m4, m6 ; r0
    movaps   m6, m7
    subps    m7, m3 ; i2
    addps    m3, m6 ; i0
	movaps	 m6, m4
	unpcklps m4, m3
	unpckhps m6, m3
	movaps	 m3, m0
	unpcklps m0, m7
	unpckhps m3, m7
    movaps Z(0), m4
    movaps Z(1), m6
    movaps Z(4), m0
    movaps Z(5), m3

    movaps   m6, Z(2)
    movaps   m2, Z(3)
    movaps   m4, m6
    movaps   m3, m2
    subps    m6, m5 ; r3
    addps    m5, m4 ; r1
    subps    m2, m1 ; i3
    addps    m1, m3 ; i1
	movaps	 m7, m5
	unpcklps m5, m1
	unpckhps m7, m1
	movaps	 m4, m6
	unpcklps m6, m2
	unpckhps m4, m2
    movaps Z(2), m5
    movaps Z(3), m7
    movaps Z(6), m6
    movaps Z(7), m4
    ret
cendfunc fft16_interleave

%define Z(x) [zq + o1q*(x&6)*((x/6)^1) + o3q*(x/6) + mmsize*(x&1)]

%macro DECL_PASS 2+ ; name, payload
align 16
%1:
DEFINE_ARGS z, w, n, o1, o3
    lea o3q, [nq*3]
    lea o1q, [nq*8]
    shl o3q, 4
.loop:
    %2
    add zq, mmsize*2
    add wq, mmsize
    sub nd, mmsize/8
    jg .loop
    ret
cendfunc_internal %1
%endmacro

INIT_XMM
%define mova movaps
%xdefine SUFFIX _sse
DECL_PASS pass_sse, PASS_BIG 1
DECL_PASS pass_interleave_sse, PASS_BIG 0

%ifdef PIC
%define SECTION_REL - $$
%else
%define SECTION_REL
%endif

%macro FFT_DISPATCH 2; clobbers 5 GPRs, 8 XMMs
    lea r2, [dispatch_tab%1]
    mov r2, [r2 + (%2q-2)*gprsize]
%ifdef PIC
    lea r3, [$$]
    add r2, r3
%endif
    call r2
%endmacro ; FFT_DISPATCH

%macro DECL_FFT 2-3 ; nbits, cpu, suffix
%xdefine list_of_fft fft4%3%2 SECTION_REL, fft8%3%2 SECTION_REL
%if %1==5
%xdefine list_of_fft list_of_fft, fft16%3%2 SECTION_REL
%endif

%assign n 1<<%1
%rep 17-%1
%assign n2 n/2
%assign n4 n/4
%xdefine list_of_fft list_of_fft, fft %+ n %+ %3%2 SECTION_REL

align 16
fft %+ n %+ %3%2:
    call fft %+ n2 %+ %2
    add r0, n*4 - (n&(-2<<%1))
    call fft %+ n4 %+ %2
    add r0, n*2 - (n2&(-2<<%1))
    call fft %+ n4 %+ %2
    sub r0, n*6 + (n2&(-2<<%1))
    lea r1, [ff_cos_ %+ n]
    mov r2d, n4/2
    jmp pass%3%2
%ifdef SIZE_DIRECTIVE_NEEDED
.endfunc
[size fft %+ n %+ %3%2 fft %+ n %+ %3%2 %+ .endfunc-fft %+ n %+ %3%2]
%endif

%assign n n*2
%endrep
%undef n

align 8
dispatch_tab%3%2: pointer list_of_fft
%endmacro

DECL_FFT 5, _sse
DECL_FFT 5, _sse, _interleave

section .text

; On x86_32, this function does the register saving and restoring for all of fft.
; The others pass args in registers and don't spill anything.
cglobal ff_fft_calc_interleave, 2,5,8, z, nbits
    FFT_DISPATCH _interleave_sse, nbits
    RET 
cendfunc ff_fft_calc_interleave

cglobal ff_fft_calc, 2,5,8, z, nbits
    FFT_DISPATCH _sse, nbits
    RET 
cendfunc ff_fft_calc

%macro CMUL 5 ;j, xmm0, xmm1, 3, 4, 5
    movaps     xmm2, [%5+2*%1+0x00]
    movaps     xmm6, [%5+2*%1+0x10]
    movaps     xmm3, xmm2
    shufps     xmm3, xmm6, 0xdd
    shufps     xmm2, xmm6, 0x88
    movaps     xmm6, %3
    movaps     xmm7, %2
    mulps      xmm6, xmm2
    mulps      xmm7, xmm2
    mulps        %2, xmm3
    mulps        %3, xmm3
%endmacro

cglobal ff_imdct_postrotate, 3,7,5, z, tcos, nbits ; FFTComplex *z, float *tcos, unsigned int nbits
    mov    r5, nbitsq
    lea    zq, [zq    + 2*r5]
    lea tcosq, [tcosq + 2*r5]
    sub    r5, 0x10
    neg nbitsq
.post:
    movaps   xmm1, [zq+nbitsq*2]
    movaps   xmm0, [zq+nbitsq*2+0x10]
    CMUL     nbitsq,   xmm0, xmm1, zq, tcosq
    subps    xmm0, xmm6
    addps    xmm1, xmm7
    movaps   xmm5, [zq+r5*2]
    movaps   xmm4, [zq+r5*2+0x10]
    CMUL     r5,   xmm4, xmm5, zq, tcosq
    subps    xmm4, xmm6
    addps    xmm5, xmm7
    shufps   xmm1, xmm1, 0x1b
    shufps   xmm5, xmm5, 0x1b
    movaps   xmm6, xmm4
    unpckhps xmm4, xmm1
    unpcklps xmm6, xmm1
    movaps   xmm2, xmm0
    unpcklps xmm0, xmm5
    unpckhps xmm2, xmm5
    movaps   [zq+r5*2],      xmm6
    movaps   [zq+r5*2+0x10], xmm4
    movaps   [zq+nbitsq*2],      xmm0
    movaps   [zq+nbitsq*2+0x10], xmm2

    sub      r5,     0x10
    add      nbitsq, 0x10
    jl       .post
    RET
cendfunc ff_imdct_postrotate

cglobal ff_mdct_postrotate, 3,7,5, z, tcos, nbits ; FFTComplex *z, float *tcos, unsigned int nbits
    mov    r5, nbitsq
    lea    zq, [zq    + 2*r5]
    lea tcosq, [tcosq + 2*r5]
    sub r5, 0x10
    neg nbitsq 
.post:
    movaps   xmm1, [zq+nbitsq*2]
    movaps   xmm0, [zq+nbitsq*2+0x10]
    CMUL     nbitsq,   xmm0, xmm1, zq, tcosq
    addps    xmm0, xmm6
    subps    xmm1, xmm7
    movaps   xmm5, [zq+r5*2]
    movaps   xmm4, [zq+r5*2+0x10]
    CMUL     r5,   xmm4, xmm5, zq, tcosq
    addps    xmm4, xmm6
    subps    xmm5, xmm7
    shufps   xmm1, xmm1, 0x1b
    shufps   xmm5, xmm5, 0x1b
    movaps   xmm6, xmm4
    unpckhps xmm4, xmm1
    unpcklps xmm6, xmm1
    movaps   xmm2, xmm0
    unpcklps xmm0, xmm5
    unpckhps xmm2, xmm5
    movaps   xmm5, [ps_80000000]
    xorps    xmm6, xmm5
    xorps    xmm4, xmm5
    xorps    xmm0, xmm5
    xorps    xmm2, xmm5
    movaps   [zq+r5*2],          xmm6
    movaps   [zq+r5*2+0x10],     xmm4
    movaps   [zq+nbitsq*2],      xmm0
    movaps   [zq+nbitsq*2+0x10], xmm2
    sub      r5,     0x10
    add      nbitsq, 0x10
    jl       .post
    RET
cendfunc ff_mdct_postrotate

%macro BUTTERF36 3
    movhlps %2, %1
    movlhps %2, %1
    xorps  %2, [ps_p1p1m1m1]
    addps  %1, %2
    mulps  %1, %3
    movaps %2, %1
    shufps %1, %1, 0xb1
    xorps  %2, [ps_p1m1p1m1]
    addps  %1, %2
%endmacro

cglobal imdct36, 4,4,8, out, in

    ; for(i=17;i>=1;i--) in[i] += in[i-1];
    movlps  m0, [inq]
    movhps  m0, [inq + 8]
    movlps  m1, [inq + 16]
    movhps  m1, [inq + 24]

    movlhps m5, m1
    movhlps m5, m0
    shufps  m5, m1, 0x99

    movaps  m7, m0
    shufps  m7, m7, 0x90
    addps m0, m7
    mulss m0, [ps_half]

    movlps  m2, [inq + 32]
    movhps  m2, [inq + 40]

    movlhps m7, m2
    movhlps m7, m1
    shufps  m7, m2, 0x99

    addps m1, m5
    movlps  m3, [inq + 48]
    movhps  m3, [inq + 56]

    movlhps m5, m3
    movhlps m5, m2
    shufps  m5, m3, 0x99

    xorps  m4, m4
    movlps m4, [inq+64]
    movaps m6, m4
    shufps m6, m3, 0xfc
    shufps m6, m6, 0x53

    addps  m4, m6
    addps  m2, m7
    addps  m3, m5

    ; for(i=17;i>=3;i-=2) in[i] += in[i-2];
    movlhps m6, m0
    andps   m6, [ps_mask2]

    movhlps m7, m0
    movlhps m7, m1
    andps  m7, [ps_mask]

    addps  m0, m6

    movhlps m6, m1
    movlhps m6, m2
    andps  m6, [ps_mask]

    addps  m1, m7

    movhlps m7, m2
    movlhps m7, m3
    andps  m7, [ps_mask]

    addps  m2, m6

    movhlps m6, m3
    andps  m6, [ps_mask3]

    addps  m3, m7
    addps  m4, m6

    ; Populate tmp[]

    movaps m6, m1
    movhps m6, [ps_mask2]    ; zero out high values
    subps  m6, m4

    movaps m5, m0
    subps  m5, m3

    movaps m7, m2
    mulps  m7, [ps_imdct36_val]

    mulps  m5, [ps_imdct36_val+0x10]
    addps  m7, m5
%ifdef ARCH_X86_64
    SWAP   m5, m8
%endif

    movaps m5, m6
    mulps  m5, [ps_imdct36_val]
    subps  m7, m5
    shufps m7, m7, 0x4e

    ; fixme: do not recalculate in x64

%ifndef ARCH_X86_64
    movaps m5, m0
    subps  m5, m3
%else
    SWAP   m5, m8
%endif

    subps  m5, m6
    addps  m5, m2

    movaps m6, m4
    shufps m6, m3, 0xe4
    subps  m6, m2
    mulps  m6, [ps_imdct36_val+0x20]

    addps  m4, m1
    mulps  m4, [ps_imdct36_val+0x30]

    shufps m1, m0, 0xe4
    addps  m1, m2
    mulps  m1, [ps_imdct36_val+0x40]
    mulps  m3, [ps_imdct36_val+0x50]
    mulps  m0, [ps_imdct36_val+0x60]
    addps  m0, m3

    ; m2 and m3 free
    movaps m2, m1
    xorps  m2, [ps_p1p1m1m1]
    subps  m2, m4
    addps  m2, m0

    movaps m3, m4
    addps  m3, m0
    subps  m3, m6
    xorps  m3, [ps_p1p1m1m1]

    shufps m0, m4, 0xe4
    subps  m0, m1
    addps  m0, m6
    shufps m0, m0, 0x4e

    movhlps m4, m2
    movlhps m4, m3
    shufps  m3, m2, 0x4e

    ; Now we have tmp = {m0, m7, m3, m4, m5}

    BUTTERF36 m0, m1, [ps_imdct36_cos]
    BUTTERF36 m7, m1, [ps_imdct36_cos + 16]
    BUTTERF36 m3, m1, [ps_imdct36_cos + 32]
    BUTTERF36 m4, m1, [ps_imdct36_cos + 48]

    mulps   m5, [ps_imdct36_cos + 64]
    movaps  m1, m5
    shufps  m5, m5, 0xe1
    xorps   m1, [ps_p1m1p1m1]
    addps   m5, m1

    ; permutates:
    ; m0    0  1  2  3     =>     2  6 10 14   m1
    ; m3    8  9 10 11     =>     0  4  8 12   m0
    ; m5   16 17 xx xx     =>          16 17   m5
    ; m7    4  5  6  7     =>     3  7 11 15   m2
    ; m4   12 13 14 15     =>     1  5  9 13   m3

    movaps   m1, m0
    unpckhps m1, m7 ; m1: 2 6 3 7
    movaps   m6, m3
    unpckhps m6, m4 ; m6: 10 14 11 15
    movaps   m2, m6
    movhlps  m2, m1 ; m2: 3 7 11 15
    movlhps  m1, m6 ; m1: 2 6 10 14

    unpcklps m3, m4 ; m3:  8 12 9 13
    unpcklps m0, m7 ; m0:  0  4 1  5
    movaps   m6, m0
    movlhps  m0, m3 ; m0: 0 4 8 12
    movhlps  m3, m6 ; m3: 1 5 9 13
    ; permutation done

    movups [outq+0x24], m1
    movups [outq+0x34], m0
    movss  [outq+0x44], m5
    movaps [outq     ], m2
    movaps [outq+0x10], m3
    shufps m5, m5, 0x55
    movss  [outq+0x20], m5
    RET
cendfunc imdct36

