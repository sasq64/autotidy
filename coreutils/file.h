#pragma once

#include <array>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <io.h>
#    undef ERROR
#else
#    include <unistd.h>
#endif

namespace utils {

class io_exception : public std::exception
{
public:
    explicit io_exception(std::string m = "IO Exception") : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

class file_not_found_exception : public std::exception
{
public:
    explicit file_not_found_exception(const std::string& fileName = "")
        : msg(std::string("File not found: ") + fileName)
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

class File
{
public:
    enum Mode
    {
        Read = 1,
        Write = 2
    };

    enum Seek
    {
        Set = SEEK_SET,
        Cur = SEEK_CUR,
        End = SEEK_END
    };

    static File& getStdIn()
    {
        static File _stdin{stdin};
        return _stdin;
    }

    // Constructors

    File() {}

    explicit File(FILE* fp) : fp(fp) {}

    /* explicit File(fs::path const& p, const Mode mode = Read) */
    /* { */
    /*     openAndThrow(p.string().c_str(), mode); */
    /* } */

    explicit File(const char* name, const Mode mode = Read)
    {
        openAndThrow(name, mode);
    }

    explicit File(const std::string& name, const Mode mode = Read)
    {
        openAndThrow(name.c_str(), mode);
    }

    File(const File&) = delete;

    File& operator=(const File&) = delete;
    File& operator=(File&& other)
    {
        fp = other.fp;
        other.fp = nullptr;
        return *this;
    }

    File(File&& other) noexcept
    {
        fp = other.fp;
        other.fp = nullptr;
    }

    operator FILE*() { return fp; }

    // Destructor

    virtual ~File() { close(); }

    // Reading

    template <typename T> T read() const
    {
        T t;
        if (fread(&t, 1, sizeof(T), fp) != sizeof(T))
            throw io_exception("Could not read object");
        return t;
    }

    template <typename T> size_t read(T* target, size_t bytes) const noexcept
    {
        return fread(target, 1, bytes, fp);
    }

    template <typename T, size_t N>
    size_t read(std::array<T, N>& target) const noexcept
    {
        return read(&target[0], target.size() * sizeof(T));
    }

    std::string readAll()
    {
        fseek(fp, 0, SEEK_END);
        size_t sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::string target;
        target.resize(sz);
        if(fread(&target[0], 1, sz, fp) != sz)
            throw io_exception("Could not read file");
        return target;
    }

    std::string readString(int maxlen = -1) const
    {
        std::vector<char> data;
        int c = 0;
        while ((int)data.size() != maxlen) {
            c = fgetc(fp);
            if (c == 0 || c == EOF)
                break;
            data.push_back(c);
        }
        return std::string(data.begin(), data.end());
    }

    std::string readLine() const
    {
        std::vector<char> lineTarget(10);
        int endp = 0;
        while (true) {
            char* ptr = fgets(&lineTarget[endp], lineTarget.size() - endp, fp);
            if (!ptr) {
                if (eof())
                    break;
                throw io_exception("readLine() failed");
            }
            // We read something. Strip LF if found
            int len = strlen(ptr);
            bool found = false;
            while ((len > 0) && (ptr[len - 1] == '\n')) {
                found = true;
                len--;
            }
            if (found && ptr[len - 1] == '\r')
                len--;
            ptr[len] = 0;
            if (found)
                break;
            if (eof())
                break;
            // If LF not found we need to read more
            endp += len;
            lineTarget.resize(lineTarget.size() + 10);
        }
        return std::string(&lineTarget[0]);
    }

    void writeln(std::string const& line) const noexcept
    {
        fputs((line + "\n").c_str(), fp);
    }

    void writeString(std::string const& line) const noexcept
    {
        fwrite(line.c_str(), 1, line.length(), fp);
    }

    size_t getSize() const noexcept
    {
        auto pos = tell();
        seek(0, Seek::End);
        auto sz = tell();
        seek(pos);
        return sz;
    }

    std::vector<uint8_t> readAll() const
    {
        std::vector<uint8_t> data;
        data.resize(getSize());
        seek(0);
        if (!data.empty()) {
            int rc = read(&data[0], data.size());
            if (rc != (int)data.size())
                throw io_exception("ReadAll failed");
        }
        return data;
    }

    bool eof() const { return feof(fp) != 0; }

    inline auto lines() &;
    inline auto lines() &&;

    // Writing

    template <typename T> void write(const T& t) const
    {
        if (fwrite(&t, 1, sizeof(T), fp) != sizeof(T))
            throw io_exception("Could not write object");
    }

    template <typename T>
    size_t write(const T* target, size_t bytes) const noexcept
    {
        return fwrite(target, 1, bytes, fp);
    }

    void seek(int64_t pos, int whence = Seek::Set) const
    {
#ifdef _WIN32
        _fseeki64(fp, pos, whence);
#else
        fseek(fp, pos, whence);
#endif
    }

    size_t tell() const
    {
#ifdef _WIN32
        return _ftelli64(fp);
#else
        return ftell(fp);
#endif
    }

    bool open(const char* name, Mode mode) noexcept
    {
        fp = fopen(name, mode == Read ? "rb" : "wb");
        return fp != nullptr;
    }

    void openAndThrow(const char* name, Mode mode)
    {
        if (!open(name, mode))
            throw io_exception(std::string("Could not open ") + name);
    }

    void close() noexcept
    {
        if (fp)
            fclose(fp);
        fp = nullptr;
    }

    FILE* filePointer() { return fp; }

    void flush() { fflush(fp); }

private:
    FILE* fp = nullptr;
};

template <bool REFERENCE> class LineReader
{
    friend File;

    explicit LineReader(File& af) : f(af) {}
    explicit LineReader(File&& af) : f(std::move(af)) {}

    typename std::conditional<REFERENCE, File&, File>::type f;
    std::string line;

    struct iterator
    {
        iterator(File& f, int64_t offset) : f(f), offset(offset)
        {
            if (offset >= 0) {
                f.seek(offset);
                line = f.readLine();
            }
        }

        File& f;
        int64_t offset;
        std::string line;

        bool operator!=(const iterator& other) const
        {
            return offset != other.offset;
        }

        std::string operator*() const { return line; }

        iterator& operator++()
        {
            if (f.eof())
                offset = -1;
            else {
                offset = f.tell();
                line = f.readLine();
                if (f.eof()) {
                    offset = -1;
                }
            }
            return *this;
        }
    };

public:
    iterator begin() { return iterator(f, 0); }
    iterator end() { return iterator(f, -1); }
};

auto File::lines() &
{
    return LineReader<true>(*this);
}

auto File::lines() &&
{
    return LineReader<false>(std::move(*this));
}
} // namespace utils
