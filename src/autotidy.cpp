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
#include <map>
#include <regex>
#include <set>
#include <utility>

using namespace std::string_literals;

extern std::map<std::string, std::string> manPages;

void AutoTidy::saveConfig()
{
    std::ofstream outf{".clang-tidy"};
    for (auto const& line : confLines) {
        if (absl::StartsWith(line, "Checks:")) {
            if (ignores.empty()) {
                outf << "Checks: '*'\n";
            } else {
                outf << fmt::format("Checks: '*, -{}'\n",
                                    absl::StrJoin(ignores, ", -"));
            }
        } else {
            outf << line << "\n";
        }
    }
}

void AutoTidy::printError(TidyError const& err)
{
    std::string baseName = err.fileName;
    if (absl::StartsWith(baseName, currDir)) {
        baseName = baseName.substr(currDir.length());
    }

    fmt::print(fmt::fg(fmt::color::white), "\n#{} ", err.number);
    fmt::print(fmt::fg(fmt::color::gold), "{}", baseName);
    fmt::print(fmt::fg(fmt::color::white), ":{}", err.line);
    fmt::print(fmt::fg(fmt::color::light_pink), " [{}]", err.check);
    fmt::print(fmt::fg(fmt::color::light_green), "\n{}\n", err.error);
    std::puts(err.text.c_str());
}

char AutoTidy::promptUser()
{
    bool hasPatch = !tempFiles.empty();
    fmt::print(fmt::fg(fmt::color::cyan),
               "{}[t]odo, [i]gnore, [s/S]kip, [n/N]olint, [d]oc, [q]uit, "
               "[?] Help : ",
               hasPatch ? "[a]pply, " : "");
    std::fflush(stdout);
    auto c = getch();
    if (c < 0x20 || c >= 0x7f) {
        c = ' ';
    }
    fmt::print(fmt::bg(fmt::color::white) | fmt::fg(fmt::color::black), "[{}]",
               static_cast<char>(c));
    std::puts("");
    return c;
}

bool AutoTidy::handleKey(char c, TidyError const& err)
{
    switch (c) {
    case 'q':
        break;
    case '?':
    case 'h':
        std::puts(helpText.c_str());
        return false;
    case 'a':
        for (auto const& f : tempFiles) {
            // Copy temporary -> real
            replacer.copyFile(std::get<RealName>(f), std::get<TempName>(f));
            replacer.removeFile(std::get<TempName>(f));
        }
        tempFiles.clear();
        break;
    case 'n':
        replacer.appendToLine(err.fileName, err.line, " //NOLINT");
        break;
    case 'N':
        replacer.appendToLine(err.fileName, err.line,
                              fmt::format(" //NOLINT({})", err.check));
        break;
    case 't':
        replacer.appendToLine(err.fileName, err.line,
                              fmt::format(" //TODO({})", err.check));
        break;
    case 'i':
        ignores.insert(err.check);
        saveConfig();
        break;
    case 's':
        break;
    case 'S':
        skippedFiles.insert(err.fileName);
        break;
    case 'd':
        pipeStringToCommand("man -l -", manPages[err.check]);
        return false;
    default:
        return false;
    }
    return true;
}

bool AutoTidy::handleError(const TidyError& err)
{
    static auto const separatorString = std::string(60, '-') + "\n";

    if (ignores.count(err.check) > 0 || err.fileName.empty()) {
        return false;
    }
    if (skippedFiles.count(err.fileName) > 0) {
        return false;
    }

    printError(err);
    tempFiles.clear();

    // Make copies of the files in the error and work on the copies instead.
    // Then we apply fixes to the copies an use 'diff' to show the changes.
    for (auto const& r : err.replacements) {
        auto temp = r.path + ".temp";
        if (!contains(tempFiles, r.path)) {
            tempFiles[r.path] = temp;
            // We copy via the replacer since it needs to keep
            // track of all files with possible replacements
            replacer.copyFile(temp, r.path);
        }
        // Patch the temporary file
        replacer.applyReplacement({temp, r});
    }

    bool quitProgram = false;
    while (true) {
        // Show diff between patched temp files and original file
        for (auto const& p : tempFiles) {
            system(fmt::format(diffCommand, std::get<RealName>(p), // NOLINT
                               std::get<TempName>(p))
                       .c_str());
        }

        char c = promptUser();
        quitProgram = (c == 'q');
        if (handleKey(c, err)) {
            break;
        }
    }

    fmt::print(fmt::fg(fmt::color::steel_blue), separatorString);
    for (auto const& f : tempFiles) {
        replacer.removeFile(std::get<TempName>(f));
    }

    return quitProgram;
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
                for (const auto& checkName : absl::StrSplit(checks, ',')) {
                    auto strippedName =
                        absl::StripLeadingAsciiWhitespace(checkName);
                    if (strippedName.front() == '-') {
                        ignores.emplace(strippedName.substr(1));
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

    std::regex errline{R"((([^:]+):(\d+):(\d+):)?\s*(\w+):\s*(.*)\[(.*)\])"};

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
                     utils::path{currentMatch[Filename]},
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
    if (!fixesFile.empty() && utils::exists(fixesFile)) {
        auto fixesData = readFile(fixesFile);

        // Try to fix up non conformant YAML strings in fixes data
        bool inQuotes = false;
        for (size_t i = 0; i < fixesData.size(); i++) {
            if (fixesData[i] == '\'' && fixesData[i + 1] != '\'') {
                inQuotes = !inQuotes;
            }
            // If we find a line feed inside quotes, add another line feed
            if (inQuotes && fixesData[i] == 10) {
                fixesData.insert(fixesData.begin() + i, 10);
                i++;
            }
        }

        YAML::Node fixes =
            YAML::Load(std::string(fixesData.begin(), fixesData.end()));
        size_t errorNo = 0;
        for (auto const& d : fixes["Diagnostics"]) {
            if (errorNo >= errorList.size()) {
                return;
            }
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

void AutoTidy::setIgnores(std::set<std::string> const& ignores)
{
    this->ignores = ignores;
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
        if (handleError(e)) {
            return;
        }
    }
}
