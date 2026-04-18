/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_HELPER_H
#define CBOR_HELPER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Key type for a path segment.
 */
typedef enum {
	CBOR_KEY_STR, /**< map string key: byte-compared against encoded text */
	CBOR_KEY_INT, /**< map integer key */
	CBOR_KEY_IDX, /**< array index (0-based) */
} cbor_key_type_t;

typedef union {
	struct {
		const void *ptr;
		size_t len;
	} str;
	intmax_t idx;
} cbor_key_t;

struct cbor_path_segment {
	cbor_key_type_t type;
	cbor_key_t key;
};

struct cbor_parser {
	const struct cbor_path_segment *path;
	size_t depth;
	void (*run)(const cbor_reader_t *reader,
			const struct cbor_parser *parser,
			const cbor_item_t *item, void *arg);
};

#define CBOR_STR_SEG(s) \
	{ CBOR_KEY_STR, { .str = { (s), sizeof(s) - 1 } } }
#define CBOR_INT_SEG(n)		{ CBOR_KEY_INT, { .idx = (intmax_t)(n) } }
#define CBOR_IDX_SEG(n)		{ CBOR_KEY_IDX, { .idx = (intmax_t)(n) } }

/*
 * CBOR_PATH(path_arr, fn) - fill a cbor_parser from a named path array.
 *
 * path_arr MUST be a named array, not a pointer.  sizeof is used to compute
 * the element count at compile time so that depth cannot fall out of sync
 * with the actual array length.
 */
#define CBOR_PATH(path_arr, fn) \
	{ .path  = (path_arr), \
	  .depth = sizeof(path_arr) / sizeof((path_arr)[0]), \
	  .run   = (fn) }

bool cbor_unmarshal(cbor_reader_t *reader,
		const struct cbor_parser *parsers, size_t nr_parsers,
		const void *msg, size_t msglen, void *arg);

size_t cbor_iterate(const cbor_reader_t *reader,
		    const cbor_item_t *parent,
		    void (*callback_each)(const cbor_reader_t *reader,
			    const cbor_item_t *item, const cbor_item_t *parent,
			    void *arg),
		    void *arg);

const char *cbor_stringify_error(cbor_error_t err);
const char *cbor_stringify_item(cbor_item_t *item);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_HELPER_H */
