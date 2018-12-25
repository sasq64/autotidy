#include "autotidy.h"
#include <CLI/CLI.hpp>
#include <string>

using namespace std::string_literals;

int main(int argc, char** argv)
{
    CLI::App app{"autotidy"};

    std::string fixesFile;
    std::string filename;
    auto diffCommand = "diff -u {0} {1}"s;
    auto configFilename = ".clang-tidy"s;

    app.add_option("log", filename, "clang-tidy output file")->required();
    app.add_option("-c,--clang-tidy-config", configFilename,
                   "clang-tidy config file", true);
    app.add_option("-d,--diff-command", diffCommand,
                   "Command to use for performing diff", true);
    app.add_option("-f,--fixes-file", fixesFile,
                   "Exported fixes from clang-tidy");

    CLI11_PARSE(app, argc, argv);

    AutoTidy tidy{filename, configFilename, diffCommand, fixesFile};
    tidy.run();
}
