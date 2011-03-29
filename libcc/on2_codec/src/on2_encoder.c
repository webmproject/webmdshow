/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\file on2_encoder.c
 * \brief Provides the high level interface to wrap encoder algorithms.
 *
 */
#include <limits.h>
#include "on2_codec/internal/on2_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

on2_codec_err_t on2_codec_enc_init_ver(on2_codec_ctx_t      *ctx,
                	               on2_codec_iface_t    *iface,
                                       on2_codec_enc_cfg_t  *cfg,
                                       on2_codec_flags_t     flags,
                                       int                   ver) {
    on2_codec_err_t res;
    
    if(ver != ON2_ENCODER_ABI_VERSION)
        res = ON2_CODEC_ABI_MISMATCH;
    else if(!ctx || !iface || !cfg)
        res = ON2_CODEC_INVALID_PARAM;
    else if(iface->abi_version != ON2_CODEC_INTERNAL_ABI_VERSION)
        res = ON2_CODEC_ABI_MISMATCH;
    else if(!(iface->caps & ON2_CODEC_CAP_ENCODER))
        res = ON2_CODEC_INCAPABLE;
   else if((flags & ON2_CODEC_USE_XMA) && !(iface->caps & ON2_CODEC_CAP_XMA))
        res = ON2_CODEC_INCAPABLE;
    else {
        ctx->iface = iface;
	ctx->name = iface->name;
	ctx->priv = NULL;
        ctx->init_flags = flags;
        ctx->config.enc = cfg;
	res = ctx->iface->init(ctx);
	if(res)
            on2_codec_destroy(ctx);
	if(ctx->priv)
	    ctx->priv->iface = ctx->iface;
    }
    
    return SAVE_STATUS(ctx,res);
}			       



on2_codec_err_t  on2_codec_enc_config_default(on2_codec_iface_t    *iface,
                                              on2_codec_enc_cfg_t  *cfg,
                                              unsigned int          usage) {    
    on2_codec_err_t res;
    on2_codec_enc_cfg_map_t *map;
    
    if(!iface || !cfg || usage > INT_MAX)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!(iface->caps & ON2_CODEC_CAP_ENCODER))
        res = ON2_CODEC_INCAPABLE;
    else {
        res = ON2_CODEC_INVALID_PARAM;
        for(map = iface->enc.cfg_maps; map->usage >= 0; map++) {
            if(map->usage == (int)usage) {
                *cfg = map->cfg;
                cfg->g_usage = usage;
                res = ON2_CODEC_OK;
                break;
            }
        }
    }
    return res;
}
                                              

on2_codec_err_t  on2_codec_encode(on2_codec_ctx_t            *ctx,
                                  const on2_image_t          *img,
                                  on2_codec_pts_t             pts,
                                  unsigned long               duration,
                                  on2_enc_frame_flags_t       flags,
                                  unsigned long               deadline) {
    on2_codec_err_t res;
    
    if(!ctx || (img && !duration))
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv)
        res = ON2_CODEC_ERROR;
    else if(!(ctx->iface->caps & ON2_CODEC_CAP_ENCODER))
        res = ON2_CODEC_INCAPABLE;
#if CONFIG_EVAL_LIMIT
    else if(ctx->priv->eval_counter >= 500) {
        ctx->priv->err_detail = "Evaluation limit exceeded.";
        res = ON2_CODEC_ERROR;
    }
#endif
    else {
        res = ctx->iface->enc.encode(ctx->priv->alg_priv, img, pts,
	                             duration, flags, deadline);
#if CONFIG_EVAL_LIMIT
        ctx->priv->eval_counter++;
#endif
    }

    return SAVE_STATUS(ctx,res);
}


on2_codec_cx_pkt_t *on2_codec_get_cx_data(on2_codec_ctx_t   *ctx,
                                          on2_codec_iter_t  *iter) {
    on2_codec_cx_pkt_t *pkt = NULL;
    
    if(ctx) {
        if(!iter)
            ctx->err = ON2_CODEC_INVALID_PARAM;        
        else if(!ctx->iface || !ctx->priv)
            ctx->err = ON2_CODEC_ERROR;        
        else if(!(ctx->iface->caps & ON2_CODEC_CAP_ENCODER))
            ctx->err = ON2_CODEC_INCAPABLE;        
        else 
            pkt = ctx->iface->enc.get_cx_data(ctx->priv->alg_priv, iter);
    }

    return pkt;
                                          
}


on2_image_t *on2_codec_get_preview_frame(on2_codec_ctx_t   *ctx) {
    on2_image_t *img = NULL;
    
    if(ctx) {
        if(!ctx->iface || !ctx->priv)
            ctx->err = ON2_CODEC_ERROR;        
        else if(!(ctx->iface->caps & ON2_CODEC_CAP_ENCODER))
            ctx->err = ON2_CODEC_INCAPABLE;        
        else if(!ctx->iface->enc.get_preview)
            ctx->err = ON2_CODEC_INCAPABLE;        
        else 
            img = ctx->iface->enc.get_preview(ctx->priv->alg_priv);
    }    
    return img;
}


on2_fixed_buf_t *on2_codec_get_global_headers(on2_codec_ctx_t   *ctx) {
    on2_fixed_buf_t *buf = NULL;
    
    if(ctx) {
        if(!ctx->iface || !ctx->priv)
            ctx->err = ON2_CODEC_ERROR;        
        else if(!(ctx->iface->caps & ON2_CODEC_CAP_ENCODER))
            ctx->err = ON2_CODEC_INCAPABLE;        
        else if(!ctx->iface->enc.get_glob_hdrs)
            ctx->err = ON2_CODEC_INCAPABLE;        
        else 
            buf = ctx->iface->enc.get_glob_hdrs(ctx->priv->alg_priv);
    }
    return buf;
}


on2_codec_err_t  on2_codec_enc_config_set(on2_codec_ctx_t            *ctx,
                                          const on2_codec_enc_cfg_t  *cfg) {
    on2_codec_err_t res;
    
    if(!ctx || !ctx->iface || !ctx->priv || !cfg)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!(ctx->iface->caps & ON2_CODEC_CAP_ENCODER))
        res = ON2_CODEC_INCAPABLE;
    else 
        res = ctx->iface->enc.cfg_set(ctx->priv->alg_priv, cfg);
    
    return SAVE_STATUS(ctx,res);
}


int on2_codec__pkt_list_add(struct on2_codec__pkt_list *list,
                            const struct on2_codec_cx_pkt *pkt) {
    if(list->cnt < list->max) {
        list->pkts[list->cnt++] = *pkt;
        return 0;
    }
    return 1;
}


on2_codec_cx_pkt_t* on2_codec__pkt_list_get(struct on2_codec__pkt_list *list,
                                            on2_codec_iter_t           *iter) {
    on2_codec_cx_pkt_t* pkt;

    if(!(*iter)) {
        *iter = list->pkts;
    }
    pkt = (void*)*iter;
    if(pkt - list->pkts < list->cnt)
        *iter = pkt + 1;
    else
        pkt = NULL;
    return pkt;
}


