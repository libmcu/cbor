/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_ENCODER_H
#define CBOR_ENCODER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"
#include <stdbool.h>

cbor_error_t cbor_encode_unsigned_integer(cbor_writer_t *writer, uint64_t value);
cbor_error_t cbor_encode_negative_integer(cbor_writer_t *writer, int64_t value);

cbor_error_t cbor_encode_byte_string(cbor_writer_t *writer,
		uint8_t const *data, size_t datasize);
cbor_error_t cbor_encode_byte_string_indefinite(cbor_writer_t *writer);

cbor_error_t cbor_encode_text_string(cbor_writer_t *writer, char const *text);
cbor_error_t cbor_encode_text_string_indefinite(cbor_writer_t *writer);

cbor_error_t cbor_encode_array(cbor_writer_t *writer, size_t length);
cbor_error_t cbor_encode_array_indefinite(cbor_writer_t *writer);

cbor_error_t cbor_encode_map(cbor_writer_t *writer, size_t length);
cbor_error_t cbor_encode_map_indefinite(cbor_writer_t *writer);

cbor_error_t cbor_encode_break(cbor_writer_t *writer);

cbor_error_t cbor_encode_simple(cbor_writer_t *writer, uint8_t value);
cbor_error_t cbor_encode_bool(cbor_writer_t *writer, bool value);
cbor_error_t cbor_encode_null(cbor_writer_t *writer);
cbor_error_t cbor_encode_undefined(cbor_writer_t *writer);

cbor_error_t cbor_encode_float(cbor_writer_t *writer, float value);
cbor_error_t cbor_encode_double(cbor_writer_t *writer, double value);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_ENCODER_H */
