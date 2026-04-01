/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_PARSER_H
#define CBOR_PARSER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/base.h"

/**
 * Parse the encoded CBOR messages into items.
 *
 * @param[in,out] reader reader context for the actual encoded message
 * @param[in] msg CBOR encoded message
 * @param[in] msgsize the @p msg size in bytes
 * @param[out] nitems_parsed the number of items parsed gets stored if not null
 *
 * @return a code of @ref cbor_error_t
 */
cbor_error_t cbor_parse(cbor_reader_t *reader, void const *msg, size_t msgsize,
		size_t *nitems_parsed);

/**
 * Count CBOR items from encoded CBOR message without storing item metadata.
 *
 * @param[in] msg CBOR encoded message
 * @param[in] msgsize the @p msg size in bytes
 * @param[out] nitems_counted the number of items counted gets stored
 *             if not null. This value is written even when the return code
 *             is an error, and then indicates the number of items counted
 *             before the parser stopped.
 *
 * @return a code of @ref cbor_error_t. Unlike @ref cbor_parse(),
 *         well-formed indefinite-length items do not cause this function to
 *         return @ref CBOR_BREAK; they are reported as @ref CBOR_SUCCESS.
 */
cbor_error_t cbor_count_items(void const *msg, size_t msgsize,
		size_t *nitems_counted);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_PARSER_H */
