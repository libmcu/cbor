#include "CppUTest/TestHarness.h"

#include <cstdio>
#include <string>

static std::string run_command(const char *command)
{
	std::string output;
	char buffer[256];
	FILE *pipe = popen(command, "r");

	CHECK_TEXT(pipe != NULL, "popen failed");
	while (fgets(buffer, (int)sizeof(buffer), pipe) != NULL) {
		output += buffer;
	}
	pclose(pipe);

	return output;
}

TEST_GROUP(ToolItemCount){};

TEST(ToolItemCount, ShouldPrintSuccessForValidDefiniteMap)
{
	std::string out = run_command(
		"python3 ../tools/cbor_item_count.py --hex \"A1 61 61 01\"");

	STRCMP_CONTAINS("error: CBOR_SUCCESS", out.c_str());
	STRCMP_CONTAINS("items: 3", out.c_str());
}

TEST(ToolItemCount, ShouldPrintIllegalForTruncatedDefiniteMap)
{
	std::string out = run_command(
		"python3 ../tools/cbor_item_count.py --hex \"A1 61 61\"");

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}
