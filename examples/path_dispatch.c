/*
 * SPDX-FileCopyrightText: 2026 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * Path-based dispatch example.
 *
 * This example demonstrates how to use cbor_unmarshal() with the path-based
 * API to precisely target leaf nodes in arbitrarily nested CBOR structures.
 *
 * The CBOR_PATH macro and CBOR_STR_SEG / CBOR_INT_SEG / CBOR_IDX_SEG helpers
 * build a cbor_parser whose path is verified against the current position in
 * the tree before invoking the registered callback.  Parsers whose paths do
 * not match are silently skipped.
 *
 * CBOR structure used in each sub-example:
 *
 * depth1 – flat map  {  "name": "Alice" }
 *
 * depth2 – sibling key conflict
 *   { "dev":  { "id": "DEV_BA",  "url": "http://dev"  },
 *     "prod": { "id": "PRD_BA",  "url": "http://prod" } }
 *   "id" and "url" appear under different parents; the dispatcher calls
 *   the correct handler for each path without manual parent tracking.
 *
 * depth3_arr – array of maps
 *   { "items": [ { "name": "foo" }, { "name": "bar" } ] }
 *   Array elements are addressed via CBOR_IDX_SEG.
 *
 * depth4 – deeply nested config
 *   { "config": { "network": { "wifi":    { "ssid": "MyNet",
 *                                           "pass": "secret" },
 *                              "eth":     { "ip":   "10.0.0.1" } } } }
 */

#include "cbor/helper.h"
#include "cbor/decoder.h"
#include <stdio.h>
#include <string.h>

/* depth1 – flat map */
static void on_name(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	char *buf = (char *)arg;
	cbor_decode(reader, item, buf, 32);
}

static void depth1_example(void)
{
	/* {"name": "Alice"}
	 * 0xA1 0x64 n a m e 0x65 A l i c e */
	const uint8_t msg[] = {
		0xA1, 0x64, 0x6E, 0x61, 0x6D, 0x65,
		0x65, 0x41, 0x6C, 0x69, 0x63, 0x65
	};
	const struct cbor_path_segment path_name[] = {
		CBOR_STR_SEG("name")
	};
	const struct cbor_parser parsers[] = {
		CBOR_PATH(path_name, on_name),
	};

	cbor_reader_t reader;
	cbor_item_t items[8];
	char name[32] = {0};

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), name);

	printf("[depth1] name = %s\n", name);   /* "Alice" */
}

/* depth2 – same key under different parents */
struct endpoint {
	char id[16];
	char url[32];
};

struct endpoints {
	struct endpoint dev;
	struct endpoint prod;
};

static void on_dev_id(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct endpoints *ep = (struct endpoints *)arg;
	cbor_decode(reader, item, ep->dev.id, sizeof(ep->dev.id));
}

static void on_dev_url(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct endpoints *ep = (struct endpoints *)arg;
	cbor_decode(reader, item, ep->dev.url, sizeof(ep->dev.url));
}

static void on_prod_id(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct endpoints *ep = (struct endpoints *)arg;
	cbor_decode(reader, item, ep->prod.id, sizeof(ep->prod.id));
}

static void on_prod_url(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct endpoints *ep = (struct endpoints *)arg;
	cbor_decode(reader, item, ep->prod.url, sizeof(ep->prod.url));
}

static void depth2_example(void)
{
	/* {"dev":  {"id": "DEV_BA", "url": "http://dev"},
	 *  "prod": {"id": "PRD_BA", "url": "http://prod"}} */
	const uint8_t msg[] = {
		0xA2,
		  0x63, 'd','e','v',
		  0xA2,
		    0x62, 'i','d',
		    0x66, 'D','E','V','_','B','A',
		    0x63, 'u','r','l',
		    0x6A, 'h','t','t','p',':','/','/','d','e','v',
		  0x64, 'p','r','o','d',
		  0xA2,
		    0x62, 'i','d',
		    0x66, 'P','R','D','_','B','A',
		    0x63, 'u','r','l',
		    0x6B, 'h','t','t','p',':','/','/','p','r','o','d',
	};

	const struct cbor_path_segment path_dev_ba[] = {
		CBOR_STR_SEG("dev"), CBOR_STR_SEG("id")
	};
	const struct cbor_path_segment path_dev_url[] = {
		CBOR_STR_SEG("dev"), CBOR_STR_SEG("url")
	};
	const struct cbor_path_segment path_prod_ba[] = {
		CBOR_STR_SEG("prod"), CBOR_STR_SEG("id")
	};
	const struct cbor_path_segment path_prod_url[] = {
		CBOR_STR_SEG("prod"), CBOR_STR_SEG("url")
	};

	const struct cbor_parser parsers[] = {
		CBOR_PATH(path_dev_ba,   on_dev_id),
		CBOR_PATH(path_dev_url,  on_dev_url),
		CBOR_PATH(path_prod_ba,  on_prod_id),
		CBOR_PATH(path_prod_url, on_prod_url),
	};

	cbor_reader_t reader;
	cbor_item_t items[32];
	struct endpoints ep;
	memset(&ep, 0, sizeof(ep));

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), &ep);

	printf("[depth2] dev.id=%s dev.url=%s\n", ep.dev.id, ep.dev.url);
	printf("[depth2] prod.id=%s prod.url=%s\n", ep.prod.id, ep.prod.url);
}

/* depth3_arr – array of maps, addressed by index */
static void on_item0_name(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	char (*names)[16] = (char (*)[16])arg;
	cbor_decode(reader, item, names[0], 16);
}

static void on_item1_name(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	char (*names)[16] = (char (*)[16])arg;
	cbor_decode(reader, item, names[1], 16);
}

static void depth3_arr_example(void)
{
	/* {"items": [{"name": "foo"}, {"name": "bar"}]} */
	const uint8_t msg[] = {
		0xA1,
		  0x65, 'i','t','e','m','s',
		  0x82,
		    0xA1, 0x64, 'n','a','m','e', 0x63, 'f','o','o',
		    0xA1, 0x64, 'n','a','m','e', 0x63, 'b','a','r',
	};

	const struct cbor_path_segment path_item0_name[] = {
		CBOR_STR_SEG("items"), CBOR_IDX_SEG(0), CBOR_STR_SEG("name")
	};
	const struct cbor_path_segment path_item1_name[] = {
		CBOR_STR_SEG("items"), CBOR_IDX_SEG(1), CBOR_STR_SEG("name")
	};

	const struct cbor_parser parsers[] = {
		CBOR_PATH(path_item0_name, on_item0_name),
		CBOR_PATH(path_item1_name, on_item1_name),
	};

	cbor_reader_t reader;
	cbor_item_t items[16];
	char names[2][16];
	memset(names, 0, sizeof(names));

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), names);

	printf("[depth3_arr] items[0].name=%s items[1].name=%s\n",
			names[0], names[1]);
}

/* depth4 – deeply nested config */
struct net_cfg {
	char wifi_ssid[32];
	char wifi_pass[32];
	char eth_ip[16];
};

static void on_wifi_ssid(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct net_cfg *cfg = (struct net_cfg *)arg;
	cbor_decode(reader, item, cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
}

static void on_wifi_pass(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct net_cfg *cfg = (struct net_cfg *)arg;
	cbor_decode(reader, item, cfg->wifi_pass, sizeof(cfg->wifi_pass));
}

static void on_eth_ip(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)parser;
	struct net_cfg *cfg = (struct net_cfg *)arg;
	cbor_decode(reader, item, cfg->eth_ip, sizeof(cfg->eth_ip));
}

static void depth4_example(void)
{
	/* {"config": {"network": {"wifi": {"ssid": "MyNet", "pass": "secret"},
	 *                         "eth":  {"ip": "10.0.0.1"}}}} */
	static const uint8_t msg[] = {
		0xA1,
		  0x66, 'c','o','n','f','i','g',
		  0xA1,
		    0x67, 'n','e','t','w','o','r','k',
		    0xA2,
		      0x64, 'w','i','f','i',
		      0xA2,
		        0x64, 's','s','i','d',
		        0x65, 'M','y','N','e','t',
		        0x64, 'p','a','s','s',
		        0x66, 's','e','c','r','e','t',
		      0x63, 'e','t','h',
		      0xA1,
		        0x62, 'i','p',
		        0x68, '1','0','.','0','.','0','.','1',
	};

	static const struct cbor_path_segment path_wifi_ssid[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("wifi"),   CBOR_STR_SEG("ssid")
	};
	static const struct cbor_path_segment path_wifi_pass[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("wifi"),   CBOR_STR_SEG("pass")
	};
	static const struct cbor_path_segment path_eth_ip[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("eth"),    CBOR_STR_SEG("ip")
	};

	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_wifi_ssid, on_wifi_ssid),
		CBOR_PATH(path_wifi_pass, on_wifi_pass),
		CBOR_PATH(path_eth_ip,    on_eth_ip),
	};

	cbor_reader_t reader;
	cbor_item_t items[32];
	struct net_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), &cfg);

	printf("[depth4] wifi.ssid=%s wifi.pass=%s eth.ip=%s\n",
			cfg.wifi_ssid, cfg.wifi_pass, cfg.eth_ip);
}

void path_dispatch_example(void)
{
	depth1_example();
	depth2_example();
	depth3_arr_example();
	depth4_example();
}
