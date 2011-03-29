#ifndef ON2_PORTS_X86_H
#define ON2_PORTS_X86_H
//#include "config.h"

#if defined(__GNUC__) && __GNUC__
#if ARCH_X86_64
#define cpuid(func,ax,bx,cx,dx)\
    __asm__ __volatile__ (\
    "cpuid           \n\t" \
    : "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) \
    : "a"  (func));
#else
#define cpuid(func,ax,bx,cx,dx)\
    __asm__ __volatile__ (\
    "pushl %%ebx     \n\t" \
    "cpuid           \n\t" \
    "movl  %%ebx, %1 \n\t" \
    "popl  %%ebx     \n\t" \
    : "=a" (ax), "=r" (bx), "=c" (cx), "=d" (dx) \
    : "a"  (func));
#endif
#else
#if ARCH_X86_64
void __cpuid(int CPUInfo[4], int InfoType);
#pragma intrinsic(__cpuid)
#define cpuid(func,a,b,c,d) do{\
    int regs[4];\
	__cpuid(regs,func); a=regs[0];	b=regs[1];	c=regs[2];	d=regs[3];\
} while(0)
#elif 0
#define cpuid(func,a,b,c,d)\
	__asm mov eax, func;\
	__asm cpuid;\
	__asm mov a, eax;\
	__asm mov b, ebx;\
	__asm mov c, ecx;\
	__asm mov d, edx;
#endif
#endif

#define HAS_MMX   0x01
#define HAS_SSE   0x02
#define HAS_SSE2  0x04
#define HAS_SSE3  0x08
//#ifndef BIT
//#define BIT(n) (1<<n)
//#endif

#if 0
static int
x86_simd_caps(void) {
    unsigned int flags = 0;
    unsigned int eax, ebx, ecx, edx;

    /* Ensure that the CPUID instruction supports extended features */
    cpuid(0,eax,ebx,ecx,edx);
    if(eax<1)
        return 0;
        
    /* Get the standard feature flags */
    cpuid(1,eax,ebx,ecx,edx);
    if(edx & BIT(23)) flags |= HAS_MMX;
    if(edx & BIT(25)) flags |= HAS_SSE;  /* aka xmm */
    if(edx & BIT(26)) flags |= HAS_SSE2; /* aka wmt */
    if(ecx & BIT(0))  flags |= HAS_SSE3;
    
    return flags;
}
#endif


#if 0
static unsigned int
x86_readtsc(void) {
#if defined(__GNUC__) && __GNUC__
    unsigned int tsc;
    __asm__ __volatile__ ("rdtsc\n\t":"=a"(tsc):);
    return tsc;
#else  
    __asm  rdtsc;
#endif
}
#endif
#endif
