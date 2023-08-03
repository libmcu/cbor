/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/helper.h"
#include "cbor/parser.h"
#include "cbor/decoder.h"

#include <string.h>

struct parser_ctx {
	const struct cbor_parser *parsers;
	size_t nr_parsers;
	void *arg;
};

static const struct cbor_parser *get_parser(const struct parser_ctx *ctx,
		intptr_t intkey, const void *strkey, size_t strkey_len)
{
	for (size_t i = 0; i < ctx->nr_parsers; i++) {
		const struct cbor_parser *p = &ctx->parsers[i];

		if (!p->key) {
			continue;
		}

		if (strkey && strkey_len) {
			if (memcmp(p->key, strkey, strkey_len) == 0) {
				return p;
			}
		} else if (intkey == (intptr_t)p->key) {
			return p;
		}
	}

	return NULL;
}

static void parse_item(const cbor_reader_t *reader, const cbor_item_t *item,
		    const cbor_item_t *parent, void *arg)
{
	struct parser_ctx *ctx = (struct parser_ctx *)arg;
	const void *strkey = NULL;
	size_t strkey_len = 0;
	intptr_t intkey = -1;

	if (parent && parent->type == CBOR_ITEM_MAP) {
		if ((item - parent) % 2) { /* key */
			return;
		}

		if ((item-1)->type == CBOR_ITEM_INTEGER) {
			cbor_decode(reader, item-1, &intkey, sizeof(intkey));
		} else {
			strkey = cbor_decode_pointer(reader, item-1);
			strkey_len = (item-1)->size;
		}
	}

	if (strkey || intkey != -1) {
		const struct cbor_parser *parser = get_parser(ctx,
				intkey, strkey, strkey_len);

		if (parser && parser->run) {
			parser->run(reader, parser, item, ctx->arg);
		}
	}
}

static size_t iterate_each(const cbor_reader_t *reader,
		    const cbor_item_t *items, size_t nr_items,
		    const cbor_item_t *parent,
		    void (*callback_each)(const cbor_reader_t *reader,
			    const cbor_item_t *item, const cbor_item_t *parent,
			    void *arg),
		    void *arg)
{
	size_t offset = 0;
	size_t i = 0;

	for (i = 0; i < nr_items; i++) {
		const cbor_item_t *item = &items[i+offset];

		if (item->type == CBOR_ITEM_MAP
				|| item->type == CBOR_ITEM_ARRAY) {
			size_t len = item->type == CBOR_ITEM_MAP?
					item->size*2 : item->size;
			offset += iterate_each(reader, item+1, len, item,
					callback_each, arg);
			continue;
		}

		if (cbor_decode(reader, item, 0, 0) == CBOR_BREAK) {
			break;
		}

		(*callback_each)(reader, item, parent, arg);
	}

	return i + offset;
}

bool cbor_unmarshal(cbor_reader_t *reader,
		const struct cbor_parser *parsers, size_t nr_parsers,
		const void *msg, size_t msglen, void *arg)
{
	size_t n;
	cbor_error_t err = cbor_parse(reader, msg, msglen, &n);

	if (err != CBOR_SUCCESS && err != CBOR_BREAK) {
		return false;
	}

	struct parser_ctx ctx = {
		.parsers = parsers,
		.nr_parsers = nr_parsers,
		.arg = arg,
	};

	iterate_each(reader, reader->items, n, 0, parse_item, &ctx);

	return true;
}

size_t cbor_iterate(const cbor_reader_t *reader,
		    const cbor_item_t *parent,
		    void (*callback_each)(const cbor_reader_t *reader,
			    const cbor_item_t *item, const cbor_item_t *parent,
			    void *arg),
		    void *arg)
{
	return iterate_each(reader, reader->items, reader->itemidx,
			parent, callback_each, arg);
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
