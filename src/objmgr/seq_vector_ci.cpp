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


#include <objects/objmgr/seq_vector_ci.hpp>
//#include <objects/objmgr/impl/data_source.hpp>
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
//#include <objects/seq/Seq_inst.hpp>
//#include <objects/seq/seqport_util.hpp>
//#include <objects/seqloc/Seq_loc.hpp>
//#include <objects/objmgr/seq_map.hpp>
#include <algorithm>
//#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const TSeqPos kCacheSize = 16384;
static const TSeqPos kCacheKeep = kCacheSize / 16;


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
      m_Position(pos),
      m_Coding(seq_vector.GetCoding()),
      m_CurrentCache(new SCacheData),
      m_BackupCache(new SCacheData)
{
    x_Seg() = m_Vector->m_SeqMap->FindResolved(m_Position,
        m_Vector->m_Scope, m_Vector->m_Strand);
    x_UpdateCache();
}


void CSeqVector_CI::x_UpdateCachePtr(void)
{
    x_Cache() = x_CacheData().empty()? 0: &x_CacheData()[0];
}


void CSeqVector_CI::x_ResizeCache(size_t size)
{
    x_CacheData().resize(size);
    x_UpdateCachePtr();
}


void CSeqVector_CI::x_UpdateCache(void)
{
    _ASSERT(bool(m_Vector));
    // Adjust segment
    if (m_Position < x_Seg().GetPosition()  ||
        m_Position >= x_Seg().GetEndPosition()) {
        x_Seg() = m_Vector->m_SeqMap->FindResolved(m_Position,
            m_Vector->m_Scope, m_Vector->m_Strand);
    }
    _ASSERT(m_Position >= x_Seg().GetPosition());
    _ASSERT(m_Position < x_Seg().GetEndPosition());

    TSeqPos segStart = x_Seg().GetPosition();
    TSeqPos segSize = x_Seg().GetLength();

    if (segSize <= kCacheSize) {
        // Try to cache the whole segment
        x_CachePos() = segStart;
        x_CacheLen() = segSize;
        x_ResizeCache(x_CacheLen());
        x_FillCache(x_CachePos(), x_CachePos() + x_CacheLen());
    }
    else {
        // Cache only a part of the current segment
        TSeqPos oldCachePos = x_CachePos();
        TSeqPos oldCacheLen = x_CacheLen();
        TSeqPos oldCacheEnd = oldCachePos + oldCacheLen;

        // Calculate new cache start
        TSignedSeqPos start;
        if ( x_CachePos() == kInvalidSeqPos ) {
            // Uninitialized cache
            start = m_Position - kCacheSize / 2;
        }
        else if ( m_Position >= x_CachePos() ) {
            // Moving forward, keep the end of the old cache
            start = m_Position - kCacheKeep;
        }
        else {
            // Moving backward, keep the start of the old cache
            start = m_Position - (kCacheSize - kCacheKeep);
        }

        // Calculate new cache end, adjust start
        TSeqPos newCachePos;
        TSeqPos newCacheEnd;
        if ( start < segStart ) {
            // Segment is too short at the beginning
            newCachePos = segStart;
            newCacheEnd = segStart + min(segSize, kCacheSize);
        }
        else if (start + kCacheSize > segStart + segSize) {
            // Segment is too short at the end
            newCachePos = segStart + max(kCacheSize, segSize) - kCacheSize;
            newCacheEnd = segStart + segSize;
        }
        else {
            // Caching internal part of the segment
            newCachePos = start;
            newCacheEnd = start + kCacheSize;
        }
        TSeqPos newCacheLen = newCacheEnd - newCachePos;

        // Calculate and copy the overlap between old and new caches
        TSeqPos copyCachePos = max(oldCachePos, newCachePos);
        TSeqPos copyCacheEnd = min(oldCacheEnd, newCacheEnd);
        if (copyCacheEnd > copyCachePos) {
            // Copy some cache data
            TSeqPos oldCopyPos = copyCachePos - oldCachePos;
            TSeqPos newCopyPos = copyCachePos - newCachePos;
            if ( newCopyPos != oldCopyPos ) {
                TSeqPos oldCopyEnd = copyCacheEnd - oldCachePos;
                if (newCacheLen != oldCacheLen) {
                    TCacheData newCacheData(newCacheLen);
                    copy(x_Cache() + oldCopyPos, x_Cache() + oldCopyEnd,
                         &newCacheData[0] + newCopyPos);
                    swap(x_CacheData(), newCacheData);
                }
                else {
                    if (newCopyPos > oldCopyPos) {
                        TSeqPos newCopyEnd = copyCacheEnd - newCachePos;
                        copy_backward(x_Cache() + oldCopyPos, x_Cache() + oldCopyEnd,
                             x_Cache() + newCopyEnd);
                    }
                    else {
                        copy(x_Cache() + oldCopyPos, x_Cache() + oldCopyEnd,
                             x_Cache() + newCopyPos);
                    }
                }
            }
            x_ResizeCache(newCacheLen);
            x_CachePos() = newCachePos;
            x_CacheLen() = newCacheLen;
            if ( copyCachePos > newCachePos ) {
                x_FillCache(newCachePos, copyCachePos);
            }
            if ( newCacheEnd > copyCacheEnd ) {
                x_FillCache(copyCacheEnd, newCacheEnd);
            }
        }
        else {
            x_ResizeCache(newCacheLen);
            x_CachePos() = newCachePos;
            x_CacheLen() = newCacheLen;
            x_FillCache(newCachePos, newCacheEnd);
        }
    }
}


void CSeqVector_CI::x_FillCache(TSeqPos pos, TSeqPos end)
{
    _ASSERT(x_Seg().GetType() != CSeqMap::eSeqEnd);
    _ASSERT(pos >= x_CachePos());
    _ASSERT(pos < end);
    _ASSERT(end <= x_CachePos() + x_CacheLen());
    _ASSERT(pos >= x_Seg().GetPosition());
    _ASSERT(pos < x_Seg().GetEndPosition());

    TCache_I dst = x_Cache() + (pos - x_CachePos());

    TSeqPos count = x_Seg().GetLength();

    _ASSERT(dst - x_Cache() + x_CachePos() + count <= end);
    switch ( x_Seg().GetType() ) {
    case CSeqMap::eSeqData:
    {
        const CSeq_data& data = x_Seg().GetRefData();
        TSeqPos dataPos = x_Seg().GetRefPosition();
        CSeqVector::TCoding dataCoding = data.Which();

        CSeqVector::TCoding cacheCoding =
            m_Vector->x_GetCoding(m_Coding, dataCoding);

        switch ( dataCoding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit(dst, count, data.GetIupacna().Get(), dataPos);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit(dst, count, data.GetIupacaa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit(dst, count, data.GetNcbi2na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit(dst, count, data.GetNcbi4na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit(dst, count, data.GetNcbi8na().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbipna:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "Ncbipna conversion not implemented");
        case CSeq_data::e_Ncbi8aa:
            copy_8bit(dst, count, data.GetNcbi8aa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit(dst, count, data.GetNcbieaa().Get(), dataPos);
            break;
        case CSeq_data::e_Ncbipaa:
            THROW1_TRACE(runtime_error,
                         "CSeqVector_CI::x_FillCache: "
                         "Ncbipaa conversion not implemented");
        case CSeq_data::e_Ncbistdaa:
            copy_8bit(dst, count, data.GetNcbistdaa().Get(), dataPos);
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
                translate(dst, count, table);
            }
            else {
                THROW1_TRACE(runtime_error,
                             "CSeqVector_CI::x_FillCache: "
                             "incompatible codings");
            }
        }
        if ( x_Seg().GetRefMinusStrand() ) {
            reverse(dst, dst + count);
            const char* table = m_Vector->sx_GetComplementTable(cacheCoding);
            if ( table ) {
                translate(dst, count, table);
            }
        }
        break;
    }
    case CSeqMap::eSeqGap:
        m_Vector->x_GetCoding(m_Coding, CSeq_data::e_not_set);
        fill(dst, dst + count, m_Vector->x_GetGapChar(m_Coding));
        break;
    default:
        THROW1_TRACE(runtime_error,
                     "CSeqVector_CI::x_FillCache: invalid segment type");
    }
}


void CSeqVector_CI::SetPos(TSeqPos pos)
{
    m_Position = min(m_Vector->size() - 1, pos);
    if (m_Position < x_CachePos()  ||  m_Position >= x_CachePos() + x_CacheLen()) {
        // Adjust cache
        swap(m_CurrentCache, m_BackupCache);
        x_UpdateCache();
    }
}


void CSeqVector_CI::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer)
{
    stop = min(stop, m_Vector->size());
    buffer.erase();
    if ( start < stop ) {
        buffer.reserve(stop-start);
        m_Position = start;
        if (m_Position < x_CachePos()  ||  m_Position >= x_CachePos() + x_CacheLen()) {
            x_UpdateCache();
        }
        do {
            TSeqPos cachePos = m_Position - x_CachePos();
            _ASSERT(cachePos < x_CacheLen());
            TSeqPos cacheEnd = min(x_CacheLen(), stop - x_CachePos());
            buffer.append(x_Cache() + cachePos, x_Cache() + cacheEnd);
            m_Position += (cacheEnd - cachePos);
            x_NextCacheSeg();
        } while (m_Position < stop);
    }
}


void CSeqVector_CI::x_NextCacheSeg()
{
    _ASSERT(m_Vector);
    if ( m_BackupCache->SegContains(m_Position) ) {
        swap(m_CurrentCache, m_BackupCache);
        if ( m_CurrentCache->CacheContains(m_Position) )
            return;
    }
    else {
        *m_BackupCache = *m_CurrentCache;
        while ((m_Position >= x_Seg().GetEndPosition()  ||  x_Seg().GetLength() == 0)
            && x_Seg().GetType() != CSeqMap::eSeqEnd) {
            ++x_Seg();
        }
    }
    if (x_Seg().GetType() != CSeqMap::eSeqEnd)
        x_UpdateCache();
}


void CSeqVector_CI::x_PrevCacheSeg()
{
    _ASSERT(m_Vector);
    if ( m_BackupCache->SegContains(m_Position) ) {
        swap(m_CurrentCache, m_BackupCache);
        if ( m_CurrentCache->CacheContains(m_Position) )
            return;
    }
    else {
        *m_BackupCache = *m_CurrentCache;
        while (m_Position < x_Seg().GetPosition()  ||
            (x_Seg().GetLength() == 0  &&  x_Seg().GetPosition() > 0)) {
            --x_Seg();
        }
    }
    if (x_Seg().GetType() != CSeqMap::eSeqEnd)
        x_UpdateCache();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/05/27 19:42:24  grichenk
* Initial revision
*
*
* ===========================================================================
*/
