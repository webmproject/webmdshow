#include "on2_blit.h"

#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_line_yv12_to_rgb565_c)
void
on2_blit_line_yv12_to_rgb565_c(uint8_t *dst8,
                               on2_image_t *src_img,
			       unsigned int y,
                               unsigned int x,
			       unsigned int w) {
    uint8_t *src_y,*src_u,*src_v;
    uint16_t *dst = (uint16_t *)dst8;
    long r,g,b;

    w = x+w <= src_img->d_w? w : src_img->d_w-x;
    src_y = src_img->planes[PLANE_Y] + y * src_img->stride[PLANE_Y];
    src_u = src_img->planes[PLANE_U]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_U];
    src_v = src_img->planes[PLANE_V]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_V];
    for(x=0; x<w; x++) {
	r = 76283*(src_y[x]-16)
	    + 104595*(src_v[x>>1]-128);
	g = 76283*(src_y[x]-16)
	    - 53280*(src_v[x>>1]-128)
	    - 25690*(src_u[x>>1]-128);
	b = 76283*(src_y[x]-16)
	    + 132186*(src_u[x>>1]-128);
	r = (r>>16 > 255)?255:(r>>16<0)?0:r>>16;
	g = (g>>16 > 255)?255:(g>>16<0)?0:g>>16;
	b = (b>>16 > 255)?255:(b>>16<0)?0:b>>16;
	r >>= 3;
	g >>= 2;
	b >>= 3;

	dst[x] = (r<<11) + (g<<5) +b;
    }
}
#endif
    
#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_yv12_to_rgb565_c)
void
on2_blit_yv12_to_rgb565_c(on2_image_t *dst_img, on2_image_t *src_img) {
    unsigned int y,w,h;
    uint8_t *dst;
    w = src_img->d_w < dst_img->d_w ? src_img->d_w : dst_img->d_w;
    h = src_img->d_h < dst_img->d_h ? src_img->d_h : dst_img->d_h;
    dst = dst_img->planes[PLANE_PACKED]; 
    for(y=0; y<h; y++) {
        on2_blit_line_yv12_to_rgb565(dst, src_img, y, 0, w);
	dst += dst_img->stride[PLANE_PACKED]>>1;
    }
}
#endif

#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_line_yv12_to_rgb555_c)
void
on2_blit_line_yv12_to_rgb555_c(uint8_t *dst8,
                               on2_image_t *src_img,
			       unsigned int y,
                               unsigned int x,
			       unsigned int w) {
    uint8_t *src_y,*src_u,*src_v;
    uint16_t *dst = (uint16_t *)dst8;
    long r,g,b;

    w = x+w <= src_img->d_w? w : src_img->d_w-x;
    src_y = src_img->planes[PLANE_Y] + y * src_img->stride[PLANE_Y];
    src_u = src_img->planes[PLANE_U]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_U];
    src_v = src_img->planes[PLANE_V]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_V];
    for(x=0; x<w; x++) {
	r = 76283*(src_y[x]-16)
	    + 104595*(src_v[x>>1]-128);
	g = 76283*(src_y[x]-16)
	    - 53280*(src_v[x>>1]-128)
	    - 25690*(src_u[x>>1]-128);
	b = 76283*(src_y[x]-16)
	    + 132186*(src_u[x>>1]-128);
	r = (r>>16 > 255)?255:(r>>16<0)?0:r>>16;
	g = (g>>16 > 255)?255:(g>>16<0)?0:g>>16;
	b = (b>>16 > 255)?255:(b>>16<0)?0:b>>16;
	r >>= 3;
	g >>= 3;
	b >>= 3;

	dst[x] = (1<<15) + (r<<11) + (g<<5) +b;
    }
}
#endif
    
#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_yv12_to_rgb555_c)
void
on2_blit_yv12_to_rgb555_c(on2_image_t *dst_img, on2_image_t *src_img) {
    unsigned int y,w,h;
    uint8_t *dst;
    w = src_img->d_w < dst_img->d_w ? src_img->d_w : dst_img->d_w;
    h = src_img->d_h < dst_img->d_h ? src_img->d_h : dst_img->d_h;
    dst = dst_img->planes[PLANE_PACKED]; 
    for(y=0; y<h; y++) {
        on2_blit_line_yv12_to_rgb555(dst, src_img, y, 0, w);
	dst += dst_img->stride[PLANE_PACKED]>>1;
    }
}
#endif

#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_line_yv12_to_rgb24_c)
void
on2_blit_line_yv12_to_rgb24_c(uint8_t *dst8,
                               on2_image_t *src_img,
			       unsigned int y,
                               unsigned int x,
			       unsigned int w) {
    uint8_t *src_y,*src_u,*src_v;
    uint16_t *dst = (uint16_t *)dst8;
    long r,g,b;

    w = x+w <= src_img->d_w? w : src_img->d_w-x;
    src_y = src_img->planes[PLANE_Y] + y * src_img->stride[PLANE_Y];
    src_u = src_img->planes[PLANE_U]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_U];
    src_v = src_img->planes[PLANE_V]
            + (y >> src_img->y_chroma_shift) * src_img->stride[PLANE_V];
    for(x=0; x<w; x++) {
	r = 76283*(src_y[x]-16)
	    + 104595*(src_v[x>>1]-128);
	g = 76283*(src_y[x]-16)
	    - 53280*(src_v[x>>1]-128)
	    - 25690*(src_u[x>>1]-128);
	b = 76283*(src_y[x]-16)
	    + 132186*(src_u[x>>1]-128);
	r = (r>>16 > 255)?255:(r>>16<0)?0:r>>16;
	g = (g>>16 > 255)?255:(g>>16<0)?0:g>>16;
	b = (b>>16 > 255)?255:(b>>16<0)?0:b>>16;

	dst[x] = (r<<16) + (g<<8) +b;
    }
}
#endif
    
#if CONFIG_RUNTIME_CPU_DETECT || defined(enable_on2_blit_yv12_to_rgb24_c)
void
on2_blit_yv12_to_rgb24_c(on2_image_t *dst_img, on2_image_t *src_img) {
    unsigned int y,w,h;
    uint8_t *dst;
    w = src_img->d_w < dst_img->d_w ? src_img->d_w : dst_img->d_w;
    h = src_img->d_h < dst_img->d_h ? src_img->d_h : dst_img->d_h;
    dst = dst_img->planes[PLANE_PACKED]; 
    for(y=0; y<h; y++) {
        on2_blit_line_yv12_to_rgb24(dst, src_img, y, 0, w);
	dst += dst_img->stride[PLANE_PACKED]>>1;
    }
}
#endif
