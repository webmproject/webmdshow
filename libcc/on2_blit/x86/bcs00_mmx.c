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

typedef unsigned long RGBColorComponent;
extern RGBColorComponent *WK_ClampOriginR;
extern RGBColorComponent *WK_ClampOriginG;
extern RGBColorComponent *WK_ClampOriginB;

extern unsigned int WK_YforY_MMX[256];	
extern unsigned int WK_UforBG_MMX[256];
extern unsigned int WK_VforRG_MMX[256];

extern __int64 WK_johnsTable_MMX;
extern __int64 WK_johnsTable;

extern __int64 WK_RGB_MULFACTOR_555;
extern __int64 WK_RB_MASK_555;
extern __int64 WK_G_MASK_555;

extern __int64 WK_RGB_MULFACTOR_565;
extern __int64 WK_RB_MASK_565;
extern __int64 WK_G_MASK_565;



// local vars
#define L_blkWidth      0
#define L_YStride       L_blkWidth+4
#define L_UVStride      L_YStride+4
#define L_height        L_UVStride+4
#define L_extraWidth	L_height+4
#define L_tempspace		L_extraWidth+4
#define LOCAL_SPACE     L_tempspace+8


__declspec( naked )
void bcs00_555_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig) 
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
			sub         esp,LOCAL_SPACE					; mov stack pointer

			mov         eax,[ebp].YStride				; eax = YStride
			mov         ecx,[ebp].UVStride				; ecx = UVStride

			mov         L_YStride[esp],eax				; save YStride local
			mov         L_UVStride[esp],ecx				; save UVStride local

			mov         eax,[ebp].YHeight				; eax = YHeight
			mov         ecx,[ebp].YWidth				; ecx = YWidth

			mov         L_height[esp],eax				; save YHeight Local
			
			mov			eax, ecx						; eax = YWidth	
			shr         ecx, 2                          ;blocks of 4 pixels

			shl			ecx, 1							; blocks of 2 pixles in 
			and			eax, 3							; get number of extra pixels

			mov         esi,[ebp].YBuffer				; esi = YBuffer
			mov			L_extraWidth[esp], eax			; save extraWidth local

			xor         ebx,ebx							; clear ebx
			mov         L_blkWidth[esp],ecx				; save blkWidth local

			mov         edx,[ebp].VBuffer				; edx = VBuffer
			mov         ebp,[ebp].UBuffer				; ebp = UBuffer

			movq        mm7,WK_RB_MASK_555
			;-
			movq        mm6,WK_G_MASK_555
			;-
			movq        mm5,WK_RGB_MULFACTOR_555
			;-

hloop555:
			xor         eax,eax
			xor         ecx,ecx

wloop555:
			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]           ;Y0
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;Y0 Y0 Y0 Y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y1
			paddsw      mm1,mm0                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;Y1 Y1 Y1 Y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm1                         ;x r0 g0 b0

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm1                         ;x r1 g1 b1

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			psllq       mm0,16                          ;0 CrforR CrforG 0
			;-------
			mov         eax,L_height[esp]
			packuswb    mm2,mm3                         ;x r1 g1 b1 x r0 g0 b0

			and         eax,03h
			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2

			shl         eax,1
			paddsw      mm1,mm0                         ;second pair chromas

			paddusb     mm2,QWORD PTR WK_johnsTable_MMX[eax*8]
			;-

			xor         eax,eax
			;-

			;-------
			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm0,mm2

			movd        mm4,WK_YforY_MMX[ecx*4]           ;Y2
			pand        mm2,mm7                         ;loose lower 3 bits of red and blue

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y3
			pmaddwd     mm2,mm5                         ;position red and blue

			punpcklwd   mm4,mm4                         ;Y2 Y2 Y2 Y2
			pand        mm0,mm6                         ;loose lower 3 bits of green

			mov         eax,L_height[esp]
			punpcklwd   mm3,mm3                         ;Y3 Y3 Y3 Y3

			and         eax,03h
			paddsw      mm4,mm1                         ;x r2 g2 b2

			shl         eax,1
			paddsw      mm3,mm1                         ;x r3 g3 b3

			por         mm2,mm0                         
			packuswb    mm4,mm3                         ;x r3 g3 b3 x r2 g2 b2

			paddusb     mm4,QWORD PTR WK_johnsTable_MMX[8+eax*8] 
			psrld       mm2,6                           ;0 0 0 0 R1 G1 B1 0 0 0 0 R0 G0 B0

			movq        mm1,mm4
			pand        mm4,mm7                         ;loose lower 3 bits of red and blue

			pmaddwd     mm4,mm5                         ;position red and blue
			pand        mm1,mm6                         ;loose lower 3 bits of green

			por         mm1,mm4                         
			;-

			psrld       mm1,6                           ;0 0 0 0 R3 G3 B3 0 0 0 0 R2 G2 B2
			add         ebx,2

			packssdw    mm2,mm1                         ;combine all 4 pixels
			xor         eax,eax

			;+

			movq        QWORD PTR [edi+ebx*4-8],mm2
			;-

			cmp         ebx,DWORD PTR L_blkWidth[esp]
			jne         wloop555
			;----------------------------------------------
			; handle the extra pixels if YWidth%4!=0
			;----------------------------------------------
			mov			ecx, L_extraWidth[esp]
			dec			ecx
			js			phloop555	
			xor			ecx, ecx						;clear out the ecx

			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]           ;Y0
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;Y0 Y0 Y0 Y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y1
			paddsw      mm1,mm0                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;Y1 Y1 Y1 Y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm1                         ;x r0 g0 b0

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm1                         ;x r1 g1 b1

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			psllq       mm0,16                          ;0 CrforR CrforG 0
			;-------
			mov         eax,L_height[esp]
			packuswb    mm2,mm3                         ;x r1 g1 b1 x r0 g0 b0

			and         eax,03h
			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2

			shl         eax,1
			paddsw      mm1,mm0                         ;second pair chromas

			paddusb     mm2,QWORD PTR WK_johnsTable_MMX[eax*8]
			;-

			xor         eax,eax
			;-

			;-------
			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm0,mm2

			movd        mm4,WK_YforY_MMX[ecx*4]           ;Y2
			pand        mm2,mm7                         ;loose lower 3 bits of red and blue

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y3
			pmaddwd     mm2,mm5                         ;position red and blue

			punpcklwd   mm4,mm4                         ;Y2 Y2 Y2 Y2
			pand        mm0,mm6                         ;loose lower 3 bits of green

			mov         eax,L_height[esp]
			punpcklwd   mm3,mm3                         ;Y3 Y3 Y3 Y3

			and         eax,03h
			paddsw      mm4,mm1                         ;x r2 g2 b2

			shl         eax,1
			paddsw      mm3,mm1                         ;x r3 g3 b3

			por         mm2,mm0                         
			packuswb    mm4,mm3                         ;x r3 g3 b3 x r2 g2 b2

			paddusb     mm4,QWORD PTR WK_johnsTable_MMX[8+eax*8] 
			psrld       mm2,6                           ;0 0 0 0 R1 G1 B1 0 0 0 0 R0 G0 B0

			movq        mm1,mm4
			pand        mm4,mm7                         ;loose lower 3 bits of red and blue

			pmaddwd     mm4,mm5                         ;position red and blue
			pand        mm1,mm6                         ;loose lower 3 bits of green

			por         mm1,mm4                         
			;-

			psrld       mm1,6                           ;0 0 0 0 R3 G3 B3 0 0 0 0 R2 G2 B2
			;    add         ebx,2

			packssdw    mm2,mm1                         ;combine all 4 pixels
			xor         eax,eax

			;+

			;movq        QWORD PTR -8[edi+ebx*4],mm2
			movq        QWORD PTR L_tempspace[esp],mm2

			lea			ebx, [edi+ebx*4]				; get the desination pointer
			mov			ecx, L_extraWidth[esp]			; get the count
			
			dec			ecx								
			xor			eax, eax						; clear eax
cploop555:			
			mov			ax,	 L_tempspace[esp+ecx*2]		; read two bytes
			mov			[ebx+ecx*2], ax					

			dec			ecx
			jge			cploop555			


			;---------------------------------------------
			; prepare to go to the next line
			;---------------------------------------------
phloop555:

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
			jg          hloop555

			;------------------------------------------------
;theExit555:
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

__declspec( naked )
void bcs00_565_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig)
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
			sub         esp,LOCAL_SPACE					; mov stack pointer

			mov         eax,[ebp].YStride				; eax = YStride
			mov         ecx,[ebp].UVStride				; ecx = UVStride

			mov         L_YStride[esp],eax				; save YStride local
			mov         L_UVStride[esp],ecx				; save UVStride local

			mov         eax,[ebp].YHeight				; eax = YHeight
			mov         ecx,[ebp].YWidth				; ecx = YWidth

			mov         L_height[esp],eax				; save YHeight Local
			
			mov			eax, ecx						; eax = YWidth	
			shr         ecx, 2                          ;blocks of 4 pixels

			shl			ecx, 1							; blocks of 2 pixles in 
			and			eax, 3							; get number of extra pixels

			mov         esi,[ebp].YBuffer				; esi = YBuffer
			mov			L_extraWidth[esp], eax			; save extraWidth local

			xor         ebx,ebx							; clear ebx
			mov         L_blkWidth[esp],ecx				; save blkWidth local

			mov         edx,[ebp].VBuffer				; edx = VBuffer
			mov         ebp,[ebp].UBuffer				; ebp = UBuffer

			movq        mm7,WK_RB_MASK_565				;
			;-
			movq        mm6,WK_G_MASK_565
			;-
			movq        mm5,WK_RGB_MULFACTOR_565
			;-


			;---------------------------------------------------

hloop565:
			xor         eax,eax
			xor         ecx,ecx

wloop565:
			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]           ;Y0
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;Y0 Y0 Y0 Y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y1
			paddsw      mm1,mm0                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;Y1 Y1 Y1 Y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm1                         ;x r0 g0 b0

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm1                         ;x r1 g1 b1

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         eax,L_height[esp]
			packuswb    mm2,mm3                         ;x r1 g1 b1 x r0 g0 b0

			and         eax,03h
			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2

			shl         eax,1
			paddsw      mm1,mm0                         ;second pair chromas

			paddusb     mm2,QWORD PTR WK_johnsTable_MMX[eax*8]
			;-

			xor         eax,eax
			;-

			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm0,mm2

			movd        mm4,WK_YforY_MMX[ecx*4]           ;Y2
			pand        mm2,mm7                         ;loose lower 3 bits of red and blue

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y3
			pmaddwd     mm2,mm5                         ;position red and blue

			punpcklwd   mm4,mm4                         ;Y2 Y2 Y2 Y2
			pand        mm0,mm6                         ;loose lower 3 bits of green

			mov         eax,L_height[esp]
			punpcklwd   mm3,mm3                         ;Y3 Y3 Y3 Y3

			and         eax,03h
			paddsw      mm4,mm1                         ;x r2 g2 b2

			shl         eax,1
			paddsw      mm3,mm1                         ;x r3 g3 b3

			por         mm2,mm0                         
			packuswb    mm4,mm3                         ;x r3 g3 b3 x r2 g2 b2

			paddusb     mm4,QWORD PTR WK_johnsTable_MMX[8+eax*8] 
			psrld       mm2,5                           ;0 0 0 0 R1 G1 B1 0 0 0 0 R0 G0 B0

			movq        mm1,mm4
			pand        mm4,mm7                         ;loose lower 3 bits of red and blue

			pmaddwd     mm4,mm5                         ;position red and blue
			pand        mm1,mm6                         ;loose lower 3 bits of green

			por         mm1,mm4                         
			xor         eax,eax

			psrld       mm1,5                           ;0 0 0 0 R3 G3 B3 0 0 0 0 R2 G2 B2
			movq        mm4,mm2

			punpckldq   mm2,mm1                         ;0 0 0 0 R2 G2 B2 0 0 0 0 R0 G0 B0
			add         ebx,2

			punpckhdq   mm4,mm1                         ;0 0 0 0 R3 G3 B3 0 0 0 0 R1 G1 B1
			;-
    
			pslld       mm4,16
			;-

			por         mm4,mm2
			;-

			movq        QWORD PTR [edi+ebx*4-8],mm4
			;-

			cmp         ebx,DWORD PTR L_blkWidth[esp]
			jne         wloop565

			;----------------------------------------------
			; handle the extra pixels if YWidth%4!=0
			;----------------------------------------------
			mov			ecx, L_extraWidth[esp]
			dec			ecx
			js			phloop565	
			xor			ecx, ecx						;clear out the ecx

			mov         cl,BYTE PTR 0[edx+ebx]          ;get v
			nop

			mov         al,BYTE PTR 0[esi+ebx*2]        ;get y0
			nop

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			;-

			movd        mm2,WK_YforY_MMX[eax*4]           ;Y0
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         cl,BYTE PTR 0[ebp+ebx]          ;get u
			nop

			mov         al,BYTE PTR 1[esi+ebx*2]        ;get y1
			nop

			movd        mm1,WK_UforBG_MMX[ecx*4]          ;0 0 CbforG CbforB
			punpcklwd   mm2,mm2                         ;Y0 Y0 Y0 Y0

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y1
			paddsw      mm1,mm0                         ;chromas

			mov         cl,BYTE PTR 1[edx+ebx]          ;get v
			punpcklwd   mm3,mm3                         ;Y1 Y1 Y1 Y1

			mov         al,BYTE PTR 1[ebp+ebx]          ;get u
			paddsw      mm2,mm1                         ;x r0 g0 b0

			movd        mm0,WK_VforRG_MMX[ecx*4]          ;0 0 CrforR CrforG
			paddsw      mm3,mm1                         ;x r1 g1 b1

			movd        mm1,WK_UforBG_MMX[eax*4]          ;0 0 CbforG CbforB
			psllq       mm0,16                          ;0 CrforR CrforG 0

			mov         eax,L_height[esp]
			packuswb    mm2,mm3                         ;x r1 g1 b1 x r0 g0 b0

			and         eax,03h
			mov         cl,BYTE PTR 2[esi+ebx*2]        ;get y2

			shl         eax,1
			paddsw      mm1,mm0                         ;second pair chromas

			paddusb     mm2,QWORD PTR WK_johnsTable_MMX[eax*8]
			;-

			xor         eax,eax
			;-

			mov         al,BYTE PTR 3[esi+ebx*2]        ;get y3
			movq        mm0,mm2

			movd        mm4,WK_YforY_MMX[ecx*4]           ;Y2
			pand        mm2,mm7                         ;loose lower 3 bits of red and blue

			movd        mm3,WK_YforY_MMX[eax*4]           ;Y3
			pmaddwd     mm2,mm5                         ;position red and blue

			punpcklwd   mm4,mm4                         ;Y2 Y2 Y2 Y2
			pand        mm0,mm6                         ;loose lower 3 bits of green

			mov         eax,L_height[esp]
			punpcklwd   mm3,mm3                         ;Y3 Y3 Y3 Y3

			and         eax,03h
			paddsw      mm4,mm1                         ;x r2 g2 b2

			shl         eax,1
			paddsw      mm3,mm1                         ;x r3 g3 b3

			por         mm2,mm0                         
			packuswb    mm4,mm3                         ;x r3 g3 b3 x r2 g2 b2

			paddusb     mm4,QWORD PTR WK_johnsTable_MMX[8+eax*8] 
			psrld       mm2,5                           ;0 0 0 0 R1 G1 B1 0 0 0 0 R0 G0 B0

			movq        mm1,mm4
			pand        mm4,mm7                         ;loose lower 3 bits of red and blue

			pmaddwd     mm4,mm5                         ;position red and blue
			pand        mm1,mm6                         ;loose lower 3 bits of green

			por         mm1,mm4                         
			xor         eax,eax

			psrld       mm1,5                           ;0 0 0 0 R3 G3 B3 0 0 0 0 R2 G2 B2
			movq        mm4,mm2

			punpckldq   mm2,mm1                         ;0 0 0 0 R2 G2 B2 0 0 0 0 R0 G0 B0
			;add         ebx,2

			punpckhdq   mm4,mm1                         ;0 0 0 0 R3 G3 B3 0 0 0 0 R1 G1 B1
			;-
    
			pslld       mm4,16
			;-

			por         mm4,mm2
			;-

			;    movq        QWORD PTR -8[edi+ebx*4],mm4
			movq        QWORD PTR L_tempspace[esp],mm4

			lea			ebx, [edi+ebx*4]				; get the desination pointer
			mov			ecx, L_extraWidth[esp]			; get the count
			
			dec			ecx								
			xor			eax, eax						; clear eax
cploop565:			
			mov			ax,	 L_tempspace[esp+ecx*2]		; read two bytes
			mov			[ebx+ecx*2], ax					

			dec			ecx
			jge			cploop565			

			;---------------------------------------------
			; prepare to go to the next line
			;---------------------------------------------
phloop565:

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
			jg          hloop565

			;------------------------------------------------
;theExit565:
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

