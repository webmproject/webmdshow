//#include "on2_ports/config.h"
#include "on2_blit/on2_blit_internal.h"
#include "on2_ports/x86.h"
#include <windows.h>

extern void
CC_RGB24toYV12_MMX_INLINE(unsigned char *src_buf, int w, int h,
                          unsigned char *y, unsigned char *u, unsigned char *v,
                          int src_pitch, int dst_pitch);
extern void
CC_RGB32toYV12_MMX_INLINE(unsigned char *src_buf, int w, int h,
                          unsigned char *y, unsigned char *u, unsigned char *v,
                          int src_pitch, int dst_pitch);
extern void
CC_RGB24toYV12_XMM_INLINE(unsigned char *src_buf, int w, int h,
                          unsigned char *y, unsigned char *u, unsigned char *v,
                          int src_pitch, int dst_pitch);
extern void
CC_RGB32toYV12_XMM_INLINE(unsigned char *src_buf, int w, int h,
                          unsigned char *y, unsigned char *u, unsigned char *v,
                          int src_pitch, int dst_pitch);
//extern void
//CC_UYVYtoYV12_MMX(unsigned char *src_buf, int w, int h,
//                  unsigned char *y, unsigned char *u, unsigned char *v,
//                  int src_pitch, int dst_pitch);
//extern void
//CC_YUY2toYV12_MMX(unsigned char *src_buf, int w, int h,
//                  unsigned char *y, unsigned char *u, unsigned char *v,
//                  int src_pitch, int dst_pitch);
//extern void
//CC_YVYUtoYV12_MMX(unsigned char *src_buf, int w, int h,
//                  unsigned char *y, unsigned char *u, unsigned char *v,
//                  int src_pitch, int dst_pitch);

extern void
CC_RGB24toYV12_C(unsigned char *RGBBuffer, int ImageWidth, int ImageHeight,
                 unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch );

extern void
CC_RGB32toYV12_C(unsigned char *RGBABuffer, int ImageWidth, int ImageHeight,
                 unsigned char *YBuffer, unsigned char *UBuffer, unsigned char *VBuffer, int SrcPitch,int DstPitch );


#if 0
extern void
bcs00_555_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern void
bct00_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern void
bcy00_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern void
bcf00_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern void
bcs00_565_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern void
bcc00_MMX(unsigned char *dst, int thisPitch, YUV_BUFFER_CONFIG *src);

extern void
bcu00_MMX(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);
#endif

#if 0
static int
packed_to_yv12(const on2_image_t *dst, const on2_image_t *src, int *info) {
    void (*func)(unsigned char *src_buf, int w, int h,
                 unsigned char *y, unsigned char *u, unsigned char *v,
                 int src_pitch, int dst_pitch) = NULL;
    int caps = x86_simd_caps();

    (void)info;
    SUPPORTED((dst->fmt == IMG_FMT_YV12 || dst->fmt == IMG_FMT_I420)
              && planar_img_is_aligned(dst,
                                       1, 1,
                                       1, 2, 1, 1)
              && (dst->stride[PLANE_U] == dst->stride[PLANE_Y] / 2)
              && (dst->stride[PLANE_V] == dst->stride[PLANE_Y] / 2));

    switch(src->fmt) {
    case IMG_FMT_BGR24:
    case IMG_FMT_RGB32_LE:
        /* These functions use multibyte accesses of the RGB data, which
         * may have alignment restrictions on some platforms.
         */
        SUPPORTED(packed_img_is_aligned(src, 2, 2,
                                        MAYBE_ALIGNED(4), MAYBE_ALIGNED(4)));
        break;

    case IMG_FMT_UYVY:
    case IMG_FMT_YUY2:
    case IMG_FMT_YVYU:
        SUPPORTED(packed_img_is_aligned(src, 2, 2, 1, 1));
        break;

    default:
        return ON2_BLIT_ERR_UNSUPPORTED;
    }

#if HAVE_MMX
    if(caps & HAS_MMX) {
        switch(src->fmt) {
        case IMG_FMT_BGR24:    func = CC_RGB24toYV12_MMX_INLINE; break;
        case IMG_FMT_RGB32_LE: func = CC_RGB32toYV12_MMX_INLINE; break;
        case IMG_FMT_UYVY:     func = CC_UYVYtoYV12_MMX; break;
        case IMG_FMT_YUY2:     func = CC_YUY2toYV12_MMX; break;
        case IMG_FMT_YVYU:     func = CC_YVYUtoYV12_MMX; break;
        }
    }
#endif
#if HAVE_SSE
    if(caps & HAS_SSE) {
        switch(src->fmt) {
        case IMG_FMT_BGR24:    func = CC_RGB24toYV12_XMM_INLINE; break;
        case IMG_FMT_RGB32_LE: func = CC_RGB32toYV12_XMM_INLINE; break;
        }
    }
#endif

    if(!func)
        return ON2_BLIT_ERR_UNSUPPORTED;

    *info |= ON2_BLIT_INFO_OPTIMIZED;
    func(src->planes[PLANE_PACKED],
         src->d_w, src->d_h,
         dst->planes[PLANE_Y],
         dst->planes[PLANE_U],
         dst->planes[PLANE_V],
         src->stride[PLANE_PACKED],
         dst->stride[PLANE_Y]);
    return 0;
}
#endif

#if 0
static void
init_rgb(void) {
    static int init_done=0;
    extern void BuildYUY2toRGB_mmx(void);

    if(!init_done) {
        BuildYUY2toRGB_mmx();
        init_done = 1;
    }
}


static int
yv12_to_packed(const on2_image_t *dst, const on2_image_t *src, int *info) {
    void (*func)(unsigned char *dst, int scrnPitch,
                 YUV_BUFFER_CONFIG *buffConfig) = NULL;
    YUV_BUFFER_CONFIG buf_cfg;
    int caps = x86_simd_caps();

    (void)info;
    SUPPORTED((src->fmt == IMG_FMT_YV12 || src->fmt == IMG_FMT_I420)
              && planar_img_is_aligned(src,
                                       1, 1,
                                       1, 2, 1, 1)
              && (src->stride[PLANE_U] == src->stride[PLANE_Y] / 2)
              && (src->stride[PLANE_V] == src->stride[PLANE_Y] / 2));

    switch(dst->fmt) {
    case IMG_FMT_UYVY:
    case IMG_FMT_YUY2:
        SUPPORTED(packed_img_is_aligned(dst, 4, 1,
                                        MAYBE_ALIGNED(4), MAYBE_ALIGNED(4)));
        break;
    case IMG_FMT_RGB555_LE:
    case IMG_FMT_RGB565_LE:
        SUPPORTED(planar_img_is_aligned(src,
                                        4, 1,
                                        MAYBE_ALIGNED(4), MAYBE_ALIGNED(4),
                                        MAYBE_ALIGNED(2), MAYBE_ALIGNED(2))
                  && packed_img_is_aligned(dst, 2, 2, 1, 1));
        break;
    case IMG_FMT_BGR24:
    case IMG_FMT_RGB32_LE:
        SUPPORTED(packed_img_is_aligned(dst, 2, 2,
                                        MAYBE_ALIGNED(4), MAYBE_ALIGNED(4)));
        break;
    default:
        return ON2_BLIT_ERR_UNSUPPORTED;
    }

#if HAVE_MMX
    if(caps & HAS_MMX) {
        switch(dst->fmt) {
        case IMG_FMT_UYVY:      func = bcu00_MMX; break;
        case IMG_FMT_YUY2:      func = bcy00_MMX; break;
        case IMG_FMT_RGB555_LE: init_rgb(); func = bcs00_555_MMX; break;
        case IMG_FMT_RGB565_LE: init_rgb(); func = bcs00_565_MMX; break;
        case IMG_FMT_BGR24:     init_rgb(); func = bcf00_MMX; break;
        case IMG_FMT_RGB32_LE:  init_rgb(); func = bct00_MMX; break;
        }
    }
#endif

    if(!func)
        return ON2_BLIT_ERR_UNSUPPORTED;

    *info |= ON2_BLIT_INFO_OPTIMIZED;
    on2_image_to_yuv_buffer_config(&buf_cfg, src);
    func(dst->planes[PLANE_PACKED],
         dst->stride[PLANE_PACKED],
         &buf_cfg);
    return 0;
}


int
on2_blit_x86(on2_image_t *dst, const on2_image_t *src, int *info) {
    int res = 0;
    do {
        if(!packed_to_yv12(dst, src, info)) break;
        if(!yv12_to_packed(dst, src, info)) break;
        res = ON2_BLIT_ERR_UNSUPPORTED;
    } while(0);
    return res;
}
#endif


//#define cpuid(func,a,b,c,d)\
//	__asm mov eax, func;\
//	__asm cpuid;\
//	__asm mov a, eax;\
//	__asm mov b, ebx;\
//	__asm mov c, ecx;\
//	__asm mov [d], edx;
//#endif
//#endif
//
//#define HAS_MMX   0x01
//#define HAS_SSE   0x02
//#define HAS_SSE2  0x04
//#define HAS_SSE3  0x08
//#ifndef BIT
//#define BIT(n) (1<<n)
//#endif
//

static DWORD
x86_simd_caps(void) {
    DWORD reg_eax, /* reg_ebx, */ reg_ecx, reg_edx;
    DWORD flags;

	__asm mov eax, 0;
	__asm cpuid;
	__asm mov [reg_eax], eax;

	if(reg_eax == 0)
	    return 0;

	__asm mov eax, 1;
	__asm cpuid;
	__asm mov [reg_eax], eax;
	//__asm mov [reg_ebx], ebx;
	__asm mov [reg_ecx], ecx;
	__asm mov [reg_edx], edx;

	flags = 0;
	if(reg_edx & (1 << 23)) flags |= HAS_MMX;
	if(reg_edx & (1 << 25)) flags |= HAS_SSE;
	if(reg_edx & (1 << 26)) flags |= HAS_SSE2;
	if(reg_ecx & (1 << 0)) flags |= HAS_SSE3;

	return flags;
}


on2_rgb_to_yuv_t on2_get_rgb_to_yuv(img_fmt_t dst, img_fmt_t src) {
    const DWORD caps = x86_simd_caps();

    if (dst != IMG_FMT_YV12)  //TODO: liberalize?
        return NULL;

    switch(src) {
    case IMG_FMT_RGB24:
#if HAVE_SSE
        if(caps & HAS_SSE)
            return CC_RGB24toYV12_XMM_INLINE;
#endif
#if HAVE_MMX
        if(caps & HAS_MMX)
            return CC_RGB24toYV12_MMX_INLINE;
#endif
        return CC_RGB24toYV12_C;

    case IMG_FMT_RGB32:
#if HAVE_SSE
        if(caps & HAS_SSE)
            return CC_RGB32toYV12_XMM_INLINE;
#endif
#if HAVE_MMX
        if(caps & HAS_MMX)
            return CC_RGB32toYV12_MMX_INLINE;
#endif
        return CC_RGB32toYV12_C;

    default:
        return NULL;
    }
}
