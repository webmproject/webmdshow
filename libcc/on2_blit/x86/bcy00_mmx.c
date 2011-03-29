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

extern __int64  L_3s; //0x0003000300030003//; 4 3's 
extern __int64  L_2s; //0x0002000200020002//; 4 3's 

// local vars
#define L_blkWidth      0
#define L_YStride       L_blkWidth+4
#define L_Height		L_YStride+4
#define L_extraWidth	L_Height+4
#define L_tempspaceL    L_extraWidth+4
#define L_tempspaceH    L_tempspaceL+8
#define LOCAL_SPACE     L_tempspaceL+16




__declspec( naked )   
void bcy00_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig) 
{
    __asm
    {
			push    esi
			push    edi

			push    ebp
			push    ebx 

			push    ecx
			push    edx

			mov         edi,[esp].dst				; edi = dst
			mov         ebp,[esp].buffConfig		; ebp = buffConfig

			nop
			sub         esp,LOCAL_SPACE				

			mov         eax,[ebp].YStride			; eax = YStride
			mov         L_YStride[esp],eax			; save to local

			mov         eax,[ebp].YHeight			; eax = Height
			dec         eax                         ; 1 less than full height

			mov			L_Height[esp], eax			; save to local
			mov         ecx,[ebp].YWidth			; ecx = YWidth

			;	mov			eax, [esp+LOCAL_SPACE].scrnPitch

			mov			eax, ecx					; eax = YWidth
			shr         ecx,3                       ;blocks of 8 pixels

			mov         esi,[ebp].YBuffer			; esi = YBuffer
			xor         ebx,ebx						; ebx = 0

			mov         L_blkWidth[esp],ecx			; Save YWidth/8 to local
			and			eax, 7						; extraWidth
			
			mov			L_extraWidth[esp], eax		; save extraWidth;
			mov         eax,ebx						; eax = 0;

			mov         edx,[ebp].VBuffer			; edx = YBuffer
			mov         ebp,[ebp].UBuffer			; ebp = UBuffer

			pxor        mm4,mm4			    
			;
			;   eax = 0
			;	ebx = 0;
			;   ecx = YWidth/8
			;   edx = VBuffer
			;   ebp = UBuffer
			;	esi = YBuffer
			;   edi = dst
			;

hloop:
wloop:
			movq        mm7,[edx+ebx]               ; get 8 v's
			punpcklbw   mm7,mm4                     ; unpack v's with 0's

			pmullw      mm7,L_3s					; v's * 3
			movq        mm5,[edx+eax]               ; get 8 v's pointed to by eax

			movq        mm6,mm5						; copy to mm7
			punpcklbw   mm6,mm4                     ; unpack v's with 0's

			paddw	    mm7,mm6						; mm7 = 3 * [ebx] + 1 * [eax]
			paddw       mm7,L_2s					; mm7 = 3 * [ebx] + 1 * [eax] + 2

			psrlw       mm7,2						; mm7 = ( 3 * [ebx] + 1 * [eax] + 2 ) / 4

			movq        mm3,[ebp+ebx]               ; get 8 u's into mm1
			punpcklbw   mm3,mm4                     ; unpack u's with 0's

			pmullw      mm3,L_3s					; u's * 3
			movq        mm5,[ebp+eax]               ; get 8 u's pointed to by eax

			movq        mm6,mm5						; copy to mm3
			punpcklbw   mm6,mm4                     ; unpack u's with 0's

			paddw	    mm3,mm6						; mm3 = 3 * [ebx] + 1 * [eax]
			paddw       mm3,L_2s					; mm3 = 3 * [ebx] + 1 * [eax] + 2
			
			psrlw       mm3,2						; mm3 = ( 3 * [ebx] + 1 * [eax] + 2 ) / 4

			psllw       mm7,8						; v3 0 v2 0 v1 0 v0 0 
			por			mm3,mm7						; v3 u3 v2 u2 v1 u1 v0 u0 

			movq        mm0,[esi+ebx*2]             ; get the y's
			movq        mm1,mm0                     ; save upper y's

			punpcklbw   mm0,mm3                     ; v1 y3 u1 y2 v0 y1 u0 y0
			punpckhbw   mm1,mm3                     ; v3 y7 u3 y6 v2 y5 u2 y4

			dec         ecx
			movq        [edi+ebx*4],mm0             ;write first 4 pixels
			;-

			movq        8[edi+ebx*4],mm1			;write next 4 pixels
			;-

			lea         eax,[eax+4]			  ;increment *1 pointer to next pixel
			lea         ebx,[ebx+4]			  ;increment *3 pointer to next pixel
    
			jg          wloop
			;------------------------------------------------------------
			; need to handle the line end condition when YWidth%8 !=0
			;------------------------------------------------------------
			mov			ecx, L_extraWidth[esp]		; extraWidth

;ewloop:
			dec			ecx
			js			phloop

			movq        mm7,[edx+ebx]               ; get 8 v's
			punpcklbw   mm7,mm4                     ; unpack v's with 0's

			pmullw      mm7,L_3s					; v's * 3
			movq        mm5,[edx+eax]               ; get 8 v's pointed to by eax

			movq        mm6,mm5						; copy to mm7
			punpcklbw   mm6,mm4                     ; unpack v's with 0's

			paddw	    mm7,mm6						; mm7 = 3 * [ebx] + 1 * [eax]
			paddw       mm7,L_2s					; mm7 = 3 * [ebx] + 1 * [eax] + 2

			psrlw       mm7,2						; mm7 = ( 3 * [ebx] + 1 * [eax] + 2 ) / 4

			movq        mm3,[ebp+ebx]               ; get 8 u's into mm1
			punpcklbw   mm3,mm4                     ; unpack u's with 0's

			pmullw      mm3,L_3s					; u's * 3
			movq        mm5,[ebp+eax]               ; get 8 u's pointed to by eax

			movq        mm6,mm5						; copy to mm3
			punpcklbw   mm6,mm4                     ; unpack u's with 0's

			paddw	    mm3,mm6						; mm3 = 3 * [ebx] + 1 * [eax]
			paddw       mm3,L_2s					; mm3 = 3 * [ebx] + 1 * [eax] + 2
			
			psrlw       mm3,2						; mm3 = ( 3 * [ebx] + 1 * [eax] + 2 ) / 4

			psllw       mm7,8						; v3 0 v2 0 v1 0 v0 0 
			por			mm3,mm7						; v3 u3 v2 u2 v1 u1 v0 u0 

			movq        mm0,[esi+ebx*2]             ; get the y's
			movq        mm1,mm0                     ; save upper y's

			punpcklbw   mm0,mm3                     ; v1 y3 u1 y2 v0 y1 u0 y0
			punpckhbw   mm1,mm3                     ; v3 y7 u3 y6 v2 y5 u2 y4

			movq        L_tempspaceL[esp],mm0       ;write first 4 pixels
			;-

			movq        L_tempspaceH[esp],mm1		;write next 4 pixels
			;-
			;------------------------------------------------------------
			; uncomment the next two line of code will make the image
			; output have a white last vertical line
			;------------------------------------------------------------
			;	mov			eax, 0ff80ff80h
			;	mov			L_tempspaceH[esp], eax;		; read two bytes			
			;------------------------------------------------------------

			lea			ebx, [edi + ebx*4];get the destination pointer	
cploop:
			mov			ax,  L_tempspaceL[esp + ecx * 2];		; read two bytes			
			mov			[ebx+ecx*2], ax

			dec			ecx	
			jge			cploop

phloop:
			;------------------------------------------------------------
			; prepare for the next line
			;------------------------------------------------------------
			mov         ecx,DWORD PTR L_Height[esp] ;get current line number
			mov         ebx,[esp+LOCAL_SPACE].scrnPitch

			shl         ecx,31                      ; save low bit
			add         edi,ebx

			sar         ecx,31                      ; even lines ecx = 00000000 odd lines it equals FFFFFFFF

			mov         ebx,L_YStride[esp]		  
			sar         ebx,1						; ebx is uv stride
 
			mov         eax,ebx                     ; eax is uv stride 
			and         eax,ecx                     ; odd lines eax equals uvpitch even lines eax = 0 

			not         ecx                         ; even lines ecx = ffffffff odd lines it equals 00000000
			and         ebx,ecx						; ebx = uv pitch on even lines and 0 on odd lines

			sub	    ebp,ebx							; increment u pointer if we're on an odd line
			sub         edx,ebx						; increment v pointer if we're on an odd line

			neg	    eax								; eax = -uvpitch on odd lines and 0 on even lines
			add         eax,ebx						; eax = -uvpitch on odd lines and +uv pitch on even lines

			xor         ebx,ebx						; ebx is used as column pointer so set it to 0  
			mov         ecx,L_blkWidth[esp]

			sub         esi,DWORD PTR L_YStride[esp]

			dec         DWORD PTR L_Height[esp]
			jg          hloop

			mov	    eax,ebx				  ; last line ebx and eax should point to the same line
			jz          hloop 
			;------------------------------------------------

;theExit:
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

