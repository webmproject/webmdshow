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


#ifndef CCSTR_H
#define CCSTR_H

#if CONFIG_BIG_ENDIAN

typedef struct RGB32Pixel
{
   unsigned char Alpha;
   unsigned char Red;
   unsigned char Green;
   unsigned char Blue;
} RGB32Pixel;

typedef struct RGB24Pixel
{
   unsigned char Blue;
   unsigned char Green;
   unsigned char Red;
} RGB24Pixel;

#else
// Little endian ordering is the default.
typedef struct RGB32Pixel
{
   unsigned char Blue;
   unsigned char Green;
   unsigned char Red;
   unsigned char Alpha;
} RGB32Pixel;

typedef struct RGB24Pixel
{
   unsigned char Blue;
   unsigned char Green;
   unsigned char Red;
} RGB24Pixel;

#endif

/*
 * not actually a single pixel.  Structure contains
 * data for two pixels.
 */
typedef struct YVYUPixel
{
   unsigned char Y0;
   unsigned char V;
   unsigned char Y1;
   unsigned char U;
} YVYUPixel;

//#define True  1
//#define False 0

#endif /* CCSTR_H */
