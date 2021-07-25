#include "cbor/decoder.h"
#include <stdbool.h>

#if !defined(MIN)
#define MIN(a, b)				(((a) > (b))? (b) : (a))
#endif

typedef cbor_error_t (*item_callback_t)(const cbor_item_t *item,
		const uint8_t *msg, uint8_t *buf, size_t bufsize);

static cbor_error_t decode_pass(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize);
static cbor_error_t decode_integer(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize);
static cbor_error_t decode_string(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize);
static cbor_error_t decode_float(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize);

static const item_callback_t callbacks[] = {
	decode_pass,		/* 0: CBOR_ITEM_UNKNOWN */
	decode_integer,		/* 1: CBOR_ITEM_INTEGER */
	decode_string,		/* 2: CBOR_ITEM_STRING */
	decode_pass,		/* 3: CBOR_ITEM_ARRAY */
	decode_pass,		/* 4: CBOR_ITEM_MAP */
	decode_float,		/* 5: CBOR_ITEM_FLOAT_AND_SIMPLE_VALUE */
};

static uint8_t get_simple_value(uint8_t val)
{
	switch (val) {
	case 20: /* false */
		return 0;
	case 21: /* true */
		return 1;
	case 22: /* null */
		return '\0';
	case 23: /* undefined */
	default:
		return val;
	}
}

static bool is_break(const cbor_item_t *item)
{
	return item->type == CBOR_ITEM_FLOAT_AND_SIMPLE_VALUE
		&& item->size == 0xff;
}

static cbor_error_t decode_unsigned_integer(const cbor_item_t *item,
		const uint8_t *msg, uint8_t *buf, size_t bufsize)
{
	uint8_t additional_info = get_cbor_additional_info(msg[item->offset]);
	uint8_t following_bytes = cbor_get_following_bytes(additional_info);

	if (following_bytes == 0) {
		buf[0] = additional_info;
	}

	cbor_copy(buf, &msg[item->offset + 1], following_bytes);

	(void)bufsize;
	return CBOR_SUCCESS;
}

static cbor_error_t decode_negative_integer(const cbor_item_t *item,
		const uint8_t *msg, uint8_t *buf, size_t bufsize)
{
	cbor_error_t err = decode_unsigned_integer(item, msg, buf, bufsize);

	if (err != CBOR_SUCCESS) {
		return err;
	}

	uint64_t val = 0;
	cbor_copy_be((uint8_t *)&val, buf, item->size);

	val = ~val;

	/* The value becomes a positive one if the data type size of the
	 * variable is larger than the value size. So we set MSB first here to
	 * keep it negative. */
	for (uint8_t i = 0; i < MIN(bufsize, 8u); i++) {
		buf[i] = 0xff;
	}

	cbor_copy_be(buf, (uint8_t *)&val, item->size);

	return CBOR_SUCCESS;
}

static cbor_error_t decode_integer(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize)
{
	switch (get_cbor_major_type(msg[item->offset])) {
	case 0: /* unsigned integer */
		return decode_unsigned_integer(item, msg, buf, bufsize);
	case 1: /* negative integer */
		return decode_negative_integer(item, msg, buf, bufsize);
	default:
		return CBOR_ILLEGAL;
	}
}

static cbor_error_t decode_string(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize)
{
	for (size_t i = 0; i < item->size; i++) {
		buf[i] = msg[item->offset + i];
	}

	(void)bufsize;
	return CBOR_SUCCESS;
}

static cbor_error_t decode_float(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize)
{
	uint8_t following_bytes = cbor_get_following_bytes(
			get_cbor_additional_info(msg[item->offset]));
	cbor_error_t err;

	if (following_bytes == 1) {
		following_bytes = 0;
	}

	err = decode_unsigned_integer(item, msg, buf, bufsize);

	if (following_bytes == 0) {
		buf[0] = get_simple_value(buf[0]);
	}

	return err;
}

static cbor_error_t decode_pass(const cbor_item_t *item, const uint8_t *msg,
		uint8_t *buf, size_t bufsize)
{
	(void)item;
	(void)msg;
	(void)buf;
	(void)bufsize;
	return CBOR_SUCCESS;
}

cbor_error_t cbor_decode(cbor_reader_t *reader, const cbor_item_t *item,
		void *buf, size_t bufsize)
{
	if (is_break(item)) {
		return CBOR_BREAK;
	}
	if (item->size > bufsize) {
		return CBOR_OVERRUN;
	}

	return callbacks[item->type](item, reader->msg, (uint8_t *)buf, bufsize);
}
