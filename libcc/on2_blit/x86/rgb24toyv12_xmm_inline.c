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
void CC_RGB24toYV12_XMM_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
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
		mov     edi,CConvParams[esp].RGBABuffer							; Input Buffer Ptr
		mov     eax,CConvParams[esp].YBuffer							; Load Y buffer ptr
		mov	    edx,CConvParams[esp].SrcPitch							; source pitch
		mov     esi,CConvParams[esp].YPitch								; destination pitch

HLoopStart:
		mov	    ebp,CConvParams[esp].ImageWidth							; setup loop ctr

WLoopStart:
		;
		; ESP = Stack Pointer                      MM0 = Free
		; ESI = YPitch                             MM1 = Free
		; EDI = Input Buffer Ptr                   MM2 = Free
		; EBP = Width Ctr                          MM3 = Free
		; EBX = Scratch                            MM4 = Free
		; ECX = Free                               MM5 = Free
		; EDX = input pitch						   MM6 = Free
		; EAX = Y output buff ptr                  MM7 = Used to accumulate sum of R,G,B values so that they can be averaged
		;

		; process two pixels from first image row
		movd        mm0,DWORD PTR 0[edi]                                ; 00,00,00,00,B1,R0,G0,B0
		movd        mm2,DWORD PTR 2[edi]								; 00 00 00 00 R1,G1,B1,R0	

		psllq       mm2,16												; 00 00 R1,G1,B1,R0,00,00 
		por			mm0,mm2												; 00 00 R1,G1,B1,R0,G0,B0

		movd        mm1,DWORD PTR 0[edi+edx]                            ; 00,00,00,00,B1,R0,G0,B0 (2nd row)
		movd        mm2,DWORD PTR 2[edi+edx]                            ; 00,00,00,00,R1,G1,B1,R0 (2nd row)

		psllq       mm2,16												; 00 00 R1,G1,B1,R0,00,00 
		por			mm1,mm2												; 00 00 R1,G1,B1,R0,G0,B0

		movq        mm2,mm0                                             ; G2,B2,R1,G1,B1,R0,G0,B0
		movq        mm4,mm1                                             ; G2,B2,R1,G1,B1,R0,G0,B0 (2nd row)

		movq        mm5,mm0                                             ; G2,B2,R1,G1,B1,R0,G0,B0
		movq        mm6,mm1                                             ; G2,B2,R1,G1,B1,R0,G0,B0 (2nd row)

		punpcklbw   mm0,Zeros                                           ; B1,R0,G0,B0
		;prefetch    6[edi]

		punpcklbw   mm1,Zeros                                           ; B1,R0,G0,B0 (2nd row)
		;prefetch    6[edi+edx]

		psllq       mm5,8                                               ; B2,R1,G1,B1,R0,G0,B0,00
		psllq       mm6,8                                               ; B2,R1,G1,B1,R0,G0,B0,00 (2nd row)

		psrlq       mm2,16                                              ; 00,00,G2,B2,R1,G1,B1,R0
		psrlq       mm4,16                                              ; 00,00,G2,B2,R1,G1,B1,R0 (2nd row)

		punpckhbw   mm5,Zeros                                           ; B2,R1,G1,B1
		punpckhbw   mm6,Zeros                                           ; B2,R1,G1,B1 (2nd row)

		movq        mm7,mm0                                             ; B1,R0,G0,B0

		punpcklbw   mm2,Zeros                                           ; R1,G1,B1,R0
		punpcklbw   mm4,Zeros                                           ; R1,G1,B1,R0 (2nd row)

		paddd       mm7,mm1                                             ; B1 + B1 , R0 + R0, G0 + G0 ,B0 + B0

		pmaddwd     mm0,YBZRYGYB                                        ; (B1*YB + R0*0) * 8000h, (G0*YB + B0*YB) * 8000h
		paddd       mm7,mm5                                             ; B2+B1+B1, R1+R0+R0, G1+G0+G0, B1+B0+B0

		pmaddwd     mm1,YBZRYGYB                                        ; (B1*YB + R0*0) * 8000h, (G0*YB + B0*YB) * 8000h (2nd row)
		paddd       mm7,mm6                                             ; B2+B2+B1+B1, R1+R1+R0+R0, G1+G1+G0+G0, B1+B1+B0+B0

		pmaddwd     mm2,YRYGZRYR                                        ; (R1*YR + G1*YG) * 8000h, (B1*0 + R0*YR) * 8000h
		pmaddwd     mm4,YRYGZRYR                                        ; (R1*YR + G1*YG) * 8000h, (B1*0 + R0*YR) * 8000h (2nd row)

		paddd       mm7,AvgRoundFactor                                  ; 2+B2+B2+B1+B1, 2+R1+R1+R0+R0, 2+G1+G1+G0+G0, 2+B1+B1+B0+B0    
		paddd       mm0,YARYAR                                          ; (B1*YB + R0*0 + 16 + 4000h) * 8000h, (G0*YB + B0*YB + 16 + 4000h) * 8000h
		paddd       mm1,YARYAR                                          ; (B1*YB + R0*0 + 16 + 4000h) * 8000h, (G0*YB + B0*YB + 16 + 4000h) * 8000h (2nd row)

		psrlw       mm7,2                                               ; Avg(B), Avg(R), Avg(G), Avg(B)

		paddd       mm0,mm2                                             ; (R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h, (R0*YR + G0*YB + B0*YB + 16 + 4000h) * 8000h
		paddd       mm1,mm4                                             ; (R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h, (R0*YR + G0*YB + B0*YB + 16 + 4000h) * 8000h (2nd row)

		;--
		movq        mm2,mm7                                             ; Avg(B), Avg(R), Avg(G), Avg(B)
    
		pshufw      mm7,mm7,044h                                        ; G, B, G, B
		pshufw      mm2,mm2,022h                                        ; X, R, X, R

		psrld       mm0,15                                              ; ((R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h) / 8000h, ((R0*YR + G0*YB + B0*YB + 16 + 4000h) * 8000h) / 8000h
		psrld       mm1,15                                              ; ((R1*YR + G1*YG + B1*YB + 16 + 4000h) * 8000h) / 8000h, ((R0*YR + G0*YB + B0*YB + 16 + 4000h) * 8000h) / 8000h (2nd row)

		pmaddwd     mm7,VGVBUGUB                                        ; (G*VG + B*VB) * 8000h, (G*UG + B*UB) * 8000h

		packssdw    mm0,mm1                                             ; Y1, Y0, Y1, Y0
    
		pmaddwd     mm2,ZRVRZRUR                                        ; (X*0 + R*VR) * 8000h, (X*0 + R*UR) * 8000h
		packuswb    mm0,mm6                                             ; X, X, X, X, Y1, Y0, Y1, Y0

		pmaddwd     mm3,VGVBZRZR                                        ; (G*VG + B*VB) * 8000h, (0*0 + 0*0) * 8000h
		movd        ebx,mm0                                             ; Y1, Y0, Y1, Y0

		paddd       mm7,VARUAR                                          ; (G*VG + B*VB + 128 + 4000h) * 8000h, (G*UG + B*UB + 128 + 4000h) * 8000h

		mov         WORD PTR 0[eax],bx                                  ; write first line of y's to memory
		shr         ebx,16                                              ; 0, 0, Y1, Y0 (2nd line)

		paddd       mm7,mm2                                             ; (R*VR + G*VG + B*VB + 128 + 4000h) * 8000h, (R*UR + G*UG + B*UB + 128 + 4000h) * 8000h
		mov         WORD PTR 0[eax+esi],bx                              ; write 2nd line of y's to memory

		psrld       mm7,15                                              ; ((G*VG + B*VB + R*VR + 128 + 4000h) * 8000h) / 8000h, ((R*UR + G*UG + B*UB + 128 + 4000h) * 8000h) / 8000h

		mov         ecx,CConvParams[esp].UBuffer                 ;

		packssdw    mm7,mm0                                             ; X, X, V, U
    
		packuswb    mm7,mm0                                             ; X, X, X, X, X, X, V, U
		add         eax,2                                               ; increment Y output buffer ptr

		movd        ebx,mm7                                             ; X, X, V, U
    
		mov         BYTE PTR 0[ecx],bl                                  ; write u to output
		inc         ecx                                                 ; point to next U

		mov         CConvParams[esp].UBuffer,ecx						; preserve pointer
		mov         ecx,CConvParams[esp].VBuffer                 
		add         edi,6                                               ; point to next block of input buffers

		mov         BYTE PTR 0[ecx],bh                                  ; write v to output buffer
		inc         ecx                                                 ; point to next V

		mov         CConvParams[esp].VBuffer,ecx						; preserve pointer

		sub         ebp,2                                               ; decrement loop ctr
		jnz         WLoopStart    


		mov			ecx,CConvParams[esp].ImageWidth						; get the width
		sub         eax,ecx                                             ; subtract width from y output buffer pointer (back to start of line)
		add			eax,esi
		add         eax,esi												; mov eax down 2 y pitches ( we do 2 lines at a time)

		sub         edi,ecx												; point destination to start of line ( subtract width 3 times)
		sub         edi,ecx								  
		sub         edi,ecx										
		add         edi,edx                                             ; mov edi down 2 lines (we do 2 lines at a time)
		add         edi,edx                                              

		sar			ecx,1												; conver to a u v pitch 

		sub  	    CConvParams[esp].UBuffer,ecx						; subtract out the width (back to start of line)  
		sub  	    CConvParams[esp].VBuffer,ecx						; subtract out the width  
		mov			ecx,esi								  
		sar         ecx,1												; convert y pitch to uv pitch
		add  	    CConvParams[esp].UBuffer,ecx						; add in uv pitch to u
		add  	    CConvParams[esp].VBuffer,ecx						; add in uv pitch to v

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
