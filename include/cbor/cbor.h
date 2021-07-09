#ifndef CBOR_H
#define CBOR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if !defined(CBOR_RECURSION_DEPTH)
#define CBOR_RECURSION_MAX_LEVEL			4
#endif

typedef enum {
	CBOR_SUCCESS, /* well-formed */
	CBOR_ILLEGAL, /* not well-formed */
	CBOR_INVALID, /* well-formed but invalid */
	CBOR_OVERRUN, /* more items than given buffer space */
	CBOR_UNDERRUN, /* larger buffer space than data items */
	CBOR_BREAK,
	CBOR_EXCESSIVE, /* recursion more than @ref CBOR_RECURSION_MAX_LEVEL */
} cbor_error_t;

cbor_error_t cbor_decode(void *buf, size_t bufsize,
		const uint8_t *msg, size_t msgsize);
size_t cbor_compute_decoded_size(const uint8_t *msg, size_t msgsize);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_H */
