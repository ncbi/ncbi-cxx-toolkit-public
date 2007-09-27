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


#include <objmgr/seq_map_ci.hpp>
#include <objects/seq/Seq_data.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CRandom;

BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CSeqVectorTypes
{
public:
    typedef unsigned char       TResidue;
    typedef CSeq_data::E_Choice TCoding;
    typedef TResidue            value_type;
    typedef TSeqPos             size_type;
    typedef TSignedSeqPos       difference_type;
    
    enum ECaseConversion {
        eCaseConversion_none,
        eCaseConversion_upper,
        eCaseConversion_lower
    };

protected:
    static TResidue sx_GetGapChar(TCoding coding,
                                  ECaseConversion case_cvt);
    static const char* sx_GetConvertTable(TCoding src, TCoding dst,
                                          bool reverse,
                                          ECaseConversion case_cvt);
};

class CSeqVector;


/////////////////////////////////////////////////////////////////////////////
// INcbi2naRandomizer interface is used to randomize ambiguous nucleotides
// when generating unambiguous Ncbi2na encoding.
// The source encoding is prepared by CSeqVector as unpacked Ncbi4na and
// must be converted to unpacked Ncbi2na with possible randomization.
// Extra parameter with position of current buffer in the whole sequence
// can be used to generate the same bases for the same ambiguous positions.
class NCBI_XOBJMGR_EXPORT INcbi2naRandomizer : public CObject
{
public:
    virtual ~INcbi2naRandomizer(void);

    /// Convert count unpacked bases in buffer 4na -> 2na with randomization.
    /// The argument pos will contain position of the buffer in
    /// the sequence, and can be used to give the same random base
    /// at the same ambiguous position.
    virtual void RandomizeData(char* buffer, size_t count, TSeqPos pos) = 0;
};


class NCBI_XOBJMGR_EXPORT CSeqVector_CI : public CSeqVectorTypes
{
public:
    CSeqVector_CI(void);
    ~CSeqVector_CI(void);
    CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos = 0);
    CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos,
                  ECaseConversion case_cvt);
    CSeqVector_CI(const CSeqVector_CI& sv_it);
    CSeqVector_CI& operator=(const CSeqVector_CI& sv_it);

    bool operator==(const CSeqVector_CI& iter) const;
    bool operator!=(const CSeqVector_CI& iter) const;
    bool operator<(const CSeqVector_CI& iter) const;
    bool operator>(const CSeqVector_CI& iter) const;
    bool operator>=(const CSeqVector_CI& iter) const;
    bool operator<=(const CSeqVector_CI& iter) const;

    /// Fill the buffer string with the sequence data for the interval
    /// [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer);
    /// Fill the buffer string with the count bytes of sequence data
    /// starting with current iterator position
    void GetSeqData(string& buffer, TSeqPos count);

    /// Get number of chars from current position to the current buffer end
    size_t GetBufferSize(void) const;
    /// Get pointer to current char in the buffer
    const char* GetBufferPtr(void) const;
    /// Get pointer to current position+size.
    /// Throw exception if current pos + size is not in the buffer.
    const char* GetBufferEnd(size_t size) const;

    CSeqVector_CI& operator++(void);
    CSeqVector_CI& operator--(void);

    /// special temporary holder for return value from postfix operators
    class CTempValue
    {
    public:
        CTempValue(value_type value)
            : m_Value(value)
            {
            }

        value_type operator*(void) const
            {
                return m_Value;
            }
    private:
        value_type m_Value;
    };
    /// Restricted postfix operators.
    /// They allow only get value from old position by operator*,
    /// like in commonly used copying cycle:
    /// CSeqVector_CI src;
    /// for ( ... ) {
    ///     *dst++ = *src++;
    /// }
    CTempValue operator++(int)
        {
            value_type value(**this);
            ++*this;
            return value;
        }
    CTempValue operator--(int)
        {
            value_type value(**this);
            --*this;
            return value;
        }

    TSeqPos GetPos(void) const;
    CSeqVector_CI& SetPos(TSeqPos pos);

    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);

    void SetRandomizeAmbiguities(void);
    void SetRandomizeAmbiguities(Uint4 seed);
    void SetRandomizeAmbiguities(CRandom& random_gen);
    void SetRandomizeAmbiguities(CRef<INcbi2naRandomizer> randomizer);
    void SetNoAmbiguities(void);

    TResidue operator*(void) const;
    bool IsValid(void) const;

    DECLARE_OPERATOR_BOOL(IsValid());

    /// true if current position of CSeqVector_CI is inside of sequence gap
    bool IsInGap(void) const;
    /// returns character representation of gap in sequence
    TResidue GetGapChar(void) const;
    /// returns number of gap symbols ahead including current symbol
    /// returns 0 if current position is not in gap
    TSeqPos GetGapSizeForward(void) const;
    /// returns number of gap symbols before current symbol
    /// returns 0 if current position is not in gap
    TSeqPos GetGapSizeBackward(void) const;
    /// skip current gap forward
    /// returns number of skipped gap symbols
    /// does nothing and returns 0 if current position is not in gap
    TSeqPos SkipGap(void);
    /// skip current gap backward
    /// returns number of skipped gap symbols
    /// does nothing and returns 0 if current position is not in gap
    TSeqPos SkipGapBackward(void);

    CSeqVector_CI& operator+=(TSeqPos value);
    CSeqVector_CI& operator-=(TSeqPos value);

private:
    TSeqPos x_GetSize(void) const;
    TCoding x_GetCoding(TCoding cacheCoding, TCoding dataCoding) const;

    void x_SetPos(TSeqPos pos);
    void x_InitializeCache(void);
    void x_DestroyCache(void);
    void x_ClearCache(void);
    void x_ResizeCache(size_t size);
    void x_SwapCache(void);
    void x_UpdateCacheUp(TSeqPos pos);
    void x_UpdateCacheDown(TSeqPos pos);
    void x_FillCache(TSeqPos start, TSeqPos count);
    void x_UpdateSeg(TSeqPos pos);
    void x_InitSeg(TSeqPos pos);
    void x_InitRandomizer(CRandom& random_gen);

    void x_NextCacheSeg(void);
    void x_PrevCacheSeg(void);

    TSeqPos x_CachePos(void) const;
    TSeqPos x_CacheSize(void) const;
    TSeqPos x_CacheEndPos(void) const;
    TSeqPos x_BackupPos(void) const;
    TSeqPos x_BackupSize(void) const;
    TSeqPos x_BackupEndPos(void) const;

    TSeqPos x_CacheOffset(void) const;

    void x_ResetCache(void);
    void x_ResetBackup(void);

    void x_ThrowOutOfRange(void) const;

    friend class CSeqVector;
    void x_SetVector(CSeqVector& seq_vector);

    typedef char* TCacheData;
    typedef char* TCache_I;

    CHeapScope               m_Scope;
    CConstRef<CSeqMap>       m_SeqMap;
    CTSE_Handle              m_TSE;
    ENa_strand               m_Strand;
    TCoding                  m_Coding;
    ECaseConversion          m_CaseConversion;
    // Current CSeqMap segment
    CSeqMap_CI               m_Seg;
    // Current cache pointer
    TCache_I                 m_Cache;
    // Current cache
    TSeqPos                  m_CachePos;
    TCacheData               m_CacheData;
    TCache_I                 m_CacheEnd;
    // Backup cache
    TSeqPos                  m_BackupPos;
    TCacheData               m_BackupData;
    TCache_I                 m_BackupEnd;
    CRef<INcbi2naRandomizer> m_Randomizer;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector_CI::TCoding CSeqVector_CI::GetCoding(void) const
{
    return m_Coding;
}


inline
TSeqPos CSeqVector_CI::x_CachePos(void) const
{
    return m_CachePos;
}


inline
TSeqPos CSeqVector_CI::x_CacheSize(void) const
{
    return TSeqPos(m_CacheEnd - m_CacheData);
}


inline
TSeqPos CSeqVector_CI::x_CacheEndPos(void) const
{
    return x_CachePos() + x_CacheSize();
}


inline
TSeqPos CSeqVector_CI::x_BackupPos(void) const
{
    return m_BackupPos;
}


inline
TSeqPos CSeqVector_CI::x_BackupSize(void) const
{
    return TSeqPos(m_BackupEnd - m_BackupData);
}


inline
TSeqPos CSeqVector_CI::x_BackupEndPos(void) const
{
    return x_BackupPos() + x_BackupSize();
}


inline
TSeqPos CSeqVector_CI::x_CacheOffset(void) const
{
    return TSeqPos(m_Cache - m_CacheData);
}


inline
TSeqPos CSeqVector_CI::GetPos(void) const
{
    return x_CachePos() + x_CacheOffset();
}


inline
void CSeqVector_CI::x_ResetBackup(void)
{
    m_BackupEnd = m_BackupData;
}


inline
void CSeqVector_CI::x_ResetCache(void)
{
    m_Cache = m_CacheEnd = m_CacheData;
}


inline
void CSeqVector_CI::x_SwapCache(void)
{
    swap(m_CacheData, m_BackupData);
    swap(m_CacheEnd, m_BackupEnd);
    swap(m_CachePos, m_BackupPos);
    m_Cache = m_CacheData;
}


inline
CSeqVector_CI& CSeqVector_CI::SetPos(TSeqPos pos)
{
    TCache_I cache = m_CacheData;
    TSeqPos offset = pos - m_CachePos;
    TSeqPos size = TSeqPos(m_CacheEnd - cache);
    if ( offset >= size ) {
        x_SetPos(pos);
    }
    else {
        m_Cache = cache + offset;
    }
    return *this;
}


inline
bool CSeqVector_CI::IsValid(void) const
{
    return m_Cache < m_CacheEnd;
}


inline
bool CSeqVector_CI::operator==(const CSeqVector_CI& iter) const
{
    return GetPos() == iter.GetPos();
}


inline
bool CSeqVector_CI::operator!=(const CSeqVector_CI& iter) const
{
    return GetPos() != iter.GetPos();
}


inline
bool CSeqVector_CI::operator<(const CSeqVector_CI& iter) const
{
    return GetPos() < iter.GetPos();
}


inline
bool CSeqVector_CI::operator>(const CSeqVector_CI& iter) const
{
    return GetPos() > iter.GetPos();
}


inline
bool CSeqVector_CI::operator<=(const CSeqVector_CI& iter) const
{
    return GetPos() <= iter.GetPos();
}


inline
bool CSeqVector_CI::operator>=(const CSeqVector_CI& iter) const
{
    return GetPos() >= iter.GetPos();
}


inline
CSeqVector_CI::TResidue CSeqVector_CI::operator*(void) const
{
    if ( !bool(*this) ) {
        x_ThrowOutOfRange();
    }
    return *m_Cache;
}


inline
bool CSeqVector_CI::IsInGap(void) const
{
    return m_Seg.GetType() == CSeqMap::eSeqGap;
}


inline
CSeqVector_CI& CSeqVector_CI::operator++(void)
{
    if ( ++m_Cache >= m_CacheEnd ) {
        x_NextCacheSeg();
    }
    return *this;
}


inline
CSeqVector_CI& CSeqVector_CI::operator--(void)
{
    TCache_I cache = m_Cache;
    if ( cache == m_CacheData ) {
        x_PrevCacheSeg();
    }
    else {
        m_Cache = cache - 1;
    }
    return *this;
}


inline
void CSeqVector_CI::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer)
{
    SetPos(start);
    if (start > stop) {
        buffer.erase();
        return;
    }
    GetSeqData(buffer, stop - start);
}


inline
size_t CSeqVector_CI::GetBufferSize(void) const
{
    return m_CacheEnd - m_Cache;
}


inline
const char* CSeqVector_CI::GetBufferPtr(void) const
{
    return m_Cache;
}


inline
const char* CSeqVector_CI::GetBufferEnd(size_t size) const
{
    const char* ptr = m_Cache + size;
    if (ptr < m_Cache || ptr > m_CacheEnd) {
        x_ThrowOutOfRange();
    }
    return ptr;
}


inline
CSeqVector_CI& CSeqVector_CI::operator+=(TSeqPos value)
{
    SetPos(GetPos() + value);
    return *this;
}


inline
CSeqVector_CI& CSeqVector_CI::operator-=(TSeqPos value)
{
    SetPos(GetPos() - value);
    return *this;
}


inline
CSeqVector_CI operator+(const CSeqVector_CI& iter, TSeqPos value)
{
    CSeqVector_CI ret(iter);
    ret += value;
    return ret;
}


inline
CSeqVector_CI operator-(const CSeqVector_CI& iter, TSeqPos value)
{
    CSeqVector_CI ret(iter);
    ret -= value;
    return ret;
}


inline
CSeqVector_CI operator+(const CSeqVector_CI& iter, TSignedSeqPos value)
{
    CSeqVector_CI ret(iter);
    ret.SetPos(iter.GetPos() + value);
    return ret;
}


inline
CSeqVector_CI operator-(const CSeqVector_CI& iter, TSignedSeqPos value)
{
    CSeqVector_CI ret(iter);
    ret.SetPos(iter.GetPos() - value);
    return ret;
}


inline
TSignedSeqPos operator-(const CSeqVector_CI& iter1,
                        const CSeqVector_CI& iter2)
{
    return iter1.GetPos() - iter2.GetPos();
}


inline
CSeqVector_CI::TCoding CSeqVector_CI::x_GetCoding(TCoding cacheCoding,
                                                  TCoding dataCoding) const
{
    return cacheCoding != CSeq_data::e_not_set? cacheCoding: dataCoding;
}


inline
CSeqVector_CI::TResidue CSeqVector_CI::GetGapChar(void) const
{
    return sx_GetGapChar(m_Coding, m_CaseConversion);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_VECTOR_CI__HPP
