/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\defgroup codec Common Algorithm Interface
 * This abstraction allows applications to easily support multiple video
 * formats with minimal code duplication. This section describes the interface
 * common to all codecs (both encoders and decoders).
 * @{
 */
 
/*!\file on2_codec.h
 * \brief Describes the codec algorithm interface to applications.
 *
 * This file describes the interface between an application and a
 * video codec algorithm.
 *
 * An application instantiates a specific codec instance by using 
 * on2_codec_init() and a pointer to the algorithm's interface structure:
 *     <pre>
 *     my_app.c:
 *       extern on2_codec_iface_t my_codec;
 *       {
 *           on2_codec_ctx_t algo; 
 *           res = on2_codec_init(&algo, &my_codec);
 *       }
 *     </pre>
 *
 * Once initialized, the instance is manged using other functions from
 * the on2_codec_* family. 
 */
#ifndef ON2_CODEC_H
#define ON2_CODEC_H
#ifdef HAVE_CONFIG_H
#  include "on2_codecs_config.h"
#endif
#if defined(HAVE_ON2_PORTS) && HAVE_ON2_PORTS
#  include "on2_ports/on2_integer.h"
#else
#  include "on2_integer.h"
#endif
#include "on2_image.h"

/*!\brief Decorator indicating a function is potentially unused */
#ifdef UNUSED
#elif __GNUC__
#define UNUSED __attribute__ ((unused));
#else
#define UNUSED
#endif

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging 
 * fields to structures
 */
#define ON2_CODEC_ABI_VERSION (2 + ON2_IMAGE_ABI_VERSION) /**<\hideinitializer*/

/*!\brief Algorithm return codes */
typedef enum {
    /*!\brief Operation completed without error */
    ON2_CODEC_OK,
    
    /*!\brief Unspecified error */
    ON2_CODEC_ERROR,
    
    /*!\brief Memory operation failed */
    ON2_CODEC_MEM_ERROR,
    
    /*!\brief ABI version mismatch */
    ON2_CODEC_ABI_MISMATCH,
    
    /*!\brief Algorithm does not have required capability */
    ON2_CODEC_INCAPABLE,

    /*!\brief The given bitstream is not supported.
     *
     * The bitstream was unable to be parsed at the highest level. The decoder
     * is unable to proceed. This error \ref SHOULD be treated as fatal to the
     * stream. */
    ON2_CODEC_UNSUP_BITSTREAM,
    
    /*!\brief Encoded bitstream uses an unsupported feature
     *
     * The decoder does not implement a feature required by the encoder. This
     * return code should only be used for features that prevent future
     * pictures from being properly decoded. This error \ref MAY be treated as
     * fatal to the stream or \ref MAY be treated as fatal to the current GOP.
     */
    ON2_CODEC_UNSUP_FEATURE,
    
    /*!\brief The coded data for this stream is corrupt or incomplete
     *
     * There was a problem decoding the current frame.  This return code
     * should only be used for failures that prevent future pictures from
     * being properly decoded. This error \ref MAY be treated as fatal to the
     * stream or \ref MAY be treated as fatal to the current GOP. If decoding
     * is continued for the current GOP, artifacts may be present.
     */
    ON2_CODEC_CORRUPT_FRAME,
    
    /*!\brief An application-supplied parameter is not valid.
     *
     */
    ON2_CODEC_INVALID_PARAM,
    
    /*!\brief An iterator reached the end of list.
     *
     */
    ON2_CODEC_LIST_END,
    
} on2_codec_err_t;


/*! \brief Codec capabilities bitfield
 *
 *  Each codec advertises the capabilities it supports as part of its
 *  ::on2_codec_iface_t interface structure. Capabilities are extra interfaces
 *  or functionality, and are not required to be supported.
 *
 *  The available flags are specified by ON2_CODEC_CAP_* defines. 
 */
typedef long on2_codec_caps_t;
#define ON2_CODEC_CAP_DECODER 0x1 /**< Is a decoder */
#define ON2_CODEC_CAP_ENCODER 0x2 /**< Is an encoder */
#define ON2_CODEC_CAP_XMA     0x4 /**< Supports eXternal Memory Allocation */


/*! \brief Initialization-time Feature Enabling
 *
 *  Certain codec features must be known at initialization time, to allow for
 *  proper memory allocation.
 *
 *  The available flags are specified by ON2_CODEC_USE_* defines. 
 */
typedef long on2_codec_flags_t;
#define ON2_CODEC_USE_XMA 0x00000001    /**< Use eXternal Memory Allocation mode */


/*!\brief Codec interface structure.
 *
 * Contains function pointers and other data private to the codec
 * implementation. This structure is opaque to the application.
 */
typedef const struct on2_codec_iface on2_codec_iface_t;


/*!\brief Codec private data structure.
 *
 * Contains data private to the codec implementation. This structure is opaque
 * to the application.
 */
typedef       struct on2_codec_priv  on2_codec_priv_t;


/*!\brief Iterator
 *
 * Opaque storage used for iterating over lists.
 */
typedef const void* on2_codec_iter_t;


/*!\brief Codec context structure
 *
 * All codecs \ref MUST support this context structure fully. In general,
 * this data should be considered private to the codec algorithm, and 
 * not be manipulated or examined by the calling application. Applications
 * may reference the 'name' member to get a printable description of the
 * algorithm.
 */
typedef struct {
    const char*              name;        /**< Printable interface name */
    on2_codec_iface_t       *iface;       /**< Interface pointers */
    on2_codec_err_t          err;         /**< Last returned error */
    on2_codec_flags_t        init_flags;  /**< Flags passed at init time */
    union {
        struct on2_codec_dec_cfg  *dec;   /**< Decoder Configuration Pointer */
        struct on2_codec_enc_cfg  *enc;   /**< Encoder Configuration Pointer */
        void                      *raw;
    }                        config;      /**< Configuration pointer aliasing union */
    on2_codec_priv_t        *priv;        /**< Algorithm private storage */
} on2_codec_ctx_t;


/*!\brief Return the build configuration
 *
 * Returns a printable string containing an encoded version of the build
 * configuration. This may be useful to On2 support.
 *
 */
const char *on2_codec_build_config(void);


/*!\brief Return the name for a given interface
 *
 * Returns a human readable string for name of the given codec interface.
 *
 * \param[in]    iface     Interface pointer
 *
 */
const char *on2_codec_iface_name(on2_codec_iface_t *iface);


/*!\brief Convert error number to printable string
 *
 * Returns a human readable string for the last error returned by the
 * algorithm. The returned error will be one line and will not contain
 * any newline characters.
 *
 *
 * \param[in]    err     Error number.
 *
 */
const char *on2_codec_err_to_string(on2_codec_err_t  err);


/*!\brief Retrieve error synopsis for codec context
 *
 * Returns a human readable string for the last error returned by the
 * algorithm. The returned error will be one line and will not contain
 * any newline characters.
 *
 *
 * \param[in]    ctx     Pointer to this instance's context.
 *
 */
const char *on2_codec_error(on2_codec_ctx_t  *ctx);


/*!\brief Retrieve detailed error information for codec context
 *
 * Returns a human readable string providing detailed information about
 * the last error. 
 *
 * \param[in]    ctx     Pointer to this instance's context.
 *
 * \retval NULL
 *     No detailed information is available.
 */
const char *on2_codec_error_detail(on2_codec_ctx_t  *ctx);


/* REQUIRED FUNCTIONS
 *
 * The following functions are required to be implemented for all codecs.
 * They represent the base case functionality expected of all codecs.
 */

/*!\brief Destroy a codec instance
 *
 * Destroys a codec context, freeing any associated memory buffers.
 *
 * \param[in] ctx   Pointer to this instance's context
 *
 * \retval #ON2_CODEC_OK
 *     The codec algorithm initialized.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Memory allocation failed.
 */
on2_codec_err_t on2_codec_destroy(on2_codec_ctx_t *ctx);


/*!\brief Get the capabilities of an algorithm.
 *
 * Retrieves the capabliities bitfield from the algorithm's interface.
 *
 * \param[in] iface   Pointer to the alogrithm interface
 *
 */
on2_codec_caps_t on2_codec_get_caps(on2_codec_iface_t *iface);


/*!\brief Control algorithm
 *
 * This function is used to exchange algorithm specific data with the codec
 * instance. This can be used to implement features specific to a particular
 * algorithm. 
 *
 * This wrapper function dispatches the request to the helper function
 * associated with the given ctrl_id. It tries to call this function
 * transparantly, but will return #ON2_CODEC_ERROR if the request could not
 * be dispatched.
 *
 * Note that this function should not be used directly. Call the 
 * #on2_codec_control wrapper macro instead.
 *
 * \param[in]     ctx              Pointer to this instance's context
 * \param[in]     ctrl_id          Algorithm specific control identifier
 *
 * \retval #ON2_CODEC_OK
 *     The control request was processed.
 * \retval #ON2_CODEC_ERROR
 *     The control request was not processed.
 * \retval #ON2_CODEC_INVALID_PARAM
 *     The data was not valid.
 */
on2_codec_err_t on2_codec_control_(on2_codec_ctx_t  *ctx,
                                   int               ctrl_id,     
                                   ...);	    
#if defined(ON2_DISABLE_CTRL_TYPECHECKS) && ON2_DISABLE_CTRL_TYPECHECKS
#    define on2_codec_control(ctx,id,data) on2_codec_control_(ctx,id,data)
#    define ON2_CTRL_USE_TYPE(id, typ)
#    define ON2_CTRL_VOID(id, typ)

#else
/*!\brief on2_codec_control wrapper macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to on2_codec_control_().
 *
 * \internal
 * It works by dispatching the call to the control function through a wrapper
 * function named with the id parameter.
 */
#    define on2_codec_control(ctx,id,data) on2_codec_control__##id(ctx,id,data)\
 /**<\hideinitializer*/


/*!\brief on2_codec_control type definition macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to on2_codec_control_(). It defines the type of the argument for a given
 * control identifier.
 *
 * \internal
 * It defines a static function with
 * the correctly typed arguments as a wrapper to the type-unsafe internal
 * function.
 */
#    define ON2_CTRL_USE_TYPE(id, typ) \
     static on2_codec_err_t \
     on2_codec_control__##id(on2_codec_ctx_t*, int, typ) UNUSED;\
     \
     static on2_codec_err_t \
     on2_codec_control__##id(on2_codec_ctx_t  *ctx, int ctrl_id, typ data) {\
         return on2_codec_control_(ctx, ctrl_id, data);\
     } /**<\hideinitializer*/


/*!\brief on2_codec_control void type definition macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to on2_codec_control_(). It indicates that a given control identifier takes
 * no argument.
 *
 * \internal
 * It defines a static function without a data argument as a wrapper to the
 * type-unsafe internal function.
 */
#    define ON2_CTRL_VOID(id) \
     static on2_codec_err_t \
     on2_codec_control__##id(on2_codec_ctx_t*, int) UNUSED;\
     \
     static on2_codec_err_t \
     on2_codec_control__##id(on2_codec_ctx_t  *ctx, int ctrl_id) {\
         return on2_codec_control_(ctx, ctrl_id);\
     } /**<\hideinitializer*/


#endif


/*!\defgroup cap_xma External Memory Allocation Functions
 *
 * The following functions are required to be implemented for all codecs
 * that advertise the ON2_CODEC_CAP_XMA capability. Calling these functions
 * for codecs that don't advertise this capability will result in an error
 * code being returned, usually ON2_CODEC_INCAPABLE
 * @{
 */


/*!\brief Memory Map Entry
 *
 * This structure is used to contain the properties of a memory segment. It
 * is populated by the codec in the request phase, and by the calling
 * application once the requested allocation has been performed.
 */
typedef struct on2_codec_mmap {
    /*
     * The following members are set by the codec when requesting a segment
     */
    unsigned int   id;     /**< identifier for the segment's contents */
    unsigned long  sz;     /**< size of the segment, in bytes */
    unsigned int   align;  /**< required alignment of the segment, in bytes */
    unsigned int   flags;  /**< bitfield containing segment properties */
#define ON2_CODEC_MEM_ZERO     0x1  /**< Segment must be zeroed by allocation */
#define ON2_CODEC_MEM_WRONLY   0x2  /**< Segment need not be readable */
#define ON2_CODEC_MEM_FAST     0x4  /**< Place in fast memory, if available */

    /* The following members are to be filled in by the allocation function */    
    void          *base;   /**< pointer to the allocated segment */
    void         (*dtor)(struct on2_codec_mmap *map); /**< destructor to call */
    void          *priv;   /**< allocator private storage */
} on2_codec_mmap_t; /**< alias for struct on2_codec_mmap */ 


/*!\brief Iterate over the list of segments to allocate.
 *
 * Iterates over a list of the segments to allocate. The iterator storage
 * should be initialized to NULL to start the iteration. Iteration is complete
 * when this function returns ON2_CODEC_LIST_END. The amount of memory needed to
 * allocate is dependant upon the size of the encoded stream. In cases where the
 * stream is not available at allocation time, a fixed size must be requested.
 * The codec will not be able to operate on streams larger than the size used at
 * allocation time.
 *
 * \param[in]      ctx     Pointer to this instance's context.
 * \param[out]     mmap    Pointer to the memory map entry to populate.
 * \param[in,out]  iter    Iterator storage, initialized to NULL
 *
 * \retval #ON2_CODEC_OK
 *     The memory map entry was populated.
 * \retval #ON2_CODEC_ERROR
 *     Codec does not support XMA mode.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Unable to determine segment size from stream info.
 */
on2_codec_err_t on2_codec_get_mem_map(on2_codec_ctx_t                *ctx,
                                      on2_codec_mmap_t               *mmap,
                                      on2_codec_iter_t               *iter);


/*!\brief Identify allocated segments to codec instance
 *
 * Stores a list of allocated segments in the codec. Segments \ref MUST be
 * passed in the order they are read from on2_codec_get_mem_map(), but may be
 * passed in groups of any size. Segments \ref MUST be set only once. The
 * allocation function \ref MUST ensure that the on2_codec_mmap_t::base member
 * is non-NULL. If the segment requires cleanup handling (eg, calling free()
 * or close()) then the on2_codec_mmap_t::dtor member \ref MUST be populated.
 *
 * \param[in]      ctx     Pointer to this instance's context.
 * \param[in]      mmaps   Pointer to the first memory map entry in the list.
 * \param[in]      num_maps  Number of entries being set at this time
 *
 * \retval #ON2_CODEC_OK
 *     The segment was stored in the codec context.
 * \retval #ON2_CODEC_INCAPABLE
 *     Codec does not support XMA mode.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Segment base address was not set, or segment was already stored.
 
 */
on2_codec_err_t  on2_codec_set_mem_map(on2_codec_ctx_t   *ctx,
                                       on2_codec_mmap_t  *mmaps,
                                       unsigned int       num_maps);

/*!@} - end defgroup cap_xma*/
/*!@} - end defgroup codec*/


#endif
