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
		const uint8_t *data, size_t datasize);
cbor_error_t cbor_encode_byte_string_indefinite(cbor_writer_t *writer);
cbor_error_t cbor_encode_text_string(cbor_writer_t *writer,
		const char *text, size_t textsize);
cbor_error_t cbor_encode_text_string_indefinite(cbor_writer_t *writer);
cbor_error_t cbor_encode_array(cbor_writer_t *writer, size_t length);
cbor_error_t cbor_encode_array_indefinite(cbor_writer_t *writer);
cbor_error_t cbor_encode_map(cbor_writer_t *writer, size_t length);
cbor_error_t cbor_encode_map_indefinite(cbor_writer_t *writer);
cbor_error_t cbor_encode_break(cbor_writer_t *writer);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_ENCODER_H */
