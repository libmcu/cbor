#include "cbor/helper.h"

const char *cbor_stringify_data_type(cbor_item_data_t type)
{
	switch (type) {
	case CBOR_ITEM_INTEGER:
		return "integer";
	case CBOR_ITEM_STRING:
		return "string";
	case CBOR_ITEM_ARRAY:
		return "array";
	case CBOR_ITEM_MAP:
		return "map";
	case CBOR_ITEM_FLOAT_AND_SIMPLE_VALUE:
		return "float and simple value";
	case CBOR_ITEM_UNKNOWN: /* fall through */
	default:
		return "unknown";
	}
}
