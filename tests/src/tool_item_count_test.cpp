#include "CppUTest/TestHarness.h"

#include <array>
#include <cstdlib>
#include <cstdio>
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

	FILE *fp = std::fopen(get_script_path().c_str(), "r");
	if (fp == nullptr) {
		return false;
	}

	std::fclose(fp);
	return true;
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
	if (!can_run_item_count_tool()) {
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
	if (!can_run_item_count_tool()) {
		return;
	}

	std::string out = run_command(
		("python3 \"" + get_script_path() + "\" --hex \"A1 61 61\"")
			.c_str());

	STRCMP_CONTAINS("error: CBOR_ILLEGAL", out.c_str());
}
