#include "cbor/cbor.h"

#if !defined(assert)
#define assert(expr)
#endif

void cbor_reader_init(cbor_reader_t *reader, const void *msg, size_t msgsize)
{
	assert(parser != NULL);
	assert(msg != NULL);

	reader->msg = (const uint8_t *)msg;
	reader->msgsize = msgsize;
	reader->msgidx = 0;
}

void cbor_writer_init(cbor_writer_t *writer, void *buf, size_t bufsize)
{
	assert(parser != NULL);
	assert(msg != NULL);

	writer->buf = (uint8_t *)buf;
	writer->bufsize = bufsize;
	writer->bufidx = 0;
}
