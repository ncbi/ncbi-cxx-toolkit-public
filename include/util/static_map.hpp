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
 *     CStaticArraySet<> -- template class to provide convenient access to
 *                          a statically-defined array, while making sure that
 *                          the order of the array meets sort criteria in
 *                          debug builds.
 *
 */


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
///   KeyComparator -- comparison functor.  This must provide an operator() as
///                 well as an Equals(const&, const&) function.  This is
///                 patterned to accept PCase and PNocase and similar objects.
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
///     typedef StaticArraySet<const char*, PNocase> TStaticArray;
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
///     size_t idx = sc_Array.FindId(some_value);
///     if (idx != TStaticArray::eNpos) {
///         ...
///     }
///
///
template <class KeyType, class KeyComparator>
class CStaticArraySet
{
public:
    enum {
        eNpos = -1
    };

    typedef KeyType             key_type;
    typedef KeyComparator       key_compare;
    typedef key_type            value_type;
    typedef const value_type&   const_reference;
    typedef const value_type*   const_iterator;
    typedef size_t              size_type;
    typedef ssize_t             difference_type;

    /// Default constructor.  This will build a set around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    CStaticArraySet(const_iterator obj, size_type array_size)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(key_type))
    {
        x_Validate();
    }

    /// Constructor to initialize comparator object.
    CStaticArraySet(const_iterator obj, size_type array_size,
                    KeyComparator comp)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(key_type))
        , m_Comparator(comp)
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

    /// Return a const_iterator pointing to the specified element, or
    /// to the end if the element is not found.
    const_iterator find(const key_type& key) const
    {
        const_iterator iter = lower_bound(key);
        if (iter == m_End  ||  !m_Comparator.Equals(*iter, key)) {
            return m_End;
        }
        return iter;
    }

    /// Return the count of the elements in the sequence.  This will be
    /// either 0 or 1, as this structure holds unique keys.
    size_type count(const key_type& key) const
    {
        return (size_type)binary_search(m_Begin, m_End, key, m_Comparator);
    }

    /// Return a pair of iterators bracketing the given element in
    /// the controlled sequence.
    pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        const_iterator iter = find(key);
        const_iterator end  = iter;
        if (end != m_End) {
            ++end;
        }
        return make_pair(iter, end);
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is less than or equal to the indicated key.
    const_iterator lower_bound(const key_type& key)
    {
        return lower_bound(m_Begin, m_End, key, m_Comparator);
    }

    /// Return an iterator into the sequence such that the iterator's key
    /// is greater than the indicated key.
    const_iterator upper_bound(const key_type& key)
    {
        return upper_bound(m_Begin, m_End, key, m_Comparator);
    }

    /// Return the index of the indicated element, or eNpos if the element is
    /// not found.
    difference_type index_of(const key_type& key) const
    {
        const_iterator iter = find(key);
        if (iter == m_End) {
            return eNpos;
        }
        return (iter - m_Begin);
    }

private:
    const_iterator m_Begin;
    const_iterator m_End;

    key_compare m_Comparator;

    /// Perform sort-order validation.  This is a no-op in release mode.
    void x_Validate()
    {
#ifdef _DEBUG
        const_iterator start = begin();
        const_iterator prev = start;
        ++begin;
        for ( ;  begin != end();  ++begin) {
            _ASSERT( m_Comparator(*prev, *begin) );
            prev = begin;
        }
#endif
    }

};


///
/// class CStaticArrayMap<> provides access to a static array in much the
/// same way as CStaticArraySet<>, except that it provides arbitrary
/// binding of a value type to each sorted key, much like an STL map<> would.
///

template <class KeyType, class ValueType, class KeyComparator>
class CStaticArrayMap
{
public:
    enum {
        eNpos = -1
    };

    typedef KeyType                     key_type;
    typedef ValueType                   mapped_type;
    typedef pair<key_type, mapped_type> value_type;
    typedef const value_type&           const_reference;
    typedef const value_type*           const_iterator;
    typedef size_t                      size_type;
    typedef ssize_t                     difference_type;

    struct SPairComparator
    {
        KeyComparator comp;
        SPairComparator()
        {
        }

        SPairComparator(KeyComparator c)
            : comp(c)
        {
        }

        bool Equals(const value_type& v0, const value_type& v1) const
        {
            return comp.Equals(v0.first, v1.first);
        }

        bool operator()(const value_type& v0, const value_type& v1) const
        {
            return comp(v0.first, v1.first);
        }
    };
    typedef SPairComparator               key_compare;

    /// default constructor.  This will build a map around a given array; the
    /// storage of the end pointer is based on the supplied array size.  In
    /// debug mode, this will verify that the array is sorted.
    CStaticArrayMap(const_iterator obj, size_type array_size)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(value_type))
    {
        x_Validate();
    }

    /// Constructor to initialize comparator object.
    CStaticArrayMap(const_iterator obj, size_type array_size,
                    KeyComparator comp)
        : m_Begin(obj)
        , m_End(obj + array_size / sizeof(value_type))
        , m_Comparator(comp)
    {
        x_Validate();
    }

    /// Return an iterator to the beginning of the controlled sequence.
    const_iterator begin() const
    {
        return m_Begin;
    }

    /// Return an iterator to the end of the controlled sequence.
    const_iterator end() const
    {
        return m_End;
    }

    /// Find an element in the controlled sequence, returning end() if
    /// the element is not found.
    const_iterator find(const key_type& key) const
    {
        value_type val(key, mapped_type());
        const_iterator iter =
            std::lower_bound(m_Begin, m_End, val, m_Comparator);
        if (iter == m_End  ||  !m_Comparator.Equals(*iter, val)) {
            return m_End;
        }
        return iter;
    }

    /// Return the count of items in the sequence.  This will return 0 or
    /// 1 since this container does not manage duplicate keys.
    size_type count(const key_type& key) const
    {
        value_type val(key, mapped_type());
        return (size_type)binary_search(m_Begin, m_End, val, m_Comparator);
    }

    /// Return a pair of iterators bracketing the range of the key.
    pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        const_iterator iter = find(key);
        const_iterator end  = iter;
        if (end != m_End) {
            ++end;
        }
        return make_pair(iter, end);
    }

    /// Return an iterator that points to the element whose key is less than
    /// or equal to the indicated key.
    const_iterator lower_bound(const key_type& key) const
    {
        value_type val(key, mapped_type());
        return std::lower_bound(m_Begin, m_End, val, m_Comparator);
    }

    /// Return an iterator that points to the element whose key is greater than
    /// the indicated key.
    const_iterator upper_bound(const key_type& key) const
    {
        value_type val(key, mapped_type());
        return std::upper_bound(m_Begin, m_End, val, m_Comparator);
    }

    /// Return the index of the indicated element, or eNpos if the element is
    /// not found.
    difference_type index_of(const key_type& key) const
    {
        const_iterator iter = find(key);
        if (iter == m_End) {
            return eNpos;
        }
        return (iter - m_Begin);
    }

private:
    const_iterator m_Begin;
    const_iterator m_End;
    key_compare    m_Comparator;

    void x_Validate()
    {
#ifdef _DEBUG
        const_iterator begin = m_Begin;
        const_iterator prev = begin;
        ++begin;
        for ( ;  begin != m_End;  ++begin) {
            _ASSERT( m_Comparator(*prev, *begin) );
            prev = begin;
        }
#endif
    }

};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/22 13:22:12  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // UTIL___STATIC_MAP__HPP
