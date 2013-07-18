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

#include <util/histogram_binning.hpp>
#include <util/bitset/bmutil.h>

BEGIN_NCBI_SCOPE

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

    // Maps the difference from one bin to the next to the
    // bins at the start of each gap between bins.
    // (This is explained better in a comment farther below)
    typedef multimap<TValue, TListOfBins::iterator> TMapDifferenceToPrevBins;
    TMapDifferenceToPrevBins mapDifferenceToPrevBins;

    TListOfBins::iterator bin_iter = pAnswer->begin();
    for( ; bin_iter != pAnswer->end(); ++bin_iter ) {
        TListOfBins::iterator next_bin_iter = bin_iter;
        ++next_bin_iter;

        if( next_bin_iter == pAnswer->end() ) {
            break;
        }

        const TValue difference = (
            next_bin_iter->first_number - bin_iter->last_number );
        mapDifferenceToPrevBins.insert(
            TMapDifferenceToPrevBins::value_type(
                difference, bin_iter) );
    }

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

    // since calculating the length of a list could be expensive, 
    // we calculate how many merges we need ahead of time
    Uint8 num_merges_needed = ( pAnswer->size() - num_bins );
    
    // merge greedily
    TMapDifferenceToPrevBins::const_iterator diff_to_prev_bin_iter =
            mapDifferenceToPrevBins.begin();
    for( ; num_merges_needed-- > 0; ++diff_to_prev_bin_iter ) {
        TListOfBins::iterator bin_iter = diff_to_prev_bin_iter->second;
        TListOfBins::iterator next_bin_iter = bin_iter;
        ++next_bin_iter;

        // merge bin_iter and next_bin_iter
        next_bin_iter->first_number = bin_iter->first_number;
        next_bin_iter->total_appearances += bin_iter->total_appearances;
        pAnswer->erase(bin_iter);
    }

    return pAnswer.release();;
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

    // goal number for each bin
    const Uint8 bin_size_goal = ( total_num_data_points / num_bins );

    TListOfBins::iterator this_bin_iter = pAnswer->begin();
    ERASE_ITERATE( TListOfBins, this_bin_iter, *pAnswer  ) {
        SBin & this_bin = *this_bin_iter;

        // get next_bin, if possible
        TListOfBins::iterator next_bin_iter = this_bin_iter;
        ++next_bin_iter;
        if( next_bin_iter == pAnswer->end() ) {
            break;
        }
        SBin & next_bin = *next_bin_iter;

        // maybe merge this bin into the next one?
        if( this_bin.total_appearances < bin_size_goal ) {
            next_bin.first_number = this_bin.last_number;
            next_bin.total_appearances += this_bin.total_appearances;
            pAnswer->erase(this_bin_iter);
        }
    }

    return pAnswer.release();
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

