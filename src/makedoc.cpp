#include "utils.h"
#include "path.h"

#include <fmt/format.h>

#include <iostream>
#include <map>

std::string hexDump(std::vector<char> const& data) {
    std::string result;
    for(auto const& c : data) {
        result += fmt::format("{},", static_cast<int>(c));
    }
    return result;
}

int main(int  /*argc*/, char ** /*argv*/)
{
    std::puts("#include <map>\n#include <string>\nstd::map<std::string, std::string> manPages = {");
    for(auto const& p : utils::directory_iterator{"extra/clang-checks-doc"}) {
        pipeCommandToFile(fmt::format("rst2man.py {}", p.path().stem()), "temp.man");
        auto data = readFile("temp.man");
        fmt::print("{{ \"{}\", {{ {} }} }},\n", p.path().filename().string(), hexDump(data)); 
    }
    std::puts("};");
    return 0;
}
