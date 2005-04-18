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
#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <numeric>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//////////////////////////////////////////////////////////////////////////////
// explicit specializations

template<>
CBlastTabular<CConstRef<CSeq_id> >::CBlastTabular(const CSeq_align& seq_align):
    TParent(seq_align)
{
    const CDense_seg &ds = seq_align.GetSegs().GetDenseg();
    const CDense_seg::TLens& lens = ds.GetLens();

    SetLength(accumulate(lens.begin(), lens.end(), 0));

    double matches;
    seq_align.GetNamedScore("num_ident", matches);
    SetIdentity(float(matches/m_Length));

    double score;
    seq_align.GetNamedScore("bit_score", score);
    SetScore(float(score));

    double evalue;
    seq_align.GetNamedScore("sum_e", evalue);
    SetEValue(evalue);
}


template<>
CBlastTabular<CConstRef<CSeq_id> >::CBlastTabular(const char* m8)
{
    const char* p0 = m8, *p = p0;
    for(; *p && isspace(*p); ++p); // skip spaces
    for(p0 = p; *p && !isspace(*p); ++p); // get token
    if(*p) {
        string id1 (p0, p - p0);
        CSeq_id* pseqid = new CSeq_id(id1);
        m_Id[0].Reset(pseqid);
        if (m_Id[0]->Which() == CSeq_id::e_not_set) {
            pseqid->SetLocal().SetStr(id1);
        }
    }

    for(; *p && isspace(*p); ++p); // skip spaces
    for(p0 = p; *p && !isspace(*p); ++p); // get token
    if(*p) {
        string id2 (p0, p - p0);
        CSeq_id* pseqid = new CSeq_id(id2);
        m_Id[1].Reset(pseqid);
        if (m_Id[1]->Which() == CSeq_id::e_not_set) {
            pseqid->SetLocal().SetStr(id2);
        }
    }

    for(; *p && isspace(*p); ++p); // skip spaces

    x_PartialDeserialize(p);
}


END_NCBI_SCOPE

