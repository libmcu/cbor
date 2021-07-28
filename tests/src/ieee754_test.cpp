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

TEST(IEEE754, t) {
}
