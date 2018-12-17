#pragma once

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>
#include <vector>

inline char getch()
{
    char buf = 0;
    struct termios old
    {};
    if (tcgetattr(0, &old) < 0) {
        throw new std::runtime_error("tcsetattr()");
    }
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0) {
        throw new std::runtime_error("tcsetattr ICANON");
    }
    if (read(0, &buf, 1) < 0) {
        throw new std::runtime_error("read()");
    }
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0) {
        throw new std::runtime_error("tcsetattr ~ICANON");
    }
    return (buf);
}

inline std::string currentDir()
{
    char buf[4096] = {0};
    getcwd(&buf[0], sizeof(buf));
    return std::string(&buf[0]);
}

inline void copyFile(std::string const& target, std::string const& source)
{
    std::ifstream src(source, std::ios::binary);
    std::ofstream dst(target, std::ios::binary);
    dst << src.rdbuf();
}

inline std::vector<char> readFile(std::string const& fileName)
{
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        return buffer;
    }
    return buffer;
}

template<typename T>
inline void writeFile(std::string const& outputFile, T const& contents)
{
    std::ofstream out(outputFile);
    out << contents;
    out.close();
}
