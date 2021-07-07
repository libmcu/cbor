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

/*
 * - little endian 만 지원
 * - 음수일 경우 변수크기가 값 크기보다 작을 경우 invalid 한데, 이걸 캐치할 수 없음
 * 가령 0x3880 이고, int8_t 일경우 -129를 담을 수 없어서 127이 됨
 * - byte string 최대 길이는 size_t. uint64_t 까지 지원하지 않음. 리턴 타입이 size_t 이기 때문
 */
