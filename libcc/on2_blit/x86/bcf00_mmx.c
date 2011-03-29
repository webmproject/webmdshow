//==========================================================================
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1999 - 2001  On2 Technologies Inc. All Rights Reserved.
//
//--------------------------------------------------------------------------


#include <stdio.h>
#include "../vpxblit.h"

extern unsigned int WK_YforY_MMX[256];	
extern unsigned int WK_UforBG_MMX[256];
extern unsigned int WK_VforRG_MMX[256];

extern __int64 WK_CLEAR_UP_5BYTES;
extern __int64 WK_CLEAR_7_3_BYTES;


// local vars
#define L_blkWidth      0
#define L_YStride       L_blkWidth+4
#define L_UVStride      L_YStride+4
#define L_height        L_UVStride+4
#define L_scrn          L_height+4
#define L_extraWidth	L_scrn+4
#define L_tempspace		L_extraWidth+4
#define LOCAL_SPACE     L_tempspace + 16


__declspec( naked )  
void bcf00_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig) 
{
    __asm
    {
			push    esi
			push    edi
			push    ebp
			push    ebx 
			push    ecx
			push    edx

			mov         edi,[esp].dst
			mov         ebp,[esp].buffConfig

			nop
			sub         esp,LOCAL_SPACE

			mov         eax,[ebp].YStride
			mov         ecx,[ebp].UVStride

			mov         L_YStride[esp],eax
			mov         L_UVStride[esp],ecx

			mov         eax,[ebp].YHeight
			mov         ecx,[ebp].YWidth

			mov         L_height[esp],eax
			mov         L_scrn[esp],edi

			mov			eax, ecx
			shr         ecx,2

			and			eax, 3
			shl			ecx, 1

			lea			eax, [eax + eax*2]
						
			mov			L_extraWidth[esp], eax
			mov         esi,[ebp].YBuffer

			xor         ebx,ebx
			mov         L_blkWidth[esp],ecx

			mov         edx,[ebp].VBuffer
			mov         ebp,[ebp].UBuffer

			movq        mm6,WK_CLEAR_7_3_BYTES
			;-

hloop:
			xor         eax,eax
			xor         ecx,ecx

wloop:
			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;y0 y0 y0 y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;get y1
			paddsw      mm0,mm1                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm0                         ;xx R0 G0 B0

			movd        mm7,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm0                         ;xx R1 G1 B1

			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2
			packuswb    mm2,mm3                         ;xx R1 G1 B1 xx R0 B0 G0

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			pand        mm2,mm6                         ;clear byte 7 and 3

			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm5,mm2                         ;00 R1 G1 B1 00 R0 B0 G0

			pand        mm2,WK_CLEAR_UP_5BYTES
			psrlq       mm5,32
 
			movd        mm4,WK_YforY_MMX[ecx*4]
			psllq       mm5, 24

			por         mm2,mm5                         ;00 00 R1 G1 B1 R0 G0 B0
			nop

			movd        mm3,WK_YforY_MMX[eax*4]           ;get y1
			psllq       mm7,16                          ;0 CrforR CrforG 0

			punpcklwd   mm4,mm4                         ;y0 y0 y0 y0
			paddsw      mm7,mm1                         ;chromas

			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1
			paddsw      mm4,mm7                         ;xx R2 G2 B2

			paddsw      mm3,mm7                         ;xx R3 G3 B3
			;-

			packuswb    mm4,mm3                         ;xx R3 G3 B3 xx R2 G2 B2
			;-

			movq        mm5,mm4
			;-

			pand        mm5,WK_CLEAR_UP_5BYTES
			pand        mm4,mm6                         ;clear byte 7 and 3

			movq        mm7,mm4
			psrlq       mm5,16                          ;00 00 00 00 00 00 00 R2

			psllq       mm7,48                          ;G2 B2 00 00 00 00 00 00 
			;-

			por         mm2,mm7                         ;G2 B2 R1 G1 B1 R0 G0 B0
			psrlq       mm4,24                           ;00 00 00 00 R3 G3 B3 00

			por         mm4,mm5                         ;00 00 00 00 R3 G3 B3 R2
			add         ebx,2

			movq        [edi],mm2                 ;G2 B2 R1 G1 B1 R0 G0 B0
			;-

			movd        8[edi],mm4                ;R3 G3 B3 R2
			;-

			add         edi,12
			nop

			cmp         ebx,DWORD PTR L_blkWidth[esp]
			jne         wloop
			;-----------------------------------------------
			; Handle the extra when extraWidth%4!=0
			;-----------------------------------------------
			mov			ecx, L_extraWidth[esp]
			dec			ecx

			js			phloop
			xor			ecx, ecx					

			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;y0 y0 y0 y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;get y1
			paddsw      mm0,mm1                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm0                         ;xx R0 G0 B0

			movd        mm7,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm0                         ;xx R1 G1 B1

			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2
			packuswb    mm2,mm3                         ;xx R1 G1 B1 xx R0 B0 G0

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			pand        mm2,mm6                         ;clear byte 7 and 3

			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm5,mm2                         ;00 R1 G1 B1 00 R0 B0 G0

			pand        mm2,WK_CLEAR_UP_5BYTES
			psrlq       mm5,32
 
			movd        mm4,WK_YforY_MMX[ecx*4]
			psllq       mm5, 24

			por         mm2,mm5                         ;00 00 R1 G1 B1 R0 G0 B0
			nop

			movd        mm3,WK_YforY_MMX[eax*4]           ;get y1
			psllq       mm7,16                          ;0 CrforR CrforG 0

			punpcklwd   mm4,mm4                         ;y0 y0 y0 y0
			paddsw      mm7,mm1                         ;chromas

			punpcklwd   mm3,mm3                         ;y1 y1 y1 y1
			paddsw      mm4,mm7                         ;xx R2 G2 B2

			paddsw      mm3,mm7                         ;xx R3 G3 B3
			;-

			packuswb    mm4,mm3                         ;xx R3 G3 B3 xx R2 G2 B2
			;-

			movq        mm5,mm4
			;-

			pand        mm5,WK_CLEAR_UP_5BYTES
			pand        mm4,mm6                         ;clear byte 7 and 3

			movq        mm7,mm4
			psrlq       mm5,16                          ;00 00 00 00 00 00 00 R2

			psllq       mm7,48                          ;G2 B2 00 00 00 00 00 00 
			;-

			por         mm2,mm7                         ;G2 B2 R1 G1 B1 R0 G0 B0
			psrlq       mm4,24                           ;00 00 00 00 R3 G3 B3 00

			por         mm4,mm5                         ;00 00 00 00 R3 G3 B3 R2
			;add         ebx,2

			;-  movq        [edi],mm2						;G2 B2 R1 G1 B1 R0 G0 B0
			;-

			;-  movd        8[edi],mm4						;R3 G3 B3 R2

			movq		L_tempspace[esp], mm2			;
			movd		(L_tempspace+8)[esp], mm4		; save 12 bytes to local space

			mov			ecx, L_extraWidth[esp]	
			xor			ebx, ebx;	

cploop:
			mov			ax,	L_tempspace[esp+ebx]		;
			mov			[edi+ebx], ax

			add			ebx, 2
			mov			al, L_tempspace[esp+ebx]		;

			mov			[edi+ebx], al
			add			ebx, 1;

			cmp			ebx, ecx
			jne			cploop

			;-----------------------------------------------
			; prepare for next line
			;-----------------------------------------------
phloop:
			mov         edi,L_scrn[esp]
			mov         ecx,L_height[esp]

			add         edi,[esp+LOCAL_SPACE].scrnPitch
			mov         ebx,L_UVStride[esp]

			shl         ecx,31                      ;save low bit
			mov         L_scrn[esp],edi

			sar         ecx,31                      ;generate mask
			nop

			and         ebx,ecx
			nop

			sub         ebp,ebx
			sub         edx,ebx

			sub         esi,DWORD PTR L_YStride[esp]
			xor         ebx,ebx

			dec         DWORD PTR L_height[esp]
			jg          hloop

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

