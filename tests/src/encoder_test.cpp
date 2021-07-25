#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "cbor/encoder.h"

TEST_GROUP(Encoder) {
	void setup(void) {
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(Encoder, t) {
}
