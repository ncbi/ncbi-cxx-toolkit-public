/* $Id$
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
 * Author:  Greg Boratyn
 *
 */

/** @file magicblast.cpp
 * Implementation of CMagicBlast.
 */

#include <ncbi_pch.hpp>
#include <objects/general/User_object.hpp>
#include <algo/blast/core/spliced_hits.h>
#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/magicblast.hpp>

#include "blast_seqalign.hpp"
#include "blast_aux_priv.hpp"
#include "../core/jumper.h"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast);

CMagicBlast::CMagicBlast(CRef<IQueryFactory> query_factory,
                         CRef<CLocalDbAdapter> blastdb,
                         CRef<CMagicBlastOptionsHandle> options)
    : m_Queries(query_factory),
      m_LocalDbAdapter(blastdb),
      m_Options(&options->SetOptions())
{
    x_Validate();
}


CRef<CSeq_align_set> CMagicBlast::Run(void)
{
    x_Run();

    // close HSP stream and create internal results structure
    BlastMappingResults* results = Blast_MappingResultsNew();
    CRef< CStructWrapper<BlastMappingResults> > wrapped_results;
    wrapped_results.Reset(WrapStruct(results, Blast_MappingResultsFree));

    BlastHSPStreamMappingClose(m_InternalData->m_HspStream->GetPointer(),
                               results);

    // build and return results
    return x_BuildSeqAlignSet(results);
}


CRef<CMagicBlastResultSet> CMagicBlast::RunEx(void)
{
    x_Run();

    // close HSP stream and create internal results structure
    BlastMappingResults* results = Blast_MappingResultsNew();
    CRef< CStructWrapper<BlastMappingResults> > wrapped_results;
    wrapped_results.Reset(WrapStruct(results, Blast_MappingResultsFree));

    BlastHSPStreamMappingClose(m_InternalData->m_HspStream->GetPointer(),
                               results);

    // build and return results
    return x_BuildResultSet(results);
}


int CMagicBlast::x_Run(void)
{
    m_PrelimSearch.Reset(new CBlastPrelimSearch(m_Queries,
                                                m_Options,
                                                m_LocalDbAdapter,
                                                GetNumberOfThreads()));

    int status = m_PrelimSearch->CheckInternalData();
    if (status != 0)
    {
         // Search was not run, but we send back an empty CSearchResultSet.
         CRef<ILocalQueryData> local_query_data =
             m_Queries->MakeLocalQueryData(m_Options);

         vector< CConstRef<objects::CSeq_id> > seqid_vec;
         vector< CRef<CBlastAncillaryData> > ancill_vec;
         TSeqAlignVector sa_vec;
         size_t index;
         unsigned int num_subjects = 0;
         if (m_LocalDbAdapter.NotEmpty() && !m_LocalDbAdapter->IsBlastDb() &&
             !m_LocalDbAdapter->IsDbScanMode()) {

             IBlastSeqInfoSrc *  subject_infosrc =
                 m_LocalDbAdapter->MakeSeqInfoSrc();

             if(subject_infosrc != NULL) {
            	 num_subjects = subject_infosrc->Size();
             }
         }
         TSearchMessages msg_vec;
         for (index=0; index<local_query_data->GetNumQueries(); index++)
         {
              CConstRef<objects::CSeq_id> query_id(
                                 local_query_data->GetSeq_loc(index)->GetId());

              TQueryMessages q_msg;
              local_query_data->GetQueryMessages(index, q_msg);
              msg_vec.push_back(q_msg);
              seqid_vec.push_back(query_id);
              CRef<objects::CSeq_align_set> tmp_align;
              sa_vec.push_back(tmp_align);
              pair<double, double> tmp_pair(-1.0, -1.0);
              CRef<CBlastAncillaryData>  tmp_ancillary_data(
                             new CBlastAncillaryData(tmp_pair, tmp_pair,
                                                     tmp_pair, 0));

              ancill_vec.push_back(tmp_ancillary_data);

              for(unsigned int i=1; i < num_subjects; i++) {
            	  TQueryMessages msg;
                  msg_vec.push_back(msg);
                  seqid_vec.push_back(query_id);
                  CRef<objects::CSeq_align_set> tmp_align;
                  sa_vec.push_back(tmp_align);
           	      CRef<CBlastAncillaryData>  tmp_ancillary_data(
                     new CBlastAncillaryData(tmp_pair, tmp_pair, tmp_pair, 0));
           	      ancill_vec.push_back(tmp_ancillary_data);
              }
         }
         msg_vec.Combine(m_PrelimSearch->GetSearchMessages());

         // FIXME: Report search messages
    }

    try {
        m_PrelimSearch->SetNumberOfThreads(GetNumberOfThreads());
        // do mapping
        m_InternalData = m_PrelimSearch->Run();

	}
    catch( CIndexedDbException & ) {
	    throw;
	}
    catch (CBlastException & e) {
		if(e.GetErrCode() == CBlastException::eCoreBlastError) {
			throw;
		}
    }
    catch (...) {

    }

    return 0;
}


void CMagicBlast::x_Validate(void)
{
    if (m_Options.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }

    if (m_Queries.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing query");
    }

    if (m_LocalDbAdapter.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing database or subject sequences");
    }
}

// Compute BTOP string and percent identity from JumperEdits structure that
// contains base mismatch infotmation
static void s_ComputeBtopAndIdentity(const HSPChain* chain,
                                     string& btop,
                                     string& md_tag,
                                     double& perc_id)
{
    _ASSERT(chain);
    _ASSERT(chain->hsps->hsp);
    const Uint1 kGap = 15;

    int num_identical = 0;
    int len = 0;
    int md_matches = 0;
    HSPContainer* h = chain->hsps;
    BlastHSP* prev = NULL;
    for (; h; prev = h->hsp, h = h->next) {
        const BlastHSP* hsp = h->hsp;
        const JumperEditsBlock* hsp_edits = hsp->map_info->edits;

        if (prev) {
            int subject_gap = hsp->subject.offset - prev->subject.end;
            if (subject_gap > 0 &&
                (prev->map_info->right_edge & MAPPER_SPLICE_SIGNAL) &&
                (hsp->map_info->left_edge & MAPPER_SPLICE_SIGNAL)) {

                // intron
                btop += (string)"^" + NStr::IntToString(subject_gap) + "^";
            }
            else if (subject_gap > 0) {
                // gap in subject/reference
                btop += (string)"%" + NStr::IntToString(subject_gap) + "%";

                md_tag += (string)"!" + NStr::IntToString(subject_gap) + "!";
            }

            int query_gap = hsp->query.offset - prev->query.end;
            if (query_gap > 0) {
                btop += (string)"_" + NStr::IntToString(query_gap) + "_";
                len += hsp->query.offset - prev->query.end;
            }
            else if (query_gap < 0) {
                btop += (string)"(" + NStr::IntToString(-query_gap) + ")";
            }
        }

        int query_pos = hsp->query.offset;
        int num_matches = 0;
        for (int i=0;i < hsp_edits->num_edits;i++) {
            num_matches = hsp_edits->edits[i].query_pos - query_pos;
            query_pos += num_matches;

            _ASSERT(num_matches >= 0);
            num_identical += num_matches;
            if (num_matches > 0) {
                btop += NStr::IntToString(num_matches);
            }

            char buff[3];
            buff[0] = BLASTNA_TO_IUPACNA[(int)hsp_edits->edits[i].query_base];
            buff[1] = BLASTNA_TO_IUPACNA[(int)hsp_edits->edits[i].subject_base];
            buff[2] = 0;
            btop += (string)buff;

            // assemble SAM MD tag
            if (buff[1] != '-') {
                if (i > 0 && hsp_edits->edits[i - 1].query_base == kGap &&
                    buff[0] == '-' && num_matches == 0) {

                    md_tag += (string)(&buff[1]);
                }
                else {
                    md_tag += NStr::IntToString(num_matches + md_matches);
                    if (buff[0] == '-') {
                        md_tag += "^";
                    }
                    md_tag += (string)(&buff[1]);
                    md_matches = 0;
                }
            }
            else {
                md_matches += num_matches;
            }

            len++;
            if (hsp_edits->edits[i].query_base != kGap) {
                query_pos++;
            }
        }
        num_matches = hsp->query.end - query_pos;
        num_identical += num_matches;
        if (num_matches > 0) {
            btop += NStr::IntToString(num_matches);
            md_matches += num_matches;
        }
    }
    if (md_matches > 0) {
        md_tag += NStr::IntToString(md_matches);
    }
    len += num_identical;

    perc_id = (double)(num_identical * 100) / (double)len;
}


static CRef<CSeq_align> s_CreateSeqAlign(const HSPChain* chain,
                                         CRef<ILocalQueryData>& qdata,
                                         CRef<IBlastSeqInfoSrc>& seqinfo_src,
                                         const BlastQueryInfo* query_info)
{
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial);
    align->SetDim(2);

    int query_index = chain->context / NUM_STRANDS;
    CConstRef<CSeq_loc> query_loc = qdata->GetSeq_loc(query_index);
    CRef<CSeq_id> query_id(new CSeq_id);
    SerialAssign(*query_id, CSeq_loc_CI(*query_loc).GetSeq_id());
    _ASSERT(query_id);
    TSeqPos query_length = qdata->GetSeqLength(query_index);

    CRef<CSeq_id> subject_id;
    TSeqPos subj_length;
    GetSequenceLengthAndId(seqinfo_src, chain->oid, subject_id,
                           &subj_length);


    MakeSplicedSeg(align->SetSegs().SetSpliced(), query_id, subject_id,
                   query_length, chain);

    // alignment score
    align->SetNamedScore(CSeq_align::eScore_Score, chain->score);

    // user objec stores auxiliary information needed for various output
    // formats
    CRef<CUser_object> user_object(new CUser_object);
    user_object->SetType().SetStr("Mapper Info");
    align->SetExt().push_back(user_object);
    // for SAM
    // context is needed mostly for printing query sequences, mostly for
    // convinience and fast lookup
    user_object->AddField("context", chain->context);
    user_object->AddField("num_hits", chain->count);

    // for tabular
    string btop;
    string md_tag;
    double perc_id;
    s_ComputeBtopAndIdentity(chain, btop, md_tag, perc_id);
    user_object->AddField("btop", btop);
    user_object->AddField("md_tag", md_tag);
    align->SetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped,
                         perc_id);

    // to diffierentiate between the first and last segment of a paired read
    // if sequence ids cannot be trusted
    user_object->AddField("segment",
                          query_info->contexts[chain->context].segment_flags);

    return align;
}

CRef<CSeq_align_set> CMagicBlast::x_CreateSeqAlignSet(
                                           const HSPChain* chains,
                                           CRef<ILocalQueryData> qdata,
                                           CRef<IBlastSeqInfoSrc> seqinfo_src,
                                           const BlastQueryInfo* query_info)
{
    CRef<CSeq_align_set> seq_aligns = CreateEmptySeq_align_set();

    // single spliced alignment
    for (const HSPChain* chain = chains; chain; chain = chain->next) {

        // mate pairs are processed together when the first one is
        // encountered, so skip the second of the pair
        if (chain->pair && chain->context > chain->pair->context) {
            continue;
        }

        CRef<CSeq_align> align;

        // pairs are reported as disc seg alignment composed of two
        // spliced segs
        if (chain->pair) {
            align.Reset(new CSeq_align);
            align->SetType(CSeq_align::eType_partial);
            align->SetDim(2);

            CSeq_align::TSegs::TDisc& disc = align->SetSegs().SetDisc();
            disc.Set().push_back(s_CreateSeqAlign(chain, qdata, seqinfo_src,
                                                  query_info));
            disc.Set().push_back(s_CreateSeqAlign(chain->pair, qdata,
                                                  seqinfo_src, query_info));
        }
        else {
            align = s_CreateSeqAlign(chain, qdata, seqinfo_src, query_info);
        }

        seq_aligns->Set().push_back(align);
    }

    return seq_aligns;
}


CRef<CSeq_align_set> CMagicBlast::x_BuildSeqAlignSet(
                                           const BlastMappingResults* results)
{
    TSeqAlignVector aligns;
    aligns.reserve(results->num_queries);

    CSearchResultSet::TQueryIdVector query_ids;
    query_ids.reserve(results->num_queries);

    CRef<ILocalQueryData> query_data = m_Queries->MakeLocalQueryData(m_Options);
    BlastQueryInfo* query_info = m_InternalData->m_QueryInfo;
    CRef<IBlastSeqInfoSrc> seqinfo_src;
    seqinfo_src.Reset(m_LocalDbAdapter->MakeSeqInfoSrc());
    _ASSERT(seqinfo_src);

    _ASSERT(results->num_queries == (int)query_data->GetNumQueries());

    CRef<CSeq_align_set> retval(new CSeq_align_set);
    for (int index=0;index < results->num_queries;index++) {
        HSPChain* chains = results->chain_array[index];
        CRef<CSeq_align_set> seq_aligns(x_CreateSeqAlignSet(chains, query_data,
                                                            seqinfo_src,
                                                            query_info));

        for (auto it: seq_aligns->Get()) {
            retval->Set().push_back(it);
        }
    }

    return retval;
}

// paired alignments go first
struct seq_align_pairs_first {
    bool operator()(const CRef<CSeq_align>& a, const CRef<CSeq_align>& b) {
        return a->GetSegs().IsDisc() && !b->GetSegs().IsDisc();
    }
};


CRef<CMagicBlastResultSet> CMagicBlast::x_BuildResultSet(
                                           const BlastMappingResults* results)
{
    CRef<ILocalQueryData> query_data = m_Queries->MakeLocalQueryData(m_Options);
    CRef<IBlastSeqInfoSrc> seqinfo_src;
    seqinfo_src.Reset(m_LocalDbAdapter->MakeSeqInfoSrc());
    _ASSERT(seqinfo_src);

    BlastQueryInfo* query_info = m_InternalData->m_QueryInfo;
    _ASSERT(results->num_queries == (int)query_data->GetNumQueries());

    const TSeqLocInfoVector& query_masks = m_PrelimSearch->GetQueryMasks();

    CRef<CMagicBlastResultSet> retval(new CMagicBlastResultSet);
    retval->reserve(results->num_queries);

    for (int index=0;index < results->num_queries;index++) {
        HSPChain* chains = results->chain_array[index];
        CConstRef<CSeq_id> query_id(query_data->GetSeq_loc(index)->GetId());
        CRef<CSeq_align_set> aligns(x_CreateSeqAlignSet(chains, query_data,
                                                        seqinfo_src,
                                                        query_info));

        int query_length =
            query_info->contexts[index * NUM_STRANDS].query_length;
        CRef<CMagicBlastResults> res;

        if (query_info->contexts[index * NUM_STRANDS].segment_flags ==
            eFirstSegment) {

            _ASSERT(query_info->contexts[(index + 1) * NUM_STRANDS]
                    .segment_flags == eLastSegment);

            CConstRef<CSeq_id> mate_id(
                                 query_data->GetSeq_loc(index + 1)->GetId());

            int mate_length =
                query_info->contexts[(index + 1) * NUM_STRANDS].query_length;

            chains = results->chain_array[index + 1];
            CRef<CSeq_align_set> mate_aligns(x_CreateSeqAlignSet(chains,
                                                                 query_data,
                                                                 seqinfo_src,
                                                                 query_info));

            for (auto it: mate_aligns->Get()) {
                aligns->Set().push_back(it);
            }

            // sort results so that pairs go first
            aligns->Set().sort(seq_align_pairs_first());

            res.Reset(new CMagicBlastResults(query_id, mate_id, aligns,
                                             &query_masks[index],
                                             &query_masks[index + 1],
                                             query_length,
                                             mate_length));
            index++;
        }
        else {
            res.Reset(new CMagicBlastResults(query_id, aligns,
                                             &query_masks[index],
                                             query_length));
        }

        retval->push_back(res);
    }

    return retval;
}


CMagicBlastResults::CMagicBlastResults(CConstRef<CSeq_id> query_id,
                           CConstRef<CSeq_id> mate_id,
                           CRef<CSeq_align_set> aligns,
                           const TMaskedQueryRegions* query_mask /* = NULL */,
                           const TMaskedQueryRegions* mate_mask /* = NULL */,
                           int query_length /* = 0 */,
                           int mate_length /* = 0 */)
    : m_QueryId(query_id),
      m_MateId(mate_id),
      m_Aligns(aligns),
      m_Paired(true)
{
    x_SetInfo(query_length, query_mask, mate_length, mate_mask);
}


CMagicBlastResults::CMagicBlastResults(CConstRef<CSeq_id> query_id,
                            CRef<CSeq_align_set> aligns,
                            const TMaskedQueryRegions* query_mask /* = NULL */,
                            int query_length /* = 0 */)
    : m_QueryId(query_id),
      m_Aligns(aligns),
      m_Paired(false)
{
    x_SetInfo(query_length, query_mask);
}


// Sort alignments so that paired, forward-reversed alignments are first
struct compare_alignments_fwrev_first
{
    bool operator()(const CRef<CSeq_align>& a, const CRef<CSeq_align>& b)
    {
        if (a->GetSegs().IsDisc() && b->GetSegs().IsDisc()) {
            const CSeq_align& a_first = *a->GetSegs().GetDisc().Get().front();
            const CSeq_align& a_second = *a->GetSegs().GetDisc().Get().back();
            const CSeq_align& b_first = *b->GetSegs().GetDisc().Get().front();
            const CSeq_align& b_second = *b->GetSegs().GetDisc().Get().back();

            if (a_first.GetSeqStrand(0) == eNa_strand_plus &&
                a_second.GetSeqStrand(0) == eNa_strand_minus &&
                a_first.GetSeqStart(1) <= a_second.GetSeqStart(1) &&
                (b_first.GetSeqStrand(0) != eNa_strand_plus ||
                 b_second.GetSeqStrand(0) != eNa_strand_minus ||
                 b_first.GetSeqStart(1) > b_second.GetSeqStart(1))) {

                return true;
            }

            return false;
        }
        
        return (a->GetSegs().IsDisc() && !b->GetSegs().IsDisc());
    }
};


// Sort alignments so that paired, reversed-forward alignments are first
struct compare_alignments_revfw_first
{
    bool operator()(const CRef<CSeq_align>& a, const CRef<CSeq_align>& b)
    {
        if (a->GetSegs().IsDisc() && b->GetSegs().IsDisc()) {
            const CSeq_align& a_first = *a->GetSegs().GetDisc().Get().front();
            const CSeq_align& a_second = *a->GetSegs().GetDisc().Get().back();
            const CSeq_align& b_first = *b->GetSegs().GetDisc().Get().front();
            const CSeq_align& b_second = *b->GetSegs().GetDisc().Get().back();

            if (a_first.GetSeqStrand(0) == eNa_strand_minus &&
                a_second.GetSeqStrand(0) == eNa_strand_plus &&
                a_second.GetSeqStart(1) <= a_first.GetSeqStart(1) &&
                (b_first.GetSeqStrand(0) != eNa_strand_minus ||
                 b_second.GetSeqStrand(0) != eNa_strand_plus ||
                 b_second.GetSeqStart(1) > b_first.GetSeqStart(1))) {

                return true;
            }

            return false;
        }
        
        return (a->GetSegs().IsDisc() && !b->GetSegs().IsDisc());
    }
};

void CMagicBlastResults::SortAlignments(CMagicBlastResults::EOrdering order)
{
    if (order == eFwRevFirst) {
        m_Aligns->Set().sort(compare_alignments_fwrev_first());
    }
    else {
        m_Aligns->Set().sort(compare_alignments_revfw_first());
    }
}


void CMagicBlastResults::x_SetInfo(int first_length,
                             const TMaskedQueryRegions* first_mask,
                             int last_length /* = 0 */,
                             const TMaskedQueryRegions* last_mask /* = NULL */)
{
    m_FirstInfo = 0;
    m_LastInfo = 0;
    m_Concordant = false;

    bool first_aligned = false;
    bool last_aligned = false;

    if (!m_Paired) {
        first_aligned = !m_Aligns->Get().empty();
        m_Concordant = true;
    }
    else {

        for (auto it: m_Aligns->Get()) {
            if (it->GetSegs().IsDisc()) {
                first_aligned = true;
                last_aligned = true;

                const CSeq_align_set::Tdata& sasd =
                        it->GetSegs().GetDisc().Get();
                ASSERT(sasd.size() == 2);

                CRef<CSeq_align>   sa_q = sasd.front();
                ENa_strand str_q = sa_q->GetSeqStrand(0);
                TSeqPos sp_q     = sa_q->GetSeqStart(1);

                CRef<CSeq_align>   sa_m = sasd.back();
                ENa_strand str_m = sa_m->GetSeqStrand(0);
                TSeqPos sp_m     = sa_m->GetSeqStart(1);

                if (str_q == eNa_strand_plus
                        &&  str_m == eNa_strand_minus) {
                    if (sp_q <= sp_m) {
                        m_Concordant = true;
                    }
                } else if (str_q == eNa_strand_minus
                        &&  str_m == eNa_strand_plus) {
                    if (sp_q >= sp_m) {
                        m_Concordant = true;
                    }
                }

                break;
            }
            else if (it->GetSeq_id(0).Match(*m_QueryId)) {
                first_aligned = true;
            }
            else if (it->GetSeq_id(0).Match(*m_MateId)) {
                last_aligned = true;
            }
        }
    }

    if (!first_aligned) {
        m_FirstInfo |= fUnaligned;
    }

    if (!last_aligned) {
        m_LastInfo |= fUnaligned;
    }

    if (first_mask && !first_mask->empty()) {
        TSeqRange first_range(*first_mask->front());
        if (first_range.GetLength() + 1 >= (TSeqPos)first_length) {
            m_FirstInfo |= fFiltered;
        }
    }

    if (last_mask && !last_mask->empty()) {
        TSeqRange last_range(*last_mask->front());
        if (last_range.GetLength() + 1 >= (TSeqPos)last_length) {
            m_LastInfo |= fFiltered;
        }
    }
}

CRef<CSeq_align_set> CMagicBlastResultSet::GetFlatResults(bool no_discordant)
{
    CRef<CSeq_align_set> retval(new CSeq_align_set);

    for (auto result: *this) {

        if (no_discordant && result->IsPaired() && !result->IsConcordant()) {
            continue;
        }

        for (auto it: result->GetSeqAlign()->Get()) {
            retval->Set().push_back(it);
        }
    }

    return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
