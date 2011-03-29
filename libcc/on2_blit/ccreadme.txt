The color conversion library currently supports the following conversions

RGB32 to YV12 format
RGB24 to YV12 format
YVYU to YV12 format

For each type of color space conversion there exists three functions, a generic
C version, a version optimized for Pentium II processors and a version
optimized for Pentium III processors.

See C versions of each function for format of parameters etc.  The C versions
of the functions can be found in colorconversions.c

The number of K cycles it takes each function to process a 320x240 image is
given.  The number given is the average of 100 runs of the function.  The
measurements were taken on a Pentium III 450 system with 128 Megs of memory running
windows NT 4.0 with SP5.  The release version of the project was run.

Function                File                    Kcycles     Comments
------------------------------------------------------------------------------------
CC_RGB32toYV12          colorconversions.c      1945        C version of
                                                            function
CC_RGB32toYV12_MMX      CC_RGB32toYV12_MMX.asm  1346        Pentium II
                                                            function version
CC_RGB32toYV12_XMM      CC_RGB32toYV12_XMM.asm  1208        Pentium III
                                                            function version
CC_RGB24toYV12          colorconversions.c      xxxx        C version of
                                                            function
CC_RGB24toYV12_MMX      CC_RGB24toYV12_MMX.asm  1731        Pentium II version
                                                            of function
CC_RGB24toYV12_XMM      CC_RGB24toYV12_XMM.asm  1592        Pentium III version
                                                            of function
CC_YVYUtoYV12           colorconversions.c      501         C version of
                                                            function
CC_YVYUtoYV12_MMX       CC_YVYUtoYV12_MMX.asm   266         Pentium II version
                                                            of function
CC_YVYUtoYV12_XMM       CC_YVYUtoYV12_XMM.asm   266         Pentium III version
                                                            of function






