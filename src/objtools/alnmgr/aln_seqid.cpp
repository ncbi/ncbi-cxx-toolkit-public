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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment seq-id
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/aln_seqid.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);


bool IAlnSeqId::IsProtein(void) const {
    return CSeq_inst::IsAa(GetSequenceType());
}


bool IAlnSeqId::IsNucleotide(void) const {
    return CSeq_inst::IsNa(GetSequenceType());
}



const CSeq_id& CAlnSeqId::GetSeqId(void) const {
    return *m_Seq_id;
}


string CAlnSeqId::AsString(void) const {
    return CSeq_id_Handle::AsString();
}


void CAlnSeqId::SetBioseqHandle(const CBioseq_Handle& handle) {
    m_BioseqHandle = handle;
    if ( !handle ) {
        return;
    }
    m_Mol = handle.GetSequenceType();
    m_BaseWidth = CSeq_inst::IsAa(m_Mol) ? 3 : 1;
}


IAlnSeqId::TMol CAlnSeqId::GetSequenceType(void) const {
    if (m_Mol == CSeq_inst::eMol_not_set) {
        switch(IdentifyAccession()) {
        case CSeq_id::fAcc_nuc:
            m_Mol = CSeq_inst::eMol_na;
            break;
        case CSeq_id::fAcc_prot:
            m_Mol = CSeq_inst::eMol_aa;
            break;
        default:
            break;
        }
    }
    return m_Mol;
}


int CAlnSeqId::GetBaseWidth(void) const {
    return m_BaseWidth;
} 


void CAlnSeqId::SetBaseWidth(int base_width) {
    m_BaseWidth = base_width;
} 


bool CAlnSeqId::operator== (const IAlnSeqId& id) const {
    return CSeq_id_Handle::operator== (dynamic_cast<const CSeq_id_Handle&>(id));
}


bool CAlnSeqId::operator!= (const IAlnSeqId& id) const {
    return CSeq_id_Handle::operator!= (dynamic_cast<const CSeq_id_Handle&>(id));
}


bool CAlnSeqId::operator<  (const IAlnSeqId& id) const {
    return CSeq_id_Handle::operator< (dynamic_cast<const CSeq_id_Handle&>(id));
} 



END_NCBI_SCOPE
