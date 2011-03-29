
/*==============================================================================
Copyright © 2005 On2 Technology, Inc. All rights reserved.

FILE NAME:  
    vpxblit.h

PURPOSE:
    header file for vpxblit

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
 7    on2_ref_pkg 1.6         2009-04-08 15:18:17Z James Berry     
$

    
==============================================================================*/
#ifndef VPXBLIT_H_INCL
#define VPXBLIT_H_INCL
/*==============================================================================
                              Includes
==============================================================================*/
#include "on2_ports/config.h"

/*==============================================================================
                              Defines
==============================================================================*/


#if CONFIG_BIG_ENDIAN
#define BYTE_ZERO(X) 	((X & 0xFF000000) >> (24 - 2)	) 
#define BYTE_ONE(X)  	((X & 0x00FF0000) >> (16 - 2)	)
#define BYTE_TWO(X)  	((X & 0x0000FF00) >> (8 - 2)	) 
#define BYTE_THREE(X) 	((X & 0x000000FF) << (0 + 2)    )

#define BYTE_ZERO_UV(X) ((X & 0x0000FF00) >> (8 - 2)	) 
#define BYTE_ONE_UV(X) 	((X & 0x000000FF) << (0 + 2)    )

#define REREFERENCE(X) (*((int *) &(X)))   

#else

#define BYTE_THREE(X) 	((X & 0xFF000000) >> (24 - 2)	) 
#define BYTE_TWO(X)  	((X & 0x00FF0000) >> (16 - 2)	)
#define BYTE_ONE(X)  	((X & 0x0000FF00) >> (8 - 2)	) 
#define BYTE_ZERO(X) 	((X & 0x000000FF) << (0 + 2)    )

#define BYTE_ONE_UV(X) ((X & 0x0000FF00) >> (8 - 2)	) 
#define BYTE_ZERO_UV(X) 	((X & 0x000000FF) << (0 + 2)    )

#define REREFERENCE(X) (*((int *) &(X)))   
    	
#endif


/*==============================================================================
                            Type Definitions
==============================================================================*/
typedef struct  // YUV buffer configuration structure
{
    int   YWidth;
    int   YHeight;
    int   YStride;

    int   UVWidth;
    int   UVHeight;
    int   UVStride;

    char *YBuffer;
    char *UBuffer;
    char *VBuffer;

	char *uvStart;
    int   uvDstArea;
    int   uvUsedArea;

} VPX_BLIT_CONFIG;

typedef struct tx86_Params
{
	unsigned int pushedRegisters[6];
	unsigned int returnAddress;
	unsigned int dst;
	unsigned int scrnPitch;
	VPX_BLIT_CONFIG * buffConfig;
}X86_Params;

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
int VPX_GetSizeOfPixel(unsigned int bd);
void * VPX_GetBlitter(unsigned int bd);
void VPX_SetBlit(void);
void VPX_DestroyBlit(void);


	
#endif //VPXBLIT_H_INCL

