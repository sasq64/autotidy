
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <absl/strings/ascii.h>

#include <fmt/color.h>
#include <fmt/format.h>

#include <CLI/CLI.hpp>

#include <yaml-cpp/yaml.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

using namespace std::string_literals;

static char getch()
{
    char buf = 0;
    struct termios old
    {};
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

std::string currentDir()
{
    char buf[4096] = {0};
    getcwd(&buf[0], sizeof(buf));
    return std::string(&buf[0]);
}

struct Error
{
    Error() = default;
    Error(std::string aCheck, int aLine, int aColumn, std::string aFileName,
          std::string aError)
        : check(std::move(aCheck)), line(aLine), column(aColumn),
          fileName(std::move(aFileName)), error(std::move(aError))
    {}
    std::string check;
    int line = 0;
    int column = 0;
    std::string fileName;
    std::string error;
    std::string text;
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

void handleError(const Error& e)
{
    if (ignores.count(e.check) > 0) {
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

    fmt::print(fmt::fg(fmt::color::cyan),
               "[e]dit, [i]gnore, [s]kip, [n/N]olint ? ");
    std::fflush(stdout);

    auto c = getch();
    std::puts("");
    switch (c) {
    case 'n':
        system(fmt::format("sed -i '{}s/$/ \\/\\/NOLINT/'", e.line).c_str());
        break;
    case 'N':
        system(fmt::format("sed -i '{}s/$/ \\/\\/NOLINT({})/'", e.line, e.check)
                   .c_str());
        break;
    case 'i':
        ignores.insert(e.check);
        saveConfig();
        break;
    case 'e':
        system(fmt::format(editCommand, e.fileName, e.line, e.column).c_str());
        break;
    }
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
    app.add_option("-e,--edit-command", editCommand, "Command to use for editing file", true);
    app.add_option("-f,--fixes-file", fixesFile, "Exported fixes from clang-tidy");

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

    std::regex errline{R"(([^:]+):(\d+):(\d+):\s*(\w+):\s*(.*)\[(.*)\])"};

    std::cmatch currentMatch;
    std::vector<std::string> text;
    Error error;
    std::vector<Error> errorList;
    std::ifstream logFile(argv[1]);
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

            error = {currentMatch[6], std::stoi(currentMatch[2].str()),
                     std::stoi(currentMatch[3].str()), currentMatch[1],
                     currentMatch[5]};
        } else {
            text.push_back(line);
        }
    }

    if(fixesFile != "") {
        YAML::Node fixes = YAML::LoadFile(fixesFile);
        int i=0;
        for(auto const& d : fixes["Diagnostics"]) {
            fmt::print("{} vs {}\n", d["DiagnosticName"].as<std::string>(), errorList[i].check);
            i++;
        }
    }

    for(auto const& e : errorList) {
        handleError(e);
    }


}
