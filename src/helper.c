/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/helper.h"
#include "cbor/parser.h"
#include "cbor/decoder.h"

#include <string.h>

struct path_stack {
	struct cbor_path_segment segments[CBOR_RECURSION_MAX_LEVEL];
	size_t depth;
};

static bool segment_equal(const struct cbor_path_segment *pattern,
		const struct cbor_path_segment *data)
{
	/* Only path (pattern) segments can be wildcards; runtime data segments never are. */
	if (pattern->type == CBOR_KEY_ANY) {
		return true;
	}

	if (pattern->type != data->type) {
		return false;
	}

	switch (pattern->type) {
	case CBOR_KEY_STR:
		return pattern->key.str.len == data->key.str.len &&
			memcmp(pattern->key.str.ptr, data->key.str.ptr,
				pattern->key.str.len) == 0;
	case CBOR_KEY_INT:
	case CBOR_KEY_IDX:
		return pattern->key.idx == data->key.idx;
	case CBOR_KEY_ANY: /* fall through */
	default:
		return false;
	}
}

static bool path_matches(const struct path_stack *stack,
		const struct cbor_parser *p)
{
	if (stack->depth != p->depth) {
		return false;
	}
	for (size_t i = 0; i < p->depth; i++) {
		if (!segment_equal(&p->path[i], &stack->segments[i])) {
			return false;
		}
	}
	return true;
}

struct parser_ctx {
	const struct cbor_parser *parsers;
	size_t nr_parsers;
	void *arg;
	struct path_stack *stack;
};

static bool path_has_wildcard(const struct cbor_parser *p)
{
	for (size_t i = 0; i < p->depth; i++) {
		if (p->path[i].type == CBOR_KEY_ANY) {
			return true;
		}
	}
	return false;
}

static void dispatch_item(const cbor_reader_t *reader,
		const cbor_item_t *item, struct parser_ctx *ctx)
{
	/* Prefer exact-path parsers over wildcard parsers.  Run exact matches
	 * first; fall back to wildcard matches only when no exact match fires. */
	bool exact_fired = false;

	for (size_t i = 0; i < ctx->nr_parsers; i++) {
		const struct cbor_parser *p = &ctx->parsers[i];
		if (p->run && path_matches(ctx->stack, p) &&
				!path_has_wildcard(p)) {
			p->run(reader, p, item, ctx->arg);
			exact_fired = true;
		}
	}

	if (exact_fired) {
		return;
	}

	for (size_t i = 0; i < ctx->nr_parsers; i++) {
		const struct cbor_parser *p = &ctx->parsers[i];
		if (p->run && path_matches(ctx->stack, p) &&
				path_has_wildcard(p)) {
			p->run(reader, p, item, ctx->arg);
		}
	}
}

typedef enum {
	ITER_LEAF,
	ITER_RECURSE,
	ITER_RECURSE_CALLBACK_FIRST,
	ITER_STOP,
	ITER_TAG,
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

typedef void (*iter_cb_t)(const cbor_reader_t *reader,
		const cbor_item_t *item, const cbor_item_t *parent,
		size_t logical_idx, void *arg);

static size_t iterate_each(const cbor_reader_t *reader,
		const cbor_item_t *items, size_t nr_items, size_t max_nodes,
		const cbor_item_t *parent,
		iter_cb_t callback_each, void *arg)
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
			(*callback_each)(reader, item, parent, i, arg);
			/* fall through */
		case ITER_RECURSE:
			extra += iterate_each(reader, item + 1, len,
					remaining_nodes, item,
					callback_each, arg);
			continue;
		case ITER_STOP:
			return i + extra;
		case ITER_TAG:
			(*callback_each)(reader, item, parent, i, arg);
			extra++;
			i--;
			continue;
		case ITER_LEAF:
		default:
			break;
		}

		if (cbor_item_is_break(item)) {
			if (parent && parent->size == (size_t)
					CBOR_INDEFINITE_VALUE) {
				i++;
			}
			break;
		}

		(*callback_each)(reader, item, parent, i, arg);
	}

	return i + extra;
}

static bool make_map_seg(const cbor_reader_t *reader,
		const cbor_item_t *key, struct cbor_path_segment *seg)
{
	if (key->type == CBOR_ITEM_INTEGER) {
		intmax_t v = 0;
		if (cbor_decode(reader, key, &v, sizeof(v)) != CBOR_SUCCESS) {
			return false;
		}
		seg->type = CBOR_KEY_INT;
		seg->key.idx = v;
	} else if (key->type == CBOR_ITEM_STRING) {
		seg->type = CBOR_KEY_STR;
		seg->key.str.ptr = cbor_decode_pointer(reader, key);
		seg->key.str.len = key->size;
	} else {
		return false;
	}

	return true;
}

static bool push_seg(struct path_stack *stack,
		const struct cbor_path_segment *seg)
{
	if (stack->depth >= CBOR_RECURSION_MAX_LEVEL) {
		return false;
	}
	stack->segments[stack->depth++] = *seg;
	return true;
}

static void pop_seg(struct path_stack *stack)
{
	if (stack->depth > 0) {
		stack->depth--;
	}
}

static size_t skip_subtree(const cbor_item_t *items, size_t nr_items,
		size_t max_nodes)
{
	size_t extra = 0;
	size_t i = 0;

	for (i = 0; i < nr_items; i++) {
		if ((i + extra) >= max_nodes) {
			break;
		}
		const cbor_item_t *item = &items[i + extra];
		size_t remaining = max_nodes - (i + extra + 1);
		size_t len = 0;
		iter_action_t action = get_iter_action(item, remaining, &len);

		if (action == ITER_TAG) {
			extra++;
			i--;
			continue;
		}

		if (cbor_item_is_break(item)) {
			i++;
			break;
		}

		if (action == ITER_STOP) {
			return i + extra;
		}

		if (action == ITER_RECURSE || action == ITER_RECURSE_CALLBACK_FIRST) {
			extra += skip_subtree(item + 1, len, remaining);
		}
	}

	return i + extra;
}

static size_t dispatch_each(const cbor_reader_t *reader,
		const cbor_item_t *items, size_t nr_items, size_t max_nodes,
		const cbor_item_t *parent, struct parser_ctx *ctx);

static size_t dispatch_value(const cbor_reader_t *reader,
		const cbor_item_t *item, size_t remaining_nodes,
		iter_action_t action, size_t len,
		const cbor_item_t *key_item,
		const cbor_item_t *parent, size_t array_idx,
		struct parser_ctx *ctx)
{
	struct cbor_path_segment seg;
	bool seg_pushed = false;

	if (parent != NULL) {
		bool seg_valid = false;

		if (parent->type == CBOR_ITEM_MAP && key_item != NULL) {
			seg_valid = make_map_seg(reader, key_item, &seg);
		} else if (parent->type == CBOR_ITEM_ARRAY) {
			seg.type = CBOR_KEY_IDX;
			seg.key.idx = (intmax_t)array_idx;
			seg_valid = true;
		}

		if (parent->type == CBOR_ITEM_MAP && !seg_valid) {
			return (action == ITER_LEAF) ? 0 :
				skip_subtree(item + 1, len, remaining_nodes);
		}

		if (seg_valid) {
			if (!push_seg(ctx->stack, &seg)) {
				return (action == ITER_LEAF) ? 0 :
					skip_subtree(item + 1, len, remaining_nodes);
			}
			seg_pushed = true;
		}
	}

	size_t consumed = 0;

	if (action == ITER_RECURSE_CALLBACK_FIRST) {
		dispatch_item(reader, item, ctx);
		consumed = skip_subtree(item + 1, len, remaining_nodes);
	} else if (action == ITER_RECURSE) {
		consumed = dispatch_each(reader, item + 1, len,
				remaining_nodes, item, ctx);
	} else if (action == ITER_LEAF) {
		dispatch_item(reader, item, ctx);
	}

	if (seg_pushed) {
		pop_seg(ctx->stack);
	}

	return consumed;
}

static size_t dispatch_each(const cbor_reader_t *reader,
		const cbor_item_t *items, size_t nr_items, size_t max_nodes,
		const cbor_item_t *parent, struct parser_ctx *ctx)
{
	size_t extra = 0;
	size_t i = 0;
	size_t array_idx = 0;
	const cbor_item_t *last_key_item = NULL;

	for (i = 0; i < nr_items; i++) {
		if ((i + extra) >= max_nodes) {
			break;
		}

		const cbor_item_t *item = &items[i + extra];
		size_t remaining_nodes = max_nodes - (i + extra + 1);
		size_t len;
		iter_action_t action =
			get_iter_action(item, remaining_nodes, &len);

		if (action == ITER_TAG) {
			extra++;
			i--;
			continue;
		}

		if (cbor_item_is_break(item)) {
			if (parent && parent->size == (size_t)
					CBOR_INDEFINITE_VALUE) {
				i++;
			}
			break;
		}

		if (action == ITER_STOP) {
			return i + extra;
		}

		if (parent != NULL && parent->type == CBOR_ITEM_MAP) {
			bool is_key = ((i % 2) == 0);

			if (is_key) {
				last_key_item = item;
				if (action == ITER_RECURSE || action == ITER_RECURSE_CALLBACK_FIRST) {
					extra += skip_subtree(item + 1, len, remaining_nodes);
					last_key_item = NULL;
				}
				continue;
			}

			extra += dispatch_value(reader, item, remaining_nodes,
					action, len, last_key_item, parent, 0, ctx);
			last_key_item = NULL;
			continue;
		}

		extra += dispatch_value(reader, item, remaining_nodes,
				action, len, NULL, parent, array_idx, ctx);
		array_idx++;
	}

	return i + extra;
}

struct iterate_wrap {
	void (*cb)(const cbor_reader_t *, const cbor_item_t *,
		   const cbor_item_t *, void *);
	void *arg;
};

static void iterate_trampoline(const cbor_reader_t *reader,
		const cbor_item_t *item, const cbor_item_t *parent,
		size_t logical_idx, void *arg)
{
	(void)logical_idx;
	const struct iterate_wrap *w = (const struct iterate_wrap *)arg;
	w->cb(reader, item, parent, w->arg);
}

bool cbor_unmarshal(cbor_reader_t *reader, const struct cbor_parser *parsers,
		size_t nr_parsers, const void *msg, size_t msglen, void *arg)
{
	size_t n;
	cbor_error_t err = cbor_parse(reader, msg, msglen, &n);

	if (err != CBOR_SUCCESS && err != CBOR_BREAK) {
		return false;
	}

	if (parsers == NULL && nr_parsers > 0) {
		return false;
	}

	for (size_t i = 0; i < nr_parsers; i++) {
		if (parsers[i].depth > CBOR_RECURSION_MAX_LEVEL) {
			return false;
		}
		if (parsers[i].depth > 0 && parsers[i].path == NULL) {
			return false;
		}
	}

	struct path_stack stack = { .depth = 0 };
	struct parser_ctx ctx = {
		.parsers    = parsers,
		.nr_parsers = nr_parsers,
		.arg        = arg,
		.stack      = &stack,
	};

	dispatch_each(reader, reader->items, n, n, NULL, &ctx);

	return true;
}

size_t cbor_iterate(const cbor_reader_t *reader, const cbor_item_t *parent,
		void (*callback_each)(const cbor_reader_t *reader,
				const cbor_item_t *item,
				const cbor_item_t *parent, void *arg),
		void *arg)
{
	struct iterate_wrap wrap = { callback_each, arg };
	return iterate_each(reader, reader->items, reader->itemidx,
			reader->itemidx, parent, iterate_trampoline, &wrap);
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
	case CBOR_ITEM_UNKNOWN:
	default:
		return "unknown";
	}
}
