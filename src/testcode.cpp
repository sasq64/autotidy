#include <fmt/format.h>

int doSomething(int code)
{
    return code * -2;
}

int main()
{
    if (doSomething(12345))
        fmt::print("Was negative\n");
    int x = 9 << doSomething(4);
    fmt::print("Hello {}\n", x);
    return 0;
}
