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
/// Collector for data distribution of the numerical samples.

#include <corelib/ncbistd.hpp>

#define _USE_MATH_DEFINES // to define math constants in math.h
#include <math.h>
#include <cmath>          // additional overloads for pow()
#include <list>


/** @addtogroup Statistics
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CHistogram -- collect the distribution of the numerical data samples.
///  
/// CHistogram <TValue, TScale, TCounter>
///
/// TValue -   type of values that distribution will be collected.
///            This can be any numerical or user defined type, if it allow
///            comparison abd can be converted to scale type TScale, see note. 
///
/// TScale -   type used to calculate histogram's bins start positions
///            and sizes, any numerical. For integer types some scales,
///            natural logarithmic for examples, will lead to truncating values
///            and creating unequal bins, so these scale types should have
///            be float/double based.
///
/// TCounter - type for the counters. Usually this is a positive integer type:
///            int, unsigned int, long, size_t, Uint8 and etc.
/// @note
///   TValue and TScale types should be equal, or allow comparison
///   between them. Also, TValue should allow conversion to TScale.
///   User defined type can be used for TValue (see test/demo), 

template <typename TValue, typename TScale, typename TCounter>
class CHistogram
{
public:
    class Collector;
    
    /// Scale for a histogram.
    /// Defines scale type and data range it cover. Doesn't store any data.
    /// 
    /// Most common is a linear scale, but sometimes data is distributed very wide,
    /// If you want to count all of them, but concentrate on a range with most 
    /// significant values, logarithmic scales helps a lot.
    /// 
    class Scale
    {
    public:
        /// Scale type.
        /// For linear scale each bin have the same size.
        /// For logarithmic scales each next bin have a greather size, and bins increasing
        /// depends on a logariphmic base and used step (see Collector constructors).
        ///
        /// @sa TFunc, Collector
        enum EType {
            eLinear = 1, ///< Arithmetic or linear scale
            eLog,        ///< Natural logarithmic scale with a base e ~ 2.72
            eLog2,       ///< Binary logarithmic scale with a base 2
            eLog10       ///< Common logarithmic scale with a base 10
        };

        /// Methods to builds bins for a specified scale.
        /// @note
        ///   Matter for non-linear scales only.
        ///   For a linear scale (eLinear) bins will have the same sizes regardless of used method.
        enum EView {
            /// Use specified scale method to calculate bins sizes from a minimum
            /// to a maximum value.
            eMonotonic,
            /// Determine a mean for a specified value range and calculates
            /// bins sizes using specified scale to both sides from it, symmetricaly.
            eSymmetrical,
            /// Special value used for multi-scale histograms with major/minor scales,
            /// the collector automatically use monotonic or symmetrical view depending on a number of scales.
            /// For single-scale histograms eMonotonic will be always used.
            eAuto
        };

        /// Type of function to calculate scale's bin range. 
        /// @note
        ///   Regardless of a scale name, that is a mathematical term,
        ///   API use scale functions, or inverse functions, to calculate bins ranges.
        ///   https://en.wikipedia.org/wiki/Inverse_function
        ///   For example, to implement a common logarithmic scale 
        ///   an exponential function with a base 10 should be used: f(x) = 10^x.
        ///   All custom functions should implement the same principle to calculate bin
        ///   ranges on a base of a scale step (provided via constructor's parameter).
        /// @note
        ///   With 'step' increase returned value should increase also.
        ///   Usually difference between returned values should increase or be
        ///   the same with each iteration.
        ///   
        /// @sa EType
        typedef TScale (*TFunc)(TScale);

        /// Scale step type.
        /// Usually scale starts calculating bins with passing initial step value into
        /// the scale function, and increase step on each iteration on a 'step' value.
        /// But some function behave really bad or even cannot be calculated for
        /// some values. To avoid this you can specify a starting point for a 'step'
        /// parameter. You can pass single TScale type value everywhere when TStep parameter
        /// is specified, or use pair {TValue, TValue}, that represent {step, start} respectively.
        ///
        struct TStep  {
            TStep(TScale v, TScale s = 0) : value(v), start(s > 0 ? s : v) {};
            TScale value;
            TScale start;
            /// Default constructor, used by Collector
            TStep(void) {};
        };

        /// Constructor for predefined scale types.
        ///
        /// @param min_value
        ///   Minimum value for this scale in range [min_value, max_value].
        /// @param max_value
        ///   Maximum value for this scale in range [min_value, max_value].
        /// @param step
        ///   Step for a scale function. A parameter for inverse function 'func' to calculate bins start
        ///   position (and sizes). For example, for a linear scale each bin will have the same
        ///   size on whole range from a 'min_value' to 'max_value' with specified step 'step'.
        ///   Another example, a common logarithm, this is a step for an exponent in inverse function:
        ///   10 ^ (step*n). 
        ///   Step parameter can be an integer type or float.
        /// @param type
        ///   Predefined scale type. Corresponding inverse function will be used to calculate 
        ///   scale's bins ranges. See TFunc description.
        /// @param view
        ///   Scale view (symmetrical or monotonic).
        /// @sa TFunc, EView
        ///
        Scale(TValue min_value, TValue max_value, TStep step, EType type, EView view = eAuto);

        /// Constructor.
        ///
        /// @param min_value
        ///   Minimum value for this scale in range [min_value, max_value].
        /// @param max_value
        ///   Maximum value for this scale in range [min_value, max_value].
        /// @param step
        ///   Step for a scale function. A parameter for inverse function 'func' to calculate bins start
        ///   position (and sizes). For example, for a linear scale each bin will have the same
        ///   size on whole range from a 'min_value' to 'max_value' with specified step 'step'.
        ///   Another example, a common logarithm, this is a step for an exponent in inverse function:
        ///   10 ^ (step*n). 
        ///   Step parameter can be an integer type or float. 
        /// @param func
        ///   Function to calculate scale's bins range (distribution function).
        /// @param view
        ///   Scale view (symmetrical or monotonic).
        /// @sa TFunc, EView
        ///
        Scale(TValue min_value, TValue max_value, TStep step, TFunc func, EView view = eAuto);

    protected:
        /// Default constructor (needed for a Collector's members definitions)
        Scale(void) {}
        friend class CHistogram<TValue,TScale,TCounter>::Collector;

        /// Call scale function: predefined type or provided by user
        TScale Func(TScale step);

        /// Calculate number of bins and its starting positions.
        /// Calculate for [start,end] range. If 'start' > 'end', calculating going from the right to left.
        /// This is applicable for symmetrical scales, or multi-scale collectors, with minor scales on the left,
        /// to calculate left side of a scale.
        /// @return
        ///   Number of bins.
        size_t CalculateBins(TScale start, TScale end, EView view = eMonotonic);

    protected:
        TValue  m_Min;     ///< Minimum scale value
        TValue  m_Max;     ///< Maximum scale value
        TValue  m_Lim;     ///< Minor scale only: limit value (min/max will be calculated later)
        TStep   m_Step;    ///< Step for a scale function
        EType   m_Type;    ///< Predefined scale type
        TFunc   m_Func;    ///< Scale's distribution function (if NULL, predefined m_Type will be used)
        EView   m_View;    ///< Scale view (symmetrical or monotonic)
        size_t  m_NumBins; ///< Number of bins (m_Starts length)

        std::list<TScale> m_Starts;  ///< Calculated bins starting positions.
    };

    /// Scale used in a multi-scale collectors for a most significant data range.
    /// Usually simple linear scale (eLinear) works best here.
    ///
    /// @sa Scale, MinorScale
    ///
    class MajorScale : public Scale
    {
    public:
        MajorScale(TValue min_value, TValue max_value, 
                   typename Scale::TStep step, typename Scale::EType type)
            : Scale(min_value, max_value, step, type, Scale::eAuto)
        {}
        MajorScale(TValue min_value, TValue max_value,
                   typename Scale::TStep step, typename Scale::TFunc func)
            : Scale(min_value, max_value, step, func, Scale::eAuto)
        {}
    };

    /// Scale used in a multi-scale collectors for a less significant data range,
    /// allow to collect all deviations, that doesn't fit into a "major" data range,
    /// but still be usefull to collect. Any logarithmic scale works best here,
    /// depending how much deviation yours data have.
    ///
    /// If major monotonic scale always calculates from the left to right, or simmetricaly
    /// around the center of the range, that minor scales depends from a major scale
    /// limits and side from which it adjoin a major scale. If it locates on the left side
    /// from a major scale, calculation going from a right to left, from a major's scale
    /// minimum value to a 'limit_value' of a minor scale.
    ///
    /// @param limit_value
    ///    Minimum or maximum value for a minor scale.
    ///    Minor scale automatically use range from a 'limit_value' to a majors's scale
    ///    minimum value, or from a majors's scale maximum value to a 'limit_value',
    ///    depends on used collectors constructor, and side from which it adjoin a major scale.
    /// @sa Scale, MajorScale
    ///
    class MinorScale : public Scale
    {
    public:
        MinorScale(TValue limit_value, 
                   typename Scale::TStep step, typename Scale::EType type)
            : Scale(limit_value, limit_value, step, type, Scale::eMonotonic)
        {}
        MinorScale(TValue limit_value,
                   typename Scale::TStep step, typename Scale::TFunc func)
            : Scale(limit_value, limit_value, step, func, Scale::eMonotonic)
        {}
    };

    /// Collector for the distribution of the numerical data samples.
    /// Can use one or some scales to split data range to bins.
    /// Each bin holds the number of data values that fit into it's range.
    ///
    /// @sa Scale, MajorScale, MinorScale
    ///
    class Collector
    {
    public:
        /// Single scale histogram data collector.
        /// @sa Scale
        Collector(Scale scale);

        /// Two scale histogram data collector - [major_scale][minor_scale]
        /// @sa MajorScale, MinorScale
        Collector(MajorScale major_scale, MinorScale minor_scale);

        /// Two scale histogram data collector - [minor_scale][major_scale]
        /// @sa MajorScale, MinorScale
        Collector(MinorScale minor_scale, MajorScale major_scale);

        /// Triple scale histogram data collector - [min_scale][major_scale][max_scale]
        /// @sa MajorScale, MinorScale
        Collector(MinorScale min_scale, MajorScale major_scale, MinorScale max_scale);

        /// Destructor.
        ~Collector();

        /// Add value to the data distribution.
        /// Try to find an appropriate bin for a specified value and increase its counter on 1.
        /// @sa GetStarts, GetCounters
        void Add(TValue value);

        /// Get minimum value for a combined scale
        TValue GetMin() const { return m_Min; }

        /// Get maximum value for a combined scale
        TValue GetMax() const { return m_Max; }

        /// Get total counted values.
        /// This is a number of Add() method calls.
        /// If value doesn't fit to scale, or combined scale for a multi-scale collector,
        /// it counts as an "anomaly". Anomalies doesn't increase any bin counters.
        /// You can get number of anomalies with GetAnomalyCount().
        /// @sa GetMin, GetMax, GetAnomalyCount
        size_t GetTotalCount() const { return m_Total; }

        /// Get the number of anomaly values that fall out of range for used scale(s).
        /// @sa GetAnomalyCountMin, GetAnomalyCountMax, GetTotalCount
        size_t GetAnomalyCount() const { return m_AnomalyMin + m_AnomalyMax; }

        /// Get number of anomaly values that are less that a minimum for used scale.
        size_t GetAnomalyCountMin() const { return m_AnomalyMin; }

        /// Get number of anomaly values that are greater that a maximum for used scale.
        size_t GetAnomalyCountMax() const { return m_AnomalyMax; }

        /// Return the number ot bins on a scale.
        /// By default it returns the total number ot bins on a combined scale.
        /// But you can get the number ot bins for each used scale specifying
        /// its number from 1 to 3. Maximum numbers of scales depends on used 
        /// constructor for a current collector.
        /// @sa GetStarts, GetCounters, Collector
        size_t GetNumberOfBins(size_t scale_num = 0) const;

        /// Get starting positions for bins on a scale(s).
        /// If multiple scales were used, this method returns starting positions
        /// for a combined scale. The number of elements/bins can be obtained with
        /// GetNumberOfBins(), and a number of a counters for each bin with GetCounters().
        /// @sa GetNumberOfBins, GetCounters
        const TScale* GetStarts() const { return (const TScale*) m_Starts; }

        /// Get counters for scale(s) bins.
        /// @sa Add, GetNumberOfBins, GetStarts
        const TCounter* GetCounters() const { return (const TCounter*) m_Counters; }

        /// Reset data counters for all scales and bins.
        void Reset();

    private:
        size_t    m_NumScales;    ///< Number of used scales.
        Scale     m_Scales[3];    ///< Scales, maximum 3, depends on used constructor.
        TValue    m_Min;          ///< Minimum value that fit into the scale(s) range
        TValue    m_Max;          ///< Maximum value that fit into the scale(s) range
        TCounter  m_Total;        ///< Total mumber of counted values
        TCounter  m_AnomalyMin;   ///< Number of anomaly values < m_Min
        TCounter  m_AnomalyMax;   ///< Number of anomaly values > m_Max

        size_t    m_NumBins;      ///< Number of bins on a combined scale (m_Starts[] and m_Counters[] length)
        TScale*   m_Starts;       ///< Combined scale: starting bins positions
        TCounter* m_Counters;     ///< Combined scale: counters - the number of measurements for each bin

    private:
        // Add() methods for searching bins
        enum EAddMethod {
            eLinear,
            eBisection
        };
        EAddMethod m_AddMethod;

        /// Add value to the data distribution, use a linear search method.
        /// Usually faster than bisection method on a small number of bins,
        /// or if the values have a tendency to fall into the starting bins
        /// for a used scale(s).
        /// @sa Add
        void AddLinear(TValue value);

        /// Add value to the data distribution, use a bisection search method.
        /// Usually faster on a long scales with a lot of bins.
        /// @sa Add, AddLinear
        void AddBisection(TValue value);

        // Prevent copying
        Collector(const Collector&);
        Collector& operator=(const Collector&);
    };
};


/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Scale::Scale(
    TValue min_value, TValue max_value, TStep step, EType type, EView view
    )
    : m_Min(min_value), m_Max(max_value), m_Lim(min_value), 
      m_Step(step), m_Type(type), m_Func(NULL), m_View(view), m_NumBins(0)
{
    if ( m_Min > m_Max ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Minimum value cannot exceed maximum value");
    }
    if ( m_Step.value <= 0 ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Step argument must be greater than zero");
    }
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Scale::Scale(
    TValue min_value, TValue max_value, TStep step, TFunc func, EView view
    )
    : m_Min(min_value), m_Max(max_value), m_Lim(min_value), 
      m_Step(step), m_Type((EType)0), m_Func(func), m_View(view), m_NumBins(0)
{
    if ( m_Min > m_Max ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Minimum value cannot exceed maximum value");
    }
    if ( m_Step.value <= 0 ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Step argument must be greater than zero");
    }
    if ( m_Func == NULL ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Scale function is not specified");
    }
}


template <typename TValue, typename TScale, typename TCounter>
TScale 
CHistogram<TValue, TScale, TCounter>::Scale::Func(TScale step)
{
    if (m_Func) {
        return (*m_Func)(step);
    } else {
        // Predefined scale functions (inverse functions for EType types)
        switch (m_Type) {
        case eLinear:
            return step;
        case eLog:
            return (TScale)pow(M_E, step);
        case eLog2:
            return (TScale)pow(2, step);
        case eLog10:
            return (TScale)pow(10, step);
        }
    }
    _TROUBLE;
    // unreachable, just to avoid warning
    return step; 
}


template <typename TValue, typename TScale, typename TCounter>
size_t CHistogram<TValue, TScale, TCounter>::Scale::CalculateBins(
    TScale start, TScale end, EView view)
{
    const char* errmsg = "Impossible to calculate scale bin size, please change its step or TScale type";

    if (view == eSymmetrical) {
        TScale median = (TScale)((end - start) / 2);
        // Calculate from a median to the left and to the right separately
        CalculateBins(median, start, eMonotonic);
        CalculateBins(median, end, eMonotonic);
        m_NumBins = m_Starts.size();
        return m_NumBins;
    }
    TScale s = start;            // starting point of scale
    TScale v = s;                // current value 
    TScale step = m_Step.start;  // accumulated step

    if (start <= end) {
        // Calculate from left to right
        while (v < end) {
            m_Starts.push_back(v);
            TScale prev = v;
            v = start + Func(step);
            // Check values to avoid infinite loops with the same starting points for bins due choosen types and truncating.
            if ( v <= prev ) {
                NCBI_THROW(CCoreException, eCore, errmsg);
            }
            step += m_Step.value;
        }
    }
    else {
        // Calculate from right to left
        while (end < v) {
            TScale prev = v;
            v = start - Func(step);
            // Check values to avoid infinite loops with the same starting points for bins due choosen types and truncating.
            if ( v >= prev ) {
                NCBI_THROW(CCoreException, eCore, errmsg);
            }
            m_Starts.push_front((end < v) ? v : end);
            step += m_Step.value;
        }
    }
    if ( !m_Starts.size() ) {
        NCBI_THROW(CCoreException, eCore, errmsg);
    }
    m_NumBins = m_Starts.size();
    return m_NumBins;
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Collector::Collector(Scale scale)
    : m_AddMethod(eBisection)
{
    m_Min = scale.m_Min;
    m_Max = scale.m_Max;
    m_NumBins = scale.CalculateBins((TScale)m_Min, (TScale)m_Max, scale.m_View);

    // Allocate memory
    m_Starts = new TScale[m_NumBins];
    m_Counters = new TCounter[m_NumBins];

    // Copy starting positions from scale list to speed up a search
    size_t i = 0;
    for (auto const& s : scale.m_Starts) { m_Starts[i++] = s; }
    scale.m_Starts.clear();

    // Set defauilt add method
    if (m_NumBins < 50) {
        m_AddMethod = eLinear;
    }
    // Save scale
    m_NumScales = 1;
    m_Scales[0] = scale;

    // Reset counters
    Reset();
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Collector::Collector(
    MinorScale minor_scale, MajorScale major_scale)
    : m_AddMethod(eBisection)
{
    if (minor_scale.m_Lim >= major_scale.m_Min) {
        NCBI_THROW(CCoreException, eInvalidArg, "Minor scale have incorrect limit");
    }
    m_Min      = minor_scale.m_Lim;
    m_Max      = major_scale.m_Max;
    m_NumBins  = minor_scale.CalculateBins((TScale)major_scale.m_Min, (TScale)minor_scale.m_Lim, Scale::eMonotonic)
               + major_scale.CalculateBins((TScale)major_scale.m_Min, (TScale)major_scale.m_Max, Scale::eMonotonic);

    // Update range for a minor scale (it already have m_Min)
    minor_scale.m_Max = major_scale.m_Min;

    // Allocate memory
    m_Starts   = new TScale   [m_NumBins];
    m_Counters = new TCounter [m_NumBins];

    // Copy starting positions from scale list to speed up a search
    size_t i = 0;
    for (auto const& s : minor_scale.m_Starts) { m_Starts[i++] = s; }
    for (auto const& s : major_scale.m_Starts) { m_Starts[i++] = s; }
    minor_scale.m_Starts.clear();
    major_scale.m_Starts.clear();

    // Set defauilt add method
    if (m_NumBins < 50  ||  (m_NumBins < 100  &&  major_scale.m_NumBins < 50) ) {
        m_AddMethod = eLinear;
    }
    // Save scales
    m_NumScales = 2;
    m_Scales[0] = minor_scale;
    m_Scales[1] = major_scale;

    // Reset counters
    Reset();
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Collector::Collector(
    MajorScale major_scale, MinorScale minor_scale)
    : m_AddMethod(eBisection)
{
    if (minor_scale.m_Lim <= major_scale.m_Max) {
        NCBI_THROW(CCoreException, eInvalidArg, "Minor scale have incorrect limit");
    }
    m_Min      = major_scale.m_Min;
    m_Max      = minor_scale.m_Lim;
    m_NumBins  = major_scale.CalculateBins((TScale)major_scale.m_Min, (TScale)major_scale.m_Max, Scale::eMonotonic)
               + minor_scale.CalculateBins((TScale)major_scale.m_Max, (TScale)minor_scale.m_Lim, Scale::eMonotonic);

    // Update range for a minor scale (it already have m_Max)
    minor_scale.m_Min = major_scale.m_Max;

    // Allocate memory
    m_Starts   = new TScale   [m_NumBins];
    m_Counters = new TCounter [m_NumBins];

    // Copy starting positions from scale list to speed up a search
    size_t i = 0;
    for (auto const& s : major_scale.m_Starts) { m_Starts[i++] = s; }
    for (auto const& s : minor_scale.m_Starts) { m_Starts[i++] = s; }
    minor_scale.m_Starts.clear();
    major_scale.m_Starts.clear();

    // Set defauilt add method
    if (m_NumBins < 50  ||  (m_NumBins < 100  &&  major_scale.m_NumBins < 50) ) {
        m_AddMethod = eLinear;
    }
    // Save scales
    m_NumScales = 2;
    m_Scales[0] = major_scale;
    m_Scales[1] = minor_scale;

    // Reset counters
    Reset();
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Collector::Collector(
    MinorScale min_scale, MajorScale major_scale, MinorScale max_scale)
    : m_AddMethod(eBisection)
{
    if (min_scale.m_Lim >= major_scale.m_Min) {
        NCBI_THROW(CCoreException, eInvalidArg, "Left minor scale have incorrect limit");
    }
    if (max_scale.m_Lim <= major_scale.m_Max) {
        NCBI_THROW(CCoreException, eInvalidArg, "Right minor scale have incorrect limit");
    }
    m_Min      = min_scale.m_Lim;
    m_Max      = max_scale.m_Lim;
    m_NumBins  = min_scale.CalculateBins((TScale)major_scale.m_Min, (TScale)min_scale.m_Lim, Scale::eMonotonic)
               + major_scale.CalculateBins((TScale)major_scale.m_Min, (TScale)major_scale.m_Max, Scale::eMonotonic)
               + max_scale.CalculateBins((TScale)major_scale.m_Max, (TScale)max_scale.m_Lim, Scale::eMonotonic);

    // Update ranges for a minor scales
    min_scale.m_Max = major_scale.m_Min;
    max_scale.m_Max = max_scale.m_Lim;
    max_scale.m_Min = major_scale.m_Max;

    // Allocate memory
    m_Starts   = new TScale   [m_NumBins];
    m_Counters = new TCounter [m_NumBins];

    // Copy starting positions from scale list to speed up a search
    size_t i = 0;
    for (auto const& s : min_scale.m_Starts)   { m_Starts[i++] = s; }
    for (auto const& s : major_scale.m_Starts) { m_Starts[i++] = s; }
    for (auto const& s : max_scale.m_Starts)   { m_Starts[i++] = s; }
    min_scale.m_Starts.clear();
    major_scale.m_Starts.clear();
    max_scale.m_Starts.clear();

    // Set defauilt add method
    if (m_NumBins < 50  ||  (m_NumBins < 200  &&  major_scale.m_NumBins < 50) ) {
        m_AddMethod = eLinear;
    }
    // Save scales
    m_NumScales = 3;
    m_Scales[0] = min_scale;
    m_Scales[1] = major_scale;
    m_Scales[2] = max_scale;

    // Reset counters
    Reset();
}


template <typename TValue, typename TScale, typename TCounter>
CHistogram<TValue, TScale, TCounter>::Collector::~Collector()
{
    delete [] m_Starts;
    delete [] m_Counters;
}


template <typename TValue, typename TScale, typename TCounter>
size_t 
CHistogram<TValue, TScale, TCounter>::Collector::GetNumberOfBins(size_t scale_num) const
{
    if (!scale_num) {
        return m_NumBins;
    }
    if (scale_num > m_NumScales) {
        return 0;
//        NCBI_THROW(CCoreException, eInvalidArg, "The scale number argument exceed number of scales");
    }
    return m_Scales[scale_num-1].m_NumBins;;
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::Collector::Reset(void)
{
    // Reset counters
    m_Total = m_AnomalyMin = m_AnomalyMax = 0;
    // Reset bins
    memset(m_Counters, 0, m_NumBins * sizeof(TCounter));
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::Collector::Add(TValue value)
{
    if (m_AddMethod == eLinear) {
        AddLinear(value);
        return;
    }
    AddBisection(value);
}


template <typename TValue, typename TScale, typename TCounter>
void 
CHistogram<TValue, TScale, TCounter>::Collector::AddLinear(TValue value)
{
    m_Total++;
    if (value < m_Min) {
        m_AnomalyMin++;
        return;
    }
    if (value > m_Max) {
        m_AnomalyMax++;
        return;
    }
    size_t i = 1;
    // speedup: find starting scale
    for (size_t s = 0; s < m_NumScales-1; s++) {
        if (value >= m_Scales[s].m_Max) {
            i += m_Scales[s].m_Starts.size();
        }
    }
    // find bin
    while (i < m_NumBins) {
        if (value < m_Starts[i]) {
            break;
        }
        i++;
    }
    // Current bin have greater value, so put it into the previous one
    m_Counters[i-1]++;
}


template <typename TValue, typename TScale, typename TCounter>
void
CHistogram<TValue, TScale, TCounter>::Collector::AddBisection(TValue value)
{
    m_Total++;
    if (value < m_Min) {
        m_AnomalyMin++;
        return;
    }
    if (value > m_Max) {
        m_AnomalyMax++;
        return;
    }
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
    // Algorithm can finish on a current or next bin, depending on even/odd number of bins
    if (value < m_Starts[i]) {
        i--;
    }
    m_Counters[i]++;
}


END_NCBI_SCOPE

#endif  /* DATA_HISTOGRAM__HPP */
