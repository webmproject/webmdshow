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
#include "colorconversions.h"
#include "lutbl.h"
#include "ccstr.h"

/*
 * **-CC_YVYUtoYV12_C
 *
 * See CCLIB.H for a more detailed description of this function
 */
void CC_YVYUtoYV12_C( unsigned char *src, int ImageWidth, int ImageHeight,
                      unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch )
{
    int i,j;

    unsigned char *YDst=YBuffer;
    unsigned char *UDst=UBuffer;
    unsigned char *VDst=VBuffer;

    // for each destination u and v pixel
    for(i=0;i<ImageHeight/2;i++)
    {
        for(j=0;j<ImageWidth/2;j++)
        {
            int u,v;

            // subsample u
            u = src[j*4+3];
            u +=src[j*4+3+SrcPitch];
            u /= 2;
            UDst[j] = u ;

            // subsample v
            v = src[j*4+1];
            v += src[j*4+1+SrcPitch];
            v /= 2;
            VDst[j] = v;

            // calculate the 4 y's
            YDst[j*2] = src[j*4];
            YDst[j*2+1] = src[j*4+2];
            YDst[DstPitch + j*2] = src[j*4+SrcPitch];
            YDst[DstPitch + j*2 + 1] = src[j*4+SrcPitch + 2];

        }
        YDst+=DstPitch*2;   // step 2 y rows
        UDst+=DstPitch/2;   // step 1 u row
        VDst+=DstPitch/2;   // step 1 v row
        src+=SrcPitch*2; // step 2 source rows
    }
}
