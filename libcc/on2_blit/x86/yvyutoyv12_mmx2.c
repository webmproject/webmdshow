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

#include <assert.h>

void CC_YVYUtoYV12_MMX( unsigned char *YVYUBuffer, int ImageWidth, int ImageHeight,
    unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch, int DstPitch )
{
    unsigned long ZERO_UV[2] =
    {
        0x00ff00ff, 0x00ff00ff
    };
    const unsigned long *_src = (const unsigned long *) YVYUBuffer;

    const long _swidth = ImageWidth;
    const long _sheight = ImageHeight;
    const long _spitch = SrcPitch;

    unsigned long *_yplane = (unsigned long *) YBuffer;
    unsigned long *_vplane = (unsigned long *) VBuffer;
    unsigned long *_uplane = (unsigned long *) UBuffer;

    long _ypitch = DstPitch;
    long _upitch = DstPitch / 2;
    long _vpitch = DstPitch / 2;


    unsigned long L_src;
    unsigned long L_spitch;
    unsigned long L_height;
    unsigned long L_blkwidth;
    unsigned long L_blkwidth_t;
    unsigned long L_yplane;
    unsigned long L_uplane;
    unsigned long L_vplane;
    unsigned long L_ypitch;
    unsigned long L_upitch;
    unsigned long L_vpitch;

    __asm
    {
        push        ebx
        push        ecx
        push        edx
        push        esi
        push        edi


        mov         edi,_yplane
        mov         esi,_src

        mov         eax,_vplane
        mov         edx,_uplane

        mov         L_src,esi
        mov         L_yplane,edi

        mov         L_vplane,eax
        mov         L_uplane,edx

        mov         ecx,_swidth
        mov         edx,_ypitch

        mov         ebx,_sheight
        mov         eax,_spitch

        shr         ecx,3
    ;;  dec         ebx                             ;last line is special -- optimazition reasons only

        shr ebx,1

        mov         L_height,ebx
        mov         L_spitch,eax

        mov         L_blkwidth,ecx
        mov         L_ypitch,edx

        mov         eax,_upitch
        mov         ebx,_vpitch

        mov         L_upitch,eax
        mov         L_vpitch,ebx


        mov         esi,L_src
        mov         edi,L_yplane

        mov         edx,L_uplane
        mov         ecx,L_vplane

    UYVY_YV12Loop_h:


        mov         eax,L_blkwidth
    ;-

        mov         L_blkwidth_t,eax
    ;-

        mov         eax,L_spitch

        mov         ebx,L_ypitch
    ;-

    UYVY_YV12Loop_w:
        movq        mm0,[esi+0]                     ;y3 v1 y2 u1 y1 v0 y0 u0
    ;-

        movq        mm1,[esi+8]                     ;y7 v3 y6 u3 y5 v2 y4 u2
        movq        mm4,mm0
        psrlw       mm4,8                           ;0 v1 0 u1 0 v0 0 u0

        movq        mm2,[esi+eax+0]                 ;yp3 vp1 yp2 up1 yp1 vp0 yp0 up0
    ;-

        movq        mm3,[esi+eax+8]                 ;yp7 vp3 yp6 up3 yp5 vp2 yp4 up2
        movq        mm5,mm1

        pand        mm0,ZERO_UV                     ;0 y3 0 y2 0 y1 0 y0
        psrlw       mm5,8                           ;0 v1 0 u1 0 v0 0 u0

        pand        mm1,ZERO_UV                     ;0 y7 0 y6 0 y5 0 y4
        movq        mm6,mm2

        movq        mm7,mm3
        packuswb    mm0,mm1                         ;y7 y6 y5 y4 y3 y2 y1 y0

        pand        mm2,ZERO_UV                     ;0 yp3 0 yp2 0 yp1 0 yp0
        psrlw       mm6,8                           ;0 vp1 0 up1 0 vp0 0 up0

        pand        mm3,ZERO_UV                     ;0 yp7 0 yp6 0 yp5 0 yp4
        psrlw       mm3,8                           ;0 vp3 0 up3 0 vp2 0 up2

        movq        [edi],mm0                       ;write 8 y's
        packuswb    mm2,mm3                         ;yp7 yp6 yp5 yp4 yp3 yp2 yp1 yp0

        paddw       mm4,mm6                         ;add u's and v's
        paddw       mm5,mm7                         ;add u's and v's

        movq        [edi + ebx],mm2                 ;write 8 y's
        psrlw       mm4,1

        psrlw       mm5,1
        add         esi,16

        movq        mm7,mm4
        punpcklbw   mm4,mm5                         ;0 0 v2 v0 0 0 u2 u0

        punpckhbw   mm7,mm5                         ;0 0 v3 v1 0 0 u3 u1
        movq        mm5,mm4

        punpcklbw   mm4,mm7                         ;0 0 0 0 u3 u2 u1 u0

        punpckhbw   mm5,mm7                         ;0 0 0 0 v3 v2 v1 v0

        movd        [edx],mm4                       ;u3 u2 u1 u0

        movd        [ecx],mm5                       ;u3 u2 u1 u0

        add         edx,4
        add         ecx,4

        add         edi,8
    ;-

        dec         DWORD PTR L_blkwidth_t
        jnz         UYVY_YV12Loop_w

        mov         esi,L_src
        mov         edi,L_yplane

        mov         edx,L_uplane
        mov         ecx,L_vplane

        add         edi,DWORD PTR L_ypitch
        add         edx,DWORD PTR L_upitch

        add         edi,DWORD PTR L_ypitch

        mov         eax,L_spitch
        add         ecx,DWORD PTR L_vpitch

        lea         esi,[esi + eax*2]
        mov         eax,L_height

        mov         L_src,esi
        mov         L_yplane,edi

        mov         L_uplane,edx
        mov         L_vplane,ecx

        dec         eax

        mov         L_height,eax
        jnz         UYVY_YV12Loop_h


        pop         edi
        pop         esi
        pop         edx
        pop         ecx
        pop         ebx

        emms
    }

}
