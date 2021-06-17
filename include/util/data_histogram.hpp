#ifndef DATA_HISTOGRAM__HPP
#define DATA_HISTOGRAM__HPP

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
 * Author:  Vladimir Ivanov
 *
 */

/// @file data_histogram.hpp
/// Frequency histogram for data distribution of the numerical samples.

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#define _USE_MATH_DEFINES // to define math constants in math.h
#include <math.h>         // log/pow functions
#include <cmath>          // additional overloads for pow()
#include <memory>
#include <mutex>


/** @addtogroup Statistics
 *
 * @{
 */

BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// Helper types for CHistogram<>::GetSum() support
//

// if T doesn't have 'foo' method  (operator+= in our case) with the signature
// that allows to compile the bellow expression then instantiating this template
// is Substitution Failure (SF) which Is Not An Error (INAE) if this happens
// during overload resolution
template <typename T>
using T_HistogramValueTypeHavePlus = decltype((T&)(std::declval<T>().operator+=(std::declval<const T&>())));

// Checks that T is arithmetic type or have operator+= implemented
template <typename T>
constexpr bool g_HistogramValueTypeHavePlus()
{
    return cxx_is_supported<T_HistogramValueTypeHavePlus, T>::value  ||  std::is_arithmetic<T>::value;
}


/////////////////////////////////////////////////////////////////////////////
///
/// CHistogram -- collect the distribution of the numerical data samples.
///  
/// CHistogram <TValue, TScale, TCounter>
///
/// TValue -   type of values that distribution will be collected.
///            This can be any numerical or user defined type, if it allow
///            comparison and can be converted to scale type TScale, see note. 
///
/// TScale -   numerical type used to calculate bins starting positions and sizes.
///            For integer types some scales, logarithmic for examples, will lead
///            to truncating values and creating unequal bins, so TScale
///            should be float/double based.
///
/// TCounter - type to store counters. Usually this is a positive integer type:
///            int, unsigned int, long, size_t, Uint8 and etc.
/// 
/// @note
///   TValue and TScale types should be equal, or allow comparison and conversion
///   between them. Any types can be used as TValue if it have:
///   = default constructor: TValue()  -- for initialization;
///   - operator TScale() const        -- to convert to scale type TScale;
///   - bool operator >(const TValue&) -- for comparison.
///
/// @note
///   CHistogram is not MT safe by default, it is intended to be created 
///   and collect data withing the same thread. If you want to use the same 
///   CHistogram class object across multiple threads, you need to provide
///   some sort of MT protection to guard access to the histogram object, 
///   or enable internal MT protection, and call CHistogram::EnableMT()
///   immediately after creating a histogram object.


template <typename TValue = int, typename TScale = TValue, typename TCounter = Uint8>
class CHistogram
{
public:
    /// Scale type.
    /// Most common is a linear scale, where each bin have the same size.
    /// It is good if you have a limited range, but sometimes values are distributed
    /// very wide, and if you want to count all of them, but concentrate on a 
    /// some range with most significant values, logarithmic scales helps a lot.
    /// For logarithmic scales each next bin have a greater size.
    /// Bins size increasing depends on a logarithmic base (used scale type).
    ///
    enum EScaleType {
        eLinear = 1, ///< Arithmetic or linear scale
        eLog,        ///< Natural logarithmic scale with a base e ~ 2.72
        eLog2,       ///< Binary logarithmic scale with a base 2
        eLog10       ///< Common logarithmic scale with a base 10
    };

    /// Methods to build bins for a specified scale.
    /// Applies to the main scale, defined in constructor, only.
    enum EScaleView {
        /// Use specified scale method to calculate bins sizes from a minimum
        /// to a maximum value.
        eMonotonic,
        /// Determine a mean for a specified value range and calculates
        /// bins sizes using specified scale to both sides from it, symmetrically.
        eSymmetrical
    };


    /////////////////////////////////////////////////////////////////////////
    // Initialization

    /// Constructor.
    ///
    /// @param min_value
    ///   Minimum allowed value in range [min_value, max_value].
    /// @param max_value
    ///   Maximum allowed value in range [min_value, max_value].
    /// @param n_bins
    ///   Number of bins for the histogram (main scale).
    /// @param scale_type
    ///   Predefined scale type. Corresponding scale function will be used
    ///   to calculate scale's bin ranges.
    /// @param scale_view
    ///   Scale view (symmetrical or monotonic).
    /// @sa EScaleType, EScaleView, AddLeftScale, AddRightScale, Add
    ///
    CHistogram(TValue min_value, TValue max_value, unsigned n_bins,
               EScaleType scale_type = eLinear, 
               EScaleView scale_view = eMonotonic);

    /// Add auxiliary left/right scales.
    ///
    /// The CHistogram constructor defines a main scale for some data range.
    /// It can be very limited to get more precise results for the value distribution.
    /// You can get the number of hits whose values fall outside the range of the main
    /// scale using GetLowerAnomalyCount()/GetUpperAnomalyCount() methods, 
    /// but each returns a single number of hits outside of the range.
    /// The other method is to use auxiliary scale(s) to count the number of hits
    /// for the less significant range(s). You can add any number of scales from each side.
    /// So, you can use linear scale with much greater bin sizes, or logarithmic scales,
    /// that allow to cover a very wide range of data with a limited number of bins.
    /// See EScaleType for details.
    ///
    /// @param min_value
    ///   Minimum allowed value for the auxiliary scale. 
    ///   Its maximum value is the same as a minimum value for the main scale,
    ///   or previously added scale.
    /// @param n_bins
    ///   Number of bins for the auxiliary scale.
    /// @param scale_type
    ///   Predefined scale type. Corresponding scale function will be used
    ///   to calculate scale's bin ranges.
    /// @note
    ///   It is not allowed to add left/right auxiliary scales after starting counting hits.
    ///   Please use these methods before Add(), or after Reset().
    /// @sa CHistogram::CHistogram, AddRightScale, EScaleType, GetLowerAnomalyCount, GetUpperAnomalyCount
    ///
    void AddLeftScale (TValue min_value, unsigned n_bins, EScaleType scale_type);
    void AddRightScale(TValue max_value, unsigned n_bins, EScaleType scale_type);


    /////////////////////////////////////////////////////////////////////////
    // Populating

    /// Sum type: double for all floating points TValue types, int64_t/uint64_t for integral, and TValue otherwise.
    using TIntegral   = typename std::conditional<std::numeric_limits   <TValue>::is_signed, int64_t,     uint64_t >::type;
    using TArithmetic = typename std::conditional<std::is_floating_point<TValue>::value,     double,      TIntegral>::type;
    using TSum        = typename std::conditional<std::is_arithmetic    <TValue>::value,     TArithmetic, TValue   >::type;

    /// Add value to the data distribution.
    /// Try to find an appropriate bin for a specified value and increase its counter on 1.
    /// Also, calculates a sum of all added values if TValue type have addition support.
    /// @sa GetStarts, GetCounters, GetSum
    ///
    template <typename V, typename S = TSum,
              std::enable_if_t<g_HistogramValueTypeHavePlus<S>(), int> = 0>
    void Add(const V& v) {
        MT_Lock();
        x_Add(v);
        // Calculate sum, TSum have operator+=
        m_Sum += v;
        MT_Unlock();
    }
    template <typename V, typename S = TSum, 
             std::enable_if_t<!g_HistogramValueTypeHavePlus<S>(), int> = 0>
    void Add(const V& v) {
        MT_Lock();
        x_Add(v);
        MT_Unlock();
    }

    // Return value for a class member, MT safe
    #define RETURN_MT_SAFE(member) \
        if (m_IsMT) {    \
            MT_Lock();       \
            auto v = member; \
            MT_Unlock();     \
            return v;        \
        }                    \
        return member

    /// Return the sum of all added values.
    /// @return
    ///   Returned type depends on TValue type, and converts to:
    ///     - double   -- for all floating point types;
    ///     - int64_t  -- signed integral types;
    ///     - uint64_t -- unsigned integral types;
    ///     - TValue   -- for all other types.
    ///   The sum can be calculated for floating, integral types and all other
    ///   TValue types that have 'operator +=' defined.
    ///   If TValue type doesn't have such operator defined, empty value {} is returned.
    ///   Note, compiler can convert this empty {} value to TScale type, because every TValue
    ///   should have 'operator TScale() const' by design.
    /// @note 
    ///   This method doesn't check calculated sum on overflow.
    /// @sa Add, TSum
    TSum GetSum(void) const { RETURN_MT_SAFE(m_Sum); }

    /// Reset all data counters.
    void Reset();

    /// Add counters from 'other' histogram to this histogram, 'other' doesn't changes.
    /// @note Both histograms should have the same structure.
    /// @sa Clone, Reset, StealCountersFrom
    void AddCountersFrom(const CHistogram& other);

    /// Add counters from 'other' histogram to this histogram,
    /// then reset the counters of 'other' histogram.
    /// @note Both histograms should have the same structure.
    /// @sa Clone, Reset, AddCountersFrom
    void StealCountersFrom(CHistogram& other);


    /////////////////////////////////////////////////////////////////////////
    // Structure

    /// Get the lower bound of the combined scale.
    TValue GetMin() const { return m_Min; }

    /// Get the upper bound of the combined scale.
    TValue GetMax() const { return m_Max; }

    /// Return the number ot bins on the combined scale.
    /// @sa GetBinStarts, GetBinCounters, GetBinCountersPtr
    unsigned GetNumberOfBins() const { return m_NumBins; }

    /// Get starting positions for bins on the combined scale.
    ///
    /// Populate a vector with starting positions for bins on the combined scale.
    /// The size of the vector changes to be equal the number of bins.
    /// @sa GetNumberOfBins, GetBinCounters, GetBinStartsPtr
    void GetBinStarts(vector<TScale>& positions) {
        MT_Lock();
        positions.resize(m_NumBins);
        positions.assign(m_Starts.get(), m_Starts.get() + m_NumBins);
        MT_Unlock();
    }

    /// Get starting positions for bins on the combined scale (not MT safe).
    ///
    /// Returns a pointer to array. The number of bins can be obtained with GetNumberOfBins().
    /// and a number of a counters in each bin -- with GetBinCounters().
    /// @warning
    ///   Be aware that any change in the histogram's structure, adding additional scales,
    ///   invalidate all data and pointer itself. This method is not intended to use in MT
    ///   environment. Please use vector version GetBinStarts(vector<TScale>&).
    /// @sa GetNumberOfBins, GetBinCountersPtr, GetBinStarts, EnableMT
    const TScale* GetBinStartsPtr() const { return m_Starts.get(); }


    /////////////////////////////////////////////////////////////////////////
    // Results

    /// Get counters for the combined scale's bins.
    ///
    /// Populate a vector with counters for the combined scale's bins.
    /// The size of the vector changes to be equal to the number of bins.
    /// @sa GetNumberOfBins, GetBinStarts, GetBinCountersPtr, Add
    void GetBinCounters(vector<TCounter>& counters) {
        MT_Lock();
        counters.resize(m_NumBins);
        counters.assign(m_Counters.get(), m_Counters.get() + m_NumBins);
        MT_Unlock();
    }

    /// Get counters for the combined scale's bins (not MT safe).
    ///
    /// Returns a pointer to array. The number of bins can be obtained with GetNumberOfBins().
    /// @warning
    ///   Be aware that any change in the histogram's structure invalidate all data
    ///   and pointer itself. This method is not intended to use in MT environment.
    ///   Please use vector version GetBinCounters(vector<TCounter>&).
    /// @sa GetNumberOfBins, GetBinStartsPtr, GetBinCounters, Add, EnableMT
    const TCounter* GetBinCountersPtr() const { return m_Counters.get(); }

    /// Get total number of hits whose value fell between GetMin() and GetMax().
    /// The number of hits whose values fall outside that range can be obtained
    /// using GetLowerAnomalyCount() and GetUpperAnomalyCount() methods.
    /// @sa GetMin, GetMax, GetLowerAnomalyCount, GetUpperAnomalyCount, GetBinCounters
    TCounter GetCount() const { RETURN_MT_SAFE(m_Count); }

    /// Get number of hits whose values were less than GetMin().
    /// @sa GetUpperAnomalyCount, GetCount
    size_t GetLowerAnomalyCount() const {RETURN_MT_SAFE(m_LowerAnomalyCount); }

    /// Get number of hits whose values were greater than GetMax().
    /// @sa GetLowerAnomalyCount, GetCount
    size_t GetUpperAnomalyCount() const { RETURN_MT_SAFE(m_UpperAnomalyCount); }


    /////////////////////////////////////////////////////////////////////////
    // Helpers

    /// Rules to calculate an estimated numbers of bins on the base of the expected
    /// number of observations.
    /// Note, that there is no "best" number of bins, and different bin sizes can reveal
    /// different features of the data. All these methods generally make strong assumptions
    /// about the shape of the distribution. Depending on the actual data distribution
    /// and the goals of the analysis, different bin widths may be appropriate, 
    /// so experimentation is usually needed to determine an appropriate width.
    /// @note
    ///   All methods applies to the linear scale only, that have a fixed bin sizes.
    /// @sa
    ///   EstimateNumberOfBins
    ///
    enum EEstimateNumberOfBinsRule {
        /// Square root rule. Used by Excel histograms and many others. 
        /// Default value for EstimateNumberOfBins() as well.
        eSquareRoot = 0,
        /// Juran's "Quality Control Handbook" that provide guidelines
        /// to select the number of bins for histograms.
        eJuran,
        /// Herbert Sturge's rule. It works best for continuous data that is
        /// normally distributed and symmetrical. As long as your data is not skewed,
        /// using Sturge's rule should give you a nice-looking, easy to read
        /// histogram that represents the data well.
        eSturge,
        /// Rice's rule. Presented as a simple alternative to Sturge's rule.
        eRice
    };

    /// Estimate numbers of bins on the base of the expected number of observations 'n'.
    /// @note
    ///   Applies to the linear scale only.
    /// @sa
    ///   EEstimateNumberOfBinsRules
    static unsigned EstimateNumberOfBins(size_t n, EEstimateNumberOfBinsRule rule = 0);


    /////////////////////////////////////////////////////////////////////////
    // Move semantics

    /// Default constructor.
    /// 
    /// Creates empty object. The object itself is invalid without min/max values,
    /// and any scale. Should be used with conjunction of Clone() only.
    /// @sa Clone
    ///
    CHistogram(void);

    /// Move constructor.
    ///
    /// Move 'other' histogram data to the current object. 'other' became invalid.
    /// @example
    ///     CHistogram<> h1(min, max, n, type);
    ///     CHistogram<> h2(h1.Clone(how));
    /// @sa Clone
    ///
    CHistogram(CHistogram&& other);

    /// Move assignment operator.
    ///
    /// Move 'other' histogram data to the current object. 'other' became invalid.
    /// @example
    ///     CHistogram<> h1(min, max, n, type);
    ///     CHistogram<> h2;
    ///     h2 = h1.Clone(how);
    /// @sa Clone
    ///
    CHistogram& operator=(CHistogram&& other);

    enum EClone {
        eCloneAll = 0,       ///< Clone whole histogram, with scale and counters
        eCloneStructureOnly  ///< Clone structure only (the counters will be zeroed)
    };

    /// Clone histogram structure.
    /// 
    /// Creates a copy of the histogram structure depending on clone method.
    /// By default it is a whole copy, but you can clone histogram's structure only, without counters.
    ///
    CHistogram Clone(EClone how = eCloneAll) const;


    /////////////////////////////////////////////////////////////////////////
    // Optional MT protection

    /// Add MT protection to histogram.
    ///
    /// By default CHistogram is not MT protected and intended to be created 
    /// and used withing the same thread. Calling this method add MT protection
    /// on adding additional scales, adding and resetting values, cloning and
    /// moving histograms. Please call this method immediately after creating
    /// each histogram, before accessing it from any other thread, or deadlock can occurs.
    ///
    void EnableMT(void) {
        m_IsMT = true; 
    };
    /// MT locking
    void MT_Lock() const {
        if (m_IsMT) m_Mutex.lock();
    }
    // MT unlocking
    void MT_Unlock() const {
        if (m_IsMT) m_Mutex.unlock();
    }

protected:
    // Note, none of x_*() methods have MT locking, it should be done in public methods only.

    /// Calculate bins starting positions.
    /// Calculate positions for 'n' number of bins in [start,end] value range,
    /// starting from 'pos' bin index.
    /// If start_value > end_value, calculating going from the right to left.
    /// This is applicable for symmetrical scales, or multi-scales,
    /// with left scale added.
    void x_CalculateBins(TScale start_value, TScale end_value,
                         unsigned pos, unsigned n,
                         EScaleType scale_type, EScaleView scale_view);

    /// Calculate bins starting positions for a linear scale.
    /// Account for an integer TScale type and calculation truncation.
    void x_CalculateBinsLinear(
                         TScale start_value, TScale end_value, TScale step,
                         TScale* arr, unsigned pos, unsigned n, 
                         EScaleView scale_view);

    /// Calculate bins starting positions for a logarithmic scale.
    void x_CalculateBinsLog(
                         TScale start_value, TScale end_value,
                         TScale* arr, unsigned pos, unsigned n, 
                         EScaleType scale_type, EScaleView scale_view);

    /// Scale function.
    /// Calculates scale position on a base of data value.
    TScale x_Func(EScaleType scale_type, TScale value);

    /// Inverse scale function.
    /// Calculates a data value on a base of scale value/position.
    TScale x_FuncInverse(EScaleType scale_type, TScale scale_value);

    /// Add value to the data distribution (internal version without locking).
    void x_Add(TValue value);

    /// Add value to the data distribution using a linear search method.
    /// Usually faster than bisection method on a small number of bins,
    /// or if the values have a tendency to fall into the starting bins
    /// for a used scale.
    /// @sa Add, x_AddBisection
    void x_AddLinear(TValue value);

    /// Add value to the data distribution using a bisection search method.
    /// Usually faster on a long scales with a lot of bins.
    /// @sa Add, x_AddLinear
    void x_AddBisection(TValue value);

    /// Move data from 'other' histogram. 'other' became invalid.
    void x_MoveFrom(CHistogram& other);

    /// Add counters from 'other' histogram.
    void x_AddCountersFrom(CHistogram& other);

    /// Check that 'a' and 'b' scale values are equal (or almost equal for floating scales).
    bool x_IsEqual(TScale a, TScale b);

    /// Reset all data counters (internal version without locking).
    void x_Reset();

    /// Prevent copying.
    /// See move constructor and move assignment operator to use with move-semantics.
    CHistogram(const CHistogram&);
    CHistogram& operator=(const CHistogram&);

protected:
    TValue    m_Min;         ///< Minimum value (the lower bound of combined scale)
    TValue    m_Max;         ///< Maximum value (the upper bound of combined scale)
    unsigned  m_NumBins;     ///< Number of bins (m_Starts[]/m_Counts[] length)
    TSum      m_Sum = {};    ///< Sum of the all added values (if applicable for TValue)

    std::unique_ptr<TScale[]>   m_Starts;    ///< Combined scale: starting bins positions
    std::unique_ptr<TCounter[]> m_Counters;  ///< Combined scale: counters - the number of measurements for each bin

    TCounter  m_Count;              ///< Number of counted values (sum all m_Counters[])
    TCounter  m_LowerAnomalyCount;  ///< Number of anomaly values < m_Min
    TCounter  m_UpperAnomalyCount;  ///< Number of anomaly values > m_Max

    bool      m_IsMT;               ///< MT protection flag
    mutable std::mutex m_Mutex;     ///< MT protection mutex
};



/// A series of same-structured histograms covering logarithmically (base 2)
/// increasing time periods... roughly
///
template <typename TValue, typename TScale, typename TCounter>
class CHistogramTimeSeries
{
public:
    /// @param model_histogram
    ///   This histogram will be used as a template for all histogram objects
    ///   in this time series.
    using THistogram = CHistogram<TValue,TScale,TCounter>;
    CHistogramTimeSeries(THistogram& model_histogram);

    /// Add value to the data distribution.
    /// Try to find an appropriate bin for a specified value and increment its counter by 1.
    /// @sa CHistogram::Add()
    void Add(TValue value);

    /// Merge the most recent (now active) histogram data into the time series.
    /// This method is generally supposed to be called periodically, thus
    /// defining the unit of time (tick) as far as this API is concerned.
    void Rotate();

    /// Reset to the initial state
    void Reset();

    /// Type of the unit of time
    using TTicks = unsigned int;

    /// A histograms which covers a certain number of ticks
    struct STimeBin {
        // Copy constructor and assignment operator should be passed with 'const'
        // qualifier to operate on list<STimeBin>. But Clone() is not 'const'
        // due optional protection, so we use const_cast<> here. It is safe, 
        // because Clone() change internal mutex only.
        STimeBin(const STimeBin& other) :
            histogram(other.histogram.Clone(THistogram::eCloneAll)),
            n_ticks(other.n_ticks)
        {}
        STimeBin& operator=(const STimeBin& other)
        {
            histogram = other.histogram.Clone(THistogram::eCloneAll);
            n_ticks = other.n_ticks;
        }
        STimeBin(THistogram&& h, TTicks t) :
            histogram(std::move(h)), n_ticks(t)
        {}
        
        THistogram histogram;  ///< Histogram for the ticks
        TTicks     n_ticks;    ///< Number of ticks in this histogram
    };
    /// Type of the series of histograms
    using TTimeBins = list<STimeBin>;

    /// Histograms -- in the order from the most recent to the least recent
    TTimeBins GetHistograms() const;

    /// Number of ticks the histogram series has handled.
    /// Initially the number of ticks is zero.
    TTicks GetCurrentTick(void) const { return m_CurrentTick; }

private:
    void x_AppendBin(const THistogram& model_histogram, TTicks n_ticks);
    void x_Shift(size_t index, typename TTimeBins::iterator current_it);

private:
    TTimeBins           m_TimeBins;
    mutable std::mutex  m_Mutex;         // for Rotate() and GetHistograms()
    TTicks              m_CurrentTick;
};


/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::CHistogram
(
    TValue      min_value, 
    TValue      max_value, 
    unsigned    n_bins,
    EScaleType  scale_type, 
    EScaleView  scale_view
)
    : m_Min(min_value), m_Max(max_value), m_NumBins(n_bins), m_IsMT(false)
{
    if ( m_Min > m_Max ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Minimum value cannot exceed maximum value");
    }
    if ( !m_NumBins ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Number of bins cannot be zero");
    }
    // Allocate memory
    m_Starts.reset(new TScale[m_NumBins]);
    m_Counters.reset(new TCounter[m_NumBins]);

    x_CalculateBins(m_Min, m_Max, 0, m_NumBins, scale_type, scale_view);
    // Reset counters
    x_Reset();
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::AddLeftScale(
    TValue min_value, unsigned n_bins, EScaleType scale_type)
{
    MT_Lock();

    if ( min_value >= m_Min ) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "New minimum value cannot exceed minimum value for the histogram");
    }
    if ( !n_bins ) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "Number of bins cannot be zero");
    }
    if (m_Count + m_LowerAnomalyCount + m_UpperAnomalyCount) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "Please call AddLeftScale() before Add()");
    }
    unsigned n_prev = m_NumBins;
    m_NumBins += n_bins;

    // Reallocate memory for starting positions and counters

    std::unique_ptr<TScale[]> tmp_starts(new TScale[m_NumBins]);
    memcpy(tmp_starts.get() + n_bins, m_Starts.get(), sizeof(TScale) * n_prev);
    m_Starts.swap(tmp_starts);
    m_Counters.reset(new TCounter[m_NumBins]);
    memset(m_Counters.get(), 0, m_NumBins * sizeof(TCounter));

    // Calculate scale for newly added bins: from right to left
    x_CalculateBins(m_Min, min_value, n_bins-1, n_bins, scale_type, eMonotonic);
    m_Min = min_value;

    MT_Unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::AddRightScale(
    TValue max_value, unsigned n_bins, EScaleType scale_type)
{
    MT_Lock();

    if ( max_value <= m_Max ) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "New maximum value cannot be less than a maximum value for the histogram");
    }
    if ( !n_bins ) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "Number of bins cannot be zero");
    }
    if (m_Count + m_LowerAnomalyCount + m_UpperAnomalyCount) {
        MT_Unlock();
        NCBI_THROW(CCoreException, eInvalidArg, "Please call AddRightScale() before Add()");
    }
    unsigned n_prev = m_NumBins;
    m_NumBins += n_bins;

    // Reallocate memory for starting positions and counters

    std::unique_ptr<TScale[]> tmp_starts(new TScale[m_NumBins]);
    memcpy(tmp_starts.get(), m_Starts.get(), sizeof(TScale) * n_prev);
    m_Starts.swap(tmp_starts);
    m_Counters.reset(new TCounter[m_NumBins]);
    memset(m_Counters.get(), 0, m_NumBins * sizeof(TCounter));

    // Calculate scale for newly added bins: from left to right
    x_CalculateBins(m_Max, max_value, n_prev, n_bins, scale_type, eMonotonic);
    m_Max = max_value;

    MT_Unlock();
}


template <typename TValue, typename TScale, typename TCounter>
unsigned 
CHistogram<TValue, TScale, TCounter>::EstimateNumberOfBins(size_t n, EEstimateNumberOfBinsRule rule)
{
    switch (rule) {
    case eSquareRoot:
        return (unsigned) ceil(sqrt(n));
    case eJuran:
        // It have no info for n < 20, but 5 is a reasonable minimum
        if (n <    20) return  5;
        if (n <=   50) return  6;
        if (n <=  100) return  7;
        if (n <=  200) return  8;
        if (n <=  500) return  9;
        if (n <= 1000) return 10;
        return 11;  // handbook: 11-20  for 1000+ observations
    case eSturge:
        return 1 + (unsigned) ceil(log2(n));
    case eRice:
        return (unsigned) ceil(pow(n, double(1)/3));
    }
    _TROUBLE;
    // unreachable, just to avoid warning
    return 0;
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::Reset(void)
{
    MT_Lock();
    x_Reset();
    MT_Unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::x_Reset(void)
{
    // Reset counters
    m_Count = m_LowerAnomalyCount = m_UpperAnomalyCount = 0;
    // Reset bins
    memset(m_Counters.get(), 0, m_NumBins * sizeof(TCounter));
    // Reset sum (it can be any type, so use default constructor)
    m_Sum = {};
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::x_Add(TValue value)
{
    if (value < m_Min) {
        m_LowerAnomalyCount++;
        return;
    }
    if (value > m_Max) {
        m_UpperAnomalyCount++;
        return;
    }
    if (m_NumBins < 50) {
        x_AddLinear(value);
        return;
    }
    x_AddBisection(value);
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::x_AddLinear(TValue value)
{
    size_t i = 1;
    while (i < m_NumBins) {
        if (value < m_Starts[i]) {
            break;
        }
        i++;
    }
    // The current bin with index 'i' have a greater value, so put value into the previous bin
    m_Counters[i-1]++;
    m_Count++;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_AddBisection(TValue value)
{
    // Bisection search

    size_t left = 0, right = m_NumBins;
    size_t i = 0, d;

    while (d = (right - left), d > 1) {
        i = left + d / 2;
        if (value < m_Starts[i]) {
            right = i;
        } else {
            left = i;
        }
    }
    // Algorithm can finish on the current or next bin, depending on even/odd number of bins
    if (value < m_Starts[i]) {
        i--;
    }
    m_Counters[i]++;
    m_Count++;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_CalculateBins
(
    TScale      start_value,
    TScale      end_value,
    unsigned    pos, 
    unsigned    n,
    EScaleType  scale_type, 
    EScaleView  scale_view
)
{
    const char* errmsg_step = "Impossible to calculate scale step, please change TScale type, range, or number of bins";
    const char* errmsg_dup  = "Impossible to calculate scales bin starting position, please change TScale type, range, or number of bins";

    TScale* arr = m_Starts.get();

    if (scale_type == eLinear) {
        // Special processing for linear scales to account for
        // an integer TScale type and calculation truncation.
        TScale step = (max(start_value, end_value) - min(start_value, end_value)) / n;
        if ( step == 0 ) {
            NCBI_THROW(CCoreException, eCore, errmsg_step);
        }
        x_CalculateBinsLinear(start_value, end_value, step, arr, pos, n, scale_view);
        return;
    }

    // Log scales only
    _ASSERT( scale_type == eLog   ||
             scale_type == eLog2  ||
             scale_type == eLog10 );

    x_CalculateBinsLog(start_value, end_value, arr, pos, n, scale_type, scale_view);

    // Check that we don't have bins with the same starting positions.
    // This can happens mostly if TScale is an integer type due truncation.
    // Linear scale doesn't need this, because we check 'step' instead (see above).
    // Checking whole combined scale.
    //
    for (unsigned i = 1; i < m_NumBins; i++) {
        if ( arr[i] <= arr[i-1] ) {
            NCBI_THROW(CCoreException, eCore, errmsg_dup);
        }
    }
    return;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_CalculateBinsLinear
(
    TScale      start_value,
    TScale      end_value,
    TScale      step,
    TScale*     arr,
    unsigned    pos,
    unsigned    n,
    EScaleView  scale_view
)
{
    if (scale_view == eSymmetrical) {
        // Account for an integer TScale type and calculation truncation.
        // If TScale type doesn't allow integer division to the specified
        // number of bins without residue, some bins will be larger than others.
        // For monotonic view this will be always a very last bin at the end of
        // the scale, but for the symmetrical view it depends on parity of 'n'.
        // For even number of bins most left and right bins can be larger.
        // For odd number of bins the center bin can be larger than other.

        if (n % 2 == 0) {
            // Calculate from the center to both sides with step 'step'
            TScale median = start_value + (end_value - start_value)/2;
            x_CalculateBinsLinear(median, start_value, step, arr, pos + n/2 - 1, n/2, eMonotonic);
            x_CalculateBinsLinear(median, end_value,   step, arr, pos + n/2,     n/2, eMonotonic);
            // We already know value for the most left bin, assign it 
            // implicitly for such symmetrical scales to have bigger
            // starting bin. We got bigger ending bin automatically.
            arr[pos] = start_value;

        } else {
            // Calculate from both sides to the center with step 'step'
            // 2nd parameter doesn't matter, it just show direction.
            x_CalculateBinsLinear(start_value, end_value, step, arr, pos, n/2 + 1, eMonotonic);
            x_CalculateBinsLinear(end_value, start_value, step, arr, n-1, n/2,     eMonotonic);
        }
        return;
    }

    // Calculate from left to right
    if (start_value <= end_value) {
        for (unsigned i = 0; i < n; i++) {
            arr[pos+i] = start_value + step*i;
        }
    }
    // Calculate from right to left
    else {
        for (unsigned i = 1; i <= n; i++) {
            arr[pos+1-i] = start_value - step*i;
        }
    }
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_CalculateBinsLog
(
    TScale      start_value,
    TScale      end_value,
    TScale*     arr,
    unsigned    pos,
    unsigned    n,
    EScaleType  scale_type,
    EScaleView  scale_view
)
{
    if (scale_view == eSymmetrical) {
        // Account for an integer TScale type and calculation truncation.
        // If TScale type doesn't allow integer division to the specified
        // number of bins without residue, some bins will be larger than others.
        // For monotonic view this will be always a very last bin at the end of
        // the scale, but for the symmetrical view it depends on parity of 'n'.
        // For even number of bins most left and right bins can be larger.
        // For odd number of bins the center bin can be larger than other.

        TScale median = start_value + (end_value - start_value)/2;
        // Set most left bin directly, we already know its value
        arr[pos] = start_value;
        unsigned n2 = n/2;

        if (n % 2 == 0) {
            // Calculate from the center to both sides
            x_CalculateBinsLog(median, start_value, arr, pos + n2 - 1, n2, scale_type, eMonotonic);
            x_CalculateBinsLog(median, end_value,   arr, pos + n2    , n2, scale_type, eMonotonic);
        } else {
            if (n == 1) {
                return;
            }
            _ASSERT(n > 1);

            // Median is located in the middle of the center bin.
            // Too calculate starting point for this center bin end its ending point,
            // by the way also a starting point for the next bin, we double the number of bins
            // on each side of the median, and calculate values for each half-mark on a scale.
            // After that copy values for needed half-marks only.

            std::unique_ptr<TScale[]> tmp(new TScale[n*2]);
            TScale* tmp_arr = tmp.get();
            // Calculate from the center to both sides (n % 2 == 0 case)
            x_CalculateBinsLog(median, start_value, tmp_arr, n - 1, n, scale_type, eMonotonic);
            x_CalculateBinsLog(median, end_value,   tmp_arr, n    , n, scale_type, eMonotonic);

            // For central bin: copy values for 2 half-marks from both sides of the center
            arr[pos + n2]     = tmp_arr[n - 1];
            arr[pos + n2 + 1] = tmp_arr[n + 1];

            // For other bins: copy values for every second half-mark from 2 half-marks above
            for (unsigned i = 0; i < n2; i++) {
                arr[pos + i] = tmp_arr[i*2];
            }
            for (unsigned i=2, j=n+3; i <= n2; i++, j+=2) {
                arr[pos + n2 + i] = tmp_arr[j];
            }
        }
        return;
    }

    // Logarithm functions cannot operate on a negative values or 0.
    // so it will be necessary to transform data range. One of the methods
    // is too shift data to start it from 1. This method also helps to normalize
    // logarithmic scale, and we will use it for all data, even positive.
    // So, we calculate logarithms scale for a range of values [1:D+1],
    // where D is a distance between starting and ending points.
    // Because x^0 = 1, we need calculate only the maximum scale value log(D+1),
    // And every scale mark in between with the step log(D+1)/N.
    // Inverse function helps to convert every scale mark back to a bin
    // starting position.

    // Calculate from left to right
    if (start_value <= end_value) {
        TScale step = x_Func(scale_type, end_value - start_value + 1) / n;
        for (unsigned i = 1; i < n; i++) {
            // Calculate data value for a given scale mark
            arr[pos+i] = start_value + x_FuncInverse(scale_type, step*i) - 1;
        }
        // Set most left bin directly, we already know its value,
        // to avoid possible rounding errors.
        arr[pos] = start_value; 
    }
    // Calculate from right to left
    else {
        TScale step = x_Func(scale_type, start_value - end_value + 1) / n;
        for (unsigned i = 1; i < n; i++) {
            // Calculate data value for a given scale mark
            arr[pos+1-i] = start_value - x_FuncInverse(scale_type, step*i) + 1;
        }
        // Set most left bin directly, we already know its value,
        // to avoid possible rounding errors.
        arr[pos+1-n] = end_value; 
    }
}


template <typename TValue, typename TScale, typename TCounter>
TScale 
CHistogram<TValue, TScale, TCounter>::x_Func(EScaleType scale_type, TScale value)
{
    switch (scale_type) {
    case eLog:
        return (TScale)log(value);
    case eLog2:
        return (TScale)log2(value);
    case eLog10:
        return (TScale)log10(value);
    case eLinear:
        // It is last, because eLinear has served by x_CalculateBinsLinear() directly
        return value;
    }
    _TROUBLE;
    // unreachable, just to avoid warning
    return value; 
}


template <typename TValue, typename TScale, typename TCounter>
TScale 
CHistogram<TValue, TScale, TCounter>::x_FuncInverse(EScaleType scale_type, TScale scale_value)
{
    switch (scale_type) {
    case eLog:
        return (TScale)pow(M_E, scale_value);
    case eLog2:
        return (TScale)pow(2, scale_value);
    case eLog10:
        return (TScale)pow(10, scale_value);
    case eLinear:
        // It is last, because eLinear has served by x_CalculateBinsLinear() directly
        return scale_value;
    }
    _TROUBLE;
    // unreachable, just to avoid warning
    return scale_value; 
}


// Move semantics

template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::CHistogram()
    : m_NumBins(0), m_IsMT(false)
{
    // Reset counters
    x_Reset();
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::CHistogram(CHistogram&& other) 
{ 
    if (this == &other) return;
    other.MT_Lock();
    x_MoveFrom(other); 
    other.MT_Unlock();
};


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>& 
CHistogram<TValue, TScale, TCounter>::operator=(CHistogram&& other)
{ 
    if (this == &other) return *this;
    other.MT_Lock();
    x_MoveFrom(other); 
    other.MT_Unlock();
    return *this;
 };


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter> 
CHistogram<TValue, TScale, TCounter>::Clone(EClone how) const
{
    MT_Lock();

    CHistogram h;
    h.m_Min     = m_Min;
    h.m_Max     = m_Max;
    h.m_NumBins = m_NumBins;
    h.m_IsMT    = m_IsMT;

    h.m_Starts.reset(new TScale[m_NumBins]);
    h.m_Counters.reset(new TCounter[m_NumBins]);
    memcpy(h.m_Starts.get(), m_Starts.get(), sizeof(TScale) * m_NumBins);

    switch (how) {
    case eCloneStructureOnly:
        // Reset counters
        h.m_Sum = 0;
        h.m_Count = 0;
        h.m_LowerAnomalyCount = 0;
        h.m_UpperAnomalyCount = 0;
        memset(h.m_Counters.get(), 0, m_NumBins * sizeof(TCounter));
        break;
    case eCloneAll:
        // Copy counters
        h.m_Sum = m_Sum;
        h.m_Count = m_Count;
        h.m_LowerAnomalyCount = m_LowerAnomalyCount;
        h.m_UpperAnomalyCount = m_UpperAnomalyCount;
        memcpy(h.m_Counters.get(), m_Counters.get(), sizeof(TCounter) * m_NumBins);
        break;
    default:
        MT_Unlock();
        _TROUBLE;
    }
    MT_Unlock();
    return h;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_MoveFrom(CHistogram& other)
{
    m_Min               = other.m_Min;
    m_Max               = other.m_Max;
    m_NumBins           = other.m_NumBins;
    m_Sum               = other.m_Sum;
    m_Count             = other.m_Count;
    m_LowerAnomalyCount = other.m_LowerAnomalyCount;
    m_UpperAnomalyCount = other.m_UpperAnomalyCount;
    m_IsMT              = other.m_IsMT;

    m_Starts.reset(other.m_Starts.release());
    m_Counters.reset(other.m_Counters.release());
    other.m_NumBins = 0;
    other.m_Sum = 0;

    return;
}


template <typename TValue, typename TScale, typename TCounter>
bool
CHistogram<TValue, TScale, TCounter>::x_IsEqual(TScale a, TScale b)
{
    if (std::numeric_limits<TScale>::is_integer) {
        return a == b;
    }
    // Approximately check for floating numbers
    return std::fabs(a - b) < 0.0001;
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::AddCountersFrom(const CHistogram& other)
{ 
    if (this == &other) return;

    MT_Lock();
    other.MT_Lock();
    try {
        x_AddCountersFrom(const_cast<CHistogram&>(other));
    }
    catch (...) {
        MT_Unlock();
        other.MT_Unlock();
        throw;
    }
    MT_Unlock();
    other.MT_Unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::StealCountersFrom(CHistogram& other)
{
    if (this == &other) return;

    MT_Lock();
    other.MT_Lock();
    try {
        x_AddCountersFrom(other);
        other.x_Reset();
    }
    catch (...) {
        MT_Unlock();
        other.MT_Unlock();
        throw;
    }
    MT_Unlock();
    other.MT_Unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::x_AddCountersFrom(CHistogram& other)
{
    // Check structure
    if ( m_NumBins != other.m_NumBins  ||
        !x_IsEqual(m_Min, other.m_Min) ||
        !x_IsEqual(m_Max, other.m_Max) 
        ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Histograms have different structure");
    }
    // Check scale
    TScale* starts_cur   = m_Starts.get();
    TScale* starts_other = other.m_Starts.get();
    for (unsigned i = 0; i < m_NumBins; i++) {
        if (!x_IsEqual(starts_cur[i], starts_other[i])) {
            NCBI_THROW(CCoreException, eInvalidArg, "Histograms have different starting positions");
        }
    }
    // Add counters
    TCounter* counters_cur   = m_Counters.get();
    TCounter* counters_other = other.m_Counters.get();
    for (unsigned i = 0; i < m_NumBins; i++) {
        counters_cur[i] += counters_other[i];
    }
    m_Count += other.m_Count;
    m_LowerAnomalyCount += other.m_LowerAnomalyCount;
    m_UpperAnomalyCount += other.m_UpperAnomalyCount;
    m_Sum += other.m_Sum;
}



//============================================================================
//
// CHistogramTimeSeries 
//
//============================================================================

template <typename TValue, typename TScale, typename TCounter>
CHistogramTimeSeries<TValue, TScale, TCounter>::CHistogramTimeSeries(THistogram& model_histogram)
    : m_CurrentTick(0)
{
    // Need to create the first item in the list
    x_AppendBin(model_histogram, 1);
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogramTimeSeries<TValue, TScale, TCounter>::Add(TValue value)
{
    // The first bin always exists
    m_TimeBins.front().histogram.Add(value);
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogramTimeSeries<TValue, TScale, TCounter>::Rotate()
{
    ++m_CurrentTick;

    m_Mutex.lock();
    x_Shift(0, m_TimeBins.begin());
    m_Mutex.unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogramTimeSeries<TValue, TScale, TCounter>::Reset()
{
    m_Mutex.lock();
    m_CurrentTick = 0;
    while (m_TimeBins.size() > 1)
        m_TimeBins.pop_back();
    m_TimeBins.front().histogram.Reset();
    m_TimeBins.front().n_ticks = 1;     // Just in case
    m_Mutex.unlock();
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogramTimeSeries<TValue, TScale, TCounter>::x_Shift(size_t index,
       typename ncbi::CHistogramTimeSeries<TValue, TScale, TCounter>::TTimeBins::iterator current_it)
{
    if (m_TimeBins.size() <= index + 1) {
        // There is not enough bins. Need to add one
        x_AppendBin(m_TimeBins.front().histogram, TTicks(1) << index);

        // Move from index to index + 1
        auto next_it = current_it;
        ++next_it;
        next_it->histogram.StealCountersFrom(current_it->histogram);

        if (index != 0)
            current_it->n_ticks /= 2;
        return;
    }

    auto next_it = current_it;
    ++next_it;

    if (next_it->n_ticks == TTicks(1) << index) {
        // Half full bin; make it full and just accumulate
        next_it->n_ticks *= 2;
        next_it->histogram.StealCountersFrom(current_it->histogram);
        if (index != 0)
            current_it->n_ticks /= 2;
        return;
    }

    // The next bin is full; need to move further
    x_Shift(index + 1, next_it);
    // Move to the vacant next
    next_it->histogram.StealCountersFrom(current_it->histogram);
    // The current one is a half now
    if (index != 0)
        current_it->n_ticks /= 2;
}


template <typename TValue, typename TScale, typename TCounter>
typename CHistogramTimeSeries<TValue, TScale, TCounter>::TTimeBins
CHistogramTimeSeries<TValue, TScale, TCounter>::GetHistograms() const
{
    m_Mutex.lock();
    TTimeBins ret = m_TimeBins;
    m_Mutex.unlock();
    return ret;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogramTimeSeries<TValue, TScale, TCounter>::x_AppendBin(
    const THistogram& model_histogram, TTicks n_ticks)
{
    m_TimeBins.push_back(
            STimeBin{model_histogram.Clone(THistogram::eCloneStructureOnly),
                     n_ticks} );
}


END_NCBI_SCOPE

#endif  /* DATA_HISTOGRAM__HPP */
