#include "autotidy.h"
#include "path.h"
#include "utils.h"

#include <CLI/CLI.hpp>
#include <absl/strings/str_split.h>
#include <absl/types/optional.h>
#include <fmt/format.h>

#include <cstdio>
#include <string>

using namespace std::string_literals;

absl::optional<std::string> which(std::string const& executable)
{
    std::string path = getenv("PATH");
    for (auto const& pathSegment : absl::StrSplit(path, ':')) {
        auto candidate = utils::path(std::string(pathSegment)) / executable;
        if (utils::exists(candidate)) {
            return candidate;
        }
    }
    return absl::nullopt;
}

int main(int argc, char** argv)
{
    CLI::App app{"autotidy"};

    std::string filename;
    std::string sourceFile;
    std::string headerFilter;
    int headerLevel = 1;
    bool runClangTidy = false;
    auto fixesFile = "fixes.yaml"s;
    utils::path clangTidy; // = "clang-tidy"s;
    auto diffCommand = "diff -u {0} {1}"s;
    auto configFilename = ".clang-tidy"s;

    app.add_option("-l,--log", filename, "clang-tidy output file");
    app.add_option("-s,--source,source", sourceFile,
                   "Source file for clang-tidy");
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

    if (sourceFile.empty() && filename.empty()) {
        std::cout
            << "**Error: Need either a source file or a clang-tidy log.\n";
        return 0;
    }

    if (!sourceFile.empty()) {
        runClangTidy = true;
    }

    if (auto executable = which("clang-tidy")) {
        clangTidy = *executable;
    } else {
        std::cout << "**Error: clang-tidy not found.\n";
        return 0;
    }

    pipeCommandToFile(fmt::format("{} --version", clangTidy.string()),
                      ".temp-out");

    std::ifstream versionText(".temp-out");
    std::string line;
    while (std::getline(versionText, line)) {
        auto versionPos = line.find("version");
        if (versionPos != std::string::npos) {
            fmt::print("Found clang-tidy : {}\n", line.substr(versionPos));
            break;
        }
    }
    versionText.close();
    std::remove(".temp-out");

    // Create a .clang-tidy if none exists
    if (!utils::exists(".clang-tidy")) {
        AutoTidy tidy{filename, configFilename, diffCommand, fixesFile};
        auto cmdLine = fmt::format("{} -dump-config", clangTidy.string());
        pipeCommandToFile(cmdLine, ".clang-tidy");
        tidy.readConfig();
        tidy.setIgnores({});
        tidy.saveConfig();
    }

    if (runClangTidy) {

        auto fullPath = utils::resolve(sourceFile);

        while (headerLevel > 0) {
            fullPath = fullPath.parent_path();
            std::cout << fullPath << "\n";
            if (fullPath == "/") {
                fullPath = ".*";
                break;
            }
            headerLevel--;
        }

        if (headerFilter.empty()) {
            headerFilter = fullPath.string();
        }

        if (filename.empty()) {
            filename = "tidy.log";
        }

        utils::remove(filename);
        utils::remove(fixesFile);

        auto cmdLine = fmt::format("{} -export-fixes={} -header-filter='{}' {}",
                                   clangTidy.string(), fixesFile, headerFilter,
                                   sourceFile);
        fmt::print("Running `{}`\n", cmdLine);
        pipeCommandToFile(cmdLine, filename);
    }

    AutoTidy tidy{filename, configFilename, diffCommand, fixesFile};
    tidy.run();
}
