#include "autotidy.h"
#include "path.h"
#include "utils.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <cstdio>
#include <string>

using namespace std::string_literals;

int main(int argc, char** argv)
{
    CLI::App app{"autotidy"};

    std::string fixesFile = "fixes.yaml";
    std::string filename;
    std::string sourceFile;
    std::string headerFilter;
    int headerLevel = 1;
    bool runClangTidy = false;
    auto clangTidy = "clang-tidy"s;
    auto diffCommand = "diff -u {0} {1}"s;
    auto configFilename = ".clang-tidy"s;

    app.add_option("-l,--log", filename, "clang-tidy output file");
    app.add_option("-s,--source", sourceFile, "Source file for clang-tidy");
    app.add_option("-F,--header-filter", headerFilter,
                   "Header filter for clang-tidy");
    app.add_option("-H,--header-strip", headerLevel,
                   "Header filter include level", true);
    app.add_option("-c,--clang-tidy-config", configFilename,
                   "clang-tidy config file", true);
    app.add_option("-d,--diff-command", diffCommand,
                   "Command to use for performing diff", true);
    app.add_option("-f,--fixes-file", fixesFile,
                   "Exported fixes from clang-tidy", true);

    CLI11_PARSE(app, argc, argv);

    if(sourceFile.empty() && filename.empty()) {
        std::cout << "**Error: Need either a source file or a clang-tidy log.\n";
        return 0;
    }

    if (!sourceFile.empty()) {
        runClangTidy = true;
    }

    if (runClangTidy) {

        auto fullPath = utils::resolve(sourceFile);

        while (headerLevel > 0) {
            fullPath = fullPath.parent_path();
            if (fullPath == "/") {
                fullPath = ".*";
                break;
            }
            headerLevel--;
        }

        if(headerFilter.empty()) {
            headerFilter = fullPath.string();
        }

        if (filename.empty()) {
            filename = "tidy.log";
        }

        ::remove(filename.c_str());
        ::remove(fixesFile.c_str());

        auto cmdLine =
            fmt::format("{} -export-fixes={} -header-filter={} {}", clangTidy,
                        fixesFile, headerFilter, sourceFile);
        fmt::print("Running `{}`\n", cmdLine);
        pipeCommandToFile(cmdLine, filename);
    }

    AutoTidy tidy{filename, configFilename, diffCommand, fixesFile};
    tidy.run();
}
