#include "catch.hpp"
#include "patched_file.h"
#include "replacer.h"
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

    int line = 3;
    int col = 11; // The third [l]ine
    auto offs = lineColToOffset(pf.contents(), line, col);

    int l0, c0;
    std::tie(l0, c0) = offsetToLineCol(pf.contents(), offs);

    std::cout << "OFFS: " << offs << "L" << l0 << " C" << c0 << "\n";

    REQUIRE(offs == 30);
    REQUIRE(line == l0);
    REQUIRE(col == c0);

    pf.patch(lineOffsets[0] + 8, 0, "\n\n");
    pf.patch(lineOffsets[2] + 4, 5, "3rd");
    pf.patch(lineOffsets[1] + 9, 1, "second");
    pf.patch(lineOffsets[0], 0, "(Almost) ");
    pf.patch(lineOffsets[3], 0, "New contents for fourth line");
    pf.flush();

    auto newOffs = pf.translateOffset(offs); // line, col);
    std::tie(line, col) = offsetToLineCol(pf.contents(), newOffs);

    REQUIRE(line == 5);
    REQUIRE(col == 9);

    auto contents = readFile("temp.dat");
    REQUIRE(
        std::string(contents.begin(), contents.end()) ==
        "(Almost) line one\n\n\nOur line second\nThe 3rd line\nNew contents for fourth line\nThe actual 5th line\n"s);
}

TEST_CASE("replacer", "")
{
    Replacer replacer;

    copyFileToFrom("tempfile0.txt", "testfile.txt");
    copyFileToFrom("tempfile1.txt", "testfile.txt");

    replacer.applyReplacement({"tempfile0.txt", 70, 4, "REPLACEMENT"});
    replacer.appendToLine("tempfile0.txt", 12, " // COMMENT");
    replacer.applyReplacement({"tempfile0.txt", 154, 0, "NEW "});
    replacer.applyReplacement({"tempfile0.txt", 201, 1, "RETURN_VALUE"});

    replacer.applyReplacement({"tempfile1.txt", 201, 1, "RETURN_VALUE"});
    replacer.applyReplacement({"tempfile1.txt", 154, 0, "NEW "});
    replacer.appendToLine("tempfile1.txt", 12, " // COMMENT");
    replacer.applyReplacement({"tempfile1.txt", 70, 4, "REPLACEMENT"});
}
