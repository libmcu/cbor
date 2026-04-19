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

#if !defined(CBOR_MAX_WILDCARD_PARSERS)
#define CBOR_MAX_WILDCARD_PARSERS	8
#endif
#if CBOR_MAX_WILDCARD_PARSERS < 1
#error "CBOR_MAX_WILDCARD_PARSERS must be >= 1"
#endif

/**
 * Key type for a path segment.
 *
 * CBOR_KEY_INT and CBOR_KEY_IDX are intentionally distinct types:
 * CBOR_INT_SEG matches integer map keys; CBOR_IDX_SEG matches array positions.
 * They do not match each other even when the numeric value is equal.
 */
typedef enum {
	CBOR_KEY_STR, /**< map string key: byte-compared against CBOR string
	               *   keys (both text and byte strings share the same
	               *   internal representation, so byte-string keys also
	               *   match) */
	CBOR_KEY_INT, /**< map integer key */
	CBOR_KEY_IDX, /**< array index (0-based) */
	CBOR_KEY_ANY, /**< wildcard: matches any map string/integer key or array
	               *   index at this depth; other map key types (e.g. float,
	               *   nested array/map keys) are skipped by the dispatcher
	               *   and will not match. See also CBOR_ANY_SEG(). */
} cbor_key_type_t;

struct cbor_path_segment {
	cbor_key_type_t type;
	intptr_t val; /* STR: (intptr_t)ptr; INT/IDX: signed index */
	size_t len; /* STR: byte length; otherwise 0 */
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
	{ CBOR_KEY_STR, (intptr_t)(const void *)(s), sizeof(s) - 1 }
/** Matches a map integer key. */
#define CBOR_INT_SEG(n)		{ CBOR_KEY_INT, (intptr_t)(n), 0 }
/** Matches an array element at a specific 0-based index. */
#define CBOR_IDX_SEG(n)		{ CBOR_KEY_IDX, (intptr_t)(n), 0 }
/** Wildcard: matches any map value whose key is a string or integer, or any
 * array element at this depth. Map values under other key types (e.g. float,
 * array keys) are skipped by the dispatcher and will not match. Does NOT
 * match map keys themselves. */
#define CBOR_ANY_SEG()		{ CBOR_KEY_ANY, 0, 0 }

/* CBOR_PATH(path_arr, fn) - declare a cbor_parser from a named path array.
 *
 * path_arr MUST be a named array variable, not a pointer. sizeof() computes
 * the element count at compile time so depth stays in sync automatically.
 *
 * Maximum matchable depth is CBOR_RECURSION_MAX_LEVEL (default 8).
 * Use CBOR_PATH_DECL to catch depth violations at compile time. */
#define CBOR_PATH(path_arr, fn) { \
	.path = (path_arr), \
	.depth = sizeof(path_arr) / sizeof((path_arr)[0]), \
	.run = (fn) \
}

/* CBOR_PATH_INLINE(fn, seg, ...) - declare a cbor_parser with inline path
 * segments, without a named path array variable.
 *
 * In C (C99/C11): uses compound literals; depth is computed at compile time
 * via sizeof. At file scope, the compound-literal path has static storage
 * duration and is safe. At block scope, the path has the lifetime of the
 * enclosing block; do not use this for a function-scope static parser or
 * when returning or storing the parser (or its path pointer) beyond the
 * block. Use CBOR_PATH_INLINE_DECL in those cases.
 *
 * In C++11 and later: uses an immediately-invoked lambda with a static
 * segment array. Includes a compile-time depth check against
 * CBOR_RECURSION_MAX_LEVEL.
 *
 * Segments are passed as CBOR_STR_SEG / CBOR_INT_SEG / CBOR_IDX_SEG /
 * CBOR_ANY_SEG() initializers.
 *
 * Example (depth 1):
 *   CBOR_PATH_INLINE(do_reboot, CBOR_STR_SEG("reboot"))
 * Example (depth 2):
 *   CBOR_PATH_INLINE(NULL, CBOR_STR_SEG("srv"), CBOR_STR_SEG("url"))
 */
#if defined(__cplusplus)
#define CBOR_PATH_INLINE(fn, ...) \
	([](void (*cbor_run_)(const cbor_reader_t *, \
			const struct cbor_parser *, \
			const cbor_item_t *, void *)) -> struct cbor_parser { \
		static const struct cbor_path_segment cbor_path_segs_[] = { __VA_ARGS__ }; \
		static_assert( \
			sizeof(cbor_path_segs_) / sizeof(cbor_path_segs_[0]) \
				<= CBOR_RECURSION_MAX_LEVEL, \
			"CBOR_PATH_INLINE: path depth exceeds CBOR_RECURSION_MAX_LEVEL"); \
		struct cbor_parser cbor_p_ = { \
			cbor_path_segs_, \
			sizeof(cbor_path_segs_) / sizeof(cbor_path_segs_[0]), \
			cbor_run_ \
		}; \
		return cbor_p_; \
	}(fn))
#else
#define CBOR_PATH_INLINE(fn, ...) { \
	.path = (const struct cbor_path_segment[]){ __VA_ARGS__ }, \
	.depth = sizeof((const struct cbor_path_segment[]){ __VA_ARGS__ }) / \
		sizeof(struct cbor_path_segment), \
	.run = (fn) \
}
#endif

/* CBOR_PATH_DECL(var, path_arr, fn) - declare a cbor_parser with a
 * compile-time check that the path depth does not exceed
 * CBOR_RECURSION_MAX_LEVEL. Emits a static_assert error if the depth
 * limit is violated. */
#define CBOR_CONCAT_INNER(a, b)	a##b
#define CBOR_CONCAT(a, b)	CBOR_CONCAT_INNER(a, b)
#if defined(__cplusplus)
#define CBOR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CBOR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
/* C99 fallback: negative-size array trick */
#define CBOR_STATIC_ASSERT(cond, msg) \
	typedef char CBOR_CONCAT(cbor_static_assert_at_line_, __LINE__)[(cond) ? 1 : -1]
#endif

#define CBOR_PATH_DECL(var, path_arr, fn) \
	CBOR_STATIC_ASSERT( \
		sizeof(path_arr) / sizeof((path_arr)[0]) \
			<= CBOR_RECURSION_MAX_LEVEL, \
		#var ": path depth exceeds CBOR_RECURSION_MAX_LEVEL"); \
	static const struct cbor_parser var = CBOR_PATH(path_arr, fn)

/* CBOR_PATH_INLINE_DECL(var, fn, seg, ...) - declare a cbor_parser from
 * inline path segments with static storage for the backing path array and a
 * compile-time depth check. Use this for file-scope parsers and block-scope
 * static parsers in C/C++. */
#define CBOR_PATH_INLINE_DECL(var, fn, ...) \
	static const struct cbor_path_segment CBOR_CONCAT(var, _path)[] = { __VA_ARGS__ }; \
	CBOR_PATH_DECL(var, CBOR_CONCAT(var, _path), fn)

/**
 * @brief Unmarshal CBOR message using parser table.
 *
 * Parser callbacks are invoked when the declared path matches a parsed item.
 * Matching items may be scalar values, indefinite-length strings, or MAP/ARRAY
 * containers. If the root item is a MAP or ARRAY, a depth-0 parser can match
 * and receive that root container.
 *
 * @param[in,out] reader     CBOR reader context.
 * @param[in]     parsers    Array of parser definitions.
 * @param[in]     nr_parsers Number of parsers in the array.
 * @param[in]     msg        CBOR-encoded message buffer.
 * @param[in]     msglen     Length of the message buffer.
 * @param[in,out] arg        User argument passed to callbacks.
 * @return true on success, false on error.
 */
bool cbor_unmarshal(cbor_reader_t *reader,
		const struct cbor_parser *parsers, size_t nr_parsers,
		const void *msg, size_t msglen, void *arg);

/**
 * @brief Dispatch CBOR items in a container using parser table.
 *
 * Allows dispatching the children of a specific container (map or array), or
 * dispatching from the root if container is NULL. When a non-NULL container is
 * given, the container item itself is not dispatched.
 *
 * @param[in]     reader     CBOR reader context.
 * @param[in]     container  Container item (map/array) from reader->items, or
 *                           NULL for root dispatch.
 * @param[in]     parsers    Array of parser definitions.
 * @param[in]     nr_parsers Number of parsers in the array.
 * @param[in,out] arg        User argument passed to callbacks.
 * @return true on success, false on error.
 */
bool cbor_dispatch(const cbor_reader_t *reader,
		const cbor_item_t *container,
		const struct cbor_parser *parsers, size_t nr_parsers,
		void *arg);

/**
 * @brief Iterate over children of a CBOR container and invoke callback.
 *
 * @param[in]     reader        CBOR reader context.
 * @param[in]     parent        Parent container item.
 * @param[in]     callback_each Callback invoked for each child item.
 * @param[in,out] arg           User argument passed to callback.
 * @return Number of items iterated.
 */
size_t cbor_iterate(const cbor_reader_t *reader,
		    const cbor_item_t *parent,
		    void (*callback_each)(const cbor_reader_t *reader,
			    const cbor_item_t *item, const cbor_item_t *parent,
			    void *arg),
		    void *arg);

/**
 * @brief Convert CBOR error code to string.
 *
 * @param[in] err CBOR error code.
 * @return String representation of error.
 */
const char *cbor_stringify_error(cbor_error_t err);

/**
 * @brief Convert CBOR item to string for debugging.
 *
 * @param[in] item CBOR item.
 * @return String representation of item.
 */
const char *cbor_stringify_item(const cbor_item_t *item);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_HELPER_H */
