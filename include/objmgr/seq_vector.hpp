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


#include <objects/seq/Seq_data.hpp>

#include <objects/objmgr1/seq_map.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

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

private:
    friend class CBioseq_Handle;

    // Created by CScope only
    CSeqVector(const CBioseq_Handle& handle, bool plus_strand, CScope& scope);

    // Get residue assuming the data in m_CurrentData are valid
    TResidue x_GetResidue(int pos);

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
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


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

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_VECTOR__HPP
