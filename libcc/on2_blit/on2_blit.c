#include "on2_blit_internal.h"

extern void
CC_RGB24toYV12_C(unsigned char *src_buf, int w, int h, 
                 unsigned char *y, unsigned char *u, unsigned char *v,
                 int src_pitch, int dst_pitch);
extern void
CC_RGB32toYV12_C(unsigned char *src_buf, int w, int h, 
                 unsigned char *y, unsigned char *u, unsigned char *v,
                 int src_pitch, int dst_pitch);
extern void
CC_UYVYtoYV12_C(unsigned char *src_buf, int w, int h, 
                unsigned char *y, unsigned char *u, unsigned char *v,
                int src_pitch, int dst_pitch);
extern void
CC_YUY2toYV12_C(unsigned char *src_buf, int w, int h, 
                unsigned char *y, unsigned char *u, unsigned char *v,
                int src_pitch, int dst_pitch);
extern void
CC_YVYUtoYV12_C(unsigned char *src_buf, int w, int h, 
                unsigned char *y, unsigned char *u, unsigned char *v,
                int src_pitch, int dst_pitch);

extern void
bcs00_555_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig); 

extern void
bct00_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig); 

extern void
bcy00_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig); 

extern void
bcf00_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig); 

extern void
bcs00_565_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig); 

extern void
bcc00_c(unsigned char *dst, int thisPitch, YUV_BUFFER_CONFIG *src);

extern void
bcu00_c(unsigned char *dst, int scrnPitch, YUV_BUFFER_CONFIG *buffConfig);

extern int
on2_blit_x86(on2_image_t *dst, const on2_image_t *src_, int *info);

static int
packed_to_yv12(const on2_image_t *dst, const on2_image_t *src, int *info) {
    void (*func)(unsigned char *src_buf, int w, int h, 
                 unsigned char *y, unsigned char *u, unsigned char *v,
                 int src_pitch, int dst_pitch);

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
    
    switch(src->fmt) {
    case IMG_FMT_BGR24:    func = CC_RGB24toYV12_C; break;
    case IMG_FMT_RGB32_LE: func = CC_RGB32toYV12_C; break;
    case IMG_FMT_UYVY:     func = CC_UYVYtoYV12_C; break;
    case IMG_FMT_YUY2:     func = CC_YUY2toYV12_C; break;
    case IMG_FMT_YVYU:     func = CC_YVYUtoYV12_C; break;
    }
    func(src->planes[PLANE_PACKED],
         src->d_w, src->d_h,
         dst->planes[PLANE_Y],
         dst->planes[PLANE_U],
         dst->planes[PLANE_V], 
         src->stride[PLANE_PACKED],
         dst->stride[PLANE_Y]);
    return 0;
}

static void
init_rgb(void) {
    static int init_done=0;
    
    if(!init_done) {
        BuildYUY2toRGB();
        init_done = 1;
    }
}

static int
yv12_to_packed(const on2_image_t *dst, const on2_image_t *src, int *info) {
    void (*func)(unsigned char *dst, int scrnPitch,
                 YUV_BUFFER_CONFIG *buffConfig);
    YUV_BUFFER_CONFIG buf_cfg;

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
    
    switch(dst->fmt) {
    case IMG_FMT_UYVY:      func = bcu00_c; break;
    case IMG_FMT_YUY2:      func = bcy00_c; break;
    case IMG_FMT_RGB555_LE: init_rgb(); func = bcs00_555_c; break;
    case IMG_FMT_RGB565_LE: init_rgb(); func = bcs00_565_c; break;
    case IMG_FMT_BGR24:     init_rgb(); func = bcf00_c; break;
    case IMG_FMT_RGB32_LE:  init_rgb(); func = bct00_c; break;
    }
    
    on2_image_to_yuv_buffer_config(&buf_cfg, src);
    func(dst->planes[PLANE_PACKED],
         dst->stride[PLANE_PACKED],
         &buf_cfg);
    return 0;
}


int
on2_blit_ex(on2_image_t       *dst,
            const on2_image_t *src_,
            int                abi,
            int               *info,
            int                update_dst) {
    on2_image_t new_src;
    const on2_image_t *src = src_;
    int new_info = 0;
    int res = 0;
    
    if(abi != ON2_BLIT_ABI_VERSION) {
        res = ON2_BLIT_ERR_ABI_MISMATCH;
        goto bail;
    }

    if(!dst || !src) {
        res = ON2_BLIT_ERR_INVALID_PARAM;
        goto bail;
    }
 
    if((dst->d_w < src->d_w) || (dst->d_h < src->d_h)) {
        new_src = *src;
        src = &new_src;
        if(dst->d_w < src->d_w)
            new_src.d_w = dst->d_w;
        if(dst->d_h < src->d_h)
            new_src.d_h = dst->d_h;
    }
    
    if(!res) {
        do {
#if ARCH_X86
            if(!on2_blit_x86(dst, src, &new_info)) break;
#endif
#if HAVE_ALTIVEC
            if(!on2_blit_ppc(dst, src, &new_info)) break;
#endif
            if(!packed_to_yv12(dst, src, &new_info)) break;
            if(!yv12_to_packed(dst, src, &new_info)) break;
            res = ON2_BLIT_ERR_UNSUPPORTED;
        } while(0);
    }    
    
bail:
    new_info |= ON2_BLIT_INFO_VALID;
    if(info)
        *info = (!res)? new_info : 0;
    if(!res && update_dst) {
        dst->d_w = src->d_w;
        dst->d_h = src->d_h;
    }

    return res;
}
