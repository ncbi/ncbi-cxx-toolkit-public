#ifndef CORELIB___NCBI_UTILITY__HPP
#define CORELIB___NCBI_UTILITY__HPP

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
 * Author: 
 *   Eugene Vasilchenko
 *
 *
 */

/// @file ncbiutil.hpp
/// Useful/utility classes and methods.


#include <corelib/ncbistd.hpp>
#include <memory>
#include <map>



BEGIN_NCBI_SCOPE

/** @addtogroup Utility Template Utilities
 *
 * @{
 */

//-------------------------------------------
// Utilities

/// Check for equality of objects pointed to by pointer.
template <class T>
struct p_equal_to
{
    bool operator() (const T* x, const T* y) const {
        return *x == *y;
    }
};

/// Compare objects pointed to by (smart) pointer.
template <class T>
struct PPtrLess
{
    bool operator() (const T& x, const T& y) const {
        return *x < *y;
    }
};

/// Check whether a pair's second element matches a given value.
template <class Pair>
struct pair_equal_to
{
    bool operator() (const Pair& x,
                     const typename Pair::second_type& y) const {
        return x.second == y;
    }
};

/// Check for not null value (after C malloc, strdup etc.).
template<class X>
inline X* NotNull(X* object)
{
    if ( !object ) {
        NCBI_THROW(CCoreException,eNullPtr,kEmptyStr);
    }
    return object;
}

/// Get map element (pointer) or NULL if absent.
template<class Key, class Element>
inline Element GetMapElement(const map<Key, Element>& m, const Key& key,
                             const Element& def = 0)
{
    typename map<Key, Element>::const_iterator ptr = m.find(key);
    if ( ptr !=m.end() )
        return ptr->second;
    return def;
}

/// Set map element -- if data is null, erase the existing key.
template<class Key, class Element>
inline void SetMapElement(map<Key, Element*>& m, const Key& key, Element* data)
{
    if ( data )
        m[key] = data;
    else
        m.erase(key);
}

/// Insert map element.
template<class Key, class Element>
inline bool InsertMapElement(map<Key, Element*>& m,
                             const Key& key, Element* data)
{
    return m.insert(map<Key, Element*>::value_type(key, data)).second;
}

/// Get string map element or "" if absent.
template<class Key>
inline string GetMapString(const map<Key, string>& m, const Key& key)
{
    typename map<Key, string>::const_iterator ptr = m.find(key);
    if ( ptr != m.end() )
        return ptr->second;
    return string();
}

/// Set string map element -- if data is null erase the existing key.
template<class Key>
inline void SetMapString(map<Key, string>& m,
                         const Key& key, const string& data)
{
    if ( !data.empty() )
        m[key] = data;
    else
        m.erase(key);
}

/// Delete all elements from a container of pointers (list, vector, set,
/// multiset); clear the container afterwards.
template<class Cnt>
inline void DeleteElements( Cnt& cnt )
{
    for ( typename Cnt::iterator i = cnt.begin(); i != cnt.end(); ++i ) {
        delete *i;
    }
    cnt.clear();
}

/// Delete all elements from map containing pointers; clear container
/// afterwards.
template<class Key, class Element>
inline void DeleteElements(map<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin();  i != m.end();
          ++i ) {
        delete i->second;
    }
    m.clear();
}

/// Delete all elements from multimap containing pointers; clear container
/// afterwards.
template<class Key, class Element>
inline void DeleteElements(multimap<Key, Element*>& m)
{
    for ( typename map<Key, Element*>::iterator i = m.begin();  i != m.end();
          ++i ) {
        delete i->second;
    }
    m.clear();
}

/// Retrieve the result from the result cache - if cache is empty,
/// insert into cache from the supplied source.
template<class Result, class Source, class ToKey>
inline
Result&
AutoMap(unique_ptr<Result>& cache, const Source& source, const ToKey& toKey)
{
    Result* ret = cache.get();
    if ( !ret ) {
        cache.reset(ret = new Result);
        for ( typename Source::const_iterator i = source.begin();
              i != source.end();
              ++i ) {
            ret->insert(Result::value_type(toKey.GetKey(*i), *i));
        }
    }
    return *ret;
}

/// Get name attribute for Value object.
template<class Value>
struct CNameGetter
{
    const string& GetKey(const Value* value) const
    {
        return value->GetName();
    }
};

// These templates may yield 0 if no (suitable) elements exist.  Also,
// in part for consistency with the C Toolkit, lower scores are
// better, and earlier elements win ties.



/// Tracks the best score (lowest value).
///
/// Values are specified by template parameter T, and scoring function by
/// template parameter F.
template <typename T, typename F>
class CBestChoiceTracker
{
public:
    /// Constructor.
    CBestChoiceTracker(F func) : m_Func(func), m_Value(T()), m_Score(kMax_Int)
    { }

    /// Define application operator.
    void operator() (const T& x)
    {
        int score = m_Func(x);
        if (score < m_Score) {
            m_Value = x;
            m_Score = score;
        }
    }

    /// Get best choice with lowest score.
    const T& GetBestChoice() { return m_Value; }

private:
    F   m_Func;         ///< Scoring function
    T   m_Value;        ///< Current best value 
    int m_Score;        ///< Current best score
};

/// Find the best choice (lowest score) for values in a container.
///
/// Container and scoring functions are specified as template parameters.
template <typename C, typename F>
inline
typename C::value_type
FindBestChoice(const C& container, F score_func)
{
    typedef typename C::value_type T;
    CBestChoiceTracker<T, F> tracker(score_func);
    ITERATE (typename C, it, container) {
        tracker(*it);
    }
    return tracker.GetBestChoice();
}


END_NCBI_SCOPE

#if !defined(HAVE_IS_SORTED)

///
/// is_sorted is provided by some implementations of the STL and may
/// be included in future releases of all standard-conforming implementations
/// This is provided here for future compatibility
///

BEGIN_STD_SCOPE

template <class Iterator>
bool is_sorted(Iterator iter1, Iterator iter2)
{
    Iterator prev = iter1;
    for (++iter1;  iter1 != iter2;  ++iter1, ++prev) {
        if (*iter1 < *prev) {
            return false;
        }
    }
    return true;
}


template <class Iterator, class Predicate>
bool is_sorted(Iterator iter1, Iterator iter2, Predicate pred)
{
    Iterator prev = iter1;
    for (++iter1;  iter1 != iter2;  ++iter1, ++prev) {
        if (pred(*iter1, *prev)) {
            return false;
        }
    }
    return true;
}


END_STD_SCOPE

#endif // !defined(HAVE_IS_SORTED)



/* @} */

#endif /* NCBI_UTILITY__HPP */
