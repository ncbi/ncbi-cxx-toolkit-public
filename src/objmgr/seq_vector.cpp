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
* ---------------------------------------------------------------------------
* $Log$
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


#include <objects/objmgr1/seq_vector.hpp>
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(const CBioseq_Handle& handle,
                       bool use_iupac_coding,
                       bool plus_strand,
                       CScope& scope,
                       CConstRef<CSeq_loc> view_loc)
    : m_Scope(&scope),
      m_Handle(handle),
      m_PlusStrand(plus_strand),
      m_Size(-1),
      m_Coding(CSeq_data::e_not_set),
      m_RangeSize(-1),
      m_CurFrom(-1),
      m_CurTo(-1),
      m_OrgTo(0)
{
    m_CurData.dest_start = -1;
    m_CurData.length = 0;
    m_SeqMap.Reset(&m_Handle.x_GetDataSource().GetSeqMap(m_Handle));
    if (use_iupac_coding)
        SetIupacCoding();
    if ( view_loc ) {
        x_SetVisibleArea(*view_loc);
    }
    else {
        m_Ranges[TRange::GetWholeTo()] = TRangeWithStrand(TRange(
            TRange::GetWholeFrom(), TRange::GetWholeTo()), plus_strand);
    }
    m_SelRange = m_Ranges.end();
}


CSeqVector::~CSeqVector(void)
{
    return;
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if (&vec == this)
        return *this;
    m_Scope = vec.m_Scope;
    m_Handle = vec.m_Handle;
    m_PlusStrand = vec.m_PlusStrand;
    m_CurData.dest_start = vec.m_CurData.dest_start;
    m_CurData.length = vec.m_CurData.length;
    m_CurData.src_start = vec.m_CurData.src_start;
    m_CurData.src_data = vec.m_CurData.src_data;
    m_SeqMap = vec.m_SeqMap;
    m_Size = vec.m_Size;
    m_Coding = vec.m_Coding;
    m_Ranges.clear();
    m_SelRange = m_Ranges.end();
    iterate(TRanges, rit, vec.m_Ranges) {
        TRanges::const_iterator tmp = m_Ranges.insert(*rit).first;
        if (vec.m_SelRange == rit)
            m_SelRange = tmp;
    }
    m_RangeSize = vec.m_RangeSize;
    m_CurFrom = vec.m_CurFrom;
    m_CurTo = vec.m_CurTo;
    m_OrgTo = vec.m_OrgTo;
    return *this;
}


void CSeqVector::x_SetVisibleArea(const CSeq_loc& view_loc)
{
    int rg_end = 0;
    CSeq_loc_CI lit(view_loc);
    for ( ; lit; lit++) {
        if ( lit.IsEmpty() )
            continue;
        int from = lit.GetRange().IsWholeFrom() ?
            0 : lit.GetRange().GetFrom();
        int to = lit.GetRange().IsWholeTo() ?
            x_GetTotalSize()-1 : lit.GetRange().GetTo();
        rg_end += to - from + 1;
        m_Ranges[rg_end] = TRangeWithStrand(TRange(from, to + 1),
            lit.GetStrand() != eNa_strand_minus);
    }
}


size_t CSeqVector::x_GetTotalSize(void)
{
    if (m_Size < 0) {
        // Calculate total sequence size
        m_Size = 0;
        for (size_t i = 0; i < m_SeqMap->size(); i++) {
            if ((*m_SeqMap)[i].m_Length > 0) {
                // Use explicit segment size
                m_Size += (*m_SeqMap)[i].m_Length;
            }
            else {
                switch ((*m_SeqMap)[i].m_SegType) {
                case CSeqMap::eSeqData:
                    {
                        break;
                    }
                case CSeqMap::eSeqRef:
                    {
                        // Zero length stands for "whole" reference
                        const CSeq_id* id =
                            &CSeq_id_Mapper::GetSeq_id(
                            (*m_SeqMap)[i].m_RefSeq);
                        CBioseq_Handle bh = m_Scope->GetBioseqHandle(*id);
                        if (bh) {
                            CBioseq_Handle::TBioseqCore ref_seq =
                                bh.GetBioseqCore();
                            if (ref_seq.GetPointer()  &&
                                ref_seq->GetInst().IsSetLength()) {
                                m_Size += ref_seq->GetInst().GetLength();
                            }
                        }
                        break;
                    }
                case CSeqMap::eSeqGap:
                    {
                        break;
                    }
                case CSeqMap::eSeqEnd:
                    {
                        break;
                    }
                }
            }
        }
    }
    return m_Size;
}


size_t CSeqVector::x_GetVisibleSize(void)
{
    if (m_Size < 0)
        x_GetTotalSize();
    if (m_RangeSize < 0) {
        // Calculate the visible area size
        m_RangeSize = 0;
        iterate (TRanges, rit, m_Ranges) {
            int from = rit->second.first.IsWholeFrom() ?
                0 : rit->second.first.GetFrom();
            int to = rit->second.first.IsWholeTo() ?
                m_Size : rit->second.first.GetTo();
            m_RangeSize += to - from;
        }
        // Change whole-to position, if any, to the sequence length
        TRanges::iterator whole_to = m_Ranges.find(TRange::GetWholeTo());
        if (whole_to != m_Ranges.end()) {
            TRangeWithStrand last_rg = whole_to->second;
            m_Ranges.erase(whole_to);
            m_Ranges[m_RangeSize] = last_rg;
        }
    }
    return m_RangeSize;
}


void CSeqVector::x_UpdateVisibleRange(int pos)
{
    // Find a range, containing the "pos" point. Ranges are
    // mapped by ends, not starts, so that we can use lower_bound()
    m_SelRange = m_Ranges.upper_bound(pos);
    if ( m_SelRange == m_Ranges.end() ) {
        throw runtime_error("CSeqVector -- position beyond vector end");
    }
    int sel_from = m_SelRange->second.first.IsWholeFrom() ?
        0 : m_SelRange->second.first.GetFrom();
    int sel_to = m_SelRange->second.first.IsWholeTo() ?
        x_GetTotalSize() : m_SelRange->second.first.GetTo();
    m_CurTo = m_SelRange->first;
    m_CurFrom = m_SelRange->first - (sel_to - sel_from);
    m_OrgTo = m_SelRange->second.first.GetTo();
    if ( m_SelRange->second.first.IsWholeTo() )
        m_OrgTo = m_Size;
}


void CSeqVector::x_UpdateSeqData(int pos)
{
    m_CurData.src_data = 0; // Reset data
    m_Scope->x_GetSequence(m_Handle, pos, &m_CurData);
    m_CachedPos = -1; // Reset cached data
    m_CachedData = "";
}


CSeqVector::TResidue CSeqVector::GetGapChar(void)
{
    switch (GetCoding()) {
    // DNA - N
    case CSeq_data::e_Iupacna:   return 'N';
    // DNA - bit representation
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbi4na:   return 0x0f;  // all bits set == any base

    // Proteins - X
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Iupacaa:   return 'X';
    // Protein - numeric representation
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbistdaa: return 21;

    case CSeq_data::e_not_set:   return 0;     // It's not good to throw an exception here
    // Codings without gap symbols
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbipaa:                 //### Not sure about this
    case CSeq_data::e_Ncbipna:                 //### Not sure about this
    default:
        {
            throw runtime_error("CSeqVector -- can not indicate gap using the selected coding");
        }
    }
}


const int kCacheSize = 65536;


CSeqVector::TResidue CSeqVector::x_GetResidue(int pos)
{
    // The cache must be initialized and include the point requested
    if (m_CachedPos < 0  ||  m_CachedLen <= 0  ||
        m_CachedPos > pos  ||  m_CachedPos+m_CachedLen <= pos) {
        // Select cache position and length to cover maximum of
        // kCacheSize*2 characters around pos.
        m_CachedPos = pos - kCacheSize;
        int cend = m_CachedPos + kCacheSize*2;
        if (m_CachedPos < m_CurData.dest_start) {
            m_CachedPos = m_CurData.dest_start;
            cend = m_CachedPos + kCacheSize*2;
        }
        if (cend > m_CurData.dest_start + m_CurData.length) {
            cend = m_CurData.dest_start + m_CurData.length;
            m_CachedPos = cend - kCacheSize*2;
            if (m_CachedPos < m_CurData.dest_start)
                m_CachedPos = m_CurData.dest_start;
        }
        m_CachedLen = cend - m_CachedPos;
        int src_start = m_CurData.src_start + m_CachedPos - m_CurData.dest_start;
        if (!m_CurData.src_data) {
            // No data - fill with the gap symbol
            m_CachedData = string(m_CachedLen, GetGapChar());
        }
        else {
            // Prepare real data
            CConstRef<CSeq_data> out;

            TSeqPosition start = src_start;

            if (m_CurData.src_data->Which() == m_Coding  ||
                m_Coding == CSeq_data::e_not_set) {
                out = m_CurData.src_data;
            }
            else {
                CSeq_data* tmp = new CSeq_data;
                out.Reset(tmp);
                CSeqportUtil::Convert(*m_CurData.src_data, tmp,
                    m_Coding, src_start, m_CachedLen);
                // Adjust starting position
                start = 0;
            }

            // XOR current range strand and destination strand -- do not
            // need to convert if both plus or both minus.
            if ( m_PlusStrand != m_SelRange->second.second ) {
                CSeq_data* tmp = new CSeq_data;
                CSeqportUtil::Complement(*out, tmp);
                out.Reset(tmp);
            }
            switch ( out->Which() ) {
            case CSeq_data::e_Iupacna:
                {
                    m_CachedData = out->GetIupacna().Get().
                        substr(start, m_CachedLen);
                    break;
                }
            case CSeq_data::e_Iupacaa:
                {
                    m_CachedData = out->GetIupacaa().Get().
                        substr(start, m_CachedLen);
                    break;
                }
            case CSeq_data::e_Ncbi2na:
                {
                    m_CachedData = "";
                    m_CachedData.reserve(m_CachedLen);
                    const vector<char>& buf = out->GetNcbi2na().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += (buf[i/4] >> (6-(i%4)*2)) & 0x03;
                    }
                    break;
                }
            case CSeq_data::e_Ncbi4na:
                {
                    m_CachedData = "";
                    m_CachedData.reserve(m_CachedLen);
                    const vector<char>& buf = out->GetNcbi4na().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += (buf[i/2] >> (4-(i % 2)*4)) & 0x0f;
                    }
                    break;
                }
            case CSeq_data::e_Ncbi8na:
                {
                    m_CachedData = "";
                    m_CachedData.reserve(m_CachedLen);
                    const vector<char>& buf = out->GetNcbi8na().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += buf[i];
                    }
                    break;
                }
            case CSeq_data::e_Ncbieaa:
                {
                    m_CachedData = out->GetNcbieaa().Get().
                        substr(start, m_CachedLen);
                    break;
                }
            case CSeq_data::e_Ncbipna:
                {
                    m_CachedData = "";
                    const vector<char>& buf = out->GetNcbipna().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += buf[i];
                    }
                    break;
                }
            case CSeq_data::e_Ncbi8aa:
                {
                    m_CachedData = "";
                    const vector<char>& buf = out->GetNcbi8aa().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += buf[i];
                    }
                    break;
                }
            case CSeq_data::e_Ncbipaa:
                {
                    m_CachedData = "";
                    const vector<char>& buf = out->GetNcbipaa().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += buf[i];
                    }
                    break;
                }
            case CSeq_data::e_Ncbistdaa:
                {
                    m_CachedData = "";
                    const vector<char>& buf = out->GetNcbistdaa().Get();
                    for (int i = start; i < start + m_CachedLen; i++) {
                        m_CachedData += buf[i];
                    }
                    break;
                }
            default:
                {
                    throw runtime_error("CSeqVector -- unknown coding");
                }
            }
            //out.Release();
        }
    }
    return m_CachedData[pos - m_CachedPos];
}


void CSeqVector::SetIupacCoding(void)
{
    // force instantiantion
    size();
    m_CurData.src_data = 0; // Reset data
    m_Scope->x_GetSequence(m_Handle, 0, &m_CurData);
    m_CachedPos = -1; // Reset cached data
    m_CachedData = "";

    // Check sequence type
    if ( !m_CurData.src_data ) {
        return;
    }
    switch (m_CurData.src_data->Which()) {
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
        SetCoding(CSeq_data::e_Iupacna);
        break;
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
        SetCoding(CSeq_data::e_Iupacaa);
        break;
    default:
        break;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
