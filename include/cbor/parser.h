#ifndef CBOR_PARSER_H
#define CBOR_PARSER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

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

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_PARSER_H */
