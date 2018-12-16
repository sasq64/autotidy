#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include <fmt/color.h>
#include <fmt/format.h>

#include <CLI/CLI.hpp>

#include <yaml-cpp/yaml.h>

#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>

using namespace std::string_literals;

static char getch() {
    char buf = 0;
    struct termios old {};
    if (tcgetattr(0, &old) < 0) {
        throw new std::runtime_error("tcsetattr()");
    }
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0) {
        throw new std::runtime_error("tcsetattr ICANON");
    }
    if (read(0, &buf, 1) < 0) {
        throw new std::runtime_error("read()");
    }
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0) {
        throw new std::runtime_error("tcsetattr ~ICANON");
    }
    return (buf);
}

std::string currentDir() {
    char buf[4096] = {0};
    getcwd(&buf[0], sizeof(buf));
    return std::string(&buf[0]);
}

struct Replacement {
    Replacement(std::string aPath, size_t aOffset, size_t aLength,
                std::string aText)
        : path(aPath), offset(aOffset), length(aLength), text(aText) {}
    std::string path;
    size_t offset;
    size_t length;
    std::string text;
};

struct Error {
    Error() = default;
    Error(std::string aCheck, int aLine, int aColumn, std::string aFileName,
          std::string aError)
        : check(std::move(aCheck)),
          line(aLine),
          column(aColumn),
          fileName(std::move(aFileName)),
          error(std::move(aError)) {}
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

void saveConfig() {
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

void copyFile(std::string const& target, std::string const& source) {
    std::ifstream src(source, std::ios::binary);
    std::ofstream dst(target, std::ios::binary);
    dst << src.rdbuf();
}

std::vector<char> readFile(std::string const& fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        return buffer;
    }
    return buffer;
}

class Replacer {
    std::vector<char> contents;
    std::string fileName;
    int count = 0;
    int64_t delta = 0;

    std::string outputFile;

    std::vector<std::string> files;

   public:
    std::vector<std::string> const& getFiles() const { return files; }

    void commit() {
        if (outputFile == "") return;
        std::ofstream out(outputFile);
        out << std::string(contents.begin(), contents.end());
        out.close();
        outputFile = "";
    }

    void applyReplacement(Replacement const& r) {
        if (r.path != fileName) {
            commit();
            contents = readFile(r.path);
            fileName = r.path;
            files.push_back(fileName);
            outputFile = fmt::format(".patchFile{}", count++);
        }

        auto offset = r.offset + delta;

        auto newLength = r.text.length();
        auto insertPos = contents.begin() + offset;

        if (newLength < r.length) {
            // Remove some characters
            contents.erase(insertPos, insertPos + r.length - newLength);
            // contents.resize(contents.size() - (r.length - newLength));
        } else if (newLength > r.length) {
            // Insert some empty characters
            contents.insert(insertPos, newLength - r.length, 0);
        }
        insertPos = contents.begin() + offset;
        delta += (newLength - r.length);

        std::copy(r.text.begin(), r.text.end(), insertPos);
    }
};

void handleError(const Error& e) {
    if (ignores.count(e.check) > 0 || e.fileName == "") {
        return;
    }

    auto fn = e.fileName;
    if (absl::StartsWith(fn, currDir)) {
        fn = fn.substr(currDir.length());
    }

    fmt::print(fmt::fg(fmt::color::gold), "{}", fn);
    fmt::print(fmt::fg(fmt::color::white), ":{}", e.line);
    fmt::print(fmt::fg(fmt::color::light_pink), " [{}]", e.check);
    fmt::print(fmt::fg(fmt::color::light_green), "\n{}\n", e.error);
    std::puts(e.text.c_str());

    Replacer replacer;
    for (auto const& r : e.replacements) {
        replacer.applyReplacement(r);
    }
    replacer.commit();

    auto const& files = replacer.getFiles();
    for (size_t i = 0; i < files.size(); i++) {
        system(fmt::format("diff -u3 --color {} .patchFile{}", files[i], i)
                   .c_str());
    }

    fmt::print(fmt::fg(fmt::color::cyan),
               "[e]dit, [f]ix, [i]gnore, [s]kip, [n/N]olint ? ");
    std::fflush(stdout);

    auto c = getch();
    std::puts("");
    switch (c) {
        case 'f':
            for (size_t i = 0; i < e.replacements.size(); i++) {
                copyFile(e.replacements[i].path,
                         fmt::format(".patchedFile{}", i));
            }
            break;
        case 'n':
            system(
                fmt::format("sed -i '{}s/$/ \\/\\/NOLINT/'", e.line).c_str());
            break;
        case 'N':
            system(fmt::format("sed -i '{}s/$/ \\/\\/NOLINT({})/'", e.line,
                               e.check)
                       .c_str());
            break;
        case 'i':
            ignores.insert(e.check);
            saveConfig();
            break;
        case 'e':
            system(
                fmt::format(editCommand, e.fileName, e.line, e.column).c_str());
            break;
    }

    for (size_t i = 0; i < replacer.getFiles().size(); i++) {
        std::remove(fmt::format(".patchedFile{}", i).c_str());
    }
}
int main(int argc, char** argv) {
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
                    fmt::print("{}\n", std::string(c.data(), c.length()));
                }
            }
        }
        confLines.push_back(line);
    }

    std::regex errline{R"((([^:]+):(\d+):(\d+):)?\s*(\w+):\s*(.*)\[(.*)\])"};

    std::cmatch currentMatch;
    std::vector<std::string> text;
    Error error;
    std::vector<Error> errorList;
    std::ifstream logFile(filename);
    while (std::getline(logFile, line)) {
        if (std::regex_match(line.c_str(), currentMatch, errline)) {
            if (currentMatch[4] == "note") {
                text.push_back(line);
                continue;
            }

            if (error.error != "") {
                errorList.push_back(error);
                errorList.back().text = absl::StrJoin(text, "\n");
            }
            text.clear();

            error = {currentMatch[7], std::atoi(currentMatch[3].str().c_str()),
                     std::atoi(currentMatch[4].str().c_str()), currentMatch[2],
                     currentMatch[6]};
        } else {
            text.push_back(line);
        }
    }
    if (error.error != "") {
        errorList.push_back(error);
        errorList.back().text = absl::StrJoin(text, "\n");
    }

    if (fixesFile != "") {
        auto fixesData = readFile(fixesFile);
        bool inQuotes;
        for (size_t i = 0; i < fixesData.size(); i++) {
            if (fixesData[i] == '\'' && fixesData[i + 1] != '\'')
                inQuotes = inQuotes ? false : true;
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
