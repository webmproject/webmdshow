/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\file on2_decoder.c
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <stdlib.h>
#include <string.h>
#include "on2_codec/on2_decoder.h"
#include "on2_codec/internal/on2_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

const char *on2_dec_iface_name(on2_dec_iface_t *iface) {
    return on2_codec_iface_name((on2_codec_iface_t*)iface);
}

const char *on2_dec_err_to_string(on2_dec_err_t  err) {
    return on2_codec_err_to_string(err);
}

const char *on2_dec_error(on2_dec_ctx_t  *ctx) {
    return on2_codec_error((on2_codec_ctx_t*)ctx);
}
    
const char *on2_dec_error_detail(on2_dec_ctx_t  *ctx) {
    return on2_codec_error_detail((on2_codec_ctx_t*)ctx);
}


on2_dec_err_t on2_dec_init_ver(on2_dec_ctx_t    *ctx,
                	       on2_dec_iface_t  *iface,
			       int               ver) {
    return on2_codec_dec_init_ver((on2_codec_ctx_t*)ctx,
                                  (on2_codec_iface_t*)iface,
                                  NULL,
                                  0,
                                  ver);
}			       


on2_dec_err_t on2_dec_destroy(on2_dec_ctx_t *ctx) {
    return on2_codec_destroy((on2_codec_ctx_t*)ctx);
}


on2_dec_caps_t on2_dec_get_caps(on2_dec_iface_t *iface) {
    return on2_codec_get_caps((on2_codec_iface_t*)iface);
}


on2_dec_err_t on2_dec_peek_stream_info(on2_dec_iface_t       *iface,
                        	       const uint8_t         *data,	  
                        	       unsigned int           data_sz,
                        	       on2_dec_stream_info_t *si) {
    return on2_codec_peek_stream_info((on2_codec_iface_t*)iface, data, data_sz,
                                      (on2_codec_stream_info_t*)si);
}


on2_dec_err_t on2_dec_get_stream_info(on2_dec_ctx_t         *ctx,
                                      on2_dec_stream_info_t *si) {
    return on2_codec_get_stream_info((on2_codec_ctx_t*)ctx,
                                     (on2_codec_stream_info_t*)si);
}				      


on2_dec_err_t on2_dec_control(on2_dec_ctx_t  *ctx,
                	      int             ctrl_id,     
                	      void           *data) {
    return on2_codec_control_((on2_codec_ctx_t*)ctx, ctrl_id, data);
}


on2_dec_err_t on2_dec_decode(on2_dec_ctx_t	*ctx,
                	     uint8_t		*data,     
                	     unsigned int	 data_sz,
                	     void		*user_priv,
			     int		 rel_pts) {
    (void)rel_pts;
    return on2_codec_decode((on2_codec_ctx_t*)ctx, data, data_sz, user_priv, 
                            0);
}

on2_image_t *on2_dec_get_frame(on2_dec_ctx_t  *ctx,
                               on2_dec_iter_t *iter) {
    return on2_codec_get_frame((on2_codec_ctx_t*)ctx, iter);
}
 

on2_dec_err_t on2_dec_register_put_frame_cb(on2_dec_ctx_t             *ctx,
                                	    on2_dec_put_frame_cb_fn_t  cb,
					    void                      *user_priv) {
    return on2_codec_register_put_frame_cb((on2_codec_ctx_t*)ctx, cb,
                                           user_priv);
}


on2_dec_err_t on2_dec_register_put_slice_cb(on2_dec_ctx_t             *ctx,
                                	    on2_dec_put_slice_cb_fn_t  cb,
					    void                      *user_priv){
    return on2_codec_register_put_slice_cb((on2_codec_ctx_t*)ctx, cb,
                                           user_priv);
}


on2_dec_err_t on2_dec_xma_init_ver(on2_dec_ctx_t    *ctx,
                	           on2_dec_iface_t  *iface,
			           int               ver) {    
    return on2_codec_dec_init_ver((on2_codec_ctx_t*)ctx,
                                  (on2_codec_iface_t*)iface,
                                  NULL,
                                  ON2_CODEC_USE_XMA,
                                  ver);
}			       

on2_dec_err_t on2_dec_get_mem_map(on2_dec_ctx_t                *ctx_,
                                  on2_dec_mmap_t               *mmap,
				  const on2_dec_stream_info_t  *si,
                                  on2_dec_iter_t               *iter) {
    on2_codec_ctx_t   *ctx = (on2_codec_ctx_t *)ctx_;    
    on2_dec_err_t      res = ON2_DEC_OK;
    
    if(!ctx || !mmap || !si || !iter || !ctx->iface)
        res = ON2_DEC_INVALID_PARAM;
    else if(!(ctx->iface->caps & ON2_DEC_CAP_XMA))
        res = ON2_DEC_ERROR;
    else {        
        if(!ctx->config.dec) {
            ctx->config.dec = malloc(sizeof(on2_codec_dec_cfg_t));
            ctx->config.dec->w = si->w;
            ctx->config.dec->h = si->h;
        }
        res = ctx->iface->get_mmap(ctx, mmap, iter);
    }
    return SAVE_STATUS(ctx,res);
}
				    

on2_dec_err_t on2_dec_set_mem_map(on2_dec_ctx_t   *ctx_,         
             			  on2_dec_mmap_t  *mmap,        
             			  unsigned int     num_maps) {  
    on2_codec_ctx_t   *ctx = (on2_codec_ctx_t *)ctx_;    
    on2_dec_err_t      res = ON2_DEC_MEM_ERROR;
    
    if(!ctx || !mmap || !ctx->iface)
        res = ON2_DEC_INVALID_PARAM;
    else if(!(ctx->iface->caps & ON2_DEC_CAP_XMA))
        res = ON2_DEC_ERROR;
    else {
        void         *save = (ctx->priv)?NULL:ctx->config.dec;
        unsigned int i;
        
	for(i=0; i<num_maps; i++, mmap++) {
	    if(!mmap->base)
	    	break;
	    
	    /* Everything look ok, set the mmap in the decoder */
	    res = ctx->iface->set_mmap(ctx, mmap);
	    if(res)
	        break;
	}
        
        if(save) free(save);
    }
    return SAVE_STATUS(ctx,res);
}
