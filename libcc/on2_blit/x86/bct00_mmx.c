//==========================================================================
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1999 - 2005  On2 Technologies Inc. All Rights Reserved.
//
//  conver yv12 to RGB32
//--------------------------------------------------------------------------


#include <stdio.h>
#include "../vpxblit.h"

typedef unsigned long RGBColorComponent;
extern RGBColorComponent *WK_ClampOriginR;
extern RGBColorComponent *WK_ClampOriginG;
extern RGBColorComponent *WK_ClampOriginB;

extern unsigned int WK_YforY_MMX[256];	
extern unsigned int WK_UforBG_MMX[256];
extern unsigned int WK_VforRG_MMX[256];


// local vars
#define L_blkWidth      0
#define L_YStride       L_blkWidth+4
#define L_UVStride      L_YStride+4
#define L_height        L_UVStride+4
#define L_width			L_height+4
#define LOCAL_SPACE     L_width+4



__declspec( naked )   
void bct00_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig) 
{
    __asm
    {
			push    esi
			push    edi
			push    ebp
			push    ebx 
			push    ecx
			push    edx

			mov         edi,[esp].dst					; edi = dst
			mov         ebp,[esp].buffConfig			; ebp = buffConfig

			nop
			sub         esp,LOCAL_SPACE					; local space

			mov         eax,[ebp].YStride				; eax = YStride
			mov         ecx,[ebp].UVStride				; ecx = UVStride

			mov         L_YStride[esp],eax				; save YStride Local
			mov         L_UVStride[esp],ecx				; save UVStride local

			mov         eax,[ebp].YHeight				; eax = YHeight
			mov         ecx,[ebp].YWidth				; ecx = YWidth

			mov         L_height[esp],eax				; save YHeight local
			nop
			mov			L_width[esp], ecx				; save YWidth local

			shr         ecx,1                           ; blocks of 2 pixels
			mov         esi,[ebp].YBuffer				; esi = YBuffer

			xor         ebx,ebx							; clear ebx
			mov         L_blkWidth[esp],ecx

			mov         edx,[ebp].VBuffer				; edx = VBuffer
			mov         ebp,[ebp].UBuffer				; ebp = UBuffer
			;
			;
			;	eax = YHeight
			;   ebx = 0
			;   ecx = YWidth /2
			;  	edx = VBuffer
			;	ebp = UBuffer
			;	esi = YBuffer
			;	edi = dst
			;


hloop:
			xor         eax,eax
			xor         ecx,ecx

wloop:
			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]      ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]      ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;y0 y0 y0 y0

			movd        mm3,WK_YforY_MMX[eax*4]       ;get y1
			paddsw      mm0,mm1                         ;chromas

			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1
			add         ebx,1

			paddsw      mm2,mm0                         ;x r g b
			paddsw      mm3,mm0                         ;x r1 g1 b1

			packuswb    mm2,mm3                         ;combine both pixels
			;-

			;-
			;-

			movq        QWORD PTR [edi+ebx*8-8],mm2
			;-

			cmp         ebx,DWORD PTR L_blkWidth[esp]
			jne         wloop
			;---------------------------------------------------
			; handle extra pixel if width%2!=0
			;---------------------------------------------------
			shl			ebx, 1

			cmp			ebx, L_width[esp]
			je			phloop

			shr			ebx, 1
			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]      ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]      ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;y0 y0 y0 y0

			movd        mm3,WK_YforY_MMX[eax*4]       ;get y1
			paddsw      mm0,mm1                         ;chromas

			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1

			paddsw      mm2,mm0                         ;x r g b
			paddsw      mm3,mm0                         ;x r1 g1 b1

			packuswb    mm2,mm3                         ;combine both pixels
			;-

			;-
			;-
			movd        [edi+ebx*8],mm2					; write out one pixel

			;---------------------------------------------------
			; prepare for the next line
			;---------------------------------------------------
phloop:
			mov         ecx,L_height[esp]
			nop

			shl         ecx,31                      ;save low bit
			add         edi,[esp+LOCAL_SPACE].scrnPitch

			sar         ecx,31                      ;generate mask
			mov         ebx,L_UVStride[esp]

			and         ebx,ecx
			nop

			sub         ebp,ebx
			sub         edx,ebx

			sub         esi,DWORD PTR L_YStride[esp]
			xor         ebx,ebx

			dec         DWORD PTR L_height[esp]
			jg          hloop
			;------------------------------------------------


			add         esp,LOCAL_SPACE
			nop

			emms

			pop     edx
			pop     ecx
			pop     ebx
			pop     ebp
			pop     edi
			pop     esi

			ret
    }


}

