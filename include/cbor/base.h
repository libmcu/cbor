/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_BASE_H
#define CBOR_BASE_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(CBOR_RECURSION_MAX_LEVEL)
#define CBOR_RECURSION_MAX_LEVEL		8
#endif

#define CBOR_INDEFINITE_VALUE			(-1)
#define CBOR_RESERVED_VALUE			(-2)

#define CBOR_ADDITIONAL_INFO_MASK		0x1fu /* the low-order 5 bits */
#define get_cbor_major_type(data_item)		((data_item) >> 5)
#define get_cbor_additional_info(major_type)	\
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
	CBOR_ITEM_TAG, /**< tag number in @p size; wraps the following item */
} cbor_item_data_t;

/**
 * Type for a CBOR tag number (RFC 8949 §3.4).
 *
 * Currently backed by @c size_t. On 32-bit targets this limits the
 * representable range to [0, 2^32-1]; tag numbers above that cause
 * @ref cbor_parse to return @ref CBOR_INVALID.
 *
 * Encoding accepts the full @c uint64_t range via @ref cbor_encode_tag.
 */
typedef size_t cbor_tag_t;

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

	cbor_item_t *items;
	size_t itemidx;
	size_t maxitems;
} cbor_reader_t;

typedef struct {
	uint8_t *buf;
	size_t bufsize;
	size_t bufidx;
} cbor_writer_t;

/**
 * Initialize the reader for CBOR encoded messages.
 *
 * @param[in,out] reader reader context for the actual encoded message
 * @param[out] items a pointer to item buffers
 * @param[in] maxitems the maximum number of items to be stored in @p items
 */
void cbor_reader_init(cbor_reader_t *reader, cbor_item_t *items, size_t maxitems);

/**
 * Initialize the writer for CBOR encoding.
 *
 * @param[in,out] writer writer context
 * @param[out] buf output buffer for encoded bytes
 * @param[in] bufsize size of @p buf in bytes
 */
void cbor_writer_init(cbor_writer_t *writer, void *buf, size_t bufsize);

/**
 * Get the number of bytes encoded into the writer buffer.
 *
 * @param[in] writer writer context
 *
 * @return encoded byte length
 */
size_t cbor_writer_len(cbor_writer_t const *writer);

/**
 * Get the pointer to the encoded writer buffer.
 *
 * @param[in] writer writer context
 *
 * @return pointer to the encoded bytes
 */
uint8_t const *cbor_writer_get_encoded(cbor_writer_t const *writer);

/**
 * Get the parsed CBOR item type.
 *
 * @param[in] item parsed CBOR item
 *
 * @return item data type
 */
cbor_item_data_t cbor_get_item_type(cbor_item_t const *item);

/**
 * Get the tag number from a parsed CBOR tag item.
 *
 * Behaviour is undefined if @p item is not of type @ref CBOR_ITEM_TAG.
 *
 * @param[in] item parsed CBOR item of type @ref CBOR_ITEM_TAG
 *
 * @return tag number
 */
cbor_tag_t cbor_get_tag_number(cbor_item_t const *item);

/**
 * Get the parsed CBOR size field of an item.
 *
 * Semantics depend on the item type and encoding form:
 * - Integer / float / simple value: encoded payload size in bytes
 * - Byte/text string (definite-length): string length in bytes
 * - Array (definite-length): number of child items
 * - Map (definite-length): number of key-value pairs
 *
 * For indefinite-length string/array/map, this returns
 * `(size_t)CBOR_INDEFINITE_VALUE`, which means the size is unknown up front and
 * should not be treated as a child-count.
 *
 * @param[in] item parsed CBOR item
 *
 * @return parsed CBOR size field for @p item
 */
size_t cbor_get_item_size(cbor_item_t const *item);

/**
 * Convert additional-info value to the number of following bytes.
 *
 * @param[in] additional_info low-order 5-bit additional information
 *
 * @return number of following bytes, or sentinel values returned as
 *         `(uint8_t)CBOR_INDEFINITE_VALUE` / `(uint8_t)CBOR_RESERVED_VALUE`
 *         (currently `0xFF` / `0xFE`)
 */
uint8_t cbor_get_following_bytes(uint8_t additional_info);

/**
 * Copy bytes from CBOR payload into host-endian order.
 *
 * Uses the library endianness configuration (`CBOR_BIG_ENDIAN`).
 *
 * @param[out] dst destination buffer
 * @param[in] src source buffer
 * @param[in] len number of bytes to copy
 *
 * @return bytes copied
 */
size_t cbor_copy(uint8_t *dst, uint8_t const *src, size_t len);

/**
 * Copy bytes as big-endian order.
 *
 * @param[out] dst destination buffer
 * @param[in] src source buffer
 * @param[in] len number of bytes to copy
 *
 * @return bytes copied
 */
size_t cbor_copy_be(uint8_t *dst, uint8_t const *src, size_t len);

/**
 * Check if a parsed item is a CBOR BREAK token.
 *
 * @param[in] item parsed CBOR item
 *
 * @return true if @p item is BREAK token, otherwise false
 */
static inline bool cbor_item_is_break(cbor_item_t const *item)
{
	return item->type == CBOR_ITEM_FLOAT &&
		item->size == (size_t)(uint8_t)CBOR_INDEFINITE_VALUE;
}

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_BASE_H */
