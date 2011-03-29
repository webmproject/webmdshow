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
void CC_RGB32toYV12_MMX_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
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
		xor	    ebp,ebp													; col iterator

WLoopStart:

		movq        mm0,QWORD PTR 0[edi+8*ebp]                          ; A1,R1,G1,B1,A0,R0,G0,B0
		movq        mm1,mm0                                             ; A1,R1,G1,B1,A0,R0,G0,B0
		movq        mm7,mm0                                             ; A1,R1,G1,B1,A0,R0,G0,B0
		psrlq       mm1,16                                              ; 00,00,A1,R1,G1,B1,A0,R0
		punpcklbw   mm0,Zeros                                           ; convert to 16-bits
																		; A0,R0,G0,B0
		movq        mm2,mm7                                             ; A1,R1,G1,B1,A0,R0,G0,B0
		punpckhbw   mm7,Zeros                                           ; unpack and covert to 16-bits
																		; A1,R1,G1,B1
		paddw       mm7,mm0                                             ; A1+A0,R1+R0,G1+G0,B1+B0
		punpcklbw   mm1,Zeros                                           ; convert to 16-bits
																		; G1,B1,A0,R0
		punpckhbw   mm2,Zeros                                           ; unpack and convert to 16-bits
																		; A1,R1,G1,B1
		pmaddwd     mm0,ZRZRYGYB                                        ; A0*0 + R0*0, G0*YG + B0*YB
		movq        mm3,QWORD PTR [edx+8*ebp]                           ; load 2nd image line
																		; A1,R1,G1,B1,A0,R0,G0,B0
		pmaddwd     mm1,YGYBZRYR                                        ; G1*YG + B1*YB, A0*0 + R0*YR
		movq        mm4,mm3                                             ; A1,R1,G1,B1,A0,R0,G0,B0
		movq        mm6,mm3                                             ; A1,R1,G1,B1,A0,R0,G0,B0

		pmaddwd     mm2,ZRYRZRZR                                        ; A*0 + R1*YR, 0*0 + 0*0

		paddd       mm0,YAddRoundFact                                   ; YAddRoundFact, G0*YG + B0*YB + YAddRoundFact
																		; A1,R1,G1,B1,A0,R0,G0,B0
		paddd       mm1,mm2
		psrlq       mm4,16                                              ; 00,00,A1,R1,G1,B1,A0,R0

		movq	    mm5,mm3
		paddd       mm0,mm1                                             ; Y1 * 8000h,Y0 * 8000h

		punpcklbw   mm3,Zeros                                           ; convert to 16-bits
																		; A0,R0,G0,B0
		punpckhbw   mm5,Zeros                                           ; Convert to 16-bits
																		; A1,R1,G1,B1
		paddw       mm7,mm3                                             ; A0+A1+A0,R0+R1+R0,G0+G1+G0,B0+B1+B0
		psrld       mm0,15                                              ; (Y1 * 8000h) / 8000h, (Y0 * 8000h) / 8000h
																		; Y1, Y0
		paddw       mm7,mm5                                             ; A1+A0+A1+A0,R1+R0+R1+R0,G1+G0+G1+G0,B1+B0+B1+B0
		punpcklbw   mm4,Zeros                                           ; convert to 16-bits
																		; G1,B1,A0,R0
		punpckhbw   mm6,Zeros                                           ; convert to 16-bits
																		; A1,R1,G1,B1
		pmaddwd     mm3,ZRZRYGYB                                        ; A0*0 + R0*0, G0*YG + B0*YB
		paddw       mm7,AvgRoundFactor                                    ; 2+A1+A0+A1+A0,2+R1+R0+R1+R0,2+G1+G0+G1+G0,2+B1+B0+B1+B0

		pmaddwd     mm4,YGYBZRYR                                        ; G1*YG + B1*YB, A0*0 + R0*YR
		psrlw       mm7,2                                               ; divide by 4 to get average of 4 RGB values
																		; A, R, G, B
		paddd       mm3,YAddRoundFact                                   ; 8000h, G0*YG + B0*YB + 8000h

		pmaddwd     mm6,ZRYRZRZR                                        ; A1*0 + R1*YR, 0*0 + 0*0,
		movq        mm1,mm7                                             ; A, R, G, B

		paddd       mm3,mm4                                             ; G1*YG + B1*YB + 8000h, R0*YR + G0*YG + B0*YB + 8000h

		pmaddwd     mm7,ZRVRUGUB                                        ; A*0 + R*VR, G*UG + B*UB
		movq        mm2,mm1                                             ; A, R, G, B

		paddd       mm3,mm6                                             ; R1*YR + G1*YG + B1*YB + 8000h, R0*YR + G0*YG + B0*YB + 8000h
		psllq       mm1,32                                              ; G, B, 0, 0
		psrlq       mm2,32                                              ; 0, 0, A, R

		pmaddwd     mm1,VGVBZRZR                                        ; G*VG + B*VB, 0*0 + 0*0
		psrld       mm3,15                                              ; (Y1 * 8000h) / 8000h, (Y0 * 8000h) / 8000h
            
		pmaddwd     mm2,ZRZRZRUR                                        ; 0, A*0 + R*UR
		paddd       mm7,VARUAR                                          ; (R*VR + 128)* 8000h, (G*UG + B*UB + 128) * 8000h

		paddd       mm1,mm2                                             ; (G*VG + B*VB) * 8000h, (R*UR) * 8000h
		packssdw    mm0,mm3                                             ; Y1,Y0 (2nd line), Y1,Y0

		paddd       mm7,mm1                                             ; (G*VG + B*VB + R*VR + 128 + 4000h) * 8000h, (G*UG + B*UB + R*UR + 128 + 4000h) * 8000h
		packuswb    mm0,mm4                                             ; XX,XX,XX,XX,Y1,Y0 (2nd line), Y1,Y0

		psrld       mm7,15                                              ; ((G*VG + B*VB + R*VR) * 8000h) / 8000h, ((G*UG + B*UB + R*UR) * 8000h) / 8000h
		movd        ebx,mm0                                             ; Y1,Y0 (2nd line), Y1,Y0

		packssdw    mm7,mm4                                             ; XX,XX, V, U
		mov         WORD PTR 0[eax+2*ebp],bx                            ; write Y1, Y2 to memory 
		shr         ebx,16                                              ; y's of 2nd line down
																		; 0,0, Y1, Y2 (2nd line)
		packuswb    mm7,mm4                                             ; XX,XX,XX,XX,XX,XX, V, U
		mov         WORD PTR 0[esi+2*ebp],bx                              ; write Y1, Y2 to memory
            
		movd        ebx,mm7                                             ; XX,XX, V, U

		mov         ecx,CConvParams[esp].UBuffer
		mov         BYTE PTR 0[ecx+ebp],bl                              ; write U to memory

		mov         ecx,CConvParams[esp].VBuffer
		mov         BYTE PTR 0[ecx+ebp],bh                              ; write V to memory

		inc         ebp                                                 ; increment Y output buffer ptr
		cmp			ebp,CConvParams[esp].ImageWidth						; setup loop ctr
		jne         WLoopStart    

		mov         ebx,CConvParams[esp].SrcPitch						; increment 2 source lines
		lea			edx,[edx+2*ebx]	  						   
		lea			edi,[edi+2*ebx]	  						   

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
