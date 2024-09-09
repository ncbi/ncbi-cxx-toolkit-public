#if 0
#include <ncbi_pch.hpp>
#endif
#include <atomic>

struct S { int x, y; };
int f(std::atomic<S> &as, S &s1, S s2)
{
    bool b = as.compare_exchange_strong(s1, s2);
    return b ? as.load().x : as.load().y;
}

int main(int, char**)
{
    std::atomic<S> as;
    S s1, s2;
    return f(as, s1, s2);
}
