#include "cbor/helper.h"
#include "cbor/decoder.h"

size_t cbor_iterate(cbor_reader_t const *reader,
		    cbor_item_t const *items, size_t nr_items,
		    cbor_item_t const *parent,
		    void (*callback_each)(cbor_reader_t const *reader,
			    cbor_item_t const *item, cbor_item_t const *parent,
			    void *udt),
		    void *udt)
{
	size_t offset = 0;
	size_t i = 0;

	for (i = 0; i < nr_items; i++) {
		cbor_item_t const *item = &items[i+offset];

		if (item->type == CBOR_ITEM_MAP || item->type == CBOR_ITEM_ARRAY) {
			size_t len = item->type == CBOR_ITEM_MAP? item->size*2 : item->size;
			offset += cbor_iterate(reader, item+1, len, item, callback_each, udt);
			continue;
		}

		if (cbor_decode(reader, item, 0, 0) == CBOR_BREAK) {
			break;
		}

		(*callback_each)(reader, item, parent, udt);
	}

	return i + offset;
}

const char *cbor_stringify_error(cbor_error_t err)
{
	switch (err) {
	case CBOR_SUCCESS:
		return "success";
	case CBOR_INVALID:
		return "invalid";
	case CBOR_OVERRUN:
		return "out of memory";
	case CBOR_BREAK:
		return "break";
	case CBOR_EXCESSIVE:
		return "too deep recursion";
	case CBOR_ILLEGAL: /* fall through */
	default:
		return "not well-formed";
	}
}

const char *cbor_stringify_item(cbor_item_t *item)
{
	switch (item->type) {
	case CBOR_ITEM_INTEGER:
		return "integer";
	case CBOR_ITEM_STRING:
		return "string";
	case CBOR_ITEM_ARRAY:
		return "array";
	case CBOR_ITEM_MAP:
		return "map";
	case CBOR_ITEM_FLOAT:
		return "float";
	case CBOR_ITEM_SIMPLE_VALUE:
		return "simple value";
	case CBOR_ITEM_UNKNOWN: /* fall through */
	default:
		return "unknown";
	}
}
