#define CVS
#undef CVS
#ifdef CVS
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const TSeqPos kCacheSize = 4096;


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
        *dst++ = (*src++   ) & 0x03;
        --count;
        break;
    }
    for ( size_t count4 = count / 4; count4; --count4, dst += 4, ++src ) {
        char c = *src;
        *(dst  ) = (c >> 6) & 0x03;
        *(dst+1) = (c >> 4) & 0x03;
        *(dst+2) = (c >> 2) & 0x03;
        *(dst+3) = (c     ) & 0x03;
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
        *(dst  ) = (*src >> 6) & 0x03;
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


CSeqVector_CI::CSeqVector_CI(void)
    : m_Vector(0),
      m_Coding(CSeq_data::e_not_set)
{
    x_InitializeCache();
}


CSeqVector_CI::~CSeqVector_CI(void)
{
    x_DestroyCache();
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
{
    x_InitializeCache();
    try {
        *this = sv_it;
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


CSeqVector_CI& CSeqVector_CI::operator=(const CSeqVector_CI& sv_it)
{
    if (this == &sv_it)
        return *this;
    m_Vector = sv_it.m_Vector;
    m_Coding = sv_it.m_Coding;
    m_Seg = sv_it.m_Seg;
    m_CachePos = sv_it.m_CachePos;
    m_CacheEnd = m_CacheData + (sv_it.m_CacheEnd - sv_it.m_CacheData);
    m_Cache = m_CacheData + (sv_it.m_Cache - sv_it.m_CacheData);
    if (sv_it.m_CacheEnd != sv_it.m_CacheData) {
        memcpy(m_CacheData, sv_it.m_CacheData, m_CacheEnd - m_CacheData);
    }
    m_BackupPos = sv_it.m_BackupPos;
    m_BackupEnd = m_BackupData + (sv_it.m_BackupEnd - sv_it.m_BackupData);
    if (sv_it.m_BackupEnd != sv_it.m_BackupData) {
        memcpy(m_BackupData, sv_it.m_BackupData, m_BackupEnd - m_BackupData);
    }
    return *this;
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos)
    : m_Vector(&seq_vector),
      m_Coding(seq_vector.GetCoding())
{
    x_InitializeCache();
    try {
        if (pos >= m_Vector->size()) {
            pos = m_Vector->size();
        }
        // Initialize segment iterator
        m_Seg = m_Vector->x_GetSeqMap().FindResolved(
            pos, m_Vector->m_Scope, m_Vector->m_Strand);
        x_UpdateCache(pos);
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


void CSeqVector_CI::x_DestroyCache(void)
{
    m_CachePos = m_BackupPos = kInvalidSeqPos;

    delete[] m_CacheData;
    m_CacheData = m_CacheEnd = m_Cache = 0;

    delete[] m_BackupData;
    m_BackupData = m_BackupEnd = 0;
}


void CSeqVector_CI::x_InitializeCache(void)
{
    m_CachePos = m_BackupPos = kInvalidSeqPos;
    m_CacheData = m_CacheEnd = m_Cache = 0;
    m_BackupData = m_BackupEnd = 0;

    m_CacheData = new char[kCacheSize];
    try {
        m_BackupData = new char[kCacheSize];
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
    m_Cache = m_CacheEnd = m_CacheData;
    m_BackupEnd = m_BackupData;
}


void CSeqVector_CI::x_ResizeCache(size_t size)
{
    m_CacheEnd = m_CacheData + size;
    m_Cache = m_CacheData;
}


void CSeqVector_CI::x_UpdateCache(TSeqPos pos)
{
    _ASSERT(bool(m_Vector));
    // Adjust segment
    if (pos == kInvalidSeqPos)
        pos = m_CachePos;
    if (pos < m_Seg.GetPosition()  || pos >= m_Seg.GetEndPosition()) {
        m_Seg = m_Vector->x_GetSeqMap().FindResolved(pos,
            m_Vector->m_Scope, m_Vector->m_Strand);
    }
    TSeqPos segStart = m_Seg.GetPosition();
    TSeqPos segSize = m_Seg.GetLength();

    if ( segSize == 0 ) {
        // Empty seq-vector, nothing to cache
        x_ClearCache();
        return;
    }

    _ASSERT(pos >= segStart);
    _ASSERT(pos < segStart + segSize);

    if (segSize <= kCacheSize) {
        // Try to cache the whole segment
        x_FillCache(segStart, segSize);
    }
    else {
        // Calculate new cache start
        TSignedSeqPos newCachePos;
        if ( m_CachePos == kInvalidSeqPos  ||
             pos >= m_CachePos ) {
            // Uninitialized cache or moving forward
            newCachePos = pos;
        }
        else {
            // Moving backward, end with pos
            newCachePos = pos - (kCacheSize - 1);
            if ( newCachePos < TSignedSeqPos(segStart) ) {
                // Segment is too short at the beginning
                newCachePos = segStart;
            }
        }

        // Calculate new cache end, adjust start
        TSeqPos newCacheEnd = newCachePos + kCacheSize;
        if ( newCacheEnd > segStart + segSize ) {
            // Segment is too short at the end
            newCachePos = segStart + max(kCacheSize, segSize) - kCacheSize;
            newCacheEnd = segStart + segSize;
        }
        TSeqPos newCacheLen = newCacheEnd - newCachePos;

        x_FillCache(newCachePos, newCacheLen);
    }
    // Adjust current cache position
    m_Cache = m_CacheData + (pos - m_CachePos);
}


void CSeqVector_CI::x_FillCache(TSeqPos start, TSeqPos count)
{
    _ASSERT(m_Seg.GetType() != CSeqMap::eSeqEnd);
    _ASSERT(start >= m_Seg.GetPosition());
    _ASSERT(start < m_Seg.GetEndPosition());

    x_ResizeCache(count);
    _ASSERT(m_CacheData + count <= m_CacheEnd);

    switch ( m_Seg.GetType() ) {
    case CSeqMap::eSeqData:
    {
        const CSeq_data& data = m_Seg.GetRefData();
        TSeqPos dataPos;
        if ( m_Seg.GetRefMinusStrand() ) {
            // Revert segment offset
            dataPos = m_Seg.GetRefEndPosition() -
                (start - m_Seg.GetPosition()) - count;
        }
        else {
            dataPos = m_Seg.GetRefPosition() +
                (start - m_Seg.GetPosition());
        }
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
                translate(m_Cache, count, table);
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
                translate(m_Cache, count, table);
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
    m_CachePos = start;
}


void CSeqVector_CI::x_SetPos(TSeqPos pos)
{
    pos = min(m_Vector->size() - 1, pos);
    // If vector size is 0 and pos is kInvalidSqePos, the result will be
    // kInvalidSeqPos, which is not good.
    if (pos == kInvalidSeqPos) {
        pos = 0;
    }

    if (pos < m_CachePos  ||  pos >= x_CacheEnd()) {
        // Swap caches
        x_SwapCache();

        // Check if restored cache is not empty and contains "pos"
        if (m_CacheEnd != m_CacheData  &&
            pos >= m_CachePos  &&  pos < x_CacheEnd()) {
            if (m_CachePos >= m_Seg.GetEndPosition()  ||
                m_CachePos < m_Seg.GetPosition()) {
                // Update map segment
                if (m_CachePos == x_BackupEnd()) {
                    // Move forward, skip zero-length segments
                    do {
                        ++m_Seg;
                    } while (m_Seg  &&  m_Seg.GetLength() == 0);
                }
                else if (x_CacheEnd() == m_BackupPos) {
                    // Move backward, skip zero-length segments
                    do {
                        --m_Seg;
                    } while (m_Seg.GetPosition() > 0
                        &&  m_Seg.GetLength() == 0);
                }
                else {
                    m_Seg = m_Vector->x_GetSeqMap().FindResolved(m_CachePos,
                        m_Vector->m_Scope, m_Vector->m_Strand);
                }
                _ASSERT(pos >= m_Seg.GetPosition());
                _ASSERT(pos < m_Seg.GetEndPosition());
            }
        }
        else {
            // Can not use backup cache -- reset and refill
            x_ClearCache();
            x_UpdateCache(pos);
        }
    }
    m_Cache = m_CacheData + (pos - m_CachePos);
}


void CSeqVector_CI::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer)
{
    stop = min(stop, m_Vector->size());
    buffer.erase();
    if ( start < stop ) {
        buffer.reserve(stop - start);
        SetPos(start);
        do {
            size_t end_offset = min<size_t>(m_CacheEnd - m_CacheData,
                                    stop - m_CachePos);
            TCache_I cacheEnd = m_CacheData + end_offset;
            buffer.append(m_Cache, cacheEnd);
            m_Cache = cacheEnd;
            if (m_Cache >= m_CacheEnd) {
                x_NextCacheSeg();
            }
        } while (GetPos() < stop);
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_Vector);
    TSeqPos newPos = x_CacheEnd();
    x_SwapCache();
    while (newPos >= m_Seg.GetEndPosition()) {
        if ( !m_Seg ) {
            x_ClearCache();
            m_CachePos = m_Seg.GetPosition();
            return;
        }
        ++m_Seg;
    }
    // Try to re-use backup cache
    if (newPos < m_CachePos  ||  newPos >= x_CacheEnd()) {
        // can not use backup cache
        x_ClearCache();
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
    else {
        m_Cache = m_CacheData + newPos - m_CachePos;
    }
}


void CSeqVector_CI::x_PrevCacheSeg()
{
    _ASSERT(m_Vector);
    if (m_CachePos == 0) {
        // Can not go further
        THROW1_TRACE(runtime_error,
                     "CSeqVector_CI::x_PrevCacheSeg: no more data");
    }
    TSeqPos newPos = m_CachePos - 1;
    x_SwapCache();
    while (m_Seg.GetPosition() > newPos) {
        --m_Seg;
    }
    // Try to re-use backup cache
    if (newPos < m_CachePos  ||  newPos >= x_CacheEnd()) {
        // can not use backup cache
        x_ClearCache();
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
    else {
        m_Cache = m_CacheData + newPos - m_CachePos;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
#else
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const TSeqPos kCacheSize = 1024;

template<class DstIter, class SrcCont>
inline
void copy_8bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    copy(src, src+count, dst);
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    for ( DstIter dst_end(dst + count); dst != dst_end; ++src, ++dst ) {
        *dst = table[static_cast<unsigned char>(*src)];
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src =
        srcCont.begin() + (srcPos + count - 1);
    for ( DstIter dst_end(dst + count); dst != dst_end; --src, ++dst ) {
        *dst = *src;
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    typename SrcCont::const_iterator src =
        srcCont.begin() + (srcPos + count - 1);
    for ( DstIter end(dst + count); dst != end; --src, ++dst ) {
        *dst = table[static_cast<unsigned char>(*src)];
    }
}


template<class DstIter, class SrcCont>
void copy_4bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src++;
        *(dst++) = (c     ) & 0x0f;
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, ++src ) {
        char c = *src;
        *(dst  ) = (c >> 4) & 0x0f;
        *(dst+1) = (c     ) & 0x0f;
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = (c >> 4) & 0x0f;
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src++;
        *(dst++) = table[(c    ) & 0x0f];
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, ++src ) {
        char c = *src;
        *(dst  ) = table[(c >> 4) & 0x0f];
        *(dst+1) = table[(c     ) & 0x0f];
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = table[(c >> 4) & 0x0f];
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + (srcPos + count - 1) / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src--;
        *(dst++) = (c >> 4) & 0x0f;
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, --src ) {
        char c = *src;
        *(dst  ) = (c     ) & 0x0f;
        *(dst+1) = (c >> 4) & 0x0f;
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = (c     ) & 0x0f;
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + (srcPos + count - 1) / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src--;
        *(dst++) = table[(c >> 4) & 0x0f];
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, --src ) {
        char c = *src;
        *(dst  ) = table[(c     ) & 0x0f];
        *(dst+1) = table[(c >> 4) & 0x0f];
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = table[(c     ) & 0x0f];
    }
}


template<class DstIter, class SrcCont>
void copy_2bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    //cp2.count += count;
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 1:
            *(dst++) = (c >> 4) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 2:
            *(dst++) = (c >> 2) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 3:
            *(dst++) = (c     ) & 0x03;
            ++src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, ++src ) {
        char c3 = *src;
        char c0 = c3 >> 6;
        char c1 = c3 >> 4;
        char c2 = c3 >> 2;
        c0 &= 0x03;
        c1 &= 0x03;
        c2 &= 0x03;
        c3 &= 0x03;
        *(dst  ) = c0;
        *(dst+1) = c1;
        *(dst+2) = c2;
        *(dst+3) = c3;
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
        *(dst  ) = (*src >> 6) & 0x03;
        break;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    //cp2.count_tbl += count;
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 1:
            *(dst++) = table[(c >> 4) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall through 
        case 2:
            *(dst++) = table[(c >> 2) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall through 
        case 3:
            *(dst++) = table[(c     ) & 0x03];
            ++src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, ++src ) {
        char c3 = *src;
        char c0 = c3 >> 6;
        char c1 = c3 >> 4;
        char c2 = c3 >> 2;
        c0 = table[c0 & 0x03];
        c1 = table[c1 & 0x03];
        *(dst  ) = c0;
        c2 = table[c2 & 0x03];
        *(dst+1) = c1;
        c3 = table[c3 & 0x03];
        *(dst+2) = c2;
        *(dst+3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = table[(*src >> 2) & 0x03];
        // intentional fall through
    case 2:
        *(dst+1) = table[(*src >> 4) & 0x03];
        // intentional fall through
    case 1:
        *(dst  ) = table[(*src >> 6) & 0x03];
        break;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    //cp2.count_rev += count;
    dst += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 1:
            *(dst--) = (c >> 4) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 2:
            *(dst--) = (c >> 2) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 3:
            *(dst--) = (c    ) & 0x03;
            ++src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst - (count & ~3); dst != end; dst -= 4, ++src ) {
        char c3 = *src;
        char c0 = c3 >> 6;
        char c1 = c3 >> 4;
        char c2 = c3 >> 2;
        c0 &= 0x03;
        c1 &= 0x03;
        c2 &= 0x03;
        c3 &= 0x03;
        *(dst  ) = c0;
        *(dst-1) = c1;
        *(dst-2) = c2;
        *(dst-3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst-2) = (*src >> 2) & 0x03;
        // intentional fall through
    case 2:
        *(dst-1) = (*src >> 4) & 0x03;
        // intentional fall through
    case 1:
        *(dst  ) = (*src >> 6) & 0x03;
        break;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    //cp2.count_tbl_rev += count;
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first  
        char c = *src;
        switch ( srcPos % 4 ) {
        case 2:
            *(dst++) = table[(c >> 2) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall thro    ugh
        case 1:
            *(dst++) = table[(c >> 4) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall thro    ugh
        case 0:
            *(dst++) = table[(c >> 6) & 0x03];
            --src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, --src ) {
        char c0 = *src;
        char c1 = c0 >> 2;
        char c2 = c0 >> 4;
        char c3 = c0 >> 6;
        c0 = table[c0 & 0x03];
        c1 = table[c1 & 0x03];
        *(dst  ) = c0;
        c2 = table[c2 & 0x03];
        *(dst+1) = c1;
        c3 = table[c3 & 0x03];
        *(dst+2) = c2;
        *(dst+3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = table[(*src >> 4) & 0x03];
        // intentional fall through
    case 2:
        *(dst+1) = table[(*src >> 2) & 0x03];
        // intentional fall through
    case 1:
        *(dst  ) = table[(*src     ) & 0x03];
        break;
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
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
    : m_Vector(0),
      m_Coding(CSeq_data::e_not_set)
{
    x_InitializeCache();
}


CSeqVector_CI::~CSeqVector_CI(void)
{
    x_DestroyCache();
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
{
    x_InitializeCache();
    try {
        *this = sv_it;
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


CSeqVector_CI& CSeqVector_CI::operator=(const CSeqVector_CI& sv_it)
{
    if (this == &sv_it)
        return *this;
    m_Vector = sv_it.m_Vector;
    m_Coding = sv_it.m_Coding;
    m_Seg = sv_it.m_Seg;
    m_CachePos = sv_it.m_CachePos;
    m_CacheEnd = m_CacheData + (sv_it.m_CacheEnd - sv_it.m_CacheData);
    m_Cache = m_CacheData + (sv_it.m_Cache - sv_it.m_CacheData);
    if (sv_it.m_CacheEnd != sv_it.m_CacheData) {
        memcpy(m_CacheData, sv_it.m_CacheData, m_CacheEnd - m_CacheData);
    }
    m_BackupPos = sv_it.m_BackupPos;
    m_BackupEnd = m_BackupData + (sv_it.m_BackupEnd - sv_it.m_BackupData);
    if (sv_it.m_BackupEnd != sv_it.m_BackupData) {
        memcpy(m_BackupData, sv_it.m_BackupData, m_BackupEnd - m_BackupData);
    }
    return *this;
}


CSeqVector_CI::CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos)
    : m_Vector(&seq_vector),
      m_Coding(seq_vector.GetCoding())
{
    x_InitializeCache();
    try {
        if (pos >= m_Vector->size()) {
            pos = m_Vector->size();
        }
        // Initialize segment iterator
        m_Seg = m_Vector->x_GetSeqMap().FindResolved(
            pos, m_Vector->m_Scope, m_Vector->m_Strand);
        x_UpdateCache(pos);
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
}


void CSeqVector_CI::x_DestroyCache(void)
{
    m_CachePos = m_BackupPos = kInvalidSeqPos;

    delete[] m_CacheData;
    m_CacheData = m_CacheEnd = m_Cache = 0;

    delete[] m_BackupData;
    m_BackupData = m_BackupEnd = 0;
}


void CSeqVector_CI::x_InitializeCache(void)
{
    m_CachePos = m_BackupPos = kInvalidSeqPos;
    m_CacheData = m_CacheEnd = m_Cache = 0;
    m_BackupData = m_BackupEnd = 0;

    m_CacheData = new char[kCacheSize];
    try {
        m_BackupData = new char[kCacheSize];
    }
    catch (...) {
        x_DestroyCache();
        throw;
    }
    m_Cache = m_CacheEnd = m_CacheData;
    m_BackupEnd = m_BackupData;
}


void CSeqVector_CI::x_ResizeCache(size_t size)
{
    m_CacheEnd = m_CacheData + size;
    m_Cache = m_CacheData;
}


void CSeqVector_CI::x_UpdateCache(TSeqPos pos)
{
    _ASSERT(bool(m_Vector));
    // Adjust segment
    if (pos == kInvalidSeqPos)
        pos = m_CachePos;
    if (pos < m_Seg.GetPosition()  || pos >= m_Seg.GetEndPosition()) {
        m_Seg = m_Vector->x_GetSeqMap().FindResolved(pos,
            m_Vector->m_Scope, m_Vector->m_Strand);
    }
    TSeqPos segStart = m_Seg.GetPosition();
    TSeqPos segSize = m_Seg.GetLength();

    if ( segSize == 0 ) {
        // Empty seq-vector, nothing to cache
        x_ClearCache();
        return;
    }

    _ASSERT(pos >= segStart);
    _ASSERT(pos < segStart + segSize);

    if (segSize <= kCacheSize) {
        // Try to cache the whole segment
        x_FillCache(segStart, segSize);
    }
    else {
        // Calculate new cache start
        TSignedSeqPos newCachePos;
        if ( m_CachePos == kInvalidSeqPos  ||
             pos >= m_CachePos ) {
            // Uninitialized cache or moving forward
            newCachePos = pos;
        }
        else {
            // Moving backward, end with pos
            newCachePos = pos - (kCacheSize - 1);
            if ( newCachePos < TSignedSeqPos(segStart) ) {
                // Segment is too short at the beginning
                newCachePos = segStart;
            }
        }

        // Calculate new cache end, adjust start
        TSeqPos newCacheEnd = newCachePos + kCacheSize;
        if ( newCacheEnd > segStart + segSize ) {
            // Segment is too short at the end
            newCachePos = segStart + max(kCacheSize, segSize) - kCacheSize;
            newCacheEnd = segStart + segSize;
        }
        TSeqPos newCacheLen = newCacheEnd - newCachePos;

        x_FillCache(newCachePos, newCacheLen);
    }
    // Adjust current cache position
    m_Cache = m_CacheData + (pos - m_CachePos);
}


void CSeqVector_CI::x_FillCache(TSeqPos start, TSeqPos count)
{
    _ASSERT(m_Seg.GetType() != CSeqMap::eSeqEnd);
    _ASSERT(start >= m_Seg.GetPosition());
    _ASSERT(start < m_Seg.GetEndPosition());

    x_ResizeCache(count);
    _ASSERT(m_CacheData + count <= m_CacheEnd);

    switch ( m_Seg.GetType() ) {
    case CSeqMap::eSeqData:
    {
        const CSeq_data& data = m_Seg.GetRefData();

        CSeqVector::TCoding dataCoding = data.Which();
        CSeqVector::TCoding cacheCoding =
            m_Vector->x_GetCoding(m_Coding, dataCoding);
        bool reverse = m_Seg.GetRefMinusStrand();

        const char* table = 0;
        if ( cacheCoding != dataCoding || reverse ) {
            table = m_Vector->sx_GetConvertTable(dataCoding, cacheCoding, reverse);
            if ( !table && cacheCoding != dataCoding ) {
                THROW1_TRACE(runtime_error,
                             "CSeqVector_CI::x_FillCache: "
                             "incompatible codings");
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
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
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
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "Ncbipaa conversion not implemented");
        case CSeq_data::e_Ncbistdaa:
            copy_8bit_any(m_Cache, count, data.GetNcbistdaa().Get(), dataPos,
                          table, reverse);
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "invalid data type");
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
    m_CachePos = start;
}


void CSeqVector_CI::x_SetPos(TSeqPos pos)
{
    pos = min(m_Vector->size() - 1, pos);
    // If vector size is 0 and pos is kInvalidSqePos, the result will be
    // kInvalidSeqPos, which is not good.
    if (pos == kInvalidSeqPos) {
        pos = 0;
    }

    if (pos < m_CachePos  ||  pos >= x_CacheEnd()) {
        // Swap caches
        x_SwapCache();

        // Check if restored cache is not empty and contains "pos"
        if (m_CacheEnd != m_CacheData  &&
            pos >= m_CachePos  &&  pos < x_CacheEnd()) {
            if (m_CachePos >= m_Seg.GetEndPosition()  ||
                m_CachePos < m_Seg.GetPosition()) {
                // Update map segment
                if (m_CachePos == x_BackupEnd()) {
                    // Move forward, skip zero-length segments
                    do {
                        ++m_Seg;
                    } while (m_Seg  &&  m_Seg.GetLength() == 0);
                }
                else if (x_CacheEnd() == m_BackupPos) {
                    // Move backward, skip zero-length segments
                    do {
                        --m_Seg;
                    } while (m_Seg.GetPosition() > 0
                        &&  m_Seg.GetLength() == 0);
                }
                else {
                    m_Seg = m_Vector->x_GetSeqMap().FindResolved(m_CachePos,
                        m_Vector->m_Scope, m_Vector->m_Strand);
                }
                _ASSERT(pos >= m_Seg.GetPosition());
                _ASSERT(pos < m_Seg.GetEndPosition());
            }
        }
        else {
            // Can not use backup cache -- reset and refill
            x_ClearCache();
            x_UpdateCache(pos);
        }
    }
    m_Cache = m_CacheData + (pos - m_CachePos);
}


void CSeqVector_CI::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer)
{
    stop = min(stop, m_Vector->size());
    buffer.erase();
    if ( start < stop ) {
        buffer.reserve(stop - start);
        SetPos(start);
        do {
            size_t end_offset = min<size_t>(m_CacheEnd - m_CacheData,
                                    stop - m_CachePos);
            TCache_I cacheEnd = m_CacheData + end_offset;
            buffer.append(m_Cache, cacheEnd);
            m_Cache = cacheEnd;
            if (m_Cache >= m_CacheEnd) {
                x_NextCacheSeg();
            }
        } while (GetPos() < stop);
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_Vector);
    TSeqPos newPos = x_CacheEnd();
    x_SwapCache();
    while (newPos >= m_Seg.GetEndPosition()) {
        if ( !m_Seg ) {
            x_ClearCache();
            m_CachePos = m_Seg.GetPosition();
            return;
        }
        ++m_Seg;
    }
    // Try to re-use backup cache
    if (newPos < m_CachePos  ||  newPos >= x_CacheEnd()) {
        // can not use backup cache
        x_ClearCache();
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
    else {
        m_Cache = m_CacheData + newPos - m_CachePos;
    }
}


void CSeqVector_CI::x_PrevCacheSeg()
{
    _ASSERT(m_Vector);
    if (m_CachePos == 0) {
        // Can not go further
        THROW1_TRACE(runtime_error,
                     "CSeqVector_CI::x_PrevCacheSeg: no more data");
    }
    TSeqPos newPos = m_CachePos - 1;
    x_SwapCache();
    while (m_Seg.GetPosition() > newPos) {
        --m_Seg;
    }
    // Try to re-use backup cache
    if (newPos < m_CachePos  ||  newPos >= x_CacheEnd()) {
        // can not use backup cache
        x_ClearCache();
        m_CachePos = m_BackupPos;
        x_UpdateCache(newPos);
    }
    else {
        m_Cache = m_CacheData + newPos - m_CachePos;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
#endif
