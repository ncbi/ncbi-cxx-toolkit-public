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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const TSeqPos kCacheSize = 16384;


template<class DstIter, class SrcCont>
void copy_8bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    copy(src, src+count, dst);
}


template<class DstIter, class SrcCont>
void copy_4bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        *dst++ = *src++ & 0x0f;
        --count;
    }
    for ( size_t count2 = count/2; count2; --count2, dst += 2, ++src ) {
        char c = *src;
        *(dst) = (c >> 4) & 0x0f;
        *(dst+1) = (c) & 0x0f;
    }
    if ( count % 2 ) {
        // remaining odd char
        *dst = (*src >> 4) & 0x0f;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    // odd chars first
    switch ( srcPos % 4 ) {
    case 1:
        *dst++ = (*src >> 4) & 0x03;
        if ( --count == 0 )
            return;
        // intentional fall through
    case 2:
        *dst++ = (*src >> 2) & 0x03;
        if ( --count == 0 )
            return;
        // intentional fall through
    case 3:
        *dst++ = (*src++) & 0x03;
        --count;
        break;
    }
    for ( size_t count4 = count / 4; count4; --count4, dst += 4, ++src ) {
        char c = *src;
        *(dst) = (c >> 6) & 0x03;
        *(dst+1) = (c >> 4) & 0x03;
        *(dst+2) = (c >> 2) & 0x03;
        *(dst+3) = (c) & 0x03;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = (*src >> 2) & 0x03;
        // intentional fall through
    case 2:
        *(dst+1) = (*src >> 4) & 0x03;
        // intentional fall through
    case 1:
        *dst = (*src >> 6) & 0x03;
        break;
    }
}


void translate(char* dst, size_t count, const char* table)
{
    for ( ; count; ++dst, --count ) {
        *dst = table[static_cast<unsigned char>(*dst)];
    }
}


// CSeqVector_CI::


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos)
    : m_Vector(&seq_vector),
      m_Coding(seq_vector.GetCoding()),
      m_CachePos(kInvalidSeqPos),
      m_Cache(0),
      m_BackupPos(kInvalidSeqPos)
{
    m_Seg = m_Vector->m_SeqMap->FindResolved(
        pos, m_Vector->m_Scope, m_Vector->m_Strand);
    x_UpdateCache(pos);
}


void CSeqVector_CI::x_UpdateCachePtr(void)
{
    m_Cache = m_CacheData.begin();
}


void CSeqVector_CI::x_ResizeCache(size_t size)
{
    m_CacheData.resize(size);
    x_UpdateCachePtr();
}


void CSeqVector_CI::x_UpdateCache(TSeqPos pos)
{
    _ASSERT(bool(m_Vector));
    // Adjust segment
    if (pos == kInvalidSeqPos)
        pos = m_CachePos;
    if (pos < m_Seg.GetPosition()  || pos >= m_Seg.GetEndPosition()) {
        m_Seg = m_Vector->m_SeqMap->FindResolved(pos,
            m_Vector->m_Scope, m_Vector->m_Strand);
    }
    TSeqPos segStart = m_Seg.GetPosition();
    TSeqPos segSize = m_Seg.GetLength();

    _ASSERT(pos >= segStart);
    _ASSERT(pos < segStart + segSize);

    if (segSize <= kCacheSize) {
        // Try to cache the whole segment
        m_CachePos = segStart;
        x_ResizeCache(segSize);
        x_FillCache(segSize);
    }
    else {
        // Calculate new cache start
        TSignedSeqPos start;
        if ( m_CachePos == kInvalidSeqPos ) {
            // Uninitialized cache
            start = max<TSignedSeqPos>(0, pos - kCacheSize / 2);
        }
        else if ( pos >= m_CachePos ) {
            // Moving forward, keep the end of the old cache
            start = pos;
        }
        else {
            // Moving backward, keep the start of the old cache
            start = pos - kCacheSize;
        }

        // Calculate new cache end, adjust start
        TSeqPos newCacheEnd;
        if ( start < TSignedSeqPos(segStart) ) {
            // Segment is too short at the beginning
            m_CachePos = segStart;
            newCacheEnd = segStart + min(segSize, kCacheSize);
        }
        else if ( start + kCacheSize > TSignedSeqPos(segStart + segSize) ) {
            // Segment is too short at the end
            m_CachePos = segStart + max(kCacheSize, segSize) - kCacheSize;
            newCacheEnd = segStart + segSize;
        }
        else {
            // Caching internal part of the segment
            m_CachePos = start;
            newCacheEnd = start + kCacheSize;
        }
        TSeqPos newCacheLen = newCacheEnd - m_CachePos;

        x_ResizeCache(newCacheLen);
        x_FillCache(newCacheLen);
    }
    m_Cache = m_CacheData.begin() + (pos - m_CachePos);
}


void CSeqVector_CI::x_FillCache(TSeqPos count)
{
    _ASSERT(m_Seg.GetType() != CSeqMap::eSeqEnd);
    _ASSERT(count <= m_CacheData.size());
    _ASSERT(m_CachePos >= m_Seg.GetPosition());
    _ASSERT(m_CachePos < m_Seg.GetEndPosition());

    count = min(m_Seg.GetLength(), count);

    switch ( m_Seg.GetType() ) {
    case CSeqMap::eSeqData:
    {
        const CSeq_data& data = m_Seg.GetRefData();
        TSeqPos dataPos = m_Seg.GetRefPosition() +
            (m_CachePos - m_Seg.GetPosition());
        CSeqVector::TCoding dataCoding = data.Which();

        CSeqVector::TCoding cacheCoding =
            m_Vector->x_GetCoding(m_Coding, dataCoding);

        switch ( dataCoding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit(m_Cache, count, data.GetIupacna().Get(), dataPos);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit(m_Cache, count, data.GetIupacaa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit(m_Cache, count, data.GetNcbi2na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit(m_Cache, count, data.GetNcbi4na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit(m_Cache, count, data.GetNcbi8na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbipna:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "Ncbipna conversion not implemented");
        case CSeq_data::e_Ncbi8aa:
            copy_8bit(m_Cache, count, data.GetNcbi8aa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit(m_Cache, count, data.GetNcbieaa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbipaa:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "Ncbipaa conversion not implemented");
        case CSeq_data::e_Ncbistdaa:
            copy_8bit(m_Cache, count, data.GetNcbistdaa().Get(), dataPos);
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "invalid data type");
        }
        if (cacheCoding != dataCoding) {
            const char* table = m_Vector->sx_GetConvertTable(dataCoding,
                                                             cacheCoding);
            if ( table ) {
                translate(&*m_Cache, count, table);
            }
            else {
                THROW1_TRACE(runtime_error,
                             "CSeqVector_CI::x_FillCache: "
                             "incompatible codings");
            }
        }
        if ( m_Seg.GetRefMinusStrand() ) {
            reverse(m_Cache, m_Cache + count);
            const char* table = m_Vector->sx_GetComplementTable(cacheCoding);
            if ( table ) {
                translate(&*m_Cache, count, table);
            }
        }
        break;
    }
    case CSeqMap::eSeqGap:
        m_Vector->x_GetCoding(m_Coding, CSeq_data::e_not_set);
        fill(m_Cache, m_Cache + count, m_Vector->x_GetGapChar(m_Coding));
        break;
    default:
        THROW1_TRACE(runtime_error,
                     "CSeqVector_CI::x_FillCache: invalid segment type");
    }
}


void CSeqVector_CI::SetPos(TSeqPos pos)
{
    pos = min(m_Vector->size() - 1, pos);
    if (pos < m_CachePos  ||  pos >= x_CacheEnd()) {
        if (!m_BackupData.empty()  &&
            pos >= m_BackupPos  &&  pos < x_BackupEnd()) {
            // Swap caches, adjust segment
            m_CacheData.swap(m_BackupData);
            swap(m_CachePos, m_BackupPos);
            x_UpdateCachePtr();
            if (m_CachePos >= m_Seg.GetEndPosition()  ||
                m_CachePos < m_Seg.GetPosition()) {
                // Update map segment
                if (m_CachePos == x_BackupEnd()) {
                    ++m_Seg;
                }
                else if (x_CacheEnd() == m_BackupPos) {
                    --m_Seg;
                }
                else {
                    m_Seg = m_Vector->m_SeqMap->FindResolved(m_CachePos,
                        m_Vector->m_Scope, m_Vector->m_Strand);
                }
                _ASSERT(pos >= m_Seg.GetPosition());
                _ASSERT(pos < m_Seg.GetEndPosition());
            }
        }
        else {
            // Can not use backup cache
            // Adjust cache
            m_CacheData = m_BackupData;
            m_CachePos = m_BackupPos;
            // Reset backup cache
            m_BackupData.clear();
            m_BackupPos = 0;
            x_UpdateCache(pos);
        }
    }
    m_Cache = m_CacheData.begin() + (pos - m_CachePos);
}


void CSeqVector_CI::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer)
{
    stop = min(stop, m_Vector->size());
    buffer.erase();
    if ( start < stop ) {
        buffer.reserve(stop-start);
        if ( m_Cache >= m_CacheData.end() ||
             start < m_CachePos  ||
             start >= x_CacheEnd() ) {
            x_UpdateCache(start);
        }
        do {
            size_t end_offset = min(m_CacheData.size(),
                                    static_cast<size_t>(stop - m_CachePos));
            TCache_I cacheEnd = m_CacheData.begin() + end_offset;
            buffer.append(m_Cache, cacheEnd);
            m_Cache = cacheEnd;
            x_NextCacheSeg();
        } while (GetPos() < stop);
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_Vector);
    TSeqPos newPos = x_CacheEnd();
    swap(m_CachePos, m_BackupPos);
    m_CacheData.swap(m_BackupData);
    if (m_CachePos != newPos) {
        // can not use backup cache
        m_CacheData.clear();
    }
    else {
        m_Cache = m_CacheData.begin();
    }
    while ((newPos >= m_Seg.GetEndPosition()
        ||  m_Seg.GetLength() == 0)
        && m_Seg.GetType() != CSeqMap::eSeqEnd) {
        ++m_Seg;
    }
    if (m_CacheData.empty()  &&  m_Seg.GetType() != CSeqMap::eSeqEnd) {
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
}


void CSeqVector_CI::x_PrevCacheSeg()
{
    _ASSERT(m_Vector);
    if (m_CachePos == 0) {
        // Can not go further
        THROW1_TRACE(runtime_error, "CSeqVector_CI::x_PrevCacheSeg: no more data");
    }
    TSeqPos newPos = m_CachePos - 1;
    swap(m_CachePos, m_BackupPos);
    m_CacheData.swap(m_BackupData);
    if (m_BackupPos != x_CacheEnd()) {
        // can not use backup cache
        m_CacheData.clear();
    }
    else {
        m_Cache = m_CacheData.end();
        --m_Cache;
    }
    while ((newPos < m_Seg.GetPosition()
        ||  m_Seg.GetLength() == 0)
        && m_Seg.GetPosition() > 0) {
        --m_Seg;
    }
    if (m_CacheData.empty()  &&  m_Seg.GetType() != CSeqMap::eSeqEnd) {
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
