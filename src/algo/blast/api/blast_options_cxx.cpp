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
* Author:  Christiam Camacho
*
* File Description:
*   Class to encapsulate all NewBlast options
*
* ===========================================================================
*/

#include <algo/blast/api/blast_option.hpp>
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastOptions::CBlastOptions(EProgram prog_name) THROWS((CBlastException))
    : m_Program(prog_name)
{
    int st;

    if ( (st = BlastQuerySetUpOptionsNew(&m_QueryOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Query setup options error");

    // Word settings 
    if ( (st = BlastInitialWordOptionsNew(m_Program, &m_InitWordOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Initial word options error");
    if ( (st = LookupTableOptionsNew(m_Program, &m_LutOpts)) != 0)
        NCBI_THROW(CBlastException, eBadParameter, "Lookup options error");

    // Hit extension settings
    if ( (st = BlastExtensionOptionsNew(m_Program, &m_ExtnOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Lookup options error");

    // Hit saving settings
    if ( (st = BlastHitSavingOptionsNew(m_Program, &m_HitSaveOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Lookup options error");

    // Protein blast settings: initialize for psi/rps-blast
    //m_prot_opts;

    if ( (st = BlastScoringOptionsNew(m_Program, &m_ScoringOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Scoring options error");

    if ( (st = BlastEffectiveLengthsOptionsNew(&m_EffLenOpts)))
        NCBI_THROW(CBlastException, eBadParameter, 
                "Effective length options error");

    if ( (st = BlastDatabaseOptionsNew(&m_DbOpts)))
        NCBI_THROW(CBlastException, eBadParameter, "Db options error");

    if ( (st = PSIBlastOptionsNew(&m_ProtOpts)))
       NCBI_THROW(CBlastException, eBadParameter, "PSI options error");

    switch(prog_name) {
    case eBlastn:       SetBlastn();        break;
    case eBlastp:       SetBlastp();        break;
    case eBlastx:       SetBlastx();        break;
    case eTblastn:      SetTblastn();       break;
    case eTblastx:      SetTblastx();       break;
    //case eMegablast:    SetMegablast();     break;
    default:
        NCBI_THROW(CBlastException, eBadParameter, "Invalid program");
    }
}

CBlastOptions::~CBlastOptions()
{
}

void
CBlastOptions::SetDbGeneticCodeAndStr(int gc)
{
    SetDbGeneticCode(gc);
    AutoPtr<Uint1, ArrayDeleter<Uint1> > gc_str =
        FindGeneticCode(gc);
    SetDbGeneticCodeStr(gc_str.get());
}

void
CBlastOptions::SetBlastp()
{
    // Lookup table options
    m_LutOpts->lut_type = AA_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_PROT;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_BLASTP;
    m_LutOpts->alphabet_size = BLASTAA_SIZE;

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_unknown;
    SetFilterString("S");

    // Initial word options
    m_InitWordOpts->extend_word_method |= EXTEND_WORD_UNGAPPED;
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_PROT;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_PROT;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_PROT;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_PROT;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName("BLOSUM62");
    SetMatrixPath(FindMatrixPath(GetMatrixName(), true).c_str());
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_PROT;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_PROT;
    m_ScoringOpts->shift_pen = INT2_MAX;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = TRUE;

    // Hit saving options
    m_HitSaveOpts->is_gapped = TRUE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    m_DbOpts->genetic_code = BLAST_GENETIC_CODE; // not really needed
}


void
CBlastOptions::SetBlastn()
{
    // Lookup table options
    m_LutOpts->lut_type = NA_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_NUCL;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_BLASTN;
    m_LutOpts->alphabet_size = BLASTNA_SIZE;
    // ag_blast is the default; variable word sizes can only be used for word
    // sizes divisible by COMPRESSION_RATIO (4)
    if (m_LutOpts->word_size % COMPRESSION_RATIO == 0)
        m_LutOpts->scan_step = m_LutOpts->word_size - 8 + COMPRESSION_RATIO;
    else
        m_LutOpts->scan_step = m_LutOpts->word_size - 8 + 1;

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_both;
    SetFilterString("D");

    // Initial word options
    m_InitWordOpts->extend_word_method |= (EXTEND_WORD_UNGAPPED | 
            EXTEND_WORD_AG);
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_NUCL;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_NUCL;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    m_ScoringOpts->penalty = BLAST_PENALTY;
    m_ScoringOpts->reward = BLAST_REWARD;
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_NUCL;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_NUCL;
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = TRUE;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only

    // Hit saving options
    m_HitSaveOpts->is_gapped = TRUE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    m_DbOpts->genetic_code = BLAST_GENETIC_CODE; // not really needed
}

void CBlastOptions::SetMegablast()
{
    // Lookup table optionas
    m_LutOpts->lut_type = MB_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_MEGABLAST;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_MEGABLAST;
    m_LutOpts->max_positions = INT4_MAX;
    m_LutOpts->alphabet_size = BLASTNA_SIZE;
    // ag_blast is the default; variable word sizes can only be used for word
    // sizes divisible by COMPRESSION_RATIO (4)
    if (m_LutOpts->word_size % COMPRESSION_RATIO == 0)
        m_LutOpts->scan_step = m_LutOpts->word_size - 12 + COMPRESSION_RATIO;
    else
        m_LutOpts->scan_step = m_LutOpts->word_size - 12 + 1;

    m_LutOpts->mb_template_length = 0;
    m_LutOpts->mb_template_type = 0; // allowed types 0, 1, 2

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_both;
    SetFilterString("D");

    // Initial word options
    // variable word size is not supported if discontiguous MB
    m_InitWordOpts->extend_word_method |= (EXTEND_WORD_UNGAPPED | 
            EXTEND_WORD_MB_STACKS);
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_NUCL;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_NUCL;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    m_ScoringOpts->penalty = BLAST_PENALTY;
    m_ScoringOpts->reward = BLAST_REWARD;
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_MEGABLAST;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_MEGABLAST;
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = TRUE;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only

    // Hit saving options
    m_HitSaveOpts->is_gapped = TRUE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    m_DbOpts->genetic_code = BLAST_GENETIC_CODE; // not really needed
}

void
CBlastOptions::SetBlastx() 
{ 
    // Lookup table options
    m_LutOpts->lut_type = AA_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_PROT;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_BLASTX;
    m_LutOpts->alphabet_size = BLASTAA_SIZE;

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_unknown;
    SetFilterString("S");

    // Initial word options
    m_InitWordOpts->extend_word_method |= EXTEND_WORD_UNGAPPED;
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_PROT;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_PROT;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_PROT;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_PROT;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName("BLOSUM62");
    SetMatrixPath(FindMatrixPath(GetMatrixName(), true).c_str());
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_PROT;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_PROT;
    m_ScoringOpts->shift_pen = INT2_MAX;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = TRUE;

    // Hit saving options
    m_HitSaveOpts->is_gapped = TRUE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    SetDbGeneticCodeAndStr(BLAST_GENETIC_CODE);
}

void 
CBlastOptions::SetTblastn()
{
    // Lookup table options
    m_LutOpts->lut_type = AA_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_PROT;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_TBLASTN;
    m_LutOpts->alphabet_size = BLASTAA_SIZE;

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_unknown;
    SetFilterString("S");

    // Initial word options
    m_InitWordOpts->extend_word_method |= EXTEND_WORD_UNGAPPED;
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_PROT;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_PROT;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_PROT;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_PROT;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName("BLOSUM62");
    SetMatrixPath(FindMatrixPath(GetMatrixName(), true).c_str());
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_PROT;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_PROT;
    m_ScoringOpts->shift_pen = INT2_MAX;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = TRUE;

    // Hit saving options
    m_HitSaveOpts->is_gapped = TRUE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;
    m_HitSaveOpts->do_sum_stats = TRUE;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    SetDbGeneticCodeAndStr(BLAST_GENETIC_CODE);
}

void 
CBlastOptions::SetTblastx()
{
    // Lookup table options
    m_LutOpts->lut_type = AA_LOOKUP_TABLE;
    m_LutOpts->word_size = BLAST_WORDSIZE_PROT;
    m_LutOpts->threshold = BLAST_WORD_THRESHOLD_TBLASTX;
    m_LutOpts->alphabet_size = BLASTAA_SIZE;

    // Query setup options
    m_QueryOpts->strand_option = eNa_strand_unknown;
    SetFilterString("S");

    // Initial word options
    m_InitWordOpts->extend_word_method |= EXTEND_WORD_UNGAPPED;
    m_InitWordOpts->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
    m_InitWordOpts->window_size = BLAST_WINDOW_SIZE_PROT;

    // Extension options
    m_ExtnOpts->gap_x_dropoff = BLAST_GAP_X_DROPOFF_TBLASTX;
    m_ExtnOpts->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_TBLASTX;
    m_ExtnOpts->gap_trigger = BLAST_GAP_TRIGGER_PROT;
    m_ExtnOpts->algorithm_type = EXTEND_DYN_PROG;

    // Scoring options
    SetMatrixName("BLOSUM62");
    SetMatrixPath(FindMatrixPath(GetMatrixName(), true).c_str());
    m_ScoringOpts->gap_open = BLAST_GAP_OPEN_PROT;
    m_ScoringOpts->gap_extend = BLAST_GAP_EXTN_PROT;
    m_ScoringOpts->shift_pen = INT2_MAX;
    m_ScoringOpts->is_ooframe = FALSE;  // allowed for blastx only
    m_ScoringOpts->decline_align = INT2_MAX;
    m_ScoringOpts->gapped_calculation = FALSE;

    // Hit saving options
    m_HitSaveOpts->is_gapped = FALSE;
    m_HitSaveOpts->hitlist_size = 500;
    m_HitSaveOpts->expect_value = BLAST_EXPECT_VALUE;
    m_HitSaveOpts->percent_identity = 0;

    // Effective length options
    m_EffLenOpts->searchsp_eff = 0;
    m_EffLenOpts->dbseq_num = 1;   // assume bl2seq by default
    m_EffLenOpts->db_length = 0;   // will populate later
    m_EffLenOpts->use_real_db_size = TRUE;

    // Blast database options
    SetDbGeneticCodeAndStr(BLAST_GENETIC_CODE);
}

bool
CBlastOptions::Validate() const
{
    Blast_Message* blmsg = NULL;
    string msg;

    if (BlastScoringOptionsValidate(m_Program, m_ScoringOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Scoring options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (LookupTableOptionsValidate(m_Program, m_LutOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Lookup table options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (BlastHitSavingOptionsValidate(m_Program, m_HitSaveOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Hit saving options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (BlastExtensionOptionsValidate(m_Program, m_ExtnOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Extension options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    return true;
}

void
CBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBlastOptions");
    DebugDumpValue(ddc,"m_Program", m_Program);
    m_QueryOpts.DebugDump(ddc, depth);
    m_LutOpts.DebugDump(ddc, depth);
    m_InitWordOpts.DebugDump(ddc, depth);
    m_ExtnOpts.DebugDump(ddc, depth);
    m_HitSaveOpts.DebugDump(ddc, depth);
    m_ProtOpts.DebugDump(ddc, depth);
    m_DbOpts.DebugDump(ddc, depth);
    m_ScoringOpts.DebugDump(ddc, depth);
    m_EffLenOpts.DebugDump(ddc, depth);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.21  2003/09/11 17:45:03  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.20  2003/09/09 22:13:36  dondosha
* Added SetDbGeneticCodeAndStr method to set both integer and string genetic code in one call
*
* Revision 1.19  2003/09/09 12:57:15  camacho
* + internal setup functions, use smart pointers to handle memory mgmt
*
* Revision 1.18  2003/09/03 19:36:27  camacho
* Fix include path for blast_setup.hpp
*
* Revision 1.17  2003/09/03 18:45:34  camacho
* Fixed small memory leak, removed unneeded function
*
* Revision 1.16  2003/09/02 21:15:11  camacho
* Fix small memory leak
*
* Revision 1.15  2003/08/27 15:05:56  camacho
* Use symbolic name for alphabet sizes
*
* Revision 1.14  2003/08/21 19:32:08  dondosha
* Call SetDbGeneticCodeStr when creating a database gen. code string, to avoid code duplication
*
* Revision 1.13  2003/08/19 20:28:10  dondosha
* EProgram enum type is no longer part of CBlastOptions class
*
* Revision 1.12  2003/08/19 13:46:13  dicuccio
* Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
*
* Revision 1.11  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.10  2003/08/14 19:07:32  dondosha
* Added BLASTGetEProgram function to convert from Uint1 to enum type
*
* Revision 1.9  2003/08/11 15:17:39  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.8  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.7  2003/08/01 22:34:11  camacho
* Added accessors/mutators/defaults for matrix_path
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/24 18:24:17  camacho
* Minor
*
* Revision 1.3  2003/07/23 21:29:37  camacho
* Update BlastDatabaseOptions
*
* Revision 1.2  2003/07/15 19:22:04  camacho
* Fix setting of scan step in blastn
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
