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
*/

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/seq_map.hpp>
#include <objects/seq/Seq_data.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_loc;
class CSeqMap;
class CSeq_data;

// Sequence data
struct SSeqData {
    TSeqPos              length;      /// Length of the sequence data piece
    TSeqPos              dest_start;  /// Starting pos in the dest. Bioseq
    TSeqPos              src_start;   /// Starting pos in the source Bioseq
    CConstRef<CSeq_data> src_data;    /// Source sequence data
};

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject
{
public:
    typedef unsigned char         TResidue;
    typedef CSeq_data::E_Choice   TCoding;
    typedef CBioseq_Handle::EVectorCoding EVectorCoding;

    CSeqVector(CConstRef<CSeqMap> seqMap, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi);
    CSeqVector(const CSeqVector& vec);

    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    TSeqPos size(void) const;
    // 0-based array of residues
    TResidue operator[] (TSeqPos pos) const;

    // Fill the buffer string with the sequence data for the interval
    // [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const;

    enum ESequenceType {
        eType_not_set,  // temporary before initialization
        eType_na,
        eType_aa,
        eType_unknown   // cannot be determined (no data)
    };
    ESequenceType GetSequenceType(void) const;

    // Target sequence coding. CSeq_data::e_not_set -- do not
    // convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);
    // Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);
    // Set coding to either Ncbi8aa or Ncbi8na depending on molecule type
    void SetNcbiCoding(void);
    // Set coding to either Iupac or Ncbi8xx
    void SetCoding(EVectorCoding coding);

    // Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(void) const;

    void ClearCache(void);

private:
    friend class CBioseq_Handle;
    //typedef CSeqMap::resolved_const_iterator TSeqMap_CI;
    typedef vector<char> TCacheData;
    typedef char* TCache_I;

    // Get residue
    TResidue x_GetResidue(TSeqPos pos) const;
    // fill part of cache
    void x_ResizeCache(size_t size) const;
    void x_FillCache(TSeqPos start, TSeqPos end) const;
    void x_ConvertCache(TCache_I pos, size_t count, TCoding from, TCoding to) const;
    void x_ReverseCache(TCache_I pos, size_t count, TCoding coding) const;

    bool x_UpdateSequenceType(TCoding coding) const;
    void x_InitSequenceType(void);

    static const char* sx_GetConvertTable(TCoding src, TCoding dst);
    static const char* sx_GetComplementTable(TCoding coding);

    CConstRef<CSeqMap>    m_SeqMap;
    CScope*               m_Scope;
    TCoding               m_Coding;
    ENa_strand            m_Strand;

    mutable ESequenceType m_SequenceType;
    mutable TSeqPos       m_CachePos;
    mutable TSeqPos       m_CacheLen;
    mutable TCache_I      m_Cache;
    mutable TCacheData    m_CacheData;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
void CSeqVector::ClearCache(void)
{
    // Reset cached data
    m_CacheLen = 0;
}


inline
CSeqVector::TCoding CSeqVector::GetCoding(void) const
{
    return m_Coding;
}


inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos) const
{
    TSeqPos offset = pos - m_CachePos;
    return offset < m_CacheLen? m_Cache[offset]: x_GetResidue(pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.23  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.22  2003/01/03 19:45:44  dicuccio
* Replaced kPosUnknwon with kInvalidSeqPos (non-static variable; work-around for
* MSVC)
*
* Revision 1.21  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.20  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.19  2002/10/03 13:45:37  grichenk
* CSeqVector::size() made const
*
* Revision 1.18  2002/09/03 21:26:58  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.17  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.16  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.15  2002/05/17 17:14:50  grichenk
* +GetSeqData() for getting a range of characters from a seq-vector
*
* Revision 1.14  2002/05/09 14:16:29  grichenk
* sm_SizeUnknown -> kPosUnknown, minor fixes for unsigned positions
*
* Revision 1.13  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.12  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.11  2002/05/03 18:36:13  grichenk
* Fixed members initialization
*
* Revision 1.10  2002/04/30 14:32:51  ucko
* Have size() return int in keeping with its actual behavior; should cut
* down on warnings about truncation of 64-bit integers.
*
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

#endif  // SEQ_VECTOR__HPP
