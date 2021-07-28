#ifndef CBOR_IEEE754_H
#define CBOR_IEEE754_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

float ieee754_convert_half_to_single(uint16_t ihalf);
uint16_t ieee754_convert_single_to_half(float fsingle);
uint16_t ieee754_convert_double_to_half(double fdouble);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_IEEE754_H */
