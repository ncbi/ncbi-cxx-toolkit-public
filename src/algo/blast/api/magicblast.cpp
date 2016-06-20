/* $Id$
/* ===========================================================================
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
#include <algo/blast/api/prelim_stage.hpp>
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
    CRef<CBlastPrelimSearch> prelim_search(new CBlastPrelimSearch(m_Queries,
                                                            m_Options,
                                                            m_LocalDbAdapter));

    int status = prelim_search->CheckInternalData();
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
         msg_vec.Combine(prelim_search->GetSearchMessages());

         // FIXME: Report search messages
    }

    try {
        prelim_search->SetNumberOfThreads(GetNumberOfThreads());
        // do mapping
        m_InternalData = prelim_search->Run();

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

    // close HSP stream and create internal results structure
    BlastMappingResults* results = Blast_MappingResultsNew();
    CRef< CStructWrapper<BlastMappingResults> > wrapped_results;
    wrapped_results.Reset(WrapStruct(results, Blast_MappingResultsFree));

    BlastHSPStreamMappingClose(m_InternalData->m_HspStream->GetPointer(),
                               results);

    // create and return results as ASN.1 objects
    return x_CreateSeqAlignSet(results);
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
static void s_ComputeBtopAndIdentity(const BlastHSPChain* chain,
                                     string& btop,
                                     double& perc_id)
{
    _ASSERT(chain);
    _ASSERT(chain->hsp_array[0]);
    const Uint1 kGap = 15;
    
    int num_identical = 0;
    int len = 0;
    for (int k=0;k < chain->num_hsps;k++) {
        const BlastHSP* hsp = chain->hsp_array[k];
        const JumperEditsBlock* hsp_edits = hsp->map_info->edits;

        if (k > 0) {
            const BlastHSP* prev = chain->hsp_array[k - 1];
            int intron = hsp->subject.offset - prev->subject.end;
            if (intron > 0) {
                btop += (string)"^" + NStr::IntToString(intron) + "^";
            }

            int query_gap = hsp->query.offset - prev->query.end;
            if (query_gap > 0) {
                btop += (string)"_" + NStr::IntToString(query_gap) + "_";
            }
            else if (query_gap < 0) {
                btop += (string)"(" + NStr::IntToString(-query_gap) + ")";
            }

            // gap in query on exon edge
            if (hsp->query.offset > prev->query.end) {
                btop += (string)"_" +
                    NStr::IntToString(hsp->query.offset - prev->query.end) +
                    "_";

                len += hsp->query.offset - prev->query.end;
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
            len++;
            if (hsp_edits->edits[i].query_base != kGap) {
                query_pos++;
            }
        }
        num_matches = hsp->query.end - query_pos;
        num_identical += num_matches;
        if (num_matches > 0) {
            btop += NStr::IntToString(num_matches);
        }
    }
    len += num_identical;

    perc_id = (double)(num_identical * 100) / (double)len;
}


static CRef<CSeq_align> s_CreateSeqAlign(const BlastHSPChain* chain,
                                         CRef<ILocalQueryData>& qdata,
                                         CRef<IBlastSeqInfoSrc>& seqinfo_src)
{
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial);
    align->SetDim(2);

    CConstRef<CSeq_loc> query_loc = qdata->GetSeq_loc(chain->query_index);
    CRef<CSeq_id> query_id(new CSeq_id);
    SerialAssign(*query_id, CSeq_loc_CI(*query_loc).GetSeq_id());
    _ASSERT(query_id);
    TSeqPos query_length = qdata->GetSeqLength(chain->query_index);

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
    user_object->AddField("context", chain->hsp_array[0]->context);
    user_object->AddField("num_hits", chain->multiplicity);

    // for tabular
    string btop;
    double perc_id;
    s_ComputeBtopAndIdentity(chain, btop, perc_id);
    user_object->AddField("btop", btop);
    align->SetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped,
                         perc_id);

    return align;
}

CRef<CSeq_align_set> CMagicBlast::x_CreateSeqAlignSet(
                                                BlastMappingResults* results)
{
    CRef<CSeq_align_set> seq_aligns = CreateEmptySeq_align_set();

    CRef<ILocalQueryData> qdata = m_Queries->MakeLocalQueryData(m_Options);
    
    CRef<IBlastSeqInfoSrc> seqinfo_src;
    seqinfo_src.Reset(m_LocalDbAdapter->MakeSeqInfoSrc());
    _ASSERT(seqinfo_src);
    seqinfo_src->GarbageCollect();

    for (int i=0;i < results->num_results;i++) {
        // single spliced alignment
        BlastHSPChain* chain = results->chain_array[i];

        // mate pairs are processed together when the first one is encountered,
        // so skip the second of the pair
        if (chain->pair && chain->query_index > chain->pair->query_index) {
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
            disc.Set().push_back(s_CreateSeqAlign(chain, qdata, seqinfo_src));
            disc.Set().push_back(s_CreateSeqAlign(chain->pair, qdata,
                                                  seqinfo_src));
        }
        else {
            align = s_CreateSeqAlign(chain, qdata, seqinfo_src);
        }

        seq_aligns->Set().push_back(align);
    }

    return seq_aligns;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
