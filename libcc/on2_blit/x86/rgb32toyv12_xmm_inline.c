;//==========================================================================
;//
;//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
;//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
;//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
;//  PURPOSE.
;//
;//  Copyright (c) 1999 - 2005  On2 Technologies Inc. All Rights Reserved.
;//
;//--------------------------------------------------------------------------


#include <stdio.h>
#include "colorconv_inlineasm.h"

 
//local variables
#define LOCAL_SPACE     0

//static const __int64 Zeros = 0x00000000000000000;//All Zeros for conversion from 8-bits to 16-bits
static const unsigned char Zeros[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	

//static const __int64 YBZRYGYB = 0x00C 8B 00 00 40 83 0C 8B; //0.504 * ScaleFactor, 0.504 * ScaleFactor, 0.098 * ScaleFactor
static const unsigned char YBZRYGYB[8] = {0x8B, 0x0C, 0x83, 0x40, 0x00, 0x00, 0x8B, 0x0C};	

//static const __int64 YRYGZRYR = 0x020 E5 40 83 00 00 20 E5; //0.257 * ScaleFactor, 0.504 * ScaleFactor, zero, 0.257 * ScaleFactor
static const unsigned char YRYGZRYR[8] = {0xE5, 0x20, 0x00, 0x00, 0x83, 0x40, 0xE5, 0x20};	

//static const __int64 AvgRoundFactor = 0x000 02 00 02 00 02 00 02; //Round factor used for dividing by 2
static const unsigned char AvgRoundFactor[8] = {0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00};	

//static const __int64 YARYAR = 0x000 08 40 00 00 08 40 00; //(16 * ScaleFactor) + (ScaleFactor / 2), (16 * ScaleFactor) + (ScaleFactor / 2)
static const unsigned char YARYAR[8] = {0x00, 0x40, 0x08, 0x00, 0x00, 0x40, 0x08, 0x00};	

//static const __int64 ZRVRUGUB = 0x000 00 38 31 DA C1 38 31; //Zero, 0.439 * ScaleFactor, -0.291 * ScaleFactor, 0.439 * ScaleFactor
static const unsigned char ZRVRUGUB[8] = {0x31, 0x38, 0xC1, 0xDA, 0x31, 0x38, 0x00, 0x00};	

//static const __int64 ZRZRZRUR = 0x000 00 00 00 00 00 ED 0F; //Zero, Zero, Zero, -0.148 * ScaleFactor
static const unsigned char ZRZRZRUR[8] = {0x0F, 0xED, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	

//static const __int64 VGVBZRZR = 0x0D0 E6 F6 EA 00 00 00 00; //-0.368 * ScaleFactor, -0.071 * ScaleFactor, Zero, Zero
static const unsigned char VGVBZRZR[8] = {0x00, 0x00, 0x00, 0x00, 0xEA, 0xF6, 0xE6, 0xD0};	

//static const __int64 VARUAR = 0x000 40 40 00 00 40 40 00; //(128 * ScaleFactor) + (ScaleFactor / 2), (128 * ScaleFactor) + (ScaleFactor / 2)    
static const unsigned char VARUAR[8] = {0x00, 0x40, 0x40, 0x00, 0x00, 0x40, 0x40, 0x00};	

//static const __int64 YGYBZRYR = 0x040 83 0C 8B 00 00 20 E5; //0.504 * ScaleFactor, 0.098 * ScaleFactor, Zero, 0.257 * ScaleFactor
static const unsigned char YGYBZRYR[8] = {0xE5, 0x20, 0x00, 0x00, 0x8B, 0x0C, 0x83, 0x40};	

//static const __int64 YAddRoundFact = 0x000 08 40 00 00 08 40 00; //(16 * ScaleFactor) + (ScaleFactor / 2), (16 * ScaleFactor) + (ScaleFactor / 2)
static const unsigned char YAddRoundFact[8] = {0x00, 0x40, 0x08, 0x00, 0x00, 0x40, 0x08, 0x00};	

//static const __int64 ZRYRZRZR = 0x000 00 20 E5 00 00 00 00; //Zero, 0.257 * ScaleFactor, Zero, Zero
static const unsigned char ZRYRZRZR[8] = {0x00, 0x00, 0x00, 0x00, 0xE5, 0x20, 0x00, 0x00};	

//static const __int64 VGVBUGUB = 0x0D0 E6 F6 EA DA C1 38 31; //-0.368 * ScaleFactor, -0.071 * ScaleFactor, -0.291 * ScaleFactor, 0.439 * ScaleFactor
static const unsigned char VGVBUGUB[8] = {0x31, 0x38, 0xC1, 0xDA, 0xEA, 0xF6, 0xE6, 0xD0};	

//static const __int64 ZRVRZRUR = 0x000 00 38 31 00 00 ED 0F; //Zero, 0.439 * ScaleFactor, Zero, -0.148 * ScaleFactor
static const unsigned char ZRVRZRUR[8] = {0x0F, 0xED, 0x00, 0x00, 0x31, 0x38, 0x00, 0x00};	

//static const __int64 ZRZRYGYB = 0x000 00 00 00 40 83 0C 8B; // Zero, Zero, 0.504 * ScaleFactor, 0.098 * ScaleFactor
static const unsigned char ZRZRYGYB[8] = {0x8B, 0x0C, 0x83, 0x40, 0x00, 0x00, 0x00, 0x00};	

//static const __int64 ZRYRYGYB = 0x000 00 20 E5 40 83 0C 8B; // Zeros, 0.257 * ScaleFactor, 0.504 * ScaleFactor, 0.098 * ScaleFactor
static const unsigned char ZRYRYGYB[8] = {0x8B, 0x0C, 0x83, 0x40, 0xE5, 0x20, 0x00, 0x00};	
   

__declspec( naked ) 
void CC_RGB32toYV12_XMM_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer,
					 int yp,int p)
{

	__asm
	{
		push    esi
		push    edi
		push    ebp
		push    ebx 
		push    ecx
		push    edx

		mov     esi,CConvParams[esp].ImageHeight						; Load Image height
		shr     esi,1													; divide by 2, we process image two lines at a time
		mov     CConvParams[esp].ImageHeight,esi						; Load Image height

		mov     esi,CConvParams[esp].ImageWidth							; Load Image Width
		shr     esi,1													; divide by 2, we process image two lines at a time
		mov     CConvParams[esp].ImageWidth,esi							; Load Image Width

		mov     edi,CConvParams[esp].RGBABuffer							; Input Buffer Ptr
		mov     edx,CConvParams[esp].SrcPitch							; ebx is second line of source
		add     edx,edi													; edx is second line 

		mov     eax,CConvParams[esp].YBuffer							; destination y buffer
		mov	    esi,CConvParams[esp].YPitch								; source pitch
		add     esi,eax													; esi is second source line of destination

HLoopStart:
		xor     ebp,ebp													; setup width loop ctr

WLoopStart:

		;    prefetch   8[edi+ebp*8]
		movq        mm0,QWORD PTR 0[edi+8*ebp]                          ; A1,R1,G1,B1,A0,R0,G0,B0
		movq        mm7,mm0                                             ; A1,R1,G1,B1,A0,R0,G0,B0

		punpcklbw   mm0,Zeros                                           ; A0,R0,G0,B0
		movq        mm1,mm7                                             ; A1,R1,G1,B1,A0,R0,G0,B0

		punpckhbw   mm7,Zeros                                           ; A1,R1,G1,B1
		movq        mm2,mm1                                             ; A1,R1,G1,B1,A0,R0,G0,B0
		psrlq       mm1,16                                              ; 00,00,A1,R1,G1,B1,A0,R0

		pshufw      mm2,mm2,0Ch                                         ; XX,XX,XX,XX,A1,R1,G0,B0

		punpcklbw   mm1,Zeros                                           ; G1,B1,A0,R0
		paddd       mm7,mm0                                             ; A1+A0,R1+R0,G1+G0,B1+B0

		punpcklbw   mm2,Zeros                                           ; A1,R1,G0,B0
    
		pmaddwd     mm1,YGYBZRYR                                        ; (G1*YG + B1*YB) * 8000h ,(A0*0 + R0*YR) * 8000h
	;    prefetch    8[edx+8*ebp]
		movq        mm6,QWORD PTR 0[edx+8*ebp]                          ; A1,R1,G1,B1,A0,R0,G0,B0 (load 2nd row)

		pmaddwd     mm2,ZRYRYGYB                                        ; (A1*0 + R1*YR) * 8000h, (G0*YG + B0*YB) * 8000h
		movq        mm5,mm6                                             ; A1,R1,G1,B1,A0,R0,G0,B0 (2nd row)
		movq        mm4,mm5                                             ; A1,R1,G1,B1,A0,R0,G0,B0

		paddd       mm1,mm2                                             ; (R1*YR + G1*YG + B1*YB) * 8000h ,(R0*YR + G0*YG + B0*YB) * 8000h
		movq        mm3,mm5                                             ; A1,R1,G1,B1,A0,R0,G0,B0

		paddd       mm1,YARYAR                                          ; (R1*YR + G1*YG + B1*YB + 16 + 16384) * 8000h ,(R0*YR + G0*YG + B0*YB + 16 + 16384) * 8000h
		psrlq       mm5,16                                              ; 00,00,A1,R1,G1,B1,A0,R0

		psrld       mm1,15                                              ; ((R1*YR + G1*YG + B1*YB + 16 + 16384) * 8000h) / 8000h ,((R0*YR + G0*YG + B0*YB + 16 + 16384) * 8000h) / 8000h

		;----------------------------------

		pshufw      mm6,mm6,0Ch                                         ; XX,XX,XX,XX,A1,R1,G0,B0

		punpcklbw   mm5,Zeros                                           ; G1,B1,A0,R0
		punpcklbw   mm6,Zeros                                           ; A1,R1,G0,B0
    
		pmaddwd     mm5,YGYBZRYR                                        ; (G1*YG + B1*YB) * 8000h ,(A0*0 + R0*YR) * 8000h
		punpcklbw   mm4,Zeros                                           ; A0,R0,G0,B0

		pmaddwd     mm6,ZRYRYGYB                                        ; (A1*0 + R1*YR) * 8000h, (G0*YG + B0*YB) * 8000h
		paddd       mm7,mm4                                             ; A0+A1+A0,R0+R1+R0,G0+G1+G0,B0+B1+B0
		punpckhbw   mm3,Zeros                                           ; A1,R1,G1,B1

		paddd       mm5,mm6                                             ; (R1*YR + G1*YG + B1*YB) * 8000h ,(R0*YR + G0*YG + B0*YB) * 8000h
		paddd       mm7,mm3                                             ; A1+A0+A1+A0,R1+R0+R1+R0,R1+G0+G1+G0,B1+B0+B1+B0
		paddd       mm5,YARYAR                                          ; (R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h ,(R0*YR + G0*YG + B0*YB + 16 + 4000h) * 8000h
		paddd       mm7,AvgRoundFactor                                    ; 2+A1+A0+A1+A0,2+R1+R0+R1+R0,2+R1+G0+G1+G0,2+B1+B0+B1+B0
		psrld       mm5,15                                              ; ((R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h) / 8000h ,((R0*YR + G0*YG + B0*YB + 16 + 4000h) * 8000h) / 8000h
		psrlw       mm7,2                                               ; Average of A, R, G, B
		packssdw    mm1,mm5                                             ; Y1 (2nd line), Y0 (2nd line), Y1, YO
		movq        mm0,mm7                                             ; A, R, G, B

		pshufw      mm7,mm7,044h                                        ; G, B, G, B
		pshufw      mm0,mm0,022h                                        ; X, R, X, R

		pmaddwd     mm7,VGVBUGUB                                        ; (G*VG + B*VB) * 8000h , (G*UG + B*UB) * 8000h
		pmaddwd     mm0,ZRVRZRUR                                        ; (X*0 + R*VR) * 8000h, (X*0 + R*UR) * 8000h
		packuswb    mm1,mm6                                             ; X, X, X, X, Y1 (2nd), Y0 (2nd), Y1, Y0

		paddd       mm0,mm7                                             ; (R*VR + G*VG + B*VB) * 8000h , (R*UR + G*UG + B*UB) * 8000h
		movd        ebx,mm1                                             ; Y1 (2nd), Y0 (2nd), Y1, Y0

		paddd       mm0,VARUAR                                          ; (R*VR + G*VG + B*VB + 128 + 4000h) * 8000h , (R*UR + G*UG + B*UB + 128 + 4000h) * 8000h
		mov         WORD PTR 0[eax+2*ebp],bx                            ; write Y1, Y2 to memory 

		psrld       mm0,15                                              ; ((R*VR + G*VG + B*VB + 128 + 4000h) * 8000h) / 8000h , ((R*UR + G*UG + B*UB + 128 + 4000h) * 8000h) / 8000h
		shr         ebx,16                                              ; 00, 00, Y1 (2nd), Y0 (2nd)

		; --------------------------
    
		packssdw    mm0,mm6                                             ; X, X, V, U
		mov         WORD PTR 0[esi+2*ebp],bx                              ; write Y1, Y2 to memory

		packuswb    mm0,mm6                                             ; X, X, X, X, X, X, V, U
		movd        ebx,mm0                                             ; X, X, V, U

		mov         ecx,CConvParams[esp].UBuffer
		mov         BYTE PTR 0[ecx+ebp],bl                              ; write U to memory

		mov         ecx,CConvParams[esp].VBuffer
		mov         BYTE PTR 0[ecx+ebp],bh                              ; write V to memory

		inc         ebp                                                 ; increment Y output buffer ptr
		cmp			ebp,CConvParams[esp].ImageWidth						; setup loop ctr
		jne         WLoopStart    

		mov         ebx,CConvParams[esp].SrcPitch						; increment 2 source lines
		lea			edx,[edx+2*ebx]	  									; 
		lea			edi,[edi+2*ebx]	  									; 

		mov			ebx,CConvParams[esp].YPitch							; increment 2 destination lines
		lea         eax,[eax+2*ebx]
		lea         esi,[esi+2*ebx]

		sar         ebx,1												; convert to u pitch
		add			CConvParams[esp].UBuffer,ebx						; increment u by 1 line
		add			CConvParams[esp].VBuffer,ebx
																		; line already processed
		dec         CConvParams[esp].ImageHeight						; decrement image height ctr
		jne         HLoopStart                                          ; are we done yet?

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
