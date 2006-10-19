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
 * Author: Christiam Camacho
 *
 */

/** @file blast_results.cpp
 * Implementation of classes which constitute the results of running a BLAST
 * search
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void
CSearchResults::GetMaskedQueryRegions
    (TMaskedQueryRegions& flt_query_regions) const
{
    flt_query_regions = m_Masks;
}

void
CSearchResults::SetMaskedQueryRegions
    (TMaskedQueryRegions& flt_query_regions)
{
    m_Masks = flt_query_regions;
}

TQueryMessages
CSearchResults::GetErrors(int min_severity) const
{
    TQueryMessages errs;
    
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() >= min_severity) {
            errs.push_back(*iter);
        }
    }
    
    return errs;
}

bool
CSearchResults::HasAlignments() const
{
    if (m_Alignment.Empty()) {
        return false;
    }
    // this are the results for a single query...
    _ASSERT(m_Alignment->Get().size() == 1);    
    // ... which are stored in a discontinuous Seq-align
    _ASSERT(m_Alignment->Get().front()->GetSegs().IsDisc());
    return m_Alignment->Get().front()->GetSegs().GetDisc().Get().size() != 0;
}

CConstRef<CSeq_id>
CSearchResults::GetSeqId() const
{
    return m_QueryId;
}

CConstRef<CSearchResults>
CSearchResultSet::operator[](const objects::CSeq_id & ident) const
{
    for( size_t i = 0;  i < m_Results.size();  i++ ) {
        if ( CSeq_id::e_YES == ident.Compare(*m_Results[i]->GetSeqId()) ) {
            return m_Results[i];
        }
    }
    
    return CConstRef<CSearchResults>();
}

CRef<CSearchResults>
CSearchResultSet::operator[](const objects::CSeq_id & ident)
{
    for( size_t i = 0;  i < m_Results.size();  i++ ) {
        if ( CSeq_id::e_YES == ident.Compare(*m_Results[i]->GetSeqId()) ) {
            return m_Results[i];
        }
    }
    
    return CRef<CSearchResults>();
}

static CConstRef<CSeq_id>
s_ExtractSeqId(CConstRef<CSeq_align_set> align_set)
{
    CConstRef<CSeq_id> retval;
    
    if (! (align_set.Empty() || align_set->Get().empty())) {
        // index 0 = query, index 1 = subject
        const int query_index = 0;
        
        CRef<CSeq_align> first_disc_align = align_set->Get().front();
        
        CRef<CSeq_align> first_align = 
            first_disc_align->GetSegs().GetDisc().Get().front();
        
        retval.Reset(& first_align->GetSeq_id(query_index));
    }
    
    return retval;
}

CSearchResultSet::CSearchResultSet(vector< CConstRef<CSeq_id> > queries,
                                   TSeqAlignVector              aligns,
                                   TSearchMessages              msg_vec,
                                   TAncillaryVector             ancillary_data)
{
    if (ancillary_data.empty()) {
        ancillary_data.resize(aligns.size());
    }
    x_Init(queries, aligns, msg_vec, ancillary_data);
}

CSearchResultSet::CSearchResultSet(TSeqAlignVector aligns,
                                   TSearchMessages msg_vec)
{
    vector< CConstRef<CSeq_id> > queries;
    TAncillaryVector ancillary_data(aligns.size()); // no ancillary_data
    
    for(size_t i = 0; i < aligns.size(); i++) {
        queries.push_back(s_ExtractSeqId(aligns[i]));
    }
    
    x_Init(queries, aligns, msg_vec, ancillary_data);
}

void CSearchResultSet::x_Init(vector< CConstRef<CSeq_id> > queries,
                              TSeqAlignVector              aligns,
                              TSearchMessages              msg_vec,
                              TAncillaryVector             ancillary_data)
{
    _ASSERT(aligns.size() == ancillary_data.size());
    _ASSERT(aligns.size() == msg_vec.size());
    _ASSERT(aligns.size() == queries.size());
    
    m_Results.resize(aligns.size());
    
    for(size_t i = 0; i < aligns.size(); i++) {
        m_Results[i].Reset(new CSearchResults(queries[i],
                                              aligns[i],
                                              msg_vec[i],
                                              ancillary_data[i]));
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
