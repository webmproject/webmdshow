/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/** \file on2_blit.h
 *  \brief Defines the interface to the on2 color converting blitter routines.
 *
 */
#ifndef ON2_BLIT_H
#define ON2_BLIT_H

#ifdef __cplusplus
extern "C" {
#endif
#include "on2_image.h"

#define ON2_BLIT_ABI_VERSION (ON2_IMAGE_ABI_VERSION + 1)

/* The following values define the informational bits populated by on2_blit
 * follwing a successful blit operation.
 */
 
/**\brief blit info has been updated. */
#define ON2_BLIT_INFO_VALID             (1 << 0)
/**\brief blit was performed by an optimized blitter */
#define ON2_BLIT_INFO_OPTIMIZED         (1 << 1)


/* The following values are returned by on2_blit follwing an unsuccessful
 * blit operation.
 */
enum on2_blit_err {
    ON2_BLIT_OK                = 0,
    /**\brief ABI version mismatch */
    ON2_BLIT_ERR_ABI_MISMATCH  = -1, 
    /**\brief Invalid parameter (image pointer was NULL) */
    ON2_BLIT_ERR_INVALID_PARAM = -2,
    /**\brief Library doesn't have support for the requested conversion */
    ON2_BLIT_ERR_UNSUPPORTED   = -3,
};


/** \brief Perform color conversion.
 *
 * This function performs the color converting blit from the source image to
 * the destination image. The source area to be blitted is defined by the
 * viewport rectangle of the src image (it's d_w and d_h parameters). The
 * destination area to be blitted into is defined by the viewport rectangle
 * of the src image (it's d_w and d_h parameters). If the source rectangle
 * is larger than the destination in either dimension, the result will be
 * cropped along the bottom or right edge, accordingly. Under no
 * circumstances will the blitter write outside of the destination viewport.
 * If the update_dst parameter is set, the destination image's viewport
 * rectangle will be updated to include only the pixels contained by the
 * blit. This could result in the viewport shrinking, but never growing.
 *
 * \param[out]   dst         Destination Image Descriptor
 * \param[in]    src         Source Image Descriptor
 * \param[in]    abi         ABI version. This should always be set to the
 *                           *symbolic* constant ON2_BLIT_ABI_VERSION.
 * \param[out]   info        Additional information about the blit operation.
 *                           If set, successful blits will update this
 *                           with information from the ON2_BLIT_INFO_*
 *                           bitfield. May be NULL.
 * \param[in]    update_dst  (bool) update the dst viewport to fit only 
 *                           the new image.
 * 
 * \retval 0 on success, nonzero on error. See ON2_BLIT_ERR_*
 */
int
on2_blit_ex(on2_image_t       *dst,
            const on2_image_t *src,
            int                abi,
            int               *info,
            int                update_dst);

/** \brief Simple blit convenience macro
 *
 * This macro can be used for the common case of a blit where the info and
 * update_dst flags are not needed.
 */
#define on2_blit(dst, src) on2_blit_ex(dst, src, ON2_BLIT_ABI_VERSION, NULL, 0)
#ifdef __cplusplus
}
#endif
#endif
