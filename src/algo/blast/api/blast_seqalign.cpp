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
* ===========================================================================
*/

/// @file blast_seqalign.cpp
/// Utility function to convert internal BLAST result structures into
/// CSeq_align_set objects.

#include <ncbi_pch.hpp>
#include "blast_seqalign.hpp"

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/general/Object_id.hpp>
#include <serial/iterator.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef SMALLEST_EVALUE
/// Threshold below which e-values are saved as 0
#define SMALLEST_EVALUE 1.0e-180
#endif
#ifndef GAP_VALUE
/// Value in the Dense-seg indicating a gap
#define GAP_VALUE -1
#endif

/// Converts a frame into the appropriate strand
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

/// Advances position in a sequence, according to an edit script instruction.
/// @param pos Current position on input, next position on output  [in] [out]
/// @param pos2advance How much the position should be advanced? [in]
/// @return Current position.
static int 
x_GetCurrPos(int& pos, int pos2advance)
{
    int retval;

    if (pos < 0) /// @todo FIXME: is this condition possible? 
        retval = -(pos + pos2advance - 1);
    else
        retval = pos;
    pos += pos2advance;
    return retval;
}

/// Finds the starting position of a sequence segment in an alignment, given an 
/// editing script.
/// @param curr_pos Current position on input, modified to next position on 
///                 output [in] [out]
/// @param esp Traceback editing script [in]
/// @param strand Sequence strand [in]
/// @param translate Is sequence translated? [in]
/// @param length Sequence length [in]
/// @param original_length Original (nucleotide) sequence length, if it is 
///                        translated [in]
/// @param frame Translating frame [in]
/// @return Start position of the current alignment segment.
static TSeqPos
x_GetAlignmentStart(int& curr_pos, const GapEditScript* esp, 
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

/// Fills vectors of start positions, lengths and strands for all alignment segments.
/// Note that even though the edit_block is passed in, data for seqalign is
/// collected from the esp_head argument for nsegs segments. This editing script may
/// not be the full editing scripg if a discontinuous alignment is being built.
/// @param edit_block Traceback editing block. [in]
/// @param esp_head Traceback editing script linked list [in]
/// @param nsegs Number of alignment segments [in]
/// @param starts Vector of starting positions to fill [out]
/// @param lengths Vector of segment lengths to fill [out]
/// @param strands Vector of segment strands to fill [out]
static void
x_CollectSeqAlignData(const GapEditBlock* edit_block, 
        const GapEditScript* esp_head, unsigned int nsegs,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    ASSERT(edit_block != NULL);

    GapEditScript* esp = const_cast<GapEditScript*>(esp_head);
    ENa_strand m_strand, s_strand;      // strands of alignment
    TSignedSeqPos m_start, s_start;     // running starts of alignment
    int start1 = edit_block->start1;    // start of alignment on master sequence
    int start2 = edit_block->start2;    // start of alignment on slave sequence

    m_strand = x_Frame2Strand(edit_block->frame1);
    s_strand = x_Frame2Strand(edit_block->frame2);

    for (unsigned int i = 0; esp && i < nsegs; esp = esp->next, i++) {
        switch (esp->op_type) {
        case eGapAlignDecline:
        case eGapAlignSub:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1 != 0, edit_block->length1,
                    edit_block->original_length1, edit_block->frame1);

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2 != 0, edit_block->length2,
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
        case eGapAlignIns:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1 != 0, edit_block->length1,
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
        case eGapAlignDel:
            m_start = GAP_VALUE;

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2 != 0, edit_block->length2,
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

/// Creates a Dense-seg object from the starts, lengths and strands vectors and two 
/// Seq-ids.
/// @param master Query Seq-id [in]
/// @param slave Subject Seq-ids [in]
/// @param starts Vector of start positions for alignment segments [in]
/// @param lengths Vector of alignment segments lengths [in]
/// @param strands Vector of alignment segments strands [in]
/// @return The Dense-seg object.
static CRef<CDense_seg>
x_CreateDenseg(const CSeq_id* master, const CSeq_id* slave,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    ASSERT(master);
    ASSERT(slave);

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
    dense_seg->SetNumseg((int) lengths.size());

    return dense_seg;
}

/// Creates a Std-seg object from the starts, lengths and strands vectors and two 
/// Seq-ids for a translated search.
/// @param master Query Seq-id [in]
/// @param slave Subject Seq-ids [in]
/// @param starts Vector of start positions for alignment segments [in]
/// @param lengths Vector of alignment segments lengths [in]
/// @param strands Vector of alignment segments strands [in]
/// @param reverse Is order of sequences reversed? [in]
/// @param translate_master Is query sequence translated? [in]
/// @param translate_slave Is subject sequenec translated? [in]
/// @return The Std-seg object.
static CSeq_align::C_Segs::TStd
x_CreateStdSegs(const CSeq_id* master, const CSeq_id* slave, 
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands, bool reverse, bool translate_master, bool
        translate_slave)
{
    ASSERT(master);
    ASSERT(slave);

    CSeq_align::C_Segs::TStd retval;
    int nsegs = (int) lengths.size();   // number of segments in alignment
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

/// Checks if any decline-to-align segments immediately follow an insertion or 
/// deletion, and swaps any such segments so indels are always to the right of 
/// the decline-to-align segments.
/// @param edit_block Traceback editing block [in] [out]
static void 
x_CorrectUASequence(GapEditBlock* edit_block)
{
    GapEditScript* curr = NULL,* curr_last = NULL;
    GapEditScript* indel_prev = NULL; // pointer to node before the last
            // insertion or deletion followed immediately by GAPALIGN_DECLINE
    bool last_indel = false;    // last operation was an insertion or deletion

    for (curr = edit_block->esp; curr; curr = curr->next) {

        // if GAPALIGN_DECLINE immediately follows an insertion or deletion
        if (curr->op_type == eGapAlignDecline && last_indel) {
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

        if (curr->op_type == eGapAlignIns || curr->op_type == eGapAlignDel) {
            last_indel = true;
            indel_prev = curr_last;
        }

        curr_last = curr;
    }
    return;
}

/// Creates a Seq-align for a single HSP from precalculated vectors of start 
/// positions, lengths and strands of segments, sequence identifiers and other 
/// information.
/// @param master Query sequence identifier [in]
/// @param slave Subject sequence identifier [in]
/// @param starts Start positions of alignment segments [in]
/// @param lengths Lengths of alignment segments [in]
/// @param strands Strands of alignment segments [in]
/// @param translate_master Is query translated? [in]
/// @param translate_slave Is subject translated? [in]
/// @param reverse Is order of sequences reversed? [in]
/// @return Resulting Seq-align object.
static CRef<CSeq_align>
x_CreateSeqAlign(const CSeq_id* master, const CSeq_id* slave,
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

/// Converts a traceback editing block to a Seq-align, provided the 2 sequence 
/// identifiers.
/// @param edit_block Traceback editing block [in]
/// @param id1 Query sequence identifier [in]
/// @param id2 Subject sequence identifier [in]
/// @return Resulting Seq-align object.
static CRef<CSeq_align>
x_GapEditBlock2SeqAlign(GapEditBlock* edit_block, 
        const CSeq_id* id1, const CSeq_id* id2)
{
    ASSERT(edit_block != NULL);

    vector<TSignedSeqPos> starts;
    vector<TSeqPos> lengths;
    vector<ENa_strand> strands;
    bool is_disc_align = false;
    int nsegs = 0;      // number of segments in edit_block->esp

    for (GapEditScript* t = edit_block->esp; t; t = t->next, nsegs++) {
        if (t->op_type == eGapAlignDecline)
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
        GapEditScript* curr = NULL,* curr_head = edit_block->esp;

        while (curr_head) {
            skip_region = false;

            for (nsegs = 0, curr = curr_head; curr; curr = curr->next, nsegs++){
                if (curr->op_type == eGapAlignDecline) {
                    if (nsegs != 0) { // end of aligned region
                        break;
                    } else {
                        while (curr && curr->op_type == eGapAlignDecline) {
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
                        lengths, strands, edit_block->translate1 !=0,
                        edit_block->translate2 != 0, edit_block->reverse != 0);

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
                edit_block->translate1 != 0, edit_block->translate2 != 0,
                edit_block->reverse != 0);
    }
}

/// This function is used for out-of-frame traceback conversion
/// Converts an OOF editing script chain to a Seq-align of type Std-seg.
/// @param program BLAST program: blastx or tblastn.
/// @param edit_block Traceback editing block produced by an out-of-frame 
///                   gapped extension [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
static CRef<CSeq_align>
x_OOFEditBlock2SeqAlign(EProgram program, 
    GapEditBlock* edit_block, 
    const CSeq_id* query_id, const CSeq_id* subject_id)
{
    ASSERT(edit_block != NULL);

    CRef<CSeq_align> seqalign(new CSeq_align());

    Boolean reverse = FALSE;
    GapEditScript* curr,* esp;
    Int2 frame1, frame2;
    Int4 start1, start2;
    Int4 original_length1, original_length2;
    CRef<CSeq_interval> seq_int1_last;
    CRef<CSeq_interval> seq_int2_last;
    CConstRef<CSeq_id> id1;
    CConstRef<CSeq_id> id2;
    CRef<CSeq_loc> slp1, slp2;
    ENa_strand strand1, strand2;
    bool first_shift;
    Int4 from1, from2, to1, to2;
    CRef<CSeq_id> tmp;

    if (program == eBlastx) {
       reverse = TRUE;
       start1 = edit_block->start2;
       start2 = edit_block->start1;
       frame1 = edit_block->frame2;
       frame2 = edit_block->frame1;
       original_length1 = edit_block->original_length2;
       original_length2 = edit_block->original_length1;
       id1.Reset(subject_id);
       id2.Reset(query_id);
    } else { 
       start1 = edit_block->start1;
       start2 = edit_block->start2;
       frame1 = edit_block->frame1;
       frame2 = edit_block->frame2;
       original_length1 = edit_block->original_length1;
       original_length2 = edit_block->original_length2;
       id1.Reset(query_id);
       id2.Reset(subject_id);
    }
 
    strand1 = x_Frame2Strand(frame1);
    strand2 = x_Frame2Strand(frame2);
    
    esp = edit_block->esp;
    
    seqalign->SetDim(2); /**only two dimention alignment**/
    
    seqalign->SetType(CSeq_align::eType_partial); /**partial for gapped translating search. */

    esp = edit_block->esp;

    first_shift = false;

    for (curr=esp; curr; curr=curr->next) {

        slp1.Reset(new CSeq_loc());
        slp2.Reset(new CSeq_loc());
        
        switch (curr->op_type) {
        case eGapAlignDel: /* deletion of three nucleotides. */
            
            first_shift = false;

            slp1->SetInt().SetFrom(x_GetCurrPos(start1, curr->num));
            slp1->SetInt().SetTo(MIN(start1,original_length1) - 1);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            
            /* Empty nucleotide piece */
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetEmpty(*tmp);

            seq_int1_last.Reset(&slp1->SetInt());
            /* Keep previous seq_int2_last, in case there is a frame shift
               immediately after this gap */
            
            break;

        case eGapAlignIns: /* insertion of three nucleotides. */
            /* If gap is followed after frameshift - we have to
               add this element for the alignment to be correct */
            
            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                slp1->SetInt().SetFrom(x_GetCurrPos(start1, 1));
                to1 = MIN(start1,original_length1) - 1;
                slp1->SetInt().SetTo(to1);
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                
                /* Nucleotide scale shifted by op_type */
                from2 = x_GetCurrPos(start2, 3);
                to2 = MIN(start2,original_length2) - 1;
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                if (start2 > original_length2)
                    slp1->SetInt().SetTo(to1 - 1);
                
                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    slp2->SetInt().SetTo(original_length2 - from2 - 1);
                    slp2->SetInt().SetFrom(original_length2 - to2 - 1);
                }
                
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);
                slp2->SetInt().SetStrand(strand2);

                CRef<CStd_seg> seg(new CStd_seg());
                seg->SetDim(2);

                CStd_seg::TIds& ids = seg->SetIds();

                if (reverse) {
                    seg->SetLoc().push_back(slp2);
                    seg->SetLoc().push_back(slp1);
                    tmp.Reset(new CSeq_id(id2->AsFastaString()));
                    ids.push_back(tmp);
                    tmp.Reset(new CSeq_id(id1->AsFastaString()));
                    ids.push_back(tmp);
                } else {
                    seg->SetLoc().push_back(slp1);
                    seg->SetLoc().push_back(slp2);
                    tmp.Reset(new CSeq_id(id1->AsFastaString()));
                    ids.push_back(tmp);
                    tmp.Reset(new CSeq_id(id2->AsFastaString()));
                    ids.push_back(tmp);
                }
                ids.resize(seg->GetDim());
                
                seqalign->SetSegs().SetStd().push_back(seg);
            }

            first_shift = false;

            /* Protein piece is empty */
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetEmpty(*tmp);
            
            /* Nucleotide scale shifted by 3, protein gapped */
            from2 = x_GetCurrPos(start2, curr->num*3);
            to2 = MIN(start2,original_length2) - 1;
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);

            /* Transfer to DNA minus strand coordinates */
            if(strand2 == eNa_strand_minus) {
                slp2->SetInt().SetTo(original_length2 - from2 - 1);
                slp2->SetInt().SetFrom(original_length2 - to2 - 1);
            }
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
            
            seq_int1_last.Reset(NULL);
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;

        case eGapAlignSub: /* Substitution. */

            first_shift = false;

            /* Protein coordinates */
            from1 = x_GetCurrPos(start1, curr->num);
            to1 = MIN(start1, original_length1) - 1;
            /* Adjusting last segment and new start point in
               nucleotide coordinates */
            from2 = x_GetCurrPos(start2, curr->num*((Uint1)curr->op_type));
            to2 = start2 - 1;
            /* Chop off three bases and one residue at a time.
               Why does this happen, seems like a bug?
            */
            while (to2 >= original_length2) {
                to2 -= 3;
                to1--;
            }
            /* Transfer to DNA minus strand coordinates */
            if(strand2 == eNa_strand_minus) {
                int tmp_int;
                tmp_int = to2;
                to2 = original_length2 - from2 - 1;
                from2 = original_length2 - tmp_int - 1;
            }

            slp1->SetInt().SetFrom(from1);
            slp1->SetInt().SetTo(to1);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
           

            seq_int1_last.Reset(&slp1->SetInt()); /* Will be used to adjust "to" value */
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;
        case eGapAlignDel2:	/* gap of two nucleotides. */
        case eGapAlignDel1: /* Gap of one nucleotide. */
        case eGapAlignIns1: /* Insertion of one nucleotide. */
        case eGapAlignIns2: /* Insertion of two nucleotides. */

            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                from1 = x_GetCurrPos(start1, 1);
                to1 = MIN(start1,original_length1) - 1;

                /* Nucleotide scale shifted by op_type */
                from2 = x_GetCurrPos(start2, (Uint1)curr->op_type);
                to2 = start2 - 1;
                if (to2 >= original_length2) {
                    to2 = original_length2 -1;
                    to1--;
                }
                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    int tmp_int;
                    tmp_int = to2;
                    to2 = original_length2 - from2 - 1;
                    from2 = original_length2 - tmp_int - 1;
                }

                slp1->SetInt().SetFrom(from1);
                slp1->SetInt().SetTo(to1);
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);
                slp2->SetInt().SetStrand(strand2);

                seq_int1_last.Reset(&slp1->SetInt()); 
                seq_int2_last.Reset(&slp2->SetInt()); 

                break;
            }
            
            first_shift = true;

            /* If this substitution is following simple frameshift
               we do not need to start new segment, but may continue
               old one */

            if(seq_int2_last) {
                x_GetCurrPos(start2, curr->num*((Uint1)curr->op_type-3));
                if(strand2 != eNa_strand_minus) {
                    seq_int2_last->SetTo(start2 - 1);
                } else {
                    /* Transfer to DNA minus strand coordinates */
                    seq_int2_last->SetFrom(original_length2 - start2);
                }

                /* Adjustment for multiple shifts - theoretically possible,
                   but very improbable */
                if(seq_int2_last->GetFrom() > seq_int2_last->GetTo()) {
                    
                    if(strand2 != eNa_strand_minus) {
                        seq_int2_last->SetTo(seq_int2_last->GetTo() + 3);
                    } else {
                        seq_int2_last->SetFrom(seq_int2_last->GetFrom() - 3);
                    }
                    
                    if (seq_int1_last.GetPointer() &&
						seq_int1_last->GetTo() != 0)
                        seq_int1_last->SetTo(seq_int1_last->GetTo() + 1);
                }

            } else if ((Uint1)curr->op_type > 3) {
                /* Protein piece is empty */
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetEmpty(*tmp);
                /* Simulating insertion of nucleotides */
                from2 = x_GetCurrPos(start2, 
                                     curr->num*((Uint1)curr->op_type-3));
                to2 = MIN(start2,original_length2) - 1;

                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    int tmp_int;
                    tmp_int = to2;
                    to2 = original_length2 - from2 - 1;
                    from2 = original_length2 - tmp_int - 1;
                }
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);

                seq_int1_last.Reset(NULL);
                seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
                break;
            } else {
                continue;       /* Main loop */
            }
            continue;       /* Main loop */
            /* break; */
        default:
            continue;       /* Main loop */
            /* break; */
        } 

        CRef<CStd_seg> seg(new CStd_seg());
        seg->SetDim(2);
        
        CStd_seg::TIds& ids = seg->SetIds();

        if (reverse) {
            seg->SetLoc().push_back(slp2);
            seg->SetLoc().push_back(slp1);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            ids.push_back(tmp);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            ids.push_back(tmp);
        } else {
            seg->SetLoc().push_back(slp1);
            seg->SetLoc().push_back(slp2);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            ids.push_back(tmp);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            ids.push_back(tmp);
        }
        ids.resize(seg->GetDim());
        
        seqalign->SetSegs().SetStd().push_back(seg);
    }
    
    return seqalign;
}

/// Creates and initializes CScore with a given name, and with integer or 
/// double value. Integer value is used if it is not zero, otherwise 
/// double value is assigned.
/// @param ident_string Score type name [in]
/// @param d Real value of the score [in]
/// @param i Integer value of the score. [in]
/// @return Resulting CScore object.
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

/// Creates a list of score objects for a Seq-align, given an HSP structure.
/// @param hsp Structure containing HSP information [in]
/// @param scores Linked list of score objects to put into a Seq-align [out]
static void
x_BuildScoreList(const BlastHSP* hsp, CSeq_align::TScore& scores)
{
    string score_type;

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
    score_type = (hsp->num <= 1) ? "e_value" : "sum_e";
    if (evalue >= 0.0)
        scores.push_back(x_MakeScore(score_type, evalue));

    // Calculate the bit score from the raw score
    score_type = "bit_score";

    if (hsp->bit_score >= 0.0)
        scores.push_back(x_MakeScore(score_type, hsp->bit_score));

    // Set the identity score
    score_type = "num_ident";
    if (hsp->num_ident > 0)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->num_ident));

    return;
}


/// Given an HSP structure, creates a list of scores and inserts them into 
/// a Seq-align.
/// @param seqalign Seq-align object to fill [in] [out]
/// @param hsp An HSP structure [in]
static void
x_AddScoresToSeqAlign(CRef<CSeq_align>& seqalign, const BlastHSP* hsp)
{
    // Add the scores for this HSP
    CSeq_align::TScore& score_list = seqalign->SetScore();
    x_BuildScoreList(hsp, score_list);
}


/// Creates a Dense-diag object from HSP information and sequence identifiers
/// for a non-translated ungapped search.
/// @param An HSP structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
/// @return Resulting Dense-diag object.
CRef<CDense_diag>
x_UngappedHSPToDenseDiag(BlastHSP* hsp, const CSeq_id *query_id, 
    const CSeq_id *subject_id,
    Int4 query_length, Int4 subject_length)
{
    CRef<CDense_diag> retval(new CDense_diag());
    
    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    // Set the sequence ids
    CDense_diag::TIds& ids = retval->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(query_id->AsFastaString()));
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(subject_id->AsFastaString()));
    ids.push_back(tmp);
    ids.resize(retval->GetDim());

    retval->SetLen(hsp->query.length);

    CDense_diag::TStrands& strands = retval->SetStrands();
    strands.push_back(x_Frame2Strand(hsp->query.frame));
    strands.push_back(x_Frame2Strand(hsp->subject.frame));
    CDense_diag::TStarts& starts = retval->SetStarts();
    if (hsp->query.frame >= 0) {
       starts.push_back(hsp->query.offset);
    } else {
       starts.push_back(query_length - hsp->query.offset - hsp->query.length);
    }
    if (hsp->subject.frame >= 0) {
       starts.push_back(hsp->subject.offset);
    } else {
       starts.push_back(subject_length - hsp->subject.offset - 
                        hsp->subject.length);
    }

    CSeq_align::TScore& score_list = retval->SetScores();
    x_BuildScoreList(hsp, score_list);

    return retval;
}

/// Creates a Std-seg object from HSP information and sequence identifiers
/// for a translated ungapped search.
/// @param An HSP structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
/// @return Resulting Std-seg object.
CRef<CStd_seg>
x_UngappedHSPToStdSeg(BlastHSP* hsp, const CSeq_id *query_id, 
    const CSeq_id *subject_id,
    Int4 query_length, Int4 subject_length)
{
    CRef<CStd_seg> retval(new CStd_seg());

    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    CRef<CSeq_loc> subject_loc(new CSeq_loc());

    // Set the sequence ids
    CStd_seg::TIds& ids = retval->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(query_id->AsFastaString()));
    query_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(subject_id->AsFastaString()));
    subject_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    ids.resize(retval->GetDim());

    query_loc->SetInt().SetStrand(x_Frame2Strand(hsp->query.frame));
    subject_loc->SetInt().SetStrand(x_Frame2Strand(hsp->subject.frame));

    if (hsp->query.frame == 0) {
       query_loc->SetInt().SetFrom(hsp->query.offset);
       query_loc->SetInt().SetTo(hsp->query.offset + hsp->query.length - 1);
    } else if (hsp->query.frame > 0) {
       query_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->query.offset) + 
                                   hsp->query.frame - 1);
       query_loc->SetInt().SetTo(CODON_LENGTH*(hsp->query.offset+
                                               hsp->query.length)
                                 + hsp->query.frame - 2);
    } else {
       query_loc->SetInt().SetFrom(query_length -
           CODON_LENGTH*(hsp->query.offset+hsp->query.length) +
           hsp->query.frame + 1);
       query_loc->SetInt().SetTo(query_length - CODON_LENGTH*hsp->query.offset
                                 + hsp->query.frame);
    }

    if (hsp->subject.frame == 0) {
       subject_loc->SetInt().SetFrom(hsp->subject.offset);
       subject_loc->SetInt().SetTo(hsp->subject.offset + 
                                   hsp->subject.length - 1);
    } else if (hsp->subject.frame > 0) {
       subject_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->subject.offset) + 
                                   hsp->subject.frame - 1);
       subject_loc->SetInt().SetTo(CODON_LENGTH*(hsp->subject.offset+
                                               hsp->subject.length) +
                                   hsp->subject.frame - 2);

    } else {
       subject_loc->SetInt().SetFrom(subject_length -
           CODON_LENGTH*(hsp->subject.offset+hsp->subject.length) +
           hsp->subject.frame + 1);
       subject_loc->SetInt().SetTo(subject_length - 
           CODON_LENGTH*hsp->subject.offset + hsp->subject.frame);
    }

    retval->SetLoc().push_back(query_loc);
    retval->SetLoc().push_back(subject_loc);

    CSeq_align::TScore& score_list = retval->SetScores();
    x_BuildScoreList(hsp, score_list);

    return retval;
}

/// Creates a Seq-align from an HSP list for an ungapped search.
/// @param program BLAST program [in]
/// @param hsp_list HSP list structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
CRef<CSeq_align>
BLASTUngappedHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id, Int4 query_length, Int4 subject_length)
{
    CRef<CSeq_align> retval(new CSeq_align()); 
    BlastHSP** hsp_array;
    int index;

    retval->SetType(CSeq_align::eType_diags);

    hsp_array = hsp_list->hsp_array;

    /* All HSPs are put in one seqalign, containing a list of 
     * DenseDiag for same molecule search, or StdSeg for translated searches.
    */
    if (program == eBlastn || 
        program == eBlastp ||
        program == eRPSBlast) {
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            retval->SetSegs().SetDendiag().push_back(
                x_UngappedHSPToDenseDiag(hsp, query_id, subject_id, 
					 query_length, subject_length));
        }
    } else { // Translated search
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            retval->SetSegs().SetStd().push_back(
                x_UngappedHSPToStdSeg(hsp, query_id, subject_id, 
				      query_length, subject_length));
        }
    }

    return retval;
}

/// This is called for each query and each subject in a BLAST search.
/// We always return CSeq_aligns of type disc to allow multiple HSPs
/// corresponding to the same query-subject pair to be grouped in one CSeq_align.
/// @param program BLAST program [in]
/// @param hsp_list HSP list structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param is_ooframe Was this a search with out-of-frame gapping? [in]
/// @return Resulting Seq-align object. 
CRef<CSeq_align>
BLASTHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id, bool is_ooframe)
{
    CRef<CSeq_align> retval(new CSeq_align()); 
    retval->SetType(CSeq_align::eType_disc);
    retval->SetDim(2);         // BLAST only creates pairwise alignments

    // Process the list of HSPs corresponding to one subject sequence and
    // create one seq-align for each list of HSPs (use disc seqalign when
    // multiple HSPs are found).
    BlastHSP** hsp_array = hsp_list->hsp_array;

    for (int index = 0; index < hsp_list->hspcnt; index++) { 
        BlastHSP* hsp = hsp_array[index];
        CRef<CSeq_align> seqalign;

        if (is_ooframe) {
            seqalign = 
                x_OOFEditBlock2SeqAlign(program, hsp->gap_info, 
                    query_id, subject_id);
        } else {
            seqalign = 
                x_GapEditBlock2SeqAlign(hsp->gap_info, 
                    query_id, subject_id);
        }
        
        x_AddScoresToSeqAlign(seqalign, hsp);
        retval->SetSegs().SetDisc().Set().push_back(seqalign);
    }
    
    return retval;
}


/// Constructs an empty Seq-align-set containing an empty discontinuous
/// seq-align, and appends it to a previously constructed Seq-align-set.
/// @param sas Pointer to a Seq-align-set, to which new object should be 
///            appended (if not NULL).
/// @return Resulting Seq-align-set. 
CSeq_align_set*
x_CreateEmptySeq_align_set(CSeq_align_set* sas)
{
    CSeq_align_set* retval = NULL;

    if (!sas) {
        retval = new CSeq_align_set;
    } else {
        retval = sas;
    }

    CRef<CSeq_align> empty_seqalign(new CSeq_align);
    empty_seqalign->SetType(CSeq_align::eType_disc);
    empty_seqalign->SetSegs().SetDisc(*new CSeq_align_set);

    retval->Set().push_back(empty_seqalign);
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.52  2005/02/08 18:50:29  bealer
* - Fix type truncation warnings.
*
* Revision 1.51  2004/11/24 16:06:47  dondosha
* Added and/or fixed doxygen comments
*
* Revision 1.50  2004/11/22 16:08:54  dondosha
* Minor fix to make sure that "evalue" score type is always used when hsp is not part of a linked set
*
* Revision 1.49  2004/11/01 18:05:17  dondosha
* Added doxygen comments
*
* Revision 1.48  2004/08/16 19:47:35  dondosha
* Removed setting of splice_junction score type
*
* Revision 1.47  2004/06/07 21:34:55  dondosha
* Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
*
* Revision 1.46  2004/06/07 20:11:02  dondosha
* Removed no longer used arguments in x_AddScoresToSeqAlign
*
* Revision 1.45  2004/06/07 19:21:17  dondosha
* Removed arguments from x_BuildScoreList that are no longer used
*
* Revision 1.44  2004/06/07 18:26:29  dondosha
* Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
*
* Revision 1.43  2004/06/02 15:57:06  bealer
* - Isolate object manager dependent code.
*
* Revision 1.42  2004/05/21 21:41:02  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.41  2004/05/19 15:26:04  dondosha
* Edit script operations changed from macros to an enum
*
* Revision 1.40  2004/05/05 14:42:25  dondosha
* Correction in x_RemapAlignmentCoordinates for whole vs. interval Seq-locs
*
* Revision 1.39  2004/04/28 19:40:45  dondosha
* Fixed x_RemapAlignmentCoordinates function to work correctly with all strand combinations
*
* Revision 1.38  2004/04/19 12:59:12  madden
* Changed BLAST_KarlinBlk to Blast_KarlinBlk to avoid conflict with blastkar.h structure
*
* Revision 1.37  2004/04/16 14:28:19  papadopo
* add use of eRPSBlast program
*
* Revision 1.36  2004/04/06 20:47:14  dondosha
* Check if BLASTSeqSrcGetSeqId returns a pointer to CRef instead of a simple pointer to CSeq_id
*
* Revision 1.35  2004/03/24 19:14:14  dondosha
* BlastHSP structure does not have ordering_method field any more, but it does contain a splice_junction field
*
* Revision 1.34  2004/03/23 18:22:06  dondosha
* Minor memory leak fix
*
* Revision 1.33  2004/03/23 14:10:50  camacho
* Minor doxygen fix
*
* Revision 1.32  2004/03/19 19:22:55  camacho
* Move to doxygen group AlgoBlast, add missing CVS logs at EOF
*
* Revision 1.31  2004/03/16 22:03:38  camacho
* Remove dead code
*
* Revision 1.30  2004/03/15 19:58:55  dondosha
* Added BLAST_OneSubjectResults2CSeqalign function to retrieve single subject results from BlastHSPResults
*
* Revision 1.29  2003/12/19 20:16:10  dondosha
* Get length in x_GetSequenceLengthAndId regardless of whether search is gapped; do not call RemapToLoc for whole Seq-locs
*
* Revision 1.28  2003/12/03 16:43:47  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.27  2003/12/01 20:02:39  coulouri
* fix msvc warning
*
* Revision 1.26  2003/11/24 20:59:12  ucko
* Change one ASSERT to _ASSERT (probably more appropriate in general)
* due to MSVC brokenness.
*
* Revision 1.25  2003/11/24 17:14:58  camacho
* Remap alignment coordinates to original Seq-locs
*
* Revision 1.24  2003/11/04 18:37:36  dicuccio
* Fix for brain-dead MSVC (operator && is ambiguous)
*
* Revision 1.23  2003/11/04 17:13:31  dondosha
* Implemented conversion of results to seqalign for out-of-frame search
*
* Revision 1.22  2003/10/31 22:08:39  dondosha
* Implemented conversion of BLAST results to seqalign for ungapped search
*
* Revision 1.21  2003/10/31 00:05:15  camacho
* Changes to return discontinuous seq-aligns for each query-subject pair
*
* Revision 1.20  2003/10/30 21:40:57  dondosha
* Removed unneeded extra argument from BLAST_Results2CSeqAlign
*
* Revision 1.19  2003/10/28 20:53:59  camacho
* Temporarily use exception for unimplemented function
*
* Revision 1.18  2003/10/15 16:59:42  coulouri
* type correctness fixes
*
* Revision 1.17  2003/09/09 15:18:02  camacho
* Fix includes
*
* Revision 1.16  2003/08/19 20:27:51  dondosha
* Rewrote the results-to-seqalign conversion slightly
*
* Revision 1.15  2003/08/19 13:46:13  dicuccio
* Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
*
* Revision 1.14  2003/08/18 22:17:36  camacho
* Renaming of SSeqLoc members
*
* Revision 1.13  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.12  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.11  2003/08/15 15:56:32  dondosha
* Corrections in implementation of results-to-seqalign function
*
* Revision 1.10  2003/08/12 19:19:34  dondosha
* Use TSeqLocVector type
*
* Revision 1.9  2003/08/11 14:00:41  dicuccio
* Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
* BEGIN_NCBI_SCOPE block)
*
* Revision 1.8  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.7  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/25 22:12:46  camacho
* Use BLAST Sequence Source to retrieve sequence identifier
*
* Revision 1.4  2003/07/25 13:55:58  camacho
* Removed unnecessary #includes
*
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
