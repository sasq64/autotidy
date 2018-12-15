#include "path.h"
#define CATCH_CONFIG_MAIN
#include <catch.hpp>

using namespace utils;

TEST_CASE("Basic", "[path]")
{
    std::vector<path> paths = {
        { "my/basic/path" },
        { "/abs/path/no/filename/" },
        { "c:/win/path/with.extension" },
        { "relative\\win\\path\\" },
        { "d:drive-relative\\win\\path.exe" }
    };

    for(const auto& path : paths) {
        std::cout << path << std::endl;
        for(const auto& p : path)
            std::cout << "    " << p << std::endl;
    }

    REQUIRE(paths[0].is_absolute() == false);
    REQUIRE(paths[0].filename() == "path");

    REQUIRE(paths[1].filename() == "");
    REQUIRE(paths[2].extension() == ".extension");

    REQUIRE(paths[1] / paths[0] == "/abs/path/no/filename/my/basic/path");

    REQUIRE(paths[2].is_relative() == false);
    REQUIRE(paths[3].is_relative() == true);
    REQUIRE(paths[4].is_relative() == true);

    REQUIRE(paths[3] / "hey" == "relative\\win\\path\\hey");
    REQUIRE(paths[2].parent_path().parent_path().parent_path() == "c:\\");
    REQUIRE(paths[2].parent_path().parent_path().parent_path().parent_path().empty() == true);

    REQUIRE(paths[4].parent_path().parent_path().parent_path() == "d:");
}
