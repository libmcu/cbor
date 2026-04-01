#include "CppUTest/TestHarness.h"

#include <array>
#include <cstdlib>
#include <stdio.h>
#include <string>

#if defined(CBOR_PROJECT_ROOT)
static constexpr const char *kProjectRoot = CBOR_PROJECT_ROOT;
#else
static constexpr const char *kProjectRoot = "..";
#endif

static std::string get_script_path(void)
{
	return std::string(kProjectRoot) + "/tools/cbor_item_count.py";
}

static bool can_run_item_count_tool(void)
{
	if (std::system("command -v python3 >/dev/null 2>&1") != 0) {
		return false;
	}

	FILE *fp = fopen(get_script_path().c_str(), "r");
	if (fp == nullptr) {
		return false;
	}

	fclose(fp);
	return true;
}

static bool require_item_count_tool(void)
{
	if (can_run_item_count_tool()) {
		return true;
	}

	fprintf(stderr,
		"Skipping ToolItemCount tests: python3 or tools/cbor_item_count.py "
		"is not available\n");
	return false;
}

static std::string run_command(const char *command)
{
	std::string output;
	std::array<char, 256> buffer{};
	FILE *pipe = popen(command, "r");

	CHECK_TEXT(pipe != nullptr, "popen failed");
	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
	       nullptr) {
		output += buffer.data();
	}
	pclose(pipe);

	return output;
}

TEST_GROUP(ToolItemCount){};

TEST(ToolItemCount, ShouldPrintSuccessForValidDefiniteMap)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"A1 61 61 01\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_SUCCESS", out.c_str());
	STRCMP_CONTAINS("items: 3", out.c_str());
}

TEST(ToolItemCount, ShouldPrintIllegalForTruncatedDefiniteMap)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"A1 61 61\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}

TEST(ToolItemCount,
	     ShouldPrintSuccessWhenDefiniteArrayContainsIndefiniteTextString)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(("python3 \"" + get_script_path() +
				       "\" --hex \"81 7f 61 61 ff\"")
				      .c_str());

	STRCMP_CONTAINS("error: CBOR_SUCCESS", out.c_str());
	STRCMP_CONTAINS("items: 4", out.c_str());
}

TEST(ToolItemCount,
     ShouldPrintBreakWhenTopLevelIndefiniteArrayContainsNestedIndefiniteArray)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(("python3 \"" + get_script_path() +
				       "\" --hex \"9f 9f 01 ff 02 ff\"")
					      .c_str());

	STRCMP_CONTAINS("error: CBOR_BREAK", out.c_str());
	STRCMP_CONTAINS("items: 6", out.c_str());
}

TEST(ToolItemCount, ShouldPrintIllegalForMissingBreakInIndefiniteString)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"7f 61 61\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}

TEST(ToolItemCount, ShouldPrintIllegalForMissingBreakInIndefiniteArray)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"9f 01\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}

TEST(ToolItemCount, ShouldPrintIllegalForMissingBreakInIndefiniteMap)
{
	if (!require_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"bf 61 61 01\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}
