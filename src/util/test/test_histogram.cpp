/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Vladimir Ivanov
*
* File Description:
*   Demo program for CHistogram class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/data_histogram.hpp>
#include <random>

// must be last
#include <common/test_assert.h>

USING_NCBI_SCOPE;


class CDataHistogramDemoApp : public CNcbiApplication
{
public:
    void Init(void) {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "CHistogram demo program");
        SetupArgDescriptions(arg_desc.release());
        // Init randomizer
        rnd.seed(seed());
    }
    // Simple linear scale histograms
    void SimpleLinear(void);
    // Simple logarithmic scale histograms
    void SimpleLog(void);
    // Combined scale histograms
    void CombinedScale(void);

    int Run(void) {
        SimpleLinear();
        SimpleLog();
        //CombinedScale();
        return 0;
    }

private:
    std::random_device seed;
    std::default_random_engine rnd;
};


// We use macro to deal with histograms for simplicity,
// it works with any CHistogram template instances.

// Simple run: add 100 integer random values to the histogram within
// specified range with some anomalies (< 5%).
//
#define RUN_INT(histogram, range_min, range_max) \
    {{ \
        int delta = int((float(range_max) - range_min)/10); \
        if (!delta) delta = 2; \
        ADD_INT_DATA(histogram, 100, 95, range_min - delta, range_max + delta, range_min, range_max); \
        PRINT_STATS(histogram); \
    }}

// Simple run: add 100 double random values to the histogram within
// specified range with some anomalies (< 5%).
//
#define RUN_DOUBLE(histogram, range_min, range_max) \
    {{ \
        double delta = (range_max - range_min)/10; \
        ADD_DOUBLE_DATA(histogram, 100, 95, range_min - delta, range_max + delta, range_min, range_max); \
        PRINT_STATS(histogram); \
    }}

// Add values: 'n' integer samples in range [range_min, range_max] with 'percent'% values
// falling into major scale range [major_min, major_max].
//
#define ADD_INT_DATA(histogram, n, percent, range_min, range_max, major_min, major_max) \
    {{ \
        std::uniform_int_distribution<int> range_dist(range_min, range_max); \
        std::uniform_int_distribution<int> major_dist(major_min, major_max); \
        int v; \
        ADD_DATA(histogram, n, percent, range_min, range_max, major_min, major_max); \
    }}

// Add values: 'n' double samples in range [range_min, range_max] with 'percent'% values
// falling into major scale range [major_min, major_max].
//
#define ADD_DOUBLE_DATA(histogram, n, percent, range_min, range_max, major_min, major_max) \
    {{ \
        std::uniform_real_distribution<double> range_dist(range_min, range_max); \
        std::uniform_real_distribution<double> major_dist(major_min, major_max); \
        double v; \
        ADD_DATA(histogram, n, percent, range_min, range_max, major_min, major_max); \
    }}

#define ADD_DATA(histogram, n, percent, range_min, range_max, major_min, major_max) \
    {{ \
        size_t n_major = 0; \
        size_t n_total = 1; \
        \
        for (size_t i = 0; i < n; i++) { \
            if ( n_major * 100 / n_total <= percent) { \
                v = major_dist(rnd); \
                n_major++; \
            } else { \
                v = range_dist(rnd); \
            } \
            histogram.Add(v); \
            n_total++; \
        } \
    }}


// Print test header
#define PRINT_HEADER(msg) \
    cout << endl \
         << string(70, '-') << endl \
         << msg << endl \
         << string(70, '-') << endl \
         << endl

// Print histogram statistics.
#define PRINT_STATS(h) \
    {{ \
        cout << "Range          : [" << h.GetMin() << ":" << h.GetMax() << "]" << endl; \
        cout << "Number of bins : "  << h.GetNumberOfBins() << endl; \
        cout << "Starts         : "; \
        PrintArray<>(h.GetNumberOfBins(), h.GetBinStarts()); \
        cout << "Counters       : "; \
        PrintArray<>(h.GetNumberOfBins(), h.GetBinCounters()); \
        cout << "Total count    : "  << h.GetCount()             << endl; \
        cout << "Anomaly (lower): "  << h.GetLowerAnomalyCount() << endl; \
        cout << "Anomaly (upper): "  << h.GetUpperAnomalyCount() << endl; \
        cout << endl; \
    }}

template<typename T>
void PrintArray(size_t n, const T* arr) 
{
    for (size_t i = 0; i < n; i++) {
        cout << arr[i];
        if (n > 1  &&  i < n - 1) { cout << ", "; };
    }
    cout << endl;
}


void CDataHistogramDemoApp::SimpleLinear(void)
{
    {{
        cout << "Linear scale from 0 to 10, 10 bins with size 1 (10x1)" << endl;
        CHistogram<int> h(0, 10, 10);
        RUN_INT(h, 0, 10);
    }}
    {{
        cout << "Linear scale from 0 to 50, 10 bins with size 5 (10x5)" << endl;
        CHistogram<int> h(0, 50, 10);
        RUN_INT(h, 0, 50);
    }}
    {{
        cout << "Linear scale from 0 to 50, int scale, 11 bins (10x4 + 1x10)" << endl;
        CHistogram<int> h(0, 50, 11);
        RUN_INT(h, 0, 50);
    }}
    {{
        cout << "Linear scale from 0 to 50, double scale, 11 bins with size 4.5454" << endl;
        CHistogram<int, double> h(0, 50, 11);
        RUN_INT(h, 0, 50);
    }}
    {{
        cout << "Linear scale from 0 to 50, int symmetrical scale, 11 bins (5x4 + 1x10 + 5x4)" << endl;
        CHistogram<int> h(0, 50, 11, CHistogram<>::eLinear, CHistogram<>::eSymmetrical);
        RUN_INT(h, 0, 50);
    }}
    {{
        cout << "Linear scale from 0 to 50, int symmetrical scale, 12 bins (1x5 + 10x4 + 1x5)" << endl;
        CHistogram<int> h(0, 50, 12, CHistogram<>::eLinear, CHistogram<>::eSymmetrical);
        RUN_INT(h, 0, 50);
    }}
    {{
        // For double scale type the scale view doesn't matter, because we have no truncation
        // all bins have same size -- so we have the same results as for eMonotonic.
        cout << "Simple linear scale from 0 to 50, double symmetrical scale, 11 bins with size 4.5454" << endl;
        typedef CHistogram<int, double> H;
        H h(0, 50, 11, H::eLinear, H::eSymmetrical);
        RUN_INT(h, 0, 50);
    }}
}


void CDataHistogramDemoApp::SimpleLog(void)
{
    // We use a common logarithmic scales (eLog10) here for simplicity,
    // but you feel free to use binary or natural logarithmic scales instead.

    {{
        PRINT_HEADER("Monotonic common logarithmic scale from 1 to 1000");

        typedef CHistogram<unsigned int, double> H;
        {{
            H h(1, 1000, 3, H::eLog10, H::eMonotonic);
            RUN_INT(h, 1, 1000);
        }}
        {{
            H h(1, 1000, 6, H::eLog10, H::eMonotonic);
            RUN_INT(h, 1, 1000);
        }}
        {{
            H h(1, 1000, 20, H::eLog10, H::eMonotonic);
            RUN_INT(h, 1, 1000);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale from 0 to 1000");

        typedef CHistogram<unsigned int, double> H;
        {{
            H h(0, 1000, 3, H::eLog10, H::eMonotonic);
            RUN_INT(h, 0, 1000);
        }}
        {{
            H h(0, 1000, 6, H::eLog10, H::eMonotonic);
            RUN_INT(h, 0, 1000);
        }}
        {{
            H h(0, 1000, 20, H::eLog10, H::eMonotonic);
            RUN_INT(h, 0, 1000);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale for negative numbers: from -1000 to -1");

        typedef CHistogram<int, double> H;
        {{
            H h(-1000, -1, 3, H::eLog10, H::eMonotonic);
            RUN_INT(h, -1000, -1);
        }}
        {{
            H h(-1000, -1, 6, H::eLog10, H::eMonotonic);
            RUN_INT(h, -1000, -1);
        }}
        {{
            H h(-1000, -1, 20, H::eLog10, H::eMonotonic);
            RUN_INT(h, -1000, -1);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale for mixed range: -100 to +100");

        typedef CHistogram<int, double> H;
        {{
            H h(-100, 100, 3, H::eLog10, H::eMonotonic);
            RUN_INT(h, -100, 100);
        }}
        {{
            H h(-100, 100, 6, H::eLog10, H::eMonotonic);
            RUN_INT(h, -100, 100);
        }}
        {{
            H h(-100, 100, 20, H::eLog10, H::eMonotonic);
            RUN_INT(h, -100, 100);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale for small numbers");

        typedef CHistogram<double, double> H;
        {{
            H h(0.00001, 1, 3, H::eLog10, H::eMonotonic);
            RUN_DOUBLE(h, 0.00001, 1);
        }}
        {{
            H h(0.000000001, 1, 3, H::eLog10, H::eMonotonic);
            RUN_DOUBLE(h, 0.000000001, 1);
        }}
        {{
            H h(0.000000001, 0.001, 3, H::eLog10, H::eMonotonic);
            RUN_DOUBLE(h, 0.000000001, 0.001);
        }}
        {{
            H h(0.000000001, 100, 6, H::eLog10, H::eMonotonic);
            RUN_DOUBLE(h, 0.000000001, 100);
        }}
    }}
    {{
        PRINT_HEADER("Symmetrical common logarithmic scale from 0 to 1000");

        typedef CHistogram<unsigned int, double> H;

        for (auto i = 1; i <= 7; i++) {
            H h(0, 1000, i, H::eLog10, H::eSymmetrical);
            RUN_INT(h, 0, 1000);
        }
        {{
            H h(0, 1000, 20, H::eLog10, H::eSymmetrical);
            RUN_INT(h, 0, 1000);
        }}
        {{
            H h(0, 1000, 25, H::eLog10, H::eSymmetrical);
            RUN_INT(h, 0, 1000);
        }}
    }}
    {{
        PRINT_HEADER("Symmetrical common logarithmic scale from -1000 to 1000");

        typedef CHistogram<int, double> H;

        for (auto i = 1; i <= 7; i++) {
            H h(-1000, 1000, i, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -1000, 1000);
        }
        {{
            H h(-1000, 1000, 20, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -1000, 1000);
        }}
        {{
            H h(-1000, 1000, 25, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -1000, 1000);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale for mixed range: -100 to +100");

        typedef CHistogram<int, double> H;

        for (auto i = 1; i <= 7; i++) {
            H h(-100, 100, i, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -100, 100);
        }
        {{
            H h(-100, 100, 20, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -100, 100);
        }}
        {{
            H h(-100, 100, 25, H::eLog10, H::eSymmetrical);
            RUN_INT(h, -100, 100);
        }}
    }}
    {{
        PRINT_HEADER("Monotonic common logarithmic scale for small numbers");

        typedef CHistogram<double, double> H;

        for (auto i = 1; i <= 7; i++) {
            H h(0.000000001, 1, i, H::eLog10, H::eSymmetrical);
            RUN_DOUBLE(h, 0.000000001, 1);
        }
        {{
            H h(0.000000001, 1, 3, H::eLog10, H::eSymmetrical);
            RUN_DOUBLE(h, 0.000000001, 1);
        }}
        {{
            H h(0.000000001, 0.001, 3, H::eLog10, H::eSymmetrical);
            RUN_DOUBLE(h, 0.000000001, 0.001);
        }}
        {{
            H h(0.000000001, 0.001, 7, H::eLog10, H::eSymmetrical);
            RUN_DOUBLE(h, 0.000000001, 0.001);
        }}
        {{
            H h(0.000000001, 100, 10, H::eLog10, H::eSymmetrical);
            RUN_DOUBLE(h, 0.000000001, 100);
        }}
    }}
}


void CDataHistogramDemoApp::CombinedScale(void)
{
    // We use a common logarithmic scales (eLog10) here for simplicity,
    // but you feel free to use binary or natural logarithmic scales instead.

    {{
        PRINT_HEADER("Monotonic common logarithmic scale from 1 to 1000");
        typedef CHistogram<unsigned int, unsigned int> H;
        {{
            H h(1, 1000, 3, H::eLog10, H::eMonotonic);

//    void AddLeftScale (TValue min_value, unsigned n_bins, EScaleType scale_type);
//    void AddRightScale(TValue max_value, unsigned n_bins, EScaleType scale_type);

            RUN_INT(h, 1, 1000);
        }}
if(0)
        {{
            H h(1, 1000, 6, H::eLog10, H::eMonotonic);
            RUN_INT(h, 1, 1000);
        }}
if(0)
        {{
            H h(1, 1000, 20, H::eLog10, H::eMonotonic);
            RUN_INT(h, 1, 1000);
        }}
    }}
}

#if 0
   
    // 2 scale collectors
    {{
        // Double scale for unsigned int values, size_t counter type.
        typedef CHistogram<unsigned int, double, size_t> H;
        {{
            // Most relevant data at start of the scales, starting from 0.
            cout << "2 scales - Linear scale from 0 to 10, and log to 100, step 0.5 for both" << endl;
            H::Collector c(
                H::MajorScale(0, 10, 0.5, H::Scale::eLinear),
                H::MinorScale(100, 0.5, H::Scale::eLog)
            );
            RUN_INT(c, 0, 100);
        }}
        {{
            // Reverse to above. Most relevant data at the end of the scales.
            cout << "2 scales - Linear scale from 90 to 100, and log from 90 to 0, step 0.5 for both" << endl;
            H::Collector c(
                H::MinorScale(0, 0.5, H::Scale::eLog),
                H::MajorScale(90, 100, 0.5, H::Scale::eLinear)
            );
            RUN_INT(c, 0, 100);
        }}
        {{
            cout << "2 scales - Linear scale from 0 to 10, and log to 100, step 0.5 for both" << endl;
            H::Collector c(
                H::MajorScale(0, 10, { 0.5, 0 }, H::Scale::eLinear),  // step starts from 0.5
                H::MinorScale(100, { 0.5, 1 }, H::Scale::eLog2)       // step starts from 1
            );
            RUN_INT(c, 0, 100);
        }}
        {{
            cout << "2 scales - Custon function monotonic scale from 1000 to 1010, and log from 100 to 0" << endl;
            H::Collector c(
                H::MinorScale(0, 1, H::Scale::eLog),
                H::MajorScale(1000, 1010, 1, fCustomFunc)
            );
            RUN_INT(c, 0, 100);
        }}
    }}

    // 3 scale collectors
    {{
        {{
            // Double scale for long values, size_t counter type.
            typedef CHistogram<long, double, size_t> H;
            cout << "3 scales - Linear scale for range [-10,+10] step 1, and log to -1000/+1000 with steps 0.7/0.5 accordingly" << endl;
            H::Collector c(
                H::MinorScale(-1000,    0.7, H::Scale::eLog),
                H::MajorScale(-10, +10,   1, H::Scale::eLinear),
                H::MinorScale(+1000,    0.5, H::Scale::eLog)
            );
            RUN_INT(c, -1000, 1000);
        }}
        {{
            // Double scale for double values, 'unsigned int' counter type.
            typedef CHistogram<double, double, unsigned int> H;
            cout << "3 scales - Linear scale for range [5,10] step 0.4, log2 on [0,5]/step 0.1, log10 on [10,100000] step 1" << endl;
            H::Collector c(
                H::MinorScale(0,     0.1, H::Scale::eLog2),
                H::MajorScale(5, 10, 0.4, H::Scale::eLinear),
                H::MinorScale(100000,  1, H::Scale::eLog10)
            );
            RUN_INT(c, 0, 100000);
        }}
        {{
            // Double scale for double values, 'unsigned int' counter type.
            typedef CHistogram<double, double, unsigned int> H;
            cout << "3 scales - Natural logaritm scale for [1000,1005] step 0.1, and log 10 for left/right parts - [0,1000] / [1005/10000] step 1" << endl;
            H::Collector c(
                H::MinorScale(0,            1, H::Scale::eLog10),
                H::MajorScale(1000, 1005, 0.1, H::Scale::eLog),
                H::MinorScale(100000,       1, H::Scale::eLog10)
            );
            RUN_INT(c, 0, 100000);
        }}
    }}

    // Custom data types
    {{
        // Histogram example with a custom data type and double scale
        {{
            // Custom data type should have:
            //   - default constructor
            //   - operator T() -- to convert to scale type T ('double' in our case)
            //   - operator >()
            //
            struct SValue {
                // Constructor / default constructor (0,0)
                SValue(size_t p1 = 0, double p2 = 0) : v1(p1), v2(p2) {};
                // Conversion to double (scale type).
                operator double(void) const {
                    return (double)v1 + v2;
                }
                // Comparison: operatior >
                bool operator> (const SValue& other) const {
                    if (v1 > other.v1) return true;
                    if (v1 < other.v1) return false;
                    return v2 > other.v2;
                }
                // Some value(s)
                size_t v1;
                double v2;
            };

            typedef CHistogram<SValue, double, unsigned long> H;
            cout << "Custom data type - Symmetrical natural logarithmic scale from (0,0) to (100,0), step 0.6" << endl;
            H::Scale s(SValue(0,0), SValue(100,0), 0.6, H::Scale::eLog, H::Scale::eSymmetrical);
            H::Collector c(s);

            // Generate and add some random data
            std::uniform_int_distribution<size_t>  d1(0, 100); // 100 instead of 99 for some anomalies on right
            std::uniform_real_distribution<double> d2(0, 1.0);
            for (size_t i = 0; i < 1000; i++) {
                c.Add(SValue(d1(rnd),d2(rnd)));
            }
            // Print result
            PRINT_STATS(c);
        }}
    }}
#endif


int main(int argc, char** argv)
{
    return CDataHistogramDemoApp().AppMain(argc, argv);
}
