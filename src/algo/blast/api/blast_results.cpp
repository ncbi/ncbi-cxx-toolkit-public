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
#include <objects/seqloc/Seq_id.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

static void 
s_InitializeKarlinBlk(Blast_KarlinBlk* src, Blast_KarlinBlk** dest)
{
    _ASSERT(dest);

    if (src && src->Lambda >= 0) {
        *dest = Blast_KarlinBlkNew();
        Blast_KarlinBlkCopy(*dest, src);
    }
}


CBlastAncillaryData::CBlastAncillaryData(EBlastProgramType program_type,
                    int query_number,
                    const BlastScoreBlk *sbp,
                    const BlastQueryInfo *query_info)
: m_UngappedKarlinBlk(0), m_GappedKarlinBlk(0), m_PsiUngappedKarlinBlk(0),
  m_PsiGappedKarlinBlk(0), m_SearchSpace(0), m_LengthAdjustment(0)
{
    int i;
    int context_per_query = BLAST_GetNumberOfContexts(program_type);

    // find the first valid context corresponding to this query
    for (i = 0; i < context_per_query; i++) {
        BlastContextInfo *ctx = query_info->contexts + 
                                query_number * context_per_query + i;
        if (ctx->is_valid) {
            m_SearchSpace = ctx->eff_searchsp;
	    m_LengthAdjustment = ctx->length_adjustment;
            break;
        }
    }
    if (i >= context_per_query) {
        return; // we didn't find a valid context :(
    }

    // fill in the Karlin blocks for that context, if they
    // are valid
    const int ctx_index = query_number * context_per_query + i;
    if (sbp->kbp_std) {
        s_InitializeKarlinBlk(sbp->kbp_std[ctx_index], &m_UngappedKarlinBlk);
    }
    if (sbp->kbp_gap) {
        s_InitializeKarlinBlk(sbp->kbp_gap[ctx_index], &m_GappedKarlinBlk);
    }
    if (sbp->kbp_psi) {
        s_InitializeKarlinBlk(sbp->kbp_psi[ctx_index], &m_PsiUngappedKarlinBlk);
    }
    if (sbp->kbp_gap_psi) {
        s_InitializeKarlinBlk(sbp->kbp_gap_psi[ctx_index], 
                              &m_PsiGappedKarlinBlk);
    }
}

CBlastAncillaryData::CBlastAncillaryData(pair<double, double> lambda,
                                         pair<double, double> k,
                                         pair<double, double> h,
                                         Int8 effective_search_space,
                                         bool is_psiblast /* = false */)
: m_UngappedKarlinBlk(0), m_GappedKarlinBlk(0), m_PsiUngappedKarlinBlk(0),
  m_PsiGappedKarlinBlk(0), m_SearchSpace(0)
{
    if (is_psiblast) {
        m_PsiGappedKarlinBlk = Blast_KarlinBlkNew();
        m_PsiGappedKarlinBlk->Lambda = lambda.second;
        m_PsiGappedKarlinBlk->K = k.second;
        m_PsiGappedKarlinBlk->H = h.second;

        m_PsiUngappedKarlinBlk = Blast_KarlinBlkNew();
        m_PsiUngappedKarlinBlk->Lambda = lambda.first;
        m_PsiUngappedKarlinBlk->K = k.first;
        m_PsiUngappedKarlinBlk->H = h.first;
    } else {
        m_GappedKarlinBlk = Blast_KarlinBlkNew();
        m_GappedKarlinBlk->Lambda = lambda.second;
        m_GappedKarlinBlk->K = k.second;
        m_GappedKarlinBlk->H = h.second;

        m_UngappedKarlinBlk = Blast_KarlinBlkNew();
        m_UngappedKarlinBlk->Lambda = lambda.first;
        m_UngappedKarlinBlk->K = k.first;
        m_UngappedKarlinBlk->H = h.first;
    }

    m_SearchSpace = effective_search_space;
}

CBlastAncillaryData::~CBlastAncillaryData()
{
    Blast_KarlinBlkFree(m_UngappedKarlinBlk);
    Blast_KarlinBlkFree(m_GappedKarlinBlk);
    Blast_KarlinBlkFree(m_PsiUngappedKarlinBlk);
    Blast_KarlinBlkFree(m_PsiGappedKarlinBlk);
}

void 
CBlastAncillaryData::do_copy(const CBlastAncillaryData& other) 
{
    if (this != &other) {
        m_UngappedKarlinBlk = m_GappedKarlinBlk = NULL;
        m_SearchSpace = other.m_SearchSpace;

        if (other.m_UngappedKarlinBlk) {
            m_UngappedKarlinBlk = Blast_KarlinBlkNew();
            Blast_KarlinBlkCopy(m_UngappedKarlinBlk, 
                                other.m_UngappedKarlinBlk);
        }
        if (other.m_GappedKarlinBlk) {
            m_GappedKarlinBlk = Blast_KarlinBlkNew();
            Blast_KarlinBlkCopy(m_GappedKarlinBlk, other.m_GappedKarlinBlk);
        }
        if (other.m_PsiUngappedKarlinBlk) {
            m_PsiUngappedKarlinBlk = Blast_KarlinBlkNew();
            Blast_KarlinBlkCopy(m_PsiUngappedKarlinBlk, 
                                other.m_PsiUngappedKarlinBlk);
        }
        if (other.m_PsiGappedKarlinBlk) {
            m_PsiGappedKarlinBlk = Blast_KarlinBlkNew();
            Blast_KarlinBlkCopy(m_PsiGappedKarlinBlk, 
                                other.m_PsiGappedKarlinBlk);
        }
    }
}

CSearchResults::CSearchResults(CConstRef<objects::CSeq_id> query,
                               CRef<objects::CSeq_align_set> align,
                               const TQueryMessages& errs,
                               CRef<CBlastAncillaryData> ancillary_data,
                               const TMaskedQueryRegions* query_masks,
                               const string& rid /* = kEmptyStr */,
                               const SPHIQueryInfo *phi_query_info /* = NULL */)
: m_QueryId(query), m_Alignment(align), m_Errors(errs), 
  m_AncillaryData(ancillary_data), m_RID(kEmptyStr), m_PhiQueryInfo(0)
{
    if (query_masks)
        SetMaskedQueryRegions(*query_masks);
    if (phi_query_info)
        m_PhiQueryInfo = SPHIQueryInfoCopy(phi_query_info);
}

CSearchResults::~CSearchResults()
{
    if (m_PhiQueryInfo) {
        SPHIQueryInfoFree(m_PhiQueryInfo);
    }
}

void
CSearchResults::GetMaskedQueryRegions
    (TMaskedQueryRegions& flt_query_regions) const
{
    flt_query_regions = m_Masks;
}

void
CSearchResults::SetMaskedQueryRegions
    (const TMaskedQueryRegions& flt_query_regions)
{
    m_Masks.clear();
    copy(flt_query_regions.begin(), flt_query_regions.end(), 
         back_inserter(m_Masks));
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

string
CSearchResults::GetErrorStrings() const
{
    if (m_Errors.empty()) {
        return string();
    }

    string retval(m_Errors.GetQueryId());
    if ( !retval.empty() ) {    // in case the query id is not known
        retval += ": ";
    }
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() >= eBlastSevError) {
            retval += (*iter)->GetMessage() + " ";
        }
    }
    return retval;
}

string
CSearchResults::GetWarningStrings() const
{
    if (m_Errors.empty()) {
        return string();
    }

    string retval(m_Errors.GetQueryId());
    if ( !retval.empty() ) {    // in case the query id is not known
        retval += ": ";
    }
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() == eBlastSevWarning) {
            retval += (*iter)->GetMessage() + " ";
        }
    }
    return retval;
}

bool
CSearchResults::HasErrors() const
{
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() >= eBlastSevError) {
            return true;
        }
    }
    return false;
}

bool
CSearchResults::HasWarnings() const
{
    ITERATE(TQueryMessages, iter, m_Errors) {
        if ((**iter).GetSeverity() == eBlastSevWarning) {
            return true;
        }
    }
    return false;
}

bool
CSearchResults::HasAlignments() const
{
    if (m_Alignment.Empty()) {
        return false;
    }

    return m_Alignment->Get().size() != 0  &&
         m_Alignment->Get().front()->IsSetSegs();
}

CConstRef<CSeq_id>
CSearchResults::GetSeqId() const
{
    return m_QueryId;
}

void
CSearchResults::GetSubjectMasks(TSeqLocInfoVector& subj_masks) const
{
    subj_masks = m_SubjectMasks;
}

void
CSearchResults::SetSubjectMasks(const TSeqLocInfoVector& subj_masks)
{
    m_SubjectMasks.clear();
    copy(subj_masks.begin(), subj_masks.end(), back_inserter(m_SubjectMasks));
}

CSearchResults&
CSearchResultSet::GetResults(size_type qi, size_type si)
{
    if (m_ResultType != eSequenceComparison) {
        NCBI_THROW(CBlastException, eNotSupported, "Invalid method accessed");
    }
    return *m_Results[qi * (GetNumResults() / m_NumQueries) + si];
}

const CSearchResults&
CSearchResultSet::GetResults(size_type qi, size_type si) const
{
    return const_cast<CSearchResultSet*>(this)->GetResults(qi, si);
}

CConstRef<CSearchResults>
CSearchResultSet::operator[](const objects::CSeq_id & ident) const
{
    if (m_ResultType != eDatabaseSearch) {
        NCBI_THROW(CBlastException, eNotSupported, "Invalid method accessed");
    }
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
    if (m_ResultType != eDatabaseSearch) {
        NCBI_THROW(CBlastException, eNotSupported, "Invalid method accessed");
    }
    for( size_t i = 0;  i < m_Results.size();  i++ ) {
        if ( CSeq_id::e_YES == ident.Compare(*m_Results[i]->GetSeqId()) ) {
            return m_Results[i];
        }
    }
    
    return CRef<CSearchResults>();
}

/// Find the first alignment in a set of blast results, and
//  return the sequence identifier of the first sequence in the alignment.
//  All alignments in the blast results are assumed to contain the
//  same identifier
// @param align_set The blast results
// @return The collection of sequence ID's corresponding to the
//         first sequence of the first alignment
static CConstRef<CSeq_id>
s_ExtractSeqId(CConstRef<CSeq_align_set> align_set)
{
    CConstRef<CSeq_id> retval;
    
    if (! (align_set.Empty() || align_set->Get().empty())) {
        // index 0 = query, index 1 = subject
        const int kQueryIndex = 0;
        
        CRef<CSeq_align> align = align_set->Get().front();

        if (align->GetSegs().IsDisc() == true)
        {
        
            if (align->GetSegs().GetDisc().Get().empty())
                return retval;
        
            CRef<CSeq_align> first_align = align->GetSegs().GetDisc().Get().front();
            retval.Reset(& align->GetSeq_id(kQueryIndex));
        }
        else
        {
            retval.Reset(& align->GetSeq_id(kQueryIndex));
        }
    }
    
    return retval;
}

CSearchResultSet::CSearchResultSet(EResultType res_type /* = eDatabaseSearch*/)
: m_ResultType(res_type)
{}

CSearchResultSet::CSearchResultSet(
    TQueryIdVector               queries,
    TSeqAlignVector              aligns,
    TSearchMessages              msg_vec,
    TAncillaryVector             ancillary_data /* = TAncillaryVector() */,
    const TSeqLocInfoVector*     query_masks /* = NULL */,
    EResultType                  res_type /* = eDatabaseSearch */,
    const SPHIQueryInfo*         phi_query_info /* = NULL */)
: m_ResultType(res_type)
{
    if (ancillary_data.empty()) {
        ancillary_data.resize(aligns.size());
    }
    x_Init(queries, aligns, msg_vec, ancillary_data, query_masks, phi_query_info);
}

CSearchResultSet::CSearchResultSet(TSeqAlignVector aligns,
                                   TSearchMessages msg_vec,
                                   EResultType res_type /* = eDatabaseSearch */)
: m_ResultType(res_type)
{
    vector< CConstRef<CSeq_id> > queries;
    TAncillaryVector ancillary_data(aligns.size()); // no ancillary_data
    
    for(size_t i = 0; i < aligns.size(); i++) {
        queries.push_back(s_ExtractSeqId(aligns[i]));
    }
    
    x_Init(queries, aligns, msg_vec, ancillary_data, NULL);
}

void
CSearchResultSet::SetFilteredQueryRegions(const TSeqLocInfoVector& orig_masks)
{
    if (orig_masks.empty()) {
        return;
    }
    TSeqLocInfoVector masks;

    if (m_ResultType == eSequenceComparison &&
        orig_masks.size() != m_Results.size()) {
        // Make the number of masks match the number of results for bl2seq if
        // it already isn't the case
        const size_t kNumQueries = orig_masks.size();
        const size_t kNumSubjects = m_Results.size() / kNumQueries;
        masks.resize(m_Results.size());
        for (size_t i = 0; i < m_Results.size(); i++) {
            const TMaskedQueryRegions& mqr = orig_masks[i/kNumSubjects];
            copy(mqr.begin(), mqr.end(), back_inserter(masks[i]));
        }
    } else {
        masks = orig_masks;
    }
    _ASSERT(masks.size() == m_Results.size());

    if (m_IsPhiBlast) {
        for (size_t i = 0; i < m_Results.size(); i++) {
            m_Results[i]->SetMaskedQueryRegions(masks[0]);
        }
    } else {
        _ASSERT(masks.size() == m_Results.size());
        for (size_t i = 0; i < m_Results.size(); i++) {
            m_Results[i]->SetMaskedQueryRegions(masks[i]);
        }
    }
}

void CSearchResultSet::x_Init(TQueryIdVector&                    queries,
                              TSeqAlignVector                    aligns,
                              TSearchMessages                    msg_vec,
                              TAncillaryVector                   ancillary_data,
                              const TSeqLocInfoVector*           query_masks,
                              const SPHIQueryInfo*               phi_query_info)
{
    _ASSERT(queries.size() == aligns.size());
    _ASSERT(aligns.size() == msg_vec.size());
    _ASSERT(aligns.size() == ancillary_data.size());

    m_IsPhiBlast = (phi_query_info != NULL) ? true : false;

    // determine the number of unique queries
    if (m_ResultType == eSequenceComparison)
    {
        // determine how many times is the first query id
        // repeated in the queries vector
        int num_repeated_ids = 1;
        for (size_t i_id = 1; i_id < queries.size(); i_id++)
        {
            if (queries[i_id]->Match(queries[0].GetObject()))
            {
                num_repeated_ids++;
            }
        }
        // calculate the actual number of queries
        m_NumQueries = queries.size() / num_repeated_ids;
    }
    else    // database search, no repeated query ids
    {
        m_NumQueries = queries.size();
    }

    m_Results.resize(aligns.size());
    
    for(size_t i = 0; i < aligns.size(); i++) {
        m_Results[i].Reset(new CSearchResults(queries[i],
                                              aligns[i],
                                              msg_vec[i],
                                              ancillary_data[i],
                                              NULL,
                                              kEmptyStr,
                                              phi_query_info));
    }
    if (query_masks) {
        SetFilteredQueryRegions(*query_masks);
    }
}

void
CSearchResultSet::push_back(CSearchResultSet::value_type& element)
{
    m_Results.push_back(element);
    m_NumQueries++;
}

void 
CSearchResultSet::SetRID(const string& rid)
{
    NON_CONST_ITERATE(vector<CSearchResultSet::value_type>, itr, m_Results) {
        (*itr)->SetRID(rid);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
