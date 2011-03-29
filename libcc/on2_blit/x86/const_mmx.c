;//==========================================================================
;//
;//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
;//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
;//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
;//  PURPOSE.
;//
;//  Copyright (c) 1999 - 2001  On2 Technologies Inc. All Rights Reserved.
;//
;//--------------------------------------------------------------------------


#include <stdio.h>


/****** params for blitters
typedef struct {
    int     YWidth;
    int     YHeight;
    int     YStride;

    int     UVWidth;
    int     UVHeight;
    int     UVStride;

    char *  YBuffer;
    char *  UBuffer;
    char *  VBuffer;

    unsigned char     *uvStart;
    int     uvDstArea;
    int     uvUsedArea;
} YUV_BUFFER_CONFIG;
****************************/


__int64 WK_CLEAR_UP_5BYTES = 0x00000000000ffffff;
__int64 WK_CLEAR_7_3_BYTES = 0x000ffffff00ffffff;

__int64 WK_johnsTable_MMX[] = {
        0x0006060600000000,
        0x0007070700010101,

        0x0002020200040404,
        0x0003030300050505,

        0x0007070700010101,
        0x0006060600000000,

        0x0003030300050505,
        0x0002020200040404
};

__int64 WK_RGB_MULFACTOR_555 = 0x2000000820000008;
__int64 WK_RB_MASK_555 = 0x00f800f800f800f8;
__int64 WK_G_MASK_555 = 0x0000f8000000f800;
__int64 WK_RGB_MULFACTOR_565 = 0x2000000420000004;
__int64 WK_RB_MASK_565 = 0x00f800f800f800f8;
__int64 WK_G_MASK_565 = 0x0000fc000000fc00;
__int64 WK_MASK_YY_MMX = 0x00ff00ff00ff00ff;
__int64 WK_MASK_BYTE0 = 0x00000000000000ff;

int WK_johnsTable[] = {
         0x7010600,
         0x3050204,
         0x6000701,
         0x2040305
};
