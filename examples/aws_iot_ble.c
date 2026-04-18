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

static void parse_type(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct aws_request *p = (struct aws_request *)arg;
	cbor_decode(reader, item, &p->type, sizeof(p->type));
}

static void parse_nr_networks(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct aws_request *p = (struct aws_request *)arg;
	cbor_decode(reader, item, &p->nr_networks, sizeof(p->nr_networks));
}

static void parse_scan_timeout(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct aws_request *p = (struct aws_request *)arg;
	cbor_decode(reader, item,
			&p->scan_timeout_msec, sizeof(p->scan_timeout_msec));
}

static void parse_ssid(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct aws_request *p = (struct aws_request *)arg;
	if (cbor_decode(reader, item, p->ssid, sizeof(p->ssid)) == CBOR_SUCCESS) {
		p->ssid_len = (uint8_t)item->size;
	}
}

static void parse_passwd(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct aws_request *p = (struct aws_request *)arg;
	if (cbor_decode(reader, item, p->passwd, sizeof(p->passwd)) == CBOR_SUCCESS) {
		p->passwd_len = (uint8_t)item->size;
	}
}

static const struct cbor_path_segment path_w[] = { CBOR_STR_SEG("w") };
static const struct cbor_path_segment path_h[] = { CBOR_STR_SEG("h") };
static const struct cbor_path_segment path_t[] = { CBOR_STR_SEG("t") };
static const struct cbor_path_segment path_r[] = { CBOR_STR_SEG("r") };
static const struct cbor_path_segment path_m[] = { CBOR_STR_SEG("m") };

static const struct cbor_parser parsers[] = {
	CBOR_PATH(path_w, parse_type),          /* message type */
	CBOR_PATH(path_h, parse_nr_networks),   /* number of networks */
	CBOR_PATH(path_t, parse_scan_timeout),  /* scan timeout */
	CBOR_PATH(path_r, parse_ssid),          /* ssid */
	CBOR_PATH(path_m, parse_passwd),        /* password */
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
