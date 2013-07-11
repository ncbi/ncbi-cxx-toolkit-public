#ifndef UTIL__HISTOGRAM_BINNING__HPP
#define UTIL__HISTOGRAM_BINNING__HPP

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
* Authors:  Michael Kornbluh, NCBI
*
*/

/// @file histogram_binning.hpp
/// Given a set of data points, automatically put them into bins
/// for histogram display.  If requested, it can even figure out
/// a good number of bins to have.

#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XUTIL_EXPORT CHistogramBinning {
public:

    typedef Int8 TValue;

    /// @param num_bins
    ///   0 means to automatically pick a reasonable number of bins.
    ///   Note that num_bins is a suggestion, and the answer
    ///   you get could havea a higher or lower number of bins
    ///   depending on misc factors.
    CHistogramBinning(size_t num_bins = 0) : m_iNumBins(num_bins) { }

    void SetNumBins(size_t num_bins) {
        m_iNumBins = num_bins;
    }

    void AddNumber(TValue the_number, size_t num_appearances = 0) {
        m_mapValueToTotalAppearances[the_number] += num_appearances;
    }

    void clear(void) {
        m_mapValueToTotalAppearances.clear();
    }

    struct SBin {
        SBin(
            TValue first_number_arg,
            TValue last_number_arg,
            size_t total_appearances_arg );

        TValue first_number;
        TValue last_number;
        size_t total_appearances;
    };
    typedef list<SBin> TListOfBins;

    enum EHistAlgo {
        /// This algorithm tries to make each bin represent
        /// values that are clustered together.
        /// Run-time O(n lg n), where n is the number
        /// of possible values, NOT the number of data points.
        eHistAlgo_IdentifyClusters,
        /// This algorithm tries to make each bin even in size.
        /// Run-time O(n lg n), where n is the number
        /// of possible values, NOT the number of data points.
        eHistAlgo_TryForSameNumDataInEachBin,
        // Maybe future algo: eHistAlgo_SameRangeInEachBin
    };

    /// Run-time should be O(n * log n), where 
    AutoPtr<TListOfBins> CalcHistogram(
        EHistAlgo eHistAlgo = eHistAlgo_IdentifyClusters) const;

private:
    size_t m_iNumBins;

    /// forbid copying 
    CHistogramBinning(const CHistogramBinning&);
    /// forbid assignment
    CHistogramBinning & operator =(const CHistogramBinning &);

    typedef map<TValue, size_t> TMapValueToTotalAppearances;
    /// maps a value to the number of times it appears
    TMapValueToTotalAppearances m_mapValueToTotalAppearances;

    AutoPtr<TListOfBins> x_IdentifyClusters(void) const;
    AutoPtr<TListOfBins> x_TryForEvenBins(void) const;

    /// shared by the various histogram algos
    enum EInitStatus {
        /// This indicates that x_InitializeHistogramAlgo
        /// has done all the work required of it and 
        /// the caller can return the output value, which
        /// usually happens in trivial cases.
        eInitStatus_AllAlgoWorkDone,
        /// This means the initialization completed, but
        /// the caller will need to do more work to shrink the
        /// number of bins.
        eInitStatus_KeepGoing
    };
    EInitStatus x_InitializeHistogramAlgo(
        TListOfBins & out_listOfBins,
        size_t & out_num_bins) const;
};

END_NCBI_SCOPE

#endif /* UTIL__HISTOGRAM_BINNING__HPP */