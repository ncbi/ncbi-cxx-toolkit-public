#if 0
#include <ncbi_pch.hpp>
#endif
#include <atomic>

struct S { int x, y; };
int f(std::atomic<S> &s) { return s.load().x; }

int main(int, char**)
{
    std::atomic<S> s;
    return f(s);
}
