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
#include <algo/blast/api/blast_dbindex.hpp>

#include "blast_aux_priv.hpp"
#include "blast_memento_priv.hpp"
#include "blast_setup.hpp"

// SeqAlignVector building
#include "blast_seqalign.hpp"
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <serial/iterator.hpp>

// CORE BLAST includes
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>
#include <algo/blast/core/hspfilter_besthit.h>
#include <algo/blast/core/hspfilter_culling.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CRef<CBlastRPSInfo>
CSetupFactory::CreateRpsStructures(const string& rps_dbname,
                                   CRef<CBlastOptions> options)
{
    CRef<CBlastRPSInfo> retval(new CBlastRPSInfo(rps_dbname));
    options->SetMatrixName(retval->GetMatrixName());
    options->SetGapOpeningCost(retval->GetGapOpeningCost());
    options->SetGapExtensionCost(retval->GetGapExtensionCost());
    return retval;
}

/** 
 * @brief Auxiliary function to extract the Seq-ids from the ILocalQueryData
 * and bundle them in a Packed-seqint
 * 
 * @param query_data input query data [in]
 * 
 * @return Packed-seqint with query Seq-locs
 */
static
CRef<CPacked_seqint> s_LocalQueryData2Packed_seqint(ILocalQueryData& query_data)
{
    const int kNumQueries = query_data.GetNumQueries();
    if (kNumQueries == 0) {
        return CRef<CPacked_seqint>();
    }

    CRef<CPacked_seqint> retval(new CPacked_seqint);
    for (int i = 0; i < kNumQueries; i++) {
        CConstRef<CSeq_id> id(query_data.GetSeq_loc(i)->GetId());
        if (query_data.GetSeq_loc(i)->IsInt()) {
            retval->AddInterval(query_data.GetSeq_loc(i)->GetInt());
        } else if (id.NotEmpty()) {
            retval->AddInterval(*id, 0, query_data.GetSeqLength(i));
        }
    }

    return retval;
}

BlastScoreBlk*
CSetupFactory::CreateScoreBlock(const CBlastOptionsMemento* opts_memento,
                                CRef<ILocalQueryData> query_data,
                                BlastSeqLoc** lookup_segments,
                                TSearchMessages& search_messages,
                                TSeqLocInfoVector* masked_query_regions,
                                const CBlastRPSInfo* rps_info)
{
    _ASSERT(opts_memento);

    double rps_scale_factor(1.0);

    if (rps_info) {
        _ASSERT(Blast_ProgramIsRpsBlast(opts_memento->m_ProgramType));
        rps_scale_factor = rps_info->GetScalingFactor();
    }

    CBlast_Message blast_msg;
    CBlastMaskLoc core_masked_query_regions;

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
                                  &core_masked_query_regions,
                                  &retval,
                                  &blast_msg,
                                  &BlastFindMatrixPath);

    Blast_Message2TSearchMessages(blast_msg.Get(), query_info, search_messages);
    if (status != 0 && 
        (!(blast_msg.Get()) || (blast_msg.Get() && blast_msg.Get()->severity == eBlastSevError)))
    {
        	string msg;
        	if (search_messages.HasMessages()) {
            		msg = search_messages.ToString();
        	} else {
            		msg = "BLAST_MainSetUp failed (" + NStr::IntToString(status) + 
            			" error code)";
                }
                // Clean up leaks before we throw
                retval = BlastScoreBlkFree(retval);
                *lookup_segments =  BlastSeqLocFree(*lookup_segments);
                NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    if (masked_query_regions) {
        CRef<CPacked_seqint> query_locations = 
            s_LocalQueryData2Packed_seqint(*query_data);
        Blast_GetSeqLocInfoVector(opts_memento->m_ProgramType,
                                  *query_locations,
                                  core_masked_query_regions,
                                  *masked_query_regions);
    }

    return retval;
}

LookupTableWrap*
CSetupFactory::CreateLookupTable(CRef<ILocalQueryData> query_data,
                                 const CBlastOptionsMemento* opts_memento,
                                 BlastScoreBlk* score_blk,
                                 CRef< CBlastSeqLocWrap > lookup_segments_wrap,
                                 const CBlastRPSInfo* rps_info,
                                 BlastSeqSrc* seqsrc)
{
    BLAST_SequenceBlk* queries = query_data->GetSequenceBlk();
    CBlast_Message blast_msg;
    LookupTableWrap* retval(0);

    BlastSeqLoc * lookup_segments = lookup_segments_wrap->getLocs();

    Int2 status = LookupTableWrapInit(queries,
                                      opts_memento->m_LutOpts,
                                      opts_memento->m_QueryOpts,
                                      lookup_segments,
                                      score_blk,
                                      &retval,
                                      rps_info ? (*rps_info)() : 0,
                                      &blast_msg);
    if (status != 0) {
         TSearchMessages search_messages;
         Blast_Message2TSearchMessages(blast_msg.Get(), 
                                           query_data->GetQueryInfo(), 
                                           search_messages);
         string msg;
         if (search_messages.HasMessages()) {
              msg = search_messages.ToString();
         } else {
              msg = "LookupTableWrapInit failed (" + 
                   NStr::IntToString(status) + " error code)";
         }
         NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    // For PHI BLAST, save information about pattern occurrences in query in
    // the BlastQueryInfo structure
    if (Blast_ProgramIsPhiBlast(opts_memento->m_ProgramType)) {
        SPHIPatternSearchBlk* phi_lookup_table
            = (SPHIPatternSearchBlk*) retval->lut;
        status = Blast_SetPHIPatternInfo(opts_memento->m_ProgramType,
                                phi_lookup_table,
                                queries,
                                lookup_segments,
                                query_data->GetQueryInfo(), 
                                &blast_msg);
        if (status != 0) {  
             TSearchMessages search_messages;
             Blast_Message2TSearchMessages(blast_msg.Get(), 
                                           query_data->GetQueryInfo(), 
                                           search_messages);
             string msg;
             if (search_messages.HasMessages()) {
                 msg = search_messages.ToString();
             } else {
                 msg = "Blast_SetPHIPatternInfo failed (" + 
                     NStr::IntToString(status) + " error code)";
             }
             NCBI_THROW(CBlastException, eCoreBlastError, msg);
        }
    }

    if (seqsrc) {
        GetDbIndexSetQueryInfoFn()( seqsrc, retval, lookup_segments_wrap);
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
                               size_t number_of_queries,
                               BlastHSPWriter* writer)
{
    _ASSERT(opts_memento);
    return BlastHSPStreamNew(opts_memento->m_ProgramType, 
                             opts_memento->m_ExtnOpts, TRUE,
                             number_of_queries, writer);
}

BlastHSPWriter*
CSetupFactory::CreateHspWriter(const CBlastOptionsMemento* opts_memento,
                               BlastQueryInfo* query_info)
{
    BlastHSPWriterInfo* writer_info = NULL;
    
    const BlastHSPFilteringOptions* filt_opts =
        opts_memento->m_HitSaveOpts->hsp_filt_opt;
    if (filt_opts) {
        bool hsp_writer_found = false;
        if (filt_opts->best_hit && (filt_opts->best_hit_stage & ePrelimSearch)) 
        {
            BlastHSPBestHitParams* params = 
                BlastHSPBestHitParamsNew(opts_memento->m_HitSaveOpts,
                     filt_opts->best_hit,
                     opts_memento->m_ExtnOpts->compositionBasedStats, 
                     opts_memento->m_ScoringOpts->gapped_calculation);
            writer_info = BlastHSPBestHitInfoNew(params);
            hsp_writer_found = true;
        }
        else if (filt_opts->culling_opts && 
                 (filt_opts->culling_stage & ePrelimSearch))
        {
            _ASSERT(hsp_writer_found == false);
            BlastHSPCullingParams* params = 
                BlastHSPCullingParamsNew(opts_memento->m_HitSaveOpts,
                     filt_opts->culling_opts,
                     opts_memento->m_ExtnOpts->compositionBasedStats,
                     opts_memento->m_ScoringOpts->gapped_calculation);
            writer_info = BlastHSPCullingInfoNew(params);
            hsp_writer_found = true;
        }
    } else {
        /* Use the collector filtering algorithm as the default */
        BlastHSPCollectorParams * params = 
            BlastHSPCollectorParamsNew(opts_memento->m_HitSaveOpts, 
                       opts_memento->m_ExtnOpts->compositionBasedStats, 
                       opts_memento->m_ScoringOpts->gapped_calculation);
        writer_info = BlastHSPCollectorInfoNew(params);
    }
    
    BlastHSPWriter* retval = BlastHSPWriterNew(&writer_info, query_info);
    _ASSERT(writer_info == NULL);
    return retval;
}

BlastHSPPipe*
CSetupFactory::CreateHspPipe(const CBlastOptionsMemento* opts_memento,
                             BlastQueryInfo* query_info)
{
    _ASSERT(opts_memento);

    BlastHSPPipe* retval = NULL;
    BlastHSPPipeInfo* pipe_info = NULL;
    
    const BlastHSPFilteringOptions* filt_opts =
        opts_memento->m_HitSaveOpts->hsp_filt_opt;
    if (filt_opts) {
        if (filt_opts->best_hit && 
            (filt_opts->best_hit_stage & eTracebackSearch)) {
            BlastHSPBestHitParams* params = 
                BlastHSPBestHitParamsNew(opts_memento->m_HitSaveOpts,
                     filt_opts->best_hit,
                     opts_memento->m_ExtnOpts->compositionBasedStats,
                     opts_memento->m_ScoringOpts->gapped_calculation);
            BlastHSPPipeInfo_Add(&pipe_info, 
                                 BlastHSPBestHitPipeInfoNew(params));
        } else if (filt_opts->culling_opts &&
                   (filt_opts->culling_stage & eTracebackSearch)) {
            BlastHSPCullingParams* params = 
                BlastHSPCullingParamsNew(opts_memento->m_HitSaveOpts,
                     filt_opts->culling_opts,
                     opts_memento->m_ExtnOpts->compositionBasedStats,
                     opts_memento->m_ScoringOpts->gapped_calculation);
            BlastHSPPipeInfo_Add(&pipe_info, 
                                 BlastHSPCullingPipeInfoNew(params));
        }
    } else {
        ; /* the default is to use no pipes */
    }
    
    retval = BlastHSPPipeNew(&pipe_info, query_info);
    _ASSERT(pipe_info == NULL);
    return retval;
}

BlastSeqSrc*
CSetupFactory::CreateBlastSeqSrc(const CSearchDatabase& db)
{
    return CreateBlastSeqSrc(db.GetSeqDb(), 
                             db.GetFilteringAlgorithm(),
                             db.GetMaskType());
}

BlastSeqSrc*
CSetupFactory::CreateBlastSeqSrc(CSeqDB * db, int filt_algo,
                                 ESubjectMaskingType mask_type)
{
    BlastSeqSrc* retval = SeqDbBlastSeqSrcInit(db, filt_algo, mask_type);
    char* error_str = BlastSeqSrcGetInitError(retval);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        retval = BlastSeqSrcFree(retval);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
    return retval;
}

void
CSetupFactory::InitializeMegablastDbIndex(BlastSeqSrc* seqsrc,
                                          CRef<CBlastOptions> options)
{
    _ASSERT(options->GetUseIndex());
    _ASSERT(seqsrc);

    if (options->GetMBIndexLoaded()) {
        return;
    }

    string errstr = "";
    BlastSeqSrc * ind_seqsrc = 0;

    if( options->GetProgramType() != eBlastTypeBlastn ) {
        errstr = "Database indexing is available for blastn only.";
    }
    else if( options->GetMBTemplateLength() > 0 ) {
        errstr = "Database indexing is not available for discontiguous ";
        errstr += "searches.";
    }
    else if( options->GetWordSize() < MinIndexWordSize() ) {
        errstr = "MegaBLAST database index requires word size greater than ";
        errstr += NStr::IntToString(MinIndexWordSize() - 1);
        errstr += ".";
    }
    else {
        BlastSeqSrc * new_seqsrc = CloneSeqSrcInit( seqsrc );

        if( new_seqsrc == 0 ) {
            errstr = "Allocation of new BlastSeqSrc structure failed.";
        }
        else {
            ind_seqsrc = DbIndexSeqSrcInit(
            options->GetIndexName(), new_seqsrc );
            char* error_str = BlastSeqSrcGetInitError(ind_seqsrc);

            if( error_str != 0 ) {
                errstr = "Allocation of BlastSeqSrc structure for index ";
                errstr += "failed: " + string(error_str);
                sfree(error_str);
                ind_seqsrc = BlastSeqSrcFree(ind_seqsrc);
                free( new_seqsrc );
            }
        }
    }

    if( errstr != "" ) {
        if( options->GetForceIndex() ) {
            NCBI_THROW( CIndexedDbException, eIndexInitError, errstr );
        }
        else {
            ERR_POST_EX(1, 1, Info << errstr << " Database index will not be used." );
            options->SetUseIndex( false );
            return;
        }
    }

    CloneSeqSrc( seqsrc, ind_seqsrc );
    free( ind_seqsrc );
    options->SetMBIndexLoaded();
    options->SetLookupTableType( eIndexedMBLookupTable );
}

SInternalData::SInternalData()
{
    m_Queries = 0;
    m_QueryInfo = 0;
}

SDatabaseScanData::SDatabaseScanData()
   : kNoPhiBlastPattern(-1)
{
   m_NumPatOccurInDB = kNoPhiBlastPattern; 
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

