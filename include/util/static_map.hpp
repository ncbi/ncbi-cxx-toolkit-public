#ifndef UTIL___STATIC_MAP__HPP
#define UTIL___STATIC_MAP__HPP

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
 * Authors:  Mike DiCuccio, Eugene Vasilchenko
 *
 * File Description:
 *     CStaticArrayMap<> -- template class to provide convenient access to
 *                          a statically-defined array, while making sure that
 *                          the order of the array meets sort criteria in
 *                          debug builds.
 *
 */


#include <util/static_set.hpp>


BEGIN_NCBI_SCOPE


///
/// class CStaticArrayMap<> is an array adaptor that provides an STLish
/// interface to statically-defined arrays, while making efficient use
/// of the inherent sort order of such arrays.
///
/// This class can be used both to verify sorted order of a static array
/// and to access a static array cleanly.  The template parameters are
/// as follows:
///
///   KeyType    -- type of key object used for access
///   ValueType  -- type of object used for access
///   KeyCompare -- comparison functor.  This must provide an operator(). 
///         This is patterned to accept PCase and PNocase and similar objects.
///
/// To use this class, define your static array as follows:
///
///  static const char* sc_MyArray[] = {
///      "val1",
///      "val2",
///      "val3"
///  };
///
/// Then, declare a static variable such as:
///
///     typedef StaticArraySet<const char*, PNocase_CStr> TStaticArray;
///     static TStaticArray sc_Array(sc_MyArray, sizeof(sc_MyArray));
///
/// In debug mode, the constructor will scan the list of items and insure
/// that they are in the sort order defined by the comparator used.  If the
/// sort order is not correct, then the constructor will ASSERT().
///
/// This can then be accessed as
///
///     if (sc_Array.find(some_value) != sc_Array.end()) {
///         ...
///     }
///
/// or
///
///     size_t idx = sc_Array.index_of(some_value);
///     if (idx != TStaticArray::eNpos) {
///         ...
///     }
///
///


///
/// class CStaticPairArrayMap<> provides access to a static array of pairs
/// in much the same way as CStaticArraySet<>, except that it provides
/// binding of a value type to each sorted key, much like an STL map<> would.
/// Its first template parameter must satisfy STL pair<> requirements:
/// 1. it must define first_type and second_type typedefs,
/// 2. it must have two data members: first and second.
///

template <class KeyType, class ValueType, class KeyCompare = less<KeyType> >
class CStaticPairArrayMap
    : public CStaticArraySearchBase<NStaticArray::PKeyValuePair< SStaticPair<KeyType, ValueType> >, KeyCompare>
{
    typedef CStaticArraySearchBase<NStaticArray::PKeyValuePair< SStaticPair<KeyType, ValueType> >, KeyCompare> TBase;
public:
    typedef typename TBase::value_type value_type;
    typedef typename TBase::const_iterator const_iterator;
    typedef typename TBase::size_type size_type;
    typedef typename TBase::key_compare key_compare;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<size_t Size>
    CStaticPairArrayMap(const value_type (&arr)[Size],
                        const char* file, int line)
        : TBase(arr, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<size_t Size>
    CStaticPairArrayMap(const value_type (&arr)[Size],
                        const key_compare& comp,
                        const char* file, int line)
        : TBase(arr, comp, file, line)
    {
    }

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<class Type>
    CStaticPairArrayMap(const Type* array_ptr, size_t array_size,
                        const char* file, int line)
        : TBase(array_ptr, array_size, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<class Type>
    CStaticPairArrayMap(const Type* array_ptr, size_t array_size,
                        const key_compare& comp,
                        const char* file, int line)
        : TBase(array_ptr, array_size, comp, file, line)
    {
    }

    NCBI_DEPRECATED_CTOR
    (CStaticPairArrayMap(const_iterator obj,
                         size_type array_size));

    NCBI_DEPRECATED_CTOR
    (CStaticPairArrayMap(const_iterator obj,
                         size_type array_size,
                         const key_compare& comp));
};


///
/// class CStaticArrayMap<> provides access to a static array in much the
/// same way as CStaticArraySet<>, except that it provides arbitrary
/// binding of a value type to each sorted key, much like an STL map<> would.
///

template <class KeyType, class ValueType, class KeyCompare = less<KeyType> >
class CStaticArrayMap
    : public CStaticArraySearchBase<NStaticArray::PKeyValuePair< pair<KeyType, ValueType> >, KeyCompare>
{
    typedef CStaticArraySearchBase<NStaticArray::PKeyValuePair< pair<KeyType, ValueType> >, KeyCompare> TBase;
public:
    typedef typename TBase::value_type value_type;
    typedef typename TBase::const_iterator const_iterator;
    typedef typename TBase::size_type size_type;
    typedef typename TBase::key_compare key_compare;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<size_t Size>
    CStaticArrayMap(const value_type (&arr)[Size],
                    const char* file, int line)
        : TBase(arr, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<size_t Size>
    CStaticArrayMap(const value_type (&arr)[Size],
                    const key_compare& comp,
                    const char* file, int line)
        : TBase(arr, comp, file, line)
    {
    }

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    template<class Type>
    CStaticArrayMap(const Type* array_ptr, size_t array_size,
                    const char* file, int line)
        : TBase(array_ptr, array_size, file, line)
    {
    }

    /// Constructor to initialize comparator object.
    template<class Type>
    CStaticArrayMap(const Type* array_ptr, size_t array_size,
                    const key_compare& comp,
                    const char* file, int line)
        : TBase(array_ptr, array_size, comp, file, line)
    {
    }

    NCBI_DEPRECATED_CTOR
    (CStaticArrayMap(const_iterator obj,
                     size_type array_size));

    NCBI_DEPRECATED_CTOR
    (CStaticArrayMap(const_iterator obj,
                     size_type array_size,
                     const key_compare& comp));
};


// Deprecated constructors (defined here to avoid GCC 3.3 parse errors)


template <class KeyType, class ValueType, class KeyCompare>
inline
CStaticPairArrayMap<KeyType, ValueType, KeyCompare>::CStaticPairArrayMap
(const_iterator obj,
 size_type array_size)
    : TBase(obj, array_size, 0, 0)
{
}

template <class KeyType, class ValueType, class KeyCompare>
inline
CStaticPairArrayMap<KeyType, ValueType, KeyCompare>::CStaticPairArrayMap
(const_iterator obj,
 size_type array_size,
 const key_compare& comp)
 : TBase(obj, array_size, comp, 0, 0)
{
}

template <class KeyType, class ValueType, class KeyCompare>
inline
CStaticArrayMap<KeyType, ValueType, KeyCompare>::CStaticArrayMap
(const_iterator obj,
 size_type array_size)
 : TBase(obj, array_size, 0, 0)
{
}

template <class KeyType, class ValueType, class KeyCompare>
inline
CStaticArrayMap<KeyType, ValueType, KeyCompare>::CStaticArrayMap
(const_iterator obj,
 size_type array_size,
 const key_compare& comp)
    : TBase(obj, array_size, comp, 0, 0)
{
}

END_NCBI_SCOPE

#endif  // UTIL___STATIC_MAP__HPP
