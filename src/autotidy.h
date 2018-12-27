#include "replacer.h"
#include <set>
#include <string>
#include <vector>

struct TidyError;

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

    void saveConfig();

    bool handleError(const TidyError& e);

public:
    AutoTidy(std::string const& aFilename, std::string const& aConfigFilename,
             std::string const& aDiffCommand, std::string const& aFixesFile)
        : filename(aFilename), configFilename(aConfigFilename),
          diffCommand(aDiffCommand), fixesFile(aFixesFile)
    {}
    void run();
};
