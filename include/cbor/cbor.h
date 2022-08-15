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
	CBOR_ITEM_FLOAT,
	CBOR_ITEM_SIMPLE_VALUE,
} cbor_item_data_t;

typedef struct {
	cbor_item_data_t type;
	size_t offset;
	size_t size; /**< either of the length of value in bytes or the number
		       of items in case of container type */
} cbor_item_t;

typedef struct {
	uint8_t const *msg;
	size_t msgsize;
	size_t msgidx;
} cbor_reader_t;

typedef struct {
	uint8_t *buf;
	size_t bufsize;
	size_t bufidx;
} cbor_writer_t;

void cbor_reader_init(cbor_reader_t *reader, void const *msg, size_t msgsize);
void cbor_writer_init(cbor_writer_t *writer, void *buf, size_t bufsize);
size_t cbor_writer_len(cbor_writer_t const *writer);
uint8_t const *cbor_writer_get_encoded(cbor_writer_t const *writer);

cbor_item_data_t cbor_get_item_type(cbor_item_t const *item);
size_t cbor_get_item_size(cbor_item_t const *item);

uint8_t cbor_get_following_bytes(uint8_t additional_info);

size_t cbor_copy(uint8_t *dst, uint8_t const *src, size_t len);
size_t cbor_copy_be(uint8_t *dst, uint8_t const *src, size_t len);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_H */
