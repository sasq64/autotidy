#pragma once

#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <climits>
#include <cstdlib>

#include <dirent.h>
#include <sys/stat.h>

namespace utils {
/**
 * A `path` is defined by a set of _segments_ and a flag
 * that says if the path is _absolute_ or _relative.
 *
 * If the last segment is empty, it is a path without a filename
 * component.
 *
 */
class path
{
    enum Format
    {
        Unknown,
        Win,
        Unix
    };
    Format format = Format::Unknown;
    bool isRelative = true;
    bool hasRootDir = false;
    std::vector<std::string> segments;
    std::string empty_string = "";

    void init(std::string const& name)
    {
        size_t start = 0;
        isRelative = true;
        if (name[0] == '/') {
            format = Format::Unix;
            start++;
            isRelative = false;
        } else if (name[1] == ':') {
            format = Format::Win;
            segments.push_back(name.substr(0, 2));
            hasRootDir = true;
            start += 2;
            if (name[2] == '\\' || name[2] == '/') {
                isRelative = false;
                start++;
            }
        }
        for (auto i = start; i < name.length(); i++) {
            if (name[i] == '/' || name[i] == '\\') {
                if (format == Format::Unknown) {
                    format = name[i] == '/' ? Format::Unix : Format::Win;
                }
                segments.push_back(name.substr(start, i - start));
                start = ++i;
            }
        }
        segments.push_back(name.substr(start));
    }

    std::string& segment(int i)
    {
        if (i < 0) {
            i += segments.size();
        }
        if (i < static_cast<int>(segments.size())) {
            return segments[i];
        }
        return empty_string;
    }

    const std::string& segment(int i) const
    {
        if (i < 0) {
            i += segments.size();
        }
        if (i < static_cast<int>(segments.size())) {
            return segments[i];
        }
        return empty_string;
    }

public:
    path() = default;
    path(std::string const& name) { init(name); } // NOLINT
    path(const char* name) { init(name); }        // NOLINT

    bool is_absolute() const { return !isRelative; }
    bool is_relative() const { return isRelative; }

    path& operator=(const char* name)
    {
        init(name);
        return *this;
    }

    path& operator=(std::string const& name)
    {
        init(name);
        return *this;
    }

    std::vector<std::string> parts() const { return segments; }

    path& operator/=(path const& p)
    {
        if (p.is_absolute()) {
            *this = p;
        } else {
            if (segment(-1).empty()) {
                segments.resize(segments.size() - 1);
            }
            segments.insert(std::end(segments), std::begin(p.segments),
                            std::end(p.segments));
        }

        return *this;
    }

    path filename() const
    {
        path p = *this;
        p.isRelative = true;
        if (!empty()) {
            p.segments[0] = segment(-1);
            p.segments.resize(1);
        }
        return p;
    }

    std::string extension() const
    {
        if (empty()) {
            return "";
        }
        const auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            return filename.substr(dot);
        }
        return "";
    }

    std::string stem() const
    {
        if (empty()) {
            return "";
        }
        const auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            return filename.substr(0, dot);
        }
        return "";
    }

    path& replace_extension(std::string const& ext)
    {
        if (empty()) {
            return *this;
        }
        auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            filename = filename.substr(0, dot) + ext;
        }
        return *this;
    }

    path parent_path() const
    {
        path p = *this;
        if (!empty()) {
            p.segments.resize(segments.size() - 1);
        }
        return p;
    }

    bool empty() const { return segments.empty(); }

    auto begin() const { return segments.begin(); }

    auto end() const { return segments.end(); }

    std::string string() const
    {
        using namespace std::string_literals;
        std::string target;
        int l = segments.size();
        int i = 0;
        std::string separator = format == Format::Win ? "\\"s : "/"s;
        if (!isRelative) {
            if (hasRootDir) {
                target = segment(i++);
            }
            target += separator;
        }
        for (; i < l; i++) {
            target += segment(i);
            if (i != l - 1) {
                target += separator;
            }
        }
        return target;
    }

    operator std::string() const { return string(); } // NOLINT

    bool operator==(const char* other) const
    {
        return strcmp(other, string().c_str()) == 0;
    }

    friend std::ostream& operator<<(std::ostream& os, const path& p)
    {
        os << std::string(p);
        return os;
    }
};

inline path resolve(path const& p)
{
    char target[PATH_MAX];
    ::realpath(p.string().c_str(), target);
    return path(target);
}

inline path operator/(path const& a, path const& b)
{
    return path(a) /= b;
}

inline bool exists(path const& p)
{
    struct stat sb; // NOLINT
    return stat(p.string().c_str(), &sb) >= 0;
}

inline void create_directory(path const& p)
{
#ifdef _WIN32
    mkdir(p.string().c_str());
#else
    mkdir(p.string().c_str(), 07777);
#endif
}

inline void create_directories(path const& p)
{
    path dir;
    for (const auto& part : p) {
        dir = dir / part;
        create_directory(dir);
    }
}

inline bool remove(path const& p)
{
    return (std::remove(p.string().c_str()) != 0);
}

inline bool copy(path const& source, path const& target)
{
    std::ifstream src(source.string(), std::ios::binary);
    std::ofstream dst(target.string(), std::ios::binary);
    dst << src.rdbuf();
    return true;
}

struct directory_entry
{
    utils::path parent;
    struct dirent* ent = nullptr;

    utils::path path() const { return parent / ent->d_name; }

    operator class path() { return parent / ent->d_name; } // NOLINT
};

struct directory_iterator
{
    DIR* dir = nullptr;
    directory_entry entry;

    ~directory_iterator()
    {
        if (dir != nullptr) {
            closedir(dir);
        }
    }

    directory_iterator() = default;

    void readNext()
    {
        while (true) {
            entry.ent = readdir(dir);
            if (entry.ent == nullptr || entry.ent->d_name[0] != '.') {
                break;
            }
        }
    }

    directory_iterator(const directory_iterator& other)
    {
        entry.parent = other.entry.parent;
        dir = opendir(entry.parent.string().c_str());
        readNext();
    }

    directory_iterator& operator=(const directory_iterator& other)
    {
        entry.parent = other.entry.parent;
        dir = opendir(entry.parent.string().c_str());
        readNext();
        return *this;
    }

    directory_iterator(directory_iterator&& other) noexcept
    {
        dir = other.dir;
        entry = other.entry;
        other.dir = nullptr;
    }

    directory_iterator& operator=(directory_iterator&& other) noexcept
    {
        dir = other.dir;
        entry = other.entry;
        other.dir = nullptr;
        return *this;
    }

    explicit directory_iterator(path const& p)
    {
        entry.parent = p;
        dir = opendir(p.string().c_str());
        readNext();
    }

    bool operator!=(const directory_iterator& other)
    {
        return other.entry.ent != entry.ent;
    }

    directory_entry& operator*() { return entry; }

    directory_iterator& operator++()
    {
        readNext();
        return *this;
    }
};

inline directory_iterator end(const directory_iterator& /*unused*/) noexcept
{
    return directory_iterator();
}

inline directory_iterator begin(directory_iterator iter) noexcept
{
    return iter;
}

/* inline directory_iterator const& begin(directory_iterator const& iter)
 * noexcept */
/* { */
/*     return iter; */
/* } */

inline bool is_directory(path const& p)
{
    struct stat ss; // NOLINT
    stat(p.string().c_str(), &ss);
    return (ss.st_mode & S_IFMT) == S_IFDIR;
}

inline void _listFiles(const std::string& dirName,
                       const std::function<void(const std::string& path)>& f)
{
    for (const auto& p : directory_iterator{path(dirName)}) {
        auto&& path = p.path().string();
        if (path[0] == '.' &&
            (path[1] == 0 || (path[1] == '.' && path[2] == 0))) {
            continue;
        }
        if (is_directory(p.path())) {
            _listFiles(path, f);
        } else {
            f(path);
        }
    }
}

inline void listFiles(const std::string& dirName,
                      const std::function<void(const std::string& path)>& f)
{
    if (!is_directory(dirName)) {
        f(dirName);
        return;
    }
    _listFiles(dirName, f);
}

} // namespace utils
