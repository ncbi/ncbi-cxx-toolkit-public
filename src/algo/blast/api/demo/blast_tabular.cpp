/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author:  Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file: blast_tabular.cpp
/// C++ implementation of the on-the-fly tabular formatting of BLAST results. 

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_traceback.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/sequence.hpp>
#include "blast_tabular.hpp"
#include <algo/blast/api/blast_seqinfosrc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

//BEGIN_NCBI_SCOPE
USING_NCBI_SCOPE;
USING_SCOPE(objects);
//BEGIN_SCOPE(blast)
USING_SCOPE(blast);

CBlastTabularFormatThread::CBlastTabularFormatThread(const CDbBlast* blaster,
                                                     CNcbiOstream& ostream,
                                                     IBlastSeqInfoSrc* seqinfo_src)
    : m_OutStream(&ostream), m_pSeqInfoSrc(seqinfo_src)
{

    m_QueryVec = blaster->GetQueries(); 
    m_Program = blaster->GetOptions().GetProgramType();
    m_pHspStream = blaster->GetHSPStream();
    m_pQuery = blaster->GetQueryBlk();

    m_ibPerformTraceback = 
        (blaster->GetOptions().GetGappedMode() && 
         blaster->GetOptions().GetGapExtnAlgorithm() != 
             eGreedyWithTracebackExt);

    m_iGenCodeString = 
        FindGeneticCode(blaster->GetOptions().GetQueryGeneticCode());
    // Sequence source must be copied, to guarantee multi-thread safety.
    m_ipSeqSrc = BlastSeqSrcCopy(blaster->GetSeqSrc());
    // Effective lengths must be duplicated in query info structure, because
    // they might be changing in the preliminary search.
    m_ipQueryInfo = BlastQueryInfoDup(blaster->GetQueryInfo());

    // If traceback will have to be performed before tabular output, 
    // do the preparation for it here.
    if (m_ibPerformTraceback) {
        BLAST_GapAlignSetUp(m_Program, m_ipSeqSrc, 
            blaster->GetOptions().GetScoringOpts(), 
            blaster->GetOptions().GetEffLenOpts(), 
            blaster->GetOptions().GetExtnOpts(), 
            blaster->GetOptions().GetHitSaveOpts(), m_ipQueryInfo, 
            blaster->GetScoreBlk(), &m_ipScoreParams, &m_ipExtParams, 
            &m_ipHitParams, &m_ipEffLenParams, &m_ipGapAlign);
        /* Set X-dropoff for traceback before tabular output to final 
           X-dropoff. */
        m_ipGapAlign->gap_x_dropoff = m_ipExtParams->gap_x_dropoff_final;
    } else {
        m_ipScoreParams = NULL;
        m_ipExtParams = NULL;
        m_ipHitParams = NULL;
        m_ipEffLenParams = NULL;
        m_ipGapAlign = NULL;
    }
}

CBlastTabularFormatThread::~CBlastTabularFormatThread()
{
   /* Free only the structures that have been initialized internally */
   m_ipQueryInfo = BlastQueryInfoFree(m_ipQueryInfo);
   m_ipScoreParams = BlastScoringParametersFree(m_ipScoreParams);
   m_ipExtParams = BlastExtensionParametersFree(m_ipExtParams);
   m_ipHitParams = BlastHitSavingParametersFree(m_ipHitParams);
   m_ipEffLenParams = 
      BlastEffectiveLengthsParametersFree(m_ipEffLenParams);
   m_ipGapAlign = BLAST_GapAlignStructFree(m_ipGapAlign);
   m_ipSeqSrc = BlastSeqSrcFree(m_ipSeqSrc);
}

static void 
Blast_ScoreAndEvalueToBuffers(double bit_score, double evalue, 
                              char* bit_score_buf, char** evalue_buf,
                              bool knock_off_allowed)
{
   if (evalue < 1.0e-180) {
      sprintf(*evalue_buf, "0.0");
   } else if (evalue < 1.0e-99) {
      sprintf(*evalue_buf, "%2.0le", evalue);
      if (knock_off_allowed)
         (*evalue_buf)++; /* Knock off digit. */
   } else if (evalue < 0.0009) {
      sprintf(*evalue_buf, "%3.0le", evalue);
   } else if (evalue < 0.1) {
      sprintf(*evalue_buf, "%4.3lf", evalue);
   } else if (evalue < 1.0) { 
      sprintf(*evalue_buf, "%3.2lf", evalue);
   } else if (evalue < 10.0) {
      sprintf(*evalue_buf, "%2.1lf", evalue);
   } else { 
      sprintf(*evalue_buf, "%5.0lf", evalue);
   }
   if (bit_score > 9999)
      sprintf(bit_score_buf, "%4.3le", bit_score);
   else if (bit_score > 99.9)
      sprintf(bit_score_buf, "%4.0ld", (long)bit_score);
   else
      sprintf(bit_score_buf, "%4.1lf", bit_score);
}

/* This function might serve as a starting point for a callback function 
 * that prints results before the traceback stage, e.g. the on-the-fly 
 * tabular output, a la the -D3 option of the old megablast.
 */
void* CBlastTabularFormatThread::Main(void) 
{
   if (!m_pHspStream || !m_ipSeqSrc) 
      return NULL;

   GetSeqArg seq_arg;

   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   if (m_ibPerformTraceback) {
      seq_arg.encoding = 
          Blast_TracebackGetEncoding(m_Program);
   }

   vector<CSeq_id*> query_id_v;

   ITERATE(TSeqLocVector, itr, m_QueryVec) {
      query_id_v.push_back(
          const_cast<CSeq_id*>(&sequence::GetId(*itr->seqloc, itr->scope)));
   }

   bool one_seq_update_params = (BLASTSeqSrcGetTotLen(m_ipSeqSrc) == 0);
   BlastHSPList* hsp_list = NULL;
 
   while (BlastHSPStreamRead(m_pHspStream, &hsp_list) 
          != kBlastHSPStream_Eof) {
       if (!hsp_list) {
           /* This should not happen, but just in case */
           continue;
       }

       /* Perform traceback if necessary */
       if (m_ibPerformTraceback) {
           BlastSequenceBlkClean(seq_arg.seq);
           seq_arg.oid = hsp_list->oid;
           if (BLASTSeqSrcGetSequence(m_ipSeqSrc, (void*) &seq_arg) < 0)
               continue;
         
           if (one_seq_update_params) {
               Int2 status;
               /* This is not a database search, so effective search spaces
                  need to be recalculated based on this subject sequence 
                  length. */
               if ((status = 
                    BLAST_OneSubjectUpdateParameters(m_Program, 
                        seq_arg.seq->length, m_ipScoreParams->options, 
                        m_ipQueryInfo, m_ipGapAlign->sbp, 
                        m_ipHitParams, NULL, m_ipEffLenParams)) != 0) {
                   continue;
               }
           }

           Blast_TracebackFromHSPList(m_Program, hsp_list, m_pQuery,
               seq_arg.seq, m_ipQueryInfo, m_ipGapAlign, m_ipGapAlign->sbp, 
               m_ipScoreParams, m_ipExtParams->options, m_ipHitParams, 
               m_iGenCodeString.get());
           BLASTSeqSrcRetSequence(m_ipSeqSrc, (void*)&seq_arg);
           /* Recalculate the bit scores, since they might have changed. */
           Blast_HSPListGetBitScores(hsp_list, 
               m_ipScoreParams->options->gapped_calculation, m_ipGapAlign->sbp);
       }

       list < CRef<CSeq_id> > subject_id_list = m_pSeqInfoSrc->GetId(hsp_list->oid);

       char* subject_buffer = 
           strdup(subject_id_list.front()->GetSeqIdString().c_str());

       int index;
       char bit_score_buff[10], eval_buff[10];

       for (index = 0; index < hsp_list->hspcnt; ++index) {
           BlastHSP* hsp = hsp_list->hsp_array[index];
           int query_index = 
               Blast_GetQueryIndexFromContext(hsp->context, m_Program);
           char* query_buffer = 
               strdup(query_id_v[query_index]->GetSeqIdString().c_str());
         
           char* eval_buff_ptr = eval_buff;
           Blast_ScoreAndEvalueToBuffers(hsp->bit_score, hsp->evalue, 
                                         bit_score_buff, &eval_buff_ptr, false);
         
           /* Calculate percentage of identities */
           Int4 align_length = 0;
           Int4 num_gaps = 0, num_gap_opens = 0;
           Blast_HSPCalcLengthAndGaps(hsp, &align_length, &num_gaps, 
                                      &num_gap_opens);
#if 0
           double perc_ident = ((double)hsp->num_ident)/align_length * 100;
#else // Try to guarantee proper precision
           double perc_ident = 
               ((double)BLAST_Nint((double)(hsp->num_ident * 10000)/align_length)) / 100;
#endif
           Int4 num_mismatches = align_length - hsp->num_ident - num_gaps;
         
           Int4 q_start=0, q_end=0, s_start=0, s_end=0;

           Blast_HSPGetAdjustedOffsets(hsp, &q_start, &q_end, &s_start, &s_end);
         
           *m_OutStream << query_buffer << "\t" << subject_buffer << "\t" <<
               perc_ident << "\t" << align_length << "\t" << num_mismatches <<
               "\t" << num_gap_opens << "\t" << q_start << "\t" << q_end << 
               "\t" << s_start << "\t" << s_end << "\t" << eval_buff <<
               "\t" << bit_score_buff << endl;
           sfree(query_buffer);
       }
       m_OutStream->flush();
       sfree(subject_buffer);
       hsp_list = Blast_HSPListFree(hsp_list);
   }
   BlastSequenceBlkFree(seq_arg.seq);

   return NULL;
}

void CBlastTabularFormatThread::OnExit(void)
{ 
}

//END_SCOPE(blast)
//END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.12  2005/02/15 23:27:45  dondosha
* Set X-dropoff in the GapAlignStruct to final X-dropoff for traceback before tabular formatting
*
* Revision 1.11  2004/12/21 17:18:56  dondosha
* eSkipTbck option has been removed
*
* Revision 1.10  2004/11/02 20:16:20  madden
* BLAST_OneSubjectUpdateParameters no longer requires BlastExtensionParameters
*
* Revision 1.9  2004/11/02 17:53:02  camacho
* Add SKIP_DOXYGEN_PROCESSING to rcsid string
*
* Revision 1.8  2004/10/06 14:58:49  dondosha
* Use IBlastSeqInfoSrc interface for Seq-ids and lengths retrieval in tabular formatting thread
*
* Revision 1.7  2004/10/04 18:12:40  dondosha
* Removed call to deleted Blast_HSPListCheckIfSorted function
*
* Revision 1.6  2004/09/21 13:51:44  dondosha
* Check if HSPs in HSP lists are sorted coming out of BlastHSPStream
*
* Revision 1.5  2004/08/11 14:24:50  camacho
* Move FindGeneticCode
*
* Revision 1.4  2004/08/04 22:03:36  dondosha
* Memset GetSeqArg structure in all cases, to prevent FUM error
*
* Revision 1.3  2004/07/06 15:55:27  dondosha
* Changed CDbBlast argument in constructor to const pointer
*
* Revision 1.2  2004/06/15 20:24:35  dondosha
* Corrected path to blast_setup.hpp header
*
* Revision 1.1  2004/06/15 18:51:16  dondosha
* Implementation of a thread producing on-the-fly output from a BLAST search
*
* ===========================================================================
*/
