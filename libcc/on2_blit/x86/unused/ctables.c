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
#include <math.h>

unsigned long xditherPats[] = 
{  
	0x00060107, 
	0x04020503,
	0x01070006,
    0x05030402
};


void BuildClampTables(void);


/* lookups to avoid multiplies -- 24/32 bit conversions */
long WK_YforY[256];	
long WK_UforBG[256*2];
long WK_VforRG[256*2];

/*

  Video Demystified  by Keith Jack (Harris is Publisher).

	r = 1.164(Y-16) + 1.596(Cr - 128)
	g = 1.164(Y - 16) - 0.813(Cr - 128) - 0.392(Cb - 128)
	b = 1.164(Y - 16) + 2.017 (Cb - 128)

  Note that here:	Cr = Y - R
			and		Cb = Y - B

		V is used as Cr, below and u is used as Cb.
		Part of the mismatch between this and older versions of the
		YUV->RGB transcode comes from the fact that the range of Y is 16 to 235
		and U and V (Cb, Cr) is 16 to 240, so some stretching has to
		be done to get to the space where the range of R,G and B is 0-255
*/

/*
    Also see Frequently Asked Question About Color by Charles A. Poynton

    ftp://ftp.inforamp.net/pub/users/poynton/doc/colour/ColorFAQ.pdf

*/

/* tables used by MMX blitters */
unsigned long WK_YforY_MMX[256];	
unsigned long WK_UforBG_MMX[256];
unsigned long WK_VforRG_MMX[256];

#define ROUND(x) ((x)<0?(x)-.5:(x)+.5)

void BuildYUY2toRGB(void)
{
    int i,j;

    BuildClampTables();

	for (i = 0; i < 256; i++) 
        WK_YforY[i] =(int) ROUND((i-16.0)*1.164);   

	for (i = 0, j = 0; i < 512; i+=2, j++) {
		WK_UforBG[i] = (int) ROUND( (j-128.0)* 2.017 );
		WK_UforBG[i+1] = (int) ROUND( (j-128.0)* 0.392);

		WK_VforRG[i] = (int) ROUND( (j-128.0) * 1.596);
		WK_VforRG[i+1] = (int) ROUND( (j-128.0) * 0.813);
    }

/* table build for MMX 16bit blitters */
{
    unsigned long x,z;

	for (i = 0; i < 256; i++) {
        z =(unsigned short) ROUND( (i - 16.0) * 1.164 ) ;   
        WK_YforY_MMX[i] = z | z<<16;
    }

	for (i = 0, j = 0; i < 256; i+=1, j++) {
        /* CbforB */
		z = (unsigned short) ROUND( (j - 128.0) * 2.017);

        /* CbforG */
		x = (unsigned short) ROUND( (j - 128.0) * -0.392);
		WK_UforBG_MMX[i] = z | x<<16;

        /* CrforG */
		z = (unsigned short) ROUND( (j - 128.0) * -0.813);

        /* CrforR */
		x = (unsigned short) ROUND( (j - 128.0) * 1.596);
		WK_VforRG_MMX[i] = z | x<<16;
    }
}
 
}


/* marky marks stuff */
typedef unsigned long RGBColorComponent;
RGBColorComponent WK_ClampTableR[1024];
RGBColorComponent *WK_ClampOriginR;
RGBColorComponent WK_ClampTableG[1024];
RGBColorComponent *WK_ClampOriginG;
RGBColorComponent WK_ClampTableB[1024];
RGBColorComponent *WK_ClampOriginB;

#define MAX_DITHER_VALUE  7
#define MIN_OUTRANGE -276 - 4  /* rounding the first terms to be safe */
#define MAX_OUTRANGE 534 + 1 + MAX_DITHER_VALUE 


RGBColorComponent WK_ClampTableR555[1024];
RGBColorComponent *WK_ClampOriginR555;
RGBColorComponent WK_ClampTableG555[1024];
RGBColorComponent *WK_ClampOriginG555;
RGBColorComponent WK_ClampTableB555[1024];
RGBColorComponent *WK_ClampOriginB555;

RGBColorComponent WK_ClampTableR565[1024];
RGBColorComponent *WK_ClampOriginR565;
RGBColorComponent WK_ClampTableG565[1024];
RGBColorComponent *WK_ClampOriginG565;
RGBColorComponent WK_ClampTableB565[1024];
RGBColorComponent *WK_ClampOriginB565;

void BuildClampTables(void)
{   int t;

	WK_ClampOriginR = (RGBColorComponent *) &(WK_ClampTableR[256+128]);  /* chosen so that indexes will not outrange out storage */
	WK_ClampOriginG = (RGBColorComponent *) &(WK_ClampTableG[256+128]);  /* chosen so that indexes will not outrange out storage */
	WK_ClampOriginB = (RGBColorComponent *) &(WK_ClampTableB[256+128]);  /* chosen so that indexes will not outrange out storage */

	WK_ClampOriginR555 = (RGBColorComponent *) &(WK_ClampTableR555[(256+128)]); 
	WK_ClampOriginG555 = (RGBColorComponent *) &(WK_ClampTableG555[(256+128)]); 
	WK_ClampOriginB555 = (RGBColorComponent *) &(WK_ClampTableB555[(256+128)]); 

	WK_ClampOriginR565 = (RGBColorComponent *) &(WK_ClampTableR565[(256+128)]); 
	WK_ClampOriginG565 = (RGBColorComponent *) &(WK_ClampTableG565[(256+128)]); 
	WK_ClampOriginB565 = (RGBColorComponent *) &(WK_ClampTableB565[(256+128)]); 

	for (t = MIN_OUTRANGE; t <= MAX_OUTRANGE; t++) {
   		if (t < 0) {
        	WK_ClampOriginR[t] = 0;
        	WK_ClampOriginG[t] = 0;
        	WK_ClampOriginB[t] = 0;

        	WK_ClampOriginR555[(t)+0] = 0;
        	WK_ClampOriginG555[(t)+0] = 0;
        	WK_ClampOriginB555[(t)+0] = 0;


            /* 565 */
        	WK_ClampOriginR565[(t)+0] = 0;
        	WK_ClampOriginG565[(t)+0] = 0;
        	WK_ClampOriginB565[(t)+0] = 0;

        }
    	else if (t > 255) {
        	WK_ClampOriginR[t] = (255 << 16);
        	WK_ClampOriginG[t] = (255 << 8);
        	WK_ClampOriginB[t] = 255;

        	WK_ClampOriginR555[(t)+0] = (0x1f << 10);
        	WK_ClampOriginG555[(t)+0] = (0x1f << 5);
        	WK_ClampOriginB555[(t)+0] = 0x1f;

            /* 565 */
        	WK_ClampOriginR565[(t)+0] = (0x1f << 11);
        	WK_ClampOriginG565[(t)+0] = (0x3f << 5);
        	WK_ClampOriginB565[(t)+0] = 0x1f;
        }
    	else {
        	WK_ClampOriginR[t] = (t << 16);
        	WK_ClampOriginG[t] = (t << 8);
        	WK_ClampOriginB[t] = t;

        	WK_ClampOriginR555[(t)+0] = ((t>>3)&0x1f) << 10;
        	WK_ClampOriginG555[(t)+0] = ((t>>3)&0x1f) << 5;
        	WK_ClampOriginB555[(t)+0] = (t>>3)&0x1f;

            /* 565 */
        	WK_ClampOriginR565[(t)+0] = ((t>>3)&0x1f) << 11;
        	WK_ClampOriginG565[(t)+0] = ((t>>2)&0x3f) << 5;
        	WK_ClampOriginB565[(t)+0] = (t>>3)&0x1f;

        }
	}
} /* BuildClampTables */
