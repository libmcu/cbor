#ifndef CBOR_DECODER_H
#define CBOR_DECODER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

cbor_error_t cbor_decode(const cbor_item_t *item, const void *msg,
		void *buf, size_t bufsize);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_DECODER_H */
