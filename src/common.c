#include "cbor/cbor.h"

#if !defined(assert)
#define assert(expr)
#endif

static size_t copy_le(uint8_t *dst, uint8_t const *src, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[len - i - 1] = src[i];
	}

	return len;
}

static size_t copy_be(uint8_t *dst, uint8_t const *src, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[i] = src[i];
	}

	return len;
}

size_t cbor_copy(uint8_t *dst, uint8_t const *src, size_t len)
{
#if defined(CBOR_BIG_ENDIAN)
	return copy_be(dst, src, len);
#else
	return copy_le(dst, src, len);
#endif
}

size_t cbor_copy_be(uint8_t *dst, uint8_t const *src, size_t len)
{
	return copy_be(dst, src, len);
}

uint8_t cbor_get_following_bytes(uint8_t additional_info)
{
	if (additional_info < 24) {
		return 0;
	} else if (additional_info == 31) {
		return (uint8_t)CBOR_INDEFINITE_VALUE;
	} else if (additional_info >= 28) {
		return (uint8_t)CBOR_RESERVED_VALUE;
	}

	return (uint8_t)(1u << (additional_info - 24));
}

cbor_item_data_t cbor_get_item_type(cbor_item_t const *item)
{
	return item->type;
}

size_t cbor_get_item_size(cbor_item_t const *item)
{
	return item->size;
}

void cbor_reader_init(cbor_reader_t *reader, void const *msg, size_t msgsize)
{
	assert(parser != NULL);
	assert(msg != NULL);

	reader->msg = (uint8_t const *)msg;
	reader->msgsize = msgsize;
	reader->msgidx = 0;
}

void cbor_writer_init(cbor_writer_t *writer, void *buf, size_t bufsize)
{
	assert(parser != NULL);
	assert(msg != NULL);

	writer->buf = (uint8_t *)buf;
	writer->bufsize = bufsize;
	writer->bufidx = 0;
}

size_t cbor_writer_len(cbor_writer_t const *writer)
{
	return writer->bufidx;
}
