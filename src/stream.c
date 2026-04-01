/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/stream.h"
#include "cbor/ieee754.h"
#include <string.h>

enum stream_state {
	STREAM_STATE_IDLE    = 0,
	STREAM_STATE_LENGTH  = 1,
	STREAM_STATE_PAYLOAD = 2,
	STREAM_STATE_ERROR   = 3,
};

/* ---------- event helpers ---------- */

static void build_event(cbor_stream_decoder_t *d,
		cbor_stream_event_type_t type, cbor_stream_event_t *event)
{
	event->type = type;
	event->depth = d->depth;
	event->is_map_key = (d->depth > 0) &&
		(d->stack[d->depth - 1].type == CBOR_ITEM_MAP) &&
		d->stack[d->depth - 1].is_key;
	event->has_tag = d->has_pending_tag;
	event->tag     = d->pending_tag;
	d->has_pending_tag = false;
}

static cbor_error_t invoke_cb(cbor_stream_decoder_t *d,
		const cbor_stream_event_t *event, const cbor_stream_data_t *data)
{
	if (!d->callback(event, data, d->callback_arg)) {
		d->error = CBOR_ABORTED;
		d->state = STREAM_STATE_ERROR;
		return CBOR_ABORTED;
	}
	return CBOR_SUCCESS;
}

/* ---------- container helpers ---------- */

static cbor_error_t emit_container_end(cbor_stream_decoder_t *d)
{
	cbor_stream_event_t event;
	cbor_stream_event_type_t type;

	d->depth--;
	type = (d->stack[d->depth].type == CBOR_ITEM_ARRAY)
		? CBOR_STREAM_EVENT_ARRAY_END : CBOR_STREAM_EVENT_MAP_END;
	build_event(d, type, &event);
	return invoke_cb(d, &event, NULL);
}

static cbor_error_t after_item(cbor_stream_decoder_t *d)
{
	while (d->depth > 0) {
		cbor_stream_frame_t *f = &d->stack[d->depth - 1];

		if (f->type == CBOR_ITEM_MAP) {
			f->is_key = !f->is_key;
		}

		if (f->count < 0) {
			/* indefinite: wait for BREAK */
			break;
		}

		f->count--;
		if (f->count > 0) {
			break;
		}

		cbor_error_t err = emit_container_end(d);
		if (err != CBOR_SUCCESS) {
			return err;
		}
		/* continue to cascade up */
	}
	return CBOR_SUCCESS;
}

static cbor_error_t push_container(cbor_stream_decoder_t *d,
		cbor_item_data_t type, int64_t count)
{
	if (d->depth >= CBOR_RECURSION_MAX_LEVEL) {
		return CBOR_EXCESSIVE;
	}

	cbor_stream_event_t event;
	cbor_stream_data_t  data;
	cbor_stream_event_type_t evtype = (type == CBOR_ITEM_ARRAY)
		? CBOR_STREAM_EVENT_ARRAY_START : CBOR_STREAM_EVENT_MAP_START;

	build_event(d, evtype, &event);
	data.container.size = count;

	cbor_error_t err = invoke_cb(d, &event, &data);
	if (err != CBOR_SUCCESS) {
		return err;
	}

	cbor_stream_frame_t *f = &d->stack[d->depth];
	f->type   = type;
	f->count  = count;
	f->is_key = (type == CBOR_ITEM_MAP);
	d->depth++;

	if (count == 0) {
		err = emit_container_end(d);
		if (err != CBOR_SUCCESS) {
			return err;
		}
		return after_item(d);
	}

	return CBOR_SUCCESS;
}

/* ---------- length decoding ---------- */

static uint64_t decode_length_buf(const cbor_stream_decoder_t *d)
{
	uint64_t val = 0;
	for (uint8_t i = 0; i < d->following_bytes; i++) {
		val = (val << 8) | d->length_buf[i];
	}
	return val;
}

static uint64_t get_item_value(const cbor_stream_decoder_t *d)
{
	if (d->following_bytes == 0) {
		return d->additional_info;
	}
	return decode_length_buf(d);
}

/* ---------- float decoding ---------- */

static double decode_float_value(const cbor_stream_decoder_t *d)
{
	uint64_t raw = decode_length_buf(d);

	if (d->following_bytes == 2) {
		return ieee754_convert_half_to_double((uint16_t)raw);
	} else if (d->following_bytes == 4) {
		uint32_t u32 = (uint32_t)raw;
		float f;
		memcpy(&f, &u32, 4);
		return (double)f;
	} else {
		double dbl;
		memcpy(&dbl, &raw, 8);
		return dbl;
	}
}

/* ---------- item emitters ---------- */

static cbor_error_t emit_uint(cbor_stream_decoder_t *d)
{
	cbor_stream_event_t event;
	cbor_stream_data_t  data;

	build_event(d, CBOR_STREAM_EVENT_UINT, &event);
	data.uint = get_item_value(d);

	cbor_error_t err = invoke_cb(d, &event, &data);
	if (err != CBOR_SUCCESS) {
		return err;
	}
	return after_item(d);
}

static cbor_error_t emit_int(cbor_stream_decoder_t *d)
{
	cbor_stream_event_t event;
	cbor_stream_data_t  data;
	uint64_t raw = get_item_value(d);

	build_event(d, CBOR_STREAM_EVENT_INT, &event);
	data.sint = (int64_t)(~raw);

	cbor_error_t err = invoke_cb(d, &event, &data);
	if (err != CBOR_SUCCESS) {
		return err;
	}
	return after_item(d);
}

static cbor_error_t emit_str_chunk(cbor_stream_decoder_t *d,
		cbor_stream_event_type_t type,
		const uint8_t *ptr, size_t len, bool first, bool last)
{
	cbor_stream_event_t event;
	cbor_stream_data_t  data;

	build_event(d, type, &event);
	data.str.ptr   = ptr;
	data.str.len   = len;
	data.str.total = d->payload_total;
	data.str.first = first;
	data.str.last  = last;

	return invoke_cb(d, &event, &data);
}

static cbor_error_t emit_simple(cbor_stream_decoder_t *d, uint8_t val)
{
	cbor_stream_event_t event;
	cbor_stream_data_t  data = {0};
	cbor_error_t        err;

	switch (val) {
	case 20:
		build_event(d, CBOR_STREAM_EVENT_BOOL, &event);
		data.boolean = false;
		break;
	case 21:
		build_event(d, CBOR_STREAM_EVENT_BOOL, &event);
		data.boolean = true;
		break;
	case 22:
		build_event(d, CBOR_STREAM_EVENT_NULL, &event);
		break;
	case 23:
		build_event(d, CBOR_STREAM_EVENT_UNDEFINED, &event);
		break;
	default:
		build_event(d, CBOR_STREAM_EVENT_SIMPLE, &event);
		data.simple = val;
		break;
	}

	err = invoke_cb(d, &event, &data);
	if (err != CBOR_SUCCESS) {
		return err;
	}
	return after_item(d);
}

static cbor_error_t emit_float(cbor_stream_decoder_t *d)
{
	cbor_stream_event_t event;
	cbor_stream_data_t  data;

	build_event(d, CBOR_STREAM_EVENT_FLOAT, &event);
	data.flt = decode_float_value(d);

	cbor_error_t err = invoke_cb(d, &event, &data);
	if (err != CBOR_SUCCESS) {
		return err;
	}
	return after_item(d);
}

/* ---------- BREAK handling ---------- */

static cbor_error_t handle_break(cbor_stream_decoder_t *d)
{
	if (d->in_indef_str) {
		cbor_stream_event_type_t type = (d->indef_str_major == 2)
			? CBOR_STREAM_EVENT_BYTES : CBOR_STREAM_EVENT_TEXT;
		cbor_error_t err = emit_str_chunk(d, type, NULL, 0,
				d->payload_first_chunk, true);
		d->in_indef_str = false;
		if (err != CBOR_SUCCESS) {
			return err;
		}
		return after_item(d);
	}

	if (d->depth == 0 || d->stack[d->depth - 1].count >= 0) {
		return CBOR_ILLEGAL;
	}

	/* indefinite container terminated by BREAK */
	cbor_error_t err = emit_container_end(d);
	if (err != CBOR_SUCCESS) {
		return err;
	}
	return after_item(d);
}

/* ---------- process complete initial byte (no following bytes) ---------- */

static cbor_error_t process_immediate(cbor_stream_decoder_t *d)
{
	cbor_error_t err;

	switch (d->major_type) {
	case 0: /* unsigned integer */
		return emit_uint(d);

	case 1: /* negative integer */
		return emit_int(d);

	case 2: /* byte string, length in additional_info */
	case 3: /* text string, length in additional_info */ {
		cbor_stream_event_type_t stype = (d->major_type == 2)
			? CBOR_STREAM_EVENT_BYTES : CBOR_STREAM_EVENT_TEXT;
		uint64_t slen = (uint64_t)d->additional_info;

		if (!d->in_indef_str) {
			d->payload_total       = (int64_t)slen;
			d->payload_first_chunk = true;
		}

		if (slen == 0) {
			bool first = d->in_indef_str ? d->payload_first_chunk : true;
			bool last  = !d->in_indef_str;
			d->payload_first_chunk = false;
			err = emit_str_chunk(d, stype, NULL, 0, first, last);
			if (err != CBOR_SUCCESS) {
				return err;
			}
			if (!d->in_indef_str) {
				return after_item(d);
			}
			return CBOR_SUCCESS;
		}

		d->payload_remaining = (int64_t)slen;
		d->state = STREAM_STATE_PAYLOAD;
		return CBOR_SUCCESS;
	}

	case 4: /* definite array */
		return push_container(d, CBOR_ITEM_ARRAY,
				(int64_t)d->additional_info);

	case 5: /* definite map */
		return push_container(d, CBOR_ITEM_MAP,
				(int64_t)(d->additional_info * 2));

	case 6: /* tag, value in additional_info */
		if (d->has_pending_tag) {
			return CBOR_EXCESSIVE;
		}
		d->has_pending_tag = true;
		d->pending_tag     = (uint64_t)d->additional_info;
		return CBOR_SUCCESS;

	case 7: /* simple value (additional_info 0-23) */
		/* additional_info >= 24 has following_bytes > 0, handled elsewhere;
		 * additional_info == 31 (BREAK) is caught by handle_indefinite() */
		return emit_simple(d, d->additional_info);

	default:
		return CBOR_ILLEGAL;
	}
}

/* ---------- process complete length bytes ---------- */

static cbor_error_t process_string_length(cbor_stream_decoder_t *d)
{
	uint64_t len = decode_length_buf(d);
	cbor_stream_event_type_t type = (d->major_type == 2)
		? CBOR_STREAM_EVENT_BYTES : CBOR_STREAM_EVENT_TEXT;

	/* For indefinite-string sub-chunks keep payload_total as -1 */
	if (!d->in_indef_str) {
		d->payload_total = (int64_t)len;
	}

	if (len == 0) {
		bool first = d->in_indef_str ? d->payload_first_chunk : true;
		bool last  = !d->in_indef_str;
		d->payload_first_chunk = false;
		cbor_error_t err = emit_str_chunk(d, type, NULL, 0, first, last);
		if (err != CBOR_SUCCESS) {
			return err;
		}
		if (!d->in_indef_str) {
			return after_item(d);
		}
		return CBOR_SUCCESS;
	}

	d->payload_remaining   = (int64_t)len;
	d->payload_first_chunk = d->in_indef_str ? d->payload_first_chunk : true;
	d->state = STREAM_STATE_PAYLOAD;
	return CBOR_SUCCESS;
}

static cbor_error_t process_container_length(cbor_stream_decoder_t *d)
{
	uint64_t n = decode_length_buf(d);
	cbor_item_data_t type = (d->major_type == 4)
		? CBOR_ITEM_ARRAY : CBOR_ITEM_MAP;
	int64_t count = (type == CBOR_ITEM_MAP) ? (int64_t)(n * 2) : (int64_t)n;
	return push_container(d, type, count);
}

static cbor_error_t process_tag_length(cbor_stream_decoder_t *d)
{
	if (d->has_pending_tag) {
		return CBOR_EXCESSIVE;
	}
	d->has_pending_tag = true;
	d->pending_tag     = decode_length_buf(d);
	d->state = STREAM_STATE_IDLE;
	return CBOR_SUCCESS;
}

static cbor_error_t process_float_simple_length(cbor_stream_decoder_t *d)
{
	if (d->following_bytes == 1) {
		/* 1 following byte → simple value */
		return emit_simple(d, d->length_buf[0]);
	}
	return emit_float(d);
}

static cbor_error_t process_length_complete(cbor_stream_decoder_t *d)
{
	d->state = STREAM_STATE_IDLE;

	switch (d->major_type) {
	case 0: return emit_uint(d);
	case 1: return emit_int(d);
	case 2: /* fall-through */
	case 3: return process_string_length(d);
	case 4: /* fall-through */
	case 5: return process_container_length(d);
	case 6: return process_tag_length(d);
	case 7: return process_float_simple_length(d);
	default: return CBOR_ILLEGAL;
	}
}

/* ---------- indefinite-length starters ---------- */

static cbor_error_t handle_indefinite(cbor_stream_decoder_t *d)
{
	switch (d->major_type) {
	case 2: /* indefinite byte string */
	case 3: /* indefinite text string */
		if (d->in_indef_str) {
			return CBOR_ILLEGAL; /* no nesting of indef strings */
		}
		d->in_indef_str        = true;
		d->indef_str_major     = d->major_type;
		d->payload_first_chunk = true;
		d->payload_total       = -1;
		d->state = STREAM_STATE_IDLE;
		return CBOR_SUCCESS;

	case 4: /* indefinite array */
		return push_container(d, CBOR_ITEM_ARRAY, -1);

	case 5: /* indefinite map */
		return push_container(d, CBOR_ITEM_MAP, -1);

	case 7: /* BREAK */
		return handle_break(d);

	default:
		return CBOR_ILLEGAL;
	}
}

/* ---------- initial byte parsing ---------- */

static cbor_error_t process_initial_byte(cbor_stream_decoder_t *d, uint8_t b)
{
	d->major_type      = b >> 5;
	d->additional_info = b & 0x1fu;
	d->following_bytes = cbor_get_following_bytes(d->additional_info);

	if (d->following_bytes == (uint8_t)CBOR_RESERVED_VALUE) {
		return CBOR_ILLEGAL;
	}

	if (d->in_indef_str) {
		/* inside indefinite string: only BREAK or same-major sub-chunks */
		if (b == 0xff) {
			return handle_break(d);
		}
		if (d->major_type != d->indef_str_major) {
			return CBOR_ILLEGAL;
		}
		if (d->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
			return CBOR_ILLEGAL; /* no nested indefinite */
		}
	}

	if (d->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		return handle_indefinite(d);
	}

	if (d->following_bytes == 0) {
		return process_immediate(d);
	}

	/* need to read following_bytes bytes */
	d->following_bytes_read = 0;
	memset(d->length_buf, 0, sizeof(d->length_buf));
	d->state = STREAM_STATE_LENGTH;
	return CBOR_SUCCESS;
}

/* ---------- payload streaming ---------- */

static cbor_error_t consume_payload(cbor_stream_decoder_t *d,
		const uint8_t **p, size_t *remaining)
{
	size_t avail = (size_t)d->payload_remaining;
	if (avail > *remaining) {
		avail = *remaining;
	}

	cbor_stream_event_type_t type = (d->major_type == 2)
		? CBOR_STREAM_EVENT_BYTES : CBOR_STREAM_EVENT_TEXT;
	bool last = ((int64_t)avail == d->payload_remaining) && !d->in_indef_str;

	cbor_error_t err = emit_str_chunk(d, type, *p, avail,
			d->payload_first_chunk, last);
	d->payload_first_chunk = false;

	*p               += avail;
	*remaining       -= avail;
	d->payload_remaining -= (int64_t)avail;

	if (err != CBOR_SUCCESS) {
		return err;
	}

	if (d->payload_remaining > 0) {
		return CBOR_SUCCESS; /* still in PAYLOAD state */
	}

	d->state = STREAM_STATE_IDLE;

	if (!d->in_indef_str) {
		return after_item(d);
	}
	/* sub-chunk done; stay in in_indef_str mode */
	return CBOR_SUCCESS;
}

/* ---------- public API ---------- */

void cbor_stream_init(cbor_stream_decoder_t *decoder,
		cbor_stream_callback_t callback, void *arg)
{
	memset(decoder, 0, sizeof(*decoder));
	decoder->callback     = callback;
	decoder->callback_arg = arg;
	decoder->state        = STREAM_STATE_IDLE;
}

cbor_error_t cbor_stream_feed(cbor_stream_decoder_t *decoder,
		const void *data, size_t len)
{
	if (decoder->state == STREAM_STATE_ERROR) {
		return decoder->error;
	}

	const uint8_t *p         = (const uint8_t *)data;
	size_t         remaining  = len;
	cbor_error_t   err        = CBOR_SUCCESS;

	while (remaining > 0) {
		switch (decoder->state) {
		case STREAM_STATE_IDLE:
			err = process_initial_byte(decoder, *p);
			p++;
			remaining--;
			break;

		case STREAM_STATE_LENGTH:
			decoder->length_buf[decoder->following_bytes_read++] = *p;
			p++;
			remaining--;
			if (decoder->following_bytes_read == decoder->following_bytes) {
				err = process_length_complete(decoder);
			}
			break;

		case STREAM_STATE_PAYLOAD:
			err = consume_payload(decoder, &p, &remaining);
			break;

		case STREAM_STATE_ERROR:
			return decoder->error;
		default:
			return CBOR_ILLEGAL;
		}

		if (err != CBOR_SUCCESS) {
			decoder->error = err;
			decoder->state = STREAM_STATE_ERROR;
			return err;
		}
	}

	return CBOR_SUCCESS;
}

cbor_error_t cbor_stream_finish(cbor_stream_decoder_t *decoder)
{
	if (decoder->state == STREAM_STATE_ERROR) {
		return decoder->error;
	}

	if (decoder->state != STREAM_STATE_IDLE || decoder->depth > 0 ||
			decoder->in_indef_str ||
			decoder->following_bytes_read > 0) {
		return CBOR_NEED_MORE;
	}

	return CBOR_SUCCESS;
}

void cbor_stream_reset(cbor_stream_decoder_t *decoder)
{
	cbor_stream_callback_t cb  = decoder->callback;
	void                  *arg = decoder->callback_arg;
	memset(decoder, 0, sizeof(*decoder));
	decoder->callback     = cb;
	decoder->callback_arg = arg;
	decoder->state        = STREAM_STATE_IDLE;
}
