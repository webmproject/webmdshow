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

//------------------------------------------------------------------------------
//  See CCLIB.H for a more detailed description of this function
void CC_YUY2toYV12_C(unsigned char* YUY2Buffer, int ImageWidth, int ImageHeight,
    unsigned char* YBuffer, unsigned char* UBuffer, unsigned char* VBuffer, int SrcPitch, int DstPitch)
{
    unsigned char* pDestY = YBuffer;
    unsigned char* pDestU = UBuffer;
    unsigned char* pDestV = VBuffer;
    int i;
    int j;

    //  For each destination U and V pixel
    for (i = 0; i < ImageHeight / 2; ++i)
    {
        for (j = 0; j < ImageWidth / 2; ++j)
        {
            int u;
            int v;

            //  Subsample U
            u = YUY2Buffer[j * 4 + 1];
            u += YUY2Buffer[j * 4 + 1 + SrcPitch];
            u /= 2;
            pDestU[j] = u;

            //  Subsample V
            v = YUY2Buffer[j * 4 + 3];
            v += YUY2Buffer[j * 4 + 3 + SrcPitch];
            v /= 2;
            pDestV[j] = v;

            //  Copy Y
            pDestY[j * 2] = YUY2Buffer[j * 4];
            pDestY[j * 2 + 1] = YUY2Buffer[j * 4 + 2];
            pDestY[DstPitch + j * 2] = YUY2Buffer[j * 4 + SrcPitch];
            pDestY[DstPitch + j * 2 + 1] = YUY2Buffer[j * 4 + SrcPitch + 2];
        }

        pDestY += DstPitch * 2;  //  Step 2 Y rows
        pDestU += DstPitch / 2;  //  Step 1 U row
        pDestV += DstPitch / 2;  //  Step 1 V row
        YUY2Buffer += SrcPitch * 2;  //  Step 2 source rows
    }

    return;
}
