#ifndef SEQ_VECTOR__HPP
#define SEQ_VECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Sequence data container for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2002/04/29 16:23:25  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.8  2002/04/25 16:37:19  grichenk
* Fixed gap coding, added GetGapChar() function
*
* Revision 1.7  2002/04/23 19:01:06  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/28 19:45:34  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/seq_map.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/seq/Seq_data.hpp>
#include <util/range.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_loc;

class CSeqVector : public CObject
{
public:
    typedef unsigned char         TResidue;
    typedef CSeq_data::E_Choice   TCoding;

    CSeqVector(const CSeqVector& vec);
    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    size_t size(void);
    // 0-based array of residues
    TResidue operator[] (int pos);

    // Target sequence coding. CSeq_data::e_not_set -- do not
    // convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void);
    void SetCoding(TCoding coding);
    // Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);

    // Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(void);

private:
    friend class CBioseq_Handle;

    // Created by CBioseq_Handle only.
    CSeqVector(const CBioseq_Handle& handle,
        bool use_iupac_coding,               // Set coding to IUPACna or IUPACaa?
        bool plus_strand,                    // Use plus strand coordinates?
        CScope& scope,
        CConstRef<CSeq_loc> view_loc);       // Restrict visible area with a seq-loc

    // Process seq-loc, create visible area ranges
    void x_SetVisibleArea(const CSeq_loc& view_loc);
    // Calculate sequence and visible area size
    size_t x_GetTotalSize(void);
    size_t x_GetVisibleSize(void);

    void x_UpdateVisibleRange(int pos);
    void x_UpdateSeqData(int pos);
    // Get residue assuming the data in m_CurrentData are valid
    TResidue x_GetResidue(int pos);

    // Single visible interval
    typedef CRange<int> TRange;
    // Interval with the plus strand flag (true = plus)
    typedef pair<TRange, bool> TRangeWithStrand;
    // Map visible interval end (not start!) to an interval with strand.
    // E.g. if there is only one visible interval [20..30], it will be
    // mapped with 10 (=30-20) key.
    typedef map<int, TRangeWithStrand> TRanges;

    CScope*            m_Scope;
    CBioseq_Handle     m_Handle;
    bool               m_PlusStrand;
    SSeqData           m_CurData;
    CConstRef<CSeqMap> m_SeqMap;
    int                m_Size;
    string             m_CachedData;
    int                m_CachedPos;
    int                m_CachedLen;
    TCoding            m_Coding;
    TRanges            m_Ranges;    // Set of visible ranges
    int                m_RangeSize; // Visible area size
    TRanges::const_iterator m_SelRange;  // Selected range from the visible area
    // Current visible range limits -- for faster checks in []
    int                m_CurFrom;   // visible segment start
    int                m_CurTo;     // visible segment end
    // End of visible segment on the original sequence
    int                m_OrgTo;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector::CSeqVector(const CSeqVector& vec)
{
    *this = vec;
}

inline
size_t CSeqVector::size(void)
{
    if (m_RangeSize < 0)
        x_GetVisibleSize();
    return m_RangeSize;
}

inline
CSeqVector::TCoding CSeqVector::GetCoding(void)
{
    if (m_Coding != CSeq_data::e_not_set) {
        return m_Coding;
    }
    else if ( m_CurData.src_data ) {
        return m_CurData.src_data->Which();
    }
    // No coding specified
    return CSeq_data::e_not_set;
}

inline
void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding == coding) return;
    m_Coding = coding;
    // Reset cached data
    m_CachedPos = 0;
    m_CachedLen = 0;
    m_CachedData = "";
}

inline
CSeqVector::TResidue CSeqVector::operator[] (int pos)
{
    // Force size calculation
    size_t seq_size = size();
    // Convert position to destination strand
    if ( !m_PlusStrand ) {
        pos = seq_size - pos - 1;
    }

    if (pos >= m_CurTo  ||  pos < m_CurFrom) {
        x_UpdateVisibleRange(pos);
    }

    // Recalculate position from visible area to the whole sequence
    pos += m_OrgTo - m_CurTo;
    // Check and update current data segment
    if (m_CurData.dest_start > pos  ||
        m_CurData.dest_start+m_CurData.length <= pos) {
        x_UpdateSeqData(pos);
    }
    // Get the residue
    return x_GetResidue(pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_VECTOR__HPP
