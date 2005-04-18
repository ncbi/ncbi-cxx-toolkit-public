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
 * Author: Aleksey Grichenko
 *
 * File Description:
 *   Wrapper for STLport internal sources
 *
 */

/*
_STLP_USE_WRAPPER_FOR_ALLOC_PARAM
    includes _hash_map_wrapper.h and _hash_set_wrapper.h
    left defined to ignore the files

_STLP_NO_SIGNED_BUILTINS
    if defined, removes declaration of hash<signed char>

_STLP_CLASS_PARTIAL_SPECIALIZATION
    forces insert_iterator definition in _hash_map.h and _hash_set.h,
    causing problems with namespaces (spec. in different namespace).

_STLP_MEMBER_TEMPLATES
    left undefined

_STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM
    used with _Alloc_traits, allocate, deallocate.
    -- defined for GCC, ICC

_STLP_STATIC_TEMPLATE_DATA
    always defined as 1 for correct initialization of static template members

_STLP_MULTI_CONST_TEMPLATE_ARG_BUG
    switch between __Select1st_hint and _Select1st
    -- need for WS, ICC, MSVC

Additional flags for some compilers
NO_STD_CONSTRUCT
NO_STD_DESTROY
NO_STD_IDENTITY
    generate some functions if they are not defined in std library

USE_NO_ALLOC_TRAITS
    some compilers already have _Alloc_traits -- do not redefine
*/

#ifndef STLP_DEFS__HPP
#define STLP_DEFS__HPP

#include <corelib/ncbistl.hpp>

// Use STL types
#include <iterator>
#include <algorithm>
#include <functional>

NCBI_USING_NAMESPACE_STD;


//////////////////////////////////////////////////////////////////////
//
// Define symbols used by STLport
//

#define _STLP_STATIC_TEMPLATE_DATA 1

// Do not try to include *.c from *.h - this is done
// explicitely in this header
#define _STLP_LINK_TIME_INSTANTIATION
#define _STLP_USE_EXCEPTIONS

// Need to undefine to avoid namespace conflicts
#define _STLP_NO_CLASS_PARTIAL_SPECIALIZATION
#undef _STLP_CLASS_PARTIAL_SPECIALIZATION


// Generate correct operator!=() for iterators
#define _STLP_USE_SEPARATE_RELOPS_NAMESPACE


// Ignore STLport internal headers
#define _STLP_INTERNAL_VECTOR_H
#define _STLP_INTERNAL_ITERATOR_H
#define _STLP_INTERNAL_FUNCTION_H
#define _STLP_INTERNAL_ALGOBASE_H

// long type name for hash<_STLP_LONG_LONG>
#ifdef SIZEOF_LONG_LONG
#  if SIZEOF_LONG_LONG > 0
#    define _STLP_LONG_LONG long long
#  endif
#endif


#define _STLP_BEGIN_NAMESPACE             BEGIN_NCBI_SCOPE
#define _STLP_END_NAMESPACE               END_NCBI_SCOPE
#define _STLP_STD                         std
#define _STLP_CALL
#define _STLP_FIX_LITERAL_BUG(x)
#define __WORKAROUND_RENAME(X)            X
#define _STLP_PLACEMENT_NEW               new
#define __STATIC_CAST(__x,__y)            static_cast<__x>(__y)
#define __REINTERPRET_CAST(__x,__y)       reinterpret_cast<__x>(__y)
#define _STLP_TEMPLATE_NULL               EMPTY_TEMPLATE
#define _STLP_TEMPLATE                    template
#define _STLP_TYPENAME_ON_RETURN_TYPE     typename
#define __vector__                        vector
#define _STLP_FORCE_ALLOCATORS(a,y)
#define _STLP_CONVERT_ALLOCATOR(__a, _Tp) __a


#define _STLP_TRY                     try
#define _STLP_UNWIND(action)          catch(...) { action; throw; }


#define __TRIVIAL_CONSTRUCTOR(__type)
#define __TRIVIAL_DESTRUCTOR(__type)
#define __TRIVIAL_STUFF(__type)  \
  __TRIVIAL_CONSTRUCTOR(__type) __TRIVIAL_DESTRUCTOR(__type)


#define _STLP_DEFINE_ARROW_OPERATOR \
    pointer operator->() const { return &(operator*()); }


#define __DFL_TMPL_PARAM( classname, defval ) class classname = defval
#define _STLP_DEFAULT_PAIR_ALLOCATOR_SELECT(_Key, _Tp ) \
        class _Alloc = allocator< pair < _Key, _Tp > >


#define _STLP_DEFAULT_ALLOCATOR_SELECT( _Tp ) \
        __DFL_TMPL_PARAM(_Alloc, allocator< _Tp >)
#define _STLP_DEFAULT_PAIR_ALLOCATOR_SELECT(_Key, _Tp ) \
        class _Alloc = allocator< pair < _Key, _Tp > >


// Ignore distance in __lower_bound
#define __lower_bound(b,e,v,c,d) lower_bound(b,e,v,c)


//////////////////////////////////////////////////////////////////////
//
// Configuration dependent defines
//

#ifdef NCBI_COMPILER_GCC
#  define _STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM 1
#  if NCBI_COMPILER_VERSION < 340
#    include <memory>
#    define USE_NO_ALLOC_TRAITS
#  endif
#  if NCBI_COMPILER_VERSION < 304
#    define NO_STD_CONSTRUCT
#    define NO_STD_DESTROY
#  endif
#endif

#ifdef NCBI_COMPILER_ICC
#  define NO_STD_IDENTITY
#  define _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
#  define _STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM 1
#endif

#ifdef NCBI_COMPILER_MSVC
#  define NO_STD_IDENTITY
#  define _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
#endif

#ifdef NCBI_COMPILER_WORKSHOP
#  define NO_STD_CONSTRUCT
#  define NO_STD_DESTROY
#  define NO_STD_IDENTITY
#  define _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
#endif

#ifdef NCBI_COMPILER_COMPAQ
#  define NO_STD_CONSTRUCT
#  define NO_STD_DESTROY
#  define NO_STD_IDENTITY
#  define _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
#endif

#ifdef NCBI_COMPILER_MIPSPRO
#  define NO_STD_CONSTRUCT
#  define NO_STD_DESTROY
// replace cstddef with stddef.h
#  define _STLP_CSTDDEF
#  include <stddef.h>
#  define USE_NO_ALLOC_TRAITS
#endif

#ifdef NCBI_COMPILER_METROWERKS
#  define NO_STD_CONSTRUCT
#  define NO_STD_DESTROY
#  define NO_STD_IDENTITY
#  define _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
#endif

#ifdef _DEBUG
#  define _STLP_DEBUG_UNINITIALIZED
// uninitialized value filler
// This value is designed to cause problems if an error occurs
#  define _STLP_SHRED_BYTE 0xA3
#endif


// Defined for WS, ICC
#ifdef _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
// fbp : sort of select1st just for maps
template <class _Pair, class _Whatever>		
// JDJ (CW Pro1 doesn't like const when first_type is also const)
struct __Select1st_hint : public unary_function<_Pair, _Whatever> {
    const _Whatever& operator () (const _Pair& __x) const { return __x.first; }
};
#  define  _STLP_SELECT1ST(__x,__y) __Select1st_hint< __x, __y >
#else
#  define  _STLP_SELECT1ST(__x, __y) _Select1st< __x >
#endif


// Use better string hash function (by Eugene Vasilchenko)
#ifndef NCBI_USE_STRING_HASH_FUNC__STLP
#  define NCBI_USE_STRING_HASH_FUNC__NCBI
#endif


//////////////////////////////////////////////////////////////////////
//
// Functions and classes required by STLport
//

template <class _Tp>
less<_Tp> __less(_Tp* ) { return less<_Tp>(); }


#ifdef NO_STD_CONSTRUCT

template <class _T1, class _T2>
inline void _Construct(_T1* __p, const _T2& __val) {
#  ifdef _STLP_DEBUG_UNINITIALIZED
    memset((char*)__p, _STLP_SHRED_BYTE, sizeof(_T1));
#  endif
    _STLP_PLACEMENT_NEW (__p) _T1(__val);
}

template <class _T1>
inline void _Construct(_T1* __p) {
#  ifdef _STLP_DEBUG_UNINITIALIZED
    memset((char*)__p, _STLP_SHRED_BYTE, sizeof(_T1));
#  endif /* _STLP_DEBUG_UNINITIALIZED */
#  ifdef _STLP_DEFAULT_CONSTRUCTOR_BUG
    typedef typename _Is_integer<_T1>::_Integral _Is_Integral;
    _Construct_aux (__p, _Is_Integral() );
#  else
    _STLP_PLACEMENT_NEW (__p) _T1();
#  endif /* if_STLP_DEFAULT_CONSTRUCTOR_BUG */
}

#endif /* NO_STD_CONSTRUCT */


#ifdef NO_STD_DESTROY

// _Destroy needs to be defined in std
namespace std {

template <class _Tp>
#ifdef _STLP_DEBUG_UNINITIALIZED
inline void _Destroy(_Tp* __pointer) {
    memset((char*)__pointer, _STLP_SHRED_BYTE, sizeof(_Tp));
}
#else
inline void _Destroy(_Tp* /*__pointer */) {
}
#endif /* if_STLP_DEBUG_UNINITIALIZED */

} /* namespace std */

#endif /* NO_STD_DESTROY */


#ifdef NO_STD_IDENTITY

template <class _Tp>
struct _Identity : public unary_function<_Tp,_Tp> {
    const _Tp& operator()(const _Tp& __x) const { return __x; }
};

#endif /* NO_STD_IDENTITY */


// fbp: those are being used for iterator/const_iterator definitions everywhere
template <class _Tp>
struct _Nonconst_traits;

template <class _Tp>
struct _Const_traits {
  typedef _Tp value_type;
  typedef const _Tp&  reference;
  typedef const _Tp*  pointer;
  typedef _Nonconst_traits<_Tp> _Non_const_traits;
};

template <class _Tp>
struct _Nonconst_traits {
  typedef _Tp value_type;
  typedef _Tp& reference;
  typedef _Tp* pointer;
  typedef _Nonconst_traits<_Tp> _Non_const_traits;
};


#ifndef USE_NO_ALLOC_TRAITS
// The fully general version.
template <class _Tp, class _Allocator>
struct _Alloc_traits
{
    typedef _Allocator _Orig;
#  if defined (_STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM)
    typedef typename _Allocator::_STLP_TEMPLATE rebind<_Tp> _Rebind_type;
    typedef typename _Rebind_type::other  allocator_type;
    static allocator_type create_allocator(const _Orig& __a)
    { return allocator_type(__a); }
#  else
    // this is not actually true, used only to pass this type through
    // to dynamic overload selection in _STLP_alloc_proxy methods
    typedef _Allocator allocator_type;
#  endif /* _STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM */
};
#endif /* USE_NO_ALLOC_TRAITS */

#if defined (_STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM) 
template <class _Tp, class _Alloc>
inline _STLP_TYPENAME_ON_RETURN_TYPE
_Alloc_traits<_Tp, _Alloc>::allocator_type  _STLP_CALL
__stl_alloc_create(const _Alloc& __a, const _Tp*) {
    typedef typename _Alloc::_STLP_TEMPLATE rebind<_Tp>::other _Rebound_type;
    return _Rebound_type(__a);
}
#else
#  if defined(_RWSTD_VER) && !defined(_RWSTD_ALLOCATOR)
// BW_1: non standard Sun's allocators
#    define ALLOCATOR(T) allocator_interface<allocator<T>, T >
#  else
#    define ALLOCATOR(T) allocator<T>
#  endif

// If custom allocators are being used without member template classes support:
// user (on purpose) is forced to define rebind/get operations !!!
template <class _Tp1, class _Tp2>
inline ALLOCATOR(_Tp2)& _STLP_CALL
__stl_alloc_rebind(allocator<_Tp1>& __a, const _Tp2*)
{  return (ALLOCATOR(_Tp2)&)(__a); }
template <class _Tp1, class _Tp2>
inline ALLOCATOR(_Tp2) _STLP_CALL
__stl_alloc_create(const allocator<_Tp1>&, const _Tp2*)
{ return ALLOCATOR(_Tp2)(); }
#endif /* _STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM */


// inheritance is being used for EBO optimization
template <class _Value, class _Tp, class _MaybeReboundAlloc>
class _STLP_alloc_proxy : public _MaybeReboundAlloc {
private:
    typedef _MaybeReboundAlloc _Base;
    typedef _STLP_alloc_proxy<_Value, _Tp, _MaybeReboundAlloc> _Self;
public:
    _Value _M_data;
    inline _STLP_alloc_proxy(const _MaybeReboundAlloc& __a, _Value __p)
        : _MaybeReboundAlloc(__a), _M_data(__p) {}

    // Unified interface to perform allocate()/deallocate() with limited
    // language support
#if ! defined (_STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM)
    // else it is rebound already, and allocate() member is accessible
    inline _Tp* allocate(size_t __n) { 
        return __stl_alloc_rebind(__STATIC_CAST(_Base&,*this),
                                  (_Tp*)0).allocate(__n,0); 
    }
    inline void deallocate(_Tp* __p, size_t __n) { 
        __stl_alloc_rebind(__STATIC_CAST(_Base&, *this),
                           (_Tp*)0).deallocate(__p, __n); 
    }
#endif /* !_STLP_USE_NESTED_TCLASS_THROUGHT_TPARAM */
};


//////////////////////////////////////////////////////////////////////
//
// Include STLport headers
//

#include <corelib/hash_impl/_hash_fun.h>
#include <corelib/hash_impl/_hashtable.h>
#include <corelib/hash_impl/_hashtable.c>

#undef __lower_bound


BEGIN_NCBI_SCOPE

// String hash functions, see also NCBI_USE_STRING_HASH_FUNC__NCBI

inline size_t __stl_hash_string(const char* __s, size_t __l)
{
#ifdef NCBI_USE_STRING_HASH_FUNC__NCBI
    unsigned long __h = __l;
    for ( ; __l; ++__s, --__l)
        __h = __h*17 + *__s;
#else
    unsigned long __h = 0; 
    for ( ; __l; ++__s, --__l)
        __h = 5*__h + *__s;
#endif
    return size_t(__h);
}

template<> struct hash<const string>
{
    size_t operator()(const string& s) const
    {
        return __stl_hash_string(s.data(), s.size());
    }
};

template<> struct hash<string>
{
    size_t operator()(const string& s) const
    {
        return __stl_hash_string(s.data(), s.size());
    }
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/04/18 13:43:46  rsmith
 * Codewarrior + _STLP_MULTI_CONST_TEMPLATE_ARG_BUG
 *
 * Revision 1.7  2005/03/28 20:59:05  rsmith
 * add defines for Metrowerks/Codewarrior compiler.
 *
 * Revision 1.6  2005/02/09 21:29:14  grichenk
 * Fixed GCC 2.9.5 options
 *
 * Revision 1.5  2005/02/09 19:11:41  grichenk
 * Added CVS log
 *
 *
 * ===========================================================================
 */

#endif /* STLP_DEFS__HPP */
