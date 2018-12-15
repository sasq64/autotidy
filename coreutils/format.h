#pragma once

#include <cstring>
#include <vector>

#include <fmt/format.h>

namespace fmt {

/* inline void format_arg(BasicFormatter<char>& bf, const char*& fmt_str, */
/*                        const bool*& v) { */
/*     bf.writer() << "PTR"; */
/* } */

template <typename T>
inline void format_arg(BasicFormatter<char>& bf, const char*& fmt_str,
                       const std::vector<T>& v)
{
    bool first = true;
    for (const auto& a : v) {
        bf.writer() << (first ? '[' : ' ') << a;
        first = false;
    }
    bf.writer() << ']';
    bf.writer() << fmt_str;
}
} // namespace fmt
#include <fmt/printf.h>

namespace utils {

template <typename T> struct FixPtr
{
    using type = T;
};

template <typename T> struct FixPtr<T*>
{
    using type = void*;
};

template <class... A> std::string format(const std::string& fmt, A&&... args)
{
    return fmt::sprintf(fmt, std::forward<A>(args)...);
}

template <class... A> void print_fmt(const std::string& fmt, A&&... args)
{
    fmt::printf(fmt, std::forward<A>(args)...);
}

template <class... A> void println(const std::string& fmt = "", A&&... args)
{
    fmt::printf(fmt + "\n", std::forward<A>(args)...);
}

} // namespace utils
