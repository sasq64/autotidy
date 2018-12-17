#include "catch.hpp"
#include "patched_file.h"
#include "utils.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace std::string_literals;

TEST_CASE("patched_file", "")
{
    std::vector<std::string> testData = {"line one\n", "Our line 2\n",
                                         "The third line\n", "\n",
                                         "The actual 5th line\n"};

    std::vector<size_t> lineOffsets;
    std::string output;
    for (auto const& line : testData) {
        lineOffsets.push_back(output.size());
        output += line;
    }

    writeFile("temp.dat", output);

    PatchedFile pf{"temp.dat"};
    pf.patch(lineOffsets[2] + 4, 5, "3rd");
    pf.patch(lineOffsets[1] + 9, 1, "second");
    pf.patch(lineOffsets[0], 0, "(Almost) ");
    pf.patch(lineOffsets[3], 0, "New contents for fourth line");
    pf.flush();

    auto contents = readFile("temp.dat");
    REQUIRE(
        std::string(contents.begin(), contents.end()) ==
        "(Almost) line one\nOur line second\nThe 3rd line\nNew contents for fourth line\nThe actual 5th line\n"s);
}

TEST_CASE("source_file", "")
{
    copyFile("temp.cpp", "convert.cpp");

    PatchedFile pf{"temp.cpp"};
    pf.patch(549, 40, "(char i : str)");
    pf.patch(604, 6, "i");
    pf.flush();

    auto contents = readFile("temp.cpp");
    std::puts(std::string(contents.begin(), contents.end()).c_str());
}
