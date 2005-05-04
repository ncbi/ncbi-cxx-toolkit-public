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
 * Authors:  Mike DiCuccio
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

template<class ValueType,
         class FirstCompare = less<typename ValueType::first_type> >
class PLessByFirst
{
public:
    typedef ValueType                       value_type;
    typedef typename value_type::first_type first_type;
    typedef FirstCompare                    first_compare;

    PLessByFirst()
    {
    }

    PLessByFirst(const first_compare& comp)
        : m_FirstComp(comp)
    {
    }

    bool operator()(const value_type& v0, const value_type& v1) const
    {
        return m_FirstComp(v0.first, v1.first);
    }

    bool operator()(const first_type& v0, const value_type& v1) const
    {
        return m_FirstComp(v0, v1.first);
    }

    bool operator()(const value_type& v0, const first_type& v1) const
    {
        return m_FirstComp(v0.first, v1);
    }

    const first_compare& first_comp() const
    {
        return m_FirstComp;
    }

private:
    first_compare m_FirstComp;
};


///
/// class CStaticArrayMap<> provides access to a static array in much the
/// same way as CStaticArraySet<>, except that it provides arbitrary
/// binding of a value type to each sorted key, much like an STL map<> would.
///

template <class KeyType, class ValueType, class KeyCompare = less<KeyType> >
class CStaticArrayMap
    : public CStaticArraySearchBase< KeyType, pair<KeyType, ValueType>,
                                     PLessByFirst< pair<KeyType, ValueType>,
                                                    KeyCompare > >
{
    typedef CStaticArraySearchBase< KeyType, pair<KeyType, ValueType>,
                                    PLessByFirst< pair<KeyType, ValueType>,
                                                  KeyCompare > > TBase;
public:
    typedef typename TBase::key_type        key_type;
    typedef KeyCompare                      key_compare;
    typedef ValueType                       mapped_type;
    typedef typename TBase::value_type      value_type;
    typedef typename TBase::value_compare   value_compare;
    typedef typename TBase::const_reference const_reference;
    typedef typename TBase::const_iterator  const_iterator;
    typedef typename TBase::size_type       size_type;
    typedef typename TBase::difference_type difference_type;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    CStaticArrayMap(const_iterator obj, size_type array_size)
        : TBase(obj, array_size)
    {
    }

    /// Constructor to initialize comparator object.
    CStaticArrayMap(const_iterator obj, size_type array_size,
                    const key_compare& comp)
        : TBase(obj, array_size, comp)
    {
    }


    /// Return the key comparison object
    const key_compare& key_comp() const
    {
        return this->value_comp().first_comp();
    }

    const key_type& extract_key(const value_type& value) const
    {
        return value.first;
    }
};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/05/04 15:59:46  ucko
 * Suggest PNocase_CStr when using const char*; make validation errors clearer.
 *
 * Revision 1.5  2004/08/19 13:09:48  dicuccio
 * Removed redundant inlcude of <utility>
 *
 * Revision 1.4  2004/04/26 14:52:14  ucko
 * Add "this->" as needed to accommodate GCC 3.4's stricter treatment of
 * templates.
 *
 * Revision 1.3  2004/01/23 18:02:23  vasilche
 * Cleaned implementation of CStaticArraySet & CStaticArrayMap.
 * Added test utility test_staticmap.
 *
 * Revision 1.2  2004/01/22 14:51:03  dicuccio
 * Fixed erroneous variable names
 *
 * Revision 1.1  2004/01/22 13:22:12  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // UTIL___STATIC_MAP__HPP
