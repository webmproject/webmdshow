/*
 * Copyright 2008
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\defgroup encoder Encoder Algorithm Interface
 * \ingroup codec
 * This abstraction allows applications using this encoder to easily support
 * multiple video formats with minimal code duplication. This section describes
 * the interface common to all encoders.
 * @{
 */
 
/*!\file on2_encoder.h
 * \brief Describes the encoder algorithm interface to applications.
 *
 * This file describes the interface between an application and a
 * video encoder algorithm.
 *
 */
#ifndef ON2_ENCODER_H
#define ON2_ENCODER_H
#include "on2_codec.h"

 
/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging 
 * fields to structures
 */
#define ON2_ENCODER_ABI_VERSION (2 + ON2_CODEC_ABI_VERSION) /**<\hideinitializer*/

/*!\brief Generic fixed size buffer structure
 *
 * This structure is able to hold a reference to any fixed size buffer.
 */
typedef struct on2_fixed_buf {
    void          *buf; /**< Pointer to the data */   
    size_t         sz;  /**< Length of the buffer, in chars */
} on2_fixed_buf_t; /**< alias for struct on2_fixed_buf */


/*!\brief Time Stamp Type
 *
 * An integer, which when multiplied by the stream's time base, provides
 * the absolute time of a sample.
 */
typedef int64_t on2_codec_pts_t;


/*!\brief Compressed Frame Flags
 *
 * This type represents a bitfield containing information about a compressed
 * frame that may be useful to an application. The most significant 16 bits
 * can be used by an algorithm to provide additional detail, for example to
 * support frame types that are codec specific (MPEG-1 D-frames for example) 
 */
typedef uint32_t on2_codec_frame_flags_t;
#define ON2_FRAME_IS_KEY       0x1 /**< frame is the start of a GOP */ 
#define ON2_FRAME_IS_DROPPABLE 0x2 /**< frame can be dropped without affecting
                                        the stream (no future frame depends on
                                        this one) */


/*!\brief Encoder output packet variants
 *
 * This enumeration lists the different kinds of data packets that can be
 * returned by calls to on2_codec_get_cx_data(). Algorithms \ref MAY 
 * extend this list to provide additional functionality.
 */
enum on2_codec_cx_pkt_kind {
    ON2_CODEC_CX_FRAME_PKT,    /**< Compressed video frame */
    ON2_CODEC_STATS_PKT,       /**< Two-pass statistics for this frame */
    ON2_CODEC_CUSTOM_PKT=256   /**< Algorithm extensions  */
};


/*!\brief Encoder output packet
 *
 * This structure contains the different kinds of output data the encoder
 * may produce while compressing a frame.
 */
typedef struct on2_codec_cx_pkt {
    enum on2_codec_cx_pkt_kind  kind; /**< packet variant */
    union {
        struct {
            void                    *buf;      /**< compressed data buffer */
            size_t                   sz;       /**< length of compressed data */
            on2_codec_pts_t          pts;      /**< time stamp to show frame
                                                    (in timebase units) */
            unsigned long            duration; /**< duration to show frame
                                                    (in timebase units) */
            on2_codec_frame_flags_t  flags;    /**< flags for this frame */
        } frame;  /**< data for compressed frame packet */
        struct on2_fixed_buf twopass_stats;  /**< data for twopass packet */
        struct on2_fixed_buf raw;            /**< data for arbitrary packets */
    } data; /**< packet data */
} on2_codec_cx_pkt_t; /**< alias for struct on2_codec_cx_pkt */


/*!\brief Rational Number
 *
 * This structure holds a fractional value.
 */
typedef struct on2_rational {
    int num; /**< fraction numerator */
    int den; /**< fraction denominator */
} on2_rational_t; /**< alias for struct on2_rational */


/*!\brief Multipass Encoding Pass */
enum on2_enc_pass {
    ON2_RC_ONE_PASS,   /**< Single pass mode */
    ON2_RC_FIRST_PASS, /**< First pass of multipass mode */
    ON2_RC_LAST_PASS,  /**< Final pass of multipass mode */
};


/*!\brief Rate control mode */
enum on2_rc_mode {
    ON2_VBR, /**< Variable Bit Rate (VBR) mode */
    ON2_CBR  /**< Constant Bit Rate (CBR) mode */
};


/*!\brief Keyframe placement mode */
enum on2_kf_mode {
    ON2_KF_FIXED, /**< Use a fixed interval between keyframes */
    ON2_KF_AUTO,  /**< Encoder determines optimal placement automatically */
};


/*!\brief Encoded Frame Flags
 *
 * This type indicates a bitfield to be passed to on2_codec_encode(), defining
 * per-frame boolean values. By convention, bits common to all codecs will be
 * named ON2_EFLAG_*, and bits specific to an algorithm will be named
 * <algo>_EFLAG_*. The lower order 16 bits are reserved for common use. 
 */
typedef long on2_enc_frame_flags_t;
#define ON2_EFLAG_FORCE_KF (1<<0)  /**< Force this frame to be a keyframe */


/*!\brief Encoder configuration structure
 *
 * This structure contains the encoder settings that have common representations
 * across all codecs. This doesn't imply that all codecs support all features,
 * however.
 */
typedef struct on2_codec_enc_cfg
{
    /*
     * generic settings (g)
     */
     
    /*!\brief Algorithm specific "usage" value
     *
     * Algorithms may define multiple values for usage, which may convey the
     * intent of how the application intends to use the stream. If this value
     * is non-zero, consult the documentation for the codec to determine its
     * meaning.
     */
	unsigned int           g_usage;
    

    /*!\brief Maximum number of threads to use
     *
     * For multithreaded implmentations, use no more than this number of
     * threads. The codec may use fewer threads than allowed. The value
     * 0 is equivalent to the value 1.
     */
    unsigned int           g_threads;
    
    
    /*!\brief Bitstream profile to use
     *
     * Some codecs support a notion of multiple bitstream profiles. Typically
     * this maps to a set of features that are turned on or off. Often the
     * profile to use is determined by the features of the intended decoder.
     * Consult the documentation for the codec to determine the valid values
     * for this parameter, or set to zero for a sane default.
     */    
    unsigned int           g_profile;  /**< profile of bitstream to use */

    
    
    /*!\brief Width of the frame
     *
     * This value identifies the presentation resolution of the frame,
     * in pixels. Note that the frames passed as input to the encoder must
     * have this resolution. Frames will be presented by the decoder in this
     * resolution, independent of any spatial resampling the encoder may do.
     */    
    unsigned int           g_w;


    /*!\brief Height of the frame
     *
     * This value identifies the presentation resolution of the frame,
     * in pixels. Note that the frames passed as input to the encoder must
     * have this resolution. Frames will be presented by the decoder in this
     * resolution, independent of any spatial resampling the encoder may do.
     */    
    unsigned int           g_h;


    /*!\brief Stream timebase units
     *
     * Indicates the smallest interval of time, in seconds, used by the stream.
     * For fixed frame rate material, or variable frame rate material where
     * frames are timed at a multiple of a given clock (ex: video capture),
     * the \ref RECOMMENDED method is to set the timebase to the reciprocal
     * of the frame rate (ex: 1001/30000 for 29.970 Hz NTSC). This allows the
     * pts to correspond to the frame number, which can be handy. For reencoding
     * video from containers with absolute time timestamps, the \ref RECOMMENDED
     * method is to set the timebase to that of the parent container or
     * multimedia framework (ex: 1/1000 for ms, as in FLV).
     */    
    struct on2_rational    g_timebase;
    
	
    /*!\brief Enable error resilient mode.
     *
     * Error resilient mode indicates to the encoder that it should take
     * measures appropriate for streaming over lossy or noisy links, if
     * possible. Set to 1 to enable this feature, 0 to disable it.
     */
    unsigned int		   g_error_resilient;

    
    /*!\brief Multipass Encoding Mode
     *
     * This value should be set to the current phase for multipass encoding.
     * For single pass, set to #ON2_RC_ONE_PASS.
     */
    enum on2_enc_pass      g_pass;


    /*!\brief Allow lagged encoding
     *
     * If set, this value allows the encoder to consume a number of input
     * frames before producing output frames. This allows the encoder to
     * base decisions for the current frame on future frames. This does
     * increase the latency of the encoding pipeline, so it is not appropriate
     * in all situations (ex: realtime encoding).
     *
     * Note that this is a maximum value -- the encoder may produce frames
     * sooner than the given limit. Set this value to 0 to disable this
     * feature.
     */
	unsigned int           g_lag_in_frames;


    /*
     * rate control settings (rc)
     */

    /*!\brief Temporal resampling configuration, if supported by the codec.
     *
     * Temporal resampling allows the codec to "drop" frames as a strategy to
     * meet its target data rate. This can cause temporal discontinuities in
     * the encoded video, which may appear as stuttering during playback. This
     * trade-off is often acceptable, but for many applications is not. It can
     * be disabled in these cases.
     *
     * Note that not all codecs support this feature. All On2 VPx codecs do.
     * For other codecs, consult the documentation for that algorithm.
     *
     * This threshold is described as a percentage of the target data buffer.
     * When the data buffer falls below this percentage of fullness, a
     * dropped frame is indicated. Set the threshold to zero (0) to disable
     * this feature.
     */
    unsigned int           rc_dropframe_thresh;
    

    /*!\brief Enable/disable spatial resampling, if supported by the codec.
     *
     * Spatial resampling allows the codec to compress a lower resolution
     * version of the frame, which is then upscaled by the encoder to the
     * correct presentation resolution. This increases visual quality at
     * low data rates, at the expense of CPU time on the encoder/decoder.
     */
	unsigned int		   rc_resize_allowed;


    /*!\brief Spatial resampling up watermark.
     *
     * This threshold is described as a percentage of the target data buffer.
     * When the data buffer rises above this percentage of fullness, the
     * encoder will step up to a higher resolution version of the frame.
     */
	unsigned int		   rc_resize_up_thresh;


    /*!\brief Spatial resampling up watermark.
     *
     * This threshold is described as a percentage of the target data buffer.
     * When the data buffer falls below this percentage of fullness, the
     * encoder will step down to a lower resolution version of the frame.
     */
	unsigned int		   rc_resize_down_thresh;
	

    /*!\brief Rate control algorithm to use.
     *
     * Indicates whether the end usage of this stream is to be streamed over
     * a bandwith constrained link, indicating that Constant Bit Rate (CBR)
     * mode should be used, or whether it will be played back on a high
     * bandwith link, as from a local disk, where higher variations in
     * bitrate are acceptable.
     */
    enum on2_rc_mode       rc_end_usage;


#if ON2_ENCODER_ABI_VERSION > (1 + ON2_CODEC_ABI_VERSION)                                                    
    /*!\brief Twopass stats buffer.
     *
     * A buffer containing all of the stats packets produced in the first
     * pass, concatenated.
     */
    struct on2_fixed_buf   rc_twopass_stats_in;      
#endif


    /*!\brief Target data rate
     *
     * Target bandwidth to use for this stream, in kilobits per second.
     */
    unsigned int           rc_target_bitrate;


    /*
     * quantizer settings
     */


    /*!\brief Minimum (Best Quality) Quantizer
     *
     * The quantizer is the most direct control over the quality of the
     * encoded image. The range of valid values for the quantizer is codec
     * specific. Consult the documentation for the codec to determine the
     * values to use. To determine the range programattically, call
     * on2_codec_enc_config_default() with a usage value of 0.
     */
    unsigned int           rc_min_quantizer;


    /*!\brief Maximum (Worst Quality) Quantizer
     *
     * The quantizer is the most direct control over the quality of the
     * encoded image. The range of valid values for the quantizer is codec
     * specific. Consult the documentation for the codec to determine the
     * values to use. To determine the range programattically, call
     * on2_codec_enc_config_default() with a usage value of 0.
     */
    unsigned int           rc_max_quantizer;


    /*
     * bitrate tolerance
     */


    /*!\brief Rate control undershoot tolerance
     *
     * This value, expressed as a percentage of the target bitrate, describes
     * the target bitrate for easier frames, allowing bits to be saved for
     * harder frames. Set to zero to use the codec default.
     */
    unsigned int           rc_undershoot_pct;     


    /*!\brief Rate control overshoot tolerance
     *
     * This value, expressed as a percentage of the target bitrate, describes
     * the maximum allowed bitrate for a given frame.  Set to zero to use the
     * codec default.
     */
    unsigned int           rc_overshoot_pct;     
    

	/*
     * decoder buffer model parameters
     */
     
     
    /*!\brief Decoder Buffer Size
     *
     * This value indicates the amount of data that may be buffered by the
     * decoding application. Note that this value is expressed in units of
     * time (milliseconds). For example, a value of 5000 indicates that the
     * client will buffer (at least) 5000ms worth of encoded data. Use the
     * target bitrate (#rc_target_bitrate) to convert to bits/bytes, if
     * necessary.  
     */
    unsigned int           rc_buf_sz;
     
     
    /*!\brief Decoder Buffer Initial Size
     *
     * This value indicates the amount of data that will be buffered by the
     * decoding application prior to beginning playback. This value is
     * expressed in units of time (milliseconds). Use the target bitrate
     * (#rc_target_bitrate) to convert to bits/bytes, if necessary. 
     */
    unsigned int           rc_buf_initial_sz;
     
     
    /*!\brief Decoder Buffer Optimal Size
     *
     * This value indicates the amount of data that the encoder should try
     * to maintain in the decoder's buffer. This value is expressed in units
     * of time (milliseconds). Use the target bitrate (#rc_target_bitrate)
     * to convert to bits/bytes, if necessary.
     */
    unsigned int           rc_buf_optimal_sz;


    /*
     * 2 pass rate control parameters
     */
     
     
    /*!\brief Two-pass mode CBR/VBR bias
     *
     * Bias, expressed on a scale of 0 to 100, for determining target size
     * for the current frame. The value 0 indicates the optimal CBR mode
     * value should be used. The value 100 indicates the optimal VBR mode
     * value should be used. Values in between indicate which way the
     * encoder should "lean."
     */
	unsigned int           rc_2pass_vbr_bias_pct;       /**< RC mode bias between CBR and VBR(0-100: 0->CBR, 100->VBR)   */ 


    /*!\brief Two-pass mode per-GOP minimum bitrate
     *
     * This value, expressed as a percentage of the target bitrate, indicates
     * the minimum bitrate to be used for a single GOP (aka "section")
     */
    unsigned int           rc_2pass_vbr_minsection_pct;


    /*!\brief Two-pass mode per-GOP maximum bitrate
     *
     * This value, expressed as a percentage of the target bitrate, indicates
     * the maximum bitrate to be used for a single GOP (aka "section")
     */
    unsigned int           rc_2pass_vbr_maxsection_pct;


    /*
     * keyframing settings (kf)
     */


    /*!\brief Keyframe placement mode
     *
     * This value indicates whether the encoder should place keyframes at a 
     * fixed interval, or determine the optimal placement automatically
     * (as governed by the #kf_min_dist and #kf_max_dist parameters)
     */
    enum on2_kf_mode       kf_mode;


    /*!\brief Keyframe minimum interval
     *
     * This value, expressed as a number of frames, prevents the encoder from
     * placing a keyframe nearer than kf_min_dist to the previous keyframe. At
     * least kf_min_dist frames non-keyframes will be coded before the next
     * keyframe.
     */
    unsigned int           kf_min_dist;


    /*!\brief Keyframe maximum interval
     *
     * This value, expressed as a number of frames, forces the encoder to code
     * a keyframe if one has not been coded in the last kf_max_dist frames.
     */
    unsigned int           kf_max_dist;


#if ON2_ENCODER_ABI_VERSION == (1 + ON2_CODEC_ABI_VERSION)                                                    
	/*
	 * first pass file. The file based implementation will be deprecated soon.
	 */

	
    /*!\brief Delete First Pass File
     *
     * A boolean. This value indicates that the first pass statistics file
     * should be deleted after the second pass encoding is complete.
     *
     * \deprecated
     * The file-based implmentation of twopass statistics is deprecated in
     * favor of a packet based approach. When the packet based method is
     * enabled, this variable will be removed (and the ABI version number
     * bumped)
     */
    unsigned int	       g_delete_firstpassfile;

	
    /*!\brief First Pass File Name
     *
     * Filename, including path, to be used for storing first pass statistics
     * information.
     *
     * \deprecated
     * The file-based implmentation of twopass statistics is deprecated in
     * favor of a packet based approach. When the packet based method is
     * enabled, this variable will be removed (and the ABI version number
     * bumped)
     */
	char				   g_firstpass_file[512];
#endif
} on2_codec_enc_cfg_t; /**< alias for struct on2_codec_enc_cfg */


/*!\brief Initialize an encoder instance
 *
 * Initializes a encoder context using the given interface. Applications
 * should call the on2_codec_enc_init convenience macro instead of this
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
 *                       ON2_ENCODER_ABI_VERSION
 * \retval #ON2_CODEC_OK
 *     The decoder algorithm initialized.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Memory allocation failed.
 */
on2_codec_err_t on2_codec_enc_init_ver(on2_codec_ctx_t      *ctx,
                                       on2_codec_iface_t    *iface,
                                       on2_codec_enc_cfg_t  *cfg,
                                       on2_codec_flags_t     flags,
                                       int                   ver);


/*!\brief Convenience macro for on2_codec_enc_init_ver()
 *
 * Ensures the ABI version parameter is properly set.
 */
#define on2_codec_enc_init(ctx, iface, cfg, flags) \
    on2_codec_enc_init_ver(ctx, iface, cfg, flags, ON2_ENCODER_ABI_VERSION)
                                       

/*!\brief Get a default configuration
 *
 * Initializes a encoder configuration structure with default values. Supports
 * the notion of "usages" so that an alogrithm may offer different default
 * settings depending on the user's intended goal. This function \ref SHOULD
 * be called by all applications to initialize the configuration structure
 * before specializing the configuration with application specific values.
 *
 * \param[in]    iface   Pointer to the alogrithm interface to use.
 * \param[out]   cfg     Configuration buffer to populate
 * \param[in]    usage   End usage. Set to 0 or use codec specific values.
 *
 * \retval #ON2_CODEC_OK
 *     The configuration was populated.
 * \retval #ON2_CODEC_INCAPABLE
 *     Interface is not an encoder interface.
 * \retval #ON2_CODEC_INVALID_PARAM
 *     A parameter was NULL, or the usage value was not recognized.
 */
on2_codec_err_t  on2_codec_enc_config_default(on2_codec_iface_t    *iface,
                                              on2_codec_enc_cfg_t  *cfg,
                                              unsigned int          usage);


/*!\brief Set or change configuration
 *
 * Reconfigures an encoder instance according to the given configuration.
 *
 * \param[in]    ctx     Pointer to this instance's context
 * \param[in]    cfg     Configuration buffer to use
 *
 * \retval #ON2_CODEC_OK
 *     The configuration was populated.
 * \retval #ON2_CODEC_INCAPABLE
 *     Interface is not an encoder interface.
 * \retval #ON2_CODEC_INVALID_PARAM
 *     A parameter was NULL, or the usage value was not recognized.
 */
on2_codec_err_t  on2_codec_enc_config_set(on2_codec_ctx_t            *ctx,
                                          const on2_codec_enc_cfg_t  *cfg);


/*!\brief Get global stream headers
 *
 * Retrieves a stream level global header packet, if supported by the codec.
 *
 * \param[in]    ctx     Pointer to this instance's context
 *
 * \retval NULL
 *     Encoder does not support global header
 * \retval Non-NULL
 *     Pointer to buffer containing global header packet 
 */
on2_fixed_buf_t *on2_codec_get_global_headers(on2_codec_ctx_t   *ctx);


/*!\brief Encode a frame
 *
 * Encodes a video frame at the given "presentation time." The presentation
 * time stamp (PTS) \ref MUST be strictly increasing.
 *
 * The encoder supports the notion of a soft real-time deadline. Given a
 * non-zero value to the deadline parameter, the encoder will make a "best
 * effort" guarantee to  return before the given time slice expires. It is
 * implicit that limiting the available time to decode will degrade the
 * output quality. The encoder can be given an unlimited time to produce the
 * best possible frame by specifying a deadline of '0'.
 * 
 * When the last frame has been passed to the encoder, this function should
 * be called one additional time with the img parameter set to NULL. This will
 * signal the end-of-stream condition to the encoder and allow it to encode
 * any held buffers.
 *
 * \param[in]    ctx       Pointer to this instance's context
 * \param[in]    img       Image data to encode, NULL to flush.
 * \param[in]    pts       Presentation time stamp, in timebase units.
 * \param[in]    duration  Duration to show frame, in timebase units.
 * \param[in]    flags     Flags to use for encoding this frame.
 * \param[in]    deadline  Time to spend encoding, in microseconds. (0=infinite)
 *
 * \retval #ON2_CODEC_OK
 *     The configuration was populated.
 * \retval #ON2_CODEC_INCAPABLE
 *     Interface is not an encoder interface.
 * \retval #ON2_CODEC_INVALID_PARAM
 *     A parameter was NULL, the image format is unsupported, etc.
 */
on2_codec_err_t  on2_codec_encode(on2_codec_ctx_t            *ctx,
                                  const on2_image_t          *img,
                                  on2_codec_pts_t             pts,
                                  unsigned long               duration,
                                  on2_enc_frame_flags_t       flags,
                                  unsigned long               deadline);


/*!\brief Encoded data iterator
 *
 * Iterates over a list of data packets to be passed from the encoder to the
 * application. The different kinds of packets available are enumerated in
 * #on2_codec_cx_pkt_kind.
 *
 * #ON2_CODEC_CX_FRAME_PKT packets should be passed to the application's
 * muxer. Multiple compressed frames may be in the list.
 * #ON2_CODEC_STATS_PKT packets should be appended to a global buffer. 
 *
 * The application \ref MUST silently ignore any packet kinds that it does
 * not recognize or support. 
 *
 * The data buffers returned from this function are only guaranteed to be
 * valid until the application makes another call to any on2_codec_* function.
 *
 * \param[in]     ctx      Pointer to this instance's context
 * \param[in,out] iter     Iterator storage, initialized to NULL
 *
 * \return Returns a pointer to an output data packet (compressed frame data,
 *         twopass statistics, etc) or NULL to signal end-of-list.
 * 
 */
on2_codec_cx_pkt_t *on2_codec_get_cx_data(on2_codec_ctx_t   *ctx,
                                          on2_codec_iter_t  *iter);


/*!\brief Get Preview Frame
 *
 * Returns an image that can be used as a preview. Shows the image as it would
 * exist at the decompressor. The application \ref MUST NOT write into this
 * image buffer.
 *
 * \param[in]     ctx      Pointer to this instance's context
 *
 * \return Returns a pointer to a preview image, or NULL if no image is
 *         available.
 * 
 */
on2_image_t *on2_codec_get_preview_frame(on2_codec_ctx_t   *ctx);


/*!@} - end defgroup encoder*/

#endif
