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
*   C++ Wrappers to NewBlast structures
*
*/

#include <BlastAux.hpp>

#include <objects/seqloc/Seq_interval.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

void
CQuerySetUpOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth)
    const
{
    ddc.SetFrame("CQuerySetUpOptionsPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "filter_string", m_Ptr->filter_string);
    DebugDumpValue(ddc, "strand_option", m_Ptr->strand_option);
    DebugDumpValue(ddc, "genetic_code", m_Ptr->genetic_code);
}

void
CBLAST_SequenceBlkPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBLAST_SequenceBlkPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "sequence", m_Ptr->sequence);
    DebugDumpValue(ddc, "sequence_start", m_Ptr->sequence_start);
    DebugDumpValue(ddc, "sequence_allocated", m_Ptr->sequence_allocated);
    DebugDumpValue(ddc, "sequence_start_allocated", m_Ptr->sequence_start_allocated);
    DebugDumpValue(ddc, "length", m_Ptr->length);

}

void
CBlastQueryInfoPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastQueryInfoPtr");
    if (!m_Ptr)
        return;

}
void
CLookupTableOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CLookupTableOptionsPtr");
    if (!m_Ptr)
        return;

    if (m_Ptr->matrixname)
        DebugDumpValue(ddc, "matrixname", m_Ptr->matrixname);
    DebugDumpValue(ddc, "threshold", m_Ptr->threshold);
    DebugDumpValue(ddc, "lut_type", m_Ptr->lut_type);
    DebugDumpValue(ddc, "word_size", m_Ptr->word_size);
    DebugDumpValue(ddc, "alphabet_size", m_Ptr->alphabet_size);
    DebugDumpValue(ddc, "mb_template_length", m_Ptr->mb_template_length);
    DebugDumpValue(ddc, "mb_template_type", m_Ptr->mb_template_type);
    DebugDumpValue(ddc, "max_positions", m_Ptr->max_positions);
    DebugDumpValue(ddc, "scan_step", m_Ptr->scan_step);
}

void
CLookupTableWrapPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CLookupTableWrapPtr");
    if (!m_Ptr)
        return;

}
void
CBlastInitialWordOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("BlastInitialWordOptionsPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "window_size", m_Ptr->window_size);
    DebugDumpValue(ddc, "extend_word_method", m_Ptr->extend_word_method);
    DebugDumpValue(ddc, "x_dropoff", m_Ptr->x_dropoff);
}
void
CBlastInitialWordParametersPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastInitialWordParametersPtr");
    if (!m_Ptr)
        return;

}
void
CBLAST_ExtendWordPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBLAST_ExtendWordPtr");
    if (!m_Ptr)
        return;

}

void
CBlastExtensionOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastExtensionOptionsPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "gap_x_dropoff", m_Ptr->gap_x_dropoff);
    DebugDumpValue(ddc, "gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
    DebugDumpValue(ddc, "gap_trigger", m_Ptr->gap_trigger);
    DebugDumpValue(ddc, "algorithm_type", m_Ptr->algorithm_type);
}

void
CBlastExtensionParametersPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastExtensionParametersPtr");
    if (!m_Ptr)
        return;

}

void
CBlastHitSavingOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastHitSavingOptionsPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "hitlist_size", m_Ptr->hitlist_size);
    DebugDumpValue(ddc, "hsp_num_max", m_Ptr->hsp_num_max);
    DebugDumpValue(ddc, "total_hsp_limit", m_Ptr->total_hsp_limit);
    DebugDumpValue(ddc, "hsp_range_max", m_Ptr->hsp_range_max);
    DebugDumpValue(ddc, "perform_culling", m_Ptr->perform_culling);
    DebugDumpValue(ddc, "required_start", m_Ptr->required_start);
    DebugDumpValue(ddc, "required_end", m_Ptr->required_end);
    DebugDumpValue(ddc, "expect_value", m_Ptr->expect_value);
    DebugDumpValue(ddc, "original_expect_value", m_Ptr->original_expect_value);
    DebugDumpValue(ddc, "single_hsp_evalue", m_Ptr->single_hsp_evalue);
    DebugDumpValue(ddc, "cutoff_score", m_Ptr->cutoff_score);
    DebugDumpValue(ddc, "single_hsp_score", m_Ptr->single_hsp_score);
    DebugDumpValue(ddc, "percent_identity", m_Ptr->percent_identity);
    DebugDumpValue(ddc, "do_sum_stats", m_Ptr->do_sum_stats);
    DebugDumpValue(ddc, "longest_intron", m_Ptr->longest_intron);
    DebugDumpValue(ddc, "is_neighboring", m_Ptr->is_neighboring);
    DebugDumpValue(ddc, "is_gapped", m_Ptr->is_gapped);
}
void
CBlastHitSavingParametersPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastHitSavingParametersPtr");
    if (!m_Ptr)
        return;

}
void
CPSIBlastOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CPSIBlastOptionsPtr");
    if (!m_Ptr)
        return;

}
void
CBlastGapAlignStructPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastGapAlignStructPtr");
    if (!m_Ptr)
        return;

}

void
CBlastEffectiveLengthsOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastEffectiveLengthsOptionsPtr");
    if (!m_Ptr)
        return;

    DebugDumpValue(ddc, "db_length", m_Ptr->db_length);
    DebugDumpValue(ddc, "dbseq_num", m_Ptr->dbseq_num);
    DebugDumpValue(ddc, "searchsp_eff", m_Ptr->searchsp_eff);
    DebugDumpValue(ddc, "use_real_db_size", m_Ptr->use_real_db_size);
}

void
CBlastScoringOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastScoringOptionsPtr");
    if (!m_Ptr)
        return;

    if (m_Ptr->matrix)
        DebugDumpValue(ddc, "matrix", m_Ptr->matrix);
    DebugDumpValue(ddc, "reward", m_Ptr->reward);
    DebugDumpValue(ddc, "penalty", m_Ptr->penalty);
    DebugDumpValue(ddc, "gapped_calculation", m_Ptr->gapped_calculation);
    DebugDumpValue(ddc, "gap_open", m_Ptr->gap_open);
    DebugDumpValue(ddc, "gap_extend", m_Ptr->gap_extend);
    DebugDumpValue(ddc, "shift_pen", m_Ptr->shift_pen);
    DebugDumpValue(ddc, "decline_align", m_Ptr->decline_align);
    DebugDumpValue(ddc, "is_ooframe", m_Ptr->is_ooframe);
}

void
CBlastDatabaseOptionsPtr::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastDatabaseOptionsPtr");
    if (!m_Ptr)
        return;

}

BlastMaskPtr
x_CSeqLoc2BlastMask(const CSeq_loc& sl, int index)
{
    _ASSERT(sl.IsInt() || sl.IsPacked_int());

    BlastSeqLocPtr bsl = NULL, curr = NULL, tail = NULL;
    BlastMaskPtr mask = NULL;

    switch (sl.Which()) {

    case CSeq_loc::e_Packed_int:
        ITERATE(list< CRef<CSeq_interval> >, itr, sl.GetPacked_int().Get()) {
            curr = BlastSeqLocNew((*itr)->GetFrom(), (*itr)->GetTo());
            if (!bsl) {
                bsl = tail = curr;
            } else {
                tail->next = curr;
                tail = tail->next;
            }
        }
        break;

    case CSeq_loc::e_Int:
        bsl = BlastSeqLocNew(sl.GetInt().GetFrom(), sl.GetInt().GetTo());
        break;

    default:
        break;
    }

    mask = (BlastMaskPtr) calloc(1, sizeof(BlastMask));
    mask->index = index;
    mask->loc_list = (ValNodePtr) bsl;

    return mask;
}

//TODO
CRef<CSeq_loc>
BLASTBlastMask2SeqLoc(BlastMaskPtr mask)
{
    CRef<CSeq_loc> retval;

    if (!mask)
        return retval;



    return retval;
}
END_NCBI_SCOPE
