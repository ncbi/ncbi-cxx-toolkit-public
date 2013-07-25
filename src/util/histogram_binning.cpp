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
 * Authors:
 *      Michael Kornbluh, NCBI
 *
 * File Description:
 *      Given a set of data points, automatically put them into bins
 *      for histogram display.  If requested, it can even figure out
 *      a good number of bins to have.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiexpt.hpp>

#include <algorithm>

#include <util/histogram_binning.hpp>
#include <util/bitset/bmutil.h>

#include <limits>

BEGIN_NCBI_SCOPE

namespace {

    // avoid putting this template in the header file

    template<typename TValue>
    class CReverseSort {
    public:
        bool operator()(
            const TValue & lhs,
            const TValue & rhs) 
        {
            // reversed
            return rhs < lhs;
        }
    };
}

CHistogramBinning::SBin::SBin(
    TValue first_number_arg,
    TValue last_number_arg,
    Uint8 total_appearances_arg )
    : first_number(first_number_arg),
    last_number(last_number_arg),
    total_appearances(total_appearances_arg )
{
}

CHistogramBinning::TListOfBins *
CHistogramBinning::CalcHistogram(EHistAlgo eHistAlgo) const
{
    switch(eHistAlgo) {
    case eHistAlgo_IdentifyClusters:
        return x_IdentifyClusters();
    case eHistAlgo_TryForSameNumDataInEachBin:
        return x_TryForEvenBins();
    default:
        NCBI_USER_THROW_FMT("Unknown eHistAlgo: " << 
            static_cast<int>(eHistAlgo) );
    }
}

CHistogramBinning::TListOfBins *
CHistogramBinning::x_IdentifyClusters(void) const
{
    AutoPtr<TListOfBins> pAnswer( new TListOfBins );
    Uint8 num_bins = 0;
    if( eInitStatus_AllAlgoWorkDone == 
        x_InitializeHistogramAlgo(*pAnswer, num_bins) ) 
    {
        return pAnswer.release();
    }

    typedef size_t SIndexOfBin;

    typedef pair<TValue, SIndexOfBin> TDiffAndBinPair;
    typedef vector<TDiffAndBinPair> TVecOfDiffAndBinPair;
    TVecOfDiffAndBinPair vecOfDiffAndBinPair;
    vecOfDiffAndBinPair.reserve(pAnswer->size());

    // Maps the difference from one bin to the next to the
    // bins at the start of each gap between bins.
    // (This is explained better in a comment farther below)

    ITERATE_0_IDX(ii, pAnswer->size() - 1) {
        const SBin & this_bin = (*pAnswer)[ii];
        const SBin & next_bin = (*pAnswer)[ii+1];

        const TValue difference = (
            next_bin.first_number - this_bin.last_number );
        vecOfDiffAndBinPair.push_back(
            TVecOfDiffAndBinPair::value_type(
                difference, ii ) );
    }

    sort( vecOfDiffAndBinPair.begin(),
        vecOfDiffAndBinPair.end(),
        CReverseSort<TVecOfDiffAndBinPair::value_type>() );

    // example contents that mapDifferenceToPrevBins might have now:
    //
    // Let's say the histogram data has the following values:
    // (disregard how many times each value appears)
    // 7 8 12 14 15
    // In that case, mapDifferenceToPrevBins would look like this;
    // 1 -> [ptr to 7, ptr to 14]
    // 2 -> [ptr to 12]
    // 4 -> [ptr to 8]
    // (Notice that nothing points to 15 because it's the last one)
    
    // Use a greedy algorithm to merge the closest bins until
    // the number of bins equals the number requested by the user

    // we start from the *end* because the biggest gaps are where
    // the divisions between ranges are.
    vector<SIndexOfBin> vecNodesThatEndRanges;
    ITERATE(TVecOfDiffAndBinPair, map_iter, 
        vecOfDiffAndBinPair) 
    {
        if( vecNodesThatEndRanges.size() >= (num_bins - 1) ) {
            break;
        }
        vecNodesThatEndRanges.push_back( map_iter->second );
    }

    // sort vecNodesThatEndRanges by their number
    // (highest first)
    sort( vecNodesThatEndRanges.begin(),
        vecNodesThatEndRanges.end() );

    AutoPtr<TListOfBins> pNewAnswer( new TListOfBins );

    SIndexOfBin bin_in_range_idx = 0;
    ITERATE(vector<SIndexOfBin>, range_near_end_it, 
        vecNodesThatEndRanges) 
    {
        Uint8 total_appearances_in_range = 0;
        const Uint8 lowest_number_in_range = 
            (*pAnswer)[bin_in_range_idx].first_number;
        for( ; bin_in_range_idx != *range_near_end_it ; 
            ++bin_in_range_idx )
        {
            const SBin & bin = (*pAnswer)[bin_in_range_idx];
            total_appearances_in_range += bin.total_appearances;
        }
        // bin_in_range_iter currently holds the last one in the range
        total_appearances_in_range += (*pAnswer)[bin_in_range_idx].total_appearances;
        const Uint8 highest_number_in_range = (*pAnswer)[bin_in_range_idx].last_number;
        pNewAnswer->push_back( SBin(lowest_number_in_range, 
            highest_number_in_range, 
            total_appearances_in_range ) );
        ++bin_in_range_idx;
    }

    // do the last range
    Uint8 total_appearances_in_range = 0;
    const Uint8 lowest_number_in_range = 
            (*pAnswer)[bin_in_range_idx].first_number;
    for( ; bin_in_range_idx != pAnswer->size(); ++bin_in_range_idx) {
        total_appearances_in_range += (*pAnswer)[bin_in_range_idx].total_appearances;
    }

    const Uint8 highest_number_in_range = pAnswer->back().last_number;
    pNewAnswer->push_back( SBin(lowest_number_in_range, 
        highest_number_in_range, 
        total_appearances_in_range ) );

    return pNewAnswer.release();
}

CHistogramBinning::TListOfBins *
CHistogramBinning::x_TryForEvenBins(void) const
{
    AutoPtr<TListOfBins> pAnswer( new TListOfBins );
    Uint8 num_bins = 0;
    if( eInitStatus_AllAlgoWorkDone == 
        x_InitializeHistogramAlgo(*pAnswer, num_bins) ) 
    {
        return pAnswer.release();
    }

    // a relatively simple algorithm: fill up a bin until it's
    // over (total_data_points / num_bins) and then start filling
    // the next one.

    // calc total number of data points
    Uint8 total_num_data_points = 0;
    ITERATE( TListOfBins, bin_iter, *pAnswer ) {
        total_num_data_points += bin_iter->total_appearances;
    }

    AutoPtr<TListOfBins> pNewAnswer( new TListOfBins );
    Uint8 total_data_points_remaining = total_num_data_points;
    ITERATE(TListOfBins, this_bin_iter, *pAnswer) {
        const SBin & current_input_bin = *this_bin_iter;

        const Uint8 num_bins_remaining = (num_bins - pNewAnswer->size() );

        // goal number for this bin
        const Uint8 bin_size_goal = 
            ( num_bins_remaining > 0 ?
            ( total_data_points_remaining / num_bins_remaining ) :
        numeric_limits<Uint8>::max()
            );

        if( pNewAnswer->empty() ) {
            pNewAnswer->push_back(current_input_bin);
        } else if( pNewAnswer->back().total_appearances >= bin_size_goal )
        {
            pNewAnswer->push_back(current_input_bin);
        } else {
            SBin & current_answer_bin = pNewAnswer->back();
            current_answer_bin.last_number = current_input_bin.last_number;
            current_answer_bin.total_appearances += current_input_bin.total_appearances;
        }
        total_data_points_remaining -= current_input_bin.total_appearances;
    }

    _ASSERT(total_data_points_remaining == 0);

    return pNewAnswer.release();
}

CHistogramBinning::EInitStatus 
CHistogramBinning::x_InitializeHistogramAlgo(
    TListOfBins & out_listOfBins,
    Uint8 & out_num_bins) const
{
    _ASSERT( out_listOfBins.empty() );

    if( m_mapValueToTotalAppearances.empty() ) {
        return eInitStatus_AllAlgoWorkDone;
    }

    // calculate total number of data points, including dups
    Uint8 total_appearances_of_all = 0;
    ITERATE(TMapValueToTotalAppearances, value_iter,
        m_mapValueToTotalAppearances) 
    {
        total_appearances_of_all += value_iter->second;
    }

    // use number of bins user provided, or auto-calculate if not
    out_num_bins = m_iNumBins;
    if( out_num_bins <= 0 ) {
        out_num_bins = 1 + bm::ilog2(total_appearances_of_all);
    }

    // start with each bin holding just one value
    ITERATE(TMapValueToTotalAppearances, value_iter,
        m_mapValueToTotalAppearances) 
    {
        const TValue value = value_iter->first;
        const Uint8 total_appearances = value_iter->second;
        out_listOfBins.push_back( SBin(value, value, total_appearances) );
    }

    // if there are not more data points than bins, just give what we have
    if( out_num_bins >= out_listOfBins.size() ) {
        return eInitStatus_AllAlgoWorkDone;
    }

    return eInitStatus_KeepGoing;
}

END_NCBI_SCOPE

