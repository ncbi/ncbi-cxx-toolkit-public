/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*
*/

#include <ncbi_pch.hpp>
#include <algo/align/util/align_shadow.hpp>

#include <numeric>

BEGIN_NCBI_SCOPE


template<>
CAlignShadow<CConstRef<objects::CSeq_id> >::CAlignShadow(
    const objects::CSeq_align& seq_align)
{
    USING_SCOPE(objects);

    if (seq_align.CheckNumRows() != 2) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Pairwise seq-align required to init align-shadow");
    }

    if (seq_align.GetSegs().IsDenseg() == false) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Must be a dense-seg to init align-shadow");
    }

    const CDense_seg &ds = seq_align.GetSegs().GetDenseg();
    const CDense_seg::TStrands& strands = ds.GetStrands();

    char query_strand = 0, subj_strand = 0;
    if(strands[0] == eNa_strand_plus) {
        query_strand = '+';
    }
    else if(strands[0] == eNa_strand_minus) {
        query_strand = '-';
    }

    if(strands[1] == eNa_strand_plus) {
        subj_strand = '+';
    }
    else if(strands[1] == eNa_strand_minus) {
        subj_strand = '-';
    }

    if(query_strand == 0 || subj_strand == 0) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Unexpected strand found when initializing "
                   "align-shadow from dense-seg");
    }

    m_Id[0].Reset(&seq_align.GetSeq_id(0));
    m_Id[1].Reset(&seq_align.GetSeq_id(1));

    if(query_strand == '+') {
        m_Box[0] = seq_align.GetSeqStart(0);
        m_Box[1] = seq_align.GetSeqStop(0);
    }
    else {
        m_Box[1] = seq_align.GetSeqStart(0);
        m_Box[0] = seq_align.GetSeqStop(0);
    }

    if(subj_strand == '+') {
        m_Box[2] = seq_align.GetSeqStart(1);
        m_Box[3] = seq_align.GetSeqStop(1);
    }
    else {
        m_Box[3] = seq_align.GetSeqStart(1);
        m_Box[2] = seq_align.GetSeqStop(1);
    }
}



template<>
CNcbiOstream& operator << (
    CNcbiOstream& os,
    const CAlignShadow<CConstRef<objects::CSeq_id> >& align_shadow )
{
    USING_SCOPE(objects);

    os  << align_shadow.GetId(0)->GetSeqIdString(true) << '\t'
        << align_shadow.GetId(1)->GetSeqIdString(true) << '\t';

    align_shadow.x_PartialSerialize(os);

    return os;
}


END_NCBI_SCOPE



/* 
 * $Log$
 * Revision 1.8  2005/04/18 15:24:47  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.7  2005/03/07 19:05:45  ucko
 * Don't mark methods that should be visible elsewhere as inline.
 * (In that case, their definitions would need to be in the header.)
 *
 * Revision 1.6  2004/12/22 22:14:18  kapustin
 * Move static messages to CSimpleMessager to satisfy Solaris/forte6u2
 *
 * Revision 1.5  2004/12/22 21:33:22  kapustin
 * Uncomment AlgoAlignUtil scope
 *
 * Revision 1.4  2004/12/22 21:26:18  kapustin
 * Move friend template definition to the header. 
 * Declare explicit specialization.
 *
 * Revision 1.3  2004/12/22 15:55:53  kapustin
 * A few type conversions to make msvc happy
 *
 * Revision 1.2  2004/12/21 21:27:42  ucko
 * Don't explicitly instantiate << for CConstRef<CSeq_id>, for which it
 * has instead been specialized.
 *
 * Revision 1.1  2004/12/21 20:07:47  kapustin
 * Initial revision
 *
 */

