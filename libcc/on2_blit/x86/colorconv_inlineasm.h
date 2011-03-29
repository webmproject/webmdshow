

/*==============================================================================
Copyright  2005 On2 Technology, Inc. All rights reserved.

FILE NAME:  
    colorcov_inlineasm.h

PURPOSE:
    

FUNCTION(S):
    <Afunction> - <Adescription>
    <Bfunction> - <Bdescription>
   
    local:
        <AlocalFunction> - <AlocalDescription>
        <BlocalFunction> - <BlocalDescription>
   
NOTES:

    
    <note description>

CHANGE HISTORY:
$History:
 6    on2_ref_pkg 1.5         2009-04-08 15:19:48Z James Berry     
$

    
==============================================================================*/
#ifndef COLORCONV_INLINEASM_H
#define COLORCONV_INLINEASM_H

/*==============================================================================
                              Includes
==============================================================================*/
//#include <stdio.h>
//#include <stdlib.h>
/*==============================================================================
                              Defines
==============================================================================*/


/*==============================================================================
                            Type Definitions
==============================================================================*/

typedef struct _CConvParams 
{
	unsigned int pushedRegisters[6];
	unsigned int returnAddress;
    unsigned int RGBABuffer;
    unsigned int ImageWidth;
    unsigned int ImageHeight;
    unsigned int YBuffer;
    unsigned int UBuffer;
    unsigned int VBuffer;
    unsigned int SrcPitch;
    unsigned int YPitch;
} CConvParams;

typedef struct _YVYUConvParams
{
	unsigned int pushedRegisters[6];
	unsigned int returnAddress;
    unsigned int YVYUBuffer;
    unsigned int ImageWidth;
    unsigned int ImageHeight;
    unsigned int YBuffer;
    unsigned int UBuffer;
    unsigned int VBuffer;
    unsigned int SrcPitch;
    unsigned int YPitch;
} YVYUConvParams;


/*=============================================================================
                                Enums
==============================================================================*/


/*==============================================================================
                              Structures
==============================================================================*/

/*==============================================================================
                             Constants
==============================================================================*/
 

/*==============================================================================
                               Variables
==============================================================================*/

/*==============================================================================
                            Function Protoypes/MICROS
==============================================================================*/


void CC_RGB24toYV12_MMX_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer,
					 int yp,int p);
void CC_RGB24toYV12_XMM_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer ,
					 int yp,int p);
void CC_RGB32toYV12_MMX_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer ,
					 int yp,int p);
void CC_RGB32toYV12_MMXLU_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer ,
					 int yp,int p);
void CC_RGB32toYV12_XMM_INLINE( unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                     unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer ,
					 int yp,int p);











#endif 