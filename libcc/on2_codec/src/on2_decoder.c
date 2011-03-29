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
#include "on2_codec/internal/on2_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

on2_codec_err_t on2_codec_peek_stream_info(on2_codec_iface_t       *iface,
                        	       const uint8_t         *data,	  
                        	       unsigned int           data_sz,
                        	       on2_codec_stream_info_t *si) {
    on2_codec_err_t res;

    if(!iface || !data || !data_sz || !si
       || si->sz < sizeof(on2_codec_stream_info_t))
        res = ON2_CODEC_INVALID_PARAM;
    else {
        /* Set default/unknown values */
	si->w = 0;
	si->h = 0;

        res = iface->dec.peek_si(data, data_sz, si);
    }
    	
    return res;
}


on2_codec_err_t on2_codec_get_stream_info(on2_codec_ctx_t         *ctx,
                                      on2_codec_stream_info_t *si) {
    on2_codec_err_t res;
    
    if(!ctx || !si || si->sz < sizeof(on2_codec_stream_info_t))
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv)
        res = ON2_CODEC_ERROR;
    else {
        /* Set default/unknown values */
	si->w = 0;
	si->h = 0;

        res = ctx->iface->dec.get_si(ctx->priv->alg_priv, si);
    }

    return SAVE_STATUS(ctx,res);
}				      


on2_codec_err_t on2_codec_decode(on2_codec_ctx_t	*ctx,
                	     uint8_t		*data,     
                	     unsigned int	 data_sz,
                	     void		*user_priv,
			     long		 deadline) {
    on2_codec_err_t res;
    
    if(!ctx || !data || !data_sz)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv)
        res = ON2_CODEC_ERROR;
#if CONFIG_EVAL_LIMIT
    else if(ctx->priv->eval_counter >= 500) {
        ctx->priv->err_detail = "Evaluation limit exceeded.";
        res = ON2_CODEC_ERROR;
    }
#endif
    else {
        res = ctx->iface->dec.decode(ctx->priv->alg_priv, data, data_sz,
	                             user_priv, deadline);
#if CONFIG_EVAL_LIMIT
        ctx->priv->eval_counter++;
#endif
    }

    return SAVE_STATUS(ctx,res);
}

on2_image_t *on2_codec_get_frame(on2_codec_ctx_t  *ctx,
                               on2_codec_iter_t *iter) {
    on2_image_t *img;
    
    if(!ctx || !iter || !ctx->iface || !ctx->priv)
        img = NULL;
    else 
        img = ctx->iface->dec.get_frame(ctx->priv->alg_priv, iter);
    
    return img;
}
 

on2_codec_err_t on2_codec_register_put_frame_cb(on2_codec_ctx_t             *ctx,
                                	    on2_codec_put_frame_cb_fn_t  cb,
					    void                      *user_priv) {
    on2_codec_err_t res;
    
    if(!ctx || !cb)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv
            || !(ctx->iface->caps & ON2_CODEC_CAP_PUT_FRAME))
        res = ON2_CODEC_ERROR;
    else {
        ctx->priv->dec.put_frame_cb.put_frame = cb;
	ctx->priv->dec.put_frame_cb.user_priv = user_priv;
	res = ON2_CODEC_OK;
    }
    
    return SAVE_STATUS(ctx,res);
}


on2_codec_err_t on2_codec_register_put_slice_cb(on2_codec_ctx_t             *ctx,
                                	    on2_codec_put_slice_cb_fn_t  cb,
					    void                      *user_priv){
    on2_codec_err_t res;
    
    if(!ctx || !cb)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv
            || !(ctx->iface->caps & ON2_CODEC_CAP_PUT_FRAME))
        res = ON2_CODEC_ERROR;
    else {
        ctx->priv->dec.put_slice_cb.put_slice = cb;
	ctx->priv->dec.put_slice_cb.user_priv = user_priv;
	res = ON2_CODEC_OK;
    }
    
    return SAVE_STATUS(ctx,res);
}


on2_codec_err_t on2_codec_get_mem_map(on2_codec_ctx_t                *ctx,
                                      on2_codec_mmap_t               *mmap,
                                      on2_codec_iter_t               *iter) {
    on2_codec_err_t res = ON2_CODEC_OK;
    
    if(!ctx || !mmap || !iter || !ctx->iface)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!(ctx->iface->caps & ON2_CODEC_CAP_XMA))
        res = ON2_CODEC_ERROR;
    else
        res = ctx->iface->get_mmap(ctx, mmap, iter);

    return SAVE_STATUS(ctx,res);
}
				    

on2_codec_err_t on2_codec_set_mem_map(on2_codec_ctx_t   *ctx,         
             			  on2_codec_mmap_t  *mmap,        
             			  unsigned int     num_maps) {  
    on2_codec_err_t res = ON2_CODEC_MEM_ERROR;
    
    if(!ctx || !mmap || !ctx->iface)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!(ctx->iface->caps & ON2_CODEC_CAP_XMA))
        res = ON2_CODEC_ERROR;
    else {
        unsigned int i;
        
	for(i=0; i<num_maps; i++, mmap++) {
	    if(!mmap->base)
	    	break;
	    
	    /* Everything look ok, set the mmap in the decoder */
	    res = ctx->iface->set_mmap(ctx, mmap);
	    if(res)
	        break;
	}
    }
    return SAVE_STATUS(ctx,res);
}
