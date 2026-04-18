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
 *
 * CBOR_KEY_INT and CBOR_KEY_IDX are intentionally distinct types:
 * CBOR_INT_SEG matches integer map keys; CBOR_IDX_SEG matches array positions.
 * They do not match each other even when the numeric value is equal.
 */
typedef enum {
	CBOR_KEY_STR, /**< map string key: byte-compared against encoded text */
	CBOR_KEY_INT, /**< map integer key */
	CBOR_KEY_IDX, /**< array index (0-based) */
	CBOR_KEY_ANY, /**< wildcard: matches any key type or index at this depth */
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

/** Matches a map string key (literal string). */
#define CBOR_STR_SEG(s) \
	{ CBOR_KEY_STR, { .str = { (s), sizeof(s) - 1 } } }
/** Matches a map integer key. */
#define CBOR_INT_SEG(n)		{ CBOR_KEY_INT, { .idx = (intmax_t)(n) } }
/** Matches an array element at a specific 0-based index. */
#define CBOR_IDX_SEG(n)		{ CBOR_KEY_IDX, { .idx = (intmax_t)(n) } }
/** Wildcard: matches any map value (regardless of its key type or value) or
 * any array element at this depth. Does NOT match map keys themselves. */
#define CBOR_ANY_SEG()		{ CBOR_KEY_ANY, { 0 } }

/*
 * CBOR_PATH(path_arr, fn) - declare a cbor_parser from a named path array.
 *
 * path_arr MUST be a named array variable, not a pointer.  sizeof() computes
 * the element count at compile time so depth stays in sync automatically.
 *
 * Maximum matchable depth is CBOR_RECURSION_MAX_LEVEL (default 8).
 * Use CBOR_PATH_DECL to catch depth violations at compile time.
 */
#define CBOR_PATH(path_arr, fn) \
	{ .path  = (path_arr), \
	  .depth = sizeof(path_arr) / sizeof((path_arr)[0]), \
	  .run   = (fn) }

/*
 * CBOR_PATH_DECL(var, path_arr, fn) - declare a cbor_parser with a
 * compile-time check that the path depth does not exceed
 * CBOR_RECURSION_MAX_LEVEL. Emits a static_assert error if the depth
 * limit is violated.
 */
#if defined(__cplusplus)
#define CBOR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define CBOR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#define CBOR_PATH_DECL(var, path_arr, fn) \
	CBOR_STATIC_ASSERT( \
		sizeof(path_arr) / sizeof((path_arr)[0]) \
			<= CBOR_RECURSION_MAX_LEVEL, \
		#var ": path depth exceeds CBOR_RECURSION_MAX_LEVEL"); \
	static const struct cbor_parser var = CBOR_PATH(path_arr, fn)

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
