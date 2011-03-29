;//==========================================================================
;//
;//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
;//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
;//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
;//  PURPOSE.
;//
;//  Copyright (c) 1999 - 2001  On2 Technologies Inc. All Rights Reserved.
;//
;//--------------------------------------------------------------------------


#include <stdio.h>
#include "vpxblit.h"

 
//local variables
#define L_blkWidth      0
#define L_vPtr	        L_blkWidth+4
#define L_extraWidth    L_vPtr+4
#define LOCAL_SPACE     L_extraWidth+4


__declspec( naked ) 
void bcc00_MMX(unsigned long *dst, int scrnPitch, VPX_BLIT_CONFIG *buffConfig)
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

			mov         eax,[esp].scrnPitch				; eax = scrnPitch
			sub         esp,LOCAL_SPACE					; mov stack pointer

			mov         ebx,[ebp].YHeight				; ebx = YHeight
			mov         ecx,[ebp].YWidth				; ecx = YWidth

			mov			edx, ecx						;
			mov         L_vPtr[esp],edi					; Save dst to local
			;add         edi,eax                         ;	point to bottom of the y area

			and			edx, 7							; get the extraWidth;
			mov			L_extraWidth[esp], edx			;

			shr         ecx,3                           ; blocks of 8 pixels
			mov         edx,[ebp].YStride				; edx = YStride
    
			mov         esi,[ebp].YBuffer				; esi = YBuffer
			mov         L_blkWidth[esp],ecx				; ecx = YWidth /8;

			xor         ebp,ebp							; ebp = 0;
			nop

			;
			; eax = srcnPitch
			; ebx = YHeight
			; ecx = YWidth/8
			; edx = YStride
			; esi = YBuffer
			; edi = dst
			; ebp = 0





y_hloop:
y_wloop:
			movq        mm0,QWORD PtR [esi+ebp]
			;-
			movq        [edi+ebp],mm0
			;-
			add         ebp,8

			dec         ecx
			jg          y_wloop

			;-------------------------------------------
			; handle the extra if width%8!=0
			;------------------------------------------
			mov			ecx, L_extraWidth[esp]
			dec			ecx

			js			py_hloop
			
ye_wloop:	
			
			mov			al, [esi+ebp];
			mov			[edi+ebp], al;

			inc			ebp
			dec			ecx

			jge			ye_wloop
			mov			eax, [esp+LOCAL_SPACE].scrnPitch;	
			;---------------------------
			; prepare for the next line
			;---------------------------
py_hloop:
			sub         esi,edx 
			xor         ebp,ebp

			mov         ecx,L_blkWidth[esp]
			add         edi,eax                         ;add in pitch

			dec         ebx
			jg          y_hloop


			;------------------------------------------------
			; do the v's
			;------------------------------------------------    
			add			eax, 1
			sar         eax, 1

			mov         ebp,[esp+LOCAL_SPACE].buffConfig

			mov         edi,[ebp].uvStart
			mov         ecx,[ebp].UVWidth

			mov			edx, ecx
			and			edx, 7

			shr         ecx,3                           ;blocks of 8 pixels 
			mov			L_extraWidth[esp], edx

			mov         esi,[ebp].VBuffer

			mov         ebx,[ebp].UVHeight
			mov         edx,[ebp].UVStride

			mov         L_blkWidth[esp],ecx
			xor         ebp,ebp

v_hloop:
v_wloop:
			movq        mm0,[esi+ebp]
			;-

			movq        [edi+ebp],mm0
			;-
			add         ebp,8

			dec         ecx
			jg          v_wloop
			;---------------------------------------
			; Handle the extra when Width%i !=0
			;---------------------------------------

			mov			ecx, L_extraWidth[esp]
			dec			ecx

			js			pv_hloop
			
ve_wloop:	
			
			mov			al, [esi+ebp];
			mov			[edi+ebp], al;

			inc			ebp
			dec			ecx

			jge			ve_wloop
			
			mov			eax, [esp+LOCAL_SPACE].scrnPitch;	
			sar			eax, 1
			;---------------------------
			; prepare for the next line
			;---------------------------
pv_hloop:

			mov         ecx,L_blkWidth[esp]
			add         edi,eax                         ;add in pitch

			sub         esi,edx                         ;add in stride
			xor         ebp,ebp

			dec         ebx
			jg          v_hloop

			


			;------------------------------------------------
			; do the u's
			;------------------------------------------------
			mov         ebp,[esp+LOCAL_SPACE].buffConfig
			mov         ecx,L_blkWidth[esp]

			mov         edi,[ebp].uvStart
			mov         ebx,[ebp].UVHeight

			mov         esi,[ebp].UBuffer
			nop

			add         edi,DWORD PTR [ebp].uvDstArea   ;skip over v's
			xor         ebp,ebp

u_hloop:
u_wloop:
			movq        mm0,[esi+ebp]
			;-

			movq        [edi+ebp],mm0
			;-

			add         ebp,8

			dec         ecx
			jg          u_wloop
			;---------------------------------------
			; Handle the extra when Width%i !=0
			;---------------------------------------
			mov			ecx, L_extraWidth[esp]
			dec			ecx

			js			pu_hloop
			
ue_wloop:	
			
			mov			al, [esi+ebp];
			mov			[edi+ebp], al;

			inc			ebp
			dec			ecx

			jge			ue_wloop
			
			mov			eax, [esp+LOCAL_SPACE].scrnPitch;	
			sar			eax, 1
			;---------------------------
			; prepare for the next line
			;---------------------------
pu_hloop:


			mov         ecx,L_blkWidth[esp]
			add         edi,eax                         ;add in pitch

			sub         esi,edx                         ;add in stride
			xor         ebp,ebp

			dec         ebx
			jg          u_hloop
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
