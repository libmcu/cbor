#include "cbor/parser.h"
#include <stdbool.h>

#if !defined(MIN)
#define MIN(a, b)				(((a) > (b))? (b) : (a))
#endif
#if !defined(assert)
#define assert(expr)
#endif

struct decode_context {
	cbor_parser_t *parser;

	cbor_item_t *items;
	size_t itemidx;
	size_t maxitems;

	uint8_t major_type;
	uint8_t additional_info;
	uint8_t following_bytes;

	uint8_t recursion_depth;
	_Static_assert(CBOR_RECURSION_MAX_LEVEL < 256, "");

};

typedef cbor_error_t (*major_type_callback_t)(struct decode_context *ctx);

static cbor_error_t do_integer(struct decode_context *ctx);
static cbor_error_t do_string(struct decode_context *ctx);
static cbor_error_t do_recursive(struct decode_context *ctx);
static cbor_error_t do_tag(struct decode_context *ctx);
static cbor_error_t do_float_and_other(struct decode_context *ctx);

/* 8 callbacks for 3-bit major type */
static const major_type_callback_t callbacks[] = {
	do_integer,		/* 0: unsigned integer */
	do_integer,		/* 1: negative integer */
	do_string,		/* 2: byte string */
	do_string,		/* 3: text string encoded as utf-8 */
	do_recursive,		/* 4: array */
	do_recursive,		/* 5: map */
	do_tag,			/* 6: tag */
	do_float_and_other,	/* 7: float, simple value, and break */
};
_Static_assert(sizeof(callbacks) == 8 * sizeof(major_type_callback_t), "");

static bool has_valid_following_bytes(const struct decode_context *ctx,
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
			> ctx->parser->msgsize - ctx->parser->msgidx) {
		*err = CBOR_ILLEGAL;
		return false;
	}

	return true;
}

static size_t go_get_item_length(struct decode_context *ctx)
{
	uint64_t len = 0;
	size_t offset = 0;

	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		len = (uint64_t)CBOR_INDEFINITE_VALUE;
	} else if (ctx->following_bytes == 0) {
		len = ctx->additional_info;
	} else {
		const uint8_t *msg = &ctx->parser->msg[ctx->parser->msgidx];
		cbor_copy((uint8_t *)&len, &msg[1], ctx->following_bytes);
		offset = ctx->following_bytes;
	}

	ctx->parser->msgidx += offset + 1;

	return (size_t)len;
}

static cbor_error_t parse(struct decode_context *ctx, size_t maxitems)
{
	cbor_error_t err = CBOR_SUCCESS;

	if (++ctx->recursion_depth > CBOR_RECURSION_MAX_LEVEL) {
		return CBOR_EXCESSIVE;
	}

	for (size_t i = 0; i < maxitems && ctx->itemidx < ctx->maxitems
			&& ctx->parser->msgidx < ctx->parser->msgsize; i++) {
		uint8_t val = ctx->parser->msg[ctx->parser->msgidx];
		ctx->major_type = get_cbor_major_type(val);
		ctx->additional_info = get_cbor_additional_info(val);
		ctx->following_bytes =
			cbor_get_following_bytes(ctx->additional_info);

		if (!has_valid_following_bytes(ctx, &err)) {
			break;
		}

		err = callbacks[ctx->major_type](ctx);

		if (err == CBOR_BREAK) {
			if (ctx->parser->msgidx == ctx->parser->msgsize) {
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

static cbor_error_t do_integer(struct decode_context *ctx)
{
	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		return CBOR_ILLEGAL;
	}

	cbor_item_t *item = &ctx->items[ctx->itemidx];
	item->type = CBOR_ITEM_INTEGER;
	item->size = (size_t)(ctx->following_bytes + !ctx->following_bytes);
	item->offset = ctx->parser->msgidx;

	ctx->parser->msgidx += ctx->following_bytes + 1;
	ctx->itemidx++;

	return CBOR_SUCCESS;
}

static cbor_error_t do_string(struct decode_context *ctx)
{
	cbor_item_t *item = &ctx->items[ctx->itemidx];
	size_t len = go_get_item_length(ctx);

	item->type = CBOR_ITEM_STRING;
	item->size = len;
	item->offset = ctx->parser->msgidx;

	if (len == (size_t)CBOR_INDEFINITE_VALUE) {
		ctx->itemidx++;
		return parse(ctx, ctx->maxitems - ctx->itemidx);
	}
	if (len > ctx->parser->msgsize - ctx->parser->msgidx) {
		return CBOR_ILLEGAL;
	}

	ctx->parser->msgidx += len;
	ctx->itemidx++;

	return CBOR_SUCCESS;
}

static cbor_error_t do_recursive(struct decode_context *ctx)
{
	size_t current_item_index = ctx->itemidx;
	cbor_item_t *item = &ctx->items[current_item_index];
	size_t len = go_get_item_length(ctx);

	item->type = (cbor_item_data_t)(ctx->major_type - 1);
	item->offset = ctx->parser->msgidx;
	item->size = len;
	if (len != (size_t)CBOR_INDEFINITE_VALUE &&
			len > ctx->parser->msgsize - ctx->parser->msgidx) {
		return CBOR_ILLEGAL;
	}

	ctx->itemidx++;

	return parse(ctx, MIN(len, ctx->maxitems - ctx->itemidx));
}

/* TODO: Implement tag */
static cbor_error_t do_tag(struct decode_context *ctx)
{
	(void)ctx;
	return CBOR_INVALID;
}

static cbor_error_t do_float_and_other(struct decode_context *ctx)
{
	cbor_item_t *item = &ctx->items[ctx->itemidx];
	cbor_error_t err = CBOR_SUCCESS;

	item->type = CBOR_ITEM_FLOAT_AND_SIMPLE_VALUE;
	item->size = (size_t)ctx->following_bytes + !ctx->following_bytes;
	item->offset = ctx->parser->msgidx;

	if (ctx->following_bytes == (uint8_t)CBOR_INDEFINITE_VALUE) {
		ctx->parser->msgidx++;
		ctx->itemidx++;
		return CBOR_BREAK;
	}
	if (!has_valid_following_bytes(ctx, &err)) {
		return err;
	}

	ctx->parser->msgidx += item->size + 1;
	ctx->itemidx++;

	return err;
}

cbor_error_t cbor_parse(cbor_parser_t *parser,
		cbor_item_t *items, size_t maxitems, size_t *items_parsed)
{
	struct decode_context ctx = {
		.parser = parser,
		.items = items,
		.itemidx = 0,
		.maxitems = maxitems,
	};

	cbor_error_t err = parse(&ctx, maxitems);

	if (err == CBOR_SUCCESS && parser->msgidx < parser->msgsize) {
		err = CBOR_OVERRUN;
	}

	if (items_parsed != NULL) {
		*items_parsed = ctx.itemidx;
	}

	return err;
}

cbor_error_t cbor_parser_init(cbor_parser_t *parser,
		const void *msg, size_t msgsize)
{
	if (parser == NULL || msg == NULL) {
		return CBOR_INVALID;
	}

	parser->msg = (const uint8_t *)msg;
	parser->msgsize = msgsize;
	parser->msgidx = 0;

	return CBOR_SUCCESS;
}
