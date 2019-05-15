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
    }
    int  Run(void) {
        TestHistogram();
        return 0;
    }
public:
    void TestHistogram(void);
};


template<typename T>
void PrintArray(size_t n, const T* arr) 
{
    for (size_t i = 0; i < n; i++) {
        cout << arr[i];
        if (n > 1  &&  i < n - 1) { cout << ", "; };
    }
    cout << endl;
}

// Add values: 'n' samples in range [range_min, range_max] with 'percent'% values
// falling into major scale range [major_min, major_max].
//
#define ADD_DATA(collector, n, percent, range_min, range_max, major_min, major_max) \
    {{ \
        std::uniform_int_distribution<int> range_dist(range_min, range_max); \
        std::uniform_int_distribution<int> major_dist(major_min, major_max); \
        \
        size_t n_major = 0; \
        size_t n_total = 1; \
        int v; \
        \
        for (size_t i = 0; i < n; i++) { \
            if ( n_major * 100 / n_total <= percent) { \
                v = major_dist(rnd); \
                n_major++; \
            } else { \
                v = range_dist(rnd); \
            } \
            collector.Add(v); \
            n_total++; \
        } \
    }}

// Print collectors statistics.
// 
#define PRINT_STATS(collector) \
    {{ \
        cout << endl; \
        cout << "Range          : [" << collector.GetMin() << ":" << collector.GetMax() << "]" << endl; \
        cout << "Number of bins : " << collector.GetNumberOfBins()  << " : " \
                                    << collector.GetNumberOfBins(1) << "-" \
                                    << collector.GetNumberOfBins(2) << "-" \
                                    << collector.GetNumberOfBins(3) << endl; \
        cout << "Starts         : "; \
        PrintArray<>(collector.GetNumberOfBins(), collector.GetStarts()); \
        cout << "Counters       : "; \
        PrintArray<>(collector.GetNumberOfBins(), collector.GetCounters()); \
        cout << "Total count    : " << collector.GetTotalCount()      << endl; \
        cout << "Anomaly count  : " << collector.GetAnomalyCount()    << endl; \
        cout << "Anomaly min    : " << collector.GetAnomalyCountMin() << endl; \
        cout << "Anomaly max    : " << collector.GetAnomalyCountMax() << endl; \
        cout << endl; \
    }}

// Simple run: add 100 values to collector within specified range with some anomalies (< 5%).
#define RUN(collector, range_min, range_max) \
    int delta = int((float(range_max) - range_min)/10); \
    if (!delta) delta = 2; \
    ADD_DATA(collector, 100, 95, range_min-delta, range_max+delta, range_min, range_max); \
    PRINT_STATS(collector)


// Simple custom scale function.
// For starting values it is the same as eLinear with step = 2, but greatly increase bin sizes later.
// We use template here, but if you know TScale type, you can use regular declaration.
//
template <typename TScale>
TScale fCustomFunc(TScale step) 
{ 
    if (step <= 3) {
        return 2 * step; 
    }
    return 5 * step; 
};


void CDataHistogramDemoApp::TestHistogram(void)
{
    std::random_device seed;
    std::default_random_engine rnd(seed());

    // 1 scale collectors
    {{
        // Simple integer scale for int values and unsigned int counters
        {{
            typedef CHistogram<int, int, unsigned int> H;
            {{
                cout << "1 scale - Linear scale from 0 to 10" << endl;
                H::Scale s(0, 10, 1, H::Scale::eLinear);
                H::Collector c(s);
                RUN(c, 0, 10);
            }}
            {{
                cout << "1 scale - Monotonic common logarithmic scale from 0 to 10000" << endl;
                H::Scale s(0, 10000, 1, H::Scale::eLog10, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 10000);
            }}
            {{
                cout << "1 scale - Symmetrical common logarithmic scale from 0 to 10000" << endl;
                H::Scale s(0, 10000, 1, H::Scale::eLog10, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 10000);
            }}
            {{
                cout << "1 scale - Custon function monotonic scale from 0 to 100" << endl;
                H::Scale s(0, 100, 1, fCustomFunc, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
            {{
                cout << "1 scale - Custon function symmetrical scale from 0 to 100" << endl;
                H::Scale s(0, 100, 1, fCustomFunc, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
        }}

        // Simple float scales for int values
        {{
            typedef CHistogram<int, double, size_t> H;
            {{
                cout << "1 scale - Linear scale from 0 to 10" << endl;
                H::Scale s(0, 10, 1, H::Scale::eLinear);
                H::Collector c(s);
                RUN(c, 0, 10);
            }}
            {{
                cout << "1 scale - Monotonic common logarithmic scale from 0 to 10000" << endl;
                H::Scale s(0, 10000, 1, H::Scale::eLog10, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 10000);
            }}
            {{
                cout << "1 scale - Symmetrical common logarithmic scale from 0 to 10000" << endl;
                H::Scale s(0, 10000, 1, H::Scale::eLog10, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 10000);
            }}
            {{
                cout << "1 scale - Custon function monotonic scale from 0 to 100" << endl;
                H::Scale s(0, 100, 1, fCustomFunc, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
            {{
                cout << "1 scale - Custon function symmetrical scale from 0 to 100" << endl;
                H::Scale s(0, 100, 1, fCustomFunc, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
        }}

        // Simple float scales for float values
        {{
            typedef CHistogram<double, double, size_t> H;
            {{
                cout << "1 scale - Linear scale from 0.1 to 10.9, step 0.5" << endl;
                H::Scale s(0.1, 10.9, 0.5, H::Scale::eLinear);
                H::Collector c(s);
                RUN(c, 0, 10);
            }}
            {{
                cout << "1 scale - Monotonic natural logarithmic scale from 0 to 1000, step 1.1" << endl;
                H::Scale s(0, 1000, 1.1, H::Scale::eLog, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 1000);
            }}
            {{
                cout << "1 scale - Symmetrical natural logarithmic scale from 0 to 1000, step 0.5" << endl;
                H::Scale s(0, 1000, 0.5, H::Scale::eLog, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 1000);
            }}
            {{
                cout << "1 scale - Custon function monotonic scale from 1.1 to 99.9, step 2.2" << endl;
                H::Scale s(1.1, 99.9, 2.2, fCustomFunc, H::Scale::eMonotonic);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
            {{
                cout << "1 scale - Custon function symmetrical scale from 1.1 to 99.9, step 1" << endl;
                H::Scale s(1.1, 99.9, 1, fCustomFunc, H::Scale::eSymmetrical);
                H::Collector c(s);
                RUN(c, 0, 100);
            }}
        }}
    }}
    
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
            RUN(c, 0, 100);
        }}
        {{
            // Reverse to above. Most relevant data at the end of the scales.
            cout << "2 scales - Linear scale from 90 to 100, and log from 90 to 0, step 0.5 for both" << endl;
            H::Collector c(
                H::MinorScale(0, 0.5, H::Scale::eLog),
                H::MajorScale(90, 100, 0.5, H::Scale::eLinear)
            );
            RUN(c, 0, 100);
        }}
        {{
            cout << "2 scales - Linear scale from 0 to 10, and log to 100, step 0.5 for both" << endl;
            H::Collector c(
                H::MajorScale(0, 10, { 0.5, 0 }, H::Scale::eLinear),  // step starts from 0.5
                H::MinorScale(100, { 0.5, 1 }, H::Scale::eLog2)       // step starts from 1
            );
            RUN(c, 0, 100);
        }}
        {{
            cout << "2 scales - Custon function monotonic scale from 1000 to 1010, and log from 100 to 0" << endl;
            H::Collector c(
                H::MinorScale(0, 1, H::Scale::eLog),
                H::MajorScale(1000, 1010, 1, fCustomFunc)
            );
            RUN(c, 0, 100);
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
            RUN(c, -1000, 1000);
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
            RUN(c, 0, 100000);
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
            RUN(c, 0, 100000);
        }}
    }}

    // Custom datat types
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
}


int main(int argc, char** argv)
{
    return CDataHistogramDemoApp().AppMain(argc, argv);
}
