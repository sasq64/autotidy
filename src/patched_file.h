#pragma once

#include "utils.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Saves the patches of a file, so subsequent patches can happen at the
// correct offset.
class PatchedFile
{
    std::string fileName_;
    std::vector<std::pair<size_t, int>> patches_;
    std::vector<char> contents_;

public:
    PatchedFile() = default;
    explicit PatchedFile(std::string const& fileName) : fileName_(fileName) {}

    std::vector<char> const& contents()
    {
        if (contents_.empty()) {
            contents_ = readFile(fileName_);
        }
        return contents_;
    }

    auto const& patches() const { return patches_; }
    auto const& fileName() const { return fileName_; }

    void setFileName(std::string const& fileName) { fileName_ = fileName; }

    size_t translateOffset(size_t offset) const
    {
        for (auto const& p : patches_) {
            if (p.first < offset) {
                offset += p.second;
            }
        }
        return offset;
    }

    // Patch this file, respecting the prevous patches_
    void patch(size_t offset, size_t length, std::string const& text)
    {
        // Make sure contents is available
        (void)contents();

        // offset depends on prevous patches_
        for (auto const& p : patches_) {
            if (p.first < offset) {
                offset += p.second;
            }
        }

        auto newLength = text.length();
        auto insertPos = contents_.begin() + offset;

        if (newLength < length) {
            // Remove some characters
            contents_.erase(insertPos, insertPos + length - newLength);
        } else if (newLength > length) {
            // Insert some empty characters
            contents_.insert(insertPos, newLength - length, 0);
        }
        insertPos = contents_.begin() + offset;
        auto delta = (newLength - length);

        patches_.emplace_back(offset, delta);

        std::copy(text.begin(), text.end(), insertPos);
    }

    void flush() const
    {
        if (contents_.empty()) {
            return;
        }
        writeFile(fileName_, std::string(contents_.begin(), contents_.end()));
    }

    bool operator==(const std::string& other) const
    {
        return other == fileName_;
    }
};

