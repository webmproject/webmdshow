/* tables used by MMX blitters */
unsigned long WK_YforY_MMX[256];	
unsigned long WK_UforBG_MMX[256];
unsigned long WK_VforRG_MMX[256];

#define ROUND(x) ((x)<0?(x)-.5:(x)+.5)

void BuildYUY2toRGB_mmx(void)
/* table build for MMX 16bit blitters */
{
    int i,j;
    unsigned long x,z;

	for (i = 0; i < 256; i++) {
        z =(unsigned short) ROUND( (i - 16.0) * 1.164 ) ;   
        WK_YforY_MMX[i] = z | z<<16;
    }

	for (i = 0, j = 0; i < 256; i+=1, j++) {
        /* CbforB */
		z = (unsigned short) ROUND( (j - 128.0) * 2.017);

        /* CbforG */
		x = (unsigned short) ROUND( (j - 128.0) * -0.392);
		WK_UforBG_MMX[i] = z | x<<16;

        /* CrforG */
		z = (unsigned short) ROUND( (j - 128.0) * -0.813);

        /* CrforR */
		x = (unsigned short) ROUND( (j - 128.0) * 1.596);
		WK_VforRG_MMX[i] = z | x<<16;
    }
}
