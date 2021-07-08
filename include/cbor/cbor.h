#ifndef LIBCBOR_H
#define LIBCBOR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
	CBOR_SUCCESS, /* well-formed */
	CBOR_ILLEGAL, /* not well-formed */
	CBOR_INVALID, /* well-formed but invalid */
	CBOR_OVERRUN, /* more bytes than data item */
	CBOR_UNDERRUN, /* less bytes than data item */
	CBOR_BREAK,
} cbor_error_t;

cbor_error_t cbor_decode(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
size_t cbor_compute_decoded_size(const uint8_t *msg, size_t msgsize);

#if defined(__cplusplus)
}
#endif

#endif /* LIBCBOR_H */
