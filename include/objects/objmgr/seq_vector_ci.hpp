#ifndef SEQ_VECTOR_CI__HPP
#define SEQ_VECTOR_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Seq-vector iterator
*
*/

#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqVector_CI
{
public:
    CSeqVector_CI(void);
    ~CSeqVector_CI(void);
    CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos = 0);
    CSeqVector_CI(const CSeqVector_CI& sv_it);
    CSeqVector_CI& operator=(const CSeqVector_CI& sv_it);

    typedef CSeqVector::TResidue     TResidue;

    // Fill the buffer string with the sequence data for the interval
    // [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer);

    CSeqVector_CI& operator++(void);
    CSeqVector_CI& operator--(void);

    TSeqPos GetPos(void) const;
    void SetPos(TSeqPos pos);

    TResidue operator*(void) const;
    operator bool(void) const;

private:
    void x_NextCacheSeg(void);
    void x_PrevCacheSeg(void);
    void x_UpdateCache(void);
    void x_FillCache(TSeqPos pos, TSeqPos end);

    friend class CSeqVector;

    struct SCacheData {
        typedef vector<char> TCacheData;
        typedef char* TCache_I;
        SCacheData(void);
        SCacheData(const SCacheData& data);
        SCacheData& operator=(const SCacheData& data);

        bool CacheContains(TSeqPos pos);
        bool SegContains(TSeqPos pos);

        TSeqPos                 m_CachePos;
        TSeqPos                 m_CacheLen;
        TCacheData              m_CacheData;
        TCache_I                m_Cache;
        CSeqMap::const_iterator m_Seg;
    };

    typedef SCacheData::TCacheData TCacheData;
    typedef SCacheData::TCache_I TCache_I;
    typedef CSeqVector::TCoding TCoding;

    // TResidue x_GetResidue(TSeqPos pos);
    void x_ResizeCache(size_t size);
    void x_UpdateCachePtr(void);

    // Getters for current cache members
    TSeqPos& x_CachePos(void) const { return m_CurrentCache->m_CachePos; }
    TSeqPos& x_CacheLen(void) const { return m_CurrentCache->m_CacheLen; }
    TCacheData& x_CacheData(void) const { return m_CurrentCache->m_CacheData; }
    TCache_I& x_Cache(void) const { return m_CurrentCache->m_Cache; }
    CSeqMap::const_iterator& x_Seg(void) { return m_CurrentCache->m_Seg; }


    CConstRef<CSeqVector>   m_Vector;
    TSeqPos                 m_Position;
    TCoding                 m_Coding;
    SCacheData*             m_CurrentCache;
    SCacheData*             m_BackupCache;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

inline
CSeqVector_CI::SCacheData::SCacheData(void)
    : m_CachePos(kInvalidSeqPos),
      m_CacheLen(0),
      m_Cache(0)
{
    return;
}

inline
CSeqVector_CI::SCacheData::SCacheData(const SCacheData& data)
    : m_CachePos(data.m_CachePos),
      m_CacheLen(data.m_CacheLen),
      m_CacheData(data.m_CacheData),
      m_Seg(data.m_Seg)
{
    m_Cache = m_CacheData.empty() ? 0 : &m_CacheData[0];
}

inline
CSeqVector_CI::SCacheData&
CSeqVector_CI::SCacheData::operator=(const SCacheData& data)
{
    if (this != &data) {
        m_CachePos = data.m_CachePos;
        m_CacheLen = data.m_CacheLen;
        m_CacheData = data.m_CacheData;
        m_Cache = m_CacheData.empty() ? 0 : &m_CacheData[0];
        m_Seg = data.m_Seg;
    }
    return *this;
}

inline
bool CSeqVector_CI::SCacheData::CacheContains(TSeqPos pos)
{
    return pos >= m_CachePos  &&  pos < m_CachePos + m_CacheLen;
}

inline
bool CSeqVector_CI::SCacheData::SegContains(TSeqPos pos)
{
    return pos >= m_Seg.GetPosition()  &&  pos < m_Seg.GetEndPosition();
}

inline
CSeqVector_CI::CSeqVector_CI(void)
    : m_Vector(0),
      m_Position(kInvalidSeqPos),
      m_Coding(CSeq_data::e_not_set),
      m_CurrentCache(new SCacheData),
      m_BackupCache(new SCacheData)
{
    return;
}

inline
CSeqVector_CI::~CSeqVector_CI(void)
{
    delete m_CurrentCache;
    delete m_BackupCache;
}

inline
CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
    : m_Vector(sv_it.m_Vector),
      m_Position(sv_it.m_Position),
      m_Coding(sv_it.m_Coding),
      m_CurrentCache(new SCacheData(*sv_it.m_CurrentCache)),
      m_BackupCache(new SCacheData(*sv_it.m_BackupCache))
{
    x_UpdateCachePtr();
}

inline
CSeqVector_CI& CSeqVector_CI::operator=(const CSeqVector_CI& sv_it)
{
    if (this == &sv_it)
        return *this;
    m_Vector = sv_it.m_Vector;
    m_Position = sv_it.m_Position;
    m_Coding = sv_it.m_Coding;
    *m_CurrentCache = *sv_it.m_CurrentCache;
    *m_BackupCache = *sv_it.m_BackupCache;
    x_UpdateCachePtr();
    return *this;
}

inline
CSeqVector_CI& CSeqVector_CI::operator++(void)
{
    if (bool(m_Vector)  &&  m_Position < m_Vector->size()) {
        ++m_Position;
        if (m_Position >= x_CachePos() + x_CacheLen()) {
            x_NextCacheSeg();
        }
    }
    return *this;
}

inline
CSeqVector_CI& CSeqVector_CI::operator--(void)
{
    if (bool(m_Vector)  &&  m_Position < m_Vector->size()) {
        --m_Position;
        if (m_Position < x_CachePos()) {
            x_PrevCacheSeg();
        }
    }
    return *this;
}

inline
TSeqPos CSeqVector_CI::GetPos(void) const
{
    return m_Position;
}

inline
CSeqVector_CI::TResidue CSeqVector_CI::operator*(void) const
{
    _ASSERT(m_Position != kInvalidSeqPos);
    return x_Cache()[m_Position - x_CachePos()];
}

inline
CSeqVector_CI::operator bool(void) const
{
    return bool(m_Vector)  &&
        m_Position != kInvalidSeqPos  &&
        m_Position < m_Vector->size();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/05/27 19:42:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_VECTOR_CI__HPP
