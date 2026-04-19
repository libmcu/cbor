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
	if (pattern->type == CBOR_KEY_ANY) {
		return true;
	}

	if (pattern->type != data->type) {
		return false;
	}

	switch (pattern->type) {
	case CBOR_KEY_STR:
		return pattern->len == data->len &&
			memcmp((const void *)pattern->val,
				(const void *)data->val, pattern->len) == 0;
	case CBOR_KEY_INT:
	case CBOR_KEY_IDX:
		return pattern->val == data->val;
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

static bool path_has_wildcard(const struct cbor_parser *p)
{
	for (size_t i = 0; i < p->depth; i++) {
		if (p->path[i].type == CBOR_KEY_ANY) {
			return true;
		}
	}
	return false;
}

struct parser_ctx {
	const struct cbor_parser *parsers;
	size_t nr_parsers;
	void *arg;
	struct path_stack *stack;
};

static void dispatch_item(const cbor_reader_t *reader,
		const cbor_item_t *item, struct parser_ctx *ctx)
{
	/* Single pass: run exact matches immediately; collect wildcard matches
	 * for deferred execution only if no exact match fires.
	 * All exact-match parsers whose paths match are invoked in registration
	 * order. Registering multiple parsers for the same path is intentional
	 * and supported (e.g. one parser processes the value, another logs it).
	 * At most CBOR_MAX_WILDCARD_PARSERS (default 8) wildcard parsers are
	 * collected per dispatch; additional matches beyond that limit are
	 * silently dropped in registration order. Raise CBOR_MAX_WILDCARD_PARSERS
	 * at build time if more wildcard parsers are needed. */
	const struct cbor_parser *wc[CBOR_MAX_WILDCARD_PARSERS];
	size_t nr_wc = 0;
	bool exact_fired = false;

	for (size_t i = 0; i < ctx->nr_parsers; i++) {
		const struct cbor_parser *p = &ctx->parsers[i];
		if (!p->run || !path_matches(ctx->stack, p)) {
			continue;
		}
		if (path_has_wildcard(p)) {
			if (nr_wc < CBOR_MAX_WILDCARD_PARSERS) {
				wc[nr_wc++] = p;
			}
		} else {
			p->run(reader, p, item, ctx->arg);
			exact_fired = true;
		}
	}

	if (!exact_fired) {
		for (size_t i = 0; i < nr_wc; i++) {
			wc[i]->run(reader, wc[i], item, ctx->arg);
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

static bool is_indefinite_string(const cbor_item_t *item)
{
	return item->type == CBOR_ITEM_STRING &&
		item->size == (size_t)CBOR_INDEFINITE_VALUE;
}

static bool is_container(const cbor_item_t *item)
{
	return item->type == CBOR_ITEM_MAP || item->type == CBOR_ITEM_ARRAY;
}

static bool is_indefinite_container(const cbor_item_t *item)
{
	return item->size == (size_t)CBOR_INDEFINITE_VALUE;
}

static iter_action_t get_iter_action(const cbor_item_t *item,
		size_t remaining_nodes, size_t *len)
{
	if (item->type == CBOR_ITEM_TAG) {
		return ITER_TAG;
	}

	if (is_indefinite_string(item)) {
		*len = remaining_nodes;
		return ITER_RECURSE_CALLBACK_FIRST;
	}

	if (!is_container(item)) {
		return ITER_LEAF;
	}

	if (is_indefinite_container(item)) {
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
		intptr_t v = 0;
		if (cbor_decode(reader, key, &v, sizeof(v)) != CBOR_SUCCESS) {
			return false;
		}
		seg->type = CBOR_KEY_INT;
		seg->val = v;
		seg->len = 0;
	} else if (key->type == CBOR_ITEM_STRING) {
		const void *p = cbor_decode_pointer(reader, key);
		seg->type = CBOR_KEY_STR;
		seg->val = (intptr_t)(uintptr_t)p;
		seg->len = key->size;
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

static bool try_make_segment(const cbor_reader_t *reader,
		const cbor_item_t *parent, const cbor_item_t *key_item,
		size_t array_idx, struct cbor_path_segment *seg)
{
	if (parent->type == CBOR_ITEM_MAP && key_item != NULL) {
		return make_map_seg(reader, key_item, seg);
	}

	if (parent->type == CBOR_ITEM_ARRAY) {
		seg->type = CBOR_KEY_IDX;
		seg->val = (intptr_t)array_idx;
		seg->len = 0;
		return true;
	}

	return false;
}

static size_t skip_if_needed(iter_action_t action, const cbor_item_t *item,
		size_t len, size_t remaining_nodes)
{
	if (action == ITER_LEAF) {
		return 0;
	}
	return skip_subtree(item + 1, len, remaining_nodes);
}

static size_t dispatch_by_action(const cbor_reader_t *reader,
		const cbor_item_t *item, iter_action_t action,
		size_t len, size_t remaining_nodes, struct parser_ctx *ctx)
{
	if (action == ITER_RECURSE_CALLBACK_FIRST) {
		dispatch_item(reader, item, ctx);
		return skip_subtree(item + 1, len, remaining_nodes);
	}

	if (action == ITER_RECURSE) {
		dispatch_item(reader, item, ctx);
		return dispatch_each(reader, item + 1, len,
				remaining_nodes, item, ctx);
	}

	if (action == ITER_LEAF) {
		dispatch_item(reader, item, ctx);
	}

	return 0;
}

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
		bool seg_valid = try_make_segment(reader, parent, key_item,
				array_idx, &seg);

		if (parent->type == CBOR_ITEM_MAP && !seg_valid) {
			return skip_if_needed(action, item, len, remaining_nodes);
		}

		if (seg_valid && !push_seg(ctx->stack, &seg)) {
			return skip_if_needed(action, item, len, remaining_nodes);
		}

		seg_pushed = seg_valid;
	}

	size_t consumed = dispatch_by_action(reader, item, action, len,
			remaining_nodes, ctx);

	if (seg_pushed) {
		pop_seg(ctx->stack);
	}

	return consumed;
}

static bool should_skip_map_key(iter_action_t action)
{
	return action == ITER_RECURSE || action == ITER_RECURSE_CALLBACK_FIRST;
}

static bool is_indefinite_break(const cbor_item_t *item,
		const cbor_item_t *parent)
{
	return cbor_item_is_break(item) && parent &&
		parent->size == (size_t)CBOR_INDEFINITE_VALUE;
}

static size_t handle_map_dispatch(const cbor_reader_t *reader,
		const cbor_item_t *items, size_t i, size_t extra,
		size_t remaining_nodes, iter_action_t action, size_t len,
		const cbor_item_t *parent, const cbor_item_t **last_key_item,
		struct parser_ctx *ctx)
{
	bool is_key = ((i % 2) == 0);

	if (is_key) {
		*last_key_item = &items[i + extra];
		if (should_skip_map_key(action)) {
			size_t skipped = skip_subtree(&items[i + extra + 1],
					len, remaining_nodes);
			*last_key_item = NULL;
			return skipped;
		}
		return 0;
	}

	size_t consumed = dispatch_value(reader, &items[i + extra],
			remaining_nodes, action, len, *last_key_item,
			parent, 0, ctx);
	*last_key_item = NULL;
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
		size_t len = 0;
		iter_action_t action =
			get_iter_action(item, remaining_nodes, &len);

		if (action == ITER_TAG) {
			extra++;
			i--;
			continue;
		}

		if (is_indefinite_break(item, parent)) {
			i++;
			break;
		}

		if (cbor_item_is_break(item)) {
			break;
		}

		if (action == ITER_STOP) {
			return i + extra;
		}

		if (parent != NULL && parent->type == CBOR_ITEM_MAP) {
			extra += handle_map_dispatch(reader, items, i, extra,
					remaining_nodes, action, len, parent,
					&last_key_item, ctx);
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

static bool validate_parsers(const struct cbor_parser *parsers,
		size_t nr_parsers)
{
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

	return true;
}

bool cbor_unmarshal(cbor_reader_t *reader, const struct cbor_parser *parsers,
		size_t nr_parsers, const void *msg, size_t msglen, void *arg)
{
	size_t n;
	cbor_error_t err = cbor_parse(reader, msg, msglen, &n);

	if (err != CBOR_SUCCESS && err != CBOR_BREAK) {
		return false;
	}

	if (!validate_parsers(parsers, nr_parsers)) {
		return false;
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

bool cbor_dispatch(const cbor_reader_t *reader,
		const cbor_item_t *container,
		const struct cbor_parser *parsers, size_t nr_parsers,
		void *arg)
{
	if (!validate_parsers(parsers, nr_parsers)) {
		return false;
	}

	struct path_stack stack = { .depth = 0 };
	struct parser_ctx ctx = {
		.parsers    = parsers,
		.nr_parsers = nr_parsers,
		.arg        = arg,
		.stack      = &stack,
	};

	if (container == NULL) {
		dispatch_each(reader, reader->items, reader->itemidx,
				reader->itemidx, NULL, &ctx);
		return true;
	}

	if (container < reader->items ||
			container >= reader->items + reader->itemidx) {
		return false;
	}
	if (container->type != CBOR_ITEM_MAP &&
			container->type != CBOR_ITEM_ARRAY) {
		return false;
	}

	size_t item_idx  = (size_t)(container - reader->items);
	size_t remaining = reader->itemidx - item_idx - 1;
	size_t nr_children;

	if (container->size == (size_t)CBOR_INDEFINITE_VALUE) {
		nr_children = remaining;
	} else if (container->type == CBOR_ITEM_MAP) {
		if (container->size > SIZE_MAX / 2) {
			return false;
		}
		nr_children = container->size * 2;
	} else {
		nr_children = container->size;
	}

	if (nr_children > remaining) {
		nr_children = remaining;
	}

	dispatch_each(reader, container + 1, nr_children,
			remaining, container, &ctx);
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
