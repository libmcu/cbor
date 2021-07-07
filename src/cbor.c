#include "cbor/cbor.h"

#define INDEFINITE_VALUE			-1
#define RESERVED_VALUE				-2

#define ADDITIONAL_INFO_MASK			0x1f /* the low-order 5 bits */

#define get_major_type(data_item)		((data_item) >> 5)
#define get_additional_info(major_type)		\
	((major_type) & ADDITIONAL_INFO_MASK)

#if !defined(MIN)
#define MIN(a, b)				(((a) > (b))? (b) : (a))
#endif

struct decode_context {
	void *buf;
	size_t bufsize;
	size_t bufidx;

	const uint8_t *msg;
	size_t msgsize;
	size_t msgidx;

	uint8_t major_type;
	uint8_t additional_info;
	int following_bytes;
};

typedef cbor_error_t (*major_type_callback_t)(struct decode_context *ctx);

static cbor_error_t do_unsigned_integer(struct decode_context *ctx);
static cbor_error_t do_negative_integer(struct decode_context *ctx);
static cbor_error_t do_byte_string(struct decode_context *ctx);
static cbor_error_t do_text_string(struct decode_context *ctx);
static cbor_error_t do_array(struct decode_context *ctx);

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

static inline int get_following_bytes(uint8_t additional_info)
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

static cbor_error_t decode_cbor(struct decode_context *ctx, size_t maxitem)
{
	size_t item = 0;

	while (ctx->msgidx < ctx->msgsize && ctx->bufidx < ctx->bufsize) {
		ctx->major_type = get_major_type(ctx->msg[ctx->msgidx]);
		ctx->additional_info = get_additional_info(ctx->msg[ctx->msgidx]);
		ctx->following_bytes = get_following_bytes(ctx->additional_info);

		if (ctx->following_bytes == RESERVED_VALUE) {
			return CBOR_ILLEGAL;
		} else if (callbacks[ctx->major_type] == NULL) {
			return CBOR_ILLEGAL;
		}

		cbor_error_t err = callbacks[ctx->major_type](ctx);

		if (err != CBOR_SUCCESS) {
			return err;
		}

		item += 1;

		if (item >= maxitem && maxitem > 0) {
			break;
		}
	}

	return CBOR_SUCCESS;
}

static cbor_error_t do_unsigned_integer(struct decode_context *ctx)
{
	const uint8_t *msg = &ctx->msg[ctx->msgidx];
	uint8_t *buf = &((uint8_t *)ctx->buf)[ctx->bufidx];

	if (ctx->following_bytes == INDEFINITE_VALUE) {
		return CBOR_ILLEGAL;
	} else if (ctx->following_bytes == 0) {
		*buf = ctx->additional_info;
		ctx->bufidx++;
	}

	size_t n = MIN((size_t)ctx->following_bytes, ctx->bufsize - ctx->bufidx);

	for (size_t i = 0; i < n; i++) {
		buf[n - i - 1] = msg[i + 1];
	}

	ctx->bufidx += n;
	ctx->msgidx += n + 1;

	return CBOR_SUCCESS;
}

static cbor_error_t do_negative_integer(struct decode_context *ctx)
{
	size_t buf_start_index = ctx->bufidx;
	uint8_t *buf = &((uint8_t *)ctx->buf)[buf_start_index];

	cbor_error_t err = do_unsigned_integer(ctx);

	if (err != CBOR_SUCCESS) {
		return err;
	}

	uint64_t val = 0;
	size_t val_len = ctx->bufidx - buf_start_index;

	for (size_t i = 0; i < val_len; i++) {
		val = (val << 8) | buf[val_len - i - 1];
	}

	if ((int64_t)val > 0) {
		val = -val;
	}
	val--;

	/* The value becomes a positive value if the data type size of the
	 * variable is larger than the value size. So we set MSB first here to
	 * keep it negative. */
	for (size_t i = 0; i < MIN(ctx->bufsize - buf_start_index, 4u); i++) {
		buf[i] = 0xff;
	}
	for (size_t i = 0; i < val_len; i++) {
		buf[i] = ((uint8_t *)&val)[i];
	}

	/* TODO: return CBOR_INVALID when bufsize is smaller than value size */
	return CBOR_SUCCESS;
}

static cbor_error_t do_byte_string(struct decode_context *ctx)
{
	const uint8_t *msg = &ctx->msg[ctx->msgidx];
	uint8_t *buf = &((uint8_t *)ctx->buf)[ctx->bufidx];
	uint64_t len = 0;
	int offset = 0;

	if (ctx->following_bytes == INDEFINITE_VALUE) {
		/* TODO: implement the indefinite value */
	} else if (ctx->following_bytes == 0) {
		len = (size_t)ctx->additional_info;
	} else {
		while (offset < ctx->following_bytes) {
			((uint8_t *)&len)[ctx->following_bytes - offset - 1]
				= msg[offset + 1];
			offset++;
		}
	}

	len = MIN(len, ctx->bufsize - ctx->bufidx);
	for (uint64_t i = 0; i < len; i++) {
		buf[i] = msg[(uint64_t)offset + i + 1];
	}

	ctx->bufidx += len;
	ctx->msgidx += (size_t)offset + len + 1;

	return CBOR_SUCCESS;
}

static cbor_error_t do_text_string(struct decode_context *ctx)
{
	return do_byte_string(ctx);
}

static cbor_error_t do_array(struct decode_context *ctx)
{
	const uint8_t *msg = &ctx->msg[ctx->msgidx];
	uint64_t len = 0;
	int offset = 0;

	if (ctx->following_bytes == INDEFINITE_VALUE) {
		/* TODO: implement the indefinite value */
	} else if (ctx->following_bytes == 0) {
		len = (size_t)ctx->additional_info;
	} else {
		while (offset < ctx->following_bytes) {
			((uint8_t *)&len)[ctx->following_bytes - offset - 1]
				= msg[offset + 1];
			offset++;
		}
	}

	ctx->msgidx += (size_t)offset + 1;
	cbor_error_t err = decode_cbor(ctx, (size_t)len);

	if (err != CBOR_SUCCESS) {
		return err;
	}

	return CBOR_SUCCESS;
}

cbor_error_t cbor_decode(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize)
{
	return decode_cbor(&(struct decode_context){
			.buf = buf,
			.bufsize = bufsize,
			.bufidx = 0,
			.msg = msg,
			.msgsize = msgsize,
			.msgidx = 0, }, 0);
}

size_t cbor_compute_decoded_size(const uint8_t *msg, size_t msgsize)
{
	(void)msg;
	(void)msgsize;
	return 0;
}
