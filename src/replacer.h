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

    PatchedFile& getPatchedFile(std::string const& name) {
        auto it = patchedFiles.find(name);
        if(it != patchedFiles.end())
            return it->second;
        copyFileToFrom(name + ".orig", name);
        return patchedFiles.emplace(name, name).first->second;
    }

public:

    ~Replacer()
    {
        for(auto const& p : patchedFiles) {
            std::remove((std::get<const std::string>(p) + ".orig").c_str());
        }
    }

    void appendToLine(std::string const& fileName, int line,
                      std::string const& text)
    {
        int col = 1;
        auto contents = patchedFiles.count(fileName) > 0
                            ? readFile(fileName + ".orig")
                            : readFile(fileName);

        size_t offs = lineColToOffset(contents, line + 1, col) - 1;
        applyReplacement({fileName, offs, 0, text});
    }

    void applyReplacement(Replacement const& r)
    {
        auto& pf = getPatchedFile(r.path);
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
        copyFileToFrom(target + ".orig", source + ".orig");
    }

    void removeFile(std::string const& name)
    {
        auto it = patchedFiles.find(name);
        if (it != patchedFiles.end()) {
            patchedFiles.erase(it);
            ::remove((name + ".orig").c_str());
        }

        ::remove(name.c_str());

    }
};


