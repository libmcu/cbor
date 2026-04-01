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
} cbor_stream_event_type_t;

typedef struct {
	cbor_stream_event_type_t type;
	uint8_t  depth;
	bool     is_map_key;
	bool     has_tag;
	uint64_t tag;
} cbor_stream_event_t;

typedef union {
	uint64_t uint;
	int64_t  sint;
	struct {
		const uint8_t *ptr;
		size_t         len;
		int64_t        total; /**< -1 if indefinite */
		bool           first;
		bool           last;
	} str;
	struct {
		int64_t size; /**< -1 if indefinite */
	} container;
	double  flt;
	bool    boolean;
	uint8_t simple;
} cbor_stream_data_t;

/**
 * Streaming event callback.
 *
 * @param[in] event structural context (type, depth, map position, tag)
 * @param[in] data  decoded value; NULL for _END events
 * @param[in] arg   user-supplied context pointer
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
	uint8_t state;
	uint8_t major_type;
	uint8_t additional_info;
	uint8_t following_bytes;
	uint8_t following_bytes_read;
	uint8_t length_buf[8];

	int64_t payload_remaining;
	int64_t payload_total;
	bool    payload_first_chunk;

	bool    in_indef_str;
	uint8_t indef_str_major;

	bool     has_pending_tag;
	uint64_t pending_tag;

	cbor_stream_frame_t stack[CBOR_RECURSION_MAX_LEVEL];
	uint8_t             depth;

	cbor_error_t           error;
	cbor_stream_callback_t callback;
	void                  *callback_arg;
} cbor_stream_decoder_t;

/**
 * Initialize a streaming CBOR decoder.
 *
 * @param[in,out] decoder  decoder context allocated by caller
 * @param[in]     callback event callback; if NULL, subsequent feed/finish
 *                           calls return CBOR_INVALID
 * @param[in]     arg      opaque pointer forwarded to every callback
 */
void cbor_stream_init(cbor_stream_decoder_t *decoder,
		cbor_stream_callback_t callback, void *arg);

/**
 * Feed bytes into the decoder.
 *
 * May be called repeatedly with any chunk size >=0 bytes.
 * A zero-length feed is treated as a no-op and @p data may be NULL.
 * After any non-CBOR_SUCCESS return the decoder enters a sticky error state;
 * subsequent calls return the same error immediately.
 *
 * @param[in,out] decoder decoder context
 * @param[in]     data    pointer to bytes to consume
 * @param[in]     len     number of bytes in @p data
 *
 * @return CBOR_SUCCESS, CBOR_ILLEGAL, CBOR_INVALID, CBOR_EXCESSIVE, or
 *         CBOR_ABORTED
 */
cbor_error_t cbor_stream_feed(cbor_stream_decoder_t *decoder,
		const void *data, size_t len);

/**
 * Signal end of input and verify the decoder is in a clean idle state.
 *
 * After any non-CBOR_SUCCESS return from cbor_stream_feed() the decoder
 * enters a sticky error state; in that case this function returns the same
 * error code without further processing.
 *
 * @param[in,out] decoder decoder context
 *
 * @return CBOR_SUCCESS if all items were complete and the decoder is idle;
 *         CBOR_NEED_MORE if the end of input was reached but additional
 *         bytes are required to complete the last item; CBOR_INVALID if
 *         @p decoder is NULL or the decoder has no valid callback; or the
 *         last non-success error previously returned by cbor_stream_feed()
 *         if the decoder is in a sticky error state
 */
cbor_error_t cbor_stream_finish(cbor_stream_decoder_t *decoder);

/**
 * Reset the decoder to its initial state so it can be reused.
 *
 * @param[in,out] decoder decoder context
 */
void cbor_stream_reset(cbor_stream_decoder_t *decoder);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_STREAM_H */
