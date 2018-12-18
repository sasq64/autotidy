#include "patched_file.h"
#include "utils.h"

#include <absl/algorithm/container.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include <fmt/color.h>
#include <fmt/format.h>

#include <CLI/CLI.hpp>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdio>
#include <regex>
#include <set>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>
#include <utility>

using namespace std::string_literals;
struct Replacement
{
    Replacement(std::string aPath, size_t aOffset, size_t aLength,
                std::string aText)
        : path(std::move(aPath)), offset(aOffset), length(aLength),
          text(std::move(aText))
    {}
    std::string path;
    size_t offset;
    size_t length;
    std::string text;
};

struct Error
{
    Error() = default;
    Error(int aNumber, std::string aCheck, int aLine, int aColumn,
          std::string aFileName, std::string aError)
        : number(aNumber), check(std::move(aCheck)), line(aLine),
          column(aColumn), fileName(std::move(aFileName)),
          error(std::move(aError))
    {}
    int number = 0;
    std::string check;
    int line = 0;
    int column = 0;
    std::string fileName;
    std::string error;
    std::string text;
    std::vector<Replacement> replacements;
};

std::set<std::string> ignores;
std::vector<std::string> confLines;
std::string currDir;
std::string editCommand = "vim {0} \"+call cursor({1}, {2})\"";

void saveConfig()
{
    std::ofstream outf{".clang-tidy"};
    for (auto const& line : confLines) {
        if (absl::StartsWith(line, "Checks:")) {
            outf << fmt::format(
                "Checks: '*, -{}'\n",
                absl::StrJoin(ignores.begin(), ignores.end(), ", -"));
        } else {
            outf << line << "\n";
        }
    }
}

class Replacer
{
    int count = 0;

    std::map<std::string, PatchedFile> patchedFiles;

    // Files temporarily patches for the current fix
    std::map<std::string, PatchedFile> tempFiles;

public:
    std::vector<std::pair<std::string, std::string>> getFiles() const
    {
        std::vector<std::pair<std::string, std::string>> result;
        absl::c_transform(tempFiles, std::back_inserter(result),
                          [](auto const& p) {
                              return std::make_pair(p.first, p.second.fileName);
                          });
        return result;
    }

    void commit()
    {
        for (auto const& p : tempFiles) {
            auto const& originalName = p.first;
            auto const& tempPatched = p.second;
            copyFileToFrom(originalName, tempPatched.fileName);
            // Remember the patches we appliced since we may patch it again
            // in a later fix
            patchedFiles[originalName] = tempPatched;
        }
        done();
    }

    void done()
    {
        for (auto const& p : tempFiles) {
            std::remove(p.second.fileName.c_str());
        }
        tempFiles.clear();
        count = 0;
    }

    void applyReplacement(Replacement const& r)
    {
        auto& pf = tempFiles[r.path];
        if (pf.fileName.empty()) {

            auto it = patchedFiles.find(r.path);
            if (it != patchedFiles.end()) {
                // This file was patched earlier
                pf.patches = it->second.patches;
            }

            pf.fileName = fmt::format(".patchedFile{}", count++);
            copyFileToFrom(pf.fileName, r.path);
        }
        pf.patch(r.offset, r.length, r.text);
        pf.flush();
    }
};

Replacer replacer;

void handleError(const Error& e)
{
    if (ignores.count(e.check) > 0 || e.fileName.empty()) {
        return;
    }

    auto fn = e.fileName;
    if (absl::StartsWith(fn, currDir)) {
        fn = fn.substr(currDir.length());
    }

    fmt::print(fmt::fg(fmt::color::white), "\n#{} ", e.number);
    fmt::print(fmt::fg(fmt::color::gold), "{}", fn);
    fmt::print(fmt::fg(fmt::color::white), ":{}", e.line);
    fmt::print(fmt::fg(fmt::color::light_pink), " [{}]", e.check);
    fmt::print(fmt::fg(fmt::color::light_green), "\n{}\n", e.error);
    std::puts(e.text.c_str());

    for (auto const& r : e.replacements) {
        replacer.applyReplacement(r);
    }

    auto const& files = replacer.getFiles();

    bool hasPatch = !files.empty();

    bool done = false;

    auto separator = std::string(60, '-');
 
    while (!done) {

        for (auto const& p : files) {
            system(fmt::format("diff -u3 --color {} {}", p.first, p.second)
                       .c_str());
        }

        fmt::print(
            fmt::fg(fmt::color::cyan),
            hasPatch ? "[a]pply patch, [e]dit, [i]gnore, [s]kip, [n/N]olint ? "
                     : "[e]dit, [i]gnore, [s]kip, [n/N]olint ? ");
        std::fflush(stdout);

        auto c = getch();
        fmt::print(fmt::bg(fmt::color::white) | fmt::fg(fmt::color::black),
                   "[{}]", (char)c);
        std::puts("");
        switch (c) {
        case 'a':
            replacer.commit();
            break;
        case 'n':
            system(fmt::format("sed -i \"\" '{}s/$/ \\/\\/NOLINT/' {}", e.line,
                               e.fileName)
                       .c_str());
            done = true;
            break;
        case 'N':
            system(fmt::format("sed -i \"\" '{}s/$/ \\/\\/NOLINT({})/' {}",
                               e.line, e.check, e.fileName)
                       .c_str());
            done = true;
            break;
        case 'i':
            ignores.insert(e.check);
            saveConfig();
            done = true;
            break;
        case 'e':
            system(
                fmt::format(editCommand, e.fileName, e.line, e.column).c_str());
            break;
        case 's':
            done = true;
            break;
        }
    }
    fmt::print(fmt::fg(fmt::color::steel_blue), separator);
    std::puts("");
    replacer.done();
}
int main(int argc, char** argv)
{
    CLI::App app{"autotidy"};

    std::string fixesFile;
    std::string filename;
    auto configFileName = ".clang-tidy"s;
    app.add_option("log", filename, "clang-tidy output file")->required();
    app.add_option("-c,--clang-tidy-config", configFileName,
                   "clang-tidy config file", true);
    app.add_option("-e,--edit-command", editCommand,
                   "Command to use for editing file", true);
    app.add_option("-f,--fixes-file", fixesFile,
                   "Exported fixes from clang-tidy");

    CLI11_PARSE(app, argc, argv);

    currDir = currentDir();
    if (!absl::EndsWith(currDir, "/")) {
        currDir += "/";
    }

    std::ifstream configFile(".clang-tidy");

    std::string line;
    while (std::getline(configFile, line)) {
        // for (auto const& line : configFile.lines()) {
        if (absl::StartsWith(line, "Checks:")) {
            auto q0 = line.find_first_of('\'');
            auto q1 = line.find_last_of('\'');
            if (q1 > q0 && q0 != std::string::npos) {
                auto checks = line.substr(q0 + 1, q1 - q0 - 1);
                for (absl::string_view c : absl::StrSplit(checks, ",")) {
                    c = absl::StripLeadingAsciiWhitespace(c);
                    if (c.front() == '-') {
                        ignores.emplace(c.substr(1));
                    }
                }
            }
        }
        confLines.push_back(line);
    }

    enum
    {
        WHOLE_MATCH,
        FILENAME_MATCH,
        FILENAME,
        LINE,
        COLUMN,
        TYPE,
        MESSAGE,
        CHECK
    };

    std::regex errline{R"((([^:]+):(\d+):(\d+):)?\s*(\w+):\s*(.*)\[(.*)\])"};

    std::cmatch currentMatch;
    std::vector<std::string> text;
    Error error;
    std::vector<Error> errorList;
    std::ifstream logFile(filename);
    int no = 0;
    while (std::getline(logFile, line)) {
        if (std::regex_match(line.c_str(), currentMatch, errline)) {
            if (currentMatch[TYPE] == "note") {
                text.push_back(line);
                continue;
            }

            if (!error.error.empty()) {
                errorList.push_back(error);
                errorList.back().text = absl::StrJoin(text, "\n");
            }
            text.clear();

            error = {no++,
                     currentMatch[CHECK],
                     std::atoi(currentMatch[LINE].str().c_str()),
                     std::atoi(currentMatch[COLUMN].str().c_str()),
                     currentMatch[FILENAME],
                     currentMatch[MESSAGE]};
        } else {
            text.push_back(line);
        }
    }
    if (!error.error.empty()) {
        errorList.push_back(error);
        errorList.back().text = absl::StrJoin(text, "\n");
    }

    if (!fixesFile.empty()) {
        auto fixesData = readFile(fixesFile);
        bool inQuotes;
        for (size_t i = 0; i < fixesData.size(); i++) {
            if (fixesData[i] == '\'' && fixesData[i + 1] != '\'') {
                inQuotes = !inQuotes;
            }
            if (inQuotes && fixesData[i] == 10) {
                fixesData.insert(fixesData.begin() + i, 10);
                i++;
            }
        }

        YAML::Node fixes =
            YAML::Load(std::string(fixesData.begin(), fixesData.end()));
        size_t i = 0;
        for (auto const& d : fixes["Diagnostics"]) {
            for (auto const& r : d["Replacements"]) {
                errorList[i].replacements.emplace_back(
                    r["FilePath"].as<std::string>(), r["Offset"].as<int>(),
                    r["Length"].as<int>(),
                    r["ReplacementText"].as<std::string>());
            }
            i++;
        }
    }

    for (auto const& e : errorList) {
        handleError(e);
    }
}
