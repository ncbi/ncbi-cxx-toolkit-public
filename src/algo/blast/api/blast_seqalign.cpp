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
*   Utility function to convert internal BLAST result structures into
*   CSeq_align_set objects
*
* ===========================================================================
*/

#include <BlastSeqalign.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <objalign.h>

#include <objects/general/Object_id.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>

// NewBlast includes
#include <blast_seqalign.h>

#define SMALLEST_EVALUE 1.0e-180
#define GAP_VALUE -1

// Converts a frame into the appropriate strand
static ENa_strand
x_Frame2Strand(short frame)
{
    if (frame > 0)
        return eNa_strand_plus;
    else if (frame < 0)
        return eNa_strand_minus;
    else
        return eNa_strand_unknown;
}

static int 
x_GetCurrPos(int& pos, int pos2advance)
{
    int retval;

    if (pos < 0)
        retval = -(pos + pos2advance - 1);
    else
        retval = pos;
    pos += pos2advance;
    return retval;
}

static TSeqPos
x_GetAlignmentStart(int& curr_pos, const GapEditScriptPtr esp, 
        ENa_strand strand, bool translate, int length, int original_length, 
        short frame)
{
    TSeqPos retval;

    if (strand == eNa_strand_minus) {

        if (translate)
            retval = original_length - 
                CODON_LENGTH*(x_GetCurrPos(curr_pos, esp->num) + esp->num)
                + frame + 1;
        else
            retval = length - x_GetCurrPos(curr_pos, esp->num) - esp->num;

    } else {

        if (translate)
            retval = frame - 1 + CODON_LENGTH*x_GetCurrPos(curr_pos, esp->num);
        else
            retval = x_GetCurrPos(curr_pos, esp->num);

    }

    return retval;
}

/// C++ version of GXECollectDataForSeqalign
/// Note that even though the edit_block is passed in, data for seqalign is
/// collected from the esp argument for nsegs segments
static void
x_CollectSeqAlignData(const GapEditBlockPtr edit_block, 
        const GapEditScriptPtr esp_head, unsigned int nsegs,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    _ASSERT(edit_block != NULL);

    GapEditScriptPtr esp = esp_head;   // start of list of esp's
    ENa_strand m_strand, s_strand;      // strands of alignment
    TSignedSeqPos m_start, s_start;     // running starts of alignment
    int start1 = edit_block->start1;    // start of alignment on master sequence
    int start2 = edit_block->start2;    // start of alignment on slave sequence

    m_strand = x_Frame2Strand(edit_block->frame1);
    s_strand = x_Frame2Strand(edit_block->frame2);

    for (unsigned int i = 0; esp && i < nsegs; esp = esp->next, i++) {
        switch (esp->op_type) {
        case GAPALIGN_DECLINE:
        case GAPALIGN_SUB:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1, edit_block->length1,
                    edit_block->original_length1, edit_block->frame1);

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2, edit_block->length2,
                    edit_block->original_length2, edit_block->frame2);

            if (edit_block->reverse) {
                strands.push_back(s_strand);
                strands.push_back(m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(m_strand);
                strands.push_back(s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        // Insertion on the master sequence (gap on slave)
        case GAPALIGN_INS:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1, edit_block->length1,
                    edit_block->original_length1, edit_block->frame1);

            s_start = GAP_VALUE;

            if (edit_block->reverse) {
                strands.push_back(i == 0 ? eNa_strand_unknown : s_strand);
                strands.push_back(m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(m_strand);
                strands.push_back(i == 0 ? eNa_strand_unknown : s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        // Deletion on master sequence (gap; insertion on slave)
        case GAPALIGN_DEL:
            m_start = GAP_VALUE;

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2, edit_block->length2,
                    edit_block->original_length2, edit_block->frame2);

            if (edit_block->reverse) {
                strands.push_back(s_strand);
                strands.push_back(i == 0 ? eNa_strand_unknown : m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(i == 0 ? eNa_strand_unknown : m_strand);
                strands.push_back(s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        default:
            break;
        }

        lengths.push_back(esp->num);
    }

    // Make sure the vectors have the right size
    if (lengths.size() != nsegs)
        lengths.resize(nsegs);

    if (starts.size() != nsegs*2)
        starts.resize(nsegs*2);

    if (strands.size() != nsegs*2)
        strands.resize(nsegs*2);
}

static CRef<CDense_seg>
x_CreateDenseg(CConstRef<CSeq_id> master, CConstRef<CSeq_id> slave,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    _ASSERT(master.NotEmpty());
    _ASSERT(slave.NotEmpty());

    CRef<CDense_seg> dense_seg(new CDense_seg());

    // Pairwise alignment is 2 dimensional
    dense_seg->SetDim(2);

    // Set the sequence ids
    CDense_seg::TIds& ids = dense_seg->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(master->AsFastaString()));
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(slave->AsFastaString()));
    ids.push_back(tmp);
    ids.resize(dense_seg->GetDim());

    dense_seg->SetLens() = lengths;
    dense_seg->SetStrands() = strands;
    dense_seg->SetStarts() = starts;
    dense_seg->SetNumseg(lengths.size());

    return dense_seg;
}

static CSeq_align::C_Segs::TStd
x_CreateStdSegs(CConstRef<CSeq_id> master, CConstRef<CSeq_id> slave, 
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands, bool reverse, bool translate_master, bool
        translate_slave)
{
    _ASSERT(master.NotEmpty());
    _ASSERT(slave.NotEmpty());

    CSeq_align::C_Segs::TStd retval;
    int nsegs = lengths.size();         // number of segments in alignment
    TSignedSeqPos m_start, m_stop;      // start and stop for master sequence
    TSignedSeqPos s_start, s_stop;      // start and stop for slave sequence

    CRef<CSeq_id> master_id(new CSeq_id(master->AsFastaString()));
    CRef<CSeq_id> slave_id(new CSeq_id(slave->AsFastaString()));
    
    for (int i = 0; i < nsegs; i++) {
        CRef<CStd_seg> std_seg(new CStd_seg());
        CRef<CSeq_loc> master_loc(new CSeq_loc());
        CRef<CSeq_loc> slave_loc(new CSeq_loc());

        // Pairwise alignment is 2 dimensional
        std_seg->SetDim(2);

        // Set master seqloc
        if ( (m_start = starts[2*i]) != GAP_VALUE) {
            master_loc->SetInt().SetId(*master_id);
            master_loc->SetInt().SetFrom(m_start);
            if (translate_master || (reverse && translate_slave))
                m_stop = m_start + CODON_LENGTH*lengths[i] - 1;
            else
                m_stop = m_start + lengths[i] - 1;
            master_loc->SetInt().SetTo(m_stop);
            master_loc->SetInt().SetStrand(strands[2*i]);
        } else {
            master_loc->SetEmpty(*master_id);
        }

        // Set slave seqloc
        if ( (s_start = starts[2*i+1]) != GAP_VALUE) {
            slave_loc->SetInt().SetId(*slave_id);
            slave_loc->SetInt().SetFrom(s_start);
            if (translate_slave || (reverse && translate_master))
                s_stop = s_start + CODON_LENGTH*lengths[i] - 1;
            else
                s_stop = s_start + lengths[i] - 1;
            slave_loc->SetInt().SetTo(s_stop);
            slave_loc->SetInt().SetStrand(strands[2*i+1]);
        } else {
            slave_loc->SetEmpty(*slave_id);
        }

        if (reverse) {
            std_seg->SetIds().push_back(slave_id);
            std_seg->SetIds().push_back(master_id);
            std_seg->SetLoc().push_back(slave_loc);
            std_seg->SetLoc().push_back(master_loc);
        } else {
            std_seg->SetIds().push_back(master_id);
            std_seg->SetIds().push_back(slave_id);
            std_seg->SetLoc().push_back(master_loc);
            std_seg->SetLoc().push_back(slave_loc);
        }

        retval.push_back(std_seg);
    }

    return retval;
}

/// Clone of GXECorrectUASequence (tools/gapxdrop.c)
/// Assumes GAPALIGN_DECLINE regions are to the right of GAPALIGN_{INS,DEL}.
/// This function swaps them (GAPALIGN_DECLINE ends to the right of the gap)
static void 
x_CorrectUASequence(GapEditBlockPtr edit_block)
{
    GapEditScriptPtr curr = NULL, curr_last = NULL;
    GapEditScriptPtr indel_prev = NULL; // pointer to node before the last
            // insertion or deletion followed immediately by GAPALIGN_DECLINE
    bool last_indel = false;    // last operation was an insertion or deletion

    for (curr = edit_block->esp; curr; curr = curr->next) {

        // if GAPALIGN_DECLINE immediately follows an insertion or deletion
        if (curr->op_type == GAPALIGN_DECLINE && last_indel) {
            /* This is invalid condition and regions should be
               exchanged */

            if (indel_prev != NULL)
                indel_prev->next = curr;
            else
                edit_block->esp = curr; /* First element in a list */

            // CC: If flaking gaps are allowed, curr_last could be NULL and the
            // following statement would core dump...
            curr_last->next = curr->next;
            curr->next = curr_last;
        }

        last_indel = false;

        if (curr->op_type == GAPALIGN_INS || curr->op_type == GAPALIGN_DEL) {
            last_indel = true;
            indel_prev = curr_last;
        }

        curr_last = curr;
    }
    return;
}

/// C++ version of GXEMakeSeqAlign (tools/gapxdrop.c)
static CRef<CSeq_align>
x_CreateSeqAlign(CConstRef<CSeq_id> master, CConstRef<CSeq_id> slave,
    vector<TSignedSeqPos> starts, vector<TSeqPos> lengths,
    vector<ENa_strand> strands, bool translate_master, bool translate_slave,
    bool reverse)
{
    CRef<CSeq_align> sar(new CSeq_align());
    sar->SetType(CSeq_align::eType_partial);
    sar->SetDim(2);         // BLAST only creates pairwise alignments

    if (translate_master || translate_slave) {
        sar->SetSegs().SetStd() =
            x_CreateStdSegs(master, slave, starts, lengths, strands,
                    reverse, translate_master, translate_slave);
    } else {
        CRef<CDense_seg> dense_seg = 
            x_CreateDenseg(master, slave, starts, lengths, strands);
        sar->SetSegs().SetDenseg(*dense_seg);
    }

    return sar;
}

static CRef<CSeq_align>
x_GapEditBlock2SeqAlign(const GapEditBlockPtr edit_block, 
        CConstRef<CSeq_id> id1, CConstRef<CSeq_id> id2)
{
    _ASSERT(edit_block != NULL);

    vector<TSignedSeqPos> starts;
    vector<TSeqPos> lengths;
    vector<ENa_strand> strands;
    bool is_disc_align = false;
    int nsegs = 0;      // number of segments in edit_block->esp

    for (GapEditScriptPtr t = edit_block->esp; t; t = t->next, nsegs++) {
        if (t->op_type == GAPALIGN_DECLINE)
            is_disc_align = true;
    }

    if (is_disc_align) {

        /* By request of Steven Altschul - we need to have 
           the unaligned part being to the left if it is adjacent to the
           gap (insertion or deletion) - so this function will do
           shuffeling */
        x_CorrectUASequence(edit_block);

        CRef<CSeq_align> seqalign(new CSeq_align());
        seqalign->SetType(CSeq_align::eType_partial);
        seqalign->SetDim(2);         // BLAST only creates pairwise alignments

        bool skip_region;
        GapEditScriptPtr curr = NULL, curr_head = edit_block->esp;

        while (curr_head) {
            skip_region = false;

            for (nsegs = 0, curr = curr_head; curr; curr = curr->next, nsegs++){
                if (curr->op_type == GAPALIGN_DECLINE) {
                    if (nsegs != 0) { // end of aligned region
                        break;
                    } else {
                        while (curr && curr->op_type == GAPALIGN_DECLINE) {
                            nsegs++;
                            curr = curr->next;
                        }
                        skip_region = true;
                        break;
                    }
                }
            }

            // build seqalign for required regions only
            if (!skip_region) {

                x_CollectSeqAlignData(edit_block, curr_head, nsegs, starts,
                        lengths, strands);

                CRef<CSeq_align> sa_tmp = x_CreateSeqAlign(id1, id2, starts,
                        lengths, strands, edit_block->translate1,
                        edit_block->translate2, edit_block->reverse);

                // Add this seqalign to the list
                if (sa_tmp)
                    seqalign->SetSegs().SetDisc().Set().push_back(sa_tmp);
            }
            curr_head = curr;
        }

        return seqalign;

    } else {

        x_CollectSeqAlignData(edit_block, edit_block->esp, nsegs, 
                starts, lengths, strands);

        return x_CreateSeqAlign(id1, id2, starts, lengths, strands,
                edit_block->translate1, edit_block->translate2,
                edit_block->reverse);
    }
}

/// Creates and initializes CScore with i (if it's non-zero) or d
static CRef<CScore> 
x_MakeScore(const string& ident_string, double d = 0.0, int i = 0)
{
    CRef<CScore> retval(new CScore());

    CRef<CObject_id> id(new CObject_id());
    id->SetStr(ident_string);
    retval->SetId(*id);

    CRef<CScore::C_Value> val(new CScore::C_Value());
    if (i)
        val->SetInt(i);
    else
        val->SetReal(d);
    retval->SetValue(*val);

    return retval;
}

/// C++ version of GetScoreSetFromBlastHsp (tools/blastutl.c)
static void
x_BuildScoreList(const BlastHSPPtr hsp, const BLAST_ScoreBlkPtr sbp, const
        BlastScoringOptionsPtr score_options, CSeq_align::TScore& scores,
        CBlastOption::EProgram program)
{
    string score_type;
    BLAST_KarlinBlkPtr kbp = NULL;

    if (!hsp)
        return;

    score_type = "score";
    if (hsp->score)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->score));

    score_type = "sum_n";
    if (hsp->num > 1)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->num));

    // Set the E-Value
    double evalue = (hsp->evalue < SMALLEST_EVALUE) ? 0.0 : hsp->evalue;
    score_type = (hsp->num == 1) ? "e_value" : "sum_e";
    if (evalue >= 0.0)
        scores.push_back(x_MakeScore(score_type, evalue));

    // Calculate the bit score from the raw score
    score_type = "bit_score";
    if (program == CBlastOption::eBlastn || !score_options->gapped_calculation)
        kbp = sbp->kbp[hsp->context];
    else
        kbp = sbp->kbp_gap[hsp->context];

    double bit_score = ((hsp->score*kbp->Lambda) - kbp->logK)/NCBIMATH_LN2;
    if (bit_score >= 0.0)
        scores.push_back(x_MakeScore(score_type, bit_score));

    // Set the identity score
    score_type = "num_ident";
    if (hsp->num_ident > 0)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->num_ident));

    if (hsp->num > 1 && hsp->ordering_method == BLAST_SMALL_GAPS) {
        score_type = "small_gap";
        scores.push_back(x_MakeScore(score_type, 0.0, 1));
    } else if (hsp->ordering_method > 3) {
        // In new tblastn this means splice junction was found
        score_type = "splice_junction";
        scores.push_back(x_MakeScore(score_type, 0.0, 1));
    }

    return;
}


static void
x_AddScoresToSeqAlign(CRef<CSeq_align>& seqalign, const BlastHSPPtr hsp, 
        const BLAST_ScoreBlkPtr sbp, CBlastOption::EProgram program,
        const BlastScoringOptionsPtr score_options)
{
    // Add the scores for this HSP
    CSeq_align::TScore& score_list = seqalign->SetScore();
    x_BuildScoreList(hsp, sbp, score_options, score_list, program);

    // Add scores for special cases: stdseg and disc alignments
    if (seqalign->GetSegs().IsStd()) {
        CSeq_align::C_Segs::TStd& stdseg_list = seqalign->SetSegs().SetStd();

        NON_CONST_ITERATE(CSeq_align::C_Segs::TStd, itr, stdseg_list) {
            CStd_seg::TScores& scores = (**itr).SetScores();
            x_BuildScoreList(hsp, sbp, score_options, scores, program);
        }
    } else if (seqalign->GetSegs().IsDisc()) {
        CSeq_align_set& seqalign_set = seqalign->SetSegs().SetDisc();

        NON_CONST_ITERATE(list< CRef<CSeq_align> >, itr, seqalign_set.Set()) {
            CSeq_align::TScore& score = (**itr).SetScore();
            x_BuildScoreList(hsp, sbp, score_options, score, program);
        }
    }
}

// This is called for each query in the BLAST search
static CRef<CSeq_align_set>
x_ProcessBlastHitList(BlastHitListPtr hit_list, 
        CConstRef<CSeq_id>& query_seqid,
        const ReadDBFILEPtr rdfp, CConstRef<CSeq_id>& subject_id, 
        const BlastScoringOptionsPtr score_options, const BLAST_ScoreBlkPtr sbp,
        CBlastOption::EProgram program)
{
    CRef<CSeq_align_set> retval(new CSeq_align_set()); 

    // Process the list of HSPs corresponding to each subject sequence (either
    // hits in the database or hits to a single sequence in the case of
    // Blast2Sequences)
    for (int i = 0; i < hit_list->hsplist_count; i++) {
        BlastHSPListPtr hsp_list = hit_list->hsplist_array[i];
        CConstRef<CSeq_id> curr_subject;

        if (!hsp_list)
            continue;

        if (rdfp) {
            // Get C SeqId and convert it to C++ Seq_id
            SeqIdPtr c_subject_id = NULL;
            readdb_get_descriptor(rdfp, hsp_list->oid, &c_subject_id, NULL);
            DECLARE_ASN_CONVERTER(CSeq_id, SeqId, sic);
            curr_subject = sic.FromC(c_subject_id);
            SeqIdSetFree(c_subject_id);
        } else {
            curr_subject.Reset(subject_id);
        }

        // Create a seqalign for each HSP in a hsp array
        for (int j = 0; j < hsp_list->hspcnt; j++) {
            BlastHSPPtr hsp = hsp_list->hsp_array[j];
            CRef<CSeq_align> seqalign = 
                x_GapEditBlock2SeqAlign(hsp->gap_info, query_seqid,
                        curr_subject);

            x_AddScoresToSeqAlign(seqalign, hsp, sbp, program, score_options);

            retval->Set().push_back(seqalign);
        }
    }
    if ( !(retval->Get().size() > 0))
        retval.Reset(NULL);
    return retval;
}

CRef<CSeq_align_set>
BLAST_Results2CppSeqAlign(const BlastResults* results, 
        CBlastOption::EProgram prog,
        vector< CConstRef<CSeq_id> >& query_seqids, 
        const ReadDBFILEPtr rdfp, 
        CConstRef<CSeq_id>& subject_seqid,
        const BlastScoringOptionsPtr score_options, 
        const BLAST_ScoreBlkPtr sbp)
{
    _ASSERT(results->num_queries == (int)query_seqids.size());
    CRef<CSeq_align_set> retval(new CSeq_align_set());

    // Process each query's hit list
    for (int i = 0; i < results->num_queries; i++) {
        BlastHitListPtr hit_list = results->hitlist_array[i];

        if (!hit_list)
            continue;

        // Should return a CSeq_align_set for each HSP in every
        // matching sequence (each sequence can have more than one HSP) or for
        // each HSP in subject_seqid (i.e.: bl2seq)
        CRef<CSeq_align_set> seqaligns = 
            x_ProcessBlastHitList(hit_list, query_seqids[i], rdfp, 
                    subject_seqid, score_options, sbp, prog);
        if (seqaligns)
            retval->Set().merge(seqaligns->Set());

        _TRACE("Query " << i << ": " << retval->Get().size() << " seqaligns");
    }

    if ( !(retval->Get().size() > 0))
        retval.Reset(NULL);
    
    return retval;
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/07/23 21:28:23  camacho
* Use new local gapinfo structures
*
* Revision 1.2  2003/07/14 21:40:22  camacho
* Pacify compiler warnings
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
