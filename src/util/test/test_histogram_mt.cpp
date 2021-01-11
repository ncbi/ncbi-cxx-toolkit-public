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
*   Test CHistogram class in MT environment.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/data_histogram.hpp>

// must be last
#include <common/test_assert.h>

#if defined(NCBI_THREADS)
#   include <thread>
#endif


USING_NCBI_SCOPE;


class CTest : public CNcbiApplication
{
public:
    int Run(void);
public:
    void ThreadHandler(int idx);
private:
    typedef Uint8 TCounter;
    typedef Uint8 TScale;
    typedef vector<TCounter>  TCounters;
    typedef vector<TCounter>  TStarts;
    typedef CHistogram<Uint8> THistogram;
    THistogram m_H;
};


const int kMax     = 10000;
const int kBins    = 10;
const int kThreads = 20;
const int kIters   = 100000;

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


int CTest::Run(void)
{
 #if defined(NCBI_THREADS)

    // Create some template histogram
    THistogram template_histogram(0, kMax, kBins, THistogram::eLinear);

    // Make it MT safe
    template_histogram.EnableMT();

    // Clone to main histogram, it will be accessible from threads.
    // It clones MT status as well.
    m_H = template_histogram.Clone();

    list<std::thread> threads;

    // Run adding threads
    for (int i = 0; i < kThreads; i++) {
        threads.push_back( std::thread(&CTest::ThreadHandler, std::ref(*this), i) );
    }
    // Wait all threads to finish
    for (auto& t : threads) {
        t.join();
    }
  
    PRINT_STATS(m_H);
/*
    Range          : [0:10000]
    Number of bins : 10
    Starts         : 0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000
    Counters       : 19810, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20020
    Total count    : 199830
    Anomaly (lower): 0
    Anomaly (upper): 1800170
    Sum            : 100018000000
*/
    // Compare results

    TCounters counters;
    m_H.GetBinCounters(counters);
    assert(counters.size() == kBins);
    TStarts starts;
    m_H.GetBinStarts(starts);
    assert(starts.size() == kBins);

    assert( m_H.GetCount()             == 199830 );
    assert( m_H.GetLowerAnomalyCount() == 0 );
    assert( m_H.GetUpperAnomalyCount() == 1800170 );
    assert( m_H.GetSum()               == 100018000000 );

    TCounters res_starts   { 0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000 };
    TStarts   res_counters { 19810, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20020 };

    assert( starts == res_starts );
    assert( counters == res_counters );

#endif
    return 0;
}


void CTest::ThreadHandler(int idx)
{
    // Add some values
    for (TCounter i = 0; i < kIters; i++) {

        m_H.Add(i + idx);

        // Emulate some other activity in between
        if (i % 10 == 0) {

            // Getting counters
            TCounters counters;
            m_H.GetBinCounters(counters);

            // Steal counters from the histogram.
            //
            // We can clone the histogram and reset it later, but this is not
            // an atomic operation and require additional mutex. 
            // So using StealCountersFrom(), that is atomic and can do both 
            // copy/reset in MT safe manner.
            //
            THistogram clone(m_H.Clone(THistogram::eCloneStructureOnly));
            clone.StealCountersFrom(m_H); 

            // Add values back to original histogram from the clone.
            m_H.AddCountersFrom(clone);
        }
    }

}


int main(int argc, const char* argv[]) 
{
#if defined(NCBI_THREADS)
    return CTest().AppMain(argc, argv);
#else
    // Do nothing for a single thread applications
    return 0;
#endif
}
