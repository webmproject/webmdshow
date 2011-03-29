#ifndef ON2_BLIT_INTERNAL_H
#define ON2_BLIT_INTERNAL_H
//#include "on2_ports/config.h"
#include "on2_ports/on2_integer.h"
#include "on2_blit.h"

#define IS_ALIGNED(x, align) (!((x)&(align-1)))
#define MAYBE_ALIGNED(n) (CONFIG_FAST_UNALIGNED ? 1 : (n))
#define SUPPORTED(x) do {if(!(x)) return ON2_BLIT_ERR_UNSUPPORTED;}while(0)

typedef struct
{
    int     YWidth;
    int     YHeight;
    int     YStride;

    int     UVWidth;
    int     UVHeight;
    int     UVStride;

    unsigned char *  YBuffer;
    unsigned char *  UBuffer;
    unsigned char *  VBuffer;

} YUV_BUFFER_CONFIG;


static int
planar_img_is_aligned(const on2_image_t *img,
                      unsigned int w_align, 
                      unsigned int h_align,
                      int y_ptr_align, 
                      int y_stride_align, 
                      int uv_ptr_align, 
                      int uv_stride_align) {
    return IS_ALIGNED(img->d_w, w_align)
           && IS_ALIGNED(img->d_h, h_align)
           && IS_ALIGNED((uintptr_t)img->planes[PLANE_Y], y_ptr_align)
           && IS_ALIGNED((uintptr_t)img->planes[PLANE_U], uv_ptr_align)
           && IS_ALIGNED((uintptr_t)img->planes[PLANE_V], uv_ptr_align)
           && IS_ALIGNED(img->stride[PLANE_Y], y_stride_align)
           && IS_ALIGNED(img->stride[PLANE_U], uv_stride_align)
           && IS_ALIGNED(img->stride[PLANE_V], uv_stride_align)
           && (img->fmt & IMG_FMT_HAS_ALPHA
               ? IS_ALIGNED((uintptr_t)img->planes[PLANE_ALPHA], y_ptr_align)
                 && IS_ALIGNED(img->stride[PLANE_ALPHA], y_stride_align)
               : 1);
}


static int
packed_img_is_aligned(const on2_image_t *img,
                      unsigned int w_align, 
                      unsigned int h_align,
                      int ptr_align, 
                      int stride_align) {
    return IS_ALIGNED(img->d_w, w_align)
           && IS_ALIGNED(img->d_h, h_align)
           && IS_ALIGNED((uintptr_t)img->planes[PLANE_PACKED], ptr_align)
           && IS_ALIGNED(img->stride[PLANE_PACKED], stride_align);
}


static void
on2_image_to_yuv_buffer_config(YUV_BUFFER_CONFIG *cfg, const on2_image_t *img) {
    /* YUV_BUFFER_CONFIG blitters expect the Y/U/V pointers to point to the
     * last row of the image, and walk through them by subtracting the 
     * stride.
     */
    cfg->YWidth = img->d_w;
    cfg->YHeight = img->d_h;
    cfg->YStride = img->stride[PLANE_Y];
    cfg->UVWidth = img->d_w >> img->x_chroma_shift;
    cfg->UVHeight = img->d_h >> img->y_chroma_shift;
    cfg->UVStride = img->stride[PLANE_U];
    cfg->YBuffer = img->planes[PLANE_Y]+(img->d_h-1)*img->stride[PLANE_Y];
    cfg->UBuffer = img->planes[PLANE_U]+(cfg->UVHeight-1)*img->stride[PLANE_U];
    cfg->VBuffer = img->planes[PLANE_V]+(cfg->UVHeight-1)*img->stride[PLANE_V];
}

#endif
