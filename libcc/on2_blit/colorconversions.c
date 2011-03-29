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


#ifdef _DEBUG
#include <assert.h>
#include <stdio.h>
#endif
#include "colorconversions.h"
/*
 * function prototypes
 */
void DefaultFunction( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                      unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch );


/*
 * Global Function pointers
 * Once InitCCLib is called they should point to the fastest functions that are able
 * to run on the current machine
 */
void (*RGB32toYV12)( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight, 
                            unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch ) = DefaultFunction;

void (*RGB24toYV12)( unsigned char *RGBBuffer, int ImageWidth, int ImageHeight,
                            unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch )= DefaultFunction;

void (*UYVYtoYV12)( unsigned char *UYVYBuffer, int ImageWidth, int ImageHeight,
                    unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch, int DstPitch ) = DefaultFunction;

void (*YUY2toYV12)( unsigned char *YUY2Buffer, int ImageWidth, int ImageHeight,
                    unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch, int DstPitch ) = DefaultFunction;

void (*YVYUtoYV12)( unsigned char *YVYUBuffer, int ImageWidth, int ImageHeight,
                           unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch ) = DefaultFunction;



/*
 * **-DefaultFunction
 *
 * Our function pointers will by default be initilized to point to this function.  The purpose
 * of this function is to prevent us from going off into the weeds if the init function is not
 * called.
 *
 * Assumptions:
 *  None
 *
 * Input:
 *  Does not matter
 *
 * Output:
 *  If init function not called will force an error
 */
void DefaultFunction( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                      unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch )
{
   char *CharPtr = 0;
   
#ifdef _DEBUG
   assert( 0 ); // InitCCLib MUST be called before using this function
#else
   // force error
   *CharPtr = 1;
#endif
}

/*
 * **-InitCCLib
 *
 * See cclib.h for a more detailed description of this function
 */
int 
InitCCLib(void)
{
	RGB32toYV12 = CC_RGB32toYV12_C;
	RGB24toYV12 = CC_RGB24toYV12_C;
	UYVYtoYV12  = CC_UYVYtoYV12_C;
	YUY2toYV12  = CC_YUY2toYV12_C;
	YVYUtoYV12  = CC_YVYUtoYV12_C;
	return 0;
}

/*
 * **-DeInitCCLib
 *
 * See cclib.h for a more detailed description of this function
 */
void DeInitCCLib( void )
{
}
