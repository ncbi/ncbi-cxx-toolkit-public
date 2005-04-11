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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-vector iterator
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/NCBIpaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIpna.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <algorithm>
#include <objmgr/impl/seq_vector_cvt.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <util/random_gen.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const TSeqPos kCacheSize = 1024;

namespace {
    void ThrowOutOfRangeSeq_inst(TSeqPos pos)
    {
        NCBI_THROW(CSeqVectorException, eOutOfRange,
                   "reference out of range of Seq-inst data: "+
                   NStr::UIntToString(pos));
    }
}

template<class DstIter, class SrcCont>
inline
void copy_8bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_8bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_8bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_8bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_8bit(dst, count, srcCont, srcPos);
        }
    }
}


template<class DstIter, class SrcCont>
inline
void copy_4bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos / 2 > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_4bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_4bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_4bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_4bit(dst, count, srcCont, srcPos);
        }
    }
}


template<class DstIter, class SrcCont>
inline
void copy_2bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos / 4 > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_2bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_2bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_2bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_2bit(dst, count, srcCont, srcPos);
        }
    }
}


// CSeqVector_CI::


CSeqVector_CI::CSeqVector_CI(void)
    : m_Strand(eNa_strand_unknown),
      m_Coding(CSeq_data::e_not_set),
      m_CaseConversion(eCaseConversion_none),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(0),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(0),
      m_BackupEnd(0)
{
}


CSeqVector_CI::~CSeqVector_CI(void)
{
    x_DestroyCache();
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
    : m_Strand(eNa_strand_unknown),
      m_Coding(CSeq_data::e_not_set),
      m_CaseConversion(eCaseConversion_none),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(0),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(0),
      m_BackupEnd(0),
      m_Randomizer(sv_it.m_Randomizer)
{
    try {
        *this = sv_it;
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos)
    : m_Scope(seq_vector.m_Scope),
      m_SeqMap(seq_vector.m_SeqMap),
      m_TSE(seq_vector.m_TSE),
      m_Strand(seq_vector.m_Strand),
      m_Coding(seq_vector.m_Coding),
      m_CaseConversion(eCaseConversion_none),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(0),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(0),
      m_BackupEnd(0),
      m_Randomizer(seq_vector.m_Randomizer)
{
    try {
        x_SetPos(pos);
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos,
                             ECaseConversion case_conversion)
    : m_Scope(seq_vector.m_Scope),
      m_SeqMap(seq_vector.m_SeqMap),
      m_TSE(seq_vector.m_TSE),
      m_Strand(seq_vector.m_Strand),
      m_Coding(seq_vector.m_Coding),
      m_CaseConversion(case_conversion),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(0),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(0),
      m_BackupEnd(0),
      m_Randomizer(seq_vector.m_Randomizer)
{
    try {
        x_SetPos(pos);
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


void CSeqVector_CI::x_SetVector(CSeqVector& seq_vector)
{
    if ( m_SeqMap ) {
        // reset old values
        m_Seg = CSeqMap_CI();
        x_ResetCache();
        x_ResetBackup();
    }

    m_Scope  = seq_vector.m_Scope;
    m_SeqMap = seq_vector.m_SeqMap;
    m_TSE = seq_vector.m_TSE;
    m_Strand = seq_vector.m_Strand;
    m_Coding = seq_vector.m_Coding;
    m_CachePos = seq_vector.size();
    m_Randomizer = seq_vector.m_Randomizer;
}


inline
TSeqPos CSeqVector_CI::x_GetSize(void) const
{
    return m_SeqMap->GetLength(m_Scope.GetScopeOrNull());
}


inline
void CSeqVector_CI::x_InitSeg(TSeqPos pos)
{
    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
    sel.SetStrand(m_Strand).SetLinkUsedTSE(m_TSE);
    m_Seg = CSeqMap_CI(m_SeqMap, m_Scope.GetScopeOrNull(), sel, pos);
}


void CSeqVector_CI::x_ThrowOutOfRange(void) const
{
    NCBI_THROW(CSeqVectorException, eOutOfRange,
               "iterator out of range: "+
               NStr::UIntToString(GetPos())+">="+
               NStr::UIntToString(x_GetSize()));
}


void CSeqVector_CI::SetCoding(TCoding coding)
{
    if ( m_Coding != coding ) {
        TSeqPos pos = GetPos();
        x_ResetCache();
        x_ResetBackup();
        m_Coding = coding;
        if ( m_Seg ) {
            x_SetPos(pos);
        }
    }
}


// returns number of gap symbols ahead including current symbol
// returns 0 if current position is not in gap
TSeqPos CSeqVector_CI::GetGapSizeForward(void) const
{
    if ( !IsInGap() ) {
        return 0;
    }
    return m_Seg.GetEndPosition() - GetPos();
}


// returns number of gap symbols before current symbol
// returns 0 if current position is not in gap
TSeqPos CSeqVector_CI::GetGapSizeBackward(void) const
{
    if ( !IsInGap() ) {
        return 0;
    }
    return GetPos() - m_Seg.GetPosition();
}


// skip current gap forward
// returns number of skipped gap symbols
// does nothing and returns 0 if current position is not in gap
TSeqPos CSeqVector_CI::SkipGap(void)
{
    if ( !IsInGap() ) {
        return 0;
    }
    TSeqPos skip = GetGapSizeForward();
    SetPos(GetPos()+skip);
    return skip;
}


// skip current gap backward
// returns number of skipped gap symbols
// does nothing and returns 0 if current position is not in gap
TSeqPos CSeqVector_CI::SkipGapBackward(void)
{
    if ( !IsInGap() ) {
        return 0;
    }
    TSeqPos skip = GetGapSizeBackward()+1;
    SetPos(GetPos()-skip);
    return skip;
}


CSeqVector_CI& CSeqVector_CI::operator=(const CSeqVector_CI& sv_it)
{
    if ( this == &sv_it ) {
        return *this;
    }

    m_Scope = sv_it.m_Scope;
    m_SeqMap = sv_it.m_SeqMap;
    m_TSE = sv_it.m_TSE;
    m_Strand = sv_it.m_Strand;
    m_Coding = sv_it.GetCoding();
    m_CaseConversion = sv_it.m_CaseConversion;
    m_Seg = sv_it.m_Seg;
    m_CachePos = sv_it.x_CachePos();
    m_Randomizer = sv_it.m_Randomizer;
    // copy cache if any
    size_t cache_size = sv_it.x_CacheSize();
    if ( cache_size ) {
        x_InitializeCache();
        m_CacheEnd = m_CacheData + cache_size;
        m_Cache = m_CacheData + sv_it.x_CacheOffset();
        memcpy(m_CacheData, sv_it.m_CacheData, cache_size);

        // copy backup cache if any
        size_t backup_size = sv_it.x_BackupSize();
        if ( backup_size ) {
            m_BackupPos = sv_it.x_BackupPos();
            m_BackupEnd = m_BackupData + backup_size;
            memcpy(m_BackupData, sv_it.m_BackupData, backup_size);
        }
        else {
            x_ResetBackup();
        }
    }
    else {
        x_ResetCache();
        x_ResetBackup();
    }
    return *this;
}


void CSeqVector_CI::x_DestroyCache(void)
{
    m_CachePos = GetPos();

    delete[] m_CacheData;
    m_Cache = m_CacheData = m_CacheEnd = 0;

    delete[] m_BackupData;
    m_BackupData = m_BackupEnd = 0;
}


void CSeqVector_CI::x_InitializeCache(void)
{
    if ( !m_Cache ) {
        m_Cache = m_CacheEnd = m_CacheData = new char[kCacheSize];
        try {
            m_BackupEnd = m_BackupData = new char[kCacheSize];
        }
        catch (...) {
            x_DestroyCache();
            throw;
        }
    }
    else {
        x_ResetCache();
    }
}


inline
void CSeqVector_CI::x_ResizeCache(size_t size)
{
    _ASSERT(size <= kCacheSize);
    if ( !m_CacheData ) {
        x_InitializeCache();
    }
    m_CacheEnd = m_CacheData + size;
    m_Cache = m_CacheData;
}


void CSeqVector_CI::x_UpdateCacheUp(TSeqPos pos)
{
    _ASSERT(pos < x_GetSize());

    TSeqPos segEnd = m_Seg.GetEndPosition();
    _ASSERT(pos >= m_Seg.GetPosition() && pos < segEnd);

    TSeqPos cache_size = min(kCacheSize, segEnd - pos);
    x_FillCache(pos, cache_size);
    m_Cache = m_CacheData;
    _ASSERT(GetPos() == pos);
}


void CSeqVector_CI::x_UpdateCacheDown(TSeqPos pos)
{
    _ASSERT(pos < x_GetSize());

    TSeqPos segStart = m_Seg.GetPosition();
    _ASSERT(pos >= segStart && pos < m_Seg.GetEndPosition());

    TSeqPos cache_offset = min(kCacheSize - 1, pos - segStart);
    x_FillCache(pos - cache_offset, cache_offset + 1);
    m_Cache = m_CacheData + cache_offset;
    _ASSERT(GetPos() == pos);
}


void CSeqVector_CI::x_FillCache(TSeqPos start, TSeqPos count)
{
    _ASSERT(m_Seg.GetType() != CSeqMap::eSeqEnd);
    _ASSERT(start >= m_Seg.GetPosition());
    _ASSERT(start < m_Seg.GetEndPosition());

    x_ResizeCache(count);

    switch ( m_Seg.GetType() ) {
    case CSeqMap::eSeqData:
    {
        const CSeq_data& data = m_Seg.GetRefData();

        CSeqVector::TCoding dataCoding = data.Which();
        CSeqVector::TCoding cacheCoding = x_GetCoding(m_Coding, dataCoding);
        bool reverse = m_Seg.GetRefMinusStrand();

        bool randomize = false;
        if (cacheCoding == CSeq_data::e_Ncbi2na  &&  m_Randomizer) {
            cacheCoding = CSeq_data::e_Ncbi4na;
            randomize = true;
        }

        const char* table = 0;
        if ( cacheCoding != dataCoding || reverse ||
             m_CaseConversion != eCaseConversion_none ) {
            table = CSeqVector::sx_GetConvertTable(dataCoding,
                                                   cacheCoding,
                                                   reverse,
                                                   m_CaseConversion);
            if ( !table && cacheCoding != dataCoding ) {
                NCBI_THROW(CSeqVectorException, eCodingError,
                           "Incompatible sequence codings");
            }
        }

        TSeqPos dataPos;
        if ( reverse ) {
            // Revert segment offset
            dataPos = m_Seg.GetRefEndPosition() -
                (start - m_Seg.GetPosition()) - count;
        }
        else {
            dataPos = m_Seg.GetRefPosition() +
                (start - m_Seg.GetPosition());
        }

        switch ( dataCoding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit_any(m_Cache, count, data.GetIupacna().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit_any(m_Cache, count, data.GetIupacaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit_any(m_Cache, count, data.GetNcbi2na().Get(), dataPos,
                            table, reverse);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit_any(m_Cache, count, data.GetNcbi4na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit_any(m_Cache, count, data.GetNcbi8na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbipna:
            NCBI_THROW(CSeqVectorException, eCodingError,
                       "Ncbipna conversion not implemented");
        case CSeq_data::e_Ncbi8aa:
            copy_8bit_any(m_Cache, count, data.GetNcbi8aa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit_any(m_Cache, count, data.GetNcbieaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbipaa:
            NCBI_THROW(CSeqVectorException, eCodingError,
                       "Ncbipaa conversion not implemented");
        case CSeq_data::e_Ncbistdaa:
            copy_8bit_any(m_Cache, count, data.GetNcbistdaa().Get(), dataPos,
                          table, reverse);
            break;
        default:
            NCBI_THROW(CSeqVectorException, eCodingError,
                       "Invalid data coding");
        }
        if ( randomize ) {
            m_Randomizer->RandomizeData(m_Cache, count, start);
        }
        break;
    }
    case CSeqMap::eSeqGap:
        if (m_Coding == CSeq_data::e_Ncbi2na  &&  m_Randomizer) {
            fill(m_Cache, m_Cache + count,
                CSeqVector::x_GetGapChar(CSeq_data::e_Ncbi4na));
            m_Randomizer->RandomizeData(m_Cache, count, start);
        }
        else {
            fill(m_Cache, m_Cache + count, CSeqVector::x_GetGapChar(m_Coding));
        }
        break;
    default:
        NCBI_THROW(CSeqVectorException, eDataError,
                   "Invalid segment type");
    }
    m_CachePos = start;
}


void CSeqVector_CI::x_SetPos(TSeqPos pos)
{
    TSeqPos size = x_GetSize();
    if ( pos >= size ) {
        if ( x_CacheSize() ) {
            // save current cache as backup
            x_SwapCache();
            x_ResetCache();
        }
        _ASSERT(x_CacheSize() == 0 && x_CacheOffset() == 0);
        m_CachePos = size;
        _ASSERT(GetPos() == size);
        return;
    }

    _ASSERT(pos - x_CachePos() >= x_CacheSize());

    // update current segment
    x_UpdateSeg(pos);

    // save current cache as backup and restore old backup
    x_SwapCache();

    // check if old backup is suitable
    TSeqPos cache_offset = pos - x_CachePos();
    TSeqPos cache_size = x_CacheSize();
    if ( cache_offset < cache_size ) {
        // can use backup
        _ASSERT(x_CacheSize() &&
                x_CachePos() >= m_Seg.GetPosition() &&
                x_CacheEndPos() <= m_Seg.GetEndPosition());
        m_Cache = m_CacheData + cache_offset;
    }
    else {
        // cannot use backup
        x_InitializeCache();
        TSeqPos old_pos = x_BackupPos();
        if ( pos < old_pos && pos >= old_pos - kCacheSize &&
             m_Seg.GetEndPosition() >= old_pos ) {
            x_UpdateCacheDown(old_pos - 1);
            cache_offset = pos - x_CachePos();
            m_Cache = m_CacheData + cache_offset;
        }
        else {
            x_UpdateCacheUp(pos);
        }
    }
    _ASSERT(x_CacheOffset() < x_CacheSize());
    _ASSERT(GetPos() == pos);
}


void CSeqVector_CI::x_UpdateSeg(TSeqPos pos)
{
    if ( m_Seg.IsInvalid() ) {
        x_InitSeg(pos);
    }
    else if ( m_Seg.GetPosition() > pos ) {
        // segment is ahead
        do {
            _ASSERT(m_Seg || m_Seg.GetPosition() == x_GetSize());
            --m_Seg;
        } while ( m_Seg.GetLength() == 0 ); // skip zero length segments
        _ASSERT(m_Seg);
        if ( m_Seg.GetPosition() > pos ) {
            // too far
            x_InitSeg(pos);
        }
    }
    else if ( m_Seg.GetEndPosition() <= pos ) {
        // segment is behind
        do {
            _ASSERT(m_Seg);
            ++m_Seg;
        } while ( m_Seg.GetLength() == 0 ); // skip zero length segments
        _ASSERT(m_Seg);
        if ( m_Seg.GetEndPosition() <= pos ) {
            // too far
            x_InitSeg(pos);
        }
    }
    _ASSERT(pos >= m_Seg.GetPosition() && pos < m_Seg.GetEndPosition());
}


void CSeqVector_CI::GetSeqData(string& buffer, TSeqPos count)
{
    buffer.erase();
    count = min(count, x_GetSize() - GetPos());
    if ( count ) {
        buffer.reserve(count);
        do {
            TCache_I cache = m_Cache;
            TCache_I cache_end = m_CacheEnd;
            TSeqPos chunk_count = min(count, TSeqPos(cache_end - cache));
            _ASSERT(chunk_count > 0);
            TCache_I chunk_end = cache + chunk_count;
            buffer.append(cache, chunk_end);
            if ( chunk_end == cache_end ) {
                x_NextCacheSeg();
            }
            else {
                m_Cache = chunk_end;
            }
            count -= chunk_count;
        } while ( count );
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_SeqMap);
    TSeqPos pos = x_CacheEndPos();
    // save current cache in backup
    _ASSERT(x_CacheSize());
    x_SwapCache();
    // update segment if needed
    while ( m_Seg.GetEndPosition() <= pos ) {
        if ( !m_Seg ) {
            // end of sequence
            _ASSERT(pos == x_GetSize());
            x_ResetCache();
            m_CachePos = pos;
            return;
        }
        ++m_Seg;
    }
    _ASSERT(m_Seg);
    // Try to re-use backup cache
    if ( pos < x_CacheEndPos() && pos >= x_CachePos() ) {
        m_Cache = m_CacheData + pos - x_CachePos();
    }
    else {
        // can not use backup cache
        x_ResetCache();
        x_UpdateCacheUp(pos);
        _ASSERT(GetPos() == pos);
        _ASSERT(x_CacheSize());
        _ASSERT(x_CachePos() == pos);
    }
}


void CSeqVector_CI::x_PrevCacheSeg()
{
    _ASSERT(m_SeqMap);
    TSeqPos pos = x_CachePos();
    if ( pos-- == 0 ) {
        // Can not go further
        NCBI_THROW(CSeqVectorException, eOutOfRange,
                   "Can not update cache: iterator out of range");
    }
    // save current cache in backup
    x_SwapCache();
    // update segment if needed
    if ( m_Seg.IsInvalid() ) {
        x_InitSeg(pos);
    }
    else {
        while ( m_Seg.GetPosition() > pos ) {
            _ASSERT(m_Seg || m_Seg.GetPosition() == x_GetSize());
            --m_Seg;
        }
    }
    _ASSERT(m_Seg);
    // Try to re-use backup cache
    if ( pos >= x_CachePos()  &&  pos < x_CacheEndPos() ) {
        m_Cache = m_CacheData + pos - x_CachePos();
    }
    else {
        // can not use backup cache
        x_ResetCache();
        x_UpdateCacheDown(pos);
        _ASSERT(GetPos() == pos);
        _ASSERT(x_CacheSize());
        _ASSERT(x_CacheEndPos() == pos+1);
    }
}


void CSeqVector_CI::x_SetRandomizer(CNcbi2naRandomizer& randomizer)
{
    TSeqPos pos = GetPos();
    x_ResetCache();
    x_ResetBackup();
    m_Randomizer.Reset(&randomizer);
    x_SetPos(pos);
}


void CSeqVector_CI::x_InitRandomizer(CRandom& random_gen)
{
    TSeqPos pos = GetPos();
    x_ResetCache();
    x_ResetBackup();
    m_Randomizer.Reset(new CNcbi2naRandomizer(random_gen));
    x_SetPos(pos);
}


void CSeqVector_CI::SetRandomizeAmbiguities(void)
{
    CRandom random_gen;
    x_InitRandomizer(random_gen);
}


void CSeqVector_CI::SetRandomizeAmbiguities(Uint4 seed)
{
    CRandom random_gen(seed);
    x_InitRandomizer(random_gen);
}


void CSeqVector_CI::SetRandomizeAmbiguities(CRandom& random_gen)
{
    x_InitRandomizer(random_gen);
}


void CSeqVector_CI::SetNoAmbiguities(void)
{
    TSeqPos pos = GetPos();
    x_ResetCache();
    x_ResetBackup();
    m_Randomizer.Reset();
    x_SetPos(pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.42  2005/04/11 15:23:23  vasilche
* Added option to change letter case in CSeqVector_CI.
*
* Revision 1.41  2005/03/28 19:23:06  vasilche
* Added restricted post-increment and post-decrement operators.
*
* Revision 1.40  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.39  2004/12/22 15:56:18  vasilche
* Added CTSE_Handle.
* Allow used TSE linking.
*
* Revision 1.38  2004/12/14 17:41:03  grichenk
* Reduced number of CSeqMap::FindResolved() methods, simplified
* BeginResolved and EndResolved. Marked old methods as deprecated.
*
* Revision 1.37  2004/10/27 16:36:28  vasilche
* Added methods for working with gaps.
*
* Revision 1.36  2004/06/14 18:30:08  grichenk
* Added ncbi2na randomizer to CSeqVector
*
* Revision 1.35  2004/06/08 13:33:49  grichenk
* Fixed randomization of gaps in ncbi2na coding
*
* Revision 1.34  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.33  2004/05/10 18:27:37  grichenk
* Check cache in x_ResizeCache()
*
* Revision 1.32  2004/04/27 14:45:01  grichenk
* Fixed warnings.
*
* Revision 1.31  2004/04/22 18:34:18  grichenk
* Added optional ambiguity randomizer for ncbi2na coding
*
* Revision 1.30  2004/04/12 16:49:16  vasilche
* Allow null scope in CSeqMap_CI and CSeqVector.
*
* Revision 1.29  2004/04/09 20:34:50  vasilche
* Fixed wrong assertion.
*
* Revision 1.28  2003/11/07 17:00:13  vasilche
* Added check for out of bounds withing CSeq_inst to avoid coredump.
*
* Revision 1.27  2003/10/08 14:16:55  vasilche
* Removed circular reference CSeqVector <-> CSeqVector_CI.
*
* Revision 1.26  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.25  2003/08/29 13:34:47  vasilche
* Rewrote CSeqVector/CSeqVector_CI code to allow better inlining.
* CSeqVector::operator[] made significantly faster.
* Added possibility to have platform dependent byte unpacking functions.
*
* Revision 1.24  2003/08/21 18:51:34  vasilche
* Fixed compilation errors on translation with reverse.
*
* Revision 1.23  2003/08/21 17:04:10  vasilche
* Fixed bug in making conversion tables.
*
* Revision 1.22  2003/08/21 13:32:04  vasilche
* Optimized CSeqVector iteration.
* Set some CSeqVector values (mol type, coding) in constructor instead of detecting them while iteration.
* Remove unsafe bit manipulations with coding.
*
* Revision 1.21  2003/08/19 18:34:11  vasilche
* Buffer length constant moved to *.cpp file for easier modification.
*
* Revision 1.20  2003/07/21 14:30:48  vasilche
* Fixed buffer destruction and exception processing.
*
* Revision 1.19  2003/07/18 20:41:48  grichenk
* Fixed memory leaks in CSeqVector_CI
*
* Revision 1.18  2003/07/18 19:42:42  vasilche
* Added x_DestroyCache() method.
*
* Revision 1.17  2003/07/01 18:00:41  vasilche
* Fixed unsigned/signed comparison.
*
* Revision 1.16  2003/06/24 19:46:43  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.15  2003/06/18 15:01:19  vasilche
* Fixed positioning of CSeqVector_CI in GetSeqData().
*
* Revision 1.14  2003/06/17 20:37:06  grichenk
* Removed _TRACE-s, fixed bug in x_GetNextSeg()
*
* Revision 1.13  2003/06/13 17:22:28  grichenk
* Check if seq-map is not null before using it
*
* Revision 1.12  2003/06/10 15:27:14  vasilche
* Fixed warning
*
* Revision 1.11  2003/06/06 15:09:43  grichenk
* One more fix for coordinates
*
* Revision 1.10  2003/06/05 20:20:22  grichenk
* Fixed bugs in assignment functions,
* fixed minor problems with coordinates.
*
* Revision 1.9  2003/06/04 13:48:56  grichenk
* Improved double-caching, fixed problem with strands.
*
* Revision 1.8  2003/06/02 21:03:59  vasilche
* Grichenko's fix of wrong data in CSeqVector.
*
* Revision 1.7  2003/06/02 16:53:35  dicuccio
* Restore file damaged by last check-in
*
* Revision 1.5  2003/05/31 01:42:51  ucko
* Avoid min(size_t, TSeqPos), as they may be different types.
*
* Revision 1.4  2003/05/30 19:30:08  vasilche
* Fixed compilation on GCC 3.
*
* Revision 1.3  2003/05/30 18:00:29  grichenk
* Fixed caching bugs in CSeqVector_CI
*
* Revision 1.2  2003/05/29 13:35:36  grichenk
* Fixed bugs in buffer fill/copy procedures.
*
* Revision 1.1  2003/05/27 19:42:24  grichenk
* Initial revision
*
*
* ===========================================================================
*/
