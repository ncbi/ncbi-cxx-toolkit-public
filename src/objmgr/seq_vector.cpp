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
* Author: Aleksey Grichenko
*
* File Description:
*   Sequence data container for object manager
*
*/


#include <objects/objmgr/seq_vector.hpp>
#include <corelib/ncbimtx.hpp>
#include "data_source.hpp"
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
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/objmgr/seq_map.hpp>
#include <algorithm>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(const CSeqVector& vec)
{
    *this = vec;
}


CSeqVector::CSeqVector(CConstRef<CSeqMap> seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_SeqMap(seqMap),
      m_Scope(&scope),
      m_Coding(CSeq_data::e_not_set),
      m_Strand(strand),
      m_SequenceType(eType_not_set),
      m_CachePos(kInvalidSeqPos),
      m_CacheLen(0),
      m_Cache(0)
{
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_SeqMap(&seqMap),
      m_Scope(&scope),
      m_Coding(CSeq_data::e_not_set),
      m_Strand(strand),
      m_SequenceType(eType_not_set),
      m_CachePos(kInvalidSeqPos),
      m_CacheLen(0),
      m_Cache(0)
{
    SetCoding(coding);
}


CSeqVector::~CSeqVector(void)
{
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if (&vec != this) {
        m_SeqMap = vec.m_SeqMap;
        m_Scope = vec.m_Scope;
        m_SequenceType = vec.m_SequenceType;
        m_Coding = vec.m_Coding;
	m_Strand = vec.m_Strand;
        m_CacheData = vec.m_CacheData;
        m_Cache = &m_CacheData[0];
        m_CachePos = vec.m_CachePos;
        m_CacheLen = vec.m_CacheLen;
    }
    return *this;
}


TSeqPos CSeqVector::size(void) const
{
    // Calculate total sequence size
    return m_SeqMap->GetLength(m_Scope);
}


CSeqVector::TResidue CSeqVector::GetGapChar(void) const
{
    switch (GetCoding()) {
    case CSeq_data::e_Iupacna: // DNA - N
        return 'N';
    
    case CSeq_data::e_Ncbi8na: // DNA - bit representation
    case CSeq_data::e_Ncbi4na:
        return 0x0f;  // all bits set == any base

    case CSeq_data::e_Ncbieaa: // Proteins - X
    case CSeq_data::e_Iupacaa:
        return 'X';
    
    case CSeq_data::e_Ncbi8aa: // Protein - numeric representation
    case CSeq_data::e_Ncbistdaa:
        return 21;

    case CSeq_data::e_not_set:
        return 0;     // It's not good to throw an exception here

    case CSeq_data::e_Ncbi2na: // Codings without gap symbols
    case CSeq_data::e_Ncbipaa: //### Not sure about this
    case CSeq_data::e_Ncbipna: //### Not sure about this
    default:
        THROW1_TRACE(runtime_error,
                     "CSeqVector::GetGapChar() -- "
                     "Can not indicate gap using the selected coding");
    }
}


static const TSeqPos kCacheSize = 16384;
static const TSeqPos kCacheKeep = kCacheSize / 16;


void CSeqVector::x_ResizeCache(size_t size) const
{
    m_CacheData.resize(size);
    m_Cache = &m_CacheData[0];
}


CSeqVector::TResidue CSeqVector::x_GetResidue(TSeqPos pos) const
{
    TSeqPos oldCachePos = m_CachePos;
    TSeqPos oldCacheLen = m_CacheLen;
    TSeqPos oldCacheEnd = oldCachePos + oldCacheLen;

    TSignedSeqPos start;
    if ( m_CachePos == kInvalidSeqPos ) {
        start = pos - kCacheSize / 2;
    }
    else if ( pos >= m_CachePos ) {
        start = pos - kCacheKeep;
    }
    else {
        start = pos - (kCacheSize - kCacheKeep);
    }
    TSeqPos newCachePos = max(TSignedSeqPos(0), start);
    TSeqPos newCacheEnd = min(size(), newCachePos + kCacheSize);
    TSeqPos newCacheLen = newCacheEnd - newCachePos;

    TSeqPos copyCachePos = max(oldCachePos, newCachePos);
    TSeqPos copyCacheEnd = min(oldCacheEnd, newCacheEnd);
    if ( copyCacheEnd > copyCachePos ) {
        // copy some cache data
        TSeqPos oldCopyPos = copyCachePos - oldCachePos;
        TSeqPos newCopyPos = copyCachePos - newCachePos;
        if ( newCopyPos != oldCopyPos ) {
            TSeqPos oldCopyEnd = copyCacheEnd - oldCachePos;
            x_ResizeCache(max(oldCacheLen, newCacheLen));
            copy(m_Cache+oldCopyPos, m_Cache+oldCopyEnd, m_Cache+newCopyPos);
        }
        x_ResizeCache(newCacheLen);
        m_CachePos = newCachePos;
        m_CacheLen = newCacheLen;
        if ( copyCachePos > newCachePos ) {
            x_FillCache(newCachePos, copyCachePos);
        }
        if ( newCacheEnd > copyCacheEnd ) {
            x_FillCache(copyCacheEnd, newCacheEnd);
        }
    }
    else {
        x_ResizeCache(newCacheLen);
        m_CachePos = newCachePos;
        m_CacheLen = newCacheLen;
        x_FillCache(newCachePos, newCacheEnd);
    }

    return m_Cache[pos - m_CachePos];
}


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


void CSeqVector::x_FillCache(TSeqPos pos, TSeqPos end) const
{
    _ASSERT(pos >= m_CachePos);
    _ASSERT(pos < end);
    _ASSERT(end <= m_CachePos+m_CacheLen);

    TCoding needCoding = GetCoding();
    TCache_I dst = m_Cache + (pos - m_CachePos);
    for ( CSeqMap::const_iterator seg =
              m_SeqMap->ResolvedRangeIterator(m_Scope, pos, end-pos, m_Strand);
          seg; ++seg ) {
        _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
        TSeqPos count = seg.GetLength();
        if ( count == 0 ) {
            continue;
        }

        _ASSERT(dst - m_Cache + m_CachePos + count <= end);
        switch ( seg.GetType() ) {
        case CSeqMap::eSeqData:
        {
            const CSeq_data& data = seg.GetRefData();
            TSeqPos dataPos = seg.GetRefPosition();
            TCoding dataCoding = data.Which();

            if ( m_SequenceType == eType_not_set ) {
                x_UpdateSequenceType(dataCoding);
            }

            TCoding cacheCoding =
                needCoding == CSeq_data::e_not_set? dataCoding: needCoding;

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
                             "CSeqVector::x_FillCache: "
                             "Ncbipna conversion not implemented");
                break;
            case CSeq_data::e_Ncbi8aa:
                copy_8bit(dst, count, data.GetNcbi8aa().Get(), dataPos);
                break;
            case CSeq_data::e_Ncbieaa:
                copy_8bit(dst, count, data.GetNcbieaa().Get(), dataPos);
                break;
            case CSeq_data::e_Ncbipaa:
                THROW1_TRACE(runtime_error,
                             "CSeqVector::x_FillCache: "
                             "Ncbipaa conversion not implemented");
                break;
            case CSeq_data::e_Ncbistdaa:
                copy_8bit(dst, count, data.GetNcbistdaa().Get(), dataPos);
                break;
            default:
                THROW1_TRACE(runtime_error,
                             "CSeqVector::x_FillCache: "
                             "invalid data type");
            }
            if ( cacheCoding != dataCoding ) {
                const char* table = sx_GetConvertTable(dataCoding,
                                                       cacheCoding);
                if ( table ) {
                    translate(dst, count, table);
                }
                else {
                    THROW1_TRACE(runtime_error,
                                 "CSeqVector::x_ConvertCache: "
                                 "incompatible codings");
                }
            }
            if ( seg.GetRefMinusStrand() ) {
                reverse(dst, dst+count);
                const char* table = sx_GetComplementTable(cacheCoding);
                if ( table ) {
                    translate(dst, count, table);
                }
            }
            break;
        }
        case CSeqMap::eSeqGap:
            fill(dst, dst+count, GetGapChar());
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CSeqVector::x_FillCache: invalid segment type");
        }
        dst += count;
    }
}


DEFINE_STATIC_FAST_MUTEX(s_ConvertTableMutex);
DEFINE_STATIC_FAST_MUTEX(s_ComplementTableMutex);


const char* CSeqVector::sx_GetConvertTable(TCoding src, TCoding dst)
{
    CFastMutexGuard guard(s_ConvertTableMutex);
    typedef map<pair<TCoding, TCoding>, char*> TTables;
    static TTables tables;

    pair<TCoding, TCoding> key(src, dst);
    TTables::iterator it = tables.find(key);
    if ( it != tables.end() ) {
        // already created
        return it->second;
    }
    it = tables.insert(TTables::value_type(key, 0)).first;
    if ( !CSeqportUtil::IsCodeAvailable(src) ||
         !CSeqportUtil::IsCodeAvailable(dst) ) {
        // invalid types
        return 0;
    }
    pair<unsigned, unsigned> srcIndex = CSeqportUtil::GetCodeIndexFromTo(src);
    pair<unsigned, unsigned> dstIndex = CSeqportUtil::GetCodeIndexFromTo(dst);
    const size_t COUNT = kMax_UChar+1;
    if ( srcIndex.second >= COUNT || dstIndex.second >= COUNT ) {
        // too large range
        return 0;
    }
    try {
        CSeqportUtil::GetMapToIndex(src, dst, srcIndex.first);
    }
    catch ( runtime_error& /*badType*/ ) {
        // incompatible types
        return 0;
    }
    char* table = it->second = new char[COUNT];
    fill(table, table+COUNT, char(255));
    for ( unsigned i = srcIndex.first; i <= srcIndex.second; ++i ) {
        table[i] = char(min(255u, CSeqportUtil::GetMapToIndex(src, dst, i)));
    }
    return table;
}


const char* CSeqVector::sx_GetComplementTable(TCoding key)
{
    CFastMutexGuard guard(s_ComplementTableMutex);
    typedef map<TCoding, char*> TTables;
    static TTables tables;

    TTables::iterator it = tables.find(key);
    if ( it != tables.end() ) {
        // already created
        return it->second;
    }
    it = tables.insert(TTables::value_type(key, 0)).first;
    if ( !CSeqportUtil::IsCodeAvailable(key) ) {
        // invalid types
        return 0;
    }
    pair<unsigned, unsigned> index = CSeqportUtil::GetCodeIndexFromTo(key);
    const size_t COUNT = kMax_UChar+1;
    if ( index.second >= COUNT ) {
        // too large range
        return 0;
    }
    try {
        CSeqportUtil::GetIndexComplement(key, index.first);
    }
    catch ( runtime_error& /*badType*/ ) {
        // incompatible types
        return 0;
    }
    char* table = it->second = new char[COUNT];
    fill(table, table+COUNT, char(255));
    for ( unsigned i = index.first; i <= index.second; ++i ) {
        try {
            table[i] = char(min(255u,
                                CSeqportUtil::GetIndexComplement(key, i)));
        }
        catch ( runtime_error& /*noComplement*/ ) {
        }
    }
    return table;
}


void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding != coding) {
        ClearCache();
        m_Coding = coding;
        _ASSERT(GetCoding() == coding);
    }
}


void CSeqVector::SetIupacCoding(void)
{
    switch ( GetSequenceType() ) {
    case eType_aa:
        SetCoding(CSeq_data::e_Iupacaa);
        break;
    case eType_na:
        SetCoding(CSeq_data::e_Iupacna);
        break;
    }
}


void CSeqVector::SetNcbiCoding(void)
{
    switch ( GetSequenceType() ) {
    case eType_aa:
        SetCoding(CSeq_data::e_Ncbistdaa);
        break;
    case eType_na:
        SetCoding(CSeq_data::e_Ncbi4na);
        break;
    }
}


void CSeqVector::SetCoding(EVectorCoding coding)
{
    switch ( coding ) {
    case CBioseq_Handle::eCoding_Iupac:
        SetIupacCoding();
        break;
    case CBioseq_Handle::eCoding_Ncbi:
        SetNcbiCoding();
        break;
    default:
        SetCoding(CSeq_data::e_not_set);
        break;
    }
}


CSeqVector::ESequenceType CSeqVector::GetSequenceType(void) const
{
    if ( m_SequenceType == eType_not_set ) {
        for ( CSeqMap::const_iterator i = m_SeqMap->BeginResolved(m_Scope);
              i != m_SeqMap->EndResolved(m_Scope); ++i ) {
            if ( i.GetType() == CSeqMap::eSeqData &&
                 x_UpdateSequenceType(i.GetRefData().Which()) ) {
                return m_SequenceType;
            }
        }
        m_SequenceType = eType_unknown;
    }
    return m_SequenceType;
}


bool CSeqVector::x_UpdateSequenceType(TCoding coding) const
{
    switch ( coding ) {
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Iupacna:
        m_SequenceType = eType_na;
        return true;
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
    case CSeq_data::e_Iupacaa:
        m_SequenceType = eType_aa;
        return true;
    default:
        return false;
    }
}


void CSeqVector::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const
{
    stop = min(stop, size());
    buffer.erase();
    while ( start < stop ) {
        (*this)[start];
        TSeqPos cachePos = start - m_CachePos;
        _ASSERT(cachePos < m_CacheLen);
        TSeqPos cacheEnd = min(m_CacheLen, stop - m_CachePos);
        buffer.append(m_Cache+cachePos, m_Cache+cacheEnd);
        start += (cacheEnd - cachePos);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.40  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.39  2003/01/23 18:26:02  ucko
* Explicitly #include <algorithm>
*
* Revision 1.38  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.37  2003/01/03 19:45:12  dicuccio
* Replaced kPosUnknwon with kInvalidSeqPos (non-static variable; worka-round for
* MSVC)
*
* Revision 1.36  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.35  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.34  2002/10/03 13:45:39  grichenk
* CSeqVector::size() made const
*
* Revision 1.33  2002/09/26 20:15:11  grichenk
* Fixed cache lengths checks
*
* Revision 1.32  2002/09/16 20:13:01  grichenk
* Fixed Iupac coding setter
*
* Revision 1.31  2002/09/16 13:52:49  dicuccio
* Fixed bug in calculating total range of cached interval to retrieve -
* must clamp the cached range to the desired range.
*
* Revision 1.30  2002/09/12 19:59:25  grichenk
* Fixed bugs in calculating cached intervals
*
* Revision 1.29  2002/09/10 19:55:52  grichenk
* Fixed reverse-complement position
*
* Revision 1.28  2002/09/09 21:38:37  grichenk
* Fixed problem with GetSeqData() for minus strand
*
* Revision 1.27  2002/09/03 21:27:01  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.26  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.25  2002/05/31 20:58:19  grichenk
* Fixed GetSeqData() bug
*
* Revision 1.24  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.23  2002/05/17 17:14:53  grichenk
* +GetSeqData() for getting a range of characters from a seq-vector
*
* Revision 1.22  2002/05/09 14:16:31  grichenk
* sm_SizeUnknown -> kPosUnknown, minor fixes for unsigned positions
*
* Revision 1.21  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.20  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.19  2002/05/03 18:36:19  grichenk
* Fixed members initialization
*
* Revision 1.18  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.17  2002/04/29 16:23:28  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.16  2002/04/26 14:37:21  grichenk
* Limited CSeqVector cache size
*
* Revision 1.15  2002/04/25 18:14:47  grichenk
* e_not_set coding gap symbol set to 0
*
* Revision 1.14  2002/04/25 16:37:21  grichenk
* Fixed gap coding, added GetGapChar() function
*
* Revision 1.13  2002/04/23 19:01:07  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.12  2002/04/22 20:03:08  grichenk
* Updated comments
*
* Revision 1.11  2002/04/17 21:07:59  grichenk
* String pre-allocation added
*
* Revision 1.10  2002/04/03 18:06:48  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.8  2002/03/28 18:34:58  grichenk
* Fixed convertions bug
*
* Revision 1.7  2002/03/08 21:24:35  gouriano
* fixed errors with unresolvable references
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
