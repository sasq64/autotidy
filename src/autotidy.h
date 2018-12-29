#include "replacer.h"
#include <set>
#include <string>
#include <vector>

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

class AutoTidy
{
    std::set<std::string> ignores;
    std::vector<std::string> confLines;
    std::string currDir;
    Replacer replacer;
    std::set<std::string> skippedFiles;
    std::string filename;
    std::string configFilename;
    std::string diffCommand;
    std::string fixesFile;
    std::vector<TidyError> errorList;

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

    void saveConfig();
    void readConfig();
    void readTidyLog();
    void readFixes();

    bool handleError(const TidyError& e);

public:
    AutoTidy(std::string const& aFilename, std::string const& aConfigFilename,
             std::string const& aDiffCommand, std::string const& aFixesFile)
        : filename(aFilename), configFilename(aConfigFilename),
          diffCommand(aDiffCommand), fixesFile(aFixesFile)
    {}
    void run();
};
