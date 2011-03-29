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
#include "vpxblit.h"


typedef unsigned int RGBColorComponent;

#ifndef ON2_NO_GLOBALS
 extern RGBColorComponent *WK_ClampOriginR;
 extern RGBColorComponent *WK_ClampOriginG;
 extern RGBColorComponent *WK_ClampOriginB;
 extern unsigned char WK_YforY[];		
 extern unsigned char WK_VforRG[];
 extern unsigned char WK_UforBG[];
#else
# include "on2_global_handling.h"
# define  WK_ClampOriginR ((RGBColorComponent *)ON2GLOBALm(vpxblit, WK_ClampOriginR))
# define  WK_ClampOriginG ((RGBColorComponent *)ON2GLOBALm(vpxblit, WK_ClampOriginG))
# define  WK_ClampOriginB ((RGBColorComponent *)ON2GLOBALm(vpxblit, WK_ClampOriginB))
# define  WK_YforY_ptr ((unsigned char *)ON2GLOBALm(vpxblit, WK_YforY))
# define  WK_VforRG_ptr ((unsigned char *)ON2GLOBALm(vpxblit, WK_VforRG))
# define  WK_UforBG_ptr ((unsigned char *)ON2GLOBALm(vpxblit, WK_UforBG))
#endif

void 
bct00_c(unsigned char *_ptrScreen, int thisPitch, VPX_BLIT_CONFIG *src)
{
    unsigned char *ptrScrn = (unsigned char *)_ptrScreen;
    
    unsigned char *YBuffer = (unsigned char *)src->YBuffer;
    unsigned char *UBuffer = (unsigned char *)src->UBuffer;
    unsigned char *VBuffer = (unsigned char *)src->VBuffer;

    unsigned int yTemp;
    unsigned short uTemp, vTemp;

    int i, j;

    int R, G, B;
    int R1, G1, B1;
    int RP,GP,BP;
    unsigned int Y0, Y1, CbforB, CbforG, CrforR, CrforG;

    typedef unsigned int tempColor;
    tempColor  *tR = (tempColor *) WK_ClampOriginR;
    tempColor  *tG = (tempColor *) WK_ClampOriginG;
    tempColor  *tB = (tempColor *) WK_ClampOriginB;
#ifdef ON2_NO_GLOBALS
	unsigned char *WK_YforY =  WK_YforY_ptr;		
	unsigned char *WK_VforRG = WK_VforRG_ptr;
	unsigned char *WK_UforBG = WK_UforBG_ptr;
#endif
	

    for(i = 0; i < src->YHeight; i += 1)
    {
        int x;
   
        for(j = 0, x = 0; j < src->YWidth/4; j += 1, x += 4)
        {
        
            /* get 4 y's */
        	yTemp = ((unsigned int *) YBuffer)[j*1];

            /* get 2 uv's */
            uTemp = ((unsigned short *) UBuffer)[j*1];
            vTemp = ((unsigned short *) VBuffer)[j*1];
        	
            Y0 = REREFERENCE(WK_YforY[BYTE_ZERO(yTemp)]);
            Y1 = REREFERENCE(WK_YforY[BYTE_ONE(yTemp)]);
    
            CbforB 	= REREFERENCE(WK_UforBG[BYTE_ZERO_UV(uTemp)<<1]);
            CbforG 	= REREFERENCE(WK_UforBG[(BYTE_ZERO_UV(uTemp)<<1)+4]);
    
            CrforR 	= REREFERENCE(WK_VforRG[BYTE_ZERO_UV(vTemp)<<1]);
            CrforG 	= REREFERENCE(WK_VforRG[(BYTE_ZERO_UV(vTemp)<<1)+4]);
        
    
            R = Y0 + CrforR;
            B = Y0 + CbforB;
            G = Y0 - CbforG - CrforG;
            
            RP = tR[R];
            GP = tG[G];
            BP = tB[B];
            
            /* reuse Y0 */
#if CONFIG_BIG_ENDIAN
            Y0 = (((RP>>8) | (GP<<8) | (BP<<24)));
#else
            Y0 = ((RP | GP | BP));
#endif
    
            R1 = Y1 + CrforR;
            B1 = Y1 + CbforB;
            G1 = Y1 - CbforG - CrforG;
            
            RP = tR[R1];
            GP = tG[G1];
            BP = tB[B1];
            
#if CONFIG_BIG_ENDIAN
            Y1 = (((RP>>8) | (GP<<8) | (BP<<24)));
#else
            Y1 = ((RP | GP | BP));
#endif
            ((unsigned int *) ptrScrn)[x] = (unsigned int)(Y0);
            ((unsigned int *) ptrScrn)[x+1] = (unsigned int)(Y1);
    
        	Y0 = REREFERENCE(WK_YforY[BYTE_TWO(yTemp)]);
        	Y1 = REREFERENCE(WK_YforY[BYTE_THREE(yTemp)]);

       		CbforB 	= REREFERENCE(WK_UforBG[BYTE_ONE_UV(uTemp)<<1]);
        	CbforG 	= REREFERENCE(WK_UforBG[(BYTE_ONE_UV(uTemp)<<1)+4]);

        	CrforR 	= REREFERENCE(WK_VforRG[BYTE_ONE_UV(vTemp)<<1]);
        	CrforG 	= REREFERENCE(WK_VforRG[(BYTE_ONE_UV(vTemp)<<1)+4]);

        	R = Y0 + CrforR;
        	B = Y0 + CbforB;
        	G = Y0 - CbforG - CrforG;
        	
        	R1 = Y1 + CrforR;
        	B1 = Y1 + CbforB;
        	G1 = Y1 - CbforG - CrforG;
            
            
            RP = tR[R];
            GP = tG[G];
            BP = tB[B];
            
            /* reuse Y0 */
#if CONFIG_BIG_ENDIAN
            Y0 = (((RP>>8) | (GP<<8) | (BP<<24)));
#else
            Y0 = ((RP | GP | BP));
#endif
            
            RP = tR[R1];
            GP = tG[G1];
            BP = tB[B1];
     
#if CONFIG_BIG_ENDIAN
            Y1 = (((RP>>8) | (GP<<8) | (BP<<24)));
#else
            Y1 = ((RP | GP | BP));
#endif
            ((unsigned int *) ptrScrn)[x+2] = (unsigned int)(Y0);
            ((unsigned int *) ptrScrn)[x+3] = (unsigned int)(Y1);
   
        } /* inner for */

        ptrScrn += thisPitch;
        YBuffer -= src->YStride;

        /* see mmx asm code on how to remove this branch */
        if(i & 1)
        {
            UBuffer -= src->UVStride;
            VBuffer -= src->UVStride;
        }
    } /* outer for */
}

