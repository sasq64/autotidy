#pragma once

#include "patched_file.h"

#include <algorithm>
#include <map>
#include <utility>

using namespace std::string_literals;
struct Replacement
{
    Replacement(std::string aPath, size_t aOffset, size_t aLength,
                std::string aText)
        : path(std::move(aPath)), offset(aOffset), length(aLength),
          text(std::move(aText))
    {}

    Replacement(std::string const& aPath, Replacement const& other)
        : path(std::move(aPath)), offset(other.offset), length(other.length),
          text(other.text)
    {}

    std::string path;
    size_t offset;
    size_t length;
    std::string text;
};

class Replacer
{
    std::map<std::string, PatchedFile> patchedFiles;

public:
    size_t offsetInOriginalFile(std::string fileName, int line, int col)
    {
        auto contents = readFile(fileName + ".orig");
        return lineColToOffset(contents, line, col);
    }

    size_t offsetInFile(std::string const& fileName, int line, int col)
    {
        return lineColToOffset(patchedFiles[fileName].contents(), line, col);
    }

    void translateLineColumn(std::string const& fileName, int& line,
                             int& column)
    {
        if (patchedFiles.count(fileName) == 0)
            return;

        auto oldOffs = offsetInOriginalFile(fileName, line, column);
        auto newOffs = patchedFiles[fileName].translateOffset(oldOffs);
        std::tie(line, column) =
            offsetToLineCol(patchedFiles[fileName].contents(), newOffs);
    }

    void appendToLine(std::string const& fileName, int line,
                      std::string const& text)
    {
        int col = 1;
        auto contents = fileExists(fileName + ".orig")
                            ? readFile(fileName + ".orig")
                            : readFile(fileName);

        std::cout << std::string(contents.begin(), contents.end()) << "\n";
        size_t offs = lineColToOffset(contents, line + 1, col) - 1;
        std::cout << "OFFS " << offs << "\n";
        applyReplacement({fileName, offs, 0, text});
    }

    void applyReplacement(Replacement const& r)
    {
        auto backupName = r.path + ".orig";
        if (!fileExists(backupName))
            copyFileToFrom(backupName, r.path);
        auto& pf = patchedFiles[r.path];
        pf.setFileName(r.path);
        pf.patch(r.offset, r.length, r.text);
        pf.flush();
    }

    void copyFile(std::string const& target, std::string const& source)
    {
        auto it = patchedFiles.find(source);
        // Patch data of source needs to be copied into target
        if (it != patchedFiles.end()) {
            patchedFiles[target] = it->second;
            patchedFiles[target].setFileName(target);
        }
        copyFileToFrom(target, source);
    }

    void removeFile(std::string const& name)
    {
        auto it = patchedFiles.find(name);
        if (it != patchedFiles.end()) {
            patchedFiles.erase(it);
        }

        ::remove(name.c_str());
    }
};

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

