#include <sys/stat.h>

#include "format.h"

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef UNIT_TEST

#include "catch.hpp"

TEST_CASE("utils::format", "format operations") {

    using namespace utils;
    using namespace std;

    int a = 20;
    const char *b = "test";
    string c = "case";

    string res = format("%x %03x %s %d %s", a, a, b, a, c);
    REQUIRE(res == "14 014 test 20 case");

    vector<int> v { 128, 129, 130, 131 };
    res = format("%02x", v);
    //TODO: Actual broken test
    //REQUIRE(res == "80 81 82 83");

    //auto s = make_slice(v, 1, 2);
    //res = format("%02x", s);
    //REQUIRE(res == "81 82");
}

#endif
