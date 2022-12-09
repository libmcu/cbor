/*
 * SPDX-FileCopyrightText: 2022 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/cbor.h"

#define SSID_MAXLEN		32
#define PASS_MAXLEN		32
#define BSSID_LEN		6

struct aws_request {
	uint8_t type;
	uint8_t nr_networks;
	uint16_t scan_timeout_msec;
	uint8_t ssid[SSID_MAXLEN];
	uint8_t ssid_len;
	uint8_t passwd[PASS_MAXLEN];
	uint8_t passwd_len;
	uint8_t bssid[BSSID_LEN];
	uint8_t bssid_len;
};

static void parse_integer(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct aws_request *p = (struct aws_request *)arg;

	if (strcmp(parser->key, "w") == 0) {
		cbor_decode(reader, item, &p->type, sizeof(p->type));
	} else if (strcmp(parser->key, "h") == 0) {
		cbor_decode(reader, item,
				&p->nr_networks, sizeof(p->nr_networks));
	} else if (strcmp(parser->key, "t") == 0) {
		cbor_decode(reader, item, &p->scan_timeout_msec,
				sizeof(p->scan_timeout_msec));
	}
}

static void parse_string(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	struct aws_request *p = (struct aws_request *)arg;

	if (strcmp(parser->key, "r") == 0) {
		cbor_decode(reader, item, p->ssid, sizeof(p->ssid));
		p->ssid_len = item->size;
	} else if (strcmp(parser->key, "m") == 0) {
		cbor_decode(reader, item, p->passwd, sizeof(p->passwd));
		p->passwd_len = item->size;
	}
}

static const struct cbor_parser parsers[] = {
	{ .key = "w", .run = parse_integer }, /* message type */
	{ .key = "h", .run = parse_integer }, /* number of networks */
	{ .key = "t", .run = parse_integer }, /* scan timeout */
	{ .key = "r", .run = parse_string },  /* ssid */
	{ .key = "m", .run = parse_string },  /* password */
};

static void on_aws_request(const void *msg, uint16_t msglen)
{
	struct aws_request aws_request;

	cbor_reader_t reader;
	cbor_item_t items[32];

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, msglen, &aws_request);
}
