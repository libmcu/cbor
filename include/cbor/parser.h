#ifndef CBOR_PARSER_H
#define CBOR_PARSER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

typedef struct {
	const uint8_t *msg;
	size_t msgsize;
	size_t msgidx;
} cbor_parser_t;

cbor_error_t cbor_parser_init(cbor_parser_t *parser,
		const void *msg, size_t msgsize);
/**
 * Parse the encoded CBOR messages into items.
 *
 * @param[in,out] parser instance to keep the context
 * @param[out] items a pointer to item buffers
 * @param[in] maxitems the maximum number of items to be stored in @p items
 * @param[out] items_parsed the number of items parsed gets stored if not null
 *
 * @return a code of @ref cbor_error_t
 */
cbor_error_t cbor_parse(cbor_parser_t *parser,
		cbor_item_t *items, size_t maxitems, size_t *items_parsed);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_PARSER_H */
