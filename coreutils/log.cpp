#include "log.h"

#include <ctime>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unordered_map>

static std::string logSource = "Grappix";

#ifdef ANDROID

#    include <android/log.h>
#    define LOG_PUTS(x)                                                        \
        ((void)__android_log_print(ANDROID_LOG_INFO, logSource.c_str(), "%s",  \
                                   x))

#else

#    ifdef LOG_INCLUDE
#        include LOG_INCLUDE
#    endif

#    ifndef LOG_PUTS // const char *
#        define LOG_PUTS(x)                                                    \
            (fwrite(x, 1, strlen(x), stdout), putchar(10), fflush(stdout))
#    endif

#endif
namespace logging {

using namespace std;

Level defaultLevel = Debug;
static FILE* logFile = nullptr;

void log(const std::string& text)
{
    log(defaultLevel, text);
}

static std::mutex logm;

#ifdef __APPLE__
static int threadId = -1;
#else
static thread_local int threadId = -1;
#endif
static int threadCount = 0;

void log(Level level, const std::string& text)
{

    static const char* levelChar = "VDIWE";

    if (level >= defaultLevel) {
        const char* cptr = text.c_str();
        std::lock_guard<std::mutex> lock(logm);
        LOG_PUTS(cptr);
    }
    if (logFile) {

        if (threadId == -1)
            threadId = threadCount++;

        std::lock_guard<std::mutex> lock(logm);

        // std::time_t t = chrono::system_clock::to_time_t(system_clock::now());
        // const std::string s = put_time(t, "%H:%M:%S - ");
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        string ts;
        if (threadCount > 1)
            ts = utils::format("%c: %02d:%02d.%02d -#%d- ", levelChar[level],
                               t->tm_hour, t->tm_min, t->tm_sec, threadId);
        else
            ts = utils::format("%c: %02d:%02d.%02d - ", levelChar[level],
                               t->tm_hour, t->tm_min, t->tm_sec);

        fwrite(ts.c_str(), 1, ts.length(), logFile);
        fwrite(text.c_str(), 1, text.length(), logFile);
        char c = text[text.length() - 1];
        if (c != '\n' && c != '\r')
            putc('\n', logFile);
        fflush(logFile);
    }
}

void log2(const char* fn, int line, Level level, const std::string& text)
{
    using namespace std;
    static int termType = 0;
    // const auto &space = LogSpace::spaces[fn];
    if (true) { // space.second || space.first == "") {
        char temp[2048];

        if (!termType) {
            const char* tt = getenv("TERM");
            // log(level, "TERMTYPE %s", tt ? tt : "NULL");
            termType = 1;
            if (tt) {
                if (strncmp(tt, "xterm", 5) == 0) {
                    termType = 2;
                }
            }
        }

        if (termType == 2) {
            int cs = 0;
            for (size_t i = 0; i < strlen(fn); i++)
                cs = cs ^ fn[i];

            cs = (cs % 6) + 1;

            sprintf(temp, "\x1b[%dm[%s:%d]\x1b[%dm ", cs + 30, fn, line, 37);
        } else {
            sprintf(temp, "[%s:%d] ", fn, line);
        }
        log(level, std::string(temp).append(text));
    }
}

void setLevel(Level level)
{
    defaultLevel = level;
}

void setOutputFile(const std::string& fileName)
{
    std::lock_guard<std::mutex> lock(logm);
    if (logFile)
        fclose(logFile);
    logFile = fopen(fileName.c_str(), "wb");
}

// void setLogSpace(const std::string &sourceFile, const std::string &function,
// const std::string &spaceName) { 	LogSpace::spaces[sourceFile] = spaceName;
//}

} // namespace logging
