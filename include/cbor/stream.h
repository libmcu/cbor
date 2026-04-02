/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_STREAM_H
#define CBOR_STREAM_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/base.h"

typedef char cbor_stream_depth_must_fit_uint8_t[
	(CBOR_RECURSION_MAX_LEVEL < 256) ? 1 : -1];

#if !defined(CBOR_STREAM_MAX_PENDING_TAGS)
/**
 * Maximum number of consecutive tag prefixes before a tagged item.
 * Attempting to open more returns CBOR_EXCESSIVE from cbor_stream_feed().
 * Can be overridden at compile time.
 */
#define CBOR_STREAM_MAX_PENDING_TAGS 4
#endif

typedef enum {
	CBOR_STREAM_EVENT_UINT,
	CBOR_STREAM_EVENT_INT,
	CBOR_STREAM_EVENT_BYTES,
	CBOR_STREAM_EVENT_TEXT,
	CBOR_STREAM_EVENT_ARRAY_START,
	CBOR_STREAM_EVENT_ARRAY_END,
	CBOR_STREAM_EVENT_MAP_START,
	CBOR_STREAM_EVENT_MAP_END,
	CBOR_STREAM_EVENT_FLOAT,
	CBOR_STREAM_EVENT_BOOL,
	CBOR_STREAM_EVENT_NULL,
	CBOR_STREAM_EVENT_UNDEFINED,
	CBOR_STREAM_EVENT_SIMPLE,
	/**
	 * A CBOR tag prefix.  Emitted immediately when the tag number is
	 * decoded, before the wrapped item's event(s).  Nested tags produce
	 * consecutive TAG events in outermost-to-innermost order.
	 *
	 * data->tag holds the tag number.
	 */
	CBOR_STREAM_EVENT_TAG,
} cbor_stream_event_type_t;

/**
 * Structural context delivered with every event.
 *
 * Intentionally compact — fits in one or two registers on most targets.
 * Does not carry tag information: tags arrive as separate TAG events that
 * immediately precede the wrapped item's events.
 */
typedef struct {
	cbor_stream_event_type_t type;
	uint8_t  depth;       /**< nesting depth: 0 = top level */
	bool     is_map_key;  /**< true when this item is a map key */
} cbor_stream_event_t;

/**
 * Value payload — discriminated by event->type.
 *
 * NULL for _END events (container close has no payload).
 */
typedef union {
	uint64_t uint;           /**< CBOR_STREAM_EVENT_UINT */
	int64_t  sint;           /**< CBOR_STREAM_EVENT_INT */

	struct {
		const uint8_t *ptr;   /**< direct pointer into caller's chunk buffer,
					 or NULL when len == 0 */
		size_t         len;   /**< bytes in this chunk; may be 0 for empty
					 strings and the final BREAK chunk of
					 indefinite-length strings */
		int64_t        total; /**< declared total length; -1 if indefinite,
					 including the final BREAK chunk */
		bool           first; /**< true for the first chunk; also true for
					 the only empty chunk of a definite
					 string */
		bool           last;  /**< true for the final chunk; for
					 indefinite-length strings this is the
					 zero-length BREAK chunk */
	} str;                    /**< CBOR_STREAM_EVENT_BYTES / _TEXT string data
				      chunk */

	struct {
		int64_t size; /**< declared item count; -1 if indefinite */
	} container;      /**< CBOR_STREAM_EVENT_ARRAY/MAP_START */

	double  flt;      /**< CBOR_STREAM_EVENT_FLOAT */
	bool    boolean;  /**< CBOR_STREAM_EVENT_BOOL */
	uint8_t simple;   /**< CBOR_STREAM_EVENT_SIMPLE */
	uint64_t tag;     /**< CBOR_STREAM_EVENT_TAG — tag number */
} cbor_stream_data_t;

/**
 * Streaming event callback.
 *
 * @param[in] event  structural context (type, depth, map position); never NULL
 * @param[in] data   decoded value; NULL for _END events
 * @param[in] arg    user-supplied context pointer
 *
 * @return true to continue decoding, false to abort (CBOR_ABORTED)
 */
typedef bool (*cbor_stream_callback_t)(const cbor_stream_event_t *event,
		const cbor_stream_data_t *data, void *arg);

typedef struct {
	cbor_item_data_t type;
	int64_t          count; /**< remaining items; -1 = indefinite */
	bool             is_key;
} cbor_stream_frame_t;

typedef struct {
	/* ---- state machine ---- */
	uint8_t state;                   /**< enum stream_state */
	uint8_t major_type;
	uint8_t additional_info;
	uint8_t following_bytes;         /**< length descriptor bytes expected */
	uint8_t following_bytes_read;    /**< length descriptor bytes received */
	uint8_t length_buf[8];           /**< accumulation buffer for length/value */

	int64_t payload_remaining;       /**< bytes left in current string payload */
	int64_t payload_total;           /**< declared total for current string */
	bool    payload_first_chunk;     /**< true until first chunk is emitted */

	bool    in_indef_str;            /**< inside an indefinite-length string */
	uint8_t indef_str_major;         /**< major type of the indef string (2 or 3) */

	/* ---- pending tag state ---- */
	uint8_t pending_tag_count;       /**< number of unresolved tag prefixes */

	/* ---- container stack ---- */
	cbor_stream_frame_t stack[CBOR_RECURSION_MAX_LEVEL];
	uint8_t             depth;

	/* ---- error state ---- */
	cbor_error_t error;

	/* ---- callback ---- */
	cbor_stream_callback_t callback;
	void                  *callback_arg;
} cbor_stream_decoder_t;

/**
 * Initialize a streaming CBOR decoder.
 *
 * The decoder accepts zero or more consecutive top-level CBOR items
 * (sequence semantics, per RFC 8742).  Callers that need "exactly one item"
 * semantics should return false from the callback after the first complete
 * top-level item (depth transitions back to 0).
 *
 * @param[in,out] decoder  decoder context allocated by caller (zeroed on init)
 * @param[in]     callback event callback; if NULL, subsequent feed() calls
 *                           return CBOR_INVALID
 * @param[in]     arg      opaque pointer forwarded to every callback
 */
void cbor_stream_init(cbor_stream_decoder_t *decoder,
		cbor_stream_callback_t callback, void *arg);

/**
 * Feed bytes into the decoder.
 *
 * May be called repeatedly with any chunk size >= 0 bytes.
 * A zero-length feed is treated as a no-op; @p data may be NULL in that case.
 * Partial items are held in decoder state and completed when more bytes arrive.
 *
 * After any non-CBOR_SUCCESS return produced while operating on a valid
 * decoder instance, the decoder enters a sticky error state; subsequent calls
 * return the same error immediately without processing bytes.  API-level
 * argument validation failures (for example NULL decoder/callback, or NULL
 * data with len > 0) return CBOR_INVALID but do not modify decoder state.
 * Call cbor_stream_reset() to clear a sticky error and reuse the decoder.
 *
 * @param[in,out] decoder decoder context initialized by cbor_stream_init()
 * @param[in]     data    pointer to bytes to consume (may be NULL if len == 0)
 * @param[in]     len     number of bytes in @p data
 *
 * @return CBOR_SUCCESS   — all bytes processed, no error
 *         CBOR_ILLEGAL   — malformed encoding (reserved additional-info); sticky
 *         CBOR_INVALID   — semantically invalid item, or invalid API arguments;
 *                          sticky only for the former
 *         CBOR_EXCESSIVE — nesting depth or tag nesting exceeded limit; sticky
 *         CBOR_ABORTED   — callback returned false; sticky
 */
cbor_error_t cbor_stream_feed(cbor_stream_decoder_t *decoder,
		const void *data, size_t len);

/**
 * Signal end of input and verify the decoder is in a clean idle state.
 *
 * Does not invoke the callback.  Safe to call even when the decoder was
 * initialized with a NULL callback.
 *
 * If the decoder is in a sticky error state (from a previous feed() call),
 * this function returns that same error code without further processing.
 *
 * @param[in,out] decoder decoder context
 *
 * @return CBOR_INVALID   — @p decoder is NULL
 *         CBOR_SUCCESS   — decoder is idle: all items complete and all
 *                          containers closed (zero or more top-level items
 *                          received)
 *         CBOR_NEED_MORE — stream ended mid-item or with unclosed containers
 *                          or pending tag prefix; more bytes needed
 *         <sticky error> — the last non-success error from feed()
 */
cbor_error_t cbor_stream_finish(cbor_stream_decoder_t *decoder);

/**
 * Reset decoder to initial state, preserving the callback.
 *
 * @param[in,out] decoder decoder context
 */
void cbor_stream_reset(cbor_stream_decoder_t *decoder);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_STREAM_H */
