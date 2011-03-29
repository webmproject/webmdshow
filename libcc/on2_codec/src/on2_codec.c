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
#include "on2_codec/internal/on2_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

const char *on2_codec_iface_name(on2_codec_iface_t *iface) {
    return iface?iface->name:"<invalid interface>";
}

const char *on2_codec_err_to_string(on2_codec_err_t  err) {
    switch(err) {
        case ON2_CODEC_OK:
	    return "Success";
	case ON2_CODEC_ERROR:
	    return "Unspecified internal error";
        case ON2_CODEC_MEM_ERROR:
	    return "Memory allocation error";
	case ON2_CODEC_ABI_MISMATCH:
	    return "ABI version mismatch";
	case ON2_CODEC_INCAPABLE:
	    return "Codec does not implement requested capability";
        case ON2_CODEC_UNSUP_BITSTREAM:
	    return "Bitstream not supported by this decoder";
	case ON2_CODEC_UNSUP_FEATURE:
	    return "Bitstream required feature not supported by this decoder";
	case ON2_CODEC_CORRUPT_FRAME:
	    return "Corrupt frame detected";
	case  ON2_CODEC_INVALID_PARAM:
	    return "Invalid parameter";
	case ON2_CODEC_LIST_END:
	    return "End of iterated list";
    }
    return "Unrecognized error code";	           
}

const char *on2_codec_error(on2_codec_ctx_t  *ctx) {
    return (ctx)?on2_codec_err_to_string(ctx->err)
                :on2_codec_err_to_string(ON2_CODEC_INVALID_PARAM);
}
    
const char *on2_codec_error_detail(on2_codec_ctx_t  *ctx) {
    if(ctx && ctx->err && ctx->priv)
        return ctx->priv->err_detail;
    return NULL;
}


on2_codec_err_t on2_codec_dec_init_ver(on2_codec_ctx_t      *ctx,
                	               on2_codec_iface_t    *iface,
                                       on2_codec_dec_cfg_t  *cfg,
                                       on2_codec_flags_t     flags,
                                       int                   ver) {
    on2_codec_err_t res;
    
    if(ver != ON2_DECODER_ABI_VERSION)
        res = ON2_CODEC_ABI_MISMATCH;
    else if(!ctx || !iface)
        res = ON2_CODEC_INVALID_PARAM;
    else if(iface->abi_version != ON2_CODEC_INTERNAL_ABI_VERSION)
        res = ON2_CODEC_ABI_MISMATCH;
    else if((flags & ON2_CODEC_USE_XMA) && !(iface->caps & ON2_CODEC_CAP_XMA))
        res = ON2_CODEC_INCAPABLE;
    else if((flags & ON2_CODEC_USE_POSTPROC) && !(iface->caps & ON2_CODEC_CAP_POSTPROC))
        res = ON2_CODEC_INCAPABLE;
    else {
        memset(ctx, 0, sizeof(*ctx));
        ctx->iface = iface;
	ctx->name = iface->name;
	ctx->priv = NULL;
        ctx->init_flags = flags;        
        ctx->config.dec = cfg;
        res = ON2_CODEC_OK;
        if(!(flags & ON2_CODEC_USE_XMA)) {
	    res = ctx->iface->init(ctx);
	    if(res)
                on2_codec_destroy(ctx);
	    if(ctx->priv)
	        ctx->priv->iface = ctx->iface;
        }
    }
    return SAVE_STATUS(ctx,res);
}			       


on2_codec_err_t on2_codec_destroy(on2_codec_ctx_t *ctx) {
    on2_codec_err_t res;
    
    if(!ctx)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv)
        res = ON2_CODEC_ERROR;
    else {
	if(ctx->priv->alg_priv)
	    ctx->iface->destroy(ctx->priv->alg_priv);
        ctx->iface = NULL;
	ctx->name = NULL;
	ctx->priv = NULL;
	res = ON2_CODEC_OK;
    }
    
    return SAVE_STATUS(ctx,res);
}


on2_codec_caps_t on2_codec_get_caps(on2_codec_iface_t *iface) {
    return (iface)? iface->caps : 0;
}


on2_codec_err_t on2_codec_control_(on2_codec_ctx_t  *ctx,
                                   int               ctrl_id,     
                                   ...) {
    on2_codec_err_t res;
    
    if(!ctx || !ctrl_id)
        res = ON2_CODEC_INVALID_PARAM;
    else if(!ctx->iface || !ctx->priv || !ctx->iface->ctrl_maps)
        res = ON2_CODEC_ERROR;
    else {
        on2_codec_ctrl_fn_map_t *entry;
	
	res = ON2_CODEC_ERROR;
	for(entry = ctx->iface->ctrl_maps; entry && entry->fn; entry++) {
	    if(!entry->ctrl_id || entry->ctrl_id == ctrl_id) {
                va_list  ap;

                va_start(ap, ctrl_id);
                res = entry->fn(ctx->priv->alg_priv, ctrl_id, ap);
                va_end(ap);        
                break;
	    }
	}
    }
    return SAVE_STATUS(ctx,res);
}



