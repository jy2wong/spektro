;*****************************************************************************
;* x86-optimized Float DSP functions
;*
;* This file is part of Libav.
;*
;* Libav is free software; you can redistribute it and/or
;* modify it under the terms of the GNU Lesser General Public
;* License as published by the Free Software Foundation; either
;* version 2.1 of the License, or (at your option) any later version.
;*
;* Libav is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;* Lesser General Public License for more details.
;*
;* You should have received a copy of the GNU Lesser General Public
;* License along with Libav; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;******************************************************************************

%include "x86inc.asm"

SECTION_RODATA
pdw_80000000:    times 4 dd 0x80000000

section .text align=16

INIT_XMM
%xdefine SUFFIX _sse

;-----------------------------------------------------------------------------
; void vector_fmul(float *dst, const float *src0, const float *src1, int len)
;-----------------------------------------------------------------------------
cglobal vector_fmul, 4,4,2, dst, src0, src1, len
    lea     lenq, [lend*4 - 2*0x10]
.loop:
    movaps  xmm0, [src0q + lenq]
    movaps  xmm1, [src0q + lenq + 0x10]
    mulps   xmm0, [src1q + lenq]
    mulps   xmm1, [src1q + lenq + 0x10]
    movaps  [dstq + lenq], xmm0
    movaps  [dstq + lenq + 0x10], xmm1

    sub     lenq, 2*0x10
    jge     .loop
    RET
cendfunc vector_fmul

;------------------------------------------------------------------------------
; void vector_fmac_scalar(float *dst, const float *src, float mul, int len)
;------------------------------------------------------------------------------

%ifdef WIN64
cglobal vector_fmac_scalar, 4,4,3, dst, src, mul, len
%else
cglobal vector_fmac_scalar, 3,3,3, dst, src, len
%endif
%ifdef WIN64
    movaps xmm0, xmm2
%endif
    shufps xmm0, xmm0, 0
    lea    lenq, [lend*4-2*0x10]
.loop:
    movaps   xmm1, [srcq+lenq       ]
    movaps   xmm2, [srcq+lenq+0x10]
    mulps    xmm1, xmm0
    mulps    xmm2, xmm0
    addps    xmm1, [dstq+lenq       ]
    addps    xmm2, [dstq+lenq+0x10]
    movaps  [dstq+lenq     ], xmm1
    movaps  [dstq+lenq+0x10], xmm2
    sub     lenq, 2*0x10
    jge    .loop
    RET
cendfunc vector_fmac_scalar

;------------------------------------------------------------------------------
; void vector_fmul_scalar(float *dst, const float *src, float mul, int len)
;------------------------------------------------------------------------------

%ifdef WIN64
cglobal vector_fmul_scalar, 4,4,3, dst, src, mul, len
%else
cglobal vector_fmul_scalar, 3,3,2, dst, src, len
%endif
%ifdef WIN64
    SWAP 0, 2
%endif
    shufps   xmm0, xmm0, 0
    lea    lenq, [lend*4-0x10]
.loop:
    movaps   xmm1, [srcq+lenq]
    mulps    xmm1, xmm0
    movaps  [dstq+lenq], xmm1
    sub     lenq, 0x10
    jge     .loop
    RET
cendfunc vector_fmul_scalar

;-----------------------------------------------------------------------------
; void vector_fmul_copy(float *dst, const float *src, int len)
;-----------------------------------------------------------------------------
cglobal vector_fmul_copy, 5,5,2, dst, src, len
    lea       lenq, [lend*4 - 0x20]
.loop:
    movaps  xmm0, [srcq + lenq]
    movaps  xmm1, [srcq + lenq + 0x10]
    movaps    [dstq + lenq], xmm0
    movaps    [dstq + lenq + 0x10], xmm1

    sub     lenq, 0x20
    jge     .loop
    RET
cendfunc vector_fmul_copy

;-----------------------------------------------------------------------------
; void vector_fmul_add(float *dst, const float *src0, const float *src1,
;                      const float *src2, int len)
;-----------------------------------------------------------------------------
cglobal vector_fmul_add, 5,5,2, dst, src0, src1, src2, len
    lea       lenq, [lend*4 - 0x20]
.loop:
    movaps  xmm0, [src0q + lenq]
    movaps  xmm1, [src0q + lenq + 0x10]
    mulps   xmm0, [src1q + lenq]
    mulps   xmm1, [src1q + lenq + 0x10]
    addps   xmm0, [src2q + lenq]
    addps   xmm1, [src2q + lenq + 0x10]
    movaps    [dstq + lenq], xmm0
    movaps    [dstq + lenq + 0x10], xmm1

    sub     lenq, 0x20
    jge     .loop
    RET
cendfunc vector_fmul_add

;-----------------------------------------------------------------------------
; void vector_fmul_reverse(float *dst, const float *src0, const float *src1, int len)
;-----------------------------------------------------------------------------
cglobal vector_fmul_reverse, 4,4,2, dst, src0, src1, len
    lea       lenq, [lend*4 - 2*0x10]
.loop:
    movaps  m0, [src1q]
    movaps  m1, [src1q + 0x10]
    shufps  m0, m0, 0x1b
    shufps  m1, m1, 0x1b
    mulps   m0, [src0q + lenq + 0x10]
    mulps   m1, [src0q + lenq]
    movaps  [dstq + lenq + 0x10], m0
    movaps  [dstq + lenq], m1
    add     src1q, 2*0x10
    sub     lenq,  2*0x10
    jge     .loop
    RET
cendfunc vector_fmul_reverse

;-----------------------------------------------------------------------------
; void vector_fmul_window(float *dst, const float *src0, const float *src1,
;                         const float *win, int len)
;-----------------------------------------------------------------------------
cglobal vector_fmul_window, 5,5,6, dst, src0, src1, win, len
%ifndef ARCH_X86_64
%xdefine tmpreg r5
    push r5
%else
%xdefine tmpreg rax
%endif
    mov       tmpreg, lenq
    neg       tmpreg
    shl       tmpreg, 0x2
    shl       lenq,  0x2
    add       dstq, lenq
    add       src0q, lenq
    add       winq, lenq
    sub       lenq, 0x10
.loop:
    movaps xmm1, [winq+lenq ]
    movaps xmm0, [winq+tmpreg  ]
    movaps xmm5, [src1q+lenq]
    movaps xmm4, [src0q+tmpreg ]
    shufps xmm1, xmm1, 0x1b
    shufps xmm5, xmm5, 0x1b
    movaps xmm2, xmm0
    movaps xmm3, xmm1
    mulps  xmm2, xmm4
    mulps  xmm3, xmm5
    mulps  xmm1, xmm4
    mulps  xmm0, xmm5
    addps  xmm2, xmm3
    subps  xmm1, xmm0
    shufps xmm2, xmm2, 0x1b
    movaps [dstq+tmpreg], xmm1
    movaps [dstq+lenq], xmm2
    sub lenq, 0x10
    add tmpreg, 0x10
    jl .loop
%ifndef ARCH_X86_64
    pop     r5
%endif
    RET
cendfunc vector_fmul_window

;-----------------------------------------------------------------------------
; float scalarproduct_float(const float *v1, const float *v2, int len)
;-----------------------------------------------------------------------------
cglobal scalarproduct_float, 3,3,3, v1, v2, offset
    neg   offsetq
    shl   offsetq, 2
    sub       v1q, offsetq
    sub       v2q, offsetq
    xorps    xmm0, xmm0
.loop:
    movaps   xmm1, [v1q+offsetq]
    movaps   xmm2, [v2q+offsetq]
    mulps    xmm1, xmm2
    addps    xmm0, xmm1
    add   offsetq, 16
    js .loop
    movaps   xmm1, xmm0
    shufps   xmm1, xmm0, 0x1b
    addps    xmm1, xmm0
    movhlps  xmm0, xmm1
    addps    xmm0, xmm1
%ifndef ARCH_X86_64
    movss     r0m,  xmm0
    fld dword r0m
%endif
    RET
cendfunc scalarproduct_float

;-----------------------------------------------------------------------------
; void butterflies_float(float *src0, float *src1, int len);
;-----------------------------------------------------------------------------
cglobal butterflies_float, 3,3,3, src0, src1, len
    test      lenq, lenq
    jz .end
    shl       lenq, 2
    lea      src0q, [src0q +   lenq]
    lea      src1q, [src1q +   lenq]
    neg       lenq
.loop:
    movaps    xmm0, [src0q + lenq]
    movaps    xmm1, [src1q + lenq]
    movaps    xmm2, xmm0
    subps     xmm2, xmm1
    addps     xmm0, xmm1
    movaps      [src1q + lenq], xmm2
    movaps      [src0q + lenq], xmm0
    add       lenq, 0x10
    jl .loop
.end:
    RET
cendfunc butterflies_float

;-----------------------------------------------------------------------------
; void sbr_sum64x5(float *z)
;-----------------------------------------------------------------------------
cglobal sbr_sum64x5, 1,2,4,z
    xor     r1q, r1q
.loop:
    movaps  xmm0, [zq+r1q+   0]
    addps   xmm0, [zq+r1q+ 256]
    addps   xmm0, [zq+r1q+ 512]
    addps   xmm0, [zq+r1q+ 768]
    addps   xmm0, [zq+r1q+1024]
    movaps  [zq+r1q], xmm0
    add     r1q, 16
    cmp     r1q, 1024
    jne  .loop
    RET
cendfunc sbr_sum64x5

;-----------------------------------------------------------------------------
; void sbr_qmf_pre_shuffle(float *z)
;-----------------------------------------------------------------------------
cglobal sbr_qmf_pre_shuffle, 1,4,6,z
    mov      r3q, 0x60
    xor      r1q, r1q
    movaps   xmm6, [pdw_80000000]
.loop:
    movups   xmm0, [zq + r1q + 0x84]
    movups   xmm2, [zq + r1q + 0x94]
    movups   xmm1, [zq + r3q + 0x14]
    movups   xmm3, [zq + r3q + 0x04]

    xorps    xmm2, xmm6
    xorps    xmm0, xmm6
    shufps   xmm2, xmm2, 0x1b
    shufps   xmm0, xmm0, 0x1b
    movaps   xmm5, xmm2
    unpcklps xmm2, xmm3
    unpckhps xmm5, xmm3
    movaps   xmm4, xmm0
    unpcklps xmm0, xmm1
    unpckhps xmm4, xmm1
    movaps  [zq + 2*r3q + 0x100], xmm2
    movaps  [zq + 2*r3q + 0x110], xmm5
    movaps  [zq + 2*r3q + 0x120], xmm0
    movaps  [zq + 2*r3q + 0x130], xmm4
    add       r1q, 0x20
    sub       r3q, 0x20
    jge      .loop
    movaps   xmm2, [zq]
    movlps  [zq + 0x100], xmm2
    RET
cendfunc sbr_qmf_pre_shuffle

;-----------------------------------------------------------------------------
; float sbr_qmf_post_shuffle(float *z)
;-----------------------------------------------------------------------------
cglobal sbr_qmf_post_shuffle, 2,3,4,W,z
    lea              r2q, [zq + (64-4)*4]
    movaps          xmm3, [pdw_80000000]
.loop:
    movaps          xmm1, [zq]
    movaps          xmm0, [r2q]
    xorps           xmm0, xmm3
    shufps          xmm0, xmm0, 0x1b
    movaps          xmm2, xmm0
    unpcklps        xmm2, xmm1
    unpckhps        xmm0, xmm1
    movaps     [Wq +  0], xmm2
    movaps     [Wq + 16], xmm0
    add               Wq, 32
    sub              r2q, 16
    add               zq, 16
    cmp               zq, r2q
    jl             .loop
    RET
cendfunc sbr_qmf_post_shuffle

;-----------------------------------------------------------------------------
; void sbr_qmf_deint_bfly(float *v, const float *src0, const float *src1)
;-----------------------------------------------------------------------------
cglobal sbr_qmf_deint_bfly, 3,5,8, v,src0,src1,vrev,c
    mov               cq, 64*4-32
    lea            vrevq, [vq + 64*4]
.loop:
    movaps            m0, [src0q+cq]
    movaps            m1, [src1q]
    movaps            m4, [src0q+cq+0x10]
    movaps            m5, [src1q+0x10]
%ifdef ARCH_X86_64
    pshufd            m2, m0, 0x1b
    pshufd            m3, m1, 0x1b
    pshufd            m6, m4, 0x1b
    pshufd            m7, m5, 0x1b
%else
    movaps            m2, m0
    movaps            m3, m1
    shufps            m2, m2, 0x1b
    shufps            m3, m3, 0x1b
    movaps            m6, m4
    movaps            m7, m5
    shufps            m6, m6, 0x1b
    shufps            m7, m7, 0x1b
%endif
    addps             m5, m2
    subps             m0, m7
    addps             m1, m6
    subps             m4, m3
    movaps       [vrevq], m1
    movaps  [vrevq+0x10], m5
    movaps       [vq+cq], m0
    movaps  [vq+cq+0x10], m4
    add            src1q, 0x20
    add            vrevq, 0x20
    sub               cq, 0x20
    jge            .loop
    RET
cendfunc sbr_qmf_deint_bfly

;-----------------------------------------------------------------------------
; void sbr_hf_g_filt(float (*Y)[2], float (*X_high)[40][2],
;                    const float *g_filt, size_t m_max, size_t ixh)
;-----------------------------------------------------------------------------
%define STEP  40*4*2
cglobal sbr_hf_g_filt, 5, 6, 5
    lea         r1, [r1 + 8*r4] ; offset by ixh elements into X_high
    mov         r5, r3
    and         r3, 0xFC
    lea         r2, [r2 + r3*4]
    lea         r0, [r0 + r3*8]
    neg         r3
    jz          .loop1
.loop4:
    movlps      m0, [r2 + 4*r3 + 0]
    movlps      m1, [r2 + 4*r3 + 8]
    movlps      m2, [r1 + 0*STEP]
    movlps      m3, [r1 + 2*STEP]
    movhps      m2, [r1 + 1*STEP]
    movhps      m3, [r1 + 3*STEP]
    unpcklps    m0, m0
    unpcklps    m1, m1
    mulps       m0, m2
    mulps       m1, m3
    movups      [r0 + 8*r3 +  0], m0
    movups      [r0 + 8*r3 + 16], m1
    add         r1, 4*STEP
    add         r3, 4
    jnz         .loop4
    and         r5, 3 ; number of single element loops
    jz          .end
.loop1: ; element 0 and 1 can be computed at the same time
    movss       m0, [r2]
    movlps      m2, [r1]
    unpcklps    m0, m0
    mulps       m2, m0
    movlps    [r0], m2
    add         r0, 8
    add         r2, 4
    add         r1, STEP
    dec         r5
    jnz         .loop1
.end:
    RET
cendfunc sbr_hf_g_filt

