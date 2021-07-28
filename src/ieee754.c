/*
 * | precision | sign | exponent | mantissa | bias |
 * | --------- | ---- | -------- | -------- | ---- |
 * | half      | 1    |  5       | 10       |   15 |
 * | single    | 1    |  8       | 23       |  127 |
 * | double    | 1    | 11       | 52       | 1023 |
 *
 * ## Secial cases
 * | s |   e  | m  |     desc.     |
 * | - | -----| -- | ------------- |
 * | 0 |    0 |  0 | +0.0          |
 * | 1 |    0 |  0 | -0.0          |
 * | 0 |    0 | !0 | 0.m * 2^-126  |
 * | 1 |    0 | !0 | -0.m * 2^-126 |
 * | 0 | 0xff |  0 | indefinite    |
 * | 1 | 0xff |  0 | -indefinite   |
 * | X | 0xff | -1 | Quiet NaN     |
 * | X | 0xff | !0 | Signaling NaN |
 */

#include "cbor/ieee754.h"

