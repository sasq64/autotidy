#include <fmt/format.h>
#include <vector>

struct Vector
{
    float x;
    float y;
    float z;
};

class Camera
{
    int fov;

public:
    Vector pos;

    Camera(int fov) : fov(fov) {}
};

int doSomething(int code)
{
    Camera cam{3};
    int total = 0;
    std::vector<int> s{1, 2, 3, 4};
    for (unsigned i = 0; i < s.size(); i++)
        total += s[i];

    return total * code * -2;
}

int main()
{
    if (doSomething(12345))
        fmt::print("Was negative\n");
    int x = 9 << doSomething(4);
    fmt::print("Hello {}\n", x);
    return 0;
}
