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

		if (strkey && strkey_len && p->keylen >= strkey_len) {
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

	/* TAG items themselves carry no value; the wrapped item is dispatched */
	if (item->type == CBOR_ITEM_TAG) {
		return;
	}

	if (parent && parent->type == CBOR_ITEM_MAP) {
		/* Count non-TAG items from parent+1 to item to get logical
		 * position, so TAG slots do not disturb key/value detection. */
		size_t pos = 0;
		for (const cbor_item_t *p = parent + 1; p <= item; p++) {
			if (p->type != CBOR_ITEM_TAG) {
				pos++;
			}
		}
		if (pos % 2 == 1) { /* key position — odd (1-indexed) */
			return;
		}

		/* Walk backward past any TAG items to reach the actual key */
		const cbor_item_t *key = item - 1;
		while (key > parent && key->type == CBOR_ITEM_TAG) {
			key--;
		}

		if (key->type == CBOR_ITEM_INTEGER) {
			cbor_decode(reader, key, &intkey, sizeof(intkey));
		} else {
			strkey = cbor_decode_pointer(reader, key);
			strkey_len = key->size;
		}
	}

	if (strkey || intkey != -1) {
		const struct cbor_parser *parser =
			get_parser(ctx, intkey, strkey, strkey_len);

		if (parser && parser->run) {
			parser->run(reader, parser, item, ctx->arg);
		}
	}
}

typedef enum {
	ITER_LEAF,                   /* not a container; decode and callback */
	ITER_RECURSE,                /* container; recurse into children */
	ITER_RECURSE_CALLBACK_FIRST, /* indefinite string; callback then recurse */
	ITER_STOP,                   /* MAP size overflow; abort iteration */
	ITER_TAG,                    /* tag prefix; callback then advance extra */
} iter_action_t;

static iter_action_t get_iter_action(const cbor_item_t *item,
		size_t remaining_nodes, size_t *len)
{
	if (item->type == CBOR_ITEM_TAG) {
		return ITER_TAG;
	}

	if (item->type == CBOR_ITEM_STRING &&
			item->size == (size_t)CBOR_INDEFINITE_VALUE) {
		*len = remaining_nodes;
		return ITER_RECURSE_CALLBACK_FIRST;
	}

	if (item->type != CBOR_ITEM_MAP && item->type != CBOR_ITEM_ARRAY) {
		return ITER_LEAF;
	}

	if (item->size == (size_t)CBOR_INDEFINITE_VALUE) {
		*len = remaining_nodes;
		return ITER_RECURSE;
	}

	if (item->type == CBOR_ITEM_MAP) {
		if (item->size > SIZE_MAX / 2) {
			return ITER_STOP;
		}

		*len = item->size * 2;
		return ITER_RECURSE;
	}

	*len = item->size;
	return ITER_RECURSE;
}

static size_t iterate_each(const cbor_reader_t *reader,
		const cbor_item_t *items, size_t nr_items, size_t max_nodes,
		const cbor_item_t *parent,
		void (*callback_each)(const cbor_reader_t *reader,
				const cbor_item_t *item,
				const cbor_item_t *parent, void *arg),
		void *arg)
{
	size_t extra = 0;
	size_t i = 0;

	for (i = 0; i < nr_items; i++) {
		if ((i + extra) >= max_nodes) {
			break;
		}
		const cbor_item_t *item = &items[i + extra];
		size_t remaining_nodes = max_nodes - (i + extra + 1);
		size_t len;

		switch (get_iter_action(item, remaining_nodes, &len)) {
		case ITER_RECURSE_CALLBACK_FIRST:
			(*callback_each)(reader, item, parent, arg);
			/* fall through */
		case ITER_RECURSE:
			extra += iterate_each(reader, item + 1, len,
					remaining_nodes, item,
					callback_each, arg);
			continue;
		case ITER_STOP:
			return i + extra;
		case ITER_TAG:
			/* TAG is transparent: visible to caller but does not
			 * consume a logical iteration slot. Advance the physical
			 * extra counter and re-process the same logical index so
			 * the wrapped item is seen under the original parent. */
			(*callback_each)(reader, item, parent, arg);
			extra++;
			i--;	/* neutralised by the for-loop's i++ */
			continue;
		case ITER_LEAF:
		default:
			break;
		}

		if (cbor_item_is_break(item)) {
			/* account for the BREAK token in the consumed count */
			if (parent && parent->size == (size_t)
					CBOR_INDEFINITE_VALUE) {
				i++;
			}
			break;
		}

		(*callback_each)(reader, item, parent, arg);
	}

	return i + extra;
}

bool cbor_unmarshal(cbor_reader_t *reader, const struct cbor_parser *parsers,
		size_t nr_parsers, const void *msg, size_t msglen, void *arg)
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

	iterate_each(reader, reader->items, n, n, 0, parse_item, &ctx);

	return true;
}

size_t cbor_iterate(const cbor_reader_t *reader, const cbor_item_t *parent,
		void (*callback_each)(const cbor_reader_t *reader,
				const cbor_item_t *item,
				const cbor_item_t *parent, void *arg),
		void *arg)
{
	return iterate_each(reader, reader->items, reader->itemidx,
			reader->itemidx, parent, callback_each, arg);
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
	case CBOR_ITEM_TAG:
		return "tag";
	case CBOR_ITEM_UNKNOWN: /* fall through */
	default:
		return "unknown";
	}
}
