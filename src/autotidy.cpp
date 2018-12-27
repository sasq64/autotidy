#include "autotidy.h"
#include "replacer.h"
#include "utils.h"

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include <fmt/color.h>
#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <cstdio>
#include <regex>
#include <set>
#include <utility>

struct TidyError
{
    TidyError() = default;
    TidyError(int aNumber, std::string aCheck, int aLine, int aColumn,
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

void AutoTidy::saveConfig()
{
    std::ofstream outf{".clang-tidy"};
    for (auto const& line : confLines) {
        if (absl::StartsWith(line, "Checks:")) {
            outf << fmt::format("Checks: '*, -{}'\n",
                                absl::StrJoin(ignores, ", -"));
        } else {
            outf << line << "\n";
        }
    }
}

template <class Container, class T>
bool contains(Container const& c, const T& value)
{
    return c.count(value) > 0;
}

bool AutoTidy::handleError(const TidyError& e)
{
    if (ignores.count(e.check) > 0 || e.fileName.empty()) {
        return false;
    }
    if (skippedFiles.count(e.fileName) > 0) {
        return false;
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

    enum
    {
        RealName,
        TempName
    };
    std::map<std::string, std::string> tempFiles;

    // Make copies and add temporary files
    for (auto const& r : e.replacements) {
        auto temp = e.fileName + ".temp";
        if (!contains(tempFiles, r.path)) {
            tempFiles[r.path] = temp;
            fmt::print("Copy {} from {}\n", temp, r.path);
            replacer.copyFile(temp, r.path);
        }
        fmt::print("Patch {}\n", temp);
        replacer.applyReplacement({temp, r});
    }

    auto const separator = std::string(60, '-');

    bool hasPatch = !tempFiles.empty();

    for (auto const& p : tempFiles) {
        system(fmt::format(diffCommand, std::get<RealName>(p),
                           std::get<TempName>(p))
                   .c_str());
    }

    fmt::print(
        fmt::fg(fmt::color::cyan),
        "{}[t]odo marker, [i]gnore, [s/S]kip (file), [n/N]olint, [q]uit ? ",
        hasPatch ? "[a]pply patch, " : "");
    std::fflush(stdout);

    auto c = getch();
    fmt::print(fmt::bg(fmt::color::white) | fmt::fg(fmt::color::black), "[{}]",
               static_cast<char>(c));
    std::puts("");
    switch (c) {
    case 'a':
        for (auto const& f : tempFiles) {
            // Copy temporary -> real
            replacer.copyFile(std::get<RealName>(f), std::get<TempName>(f));
            replacer.removeFile(std::get<TempName>(f));
        }
        tempFiles.clear();
        break;
    case 'n':
        replacer.appendToLine(e.fileName, e.line, " //NOLINT");
        break;
    case 'N':
        replacer.appendToLine(e.fileName, e.line,
                              fmt::format(" //NOLINT({})", e.check));
        break;
    case 't':
        replacer.appendToLine(e.fileName, e.line,
                              fmt::format(" //TODO({})", e.check));
        break;
    case 'i':
        ignores.insert(e.check);
        saveConfig();
        break;
    case 's':
        break;
    case 'S':
        skippedFiles.insert(e.fileName);
        break;
    case 'q':
        return true;
    }

    fmt::print(fmt::fg(fmt::color::steel_blue), separator);
    std::puts("");
    for (auto const& f : tempFiles) {
        replacer.removeFile(std::get<TempName>(f));
    }

    return false;
}

void AutoTidy::run()
{
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
        WholeMatch,
        FilenameMatch,
        Filename,
        Line,
        Column,
        Type,
        Message,
        Check
    };

    std::regex errline{
        R"((([^:]+):(\d+):(\d+):)?\s*(\w+):\s*(.*)\[(.*)\])"};

    std::cmatch currentMatch;
    std::vector<std::string> text;
    TidyError error;
    std::vector<TidyError> errorList;
    std::ifstream logFile(filename);
    int no = 0;
    while (std::getline(logFile, line)) {
        if (std::regex_match(line.c_str(), currentMatch, errline)) {
            if (currentMatch[Type] == "note") {
                text.push_back(line);
                continue;
            }

            if (!error.error.empty()) {
                errorList.push_back(error);
                errorList.back().text = absl::StrJoin(text, "\n");
            }
            text.clear();

            error = {no++,
                     currentMatch[Check],
                     std::atoi(currentMatch[Line].str().c_str()),
                     std::atoi(currentMatch[Column].str().c_str()),
                     currentMatch[Filename],
                     currentMatch[Message]};
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
