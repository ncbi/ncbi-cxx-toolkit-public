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

/// Given a set of integer data, this will bin the data
/// for use in histograms.
/// Usage is generally as follows: AddNumber is called
/// repeatedly until all data points are loaded, then
/// CalcHistogram is called to get the resulting histogram.
class NCBI_XUTIL_EXPORT CHistogramBinning {
public:

    /// The numeric type this bins.  It is big so that it can cover
    /// whatever type of integer the caller wants to bin.
    typedef Int8 TValue;

    /// Constructor.
    ///
    /// @param num_bins
    ///   0 means to automatically pick a reasonable number of bins.
    ///   Note that num_bins is a suggestion, and the answer
    ///   you get could havea a higher or lower number of bins
    ///   depending on misc factors.
    CHistogramBinning(Uint8 num_bins = 0) : m_iNumBins(num_bins) { }

    /// This should not normally be needed, since number of bins
    /// is usually picked in the constructor.  Callers might
    /// use it if they want to get histograms with differing 
    /// number of bins for the same data, for example.
    ///
    /// @param num_bins
    ///   This sets the target number of bins for the next histogram
    ///   calculated.
    ///   As in the constructor, 0 means to auto-pick number of bins.
    void SetNumBins(Uint8 num_bins) {
        m_iNumBins = num_bins;
    }

    /// Give this histogram another number to bin.  This is called
    /// repeatedly, and then the caller will probably want to
    /// call CalcHistogram to get the answer.
    /// It is perfectly fine to call this function multiple times
    /// with the same the_number; the total appearances will be summed
    /// up over all the calls.
    ///
    /// @param the_number
    ///   This is the value of the data point.  For example, if this
    ///   is a histogram of various persons' ages then the_number
    ///   would be 42 if the next data point is a 42-year-old person.
    /// @param num_appearances
    ///   This is how many times the data point appears.  For example,
    ///   setting this to "3" is the same as calling the function
    ///   3 times with a num_appearances value of 1.
    void AddNumber(TValue the_number, Uint8 num_appearances = 1) {
        m_mapValueToTotalAppearances[the_number] += num_appearances;
    }

    /// This clears all data given to the object.  It behaves pretty
    /// much like destroying the object and then recreating it anew.
    void clear(void) {
        m_mapValueToTotalAppearances.clear();
    }

    /// Holds the information about a bin.
    struct SBin {
        SBin(
            TValue first_number_arg,
            TValue last_number_arg,
            Uint8 total_appearances_arg );

        /// The start range of the bin (inclusive)
        TValue first_number; 
        /// The end range of the bin (inclusive)
        TValue last_number;
        /// The total number of data points in this bin
        /// for all values from first_number to last_number
        Uint8 total_appearances;
    };
    /// A histogram is given as a vector of bins.
    typedef vector<SBin> TListOfBins;

    /// Pick which binning algorithm to use when generating
    /// the histogram.
    enum EHistAlgo {
        /// This algorithm tries to make each bin represent
        /// values that are clustered together.
        /// Run-time O(n lg n), where n is the number
        /// of possible values, NOT the number of data points.
        eHistAlgo_IdentifyClusters,
        /// This algorithm tries to make each bin roughly even in size,
        /// except the last bin which may be much smaller.
        /// Run-time O(n lg n), where n is the number
        /// of possible values, NOT the number of data points.
        eHistAlgo_TryForSameNumDataInEachBin,
        // Maybe future algo: eHistAlgo_SameRangeInEachBin

        /// The default algorithm
        eHistAlgo_Default = eHistAlgo_IdentifyClusters
    };

    /// Call this after data is loaded via AddNumber, etc. and it
    /// will give you a list of SBin's, each of which identifies
    /// a bin of the resulting histogram.
    ///
    /// NOTE: Caller must deallocate the returned histogram object!
    ///
    /// @param eHistAlgo
    ///   This lets the caller pick which histogram binning algorithm
    ///   to use.  The default should suffice for most situations.
    /// @return
    ///   This returns a newly-allocated list of the bins in the resulting
    ///   histogram.  The caller is expected to free this.
    TListOfBins* CalcHistogram(
        EHistAlgo eHistAlgo = eHistAlgo_Default) const;

private:
    /// The number of bins to aim for the next time CalcHistogram
    /// is called.  It is 0 to automatically pick the number of bins.
    Uint8 m_iNumBins;

    /// forbid copying 
    CHistogramBinning(const CHistogramBinning&);
    /// forbid assignment
    CHistogramBinning & operator =(const CHistogramBinning &);

    /// Maps each value (e.g. age of person if you are histogramming
    /// ages) to the number of times that value appears.
    typedef map<TValue, Uint8> TMapValueToTotalAppearances;
    /// Maps a value to the number of times it appears, for the data
    /// given so far.
    TMapValueToTotalAppearances m_mapValueToTotalAppearances;

    /// Implementation of eHistAlgo_IdentifyClusters
    /// NOTE: Caller must deallocate the returned object!
    TListOfBins * x_IdentifyClusters(void) const;
    /// Implementation of eHistAlgo_TryForSameNumDataInEachBin
    /// NOTE: Caller must deallocate the returned object!
    TListOfBins * x_TryForEvenBins(void) const;

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

    /// This holds the shared logic used by the various histogram
    /// algorithms.
    /// 
    /// @param out_listOfBins
    ///   This will be filled in with a dummy histogram consisting
    ///   of one bin for each data value.
    /// @param out_num_bins
    ///   This will be filled in by the target number of bins.
    ///   This is usually m_iNumBins, but if m_iNumBins is zero
    ///   it will be the automatically picked number of bins.
    /// @return
    ///   The return value indicates how initialization went.  In particular,
    ///   there are cases where the work is already done and out_listOfBins
    ///   can be returned as-is to the user.
    EInitStatus x_InitializeHistogramAlgo(
        TListOfBins & out_listOfBins,
        Uint8 & out_num_bins) const;
};

END_NCBI_SCOPE

#endif /* UTIL__HISTOGRAM_BINNING__HPP */

