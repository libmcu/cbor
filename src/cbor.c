#include "cbor/cbor.h"

#define INDEFINITE_VALUE			-1
#define RESERVED_VALUE				-2

#define ADDITIONAL_INFO_MASK			0x1f /* the low-order 5 bits */

#if !defined(MIN)
#define MIN(a, b)				(((a) > (b))? (b) : (a))
#endif
#define get_additional_info(major_type)		\
	((major_type) & ADDITIONAL_INFO_MASK)

typedef size_t (*major_type_callback_t)(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);

static size_t do_unsigned_integer(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
static size_t do_negative_integer(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
static size_t do_byte_string(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
static size_t do_text_string(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
static size_t do_array(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);

/* 16 callbacks for 3-bit major type */
static const major_type_callback_t callbacks[16] = {
	do_unsigned_integer,	/* 0 */
	do_negative_integer,	/* 1 */
	do_byte_string,		/* 2 */
	do_text_string,		/* 3 */
	do_array,		/* 4 */
	//do_map,			/* 5 */
	//do_tag,			/* 6 */
	//do_float_and_other,	/* 7 */
};

static cbor_error_t decode_cbor(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	uint8_t *out = (uint8_t *)buf;
	size_t index = 0;
	size_t outdex = 0;

	while (index < msgsize && outdex < bufsize) {
		uint8_t major_type = msg[index] >> 5;

		if (callbacks[major_type] == NULL) {
			return CBOR_ILLEGAL;
		}

		size_t l = callbacks[major_type](&out[outdex], bufsize - outdex,
				&msg[index], msgsize - index);

		if (l == 0) {
			return CBOR_ILLEGAL;
		}

		index += l;
		outdex += l;
	}

	return CBOR_SUCCESS;
}

static int get_following_bytes(uint8_t additional_info)
{
	if (additional_info < 24) {
		return 0;
	} else if (additional_info == 31) {
		return INDEFINITE_VALUE;
	} else if (additional_info >= 28) {
		return RESERVED_VALUE;
	}

	return 1 << (additional_info - 24);
}

static size_t do_unsigned_integer(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	uint8_t *p = (uint8_t *)buf;
	uint8_t additional_info = get_additional_info(msg[0]);
	int bytes_to_read = get_following_bytes(additional_info);

	if (bytes_to_read < 0) {
		return 0;
	} else if (bytes_to_read == 0) {
		*p = additional_info;
		return 1;
	}

	for (int i = 0; i < bytes_to_read; i++) {
		p[bytes_to_read - i - 1] = msg[i + 1];
	}

	return (size_t)bytes_to_read + 1;
}

static size_t do_negative_integer(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	size_t len = do_unsigned_integer(buf, bufsize, msg, msgsize);
	size_t val_len = (len > 1)? len - 1 : len;
	uint64_t val = 0;

	for (size_t i = 0; i < val_len; i++) {
		val = (val << 8) | ((uint8_t *)buf)[val_len - i - 1];
	}

	if ((int64_t)val > 0) {
		val = -val;
	}
	val--;

	/* The value becomes a positive value if the data type size of the
	 * variable is larger than the value size. So it sets MSB first here to
	 * keep it negative. */
	for (size_t i = 0; i < MIN(bufsize, 4u); i++) {
		((uint8_t *)buf)[i] = 0xff;
	}
	for (size_t i = 0; i < val_len; i++) {
		((uint8_t *)buf)[i] = ((uint8_t *)&val)[i];
	}

	return len;
}

static size_t do_byte_string(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	uint8_t *p = (uint8_t *)buf;
	uint8_t additional_info = get_additional_info(msg[0]);
	int bytes_to_read = get_following_bytes(additional_info);
	uint64_t len = 0;
	int offset = 0;

	if (bytes_to_read == RESERVED_VALUE) {
		return 0;
	} else if (bytes_to_read == INDEFINITE_VALUE) {
		// TODO: implement the indefinite value
	} else if (bytes_to_read == 0) {
		len = (size_t)additional_info;
	} else {
		while (offset < bytes_to_read) {
			((uint8_t *)&len)[bytes_to_read - offset - 1]
				= msg[offset + 1];
			offset++;
		}
	}

	for (uint64_t i = 0; i < len; i++) {
		*p++ = msg[(uint64_t)offset + i + 1];
	}

	return (size_t)offset + len + 1;
}

static size_t do_text_string(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	return do_byte_string(buf, bufsize, msg, msgsize);
}

static size_t do_array(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	uint8_t additional_info = get_additional_info(msg[0]);
	int bytes_to_read = get_following_bytes(additional_info);
	uint64_t len = 0;
	int offset = 0;

	if (bytes_to_read == RESERVED_VALUE) {
		return 0;
	} else if (bytes_to_read == INDEFINITE_VALUE) {
		// TODO: implement the indefinite value
	} else if (bytes_to_read == 0) {
		len = (size_t)additional_info;
	} else {
		while (offset < bytes_to_read) {
			((uint8_t *)&len)[bytes_to_read - offset - 1]
				= msg[offset + 1];
			offset++;
		}
	}

	if (decode_cbor(buf, bufsize, &msg[offset+1], (size_t)len)
			!= CBOR_SUCCESS) {
		return 0;
	}

	return (size_t)offset + len + 1;
}

cbor_error_t cbor_decode(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	return decode_cbor(buf, bufsize, msg, msgsize);
}

#if 0
size_t cbor_compute_decoded_size(const uint8_t *msg, size_t msgsize)
{
	(void)msg;
	(void)msgsize;
	return 0;
}
#endif
