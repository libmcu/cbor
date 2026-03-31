/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/parser.h"
#include <stdbool.h>

#if !defined(assert)
#define assert(expr)
#endif

struct parser_context {
	cbor_reader_t *reader;

	uint8_t major_type;
	uint8_t additional_info;
	uint8_t following_bytes;

	uint8_t recursion_depth;
	_Static_assert(CBOR_RECURSION_MAX_LEVEL < 256, "");

	bool dry_run;
};

typedef cbor_error_t (*type_parser_t)(struct parser_context *ctx);

static cbor_error_t do_integer(struct parser_context *ctx);
static cbor_error_t do_string(struct parser_context *ctx);
static cbor_error_t do_recursive(struct parser_context *ctx);
static cbor_error_t do_tag(struct parser_context *ctx);
static cbor_error_t do_float_and_other(struct parser_context *ctx);

/* 8 parsers for 3-bit major type */
static const type_parser_t parsers[] = {
	do_integer,		/* 0: unsigned integer */
	do_integer,		/* 1: negative integer */
	do_string,		/* 2: byte string */
	do_string,		/* 3: text string encoded as utf-8 */
	do_recursive,		/* 4: array */
	do_recursive,		/* 5: map */
	do_tag,			/* 6: tag */
	do_float_and_other,	/* 7: float, simple value, and break */
};

static bool has_valid_following_bytes(const struct parser_context *ctx,
		cbor_error_t *err)
{
	*err = CBOR_SUCCESS;

	if (ctx->following_bytes == (uint8_t)CBOR_RESERVED_VALUE) {
		*err = CBOR_ILLEGAL;
		return false;
	} else if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		return true;
	}

	if ((ctx->following_bytes + 1u)
			> ctx->reader->msgsize - ctx->reader->msgidx) {
		*err = CBOR_ILLEGAL;
		return false;
	}

	return true;
}

static size_t go_get_item_length(struct parser_context *ctx)
{
	uint64_t len = 0;
	size_t offset = 0;

	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		len = (uint64_t)CBOR_INDEFINITE_VALUE;
	} else if (ctx->following_bytes == 0) {
		len = ctx->additional_info;
	} else {
		const uint8_t *msg = &ctx->reader->msg[ctx->reader->msgidx];
		cbor_copy((uint8_t *)&len, &msg[1], ctx->following_bytes);
		offset = ctx->following_bytes;
	}

	ctx->reader->msgidx += offset + 1;

	return (size_t)len;
}

static cbor_error_t parse(struct parser_context *ctx, size_t maxitems)
{
	cbor_error_t err = CBOR_SUCCESS;

	if (++ctx->recursion_depth > CBOR_RECURSION_MAX_LEVEL) {
		return CBOR_EXCESSIVE;
	}

	for (size_t i = 0; i < maxitems &&
			ctx->reader->itemidx < ctx->reader->maxitems &&
			ctx->reader->msgidx < ctx->reader->msgsize; i++) {
		uint8_t val = ctx->reader->msg[ctx->reader->msgidx];
		ctx->major_type = get_cbor_major_type(val);
		ctx->additional_info = get_cbor_additional_info(val);
		ctx->following_bytes =
			cbor_get_following_bytes(ctx->additional_info);

		if (!has_valid_following_bytes(ctx, &err)) {
			break;
		}

		err = parsers[ctx->major_type](ctx);

		if (err == CBOR_BREAK) {
			if ((maxitems == (size_t)CBOR_INDEFINITE_VALUE) ||
					(ctx->reader->msgidx ==
						ctx->reader->msgsize)) {
				break;
			}
		} else if (err != CBOR_SUCCESS) {
			break;
		}

		err = CBOR_SUCCESS;
	}

	ctx->recursion_depth--;

	assert(ctx->msgidx <= ctx->msgsize);

	return err;
}

static void set_item(struct parser_context *ctx, cbor_item_data_t type,
		     size_t offset, size_t size)
{
	if (ctx->dry_run) {
		return;
	}

	cbor_item_t *item = &ctx->reader->items[ctx->reader->itemidx];
	item->type = type;
	item->size = size;
	item->offset = offset;
}

static cbor_error_t do_integer(struct parser_context *ctx)
{
	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		return CBOR_ILLEGAL;
	}

	set_item(ctx, CBOR_ITEM_INTEGER, ctx->reader->msgidx,
			(size_t)ctx->following_bytes);

	ctx->reader->msgidx += (size_t)(ctx->following_bytes + 1);
	ctx->reader->itemidx++;

	return CBOR_SUCCESS;
}

static cbor_error_t do_string(struct parser_context *ctx)
{
	size_t len = go_get_item_length(ctx);
	cbor_error_t err;

	set_item(ctx, CBOR_ITEM_STRING, ctx->reader->msgidx, len);

	if (len == (size_t)CBOR_INDEFINITE_VALUE) {
		ctx->reader->itemidx++;
		err = parse(ctx, (size_t)CBOR_INDEFINITE_VALUE);
		if (err != CBOR_SUCCESS) {
			return err;
		} else if (ctx->reader->itemidx >= ctx->reader->maxitems) {
			return CBOR_OVERRUN;
		}
		return CBOR_ILLEGAL;
	}
	if (len > ctx->reader->msgsize - ctx->reader->msgidx) {
		return CBOR_ILLEGAL;
	}

	ctx->reader->msgidx += len;
	ctx->reader->itemidx++;

	return CBOR_SUCCESS;
}

static cbor_error_t do_recursive(struct parser_context *ctx)
{
	size_t len = go_get_item_length(ctx);
	size_t expected_items = len;
	size_t nitems_before = ctx->reader->itemidx;
	cbor_error_t err;

	set_item(ctx, (cbor_item_data_t)(ctx->major_type - 1),
			ctx->reader->msgidx, len);
	if (len != (size_t)CBOR_INDEFINITE_VALUE && (cbor_item_data_t)
			(ctx->major_type - 1) == CBOR_ITEM_MAP) {
		if (len > SIZE_MAX / 2) {
			return CBOR_ILLEGAL;
		}
		expected_items = len * 2;
	}

	ctx->reader->itemidx++;

	err = parse(ctx, expected_items);
	if (len == (size_t)CBOR_INDEFINITE_VALUE) {
		if (err != CBOR_SUCCESS) {
			return err;
		} else if (ctx->reader->itemidx >= ctx->reader->maxitems) {
			return CBOR_OVERRUN;
		}
		return CBOR_ILLEGAL;
	} else if (err != CBOR_SUCCESS) {
		return err;
	}

	if ((ctx->reader->itemidx - nitems_before - 1) < expected_items) {
		if (ctx->reader->itemidx >= ctx->reader->maxitems) {
			return CBOR_OVERRUN;
		}
		return CBOR_ILLEGAL;
	}

	return CBOR_SUCCESS;
}

/* TODO: Implement tag */
static cbor_error_t do_tag(struct parser_context *ctx)
{
	(void)ctx;
	return CBOR_INVALID;
}

static cbor_error_t do_float_and_other(struct parser_context *ctx)
{
	cbor_error_t err = CBOR_SUCCESS;
	cbor_item_data_t type = CBOR_ITEM_FLOAT;
	size_t size = (size_t)ctx->following_bytes;

	if (ctx->following_bytes <= 1 &&
			ctx->following_bytes != (uint8_t)CBOR_INDEFINITE_VALUE) {
		type = CBOR_ITEM_SIMPLE_VALUE;
	}
	set_item(ctx, type, ctx->reader->msgidx, size);

	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		ctx->reader->msgidx++;
		ctx->reader->itemidx++;
		return CBOR_BREAK;
	} else if (!has_valid_following_bytes(ctx, &err)) {
		return err;
	}

	ctx->reader->msgidx += size + 1;
	ctx->reader->itemidx++;

	return err;
}

cbor_error_t cbor_parse(cbor_reader_t *reader, void const *msg, size_t msgsize,
		size_t *nitems_parsed)
{
	assert(reader->items != NULL);
	reader->itemidx = 0;

	reader->msg = (uint8_t const *)msg;
	reader->msgsize = msgsize;
	reader->msgidx = 0;

	struct parser_context ctx = {
		.reader = reader,
		.dry_run = false,
	};

	cbor_error_t err = parse(&ctx, reader->maxitems);

	if (err == CBOR_SUCCESS && reader->msgidx < reader->msgsize) {
		err = CBOR_OVERRUN;
	}

	if (nitems_parsed != NULL) {
		*nitems_parsed = reader->itemidx;
	}

	return err;
}

cbor_error_t cbor_count_items(void const *msg, size_t msgsize,
		size_t *nitems_counted)
{
	cbor_reader_t reader = {
		.msg = (uint8_t const *)msg,
		.msgsize = msgsize,
		.msgidx = 0,
		.items = NULL,
		.itemidx = 0,
		.maxitems = (size_t)-1,
	};

	struct parser_context ctx = {
		.reader = &reader,
		.dry_run = true,
	};

	/* Use reader.msgsize as top-level maxitems in dry-run counting.
	 * reader.maxitems == (size_t)-1 conflicts with BREAK sentinel behavior
	 * and can terminate early. msgsize is a safe upper bound; long-term,
	 * split maxitems and break policy in parse(). */
	cbor_error_t err = parse(&ctx, reader.msgsize);

	if (err == CBOR_SUCCESS && reader.msgidx < reader.msgsize) {
		err = CBOR_OVERRUN;
	}

	if (nitems_counted != NULL) {
		*nitems_counted = reader.itemidx;
	}

	return err;
}
