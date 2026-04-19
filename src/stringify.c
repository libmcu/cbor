/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/helper.h"

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
		return "excessive nesting";
	case CBOR_NEED_MORE:
		return "need more data";
	case CBOR_ABORTED:
		return "aborted";
	case CBOR_ILLEGAL:
	default:
		return "not well-formed";
	}
}

const char *cbor_stringify_item(const cbor_item_t *item)
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
	case CBOR_ITEM_TAG:
		return "tag";
	case CBOR_ITEM_UNKNOWN:
	default:
		return "unknown";
	}
}
