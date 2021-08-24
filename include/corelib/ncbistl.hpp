#ifndef CORELIB___NCBISTL__HPP
#define CORELIB___NCBISTL__HPP

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
 * Author:  Denis Vakatov
 *
 *
 */

/// @file ncbistl.hpp
/// The NCBI C++/STL use hints.


#include <common/ncbi_export.h>
#include <iosfwd>
#include <type_traits>
#include <typeinfo>


// Get rid of some warnings in MSVC++
#if (_MSC_VER >= 1200)
// too long identificator name in the debug info;  truncated
#  pragma warning(disable: 4786)
// too long decorated name;  truncated
#  pragma warning(disable: 4503)
// default copy constructor cannot be generated
#  pragma warning(disable: 4511)
// default assignment operator cannot be generated
#  pragma warning(disable: 4512)
// synonymous name used
#  pragma warning(disable: 4097)
// inherits ... via dominance
#  pragma warning(disable: 4250)
// 'this' : used in base member initializer list
#  pragma warning(disable: 4355)
// identifier was truncated to '255' characters in the browser information
#  pragma warning(disable: 4786)
#endif /* _MSC_VER >= 1200 */


/** @addtogroup STL
 *
 * @{
 */


/// Define a new scope.
#define BEGIN_SCOPE(ns) namespace ns {

/// End the previously defined scope.
#define END_SCOPE(ns) }

/// Use the specified namespace.
#define USING_SCOPE(ns) using namespace ns

// Using STD and NCBI namespaces

/// Define the std namespace.
#define NCBI_NS_STD  std

/// Use the std namespace.
#define NCBI_USING_NAMESPACE_STD using namespace NCBI_NS_STD

/// Define the name for the NCBI namespace.
#define NCBI_NS_NCBI ncbi

/// Place it for adding new funtionality to STD scope
#define BEGIN_STD_SCOPE BEGIN_SCOPE(NCBI_NS_STD)

/// End previously defined STD scope.
#define END_STD_SCOPE   END_SCOPE(NCBI_NS_STD)

/// Define ncbi namespace.
///
/// Place at beginning of file for NCBI related code.
#define BEGIN_NCBI_SCOPE BEGIN_SCOPE(NCBI_NS_NCBI)

/// End previously defined NCBI scope.
#define END_NCBI_SCOPE   END_SCOPE(NCBI_NS_NCBI)

/// For using NCBI namespace code.
#define USING_NCBI_SCOPE USING_SCOPE(NCBI_NS_NCBI)


/// Magic spell ;-) needed for some weird compilers... very empiric.
namespace NCBI_NS_STD  { /* the fake one */ }

/// Magic spell ;-) needed for some weird compilers... very empiric.
namespace NCBI_NS_NCBI { /* the fake one, +"std" */ NCBI_USING_NAMESPACE_STD; }

/// Magic spell ;-) needed for some weird compilers... very empiric.
namespace NCBI_NS_NCBI { /* the fake one */ }


#if !defined(NCBI_NAME2)
/// Name concatenation macro with two names.
/// NB: If this names are macros themselves expanding will not take place.
#  define NCBI_NAME2(Name1, Name2) Name1##Name2
#endif
#if !defined(NCBI_NAME3)
/// Name concatenation macro with three names.
/// NB: If this names are macros themselves expanding will not take place.
#  define NCBI_NAME3(Name1, Name2, Name3) Name1##Name2##Name3
#endif

#if !defined(NCBI_EAT_SEMICOLON)
namespace DummyNS { class CDummyClassToEatSemicolon; }
#  define NCBI_EAT_SEMICOLON(UniqueName)                \
    using ::DummyNS::CDummyClassToEatSemicolon
#endif

#define BEGIN_NAMESPACE(ns) namespace ns { NCBI_EAT_SEMICOLON(ns)
#define END_NAMESPACE(ns) } NCBI_EAT_SEMICOLON(ns)
#define BEGIN_NCBI_NAMESPACE BEGIN_NAMESPACE(NCBI_NS_NCBI)
#define END_NCBI_NAMESPACE END_NAMESPACE(NCBI_NS_NCBI)
#define BEGIN_STD_NAMESPACE BEGIN_NAMESPACE(NCBI_NS_STD)
#define END_STD_NAMESPACE END_NAMESPACE(NCBI_NS_STD)
#define BEGIN_LOCAL_NAMESPACE namespace { NCBI_EAT_SEMICOLON(ns)
#define END_LOCAL_NAMESPACE } NCBI_EAT_SEMICOLON(ns)

/// Convert some value to string even if this value is macro itself
#define NCBI_AS_STRING(value)   NCBI_AS_STRING2(value)
#define NCBI_AS_STRING2(value)   #value


#if defined(NCBI_COMPILER_MSVC) && _MSC_VER < 1400 && !defined(for)
/// Fix nonstandard 'for' statement behaviour on MSVC 7.1.
# define for if(0);else for
#endif

#if defined(NCBI_COMPILER_ICC)  &&  NCBI_COMPILER_VERSION < 2000
/// ICC may fail to generate code preceded by "template<>".
#define EMPTY_TEMPLATE
#else
#define EMPTY_TEMPLATE template<>
#endif

#ifdef NCBI_COMPILER_WORKSHOP
# if NCBI_COMPILER_VERSION < 530
/// Advance iterator to end and then break out of loop.
///
/// Sun WorkShop < 5.3 fails to call destructors for objects created in
/// for-loop initializers; this macro prevents trouble with iterators
/// that contain CRefs by advancing them to the end, avoiding
/// "deletion of referenced CObject" errors.
#  define BREAK(it) while (it) { ++(it); }  break
# else
#  define BREAK(it) break
# endif
#else
# define BREAK(it) break
#endif

#if defined(NCBI_COMPILER_GCC) || defined(NCBI_COMPILER_WORKSHOP)
#  ifdef NCBI_COMPILER_GCC
#    include <algorithm>
#  endif
// This template is used by some stl algorithms (sort, reverse...)
// We need to have our own implementation because some C++ Compiler vendors 
// implemented it by using a temporary variable and an assignment operator 
// instead of std::swap function.
// GCC 3.4 note: because this compiler has a broken function template 
// specialization this template function should be defined before 
// including any stl include files.
BEGIN_STD_SCOPE
template<typename Iter>
inline
void iter_swap( Iter it1, Iter it2 )
{
    swap( *it1, *it2 );
}

END_STD_SCOPE

#if defined(_GLIBCXX_DEBUG)
/* STL iterators are non-POD types */
# define NCBI_NON_POD_TYPE_STL_ITERATORS  1
#endif

#endif

#if defined(NCBI_HAVE_CXX11) && !defined(NCBI_TEST_CXX17)
// Avoid (copious) warnings from using auto_ptr in C++ '11.
#  if defined(_GLIBCXX_DEPRECATED_ATTR)
#    include <string>
#    undef _GLIBCXX_DEPRECATED_ATTR
#    define _GLIBCXX_DEPRECATED_ATTR /* temporarily empty */
#    include <backward/auto_ptr.h>
#    undef _GLIBCXX_DEPRECATED_ATTR
#    define _GLIBCXX_DEPRECATED_ATTR NCBI_DEPRECATED
#  elif defined(_GLIBCXX_DEPRECATED)
#    include <string>
#    include <ext/concurrence.h>
#    ifdef _GLIBCXX_THROW_OR_ABORT /* using libstdc++ from GCC 4.8 or later */
#      include <typeinfo>
#      include <bits/alloc_traits.h>
#      include <bits/unique_ptr.h>
#      include <bits/shared_ptr.h>
#    endif
#    undef _GLIBCXX_DEPRECATED
#    define _GLIBCXX_DEPRECATED /* temporarily empty */
#    include <backward/auto_ptr.h>
#    undef _GLIBCXX_DEPRECATED
#    define _GLIBCXX_DEPRECATED NCBI_DEPRECATED
#  elif defined(_LIBCPP_DEPRECATED_IN_CXX11)
#    include <atomic>
#    include <cassert>
#    include <iterator>
#    include <stdexcept>
#    include <tuple>
#    undef _LIBCPP_DEPRECATED_IN_CXX11
#    define _LIBCPP_DEPRECATED_IN_CXX11 /**/
#    include <memory>
#    undef _LIBCPP_DEPRECATED_IN_CXX11
#    define _LIBCPP_DEPRECATED_IN_CXX11 _LIBCPP_DEPRECATED
#  endif
#endif

#if defined(_LIBCPP_VERSION)  &&  defined(__cpp_lib_hardware_interference_size)
#  if _LIBCPP_VERSION < 12000
// Not actually implemented; see https://bugs.llvm.org/show_bug.cgi?id=41423
// (Still unimplemented as of August 2021, version 14[000], but at least
// honest about it!)
#    undef __cpp_lib_hardware_interference_size
#  endif
#endif

#include <string>
BEGIN_NCBI_SCOPE
typedef std::string CStringUTF8;
END_NCBI_SCOPE


/// Helper template to check that type Type have some method declared
/// using TypeChecker<Type>.
/// 
template <template <typename> class TypeChecker, typename Type>
struct cxx_is_supported
{
    // these structs are used to recognize which version
    // of the two functions was chosen during overload resolution
    struct supported {};
    struct not_supported {};

    // this overload of chk will be ignored by SFINAE principle
    // if TypeChecker<Type_> is invalid type
    template <typename Type_>
    static supported chk(typename std::decay<TypeChecker<Type_>>::type *);

    // ellipsis has the lowest conversion rank, so this overload will be
    // chosen during overload resolution only if the template overload above is ignored
    template <typename Type_>
    static not_supported chk(...);

    // if the template overload of chk is chosen during
    // overload resolution then the feature is supported
    // if the ellipses overload is chosen the the feature is not supported
    static constexpr bool value = std::is_same<decltype(chk<Type>(nullptr)), supported>::value;
};



/* @} */

#endif /* NCBISTL__HPP */
