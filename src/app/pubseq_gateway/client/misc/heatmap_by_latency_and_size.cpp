// To make svn hooks happy
#if 0
#include <ncbi_pch.hpp>
#endif

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

struct SPoint
{
    double latency;
    size_t size;
    double log_latency;
    double log_size;
    double heat = 0.0;
    SPoint(double l, size_t s, double ll, double ls) : latency(l), size(s), log_latency(ll), log_size(ls) {}
};

int main(int argc, char* argv[])
{
    using namespace std;

    auto r = [](double v) { return log10(v * 10); };

    double latency;
    size_t size;
    vector<double> log_latency_tics, log_size_tics;
    vector<SPoint> data;

    // Reading x and y (latency and size)
    while (cin >> latency >> size) {
        auto log_latency = r(latency);
        auto log_size = r(size);
        log_latency_tics.push_back(log_latency);
        log_size_tics.push_back(log_size);
        data.emplace_back(latency, size, log_latency, log_size);
    }

    if (data.size() <= 1) return 0;

    sort(log_latency_tics.begin(), log_latency_tics.end());
    sort(log_size_tics.begin(), log_size_tics.end());

    auto lmax = log_latency_tics.back() - log_latency_tics.front();
    auto lmaxp2 = lmax * lmax;
    auto smax = log_size_tics.back() - log_size_tics.front();
    auto smaxp2 = smax * smax;
    auto rmax = lmaxp2 + smaxp2;

    // Writing x, y and z (latency, size and 'heat')
    for (size_t i = 0; i < data.size(); ++i) {
        auto& p = data[i];

        for (size_t j = i + 1; j < data.size(); ++j) {
            if (j != i) {
                auto& o = data[j];
                auto ld = p.log_latency - o.log_latency;
                auto sd = p.log_size - o.log_size;
                auto r = ld * ld / lmaxp2 + sd * sd / smaxp2;

                if (r < rmax / 10000) {
                    auto heat = 1 / (1 + r * r * r * r);
                    p.heat += heat;
                    o.heat += heat;
                }
            }
        }

        cout << p.latency << ' ' << p.size << ' ' << p.heat << endl;
    }
}
