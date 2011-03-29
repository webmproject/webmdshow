/*
 * Copyright 2007
 * On2 Technologies Incorporated
 *
 * All rights reserved.
 *
 * $Id$
 */
/*!\file decoder_impl.h
 * \brief Describes the decoder algorithm interface for algorithm
 *        implementations.
 *
 * This file defines the private structures and data types that are only
 * relevant to implementing an algorithm, as opposed to using it.
 *
 * To create a decoder algorithm class, an interface structure is put
 * into the global namespace:
 *     <pre>
 *     my_codec.c:
 *       on2_codec_iface_t my_codec = {
 *           "My Codec v1.0",
 *           ON2_CODEC_ALG_ABI_VERSION,
 *           ...
 *       };
 *     </pre>
 *
 * An application instantiates a specific decoder instance by using 
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
#ifndef ON2_CODEC_INTERNAL_H
#define ON2_CODEC_INTERNAL_H
#include "../on2_decoder.h"
#include "../on2_encoder.h"
#include <stdarg.h>


/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging 
 * fields to structures
 */
#define ON2_CODEC_INTERNAL_ABI_VERSION (2) /**<\hideinitializer*/

typedef struct on2_codec_alg_priv  on2_codec_alg_priv_t;

/*!\brief init function pointer prototype
 *
 * Performs algorithm-specific initialization of the decoder context. This
 * function is called by the generic on2_codec_init() wrapper function, so
 * plugins implementing this interface may trust the input parameters to be
 * properly initialized.
 *
 * \param[in] ctx   Pointer to this instance's context
 * \retval #ON2_CODEC_OK
 *     The input stream was recognized and decoder initialized.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Memory operation failed.
 */
typedef on2_codec_err_t (*on2_codec_init_fn_t)  (on2_codec_ctx_t *ctx);
                                       
/*!\brief destroy function pointer prototype
 *
 * Performs algorithm-specific destruction of the decoder context. This
 * function is called by the generic on2_codec_destroy() wrapper function,
 * so plugins implementing this interface may trust the input parameters
 * to be properly initialized.
 *
 * \param[in] ctx   Pointer to this instance's context
 * \retval #ON2_CODEC_OK
 *     The input stream was recognized and decoder initialized.
 * \retval #ON2_CODEC_MEM_ERROR
 *     Memory operation failed.
 */
typedef on2_codec_err_t (*on2_codec_destroy_fn_t)  (on2_codec_alg_priv_t *ctx);                                                                             

/*!\brief parse stream info function pointer prototype
 *
 * Performs high level parsing of the bitstream. This function is called by
 * the generic on2_codec_parse_stream() wrapper function, so plugins implementing
 * this interface may trust the input parameters to be properly initialized.
 *
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
typedef on2_codec_err_t (*on2_codec_peek_si_fn_t) (const uint8_t         *data,    
                                               unsigned int           data_sz,
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
typedef on2_codec_err_t (*on2_codec_get_si_fn_t) (on2_codec_alg_priv_t    *ctx,
                                              on2_codec_stream_info_t *si);

/*!\brief control function pointer prototype
 *
 * This function is used to exchange algorithm specific data with the decoder
 * instance. This can be used to implement features specific to a particular
 * algorithm. 
 *
 * This function is called by the generic on2_codec_control() wrapper
 * function, so plugins implementing this interface may trust the input
 * parameters to be properly initialized. However,  this interface does not
 * provide type safety for the exchanged data or assign meanings to the
 * control codes. Those details should be specified in the algorithm's
 * header file. In particular, the ctrl_id parameter is guaranteed to exist
 * in the algorithm's control mapping table, and the data paramter may be NULL.
 *
 *
 * \param[in]     ctx              Pointer to this instance's context
 * \param[in]     ctrl_id          Algorithm specific control identifier
 * \param[in,out] data             Data to exchange with algorithm instance. 
 *
 * \retval #ON2_CODEC_OK
 *     The internal state data was deserialized.
 */
typedef on2_codec_err_t (*on2_codec_control_fn_t)  (on2_codec_alg_priv_t  *ctx,
                                        	int                  ctrl_id,    
                                        	va_list              ap);

/*!\brief control function pointer mapping
 *
 * This structure stores the mapping between control identifiers and 
 * implementing functions. Each algorithm provides a list of these
 * mappings. This list is searched by the on2_codec_control() wrapper
 * function to determine which function to invoke. The special
 * value {0, NULL} is used to indicate end-of-list, and must be 
 * present. The special value {0, <non-null>} can be used as a catch-all
 * mapping. This implies that ctrl_id values chosen by the algorithm
 * \ref MUST be non-zero.
 */
typedef const struct {
    int                    ctrl_id;
    on2_codec_control_fn_t   fn;
} on2_codec_ctrl_fn_map_t;

/*!\brief decode data function pointer prototype
 *
 * Processes a buffer of coded data. If the processing results in a new
 * decoded frame becoming available, #ON2_CODEC_CB_PUT_SLICE and 
 * #ON2_CODEC_CB_PUT_FRAME events are generated as appropriate. This
 * function is called by the generic on2_codec_decode() wrapper function,
 * so plugins implementing this interface may trust the input parameters
 * to be properly initialized.
 *
 * \param[in] ctx          Pointer to this instance's context
 * \param[in] data         Pointer to this block of new coded data. If
 *                         NULL, a #ON2_CODEC_CB_PUT_FRAME event is posted
 *                         for the previously decoded frame.
 * \param[in] data_sz      Size of the coded data, in bytes.
 *
 * \return Returns #ON2_CODEC_OK if the coded data was processed completely
 *         and future pictures can be decoded without error. Otherwise,
 *         see the descriptions of the other error codes in ::on2_codec_err_t
 *         for recoverability capabilities.
 */
typedef on2_codec_err_t (*on2_codec_decode_fn_t)  (on2_codec_alg_priv_t  *ctx,
                			       uint8_t		   *data,     
                			       unsigned int	    data_sz,
                			       void		   *user_priv,
					       long		    deadline);  
                                                                            
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
 * \param[in out] iter     Iterator storage, initialized to NULL
 *
 * \return Returns a pointer to an image, if one is ready for display. Frames
 *         produced will always be in PTS (presentation time stamp) order.
 */
typedef on2_image_t* (*on2_codec_get_frame_fn_t) (on2_codec_alg_priv_t *ctx,
                                                  on2_codec_iter_t     *iter);
						

/*\brief eXternal Memory Allocation memory map get iterator
 *
 * Iterates over a list of the memory maps requested by the decoder. The
 * iterator storage should be initialized to NULL to start the iteration.
 * Iteration is complete when this function returns NULL.
 *
 * \param[in out] iter     Iterator storage, initialized to NULL
 *
 * \return Returns a pointer to an memory segment descriptor, or NULL to
 *         indicate end-of-list.
 */
typedef on2_codec_err_t (*on2_codec_get_mmap_fn_t) (const on2_codec_ctx_t      *ctx,
                                                    on2_codec_mmap_t 	       *mmap,
                                                    on2_codec_iter_t 	       *iter);
 

/*\brief eXternal Memory Allocation memory map set iterator
 *
 * Sets a memory descriptor inside the decoder instance.
 *
 * \param[in] ctx      Pointer to this instance's context
 * \param[in] mmap     Memory map to store.
 *
 * \retval #ON2_CODEC_OK
 *     The memory map was accepted and stored.
 * \retval #ON2_CODEC_MEM_ERROR
 *     The memory map was rejected.
 */
typedef on2_codec_err_t (*on2_codec_set_mmap_fn_t) (on2_codec_ctx_t         *ctx,
                                                    const on2_codec_mmap_t  *mmap);
 

typedef on2_codec_err_t (*on2_codec_encode_fn_t) (on2_codec_alg_priv_t  *ctx,
                                                  const on2_image_t     *img,
                                                  on2_codec_pts_t        pts,
                                                  unsigned long          duration,
                                                  on2_enc_frame_flags_t  flags,
                                                  unsigned long          deadline);
typedef on2_codec_cx_pkt_t* (*on2_codec_get_cx_data_fn_t) (on2_codec_alg_priv_t *ctx,
                                                           on2_codec_iter_t     *iter);
						
typedef on2_codec_err_t
(*on2_codec_enc_config_set_fn_t) (on2_codec_alg_priv_t       *ctx,
                                  const on2_codec_enc_cfg_t  *cfg);
typedef on2_fixed_buf_t *
(*on2_codec_get_global_headers_fn_t) (on2_codec_alg_priv_t   *ctx);

typedef on2_image_t *
(*on2_codec_get_preview_frame_fn_t) (on2_codec_alg_priv_t   *ctx);

/*!\brief usage configuration mapping
 *
 * This structure stores the mapping between usage identifiers and 
 * configuration structures. Each algorithm provides a list of these
 * mappings. This list is searched by the on2_codec_enc_config_default()
 * wrapper function to determine which config to return. The special value
 * {-1, {0}} is used to indicate end-of-list, and must be present. At least
 * one mapping must be present, in addition to the end-of-list.
 * 
 */
typedef const struct {
    int                 usage;
    on2_codec_enc_cfg_t cfg;
} on2_codec_enc_cfg_map_t;

#define NOT_IMPLEMENTED 0

/*!\brief Decoder algorithm interface interface
 *
 * All decoders \ref MUST expose a variable of this type.
 */
struct on2_codec_iface {
    const char               *name;        /**< Identification String  */
    int                       abi_version; /**< Implemented ABI version */
    on2_codec_caps_t          caps;	   /**< Decoder capabilities */ 
    on2_codec_init_fn_t       init;	   /**< \copydoc ::on2_codec_init_fn_t */
    on2_codec_destroy_fn_t    destroy;	   /**< \copydoc ::on2_codec_destroy_fn_t */
    on2_codec_ctrl_fn_map_t  *ctrl_maps;   /**< \copydoc ::on2_codec_ctrl_fn_map_t */
    on2_codec_get_mmap_fn_t   get_mmap;	   /**< \copydoc ::on2_codec_get_mmap_fn_t */
    on2_codec_set_mmap_fn_t   set_mmap;	   /**< \copydoc ::on2_codec_set_mmap_fn_t */
    struct {
        on2_codec_peek_si_fn_t    peek_si;     /**< \copydoc ::on2_codec_peek_si_fn_t */
        on2_codec_get_si_fn_t     get_si;      /**< \copydoc ::on2_codec_peek_si_fn_t */
        on2_codec_decode_fn_t     decode;	   /**< \copydoc ::on2_codec_decode_fn_t */
        on2_codec_get_frame_fn_t  get_frame;   /**< \copydoc ::on2_codec_get_frame_fn_t */
    } dec;
    struct {
        on2_codec_enc_cfg_map_t           *cfg_maps;      /**< \copydoc ::on2_codec_enc_cfg_map_t */
        on2_codec_encode_fn_t              encode;        /**< \copydoc ::on2_codec_encode_fn_t */
        on2_codec_get_cx_data_fn_t         get_cx_data;   /**< \copydoc ::on2_codec_get_cx_data_fn_t */
        on2_codec_enc_config_set_fn_t      cfg_set;       /**< \copydoc ::on2_codec_enc_config_set_fn_t */
        on2_codec_get_global_headers_fn_t  get_glob_hdrs; /**< \copydoc ::on2_codec_enc_config_set_fn_t */
        on2_codec_get_preview_frame_fn_t   get_preview;   /**< \copydoc ::on2_codec_get_preview_frame_fn_t */
    } enc;
};

/*!\brief Callback function pointer / user data pair storage */
typedef struct {
    union {
        on2_codec_put_frame_cb_fn_t    put_frame;
        on2_codec_put_slice_cb_fn_t    put_slice;
    };
    void                            *user_priv;
} on2_codec_priv_cb_pair_t;


/*!\brief Instance private storage
 *
 * This structure is allocated by the algorithm's init function. It can be
 * extended in one of two ways. First, a second, algorithm specific structure
 * can be allocated and the priv member pointed to it. Alternatively, this
 * structure can be made the first member of the algorithm specific structure,
 * and the pointer casted to the proper type.
 */
struct on2_codec_priv {
    unsigned int                    sz;
    on2_codec_iface_t              *iface;
    struct on2_codec_alg_priv      *alg_priv;
    const char                     *err_detail;
    unsigned int                    eval_counter;
    on2_codec_flags_t               init_flags;
    struct {
        on2_codec_priv_cb_pair_t    put_frame_cb;
        on2_codec_priv_cb_pair_t    put_slice_cb;
    } dec;
    struct {
        int                         tbd;
    } enc;
};

#undef ON2_CTRL_USE_TYPE
#define ON2_CTRL_USE_TYPE(id, typ) \
static typ id##__value(va_list args) {return va_arg(args, typ);}    

#define CAST(id, arg) id##__value(arg)


/* Internal Utility Functions
 *
 * The following functions are indended to be used inside algorithms as 
 * utilities for manipulating on2_codec_* data structures.
 */
struct on2_codec__pkt_list {
    unsigned int            cnt;
    unsigned int            max;    
    struct on2_codec_cx_pkt pkts[1];
};

#define on2_codec__pkt_list_decl(n)\
    union {struct on2_codec__pkt_list head;\
           struct {struct on2_codec__pkt_list head;\
                   struct on2_codec_cx_pkt    pkts[n];} alloc;}

#define on2_codec__pkt_list_init(m)\
    (m)->alloc.head.cnt = 0,\
    (m)->alloc.head.max = sizeof((m)->alloc.pkts) / sizeof((m)->alloc.pkts[0])

int
on2_codec__pkt_list_add(struct on2_codec__pkt_list *,
                        const struct on2_codec_cx_pkt *);

on2_codec_cx_pkt_t*
on2_codec__pkt_list_get(struct on2_codec__pkt_list *list,
                        on2_codec_iter_t           *iter);


#endif
