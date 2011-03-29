/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\defgroup decoder Decoder Algorithm Interface
 * \ingroup codec
 * This abstraction allows applications using this decoder to easily support
 * multiple video formats with minimal code duplication. This section describes
 * the interface common to all decoders.
 * @{
 */
 
/*!\file on2_decoder.h
 * \brief Describes the decoder algorithm interface to applications.
 *
 * This file describes the interface between an application and a
 * video decoder algorithm.
 *
 */
#ifndef ON2_DECODER_H
#define ON2_DECODER_H
#include "on2_codec.h"

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging 
 * fields to structures
 */
#define ON2_DECODER_ABI_VERSION (2 + ON2_CODEC_ABI_VERSION) /**<\hideinitializer*/

/*! \brief Decoder capabilities bitfield
 *
 *  Each decoder advertises the capabilities it supports as part of its
 *  ::on2_codec_iface_t interface structure. Capabilities are extra interfaces
 *  or functionality, and are not required to be supported by a decoder.
 *
 *  The available flags are specifiedby ON2_CODEC_CAP_* defines. 
 */
#define ON2_CODEC_CAP_PUT_SLICE  0x10000 /**< Will issue put_slice callbacks */
#define ON2_CODEC_CAP_PUT_FRAME  0x20000 /**< Will issue put_frame callbacks */
#define ON2_CODEC_CAP_POSTPROC   0x40000 /**< Can postprocess decoded frame */

/*! \brief Initialization-time Feature Enabling
 *
 *  Certain codec features must be known at initialization time, to allow for
 *  proper memory allocation.
 *
 *  The available flags are specified by ON2_CODEC_USE_* defines. 
 */
#define ON2_CODEC_USE_POSTPROC   0x10000 /**< Postprocess decoded frame */

/*!\brief Stream properties
 *
 * This structure is used to query or set properties of the decoded
 * stream. Algorithms may extend this structure with data specific
 * to their bitstream by setting the sz member appropriately.
 */
typedef struct {
    unsigned int sz;     /**< Size of this structure */
    unsigned int w;      /**< Width (or 0 for unknown/default) */
    unsigned int h;      /**< Height (or 0 for unknown/default) */
    unsigned int is_kf;  /**< Current frame is a keyframe */
} on2_codec_stream_info_t;

/* REQUIRED FUNCTIONS
 *
 * The following functions are required to be implemented for all decoders.
 * They represent the base case functionality expected of all decoders.
 */


/*!\brief Initialization Configurations
 *
 * This structure is used to pass init time configuration options to the
 * decoder.
 */
typedef struct on2_codec_dec_cfg {
    unsigned int w;      /**< Width */
    unsigned int h;      /**< Height */
} on2_codec_dec_cfg_t; /**< alias for struct on2_codec_dec_cfg */


/*!\brief Initialize a decoder instance
 *
 * Initializes a decoder context using the given interface. Applications
 * should call the on2_codec_dec_init convenience macro instead of this
 * function directly, to ensure that the ABI version number parameter
 * is properly initialized.
 *
 * In XMA mode (activated by setting ON2_CODEC_USE_XMA in the flags
 * parameter), the storage pointed to by the cfg parameter must be
 * kept readable and stable until all memory maps have been set.
 *
 * \param[in]    ctx     Pointer to this instance's context.
 * \param[in]    iface   Pointer to the alogrithm interface to use.
 * \param[in]    cfg     Configuration to use, if known. May be NULL.
 * \param[in]    flags   Bitfield of ON2_CODEC_USE_* flags
 * \param[in]    ver     ABI version number. Must be set to 
 *                       ON2_DECODER_ABI_VERSION
 * \retval #ON2_CODEC_OK
 *     The decoder algorithm initialized.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Memory allocation failed.
 */
on2_codec_err_t on2_codec_dec_init_ver(on2_codec_ctx_t      *ctx,
                                       on2_codec_iface_t    *iface,
                                       on2_codec_dec_cfg_t  *cfg,
                                       on2_codec_flags_t     flags,
                                       int                   ver);

/*!\brief Convenience macro for on2_codec_dec_init_ver()
 *
 * Ensures the ABI version parameter is properly set.
 */
#define on2_codec_dec_init(ctx, iface, cfg, flags) \
    on2_codec_dec_init_ver(ctx, iface, cfg, flags, ON2_DECODER_ABI_VERSION)


/*!\brief Parse stream info from a buffer
 *
 * Performs high level parsing of the bitstream. Construction of a decoder
 * context is not necessary. Can be used to determine if the bitstream is
 * of the proper format, and to extract information from the stream.
 *
 * \param[in]      iface   Pointer to the alogrithm interface
 * \param[in]      data    Pointer to a block of data to parse
 * \param[in]      data_sz Size of the data buffer
 * \param[in,out]  si      Pointer to stream info to update. The size member
 *                         \ref MUST be properly initialized, but \ref MAY be
 *                         clobbered by the algorithm. This parameter \ref MAY
 *                         be NULL.
 *
 * \retval #ON2_CODEC_OK
 *     Bitstream is parsable and stream information updated
 */
on2_codec_err_t on2_codec_peek_stream_info(on2_codec_iface_t       *iface,
                                           const uint8_t           *data,     
                                           unsigned int             data_sz,
                                           on2_codec_stream_info_t *si);


/*!\brief Return information about the current stream.
 *
 * Returns information about the stream that has been parsed during decoding.
 *
 * \param[in]      ctx     Pointer to this instance's context
 * \param[in,out]  si      Pointer to stream info to update. The size member
 *                         \ref MUST be properly initialized, but \ref MAY be
 *                         clobbered by the algorithm. This parameter \ref MAY
 *                         be NULL.
 *
 * \retval #ON2_CODEC_OK
 *     Bitstream is parsable and stream information updated
 */
on2_codec_err_t on2_codec_get_stream_info(on2_codec_ctx_t         *ctx,
                                          on2_codec_stream_info_t *si);


/*!\brief Decode data
 *
 * Processes a buffer of coded data. If the processing results in a new
 * decoded frame becoming available, PUT_SLICE and PUT_FRAME events may be
 * generated, as appropriate. Encoded data \ref MUST be passed in DTS (decode
 * time stamp) order. Frames produced will always be in PTS (presentation
 * time stamp) order.
 *
 * \param[in] ctx          Pointer to this instance's context
 * \param[in] data         Pointer to this block of new coded data. If
 *                         NULL, a ON2_CODEC_CB_PUT_FRAME event is posted
 *                         for the previously decoded frame.
 * \param[in] data_sz      Size of the coded data, in bytes.
 * \param[in] user_priv    Application specific data to associate with 
 *                         this frame.
 * \param[in] deadline     Soft deadline the decoder should attempt to meet,
 *                         in us. Set to zero for unlimited.
 *
 * \return Returns #ON2_CODEC_OK if the coded data was processed completely
 *         and future pictures can be decoded without error. Otherwise,
 *         see the descriptions of the other error codes in ::on2_codec_err_t
 *         for recoverability capabilities.
 */
on2_codec_err_t on2_codec_decode(on2_codec_ctx_t	*ctx,
                                 uint8_t        *data,     
                                 unsigned int            data_sz,
                                 void               *user_priv,  
                                 long                deadline);   


/*!\brief Decoded frames iterator
 *
 * Iterates over a list of the frames available for display. The iterator
 * storage should be initialized to NULL to start the iteration. Iteration is
 * complete when this function returns NULL.
 *
 * The list of available frames becomes valid upon completion of the
 * on2_codec_decode call, and remains valid until the next call to on2_codec_decode.
 *
 * \param[in]     ctx      Pointer to this instance's context
 * \param[in,out] iter     Iterator storage, initialized to NULL
 *
 * \return Returns a pointer to an image, if one is ready for display. Frames
 *         produced will always be in PTS (presentation time stamp) order.
 */
on2_image_t *on2_codec_get_frame(on2_codec_ctx_t  *ctx,
                                 on2_codec_iter_t *iter);
 

/*!\defgroup cap_put_frame Frame-Based Decoding Functions
 *
 * The following functions are required to be implemented for all decoders
 * that advertise the ON2_CODEC_CAP_PUT_FRAME capability. Calling these functions
 * for codecs that don't advertise this capability will result in an error
 * code being returned, usually ON2_CODEC_ERROR
 * @{
 */
 
/*!\brief put frame callback prototype
 *
 * This callback is invoked by the decoder to notify the application of
 * the availability of decoded image data.
 */
typedef void (*on2_codec_put_frame_cb_fn_t) (void 	     *user_priv, 
                                             const on2_image_t *img);


/*!\brief Register for notification of frame completion.
 *
 * Registers a given function to be called when a decoded frame is
 * available.
 *
 * \param[in] ctx          Pointer to this instance's context
 * \param[in] cb           Pointer to the callback function
 * \param[in] user_priv    User's private data
 *
 * \retval #ON2_CODEC_OK
 *     Callback successfully registered.
 * \retval #ON2_CODEC_ERROR
 *     Decoder context not initialized, or algorithm not capable of 
 *     posting slice completion.
 */
on2_codec_err_t on2_codec_register_put_frame_cb(on2_codec_ctx_t             *ctx,
                                                on2_codec_put_frame_cb_fn_t  cb,
                                                void                        *user_priv);


/*!@} - end defgroup cap_put_frame */

/*!\defgroup cap_put_slice Slice-Based Decoding Functions
 *
 * The following functions are required to be implemented for all decoders
 * that advertise the ON2_CODEC_CAP_PUT_SLICE capability. Calling these functions
 * for codecs that don't advertise this capability will result in an error
 * code being returned, usually ON2_CODEC_ERROR
 * @{
 */
 
/*!\brief put slice callback prototype
 *
 * This callback is invoked by the decoder to notify the application of
 * the availability of partially decoded image data. The 
 */
typedef void (*on2_codec_put_slice_cb_fn_t) (void 		  *user_priv, 
                                           const on2_image_t	  *img,       
					   const on2_image_rect_t *valid,     
					   const on2_image_rect_t *update);   


/*!\brief Register for notification of slice completion.
 *
 * Registers a given function to be called when a decoded slice is
 * available.
 *
 * \param[in] ctx          Pointer to this instance's context
 * \param[in] cb           Pointer to the callback function
 * \param[in] user_priv    User's private data
 *
 * \retval #ON2_CODEC_OK
 *     Callback successfully registered.
 * \retval #ON2_CODEC_ERROR
 *     Decoder context not initialized, or algorithm not capable of 
 *     posting slice completion.
 */
on2_codec_err_t on2_codec_register_put_slice_cb(on2_codec_ctx_t             *ctx,
                                                on2_codec_put_slice_cb_fn_t  cb,
                                                void                        *user_priv);


/*!@} - end defgroup cap_put_slice*/

/*!@} - end defgroup decoder*/

#endif

#if !defined(ON2_CODEC_DISABLE_COMPAT) || !ON2_CODEC_DISABLE_COMPAT
#include "on2_decoder_compat.h"
#endif
