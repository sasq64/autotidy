#include <coreutils/file.h>
#include <coreutils/split.h>
#include <coreutils/utils.h>
#include <cstdio>
#include <iostream>
#include <regex>
#include <termios.h>
#include <unistd.h>

#include <fmt/format.h>

#include <set>

char getch()
{
    char buf = 0;
    struct termios old
    {};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return (buf);
}

std::string currentDir()
{
    char buf[4096] = {0};
    getcwd(buf, sizeof(buf));
    return std::string(buf);
}

struct Error
{
    Error() = default;
    Error(std::string const& aCheck, int aLine, int aColumn,
          std::string const& aFileName, std::string const& aError)
        : check(aCheck), line(aLine), column(aColumn), fileName(aFileName),
          error(aError)
    {}
    std::string check;
    int line;
    int column;
    std::string fileName;
    std::string error;
};

std::set<std::string> ignores;
std::vector<std::string> confLines;
std::string currDir;

void saveConfig()
{
    utils::File outf{".clang-tidy", utils::File::Mode::Write};
    for (auto const& line : confLines) {
        if (utils::startsWith(line, "Checks:")) {
            outf.writeString(
                fmt::format("Checks: '*, -{}'\n",
                            utils::join(ignores.begin(), ignores.end(), ", -")));
        } else
            outf.writeln(line);
    }
}

void handleError(const Error& e, std::vector<std::string> const& text)
{
    if (ignores.count(e.check) > 0)
        return;

    auto fn = e.fileName;
    if(utils::startsWith(fn, currDir)) {
        fn = fn.substr(currDir.length());
    }

    fmt::print_colored(fmt::Color::YELLOW, "{}", fn);
    fmt::print_colored(fmt::Color::WHITE, ":{}", e.line);
    fmt::print_colored(fmt::Color::RED, " [{}]", e.check);
    fmt::print_colored(fmt::Color::GREEN, "\n{}\n", e.error);
    for (auto const& t : text)
        std::puts(t.c_str());

    fmt::print_colored(fmt::Color::CYAN,
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
        system(fmt::format("nvim {} \"+call cursor({}, {})\"", e.fileName,
                           e.line, e.column)
                   .c_str());
        break;
    }
}
int main(int argc, char** argv)
{
    utils::File configFile{".clang-tidy"};

    currDir = currentDir();
    if(!utils::endsWith(currDir, "/"))
        currDir += "/";

    for (auto const& line : configFile.lines()) {
        if (utils::startsWith(line, "Checks:")) {
            auto q0 = line.find_first_of("'");
            auto q1 = line.find_last_of("'");
            if (q1 > q0 && q0 != std::string::npos) {
                auto checks = line.substr(q0 + 1, q1 - q0 -1);
                for (const char* c : utils::split(checks, ",")) {
                    while (*c && *c == ' ')
                        c++;
                    if (*c == '-')
                        ignores.emplace(&c[1]);
                    std::puts(c);
                }
            }
        }
        confLines.push_back(line);
    }

    std::regex errline{R"(([^:]+):(\d+):(\d+):\s*(\w+):\s*(.*)\[(.*)\])"};

    std::cmatch currentMatch;
    std::vector<std::string> text;
    Error error;
    std::puts("Lines");
    for (auto const& l : utils::File{argv[1]}.lines()) {
        if (std::regex_match(l.c_str(), currentMatch, errline)) {

            if(currentMatch[4] == "note") {
                text.push_back(l);
                continue;
            }



            if (error.error != "")
                handleError(error, text);
            text.clear();

            error = {currentMatch[6], std::stoi(currentMatch[2].str()),
                     std::stoi(currentMatch[3].str()), currentMatch[1],
                     currentMatch[5]};
        } else
            text.push_back(l);
    }
}
