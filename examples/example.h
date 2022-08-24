#ifndef CBOR_EXAMPLE_H
#define CBOR_EXAMPLE_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* User-defined Data Type */
struct udt {
	uint32_t time;
	uint8_t type;
	uint8_t uuid[16];
	struct {
	     struct {
		     float x;
		     float y;
		     float z;
	     } acc;
	     struct {
		     float x;
		     float y;
		     float z;
	     } gyro;
	} data;
};

typedef void (*example_writer)(void const *data, size_t datasize);

void simple_example(example_writer print);
void complex_example(void const *data, size_t datasize, void *udt);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_EXAMPLE_H */
