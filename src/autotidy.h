#pragma once

#include "path.h"
#include "replacer.h"

#include <set>
#include <string>
#include <vector>

struct TidyError
{
    TidyError() = default;
    TidyError(int aNumber, std::string const& aCheck, int aLine, int aColumn,
              utils::path const& aFileName, std::string const& aError)
        : number(aNumber), check(aCheck), line(aLine), column(aColumn),
          fileName(aFileName), error(aError)
    {}
    int number = 0;
    std::string check;
    int line = 0;
    int column = 0;
    utils::path fileName;
    std::string error;
    std::string text;
    std::vector<Replacement> replacements;
};

class AutoTidy
{
    std::set<std::string> ignores;
    std::vector<std::string> confLines;
    std::string currDir;
    Replacer replacer;
    std::set<std::string> skippedFiles;
    utils::path filename;
    utils::path configFilename;
    std::string diffCommand;
    utils::path fixesFile;
    std::vector<TidyError> errorList;

    enum
    {
        RealName,
        TempName
    };
    std::map<std::string, std::string> tempFiles;

    std::string helpText =
        R"([?] = This help text
[a] = Apply the shown patch, if this issue has a Fix
[i] = Ignore this check, add it to list of ignored checks in .clang-tidy
[s] = Skip this issue
[S] = Skip all issues in this file
[n] = Add a NOLINT comment to the line where the issue appears
[N] = As above, but only for the current check
[d] = Show documentation on the current check
[t] = Add a TODO comment to the line where the issue appears
[q] = Quit autotidy)";

    void readTidyLog();
    void readFixes();

    char promptUser();
    bool handleKey(char c, TidyError const& err);
    void printError(TidyError const& err);
    bool handleError(TidyError const& err);

public:
    AutoTidy(utils::path const& aFilename, utils::path const& aConfigFilename,
             std::string const& aDiffCommand, utils::path const& aFixesFile)
        : filename(aFilename), configFilename(aConfigFilename),
          diffCommand(aDiffCommand), fixesFile(aFixesFile)
    {}
    void run();
    void saveConfig();
    void readConfig();
    void setIgnores(std::set<std::string> const& ignores);
};
