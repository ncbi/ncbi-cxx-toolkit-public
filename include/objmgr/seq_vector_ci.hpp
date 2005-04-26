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
class CNcbi2naRandomizer;

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
    void x_SetRandomizer(CNcbi2naRandomizer& randomizer);
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
    CRef<CNcbi2naRandomizer> m_Randomizer;
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
    return m_CacheEnd - m_CacheData;
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
    return m_BackupEnd - m_BackupData;
}


inline
TSeqPos CSeqVector_CI::x_BackupEndPos(void) const
{
    return x_BackupPos() + x_BackupSize();
}


inline
TSeqPos CSeqVector_CI::x_CacheOffset(void) const
{
    return m_Cache - m_CacheData;
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
    TSeqPos size = m_CacheEnd - cache;
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2005/04/26 18:48:00  vasilche
* Use case conversion to get gap symbol.
* Removed obsolete structur SSeqData.
* Put common code for CSeqVector and CSeqVector_CI into CSeqVectorTypes.
*
* Revision 1.29  2005/04/11 15:23:23  vasilche
* Added option to change letter case in CSeqVector_CI.
*
* Revision 1.28  2005/03/28 19:23:06  vasilche
* Added restricted post-increment and post-decrement operators.
*
* Revision 1.27  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.26  2004/12/22 15:56:18  vasilche
* Added CTSE_Handle.
* Allow used TSE linking.
*
* Revision 1.25  2004/12/09 20:36:41  grichenk
* Throw exception from operator*() if the iterator is out of range
*
* Revision 1.24  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.23  2004/10/27 16:36:29  vasilche
* Added methods for working with gaps.
*
* Revision 1.22  2004/10/27 14:17:33  vasilche
* Implemented CSeqVector::IsInGap() and CSeqVector_CI::IsInGap().
*
* Revision 1.21  2004/10/01 19:52:50  kononenk
* Added doxygen formatting
*
* Revision 1.20  2004/06/14 18:30:08  grichenk
* Added ncbi2na randomizer to CSeqVector
*
* Revision 1.19  2004/04/26 14:15:00  grichenk
* Added standard container methods
*
* Revision 1.18  2004/04/22 18:34:18  grichenk
* Added optional ambiguity randomizer for ncbi2na coding
*
* Revision 1.17  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.16  2003/12/02 16:42:50  grichenk
* Fixed GetSeqData to return empty string if start > stop.
* Added GetSeqData(const_iterator, const_iterator, string).
*
* Revision 1.15  2003/10/08 14:16:55  vasilche
* Removed circular reference CSeqVector <-> CSeqVector_CI.
*
* Revision 1.14  2003/08/29 13:34:47  vasilche
* Rewrote CSeqVector/CSeqVector_CI code to allow better inlining.
* CSeqVector::operator[] made significantly faster.
* Added possibility to have platform dependent byte unpacking functions.
*
* Revision 1.13  2003/08/21 13:32:04  vasilche
* Optimized CSeqVector iteration.
* Set some CSeqVector values (mol type, coding) in constructor instead of detecting them while iteration.
* Remove unsafe bit manipulations with coding.
*
* Revision 1.12  2003/08/19 18:34:11  vasilche
* Buffer length constant moved to *.cpp file for easier modification.
*
* Revision 1.11  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.10  2003/07/18 19:42:42  vasilche
* Added x_DestroyCache() method.
*
* Revision 1.9  2003/06/24 19:46:41  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.8  2003/06/17 22:03:37  dicuccio
* Added missing export specifier
*
* Revision 1.7  2003/06/05 20:20:21  grichenk
* Fixed bugs in assignment functions,
* fixed minor problems with coordinates.
*
* Revision 1.6  2003/06/04 13:48:53  grichenk
* Improved double-caching, fixed problem with strands.
*
* Revision 1.5  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.4  2003/05/30 20:43:54  vasilche
* Fixed CSeqVector_CI::operator--().
*
* Revision 1.3  2003/05/30 19:30:08  vasilche
* Fixed compilation on GCC 3.
*
* Revision 1.2  2003/05/30 18:00:26  grichenk
* Fixed caching bugs in CSeqVector_CI
*
* Revision 1.1  2003/05/27 19:42:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_VECTOR_CI__HPP
