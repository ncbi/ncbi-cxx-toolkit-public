//  (C) Copyright Gennadiy Rozental 2001-2008.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : defines algoirthms for comparing 2 floating point values
// ***************************************************************************

/*
 * $Id$
 * NOTE: This file was modified from its original version 1.39.0
 *       to fit the NCBI C++ Toolkit structure and namespaces.
 *
 * Disclaimer: The boost code is not supported by the C++ Toolkit group.
 *             Therefore please do not use this header directly. Use the
 *             <util/floating_point.hpp> declared functionality instead.
 */


#ifndef NCBI_BOOST_TEST_FLOATING_POINT_COMPARISON_HPP_071894GER
#define NCBI_BOOST_TEST_FLOATING_POINT_COMPARISON_HPP_071894GER

#ifndef FLOATING_POINT__HPP
#  error Do not include this header directly. Use <util/floating_point.hpp> instead.
#endif


#include <limits>

//____________________________________________________________________________//

namespace ncbi {

namespace boost_fp_impl {

// ************************************************************************** //
// **************        floating_point_comparison_type        ************** //
// ************************************************************************** //

enum floating_point_comparison_type {
    FPC_STRONG, // "Very close"   - equation 1' in docs, the default
    FPC_WEAK    // "Close enough" - equation 2' in docs.

};

// ************************************************************************** //
// **************                    details                   ************** //
// ************************************************************************** //

namespace tt_detail {

// FPT is Floating-Point Type: float, double, long double or User-Defined.
template<typename FPT>
inline FPT
fpt_abs( FPT arg )
{
    return arg < static_cast<FPT>(0) ? -arg : arg;
}

//____________________________________________________________________________//

template<typename FPT>
struct fpt_limits {
    static FPT    min_value()
    {
        return std::numeric_limits<FPT>::is_specialized
                    ? (std::numeric_limits<FPT>::min)()
                    : 0;
    }
    static FPT    max_value()
    {
        return std::numeric_limits<FPT>::is_specialized
                    ? (std::numeric_limits<FPT>::max)()
                    : static_cast<FPT>(1000000); // for the our purpuses it doesn't really matter what value is returned here
    }
};

//____________________________________________________________________________//

// both f1 and f2 are unsigned here
template<typename FPT>
inline FPT
safe_fpt_division( FPT f1, FPT f2 )
{
    // Avoid overflow.
    if( f2 < static_cast<FPT>(1)  && f1 > f2*fpt_limits<FPT>::max_value() )
        return fpt_limits<FPT>::max_value();

    // Avoid underflow.
    if( f1 == static_cast<FPT>(0) ||
        f2 > static_cast<FPT>(1) && f1 < f2*fpt_limits<FPT>::min_value() )
        return static_cast<FPT>(0);

    return f1/f2;
}

//____________________________________________________________________________//

} // namespace tt_detail

// ************************************************************************** //
// **************         tolerance presentation types         ************** //
// ************************************************************************** //

template<typename FPT>
struct percent_tolerance_t {
    explicit    percent_tolerance_t( FPT v ) : m_value( v ) {}

    FPT m_value;
};

//____________________________________________________________________________//

template<typename Out,typename FPT>
Out& operator<<( Out& out, percent_tolerance_t<FPT> t )
{
    return out << t.m_value;
}

//____________________________________________________________________________//

template<typename FPT>
inline percent_tolerance_t<FPT>
percent_tolerance( FPT v )
{
    return percent_tolerance_t<FPT>( v );
}

//____________________________________________________________________________//

template<typename FPT>
struct fraction_tolerance_t {
    explicit fraction_tolerance_t( FPT v ) : m_value( v ) {}

    FPT m_value;
};

//____________________________________________________________________________//

template<typename Out,typename FPT>
Out& operator<<( Out& out, fraction_tolerance_t<FPT> t )
{
    return out << t.m_value;
}

//____________________________________________________________________________//

template<typename FPT>
inline fraction_tolerance_t<FPT>
fraction_tolerance( FPT v )
{
    return fraction_tolerance_t<FPT>( v );
}

//____________________________________________________________________________//

// ************************************************************************** //
// **************             close_at_tolerance               ************** //
// ************************************************************************** //

template<typename FPT>
class close_at_tolerance {
public:
    // Public typedefs
    typedef bool result_type;

    // Constructor
    template<typename ToleranceBaseType>
    explicit    close_at_tolerance( percent_tolerance_t<ToleranceBaseType>  tolerance,
                                    floating_point_comparison_type          fpc_type = FPC_STRONG )
    : p_fraction_tolerance( tt_detail::fpt_abs( static_cast<FPT>(0.01)*tolerance.m_value ) )
    , p_strong_or_weak( fpc_type ==  FPC_STRONG )
    {}
    template<typename ToleranceBaseType>
    explicit    close_at_tolerance( fraction_tolerance_t<ToleranceBaseType> tolerance,
                                    floating_point_comparison_type          fpc_type = FPC_STRONG )
    : p_fraction_tolerance( tt_detail::fpt_abs( tolerance.m_value ) )
    , p_strong_or_weak( fpc_type ==  FPC_STRONG )
    {}

    bool        operator()( FPT left, FPT right ) const
    {
        FPT diff = tt_detail::fpt_abs( left - right );
        FPT d1   = tt_detail::safe_fpt_division( diff, tt_detail::fpt_abs( right ) );
        FPT d2   = tt_detail::safe_fpt_division( diff, tt_detail::fpt_abs( left ) );

        return p_strong_or_weak
                   ? (d1 <= p_fraction_tolerance && d2 <= p_fraction_tolerance)
                   : (d1 <= p_fraction_tolerance || d2 <= p_fraction_tolerance);
    }

    // Public properties
    FPT     p_fraction_tolerance;
    bool    p_strong_or_weak;
};

//____________________________________________________________________________//

// ************************************************************************** //
// **************               check_is_close                 ************** //
// ************************************************************************** //

struct check_is_close_t {
    // Public typedefs
    typedef bool result_type;

    template<typename FPT, typename ToleranceBaseType>
    bool
    operator()( FPT left, FPT right, percent_tolerance_t<ToleranceBaseType> tolerance, 
                floating_point_comparison_type fpc_type = FPC_STRONG )
    {
        close_at_tolerance<FPT> pred( tolerance, fpc_type );

        return pred( left, right );
    }
    template<typename FPT, typename ToleranceBaseType>
    bool
    operator()( FPT left, FPT right, fraction_tolerance_t<ToleranceBaseType> tolerance,
                floating_point_comparison_type fpc_type = FPC_STRONG )
    {
        close_at_tolerance<FPT> pred( tolerance, fpc_type );

        return pred( left, right );
    }
};

namespace {
check_is_close_t check_is_close;
}

//____________________________________________________________________________//

// ************************************************************************** //
// **************               check_is_small                 ************** //
// ************************************************************************** //

struct check_is_small_t {
    // Public typedefs
    typedef bool result_type;

    template<typename FPT>
    bool
    operator()( FPT fpv, FPT tolerance )
    {
        return tt_detail::fpt_abs( fpv ) < tt_detail::fpt_abs( tolerance );
    }
};

namespace {
check_is_small_t check_is_small; /* NCBI_FAKE_WARNING */
}

//____________________________________________________________________________//

} // namespace boost_fp_impl

} // namespace ncbi

//____________________________________________________________________________//


#endif // NCBI_BOOST_FLOATING_POINT_COMAPARISON_HPP_071894GER
