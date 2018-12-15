#pragma once

#include "split.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace utils {

template <class T, template<typename DUMMY> class CONTAINER>
auto find(CONTAINER<T> const& haystack, T const& needle)
{
    return std::find(begin(haystack), end(haystack), needle);
}



template <class T, class FX, template <typename> class CONTAINER>
auto find_if(CONTAINER<T> const& haystack, FX const& fn)
{
    return std::find_if(begin(haystack), end(haystack), fn);
}

template <typename ITERATOR>
std::string join(ITERATOR begin, ITERATOR end,
                 const std::string& separator = ", ")
{
    std::ostringstream ss;

    if (begin != end) {
        ss << *begin++;
    }

    while (begin != end) {
        ss << separator;
        ss << *begin++;
    }
    return ss.str();
}

inline std::string spaces(int n)
{
    return std::string(n, ' ');
}

// Indent `text` by prefixing each line with `n` spaces
inline std::string indent(std::string const& text, int n)
{
    std::vector<std::string> target;
    auto v = split(text, "\n");
    std::transform(v.begin(), v.end(), std::back_inserter(target),
                   [prefix = spaces(n)](auto const& s) { return prefix + s; });
    return join(target.begin(), target.end(), "\n");
}

inline bool endsWith(const std::string& name, const std::string& ext)
{
    auto pos = name.rfind(ext);
    return (pos != std::string::npos && pos == name.length() - ext.length());
}

inline bool startsWith(const std::string& name, const std::string& pref)
{
    return name.find(pref) == 0;
}

inline void quote(std::string& s)
{
    s = "\"" + s + "\"";
}

inline bool isUpper(char const& c)
{
    return std::isupper(c) != 0;
}
inline bool isLower(char const& c)
{
    return std::islower(c) != 0;
}

inline bool isUpper(std::string const& s)
{
    return std::all_of(s.begin(), s.end(),
                       static_cast<bool (*)(char const&)>(&isUpper));
}

inline bool isLower(std::string const& s)
{
    return std::all_of(s.begin(), s.end(),
                       static_cast<bool (*)(char const&)>(&isLower));
}

} // namespace utils
