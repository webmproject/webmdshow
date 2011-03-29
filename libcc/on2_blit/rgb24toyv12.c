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
 * **-CC_RGB24toYV12_C
 *
 * See cclib.h for a more detailed desciption of the function
 *
 */
void CC_RGB24toYV12_C( unsigned char *RGBBuffer, int ImageWidth, int ImageHeight,
                       unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch )
{
   unsigned char *YPtr1, *YPtr2;                        /* Local pointers to buffers */
   unsigned char *UPtr, *VPtr;
   RGB24Pixel    *RGBPtr1, *RGBPtr2;
   int           WidthCtr, HeightCtr;
   int           RSum, GSum, BSum;
   int           YScaled, UScaled, VScaled;             /* scaled Y, U, V values */
   const int          *YRMultPtr;
   const int          *YGMultPtr;
   const int          *YBMultPtr;
   const int          *URMultPtr;
   const int          *UGMultPtr;
   const int          *UBVRMultPtr;
   const int          *VGMultPtr;
   const int          *VBMultPtr;

   assert( !(ImageHeight & 0x1) ); /* Height must be even otherwise we will behave incorrectly */
   assert( !(ImageWidth & 0x1) );  /* Width must be even otherwise we will behave incorrectly */

   YRMultPtr   = YRMult;   
   YGMultPtr   = YGMult;   
   YBMultPtr   = YBMult;   
   URMultPtr   = URMult;   
   UGMultPtr   = UGMult;   
   UBVRMultPtr = UBVRMult; 
   VGMultPtr   = VGMult;   
   VBMultPtr   = VBMult;   

   /* setup pointers to first image line that we are processing */
   RGBPtr1 = (RGB24Pixel *)RGBBuffer;
   YPtr1 = YBuffer;
   UPtr  = UBuffer;
   VPtr  = VBuffer;

   /* setup pointers to second image line that we are processing */
   RGBPtr2 = ((RGB24Pixel *)(RGBBuffer + SrcPitch));
   YPtr2 = YBuffer + DstPitch;

   for( HeightCtr = ImageHeight; HeightCtr != 0; HeightCtr-=2 )
   {
      for( WidthCtr = 0; WidthCtr < ImageWidth; WidthCtr += 2 )
      {
         /* process pixel (0,0) in our 2x2 block */
         YScaled = YRMultPtr[RGBPtr1[WidthCtr].Red];
         RSum = RGBPtr1[WidthCtr].Red;

         YScaled += YGMultPtr[RGBPtr1[WidthCtr].Green];
         GSum = RGBPtr1[WidthCtr].Green;

         YScaled += YBMultPtr[RGBPtr1[WidthCtr].Blue];
         BSum = RGBPtr1[WidthCtr].Blue;

         YPtr1[WidthCtr] = (unsigned char)(YScaled >> ShiftFactor); /* Y value valid write to output array */
 
         /* process pixel (1,0) in our 2x2 block */
         YScaled = YRMultPtr[RGBPtr2[WidthCtr].Red];
         RSum += RGBPtr2[WidthCtr].Red;
         
         YScaled += YGMultPtr[RGBPtr2[WidthCtr].Green];
         GSum += RGBPtr2[WidthCtr].Green;

         YScaled += YBMultPtr[RGBPtr2[WidthCtr].Blue];
         BSum += RGBPtr2[WidthCtr].Blue;

         YPtr2[WidthCtr] = (unsigned char)(YScaled >> ShiftFactor); /* Y value valid write to output array */

         /* process pixel (0,1) in our 2x2 block */
         YScaled = YRMultPtr[RGBPtr1[WidthCtr+1].Red];
         RSum += RGBPtr1[WidthCtr+1].Red;

         YScaled += YGMultPtr[RGBPtr1[WidthCtr+1].Green];
         GSum += RGBPtr1[WidthCtr+1].Green;

         YScaled += YBMultPtr[RGBPtr1[WidthCtr+1].Blue];
         BSum += RGBPtr1[WidthCtr+1].Blue;

         YPtr1[WidthCtr+1] = (unsigned char)(YScaled >> ShiftFactor); /* Y value valid write to output array */

         /* process pixel (1,1) in our 2x2 block */
         YScaled = YRMultPtr[RGBPtr2[WidthCtr+1].Red];
         RSum += RGBPtr2[WidthCtr+1].Red;

         YScaled += YGMultPtr[RGBPtr2[WidthCtr+1].Green];
         GSum += RGBPtr2[WidthCtr+1].Green;

         YScaled += YBMultPtr[RGBPtr2[WidthCtr+1].Blue];
         BSum += RGBPtr2[WidthCtr+1].Blue;

         YPtr2[WidthCtr+1] = (unsigned char)(YScaled >> ShiftFactor);       /* Y value valid write to output array */

		 // convert our sums to averages
         RSum += 2;
         GSum += 2;
         BSum += 2;

         RSum = RSum >> 2;
         GSum = GSum >> 2;
         BSum = BSum >> 2;

		 // process u for the 4 pixels 
         UScaled = URMultPtr[RSum];
         UScaled += UGMultPtr[GSum];
         UScaled += UBVRMultPtr[BSum];
         UScaled = UScaled >> ShiftFactor;
         UPtr[(WidthCtr>>1)] = (unsigned char)UScaled;

		 // process v for the 4 pixels 
         VScaled = UBVRMultPtr[RSum];
         VScaled += VGMultPtr[GSum];
         VScaled += VBMultPtr[BSum];
         VScaled = VScaled >> ShiftFactor;
         VPtr[(WidthCtr>>1)] = (unsigned char)VScaled;

      }

      /* Increment our pointers */
	  UPtr     += (DstPitch>>1);
	  VPtr     += (DstPitch>>1);
      YPtr1    += 2*DstPitch;
      YPtr2    += 2*DstPitch;
      RGBPtr1 = ((RGB24Pixel *)((unsigned char *) RGBPtr1 + SrcPitch*2));;
      RGBPtr2 = ((RGB24Pixel *)((unsigned char *) RGBPtr2 + SrcPitch*2));;
   }
}
