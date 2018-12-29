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

bool AutoTidy::handleError(const TidyError& e)
{
    static auto const separator = std::string(60, '-') + "\n"s;

    if (ignores.count(e.check) > 0 || e.fileName.empty()) {
        return false;
    }
    if (skippedFiles.count(e.fileName) > 0) {
        return false;
    }

    auto baseName = e.fileName;
    if (absl::StartsWith(baseName, currDir)) {
        baseName = baseName.substr(currDir.length());
    }

    fmt::print(fmt::fg(fmt::color::white), "\n#{} ", e.number);
    fmt::print(fmt::fg(fmt::color::gold), "{}", baseName);
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

    // Make copies and add the temporary files instead
    for (auto const& r : e.replacements) {
        auto temp = e.fileName + ".temp";
        if (!contains(tempFiles, r.path)) {
            tempFiles[r.path] = temp;
            replacer.copyFile(temp, r.path);
        }
        replacer.applyReplacement({temp, r});
    }

    bool hasPatch = !tempFiles.empty();

    // Show diff between patched temp files and original file
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
    for (auto const& f : tempFiles) {
        replacer.removeFile(std::get<TempName>(f));
    }

    return false;
}

void AutoTidy::readConfig()
{
    std::ifstream configFile(".clang-tidy");

    std::string line;
    while (std::getline(configFile, line)) {
        if (absl::StartsWith(line, "Checks:")) {
            auto firstQuote = line.find_first_of('\'');
            auto lastQuote = line.find_last_of('\'');
            if (lastQuote > firstQuote && firstQuote != std::string::npos) {
                auto checks =
                    line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                for (absl::string_view checkName :
                     absl::StrSplit(checks, ",")) {
                    checkName = absl::StripLeadingAsciiWhitespace(checkName);
                    if (checkName.front() == '-') {
                        ignores.emplace(checkName.substr(1));
                    }
                }
            }
        }
        confLines.push_back(line);
    }
}

void AutoTidy::readTidyLog()
{
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

    std::vector<std::string> text;
    TidyError error;
    int errorNo = 0;
    std::string line;
    std::ifstream logFile(filename);
    while (std::getline(logFile, line)) {
        std::cmatch currentMatch;
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

            error = {errorNo++,
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
}

void AutoTidy::readFixes()
{
    if (!fixesFile.empty()) {
        auto fixesData = readFile(fixesFile);

        // Try to fix up non conformant YAML strings in fixes data
        bool inQuotes;
        for (size_t i = 0; i < fixesData.size(); i++) {
            if (fixesData[i] == '\'' && fixesData[i + 1] != '\'') {
                inQuotes = !inQuotes;
            }
            // If we find a line feed instide quotes, add another line feed
            if (inQuotes && fixesData[i] == 10) {
                fixesData.insert(fixesData.begin() + i, 10);
                i++;
            }
        }

        YAML::Node fixes =
            YAML::Load(std::string(fixesData.begin(), fixesData.end()));
        size_t errorNo = 0;
        for (auto const& d : fixes["Diagnostics"]) {
            for (auto const& r : d["Replacements"]) {
                errorList[errorNo].replacements.emplace_back(
                    r["FilePath"].as<std::string>(), r["Offset"].as<int>(),
                    r["Length"].as<int>(),
                    r["ReplacementText"].as<std::string>());
            }
            errorNo++;
        }
    }
}

void AutoTidy::run()
{
    currDir = currentDir();
    if (!absl::EndsWith(currDir, "/")) {
        currDir += "/";
    }

    readConfig();
    readTidyLog();
    readFixes();

    for (auto const& e : errorList) {
        handleError(e);
    }
}
