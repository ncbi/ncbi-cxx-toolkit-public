#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
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
 * Author:  Christiam Camacho
 *
 */

/** @file setup_factory.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>      // for SeqDbBlastSeqSrcInit
#include <algo/blast/api/blast_mtlock.hpp>      // for Blast_DiagnosticsInitMT

#include "blast_memento_priv.hpp"
#include "blast_seqsrc_adapter_priv.hpp"

// SeqAlignVector building
#include "blast_seqalign.hpp"
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <serial/iterator.hpp>

// CORE BLAST includes
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/hspstream_collector.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastRPSInfo*
CSetupFactory::CreateRpsStructures(const string& rps_dbname,
                                   CRef<CBlastOptions> options)
{
    CBlastRPSInfo* retval(new CBlastRPSInfo(rps_dbname));
    options->SetMatrixName(retval->GetMatrixName());
    options->SetGapOpeningCost(retval->GetGapOpeningCost());
    options->SetGapExtensionCost(retval->GetGapExtensionCost());
    return retval;
}

BlastScoreBlk*
CSetupFactory::CreateScoreBlock(const CBlastOptionsMemento* opts_memento,
                                CRef<ILocalQueryData> query_data,
                                BlastSeqLoc** lookup_segments,
                                const CBlastRPSInfo* rps_info)
{
    ASSERT(opts_memento);

    double rps_scale_factor(1.0);

    if (rps_info) {
        ASSERT(Blast_ProgramIsRpsBlast(opts_memento->m_ProgramType));
        rps_scale_factor = rps_info->GetScalingFactor();
    }

    CBlast_Message blast_msg;
    BlastMaskLoc* masked_query_regions(0);

    BlastQueryInfo* query_info = query_data->GetQueryInfo();
    BLAST_SequenceBlk* queries = query_data->GetSequenceBlk();

    BlastScoreBlk* retval(0);
    Int2 status = BLAST_MainSetUp(opts_memento->m_ProgramType,
                                  opts_memento->m_QueryOpts,
                                  opts_memento->m_ScoringOpts,
                                  queries,
                                  query_info,
                                  rps_scale_factor,
                                  lookup_segments,
                                  &masked_query_regions,
                                  &retval,
                                  &blast_msg);
    // FIXME: should this be returned, wait until Ilya's
    // changes for returning filtering locations are checked in
    BlastMaskLocFree(masked_query_regions);

    if (blast_msg.Get() || status != 0) {
        string msg = blast_msg ? blast_msg->message : 
            "BLAST_MainSetUp failed (" + NStr::IntToString(status) + 
            " error code)";
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }
    return retval;
}

LookupTableWrap*
CSetupFactory::CreateLookupTable(CRef<ILocalQueryData> query_data,
                                 const CBlastOptionsMemento* opts_memento,
                                 BlastScoreBlk* score_blk,
                                 BlastSeqLoc* lookup_segments,
                                 const CBlastRPSInfo* rps_info)
{
    BLAST_SequenceBlk* queries = query_data->GetSequenceBlk();

    LookupTableWrap* retval(0);
    Int2 status = LookupTableWrapInit(queries,
                                      opts_memento->m_LutOpts,
                                      lookup_segments,
                                      score_blk,
                                      &retval,
                                      rps_info ? (*rps_info)() : 0);
    if (status != 0) {
        NCBI_THROW(CBlastException, eCoreBlastError,
                   "LookupTableWrapInit failed (" + 
                   NStr::IntToString(status) + " error code)");
    }

    // For PHI BLAST, save information about pattern occurrences in query in
    // the BlastQueryInfo structure
    if (Blast_ProgramIsPhiBlast(opts_memento->m_ProgramType)) {
        SPHIPatternSearchBlk* phi_lookup_table
            = (SPHIPatternSearchBlk*) retval->lut;
        Blast_SetPHIPatternInfo(opts_memento->m_ProgramType,
                                phi_lookup_table,
                                queries,
                                lookup_segments,
                                query_data->GetQueryInfo());
    }

    return retval;
}

BlastDiagnostics*
CSetupFactory::CreateDiagnosticsStructure()
{
    return Blast_DiagnosticsInit();
}

BlastDiagnostics*
CSetupFactory::CreateDiagnosticsStructureMT()
{
    return Blast_DiagnosticsInitMT(Blast_CMT_LOCKInit());
}

BlastHSPStream*
CSetupFactory::CreateHspStream(const CBlastOptionsMemento* opts_memento,
                               size_t number_of_queries)
{
    return x_CreateHspStream(opts_memento, number_of_queries, false);
}

BlastHSPStream*
CSetupFactory::CreateHspStreamMT(const CBlastOptionsMemento* opts_memento,
                                 size_t number_of_queries)
{
    return x_CreateHspStream(opts_memento, number_of_queries, true);
}

BlastHSPStream*
CSetupFactory::x_CreateHspStream(const CBlastOptionsMemento* opts_memento,
                                 size_t number_of_queries,
                                 bool is_multi_threaded)
{
    ASSERT(opts_memento);

    SBlastHitsParameters* blast_hit_params(0);
    SBlastHitsParametersNew(opts_memento->m_HitSaveOpts,
                            opts_memento->m_ExtnOpts,
                            opts_memento->m_ScoringOpts,
                            &blast_hit_params);

    if (is_multi_threaded) {
        return Blast_HSPListCollectorInitMT(opts_memento->m_ProgramType,
                                            blast_hit_params,
                                            number_of_queries,
                                            true,
                                            Blast_CMT_LOCKInit());
    } else {
        return Blast_HSPListCollectorInit(opts_memento->m_ProgramType,
                                          blast_hit_params,
                                          number_of_queries,
                                          true);
    }
}

BlastSeqSrc*
CSetupFactory::CreateBlastSeqSrc(const CSearchDatabase& db)
{
    bool prot = (db.GetMoleculeType() == CSearchDatabase::eBlastDbIsProtein);

    BlastSeqSrc* retval = SeqDbBlastSeqSrcInit(db.GetDatabaseName(), prot);
    char* error_str = BlastSeqSrcGetInitError(retval);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        retval = BlastSeqSrcFree(retval);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
    return retval;
}

SInternalData::SInternalData()
{
    m_Queries = 0;
    m_QueryInfo = 0;
}

static void
x_RemapToQueryLoc(CRef<CSeq_align> sar, const CSeq_loc & query)
{
    _ASSERT(sar);
    const int query_dimension = 0;

    TSeqPos q_shift = 0;

    if (query.IsInt()) {
        q_shift = query.GetInt().GetFrom();
    }
    if (q_shift > 0) {
        for (CTypeIterator<CDense_seg> itr(Begin(*sar)); itr; ++itr) {
            const vector<ENa_strand> strands = itr->GetStrands();
            // Create temporary CSeq_locs with strands either matching 
            // (for query and for subject if it is not on a minus strand),
            // or opposite to those in the segment, to force RemapToLoc to 
            // behave in the correct way.
            CSeq_loc q_seqloc;
            ENa_strand q_strand = strands[0];
            q_seqloc.SetInt().SetFrom(q_shift);
            q_seqloc.SetInt().SetTo(query.GetInt().GetTo());
            q_seqloc.SetInt().SetStrand(q_strand);
            q_seqloc.SetInt().SetId().Assign(CSeq_loc_CI(query).GetSeq_id());
            itr->RemapToLoc(query_dimension, q_seqloc, true);
        }
    }
}

static void
x_GetSequenceLengthAndId(const IBlastSeqInfoSrc* seqinfo_src, // [in]
                         int oid,                    // [in] 
                         CConstRef<CSeq_id>& seqid,  // [out]
                         TSeqPos* length)            // [out]
{
    ASSERT(length);
    list<CRef<CSeq_id> > seqid_list = seqinfo_src->GetId(oid);

    seqid.Reset(seqid_list.front());
    *length = seqinfo_src->GetLength(oid);

    return;
}

CSeq_align_set*
BlastHitList2SeqAlign_OMF(const BlastHitList     * hit_list,
                          EBlastProgramType        prog,
                          const CSeq_loc         & query_loc,
                          TSeqPos                  query_length,
                          const IBlastSeqInfoSrc * seqinfo_src,
                          bool                     is_gapped,
                          bool                     is_ooframe)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();
    
    if (!hit_list) {
        return CreateEmptySeq_align_set(seq_aligns);
    }
    
//     TSeqPos query_length = sequence::GetLength(*query.seqloc, query.scope);
//     CConstRef<CSeq_id> query_id(&sequence::GetId(*query.seqloc, query.scope));
    CConstRef<CSeq_id> query_id(& CSeq_loc_CI(query_loc).GetSeq_id());
    
    _ASSERT(query_id);
    
    TSeqPos subj_length = 0;
    CConstRef<CSeq_id> subject_id;
    
    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;
        
        // Sort HSPs with e-values as first priority and scores as 
        // tie-breakers, since that is the order we want to see them in 
        // in Seq-aligns.
        Blast_HSPListSortByEvalue(hsp_list);
        
        x_GetSequenceLengthAndId(seqinfo_src, hsp_list->oid,
                                 subject_id, &subj_length);
        
        // Create a CSeq_align for each matching sequence
        CRef<CSeq_align> hit_align;
        if (is_gapped) {
            hit_align =
                BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                       subject_id, query_length, subj_length,
                                       is_ooframe);
        } else {
            hit_align =
                BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                    subject_id, query_length, subj_length);
        }
        x_RemapToQueryLoc(hit_align, query_loc);
        seq_aligns->Set().push_back(hit_align);
    }
    return seq_aligns;
}

TSeqAlignVector 
PhiBlastResults2SeqAlign_OMF(const BlastHSPResults  * results, 
                             EBlastProgramType        prog,
                             class ILocalQueryData  & query,
                             const IBlastSeqInfoSrc * seqinfo_src,
                             const SPHIQueryInfo    * pattern_info)
{
    TSeqAlignVector retval;
    CRef<CSeq_align_set> wrap_list(new CSeq_align_set());

    /* Split results into an array of BlastHSPResults structures corresponding
       to different pattern occurrences. */
    BlastHSPResults* *phi_results = 
        PHIBlast_HSPResultsSplit(results, pattern_info);

    if (phi_results) {
        for (int pattern_index = 0; pattern_index < pattern_info->num_patterns;
             ++pattern_index) {
            CBlastHSPResults one_phi_results(phi_results[pattern_index]);

            if (one_phi_results) {
                // PHI BLAST does not work with multiple queries, so we only 
                // need to look at the first hit list.
                BlastHitList* hit_list = one_phi_results->hitlist_array[0];

                // PHI BLAST is always gapped, and never out-of-frame, hence
                // true and false values for the respective booleans in the next
                // call.
                CRef<CSeq_align_set> seq_aligns(
                    BlastHitList2SeqAlign_OMF(hit_list,
                                              prog,
                                              *query.GetSeq_loc(0),
                                              query.GetSeqLength(0),
                                              seqinfo_src,
                                              true,
                                              false));
                
                // Wrap this Seq-align-set in a discontinuous Seq-align and 
                // attach it to the final Seq-align-set.
                CRef<CSeq_align> wrap_align(new CSeq_align());
                wrap_align->SetType(CSeq_align::eType_partial);
                wrap_align->SetDim(2); // Always a pairwise alignment
                wrap_align->SetSegs().SetDisc(*seq_aligns);
                wrap_list->Set().push_back(wrap_align);
            }
        }
        sfree(phi_results);
    }
    
    // Put the final Seq-align-set into the vector
    retval.push_back(wrap_list);

    return retval;
}

TSeqAlignVector
BlastResults2SeqAlign_OMF(const BlastHSPResults  * results,
                          EBlastProgramType        prog,
                          class ILocalQueryData  & query,
                          const IBlastSeqInfoSrc * seqinfo_src,
                          bool                     is_gapped,
                          bool                     is_ooframe)
{
    ASSERT(results->num_queries == (int)query.GetNumQueries());
    
    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;
    
    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set>
           seq_aligns(BlastHitList2SeqAlign_OMF(hit_list,
                                                prog,
                                                *query.GetSeq_loc(index),
                                                query.GetSeqLength(index),
                                                seqinfo_src,
                                                is_gapped,
                                                is_ooframe));
       
       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size()
              << " seqaligns");

    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

