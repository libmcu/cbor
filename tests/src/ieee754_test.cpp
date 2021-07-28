#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "cbor/ieee754.h"

TEST_GROUP(IEEE754) {
	void setup(void) {
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionZeroGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionIndefinteGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionNaNGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenHalfPrecisionSingleValueGiven) {
}
TEST(IEEE754, ShouldKeepSingle_WhenSinglePrecisionGiven) {
}

TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionZeroGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionIndefinteGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionNaNGiven) {
}
TEST(IEEE754, ShouldConvertToHalf_WhenHalfPrecisionDoubleValueGiven) {
}
TEST(IEEE754, ShouldConvertToSingle_WhenSinglePrecisionDoubleValueGiven) {
}
TEST(IEEE754, ShouldKeepDouble_WhenDoublePrecisionGiven) {
}
