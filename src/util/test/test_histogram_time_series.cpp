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
 * Author:  Sergey Satskiy
 *
 * File Description:
 *   Test of CHistogramTimeSeries
 *
 */

#include <ncbi_pch.hpp>
#include <ncbiconf.h>
#include <corelib/test_boost.hpp>
#include <util/data_histogram.hpp>


USING_NCBI_SCOPE;

typedef CHistogram<int64_t, int64_t, int64_t>            TTestHistogram;
typedef CHistogramTimeSeries<int64_t, int64_t, int64_t>  TTestSeries;


BOOST_AUTO_TEST_CASE(Workflow)
{
    // Create Time series
    TTestHistogram      model_histogram(0, 9, 2, TTestHistogram::eLinear);
    TTestSeries         test_series(model_histogram);
    {
        auto    bins = test_series.GetHistograms();
        BOOST_CHECK(1 == bins.size());
        BOOST_CHECK(1 == bins.front().n_ticks);

        auto    counters = bins.front().histogram.GetBinCountersPtr();
        BOOST_CHECK(0 == counters[0]);
        BOOST_CHECK(0 == counters[1]);
        BOOST_CHECK(0 == bins.front().histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bins.front().histogram.GetUpperAnomalyCount());
    }

    // Do a few adds
    test_series.Add(1);
    test_series.Add(8);
    test_series.Add(720);
    {
        auto    bins = test_series.GetHistograms();
        BOOST_CHECK(1 == bins.size());
        BOOST_CHECK(1 == bins.front().n_ticks);

        auto    counters = bins.front().histogram.GetBinCountersPtr();
        BOOST_CHECK(1 == counters[0]);
        BOOST_CHECK(1 == counters[1]);
        BOOST_CHECK(0 == bins.front().histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bins.front().histogram.GetUpperAnomalyCount());
    }

    // Tick
    test_series.Rotate();
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        BOOST_CHECK(2 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(1 == bin1.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        BOOST_CHECK(0 == counters0[0]);
        BOOST_CHECK(0 == counters0[1]);
        BOOST_CHECK(0 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bin0.histogram.GetUpperAnomalyCount());
        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(1 == counters1[1]);
        BOOST_CHECK(0 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin1.histogram.GetUpperAnomalyCount());
    }

    // add
    test_series.Add(8);
    test_series.Add(8);
    test_series.Add(-1);
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        BOOST_CHECK(2 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(1 == bin1.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        BOOST_CHECK(0 == counters0[0]);
        BOOST_CHECK(2 == counters0[1]);
        BOOST_CHECK(1 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bin0.histogram.GetUpperAnomalyCount());
        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(1 == counters1[1]);
        BOOST_CHECK(0 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin1.histogram.GetUpperAnomalyCount());
    }

    // tick and add
    test_series.Rotate();
    test_series.Add(1);
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        BOOST_CHECK(2 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(2 == bin1.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        BOOST_CHECK(1 == counters0[0]);
        BOOST_CHECK(0 == counters0[1]);
        BOOST_CHECK(0 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bin0.histogram.GetUpperAnomalyCount());
        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(3 == counters1[1]);
        BOOST_CHECK(1 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin1.histogram.GetUpperAnomalyCount());
    }

    // tick and add
    test_series.Rotate();
    test_series.Add(8);
    test_series.Add(8);
    test_series.Add(-2);
    test_series.Add(500);
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        BOOST_CHECK(3 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(1 == bin1.n_ticks);
        BOOST_CHECK(2 == bin2.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        auto    counters2 = bin2.histogram.GetBinCountersPtr();

        BOOST_CHECK(0 == counters0[0]);
        BOOST_CHECK(2 == counters0[1]);
        BOOST_CHECK(1 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin0.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(0 == counters1[1]);
        BOOST_CHECK(0 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bin1.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(1 == counters2[0]);
        BOOST_CHECK(3 == counters2[1]);
        BOOST_CHECK(1 == bin2.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin2.histogram.GetUpperAnomalyCount());
    }

    // tick
    test_series.Rotate();
    test_series.Add(1);
    test_series.Add(500);
    test_series.Add(500);
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        BOOST_CHECK(3 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(2 == bin1.n_ticks);
        BOOST_CHECK(2 == bin2.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        auto    counters2 = bin2.histogram.GetBinCountersPtr();

        BOOST_CHECK(1 == counters0[0]);
        BOOST_CHECK(0 == counters0[1]);
        BOOST_CHECK(0 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(2 == bin0.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(2 == counters1[1]);
        BOOST_CHECK(1 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin1.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(1 == counters2[0]);
        BOOST_CHECK(3 == counters2[1]);
        BOOST_CHECK(1 == bin2.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin2.histogram.GetUpperAnomalyCount());
    }

    // tick
    test_series.Rotate();
    test_series.Add(1);
    test_series.Add(7);
    test_series.Add(500);
    test_series.Add(-2);
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        BOOST_CHECK(3 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(1 == bin1.n_ticks);
        BOOST_CHECK(4 == bin2.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        auto    counters2 = bin2.histogram.GetBinCountersPtr();

        BOOST_CHECK(1 == counters0[0]);
        BOOST_CHECK(1 == counters0[1]);
        BOOST_CHECK(1 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(1 == bin0.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(1 == counters1[0]);
        BOOST_CHECK(0 == counters1[1]);
        BOOST_CHECK(0 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(2 == bin1.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(2 == counters2[0]);
        BOOST_CHECK(5 == counters2[1]);
        BOOST_CHECK(2 == bin2.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(2 == bin2.histogram.GetUpperAnomalyCount());
    }

    // tick
    test_series.Rotate();
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        BOOST_CHECK(3 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(2 == bin1.n_ticks);
        BOOST_CHECK(4 == bin2.n_ticks);

        auto    counters0 = bin0.histogram.GetBinCountersPtr();
        auto    counters1 = bin1.histogram.GetBinCountersPtr();
        auto    counters2 = bin2.histogram.GetBinCountersPtr();

        BOOST_CHECK(0 == counters0[0]);
        BOOST_CHECK(0 == counters0[1]);
        BOOST_CHECK(0 == bin0.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(0 == bin0.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(2 == counters1[0]);
        BOOST_CHECK(1 == counters1[1]);
        BOOST_CHECK(1 == bin1.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(3 == bin1.histogram.GetUpperAnomalyCount());

        BOOST_CHECK(2 == counters2[0]);
        BOOST_CHECK(5 == counters2[1]);
        BOOST_CHECK(2 == bin2.histogram.GetLowerAnomalyCount());
        BOOST_CHECK(2 == bin2.histogram.GetUpperAnomalyCount());
    }

    test_series.Rotate();
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        ++it;
        auto    bin3 = *it;
        BOOST_CHECK(4 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(1 == bin1.n_ticks);
        BOOST_CHECK(2 == bin2.n_ticks);
        BOOST_CHECK(4 == bin3.n_ticks);
    }

    test_series.Rotate();
    {
        auto    bins = test_series.GetHistograms();
        auto    it = bins.begin();
        auto    bin0 = *it;
        ++it;
        auto    bin1 = *it;
        ++it;
        auto    bin2 = *it;
        ++it;
        auto    bin3 = *it;
        BOOST_CHECK(4 == bins.size());
        BOOST_CHECK(1 == bin0.n_ticks);
        BOOST_CHECK(2 == bin1.n_ticks);
        BOOST_CHECK(2 == bin2.n_ticks);
        BOOST_CHECK(4 == bin3.n_ticks);
    }
}

