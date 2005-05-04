#ifndef UTIL___STATIC_SET__HPP
#define UTIL___STATIC_SET__HPP

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
 *     CStaticArraySet<> -- template class to provide convenient access to
 *                          a statically-defined array, while making sure that
 *                          the order of the array meets sort criteria in
 *                          debug builds.
 *
 */

#include <corelib/ncbistd.hpp>
#include <utility>
#include <algorithm>
#include <functional>


BEGIN_NCBI_SCOPE

///
/// class CStaticArraySet<> is an array adaptor that provides an STLish
/// interface to statically-defined arrays, while making efficient use
/// of the inherent sort order of such arrays.
///
/// This class can be used both to verify sorted order of a static array
/// and to access a static array cleanly.  The template parameters are
/// as follows:
///
///   KeyType    -- type of object used for access
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
template <typename KeyType, typename ValueType, typename ValueCompare>
class CStaticArraySearchBase
{
public:
    enum {
        eNpos = -1
    };

    typedef KeyType             key_type;
    typedef ValueType           value_type;
    typedef ValueCompare        value_compare;
    typedef const value_type&   const_reference;
    typedef const value_type*   const_iterator;
    typedef size_t              size_type;
    typedef ssize_t             difference_type;

    /// Default constructor.  This will build a set around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    CStaticArraySearchBase(const_iterator obj, size_type array_size)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(value_type))
    {
        x_Validate();
    }

    /// Constructor to initialize comparator object.
    CStaticArraySearchBase(const_iterator obj, size_type array_size,
                           const value_compare& comp)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(value_type))
        , m_Compare(comp)
    {
        x_Validate();
    }

    /// Return the start of the controlled sequence.
    const_iterator begin() const
    {
        return m_Begin;
    }

    /// Return the end of the controlled sequence.
    const_iterator end() const
    {
        return m_End;
    }

    /// Return true if the container is empty.
    bool empty() const
    {
        return begin() == end();
    }

    /// Return number of elements in the container.
    size_type size() const
    {
        return end() - begin();
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is less than or equal to the indicated key.
    const_iterator lower_bound(const key_type& key) const
    {
        return std::lower_bound(begin(), end(), key, m_Compare);
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is greater than the indicated key.
    const_iterator upper_bound(const key_type& key) const
    {
        return std::upper_bound(begin(), end(), key, m_Compare);
    }

    /// Return a const_iterator pointing to the specified element, or
    /// to the end if the element is not found.
    const_iterator find(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? end(): iter;
    }

    /// Return the count of the elements in the sequence.  This will be
    /// either 0 or 1, as this structure holds unique keys.
    size_type count(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? 0: 1;
    }

    /// Return a pair of iterators bracketing the given element in
    /// the controlled sequence.
    pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        const_iterator start = lower_bound(key);
        const_iterator iter  = start;
        if ( !x_Bad(key, iter) ) {
            ++iter;
        }
        return make_pair(start, iter);
    }

    /// Return the index of the indicated element, or eNpos if the element is
    /// not found.
    difference_type index_of(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        return x_Bad(key, iter)? eNpos: iter - begin();
    }

    const value_compare& value_comp() const
    {
        return m_Compare;
    }

    virtual const key_type& extract_key(const value_type& value) const = 0;

private:
    const_iterator m_Begin;
    const_iterator m_End;

    value_compare m_Compare;

    bool x_Bad(const key_type& key, const_iterator iter) const
    {
        return iter == end()  ||  m_Compare(key, *iter);
    }

    /// Perform sort-order validation.  This is a no-op in release mode.
    void x_Validate() const
    {
#ifdef _DEBUG
        if ( !empty() ) {
            const_iterator curr = begin(), prev = curr;
            while ( ++curr != end() ) {
                bool in_order = m_Compare(*prev, *curr);
                if ( !in_order ) {
                    ERR_POST("keys out of order: " << extract_key(*prev)
                             << " vs. " << extract_key(*curr));
                }
                _ASSERT( in_order );
                prev = curr;
            }
        }
#endif
    }

};


template <class KeyType, class KeyCompare = less<KeyType> >
class CStaticArraySet
    : public CStaticArraySearchBase<KeyType, KeyType, KeyCompare>
{
    typedef CStaticArraySearchBase<KeyType, KeyType, KeyCompare> TBase;
public:
    typedef typename TBase::key_type        key_type;
    typedef KeyCompare                      key_compare;
    typedef typename TBase::value_type      value_type;
    typedef typename TBase::value_compare   value_compare;
    typedef typename TBase::const_reference const_reference;
    typedef typename TBase::const_iterator  const_iterator;
    typedef typename TBase::size_type       size_type;
    typedef typename TBase::difference_type difference_type;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    CStaticArraySet(const_iterator obj, size_type array_size)
        : TBase(obj, array_size)
    {
    }

    /// Constructor to initialize comparator object.
    CStaticArraySet(const_iterator obj, size_type array_size,
                    const key_compare& comp)
        : TBase(obj, array_size, comp)
    {
    }

    /// Return the key comparison object
    const key_compare& key_comp() const
    {
        return value_compare();
    }

    const key_type& extract_key(const value_type& value) const
    {
        return value;
    }
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/05/04 15:59:46  ucko
 * Suggest PNocase_CStr when using const char*; make validation errors clearer.
 *
 * Revision 1.2  2004/08/19 13:10:09  dicuccio
 * Added include for ncbistd.hpp for standard definitions
 *
 * Revision 1.1  2004/01/23 18:02:23  vasilche
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

#endif  // UTIL___STATIC_SET__HPP
