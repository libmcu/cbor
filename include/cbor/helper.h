#ifndef CBOR_HELPER_H
#define CBOR_HELPER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/cbor.h"

const char *cbor_stringify_error(cbor_error_t err);
const char *cbor_stringify_data_type(cbor_item_data_t type);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_HELPER_H */
