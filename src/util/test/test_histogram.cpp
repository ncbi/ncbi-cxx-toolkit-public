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
    void EstimateNumberOfBins();
    void SimpleLinear(void);
    void SimpleLog(void);
    void CombinedScale(void);
    void CustomDataType(void);
    void Clone(void);

    int Run(void) {
        SimpleLinear();
        SimpleLog();
        CombinedScale();
        CustomDataType();
        EstimateNumberOfBins();
        Clone();
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
        PrintArray<>(h.GetNumberOfBins(), h.GetBinStartsPtr()); \
        cout << "Counters       : "; \
        PrintArray<>(h.GetNumberOfBins(), h.GetBinCountersPtr()); \
        cout << "Total count    : "  << h.GetCount()             << endl; \
        cout << "Anomaly (lower): "  << h.GetLowerAnomalyCount() << endl; \
        cout << "Anomaly (upper): "  << h.GetUpperAnomalyCount() << endl; \
        cout << "Sum            : "  << h.GetSum() << endl; \
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
    PRINT_HEADER("Simple linear monotonic/simmetrical scales");
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
    {{
        typedef CHistogram<unsigned int, unsigned int> H;
        {{
            PRINT_HEADER("Combined scale: monotonic linear scale from 0 to 100 + 1L");
            H h(50, 100, 5, H::eLinear, H::eMonotonic);
            h.AddLeftScale(0, 10, H::eLinear);
            RUN_INT(h, 0, 100);
        }}
        {{
            PRINT_HEADER("Combined scale: monotonic linear scale from 0 to 100 + 1R");
            H h(0, 50, 5, H::eLinear, H::eMonotonic);
            h.AddRightScale(100, 10, H::eLinear);
            RUN_INT(h, 0, 100);
        }}
        {{
            PRINT_HEADER("Combined scale: monotonic linear scale from 0 to 100 + 2L + 2R");
            H h(50, 60, 10, H::eLinear, H::eMonotonic);  // [50:60]  10 x 1
            h.AddLeftScale(20, 5, H::eLinear);           // [20:50]   5 x 6
            h.AddLeftScale(0, 4, H::eLinear);            // [ 0:20]   4 x 5
            h.AddRightScale(70, 5, H::eLinear);          // [60:70]   5 x 2
            h.AddRightScale(100, 6, H::eLinear);         // [70:100]  6 x 5
            RUN_INT(h, 0, 100);
        }}
    }}
    {{
        typedef CHistogram<int, double> H;
        {{
            PRINT_HEADER("Combined scale: monotonic linear scale from 0 to 10 (x10) + log10 to 1000 (x3)");
            H h(0, 10, 10, H::eLinear, H::eMonotonic);
            h.AddRightScale(1000, 3, H::eLog10);
            RUN_INT(h, 0, 1000);
        }}
        {{
            PRINT_HEADER("Combined scale: monotonic linear scale from 0 to -10 (x10) + log10 to -1000 (x3)");
            H h(-10, 0, 10, H::eLinear, H::eMonotonic);
            h.AddLeftScale(-1000, 3, H::eLog10);
            RUN_INT(h, -1000, 0);
        }}
        {{
            PRINT_HEADER("Combined scale: symmetrical log2 from 0 to 10 (x10) + log10 to 1000 (x5)");
            H h(0, 10, 10, H::eLog2, H::eSymmetrical);
            h.AddRightScale(1000, 5, H::eLog10);
            RUN_INT(h, 0, 1000);
        }}
    }}
    {{
        {{
            PRINT_HEADER("Combined scale: mix (int)");
            typedef CHistogram<int> H;
            H h(5, 100, 4, H::eLog, H::eSymmetrical);
            h.AddLeftScale(0, 2, H::eLinear);
            h.AddLeftScale(-5, 3, H::eLinear);
            h.AddLeftScale(-1000, 3, H::eLog2);
            h.AddLeftScale(-10000, 3, H::eLog10);
            h.AddRightScale(110, 5, H::eLinear);
            h.AddRightScale(200, 6, H::eLinear);
            h.AddRightScale(10000, 10, H::eLog2);
            RUN_INT(h, -1000, 1000);
        }}
        {{
            // same as above, but no truncation due int type
            PRINT_HEADER("Combined scale: mix (double)");
            typedef CHistogram<double> H;
            H h(5, 100, 4, H::eLog, H::eSymmetrical);
            h.AddLeftScale(0, 2, H::eLinear);
            h.AddLeftScale(-5, 3, H::eLinear);
            h.AddLeftScale(-1000, 3, H::eLog2);
            h.AddLeftScale(-10000, 3, H::eLog10);
            h.AddRightScale(110, 5, H::eLinear);
            h.AddRightScale(200, 6, H::eLinear);
            h.AddRightScale(10000, 10, H::eLog2);
            RUN_DOUBLE(h, -1000, 1000);
        }}
    }}
}


void CDataHistogramDemoApp::CustomDataType(void)
{
    // Custom data type should have:
    //   - operator T() -- to convert to scale type T ('double' in our case)
    //   - operator >()
    //
    struct SValue {
        // Default constructor
        SValue() : v1(0), v2(0) {};
        // Constructor
        SValue(size_t p1, size_t p2) : v1(p1), v2(p2) {};
        // Conversion to double (scale type).
        operator double(void) const {
            return (double)v1 + (v2 == 0 ? 0 : 1/(double)v2);
        }
        // Comparison: operatior >
        bool operator> (const SValue& other) const {
            if (v1 > other.v1) return true;
            if (v1 < other.v1) return false;
            return v2 > other.v2;
        }
        // Some value(s)
        size_t v1;
        size_t v2;
    };

    PRINT_HEADER("Custom data type: symmetrical natural logarithmic scale from (0,0) to (100,0)");
    typedef CHistogram<SValue, double, unsigned long> H;
    H h(SValue(0,0), SValue(100,0), 10, H::eLog, H::eSymmetrical);

    // Generate and add some random data
    std::uniform_int_distribution<size_t>  d1(0, 99);
    std::uniform_int_distribution<size_t>  d2(0, 9999);

    for (size_t i = 0; i < 1000; i++) {
        h.Add(SValue(d1(rnd),d2(rnd)));
    }
    // Print result
    PRINT_STATS(h);
}


void CDataHistogramDemoApp::EstimateNumberOfBins(void)
{
    PRINT_HEADER("Estimate number of bins");
    typedef CHistogram<> H;

    const int N = 12;
    const size_t num[N] = { 1, 3, 7, 10, 20, 40, 60, 100, 500, 1000, 5000, 20000 }; 

    cout << "N          : ";
    for (int i = 0; i < N; i++) {
        cout << num[i] << " ";
    }
    cout << "\nSquareRoot : ";
    for (int i = 0; i < N; i++) {
        cout << H::EstimateNumberOfBins(num[i], H::eSquareRoot) << " ";
    }
    cout << "\nJuran      : ";
    for (int i = 0; i < N; i++) {
        cout << H::EstimateNumberOfBins(num[i], H::eJuran) << " ";
    }
    cout << "\nSturge     : ";
    for (int i = 0; i < N; i++) {
        cout << H::EstimateNumberOfBins(num[i], H::eSturge) << " ";
    }
    cout << "\nRice       : ";
    for (int i = 0; i < N; i++) {
        cout << H::EstimateNumberOfBins(num[i], H::eRice) << " ";
    }
    cout << endl;
}


void CDataHistogramDemoApp::Clone(void)
{
    PRINT_HEADER("CloneStructure() and move semantics");
    typedef CHistogram<> H;

    H h(0, 10, 10, H::eLinear);
    RUN_INT(h, 0, 10);

    // clone
    {{
        H hclone(h.Clone());
        PRINT_STATS(hclone);
    }}
    {{
        H hclone;
        hclone = h.Clone();
        PRINT_STATS(hclone);
    }}
    {{
        H hclone(0, 100000000, 5, H::eLog10);
        hclone = h.Clone(H::eCloneStructureOnly);
        PRINT_STATS(hclone);
    }}

    // clone and steal counters
    {{
        // Create clone and add counters to it
        H hclone(h.Clone(H::eCloneStructureOnly));
        for (size_t i = 0; i < 10; i++) {
            hclone.Add(rand() % 10);
        }
        PRINT_STATS(hclone);
        // Move counters to original histogram (add)
        h.StealCountersFrom(hclone);
        PRINT_STATS(h);
    }}
}


int main(int argc, char** argv)
{
    return CDataHistogramDemoApp().AppMain(argc, argv);
}
