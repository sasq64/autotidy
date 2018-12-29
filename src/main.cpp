#include "autotidy.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <array>
#include <cstdio>
#include <string>

using namespace std::string_literals;

void pipeCommandToFile(std::string const& cmdLine, std::string const& outFile)
{
    using arr = std::array<uint8_t, 1024>;
    auto* fp = popen(cmdLine.c_str(), "r");
    auto* outfp = fopen(outFile.c_str(), "we");
    arr buf;
    while (feof(fp) == 0) {
        auto sz = fread(buf.data(), sizeof(arr::value_type), buf.size(), fp);
        fwrite(buf.data(), 1, sz, outfp);
    }
    fclose(outfp);
    pclose(fp);
}

int main(int argc, char** argv)
{
    CLI::App app{"autotidy"};

    std::string fixesFile = "fixes.yaml";
    std::string filename;
    std::string sourceFile;
    bool runClangTidy = false;
    auto clangTidy = "clang-tidy"s;
    auto diffCommand = "diff -u {0} {1}"s;
    auto configFilename = ".clang-tidy"s;

    app.add_option("-l,--log", filename, "clang-tidy output file");
    app.add_option("-s,--source", sourceFile, "Source file for clang-tidy");
    app.add_flag("-R,--run-clang-tidy", runClangTidy, "Run clang-tidy");
    app.add_option("-c,--clang-tidy-config", configFilename,
                   "clang-tidy config file", true);
    app.add_option("-d,--diff-command", diffCommand,
                   "Command to use for performing diff", true);
    app.add_option("-f,--fixes-file", fixesFile,
                   "Exported fixes from clang-tidy", true);

    CLI11_PARSE(app, argc, argv);

    if (runClangTidy) {
        if (filename.empty()) {
            filename = "tidy.log";
        }
        auto cmdLine = fmt::format("{} -export-fixes={} -header-filter=* {}",
                                   clangTidy, fixesFile, sourceFile);
        fmt::print("Running `{}`\n", cmdLine);
        pipeCommandToFile(cmdLine, filename);
    }

    AutoTidy tidy{filename, configFilename, diffCommand, fixesFile};
    tidy.run();
}
