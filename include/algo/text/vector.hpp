#ifndef ALGO_TEXT___VECTOR__HPP
#define ALGO_TEXT___VECTOR__HPP

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

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

template <class Key, class Score>
class CScoreVector;

template <class Key, class Score>
class CRawScoreVector;


/////////////////////////////////////////////////////////////////////////////
///
/// class CRawScoreVector stores its data in a (sorted) STL vector
/// this gives a better memory profile and ias easier to serialize
///

template <class Key, class Score>
class CRawScoreVector : public CObject
{
public:
    typedef Key                              key_type;
    typedef Score                            score_type;
    typedef pair<Key, Score>                 TIdxScore;
    typedef vector<TIdxScore>                TVector;
    typedef TIdxScore                        value_type;
    typedef typename TVector::iterator       iterator;
    typedef typename TVector::const_iterator const_iterator;

    CRawScoreVector();
    virtual ~CRawScoreVector() {}

    CRawScoreVector(const CScoreVector<Key, Score>&);
    CRawScoreVector& operator=(const CScoreVector<Key, Score>&);

    CRawScoreVector(const CRawScoreVector<Key, Score>&);
    CRawScoreVector& operator=(const CRawScoreVector<Key, Score>&);

    virtual void Swap(CRawScoreVector<Key, Score>& other);

    /// @name STL-ish functions
    /// @{

    void clear();
    bool empty() const;
    size_t size() const;
    void reserve(size_t size);
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    iterator find(const Key& key);
    const_iterator find(const Key& key) const;
    void insert(const value_type& val);
    void insert(iterator ins_before,
                const_iterator start,
                const_iterator stop);
    void erase(iterator it);

    /// @}

    ///
    /// setup functions
    ///
    key_type GetId() const;
    void SetId(key_type uid);
    Score Get(Key idx) const;
    void Set(Key idx, Score weight);
    void Add(Key idx, Score weight = Score(1));
    void Set(const_iterator begin, const_iterator end);

    void TrimLength(float  trim_pct);
    void TrimCount (size_t max_words);
    void TrimThresh(Score  min_score);

    /// force the vector to be sorted in order of descending score
    void SortByScore();

    /// re-sort the vector by index.
    /// This should normally never need to be done
    void SortByIndex();

    ///
    /// math functions
    ///
    float Length2() const;
    float Length() const;
    void Normalize();
    void ProbNormalize();

    CRawScoreVector<Key, Score>& operator+=(const CRawScoreVector<Key, Score>& other);
    CRawScoreVector<Key, Score>& operator-=(const CRawScoreVector<Key, Score>& other);
    CRawScoreVector<Key, Score>& operator*=(Score val);
    CRawScoreVector<Key, Score>& operator/=(Score val);

    TVector&       Set()       { return m_Data; }
    const TVector& Get() const { return m_Data; }
    
protected:
    /// UID for this set
    key_type m_Uid;

    /// the data for this document
    TVector m_Data;
};


/////////////////////////////////////////////////////////////////////////////


template <class Key, class Score>
class CScoreVector : public CObject
{
public:
    typedef Key                     key_type;
    typedef Score                   score_type;
    typedef map<Key, Score> TVector;
    typedef typename TVector::value_type     value_type;
    typedef typename TVector::iterator       iterator;
    typedef typename TVector::const_iterator const_iterator;

    CScoreVector();
    virtual ~CScoreVector() {}
    CScoreVector(const CScoreVector<Key, Score>& other);
    CScoreVector(const CRawScoreVector<Key, Score>& other);
    CScoreVector& operator=(const CScoreVector<Key, Score>& other);
    CScoreVector& operator=(const CRawScoreVector<Key, Score>& other);

    virtual void Swap(CScoreVector<Key, Score>& other);

    /// @name STL-ish functions
    /// @{

    void clear();
    bool empty() const;
    size_t size() const;
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    iterator find(const Key& key);
    const_iterator find(const Key& key) const;
    pair<iterator, bool> insert(const value_type& val);
    iterator insert(iterator hint, const value_type& val);
    void erase(iterator it);
    void erase(const key_type& v);

    template <typename OtherIterator>
    void insert(OtherIterator it_begin, OtherIterator it_end)
    {
        m_Data.insert(it_begin, it_end);
    }

    /// @}

    ///
    /// setup functions
    ///
    key_type GetId() const;
    void SetId(key_type uid);
    size_t GetSize() const { return m_Data.size(); }
    Score Get(Key idx) const;
    void Set(Key idx, Score weight);
    void Add(Key idx, Score weight = Score(1));

    void TrimLength(float  trim_pct);
    void TrimCount (size_t max_words);
    void TrimThresh(Score  min_score);

    void SubtractMissing(const CScoreVector<Key, Score>& other);
    void AddScores      (const CScoreVector<Key, Score>& other);

    ///
    /// math functions
    ///
    float Length2() const;
    float Length() const;
    void Normalize();
    void ProbNormalize();

    CScoreVector<Key, Score>& operator+=(const CScoreVector<Key, Score>& other);
    CScoreVector<Key, Score>& operator-=(const CScoreVector<Key, Score>& other);
    CScoreVector<Key, Score>& operator*=(Score val);
    CScoreVector<Key, Score>& operator/=(Score val);

    TVector&       Set()       { return m_Data; }
    const TVector& Get() const { return m_Data; }
    
protected:
    /// UID for this set
    key_type m_Uid;

    /// the data for this document
    TVector m_Data;
};



/// @name Scoring Interface
/// @{

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreCombined(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreCosine(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDice(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDistance(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreDot(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreJaccard(const ScoreVectorA& query, const ScoreVectorB& vec);

template <class ScoreVectorA, class ScoreVectorB>
inline
float ScoreOverlap(const ScoreVectorA& query, const ScoreVectorB& vec);

/// @}



END_NCBI_SCOPE


#include <algo/text/vector_impl.hpp>

#endif  // ALGO_TEXT___VECTOR__HPP
