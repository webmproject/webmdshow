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
void CC_UYVYtoYV12_C(unsigned char* UYVYBuffer, int ImageWidth, int ImageHeight,
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
            u = UYVYBuffer[j * 4];
            u += UYVYBuffer[j * 4 + SrcPitch];
            u /= 2;
            pDestU[j] = u;

            //  Subsample V
            v = UYVYBuffer[j * 4 + 2];
            v += UYVYBuffer[j * 4 + 2 + SrcPitch];
            v /= 2;
            pDestV[j] = v;

            //  Copy Y
            pDestY[j * 2] = UYVYBuffer[j * 4 + 1];
            pDestY[j * 2 + 1] = UYVYBuffer[j * 4 + 3];
            pDestY[DstPitch + j * 2] = UYVYBuffer[j * 4 + SrcPitch + 1];
            pDestY[DstPitch + j * 2 + 1] = UYVYBuffer[j * 4 + SrcPitch + 3];
        }

        pDestY += DstPitch * 2;  //  Step 2 Y rows
        pDestU += DstPitch / 2;  //  Step 1 U row
        pDestV += DstPitch / 2;  //  Step 1 V row
        UYVYBuffer += SrcPitch * 2;  //  Step 2 source rows
    }

    return;
}
