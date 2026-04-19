/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "helper_inline_decl_test.h"

#include "cbor/cbor.h"

static void on_certificate(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct helper_inline_decl_counts *counts = (struct helper_inline_decl_counts *)arg;

	(void)reader;
	(void)parser;
	(void)item;

	counts->certificate++;
}

static void on_private_key(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct helper_inline_decl_counts *counts = (struct helper_inline_decl_counts *)arg;

	(void)reader;
	(void)parser;
	(void)item;

	counts->private_key++;
}

static void on_dev_url(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct helper_inline_decl_counts *counts = (struct helper_inline_decl_counts *)arg;

	(void)reader;
	(void)parser;
	(void)item;

	counts->dev_url++;
}

static void on_prod_url(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct helper_inline_decl_counts *counts = (struct helper_inline_decl_counts *)arg;

	(void)reader;
	(void)parser;
	(void)item;

	counts->prod_url++;
}

bool helper_inline_decl_dispatch_depth1(const uint8_t *msg, size_t msglen,
		struct helper_inline_decl_counts *counts)
{
	cbor_reader_t reader;
	cbor_item_t items[32];
	CBOR_PATH_INLINE_DECL(parser_certificate, on_certificate,
			CBOR_STR_SEG("certificate"));
	CBOR_PATH_INLINE_DECL(parser_private_key, on_private_key,
			CBOR_STR_SEG("privateKey"));
	const struct cbor_parser parsers[] = {
		parser_certificate,
		parser_private_key,
	};

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(items[0]));

	return cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(parsers[0]),
			msg, msglen, counts);
}

bool helper_inline_decl_dispatch_depth2(const uint8_t *msg, size_t msglen,
		struct helper_inline_decl_counts *counts)
{
	cbor_reader_t reader;
	cbor_item_t items[32];
	CBOR_PATH_INLINE_DECL(parser_dev_url, on_dev_url,
			CBOR_STR_SEG("dev"), CBOR_STR_SEG("url"));
	CBOR_PATH_INLINE_DECL(parser_prod_url, on_prod_url,
			CBOR_STR_SEG("prod"), CBOR_STR_SEG("url"));
	const struct cbor_parser parsers[] = {
		parser_dev_url,
		parser_prod_url,
	};

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(items[0]));

	return cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(parsers[0]),
			msg, msglen, counts);
}
