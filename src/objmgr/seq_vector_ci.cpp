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

void ThrowOutOfRangeSeq_inst(size_t pos)
{
    NCBI_THROW_FMT(CSeqVectorException, eOutOfRange,
                   "reference out of range of Seq-inst data: "<<pos);
}

// CSeqVector_CI::


CSeqVector_CI::CSeqVector_CI(void)
    : m_Strand(eNa_strand_unknown),
      m_Coding(CSeq_data::e_not_set),
      m_CaseConversion(eCaseConversion_none),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(),
      m_BackupEnd(0),
      m_ScannedStart(0),
      m_ScannedEnd(0)
{
}


CSeqVector_CI::~CSeqVector_CI(void)
{
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
    : m_Strand(eNa_strand_unknown),
      m_Coding(CSeq_data::e_not_set),
      m_CaseConversion(eCaseConversion_none),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(),
      m_BackupEnd(0),
      m_Randomizer(sv_it.m_Randomizer),
      m_ScannedStart(0),
      m_ScannedEnd(0)
{
    *this = sv_it;
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
      m_CacheData(),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(),
      m_BackupEnd(0),
      m_Randomizer(seq_vector.m_Randomizer),
      m_ScannedStart(0),
      m_ScannedEnd(0)
{
    x_SetPos(pos);
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos,
                             ECaseConversion case_cvt)
    : m_Scope(seq_vector.m_Scope),
      m_SeqMap(seq_vector.m_SeqMap),
      m_TSE(seq_vector.m_TSE),
      m_Strand(seq_vector.m_Strand),
      m_Coding(seq_vector.m_Coding),
      m_CaseConversion(case_cvt),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(),
      m_BackupEnd(0),
      m_Randomizer(seq_vector.m_Randomizer),
      m_ScannedStart(0),
      m_ScannedEnd(0)
{
    x_SetPos(pos);
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, ENa_strand strand,
                             TSeqPos pos, ECaseConversion case_cvt)
    : m_Scope(seq_vector.m_Scope),
      m_SeqMap(seq_vector.m_SeqMap),
      m_TSE(seq_vector.m_TSE),
      m_Strand(strand),
      m_Coding(seq_vector.m_Coding),
      m_CaseConversion(case_cvt),
      m_Cache(0),
      m_CachePos(0),
      m_CacheData(),
      m_CacheEnd(0),
      m_BackupPos(0),
      m_BackupData(),
      m_BackupEnd(0),
      m_Randomizer(seq_vector.m_Randomizer),
      m_ScannedStart(0),
      m_ScannedEnd(0)
{
    x_SetPos(pos);
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
    m_ScannedStart = m_ScannedEnd = 0;
}


inline
TSeqPos CSeqVector_CI::x_GetSize(void) const
{
    return m_SeqMap->GetLength(m_Scope.GetScopeOrNull());
}


static const TSeqPos kMaxPreloadBases = 10*1000*1000;


bool CSeqVector_CI::CanGetRange(TSeqPos start, TSeqPos stop)
{
    try {
        if ( stop < start ) {
            return false;
        }
        SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
        sel.SetStrand(m_Strand).SetRange(start, stop-start);
        sel.SetLinkUsedTSE(m_TSE).SetLinkUsedTSE(m_UsedTSEs);
        if ( !m_SeqMap->CanResolveRange(m_Scope.GetScopeOrNull(), sel) ) {
            return false;
        }
        if ( start > m_ScannedEnd || stop < m_ScannedStart ) {
            m_ScannedStart = start;
            m_ScannedEnd = stop;
        }
        else {
            m_ScannedStart = min(m_ScannedStart, start);
            m_ScannedEnd = max(m_ScannedEnd, stop);
        }
        return true;
    }
    catch ( exception& /*ignored*/ ) {
        return false;
    }
}


void CSeqVector_CI::x_CheckForward(void)
{
    TSeqPos scanned = m_ScannedEnd - m_ScannedStart;
    TSeqPos more = x_GetSize() - m_ScannedEnd;
    TSeqPos check = min(min(scanned, more), kMaxPreloadBases);
    if ( check > 0 ) {
        CanGetRange(m_ScannedEnd, m_ScannedEnd+check);
    }
}


void CSeqVector_CI::x_CheckBackward(void)
{
    TSeqPos scanned = m_ScannedEnd - m_ScannedStart;
    TSeqPos more = m_ScannedStart;
    TSeqPos check = min(min(scanned, more), kMaxPreloadBases);
    if ( check > 0 ) {
        CanGetRange(m_ScannedStart-check, m_ScannedStart);
    }
}


inline
void CSeqVector_CI::x_InitSeg(TSeqPos pos)
{
    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
    sel.SetStrand(m_Strand).SetLinkUsedTSE(m_TSE);
    if ( pos == m_ScannedEnd ) {
        x_CheckForward();
    }
    else if ( pos < m_ScannedStart || pos > m_ScannedEnd ) {
        m_ScannedStart = m_ScannedEnd = pos;
    }
    m_Seg = CSeqMap_CI(m_SeqMap, m_Scope.GetScopeOrNull(), sel, pos);
    m_ScannedStart = min(m_ScannedStart, m_Seg.GetPosition());
    m_ScannedEnd = max(m_ScannedEnd, m_Seg.GetEndPosition());
}


inline
void CSeqVector_CI::x_IncSeg(void)
{
    if ( m_Seg.GetEndPosition() == m_ScannedEnd ) {
        x_CheckForward();
    }
    ++m_Seg;
    m_ScannedEnd = max(m_ScannedEnd, m_Seg.GetEndPosition());
}


inline
void CSeqVector_CI::x_DecSeg(void)
{
    if ( m_Seg.GetPosition() == m_ScannedStart ) {
        x_CheckBackward();
    }
    --m_Seg;
    m_ScannedStart = min(m_ScannedStart, m_Seg.GetPosition());
}


void CSeqVector_CI::x_ThrowOutOfRange(void) const
{
    NCBI_THROW_FMT(CSeqVectorException, eOutOfRange,
                   "iterator out of range: "<<GetPos()<<">="<<x_GetSize());
}


void CSeqVector_CI::SetCoding(TCoding coding)
{
    if ( m_Coding != coding ) {
        TSeqPos pos = GetPos();
        m_Coding = coding;
        x_ResetBackup();
        if ( x_CacheSize() ) {
            x_ResetCache();
            if ( m_Seg ) {
                x_SetPos(pos);
            }
        }
    }
}


void CSeqVector_CI::SetStrand(ENa_strand strand)
{
    if ( IsReverse(m_Strand) == IsReverse(strand) ) {
        m_Strand = strand;
        return;
    }

    TSeqPos pos = GetPos();
    m_Strand = strand;
    x_ResetBackup();
    if ( x_CacheSize() ) {
        x_ResetCache();
        if ( m_Seg ) {
            m_Seg = CSeqMap_CI();
            x_SetPos(pos);
        }
    }
}


// returns gap Seq-data object ref
// returns null if it's not a gap or an unspecified gap
CConstRef<CSeq_literal> CSeqVector_CI::GetGapSeq_literal(void) const
{
    if ( !IsInGap() ) {
        return null;
    }
    return m_Seg.GetRefGapLiteral();
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


// return true if there is zero-length gap before current position
// it might happen only if current position is at the beginning of buffer
bool CSeqVector_CI::HasZeroGapBefore(void)
{
    if ( x_CacheOffset() != 0 ) {
        return false;
    }
    TSeqPos pos = GetPos();
    if ( IsReverse(m_Strand) ) {
        pos = x_GetSize() - pos;
    }
    return m_SeqMap->HasZeroGapAt(pos, m_Scope.GetScopeOrNull());
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
    m_ScannedStart = sv_it.m_ScannedStart;
    m_ScannedEnd = sv_it.m_ScannedEnd;
    // copy cache if any
    size_t cache_size = sv_it.x_CacheSize();
    if ( cache_size ) {
        x_InitializeCache();
        m_CacheEnd = m_CacheData.get() + cache_size;
        m_Cache = m_CacheData.get() + sv_it.x_CacheOffset();
        memcpy(m_CacheData.get(), sv_it.m_CacheData.get(), cache_size);

        // copy backup cache if any
        size_t backup_size = sv_it.x_BackupSize();
        if ( backup_size ) {
            m_BackupPos = sv_it.x_BackupPos();
            m_BackupEnd = m_BackupData.get() + backup_size;
            memcpy(m_BackupData.get(), sv_it.m_BackupData.get(), backup_size);
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


void CSeqVector_CI::x_InitializeCache(void)
{
    if ( !m_Cache ) {
        m_CacheData.reset(new char[kCacheSize]);
        m_BackupData.reset(new char[kCacheSize]);
        m_BackupEnd = m_BackupData.get();
        m_Cache = m_CacheEnd = m_CacheData.get();
    }
    else {
        x_ResetCache();
    }
}


inline
void CSeqVector_CI::x_ResizeCache(size_t size)
{
    _ASSERT(size <= kCacheSize);
    if ( !m_CacheData.get() ) {
        x_InitializeCache();
    }
    m_Cache = m_CacheData.get();
    m_CacheEnd = m_CacheData.get() + size;
}


void CSeqVector_CI::x_UpdateCacheUp(TSeqPos pos)
{
    _ASSERT(pos < x_GetSize());

    TSeqPos segEnd = m_Seg.GetEndPosition();
    _ASSERT(pos >= m_Seg.GetPosition() && pos < segEnd);

    TSeqPos cache_size = min(kCacheSize, segEnd - pos);
    x_FillCache(pos, cache_size);
    m_Cache = m_CacheData.get();
    _ASSERT(GetPos() == pos);
}


void CSeqVector_CI::x_UpdateCacheDown(TSeqPos pos)
{
    _ASSERT(pos < x_GetSize());

    TSeqPos segStart = m_Seg.GetPosition();
    _ASSERT(pos >= segStart && pos < m_Seg.GetEndPosition());

    TSeqPos cache_offset = min(kCacheSize - 1, pos - segStart);
    x_FillCache(pos - cache_offset, cache_offset + 1);
    m_Cache = m_CacheData.get() + cache_offset;
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
        if ( data.IsGap() && m_Seg.GetType() == CSeqMap::eSeqGap ) {
            // workaround for erroneously split gap Seq-data
            x_FillCache(start, count);
            return;
        }
        
        TCoding dataCoding = data.Which();
        TCoding cacheCoding = x_GetCoding(m_Coding, dataCoding);
        bool reverse = m_Seg.GetRefMinusStrand();

        bool randomize = false;
        if ( cacheCoding != dataCoding &&
             cacheCoding == CSeq_data::e_Ncbi2na &&
             m_Randomizer) {
            cacheCoding = CSeq_data::e_Ncbi4na;
            randomize = true;
        }

        const char* table = 0;
        if ( cacheCoding != dataCoding || reverse ||
             m_CaseConversion != eCaseConversion_none ) {
            table = sx_GetConvertTable(dataCoding, cacheCoding,
                                       reverse, m_CaseConversion);
            if ( !table && cacheCoding != dataCoding ) {
                NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                               "Incompatible sequence codings: "<<
                               dataCoding<<" -> "<<cacheCoding);
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
            NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                           "Invalid data coding: "<<dataCoding);
        }
        if ( randomize ) {
            m_Randomizer->RandomizeData(m_Cache, count, start);
        }
        break;
    }
    case CSeqMap::eSeqGap:
        if (m_Coding == CSeq_data::e_Ncbi2na  &&  m_Randomizer) {
            fill_n(m_Cache, count,
                   sx_GetGapChar(CSeq_data::e_Ncbi4na, eCaseConversion_none));
            m_Randomizer->RandomizeData(m_Cache, count, start);
        }
        else {
            fill_n(m_Cache, count, GetGapChar());
        }
        break;
    default:
        NCBI_THROW_FMT(CSeqVectorException, eDataError,
                       "Invalid segment type: "<<m_Seg.GetType());
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
        m_Cache = m_CacheData.get() + cache_offset;
    }
    else {
        // cannot use backup
        x_InitializeCache();
        TSeqPos old_pos = x_BackupPos();
        if ( pos < old_pos && pos >= old_pos - kCacheSize &&
             m_Seg.GetEndPosition() >= old_pos ) {
            x_UpdateCacheDown(old_pos - 1);
            cache_offset = pos - x_CachePos();
            m_Cache = m_CacheData.get() + cache_offset;
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
            x_DecSeg();
        } while ( m_Seg && m_Seg.GetLength() == 0 ); // skip 0 length segments
        if ( !m_Seg || m_Seg.GetPosition() > pos ) {
            // too far
            x_InitSeg(pos);
        }
    }
    else if ( m_Seg.GetEndPosition() <= pos ) {
        // segment is behind
        do {
            x_IncSeg();
        } while ( m_Seg && m_Seg.GetLength() == 0 ); // skip 0 length segments
        if ( !m_Seg || m_Seg.GetEndPosition() <= pos ) {
            // too far
            x_InitSeg(pos);
        }
    }
    if ( !m_Seg && pos == x_GetSize() ) {
        // it's ok to position to the very end
        return;
    }
    if ( !m_Seg || pos<m_Seg.GetPosition() || pos>=m_Seg.GetEndPosition() ) {
        NCBI_THROW_FMT(CSeqVectorException, eDataError,
                       "CSeqVector_CI: cannot locate segment at "<<pos);
    }
    _ASSERT(m_Seg && pos>=m_Seg.GetPosition() && pos<m_Seg.GetEndPosition());
}


void CSeqVector_CI::GetSeqData(string& buffer, TSeqPos count)
{
    buffer.erase();
    TSeqPos pos = GetPos();
    _ASSERT(pos <= x_GetSize());
    count = min(count, x_GetSize() - pos);
    if ( !count ) {
        return;
    }

    if ( m_TSE && !CanGetRange(pos, pos+count) ) {
        NCBI_THROW_FMT(CSeqVectorException, eDataError,
                       "CSeqVector_CI::GetSeqData: "
                       "cannot get seq-data in range: "
                       <<pos<<"-"<<pos+count);
    }
    
    buffer.reserve(count);
    while ( count ) {
        TCache_I cache = m_Cache;
        TCache_I cache_end = m_CacheEnd;
        TSeqPos chunk_count = min(count, TSeqPos(cache_end - cache));
        _ASSERT(chunk_count > 0);
        TCache_I chunk_end = cache + chunk_count;
        buffer.append(cache, chunk_end);
        count -= chunk_count;
        //if ( count == 0 ) break;
        if ( chunk_end == cache_end ) {
            x_NextCacheSeg();
        }
        else {
            m_Cache = chunk_end;
        }
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_SeqMap);
    TSeqPos pos = x_CacheEndPos();
    TSeqPos size = x_GetSize();
    if ( pos >= size ) {
        if ( x_CachePos() < pos ) {
            x_SwapCache();
            x_ResetCache();
            m_CachePos = pos;
            return;
        }
        else {
            // Can not go further
            NCBI_THROW(CSeqVectorException, eOutOfRange,
                       "Can not update cache: iterator beyond end");
        }
    }
    // save current cache in backup
    _ASSERT(x_CacheSize());
    x_SwapCache();
    // update segment if needed
    x_UpdateSeg(pos);
    if ( !m_Seg ) {
        // end of sequence
        NCBI_THROW_FMT(CSeqVectorException, eDataError,
                       "CSeqVector_CI: invalid sequence length: "
                       <<pos<<" <> "<<size);
    }
    // Try to re-use backup cache
    if ( pos < x_CacheEndPos() && pos >= x_CachePos() ) {
        m_Cache = m_CacheData.get() + pos - x_CachePos();
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
                   "Can not update cache: iterator beyond start");
    }
    TSeqPos size = x_GetSize();
    // save current cache in backup
    x_SwapCache();
    // update segment if needed
    if ( m_Seg.IsInvalid() ) {
        x_InitSeg(pos);
    }
    else {
        while ( m_Seg && m_Seg.GetPosition() > pos ) {
            x_DecSeg();
        }
    }
    if ( !m_Seg ) {
        NCBI_THROW_FMT(CSeqVectorException, eDataError,
                       "CSeqVector_CI: invalid sequence length: "
                       <<pos<<" <> "<<size);
    }
    // Try to re-use backup cache
    if ( pos >= x_CachePos()  &&  pos < x_CacheEndPos() ) {
        m_Cache = m_CacheData.get() + pos - x_CachePos();
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


void CSeqVector_CI::SetRandomizeAmbiguities(CRef<INcbi2naRandomizer> randomizer)
{
    if ( randomizer != m_Randomizer ) {
        TSeqPos pos = GetPos();
        m_Randomizer = randomizer;
        x_ResetBackup();
        if ( x_CacheSize() ) {
            x_ResetCache();
            if ( m_Seg ) {
                x_SetPos(pos);
            }
        }
    }
}


void CSeqVector_CI::x_InitRandomizer(CRandom& random_gen)
{
    CRef<INcbi2naRandomizer> randomizer(new CNcbi2naRandomizer(random_gen));
    SetRandomizeAmbiguities(randomizer);
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
    SetRandomizeAmbiguities(null);
}


END_SCOPE(objects)
END_NCBI_SCOPE
