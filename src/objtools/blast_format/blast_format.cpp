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
* ===========================================================================*/

/*****************************************************************************

File name: blast_format.cpp

Author: Ilya Dondoshansky

Contents: Formatting of BLAST results (SeqAlign)

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/api/blast_format.hpp>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/api/blast_seq.hpp>
#include <algo/blast/api/seqsrc_readdb.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

CBlastFormatOptions::CBlastFormatOptions(EProgram program, 
    CNcbiOstream &ostr) THROWS((CBlastException))
        : m_ostr(&ostr)
{
   m_believe_query = FALSE;
   m_descriptions = BLAST_NUM_DESCRIPTIONS;
   m_alignments = BLAST_NUM_ALIGNMENTS;
   
   m_print_options = 0;
   m_align_options = 0;
   m_align_options += TXALIGN_COMPRESS;
   m_align_options += TXALIGN_END_NUM;
   m_align_options += TXALIGN_SHOW_GI;
   m_print_options += TXALIGN_SHOW_GI;
   
   m_align_options += TXALIGN_MATRIX_VAL;
   m_align_options += TXALIGN_SHOW_QS;
   if (program == eBlastx)
      m_align_options += TXALIGN_BLASTX_SPECIAL;
   
   m_align_view = BLAST_ALIGN_VIEW;
}

CBlastFormatOptions::~CBlastFormatOptions()
{
}

static void 
SetDisplayParameters(CDisplaySeqalign &display, 
    const CBlastFormatOptions* format_options, EProgram program)
{
    bool db_is_na = (program == eBlastn ||
                     program == eTblastn ||
                     program == eTblastx);
    bool query_is_na = (program == eBlastn ||
                        program == eBlastx ||
                        program == eTblastx);
    display.SetDbType(db_is_na);
    display.SetQueryType(query_is_na);
    
    int AlignOption=0;
    int align_view = format_options->GetAlignView();

    if (align_view == 1 ){
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eMasterAnchored;
        AlignOption += CDisplaySeqalign::eShowIdentity;
    } else if (align_view == 2){
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eMasterAnchored;
    } else if (align_view == 3 ) {
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eShowIdentity;
    } else if (align_view ==  4) {
        AlignOption += CDisplaySeqalign::eMultiAlign;
    }

    if(program == eBlastn){
        AlignOption += CDisplaySeqalign::eShowBarMiddleLine;
        display.SetAlignType(CDisplaySeqalign::eNuc);
    } else {
        AlignOption += CDisplaySeqalign::eShowCharMiddleLine;
        display.SetAlignType(CDisplaySeqalign::eProt);
    }
    AlignOption += CDisplaySeqalign::eShowBlastInfo;
    AlignOption += CDisplaySeqalign::eShowBlastStyleId;
    if(format_options->GetHtml()){
        AlignOption += CDisplaySeqalign::eHtml;
    }

    display.SetAlignOption(AlignOption);
}

#define NUM_FRAMES 6

TSeqLocInfoVector
BlastMask2CSeqLoc(BlastMask* mask, TSeqLocVector &slp, 
    EProgram program)
{
    TSeqLocInfoVector retval;
    int index, frame, num_frames;
    bool translated_query;

    if (!mask)
        return retval;

    translated_query = (program == eBlastx || 
                        program == eTblastx);
    
    num_frames = (translated_query ? NUM_FRAMES : 1);
    
    TSeqLocInfo mask_info_list;

    for (index = 0; index < slp.size(); ++index) {
        for ( ; mask && mask->index < index*num_frames; 
              mask = mask->next);
        CDisplaySeqalign::SeqlocInfo seqloc_info;
        BlastSeqLoc* loc;

        mask_info_list.clear();

        for ( ; mask && mask->index < (index+1)*num_frames;
              mask = mask->next) {
            frame = (translated_query ? (mask->index % num_frames) + 1 : 0);


            for (loc = mask->loc_list; loc; loc = loc->next) {
                seqloc_info.frame = frame;
                CRef<CSeq_loc> seqloc(new CSeq_loc());
                seqloc->SetInt().SetFrom(((DoubleInt*) loc->ptr)->i1);
                seqloc->SetInt().SetTo(((DoubleInt*) loc->ptr)->i2);
                seqloc->SetInt().SetId(*(const_cast<CSeq_id*>(&sequence::GetId(*slp[index].seqloc, slp[index].scope))));
                
                seqloc_info.seqloc = seqloc;
                mask_info_list.push_back(seqloc_info);
            }
        }            
        retval.push_back(mask_info_list);
    }

    return retval;
}

int
BLAST_FormatResults(TSeqAlignVector &seqalignv, 
    EProgram program, TSeqLocVector &query,
    BlastMask* filter_loc, const CBlastFormatOptions* format_options, 
    bool is_ooframe)
{
    int index;
    
    TSeqLocInfoVector maskv = 
        BlastMask2CSeqLoc(filter_loc, query, program);

    list <CDisplaySeqalign::FeatureInfo> featureInfo;
    
    for (index = 0; index < seqalignv.size(); ++index) {
        if (!seqalignv[index]->IsSet())
            continue;
        
        query[index].scope->AddDataLoader("BLASTDB");
        CDisplaySeqalign display(*seqalignv[index], maskv[index], 
                                 featureInfo, 0, *query[index].scope);
        SetDisplayParameters(display, format_options, program);
        display.DisplaySeqalign(*format_options->GetOstream());
    }

    return 0;
}
