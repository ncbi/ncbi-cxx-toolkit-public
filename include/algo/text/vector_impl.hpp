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
 *
 */

#include "vector_score.hpp"
#include <algorithm>
#include <math.h>
#include <set>


#ifndef ALGO_TEXT___VECTOR__HPP
#  error  This file should never be included directly
#endif

BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////

template <class T>
inline T InitialValue(T* t)
{
    return 0;
}

template<>
inline string InitialValue<string>(string* t)
{
    return kEmptyStr;
}


/////////////////////////////////////////////////////////////////////////////

template<class T, class U>
struct SSortByFirst : public binary_function< pair<T,U>,  pair<T,U>, bool>
{
    bool operator()(const pair<T,U>& it1, const pair<T,U>& it2) const
    {
        if (it1.first < it2.first) {
            return true;
        } else if (it2.first < it1.first) {
            return false;
        } else {
            return it2.second < it1.second;
        }
    }
};


template<class T, class U>
struct SSortBySecond : public binary_function<pair<T,U>, pair<T,U>, bool>
{
    bool operator()(const pair<T,U>& it1, const pair<T,U>& it2) const
    {
        if (it2.second < it1.second) {
            return true;
        } else if (it1.second < it2.second) {
            return false;
        } else {
            return it1.first < it1.first;
        }
    }
};



/////////////////////////////////////////////////////////////////////////////


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>::CRawScoreVector()
{
    m_Uid = InitialValue(&m_Uid);
}


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>::CRawScoreVector(const CScoreVector<Key, Score>& other)
{
    *this = other;
}


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>::CRawScoreVector(const CRawScoreVector<Key, Score>& other)
{
    *this = other;
}


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>& CRawScoreVector<Key, Score>::operator=(const CRawScoreVector<Key, Score>& other)
{
    if (this != &other) {
        m_Data = other.m_Data;
        m_Uid  = other.m_Uid;
    }

    return *this;
}

template <class Key, class Score>
inline
CRawScoreVector<Key, Score>& CRawScoreVector<Key, Score>::operator=(const CScoreVector<Key, Score>& vec)
{
    m_Data.clear();
    m_Data.reserve(vec.size());

    typedef CScoreVector<Key, Score> TOtherVector;
    ITERATE (typename TOtherVector, iter, vec) {
        m_Data.push_back(typename TVector::value_type(iter->first, iter->second));
    }

    m_Uid = vec.GetId();

    return *this;
}



template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::Swap(CRawScoreVector<Key, Score>& other)
{
    if (this != &other) {
        m_Data.swap(other.m_Data);
        swap(m_Uid, other.m_Uid);
    }
}


template <class Key, class Score>
inline
Key CRawScoreVector<Key, Score>::GetId() const
{
    return m_Uid;
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::SetId(Key uid)
{
    m_Uid = uid;
}


template <class Key, class Score>
inline
size_t CRawScoreVector<Key, Score>::size() const
{
    return m_Data.size();
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::reserve(size_t size)
{
    m_Data.reserve(size);
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::iterator
CRawScoreVector<Key, Score>::begin()
{
    return m_Data.begin();
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::iterator
CRawScoreVector<Key, Score>::end()
{
    return m_Data.end();
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::const_iterator
CRawScoreVector<Key, Score>::begin() const
{
    return m_Data.begin();
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::const_iterator
CRawScoreVector<Key, Score>::end() const
{
    return m_Data.end();
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::const_iterator
CRawScoreVector<Key, Score>::find(const Key& key) const
{
    value_type v(key, 0);
    return std::find(m_Data.begin(), m_Data.end(), v);
}


template <class Key, class Score>
inline
typename CRawScoreVector<Key, Score>::iterator
CRawScoreVector<Key, Score>::find(const Key& key)
{
    value_type v(key, 0);
    return std::find(m_Data.begin(), m_Data.end(), v);
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::erase(iterator it)
{
    m_Data.erase(it);
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::clear()
{
    m_Data.clear();
    m_Uid = InitialValue(&m_Uid);
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::insert(const value_type& val)
{
    TIdxScore p(val.first, 0);
    iterator iter = lower_bound(m_Data.begin(), m_Data.end(), p,
                                SSortByFirst<Key, Score>());
    if (iter == m_Data.end()  ||  iter->first != val.first) {
        m_Data.insert(iter, val);
    } else {
        iter->second = val.second;
    }
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::insert(iterator ins_before,
                                             const_iterator start,
                                             const_iterator stop)
{
    /// verify that we don't repeat any IDs
#ifdef _DEBUG
    for (const_iterator it = start;  it != stop;  ++it) {
        const_iterator i = lower_bound(m_Data.begin(), m_Data.end(), *it,
                                       SSortByFirst<Key, Score>());
        _ASSERT(i == m_Data.end()  ||  i->first != it->first);
    }
#endif
    m_Data.insert(ins_before, start, stop);
}


template <class Key, class Score>
inline
bool CRawScoreVector<Key, Score>::empty() const
{
    return m_Data.empty();
}


template <class Key, class Score>
inline
Score CRawScoreVector<Key, Score>::Get(Key idx) const
{
    TIdxScore p(idx, 0);
    const_iterator iter = lower_bound(m_Data.begin(), m_Data.end(), p,
                                      SSortByFirst<Key, Score>());
    if (iter == m_Data.end()  ||  iter->first != idx) {
        return 0;
    } else {
        return iter->second;
    }
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::Set(Key idx, Score weight)
{
    insert(value_type(idx, weight));
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::Set(const_iterator begin, const_iterator end)
{
    size_t diff = end - begin;
    m_Data.reserve(m_Data.size() + diff);
    size_t orig_size = m_Data.size();

    bool need_sort = false;
    for ( ;  begin != end;  ++begin) {
        iterator iter =
            lower_bound(m_Data.begin(), m_Data.begin() + orig_size,
                        *begin, SSortByFirst<Key, Score>());
        if (iter == m_Data.end()  ||  iter->first != begin->first) {
            m_Data.push_back(*begin);
            need_sort = true;
        } else {
            iter->second = begin->second;
        }
    }

    if (need_sort) {
        if (is_sorted(m_Data.begin() + orig_size, m_Data.end())) {
            std::inplace_merge(m_Data.begin(),
                               m_Data.begin() + orig_size,
                               m_Data.end(),
                               SSortByFirst<Key, Score>());
        } else {
            std::sort(m_Data.begin(), m_Data.end(),
                      SSortByFirst<Key, Score>());
        }
    }
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::Add(Key idx, Score weight)
{
    TIdxScore p(idx, weight);
    iterator iter = lower_bound(m_Data.begin(), m_Data.end(), p,
                                SSortByFirst<Key, Score>());
    if (iter == m_Data.end()  ||  iter->first != idx) {
        m_Data.insert(iter, p);
    } else {
        iter->second += weight;
    }
}



template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::TrimLength(float trim_pct)
{
    if (trim_pct >= 1) {
        return;
    }

    /// first, sort by score
    /// this sorts in descending order!
    std::sort(m_Data.begin(), m_Data.end(),
              SSortBySecond<Key, Score>());

    /// now, determine how far in we march
    Score sum = 0;
    Score length = Length() * trim_pct;
    iterator iter = m_Data.begin();
    for ( ;  iter != m_Data.end();  ++iter) {
        sum += iter->second * iter->second;
        if (sqrt(sum) >= length) {
            break;
        }
    }

    /// make sure we go at least one
    if (iter == m_Data.begin()) {
        ++iter;
    }

    /// make sure we account for ties
    iterator prev = iter;
    for (;  iter != m_Data.end()  &&  iter->second == prev->second;  ++iter) {
    }

    /// erase and re-sort
    m_Data.erase(iter, m_Data.end());
    std::sort(m_Data.begin(), m_Data.end(),
              SSortByFirst<Key, Score>());
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::TrimCount(size_t max_words)
{
    max_words = max(max_words, (size_t)1);
    if (m_Data.size() <= max_words) {
        return;
    }

    /// first, sort by score
    /// this sorts in descending order!
    SortByScore();

    /// now, determine how far in we march
    iterator iter = m_Data.begin() + max_words - 1;

    /// make sure we account for ties
    iterator prev = iter;
    for (;  iter != m_Data.end()  &&  iter->second == prev->second;  ++iter) {
    }

    /// erase and re-sort
    if (iter != m_Data.end()) {
        m_Data.erase(iter, m_Data.end());
        std::sort(m_Data.begin(), m_Data.end(),
                  SSortByFirst<Key, Score>());
    }
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::TrimThresh(Score  min_score)
{
    /// first, sort by score
    /// this sorts in descending order!
    std::sort(m_Data.begin(), m_Data.end(), SSortBySecond<Key, Score>());

    /// now, determine how far in we march
    iterator iter = m_Data.begin();
    for ( ;  iter != m_Data.end();  ++iter) {
        if (iter->second < min_score) {
            break;
        }
    }

    /// make sure we account for ties
    iterator prev = iter;
    for (;  iter != m_Data.end()  &&  iter->second == prev->second;  ++iter) {
    }

    /// erase and re-sort
    m_Data.erase(iter, m_Data.end());
    std::sort(m_Data.begin(), m_Data.end(), SSortByFirst<Key, Score>());
}


/// force the vector to be sorted in order of descending score
template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::SortByScore()
{
    std::sort(m_Data.begin(), m_Data.end(), SSortBySecond<Key, Score>());
}


// re-sort the vector by index.  This should normally never need to be done
template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::SortByIndex()
{
    std::sort(m_Data.begin(), m_Data.end(), SSortByFirst<Key, Score>());
}



///
/// math functions
///
template <class Key, class Score>
inline
float CRawScoreVector<Key, Score>::Length2() const
{
    Score len = 0;
    ITERATE (typename TVector, iter, m_Data) {
        len += iter->second * iter->second;
    }
    return len;
}


template <class Key, class Score>
inline
float CRawScoreVector<Key, Score>::Length() const
{
    return sqrt(Length2());
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::Normalize()
{
    Score inv_len = Length();
    if (inv_len) {
        inv_len = 1.0f / inv_len;

        NON_CONST_ITERATE (typename TVector, iter, m_Data) {
            iter->second *= inv_len;
        }
    }
}


template <class Key, class Score>
inline
void CRawScoreVector<Key, Score>::ProbNormalize()
{
    Score inv_len = 0;
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        inv_len += iter->second;
    }
    if (inv_len) {
        inv_len = 1.0f / inv_len;

        NON_CONST_ITERATE (typename TVector, iter, m_Data) {
            iter->second *= inv_len;
        }
    }
}


template <class Key, class Score>
inline CRawScoreVector<Key, Score>&
CRawScoreVector<Key, Score>::operator+=(const CRawScoreVector<Key, Score>& other)
{
    const_iterator begin = other.m_Data.begin();
    const_iterator end   = other.m_Data.end();
    size_t diff = end - begin;
    size_t orig_size = m_Data.size();
    m_Data.reserve(m_Data.size() + diff);

    bool need_sort = false;
    for ( ;  begin != end;  ++begin) {
        iterator iter =
            lower_bound(m_Data.begin(), m_Data.begin() + orig_size,
                        *begin, SSortByFirst<Key, Score>());
        if (iter == m_Data.end()  ||  iter->first != begin->first) {
            m_Data.push_back(*begin);
            need_sort = true;
        } else {
            iter->second += begin->second;
        }
    }

    if (need_sort) {
        if (is_sorted(m_Data.begin() + orig_size, m_Data.end())) {
            std::inplace_merge(m_Data.begin(),
                               m_Data.begin() + orig_size,
                               m_Data.end(),
                               SSortByFirst<Key, Score>());
        } else {
            std::sort(m_Data.begin(), m_Data.end(),
                      SSortByFirst<Key, Score>());
        }
    }

    return *this;
}


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>&
CRawScoreVector<Key, Score>::operator-=(const CRawScoreVector<Key, Score>& other)
{
    iterator       iter1 = m_Data.begin();
    const_iterator iter2 = other.m_Data.begin();

    for ( ;  iter1 != m_Data.end()  &&  iter2 != other.m_Data.end();  ) {
        if (iter1->first == iter2->first) {
            iter1->second -= iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                TIdxScore p(iter2->first, -iter2->second);
                iter1 = m_Data.insert(iter1, p);
                ++iter2;
            }
        }
    }

    for ( ;  iter2 != other.m_Data.end();  ++iter2) {
        TIdxScore p(iter2->first, -iter2->second);
        m_Data.push_back(p);
    }

    return *this;
}


template <class Key, class Score>
inline CRawScoreVector<Key, Score>&
CRawScoreVector<Key, Score>::operator*=(Score val)
{
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        iter->second *= val;
    }
    return *this;
}


template <class Key, class Score>
inline
CRawScoreVector<Key, Score>&
CRawScoreVector<Key, Score>::operator/=(Score val)
{
    if (val) {
        val = 1.0f / val;
    }
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        iter->second *= val;
    }
    return *this;
}




/////////////////////////////////////////////////////////////////////////////


template <class Key, class Score>
inline
CScoreVector<Key, Score>::CScoreVector()
{
    m_Uid = InitialValue(&m_Uid);
}


template <class Key, class Score>
inline
CScoreVector<Key, Score>::CScoreVector(const CScoreVector<Key, Score>& other)
{
    *this = other;
}


template <class Key, class Score>
inline
CScoreVector<Key, Score>::CScoreVector(const CRawScoreVector<Key, Score>& other)
{
    *this = other;
}


template <class Key, class Score>
inline
CScoreVector<Key, Score>& CScoreVector<Key, Score>::operator=(const CScoreVector<Key, Score>& other)
{
    if (this != &other) {
        m_Data = other.m_Data;
        m_Uid  = other.m_Uid;
    }
    return *this;
}


template <class Key, class Score>
inline
CScoreVector<Key, Score>& CScoreVector<Key, Score>::operator=(const CRawScoreVector<Key, Score>& other)
{
    m_Data.clear();
    typedef CRawScoreVector<Key, Score> TOtherVector;
    ITERATE (typename TOtherVector, iter, other) {
        m_Data.insert(m_Data.end(), value_type(iter->first, iter->second));
    }
    m_Uid = other.GetId();
    return *this;
}


template <class Key, class Score>
inline
Key CScoreVector<Key, Score>::GetId() const
{
    return m_Uid;
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::SetId(key_type uid)
{
    m_Uid = uid;
}


template <class Key, class Score>
inline
size_t CScoreVector<Key, Score>::size() const
{
    return m_Data.size();
}


template <class Key, class Score>
inline typename CScoreVector<Key, Score>::iterator
CScoreVector<Key, Score>::begin()
{
    return m_Data.begin();
}


template <class Key, class Score>
inline typename CScoreVector<Key, Score>::iterator
CScoreVector<Key, Score>::end()
{
    return m_Data.end();
}


template <class Key, class Score>
inline typename CScoreVector<Key, Score>::const_iterator
CScoreVector<Key, Score>::begin() const
{
    return m_Data.begin();
}


template <class Key, class Score>
inline typename CScoreVector<Key, Score>::const_iterator
CScoreVector<Key, Score>::end() const
{
    return m_Data.end();
}


template <class Key, class Score>
inline
typename CScoreVector<Key, Score>::const_iterator
CScoreVector<Key, Score>::find(const Key& key) const
{
    return m_Data.find(key);
}


template <class Key, class Score>
inline
typename CScoreVector<Key, Score>::iterator
CScoreVector<Key, Score>::find(const Key& key)
{
    return m_Data.find(key);
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::insert(iterator hint, const value_type& v)
{
    m_Data.insert(hint, v);
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::insert(const value_type& v)
{
    m_Data.insert(v);
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::clear()
{
    m_Data.clear();
    m_Uid = InitialValue(&m_Uid);
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::erase(iterator it)
{
    m_Data.erase(it);
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::erase(const key_type& key)
{
    m_Data.erase(key);
}


template <class Key, class Score>
inline
bool CScoreVector<Key, Score>::empty() const
{
    return m_Data.empty();
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::Swap(CScoreVector<Key, Score>& other)
{
    if (this != &other) {
        m_Data.swap(other.m_Data);
        swap(m_Uid, other.m_Uid);
    }
}


template <class Key, class Score>
inline
Score CScoreVector<Key, Score>::Get(Key idx) const
{
    const_iterator iter = m_Data.find(idx);
    if (iter == m_Data.end()) {
        return 0;
    } else {
        return iter->second;
    }
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::Set(Key idx, Score weight)
{
    iterator iter = m_Data.find(idx);
    if (iter == m_Data.end()) {
        m_Data[idx] = weight;
    } else {
        iter->second = weight;
    }
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::Normalize()
{
    // recompute magnitude
    Score magnitude = 0;
    ITERATE (typename TVector, iter, m_Data) {
        magnitude += Score(iter->second) * Score(iter->second);
    }
    if (magnitude) {
        magnitude = 1.0f / sqrt(magnitude);
        NON_CONST_ITERATE (typename TVector, iter, m_Data) {
            iter->second *= magnitude;
        }
    }
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::ProbNormalize()
{
    Score inv_len = 0;
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        inv_len += iter->second;
    }
    if (inv_len) {
        inv_len = 1.0f / inv_len;

        NON_CONST_ITERATE (typename TVector, iter, m_Data) {
            iter->second *= inv_len;
        }
    }
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::Add(Key idx, Score weight)
{
    iterator iter = m_Data.find(idx);
    if (iter == m_Data.end()) {
        m_Data[idx] = weight;
    } else {
        iter->second += weight;
    }
}


template <class Key, class Score>
inline
float CScoreVector<Key, Score>::Length2() const
{
    Score len = 0;
    ITERATE (typename TVector, iter, m_Data) {
        len += iter->second * iter->second;
    }
    return len;
}


template <class Key, class Score>
inline
float CScoreVector<Key, Score>::Length() const
{
    return sqrt(Length2());
}


template <class Key, class Score>
inline CScoreVector<Key, Score>&
CScoreVector<Key, Score>::operator*=(Score val)
{
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        iter->second *= val;
    }
    return *this;
}


template <class Key, class Score>
inline CScoreVector<Key, Score>&
CScoreVector<Key, Score>::operator/=(Score val)
{
    val = 1.0f / val;
    NON_CONST_ITERATE (typename TVector, iter, m_Data) {
        iter->second *= val;
    }
    return *this;
}


template <class Key, class Score>
inline CScoreVector<Key, Score>&
CScoreVector<Key, Score>::operator+=(const CScoreVector<Key, Score>& other)
{
    iterator       iter1 = begin();
    iterator       end1  = end();
    const_iterator iter2 = other.begin();
    const_iterator end2  = other.end();

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            iter1->second += iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                m_Data.insert(iter1, *iter2);
                ++iter2;
            }
        }
    }

    for ( ;  iter2 != end2;  ++iter2) {
        iter1 =
            m_Data.insert(iter1,
                          value_type(iter2->first, iter2->second));
    }
    return *this;
}


template <class Key, class Score>
inline
CScoreVector<Key, Score>& CScoreVector<Key, Score>::operator-=(const CScoreVector<Key, Score>& other)
{
    iterator       iter1 = m_Data.begin();
    iterator       end1  = m_Data.end();
    const_iterator iter2 = other.m_Data.begin();
    const_iterator end2  = other.m_Data.end();

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            iter1->second -= iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                value_type val(iter2->first, -iter2->second);
                m_Data.insert(iter1, val);
                ++iter2;
            }
        }
    }

    for ( ;  iter2 != end2;  ++iter2) {
        value_type val(iter2->first, -iter2->second);
        m_Data.insert(iter1, val);
    }

    return *this;
}


template <class Key, class Score>
inline
void CScoreVector<Key, Score>::TrimLength(float trim_pct)
{
    if (trim_pct < 1.0f) {

        /// trim the document vector
        typedef vector< pair<Key, Score> > TInvVector;
        TInvVector v;
        v.reserve(Get().size());
        NON_CONST_ITERATE (typename TVector, iter, Set()) {
            v.push_back(*iter);
        }
        std::sort(v.begin(), v.end(), SSortBySecond<Key, Score>());

        trim_pct *= Length();
        Score sum = 0;
        typename TInvVector::iterator iter = v.begin();
        typename TInvVector::iterator end  = v.end();

        /// determine where the break point is
        for ( ;  iter != end  &&  sqrt(sum) < trim_pct;  ++iter) {
            sum += iter->second * iter->second;
        }

        /// march forward preserving all scores that are equal here
        typename TInvVector::iterator prev = iter;
        if (prev != v.begin()) {
            --prev;
        }
        for (;  iter != end  &&  prev->first == iter->first;  ++iter) {
        }

        /// erase our words
        for ( ;  iter != end;  ++iter) {
            Set().erase(iter->first);
        }
    }
}

template <class Key, class Score>
inline
void CScoreVector<Key, Score>::TrimCount(size_t max_words)
{
    if (max_words < m_Data.size()) {

        /// trim the document vector
        typedef vector< pair<Key, Score> > TInvVector;
        TInvVector v;
        v.reserve(Get().size());
        NON_CONST_ITERATE (typename TVector, iter, Set()) {
            v.push_back(*iter);
        }
        std::sort(v.begin(), v.end(), SSortBySecond<Key, Score>());

        typename TInvVector::iterator iter = v.begin() + max_words;
        typename TInvVector::iterator prev = iter - 1;
        typename TInvVector::iterator end  = v.end();
        for ( ;  iter != end  &&  prev->first == iter->first;  ++iter) {
            prev = iter;
        }

        for ( ; iter != end;  ++iter) {
            m_Data.erase(iter->first);
        }
    }
}

template <class Key, class Score>
inline
void CScoreVector<Key, Score>::TrimThresh(Score min_score)
{
    /// now, determine how far in we march
    iterator iter = m_Data.begin();
    for ( ;  iter != m_Data.end();  ) {
        if (iter->second < min_score) {
            m_Data.erase(iter++);
        } else {
            ++iter;
        }
    }
}



template <class Key, class Score>
inline void
CScoreVector<Key, Score>::SubtractMissing(const CScoreVector<Key, Score>& other)
{
    iterator       iter1 = begin();
    iterator       end1  = end();
    const_iterator iter2 = other.begin();
    const_iterator end2  = other.end();

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                m_Data.insert(iter1,
                              typename TVector::value_type(iter2->first,
                                                           -iter2->second));
                ++iter2;
            }
        }
    }

    for ( ;  iter2 != end2;  ++iter2) {
        iter1 =
            m_Data.insert(iter1,
                          typename TVector::value_type(iter2->first,
                                                       -iter2->second));
    }
}


template <class Key, class Score>
inline void
CScoreVector<Key, Score>::AddScores(const CScoreVector<Key, Score>& other)
{
    iterator       iter1 = begin();
    iterator       end1  = end();
    const_iterator iter2 = other.begin();
    const_iterator end2  = other.end();

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            iter1->second += iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                m_Data.insert(iter1, *iter2);
                ++iter2;
            }
        }
    }

    for ( ;  iter2 != end2;  ++iter2) {
        iter1 =
            m_Data.insert(iter1,
                          typename TVector::value_type(iter2->first,
                                                       iter2->second));
    }
}


/////////////////////////////////////////////////////////////////////////////
///

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreCombined(const ScoreVectorA& vec1, const ScoreVectorB& vec2)
{
    typename ScoreVectorA::score_type dot = 0;
    typename ScoreVectorA::score_type distance = 0;
    ncbi::DotAndDistance(vec1.begin(), vec1.end(),
                         vec2.begin(), vec2.end(),
                         &dot, &distance);
    return dot * distance / (vec1.Length() * vec2.Length());
}


template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreCosine(const ScoreVectorA& vec1, const ScoreVectorB& vec2)
{
    return ncbi::Cosine(vec1.begin(), vec1.end(),
                        vec2.begin(), vec2.end());
}


///
/// The dice coefficient is defined as
///
///     d = (dot) / (sum(scores_a) + sum(scores_b))
///

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDice(const ScoreVectorA& vec_a, const ScoreVectorB& vec_b)
{
    return ncbi::Dice(vec_a.begin(), vec_a.end(),
                      vec_b.begin(), vec_b.end());
}


template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDistance(const ScoreVectorA& vec_a, const ScoreVectorB& vec_b)
{
    return ncbi::Distance(vec_a.begin(), vec_a.end(),
                          vec_b.begin(), vec_b.end());
}


template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDot(const ScoreVectorA& vec_a, const ScoreVectorB& vec_b)
{
    return ncbi::Dot(vec_a.begin(), vec_a.end(),
                     vec_b.begin(), vec_b.end());
}


///
/// The Jaccard coefficient is defined as
///
///     d = (dot) / (sum(scores_a)^2 + sum(scores_b)^2 - dot)
///

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreJaccard(const ScoreVectorA& vec_a, const ScoreVectorB& vec_b)
{
    return ncbi::Jaccard(vec_a.begin(), vec_a.end(),
                         vec_b.begin(), vec_b.end());
}


///
/// The overlap function is a dot product weighted by the *shortest* of each
/// term
///
template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreOverlap(const ScoreVectorA& vec_a, const ScoreVectorB& vec_b)
{
    return ncbi::Overlap(vec_a.begin(), vec_a.end(),
                         vec_b.begin(), vec_b.end());
}


END_NCBI_SCOPE
