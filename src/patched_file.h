#include "utils.h"


#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iostream>

// Add all files that are getting replacements to this instance
// For each fix, create temporary file with patches
// Requires remembering delta within fix and saving it "permanently"
// if fix is applied
// startFix()
// addReplacement() (Changes delta for file)
// cancel() (Resets delta)
// apply() (Remembers delta)
//
// Original file -> PATCH -> Temp file -> COPY -> Original File

// Saves the patches of a file, so subsequent patches can happen at the
// correct offset.
struct PatchedFile
{
    std::string fileName;
    std::vector<std::pair<size_t, int>> patches;
    std::vector<char> contents;

    PatchedFile() = default;

    PatchedFile(std::string const& aFileName) : fileName(aFileName) {}

    // Patch this file, respecting the prevous patches
    void patch(size_t offset, size_t length, std::string const& text)
    {
        if (contents.size() == 0)
            contents = readFile(fileName);

        // offset depends on prevous patches
        for (auto const& p : patches) {
            if (p.first < offset) {
                offset += p.second;
            }
        }

        auto newLength = text.length();
        auto insertPos = contents.begin() + offset;

        if (newLength < length) {
            // Remove some characters
            contents.erase(insertPos, insertPos + length - newLength);
            // contents.resize(contents.size() - (r.length - newLength));
        } else if (newLength > length) {
            // Insert some empty characters
            contents.insert(insertPos, newLength - length, 0);
        }
        insertPos = contents.begin() + offset;
        auto delta = (newLength - length);

        patches.emplace_back(offset, delta);

        std::copy(text.begin(), text.end(), insertPos);
    }

    void flush()
    {
        writeFile(fileName, std::string(contents.begin(), contents.end()));
    }

    bool operator==(const std::string& other) const
    {
        return other == fileName;
    }
};

