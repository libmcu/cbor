#ifndef CBOR_H
#define CBOR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if !defined(CBOR_RECURSION_DEPTH)
#define CBOR_RECURSION_MAX_LEVEL			4
#endif

#define CBOR_INDEFINITE_VALUE				(-1)
#define CBOR_RESERVED_VALUE				(-2)

#define CBOR_ADDITIONAL_INFO_MASK			0x1fu /* the low-order
								 5 bits */
#define get_cbor_major_type(data_item)			((data_item) >> 5)
#define get_cbor_additional_info(major_type)		\
	((major_type) & CBOR_ADDITIONAL_INFO_MASK)

typedef enum {
	CBOR_SUCCESS, /**< well-formed */
	CBOR_ILLEGAL, /**< not well-formed */
	CBOR_INVALID, /**< well-formed but invalid */
	CBOR_OVERRUN, /**< more items than given buffer space */
	CBOR_BREAK,
	CBOR_EXCESSIVE, /**< recursion more than @ref CBOR_RECURSION_MAX_LEVEL */
} cbor_error_t;

typedef enum {
	CBOR_ITEM_UNKNOWN,
	CBOR_ITEM_INTEGER, /**< unsigned integer and negative integer */
	CBOR_ITEM_STRING, /**< byte string and text string */
	CBOR_ITEM_ARRAY,
	CBOR_ITEM_MAP,
	CBOR_ITEM_FLOAT_AND_SIMPLE_VALUE,
} cbor_item_data_t;

typedef struct {
	cbor_item_data_t type;
	size_t offset;
	size_t size; /**< either of the length of value in bytes or the number
		       of items in case of container type */
} cbor_item_t;

typedef struct {
	const uint8_t *msg;
	size_t msgsize;
	size_t msgidx;
} cbor_reader_t;

typedef struct {
	uint8_t *buf;
	size_t bufsize;
	size_t bufidx;
} cbor_writer_t;

void cbor_reader_init(cbor_reader_t *reader, const void *msg, size_t msgsize);
void cbor_writer_init(cbor_writer_t *writer, void *buf, size_t bufsize);

static inline void cbor_copy_le(uint8_t *dst, const uint8_t *src, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[len - i - 1] = src[i];
	}
}

static inline void cbor_copy_be(uint8_t *dst, const uint8_t *src, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[i] = src[i];
	}
}

static inline void cbor_copy(uint8_t *dst, const uint8_t *src, size_t len)
{
#if defined(CBOR_BIG_ENDIAN)
	cbor_copy_be(dst, src, len);
#else
	cbor_copy_le(dst, src, len);
#endif
}

static inline uint8_t cbor_get_following_bytes(uint8_t additional_info)
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

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_H */
