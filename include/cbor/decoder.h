#ifndef CBOR_DECODER_H
#define CBOR_DECODER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

/**
 * Decode a CBOR data item
 *
 * @param[in] reader reader context for the actual encoded message
 * @param[in] item meta data about the item to be decoded
 * @param[out] buf the buffer where decoded value to be written in
 * @param[in] bufsize the buffer size
 *
 * @return a code of @ref cbor_error_t
 */
cbor_error_t cbor_decode(cbor_reader_t *reader, const cbor_item_t *item,
		void *buf, size_t bufsize);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_DECODER_H */
