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



void bcy00_c(unsigned char *_ptrScreen, int thisPitch, VPX_BLIT_CONFIG *src)
{
    unsigned char *ptrScrn = (unsigned char *)_ptrScreen;
    
    unsigned char *YBuffer = (unsigned char *)src->YBuffer;
    unsigned char *UBuffer = (unsigned char *)src->UBuffer;
    unsigned char *VBuffer = (unsigned char *)src->VBuffer;

    unsigned int yTemp;
    int i, j;

    for(i = 0; i < src->YHeight; i += 1)
    {
        int x;
   
        for(j = 0, x = 0; j < src->YWidth; j += 4, x += 2)
        {
        
#if CONFIG_BIG_ENDIAN
            /* get 2 y's */
        	yTemp = (unsigned int) (YBuffer[j+0] << 24 | ((YBuffer[j+1]) << 8));

            /* get 1 uv's */
            yTemp |= (unsigned int)(UBuffer[x+0] << 16);
            yTemp |= (unsigned int)(VBuffer[x+0]);
        	
            ((unsigned int *) ptrScrn)[x] = yTemp;

            /* get 2 y's */
        	yTemp = (unsigned int) (YBuffer[j+2] << 24 | ((YBuffer[j+3]) << 8));

            /* get 1 uv's */
            yTemp |= (unsigned int)(UBuffer[x+1] << 16);
            yTemp |= (unsigned int)(VBuffer[x+1]);

            ((unsigned int *) ptrScrn)[x+1] = yTemp;
#else
            /* get 2 y's */
        	yTemp = (unsigned int) (YBuffer[j+0] | ((YBuffer[j+1]) << 16));

            /* get 1 uv's */
            yTemp |= (unsigned int)(UBuffer[x+0] << 8);
            yTemp |= (unsigned int)(VBuffer[x+0] << 24);
        	
            ((unsigned int *) ptrScrn)[x] = yTemp;

            /* get 2 y's */
        	yTemp = (unsigned int) (YBuffer[j+2] | ((YBuffer[j+3]) << 16));

            /* get 1 uv's */
            yTemp |= (unsigned int)(UBuffer[x+1] << 8);
            yTemp |= (unsigned int)(VBuffer[x+1] << 24);

            ((unsigned int *) ptrScrn)[x+1] = yTemp;
#endif
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

