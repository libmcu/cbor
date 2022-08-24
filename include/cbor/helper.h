#ifndef CBOR_HELPER_H
#define CBOR_HELPER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

const char *cbor_stringify_error(cbor_error_t err);
const char *cbor_stringify_item(cbor_item_t *item);

size_t cbor_iterate(cbor_reader_t const *reader,
		    cbor_item_t const *items, size_t nr_items,
		    cbor_item_t const *parent,
		    void (*callback_each)(cbor_reader_t const *reader,
			    cbor_item_t const *item, cbor_item_t const *parent,
			    void *udt),
		    void *udt);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_HELPER_H */
