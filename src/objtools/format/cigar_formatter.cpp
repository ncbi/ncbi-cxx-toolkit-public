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
* Authors:  Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Base class for CIGAR formatters.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/cigar_formatter.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_CIGAR

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CCIGAR_Formatter::CCIGAR_Formatter(const CSeq_align&    aln,
                                   CScope*              scope,
                                   TCIGARFlags          flags)
    : m_Align(aln),
      m_CurAlign(NULL),
      m_Scope(scope),
      m_Flags(flags),
      m_IsFirstSubalign(true),
      m_IsTrivial(true),
      m_LastType(0),
      m_Frame(-1),
      m_RefRow(-1),
      m_RefSign(1),
      m_TargetRow(-1),
      m_TargetSign(1),
      m_FormatBy(eFormatBy_NotSet)
{
}


CCIGAR_Formatter::~CCIGAR_Formatter(void)
{
}


void CCIGAR_Formatter::x_FormatAlignmentRows(void)
{
    StartAlignment();
    x_FormatAlignmentRows(GetSeq_align(), false);
    EndAlignment();
}


void CCIGAR_Formatter::x_FormatAlignmentRows(const CSeq_align& sa,
                                             bool              width_inverted)
{
    switch (sa.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        x_FormatDensegRows(sa.GetSegs().GetDenseg(), width_inverted);
        break;

    case CSeq_align::TSegs::e_Spliced:
        {
            CRef<CSeq_align> sa2;
            try {
                sa2 = sa.GetSegs().GetSpliced().AsDiscSeg();
                if (sa.IsSetScore()) {
                    sa2->SetScore().insert(sa2->SetScore().end(),
                        sa.GetScore().begin(),
                        sa.GetScore().end());
                }
            } STD_CATCH_ALL_X(1, "CCIGAR_Formatter::x_FormatAlignmentRows")
            if (sa2) {
                // HACK HACK HACK WORKAROUND
                // Conversion from Spliced to Disc inverts meaning of width!!!
                x_FormatAlignmentRows(*sa2, true);
            }
            break;
        }

    case CSeq_align::TSegs::e_Std:
        {
            CRef<CSeq_align> sa2;
            try {
                sa2 = sa.CreateDensegFromStdseg();
            } STD_CATCH_ALL_X(1, "CCIGAR_Formatter::x_FormatAlignmentRows")
            if (sa2.NotEmpty()  &&  sa2->GetSegs().IsDenseg()) {
                x_FormatDensegRows(sa2->GetSegs().GetDenseg(), width_inverted);
            }
            break;
        }

    case CSeq_align::TSegs::e_Disc:
        {
            ITERATE (CSeq_align_set::Tdata, it, sa.GetSegs().GetDisc().Get()) {
                m_CurAlign = (*it).GetPointer();
                StartSubAlignment();
                x_FormatAlignmentRows(**it, width_inverted);
                EndSubAlignment();
                m_CurAlign = NULL;
                m_IsFirstSubalign = false;
            }
            break;
        }

    default: // dendiag or packed; unsupported
        NCBI_THROW(CFlatException, eNotSupported,
            "Conversion of alignments of type dendiag and packed "
            "not supported in current CIGAR output");
    }
}


CCIGAR_Formatter::TNumrow CCIGAR_Formatter::x_GetRowById(const CSeq_id& id)
{
    CScope* scope = GetScope();
    for (TNumrow row = 0; row < m_AlnMap->GetNumRows(); ++row) {
        if (sequence::IsSameBioseq(m_AlnMap->GetSeqId(row), id, scope)) {
            return row;
        }
    }
    ERR_POST_X(1, "CCIGAR_Formatter::x_GetRowById: "
        "no row with a matching ID found: " << id.AsFastaString());
    return -1;
}


void CCIGAR_Formatter::x_FormatLine(bool width_inverted)
{
    if (m_TargetRow == m_RefRow) {
        return;
    }
    CNcbiOstrstream cigar;
    m_LastType = 0;
    TSeqPos last_count = 0;

    if ( !m_RefId ) {
        m_RefId.Reset(&m_AlnMap->GetSeqId(m_RefRow));
    }
    if ( !m_TargetId ) {
        m_TargetId.Reset(&m_AlnMap->GetSeqId(m_TargetRow));
    }

    m_RefWidth =
        (static_cast<size_t>(m_RefRow) < m_DenseSeg->GetWidths().size()) ?
        m_DenseSeg->GetWidths()[m_RefRow] : 1;
    m_RefSign = m_AlnMap->StrandSign(m_RefRow);
    m_TargetWidth =
        (static_cast<size_t>(m_TargetRow) < m_DenseSeg->GetWidths().size()) ?
        m_DenseSeg->GetWidths()[m_TargetRow] : 1;
    m_TargetSign = m_AlnMap->StrandSign(m_TargetRow);
    m_IsTrivial = true;
    TSignedSeqPos last_frameshift = 0;

    if (! width_inverted  &&  (m_RefWidth != 1  ||  m_TargetWidth != 1)) {
        // Supporting widths ONLY in the unamiguous case when we
        // know they are WRONG and put there incorrectly from conversion
        // from Spliced-seg. If we didn't get widths that way, we don't
        // know what they mean, so punt if not all widths are 1.
        NCBI_THROW(CFlatException, eNotSupported,
            "Widths in alignments do not have clear semantics, "
            "and thus are not supported in current CIGAR output");
    }

    // HACK HACK HACK
    // Is the following correct???
    //
    // Expecting all coordinates to be normalized relative to
    // some reference width, which might be the
    // Least Common Multiple of length in nucleotide bases of
    // the coordinate system used for each row, e.g. using
    // LCM of 3 if either row is protein. The Least Common Multiple
    // would allow accurately representing frameshifts in either
    // sequence.
    //
    // What does width for an alignment really mean, biologically?
    // It can't have arbitrary meaning, because CIGAR has fixed
    // semantics that M/I/D/F/R are in 3-bp units (i.e. one aa) for
    // proteins and 1-bp units for cDNA.
    //
    // Thus, in practice, I think we are expecting widths to be
    // one of (1, 1) for nuc-nuc, (1, 3) for nuc-prot,
    // (3, 1) for prot-nuc, and (3, 3) for prot-prot.
    TSeqPos width = max(m_RefWidth, m_TargetWidth);

    for (CAlnMap::TNumchunk i0 = 0; i0 < m_AlnMap->GetNumSegs(); ++i0) {
        TRange ref_piece = m_AlnMap->GetRange(m_RefRow, i0);
        TRange tgt_piece = m_AlnMap->GetRange(m_TargetRow, i0);
        CAlnMap::TSegTypeFlags ref_flags = m_AlnMap->GetSegType(m_RefRow, i0);
        CAlnMap::TSegTypeFlags tgt_flags = m_AlnMap->GetSegType(m_TargetRow, i0);
        //The type and count are guaranteed set by one of the if/else cases below.  
        char type = 'X'; // Guaranteed set. Pacify compiler.
        TSeqPos count = 0;  // Guaranteed set. Pacify compiler.
        TSignedSeqPos frameshift = 0;

        if ( (tgt_flags & CAlnMap::fSeq)  &&
            !(ref_flags & CAlnMap::fSeq) ) {
            // TODO: Handle non-initial protein gap that does not start
            //       on an aa boundary.
            //
            type = 'I';
            if (i0 == 0  &&  IsSetFlag(fCIGAR_GffForFlybase)  &&  m_TargetWidth == 3) {
                // See comments about frame and phase, below.
                m_Frame = tgt_piece.GetFrom() % m_TargetWidth;
            }
            count = tgt_piece.GetLength()/width;
            frameshift = -(tgt_piece.GetLength()%TSignedSeqPos(width));
            tgt_piece.SetFrom(tgt_piece.GetFrom()/m_TargetWidth);
            tgt_piece.SetTo(tgt_piece.GetTo()/m_TargetWidth);
            m_TargetRange += tgt_piece;
        }
        else if (! (tgt_flags & CAlnMap::fSeq)  &&
            (ref_flags & CAlnMap::fSeq)) {
            // TODO: Handle gap that does not start on an aa boundary.
            //
            type = 'D';
            if (i0 == 0  &&  IsSetFlag(fCIGAR_GffForFlybase)  &&  m_RefWidth == 3) {
                // See comments about frame and phase, below.
                m_Frame = ref_piece.GetFrom() % m_RefWidth;
            }
            count = ref_piece.GetLength()/width;
            frameshift = +(ref_piece.GetLength()%width);
            // Adjusting for start position, converting to natural cordinates
            // (aa for protein locations, which would imply divide by 3).
            ref_piece.SetFrom(ref_piece.GetFrom()/m_RefWidth);
            ref_piece.SetTo(ref_piece.GetTo()/m_RefWidth);
            m_RefRange += ref_piece;
        }
        else if ((tgt_flags & CAlnMap::fSeq)  &&
            (ref_flags & CAlnMap::fSeq)) {
            // Hanlde case when sequences aligned.
            // The remaining case is when both don't align at all,
            // which shouldn't happen in a pairwise alignment. If we
            // happen to have a multiple alignment, the remaining case
            // would be one that aligns unrelated sequences, thus has
            // no affect on the current GFF3 output.
            // TODO: Resolve why the following implementation is different
            //       from the above historic implementation. The difference
            //       will be in rounding down vs up on single or last
            //       segment.
            //
            type = 'M';
            if (ref_piece.GetLength()  !=  tgt_piece.GetLength()) {
                // There's a frameshift.. somewhere. Is this valid? Bail.
                NCBI_THROW(CFlatException, eNotSupported,
                    "Frameshift(s) in Spliced-exon-chunk's diag "
                    "not supported in current CIGAR output");
            }
            if (i0 == 0  &&  IsSetFlag(fCIGAR_GffForFlybase)) {
                // Semantics of the phase aren't defined in GFF3 for
                // feature types other than a CDS, and this is an alignment.
                //
                // Since phase is not required for alignment features, don't
                // emit one, unless we have been requested with the special
                // Flybase variant of GFF3 -- they did ask for phase.
                //
                // Also, phase can only be interpreted if we have an alignment
                // in terms of protein aa, and a width of 3 for one or
                // the other.
                //
                // For an alignment, the meaning of phase is ambiguous,
                // particularly in dealing with a protein-protein
                // alignment (if ever it allowed alignment to parts of
                // a codon), and when the seqid is the protein, rather
                // than the target.
                //
                // A protein won't be "reverse complemented" thus,
                // can assume that it's plus-strand and look at start
                // position.
                //
                // The computation below is actually for the frame.
                // The phase is not the same, and will be derived from
                // the frame.
                if (m_RefWidth == 3) {
                    m_Frame = ref_piece.GetFrom() % m_RefWidth;
                } else if (m_TargetWidth == 3) {
                    m_Frame = tgt_piece.GetFrom() % m_TargetWidth;
                }
            }
            // Adjusting for start position, converting to natural cordinates
            // (aa for protein locations, which would imply divide by 3).
            count = ref_piece.GetLength()/width;
            ref_piece.SetFrom(ref_piece.GetFrom()/m_RefWidth);
            ref_piece.SetTo(ref_piece.GetTo()/m_RefWidth);
            m_RefRange += ref_piece;
            tgt_piece.SetFrom(tgt_piece.GetFrom()/m_TargetWidth);
            tgt_piece.SetTo(tgt_piece.GetTo()/m_TargetWidth);
            m_TargetRange += tgt_piece;
        }
        if (type == m_LastType) {
            last_count += count;
            last_frameshift += frameshift;
        } else {
            if (m_LastType) {
                if (last_count) {
                    m_IsTrivial = false;
                    AddSegment(cigar, m_LastType, last_count);
                }
                if (last_frameshift) {
                    m_IsTrivial = false;
                    AddSegment(cigar,
                        (last_frameshift < 0 ? 'F' : 'R'),
                        abs(last_frameshift));
                }
            }
            m_LastType = type;
            last_count = count;
            last_frameshift = frameshift;
        }
    }
    CNcbiOstrstream aln_out;
    m_TargetId.Reset(&m_AlnMap->GetSeqId(m_TargetRow));
    if ( m_Scope ) {
        try {
            m_TargetId.Reset(sequence::GetId(
                *m_TargetId, *m_Scope, sequence::eGetId_ForceAcc).
                GetSeqId());
        }
        catch (CException&) {
        }
    }
    StartRow();

    AddSegment(cigar, m_LastType, last_count);
    string cigar_string = CNcbiOstrstreamToString(cigar);

    AddRow(cigar_string);

    EndRow();
}


void CCIGAR_Formatter::x_FormatDensegRows(const CDense_seg& ds,
                                          bool width_inverted)
{
    m_DenseSeg.Reset(&ds);

    // Frame, as a value of 0, 1, 2, or -1 for undefined.
    // This is NOT the same frame as the frame in ASN.1!
    m_Frame = -1;

    // HACK HACK HACK WORKAROUND
    // I do believe there is lack of agreement on what
    // the "widths" of a dense-seg mean -- as multiplier, or as divisor.
    //
    // In CSpliced_seg::s_ExonToDenseg, the widths act as divisors,
    // i.e. width 3 means every 3 units (na) in the alignment
    // correspond to 1 unit (aa) on the sequence.
    //
    // In CAlnMix as witnessed by e.g. x_ExtendDSWithWidths,
    // or even CDense_seg::Validate, the widths are a multiplier,
    // i.e. width 3 means every 1 unit (aa) in the alignment
    // corresponds to an alignment of 3 units (na) on the sequence.
    //
    // These definitions are incompatible.
    // The problem with the latter definition as a multiplier,
    // is that the smallest unit of alignment (in a protein-to-nucleotide)
    // is 1 aa = 3 bp... no opportunity for a frameshift. :-(
    //
    // To compensate (or rather, avoid double-compentating), avoid use
    // of widths, and copy to a temporary alignment, storing the old widths
    // for lookup, but reset them in the temporary alignment.
    const CDense_seg* ds_for_alnmix(&ds);
    CDense_seg ds_no_widths;
    if (width_inverted) {
        ds_no_widths.Assign(ds);
        ds_no_widths.ResetWidths();
        ds_for_alnmix = &ds_no_widths;
    }

    m_AlnMap.Reset(new CAlnMap(*ds_for_alnmix));
    CScope* scope = GetScope();

    switch ( m_FormatBy ) {
    case eFormatBy_ReferenceId:
        {
            bool by_id = m_RefId.NotNull();
            if ( by_id ) {
                m_RefRow = x_GetRowById(*m_RefId);
            }
            else {
                _ASSERT(m_RefRow >= 0);
                m_RefId.Reset(&m_AlnMap->GetSeqId(m_RefRow));
            }
            StartRows();
            for (m_TargetRow = 0; m_TargetRow < m_AlnMap->GetNumRows(); ++m_TargetRow) {
                x_FormatLine(width_inverted);
                m_TargetId.Reset();
            }
            m_TargetRow = -1;
            if ( by_id ) {
                m_RefRow = -1;
            }
            else {
                m_RefId.Reset();
            }
            break;
        }
    case eFormatBy_TargetId:
        {
            bool by_id = m_TargetId.NotNull();
            if ( by_id ) {
                m_TargetRow = x_GetRowById(*m_TargetId);
            }
            else {
                _ASSERT(m_TargetRow >= 0);
                m_TargetId.Reset(&m_AlnMap->GetSeqId(m_TargetRow));
            }
            StartRows();
            for (m_RefRow = 0; m_RefRow < m_AlnMap->GetNumRows(); ++m_RefRow) {
                x_FormatLine(width_inverted);
                m_RefId.Reset();
            }
            m_RefRow = -1;
            if ( by_id ) {
                m_TargetRow = -1;
            }
            else {
                m_TargetId.Reset();
            }
            break;
        }
    default:
        break;
    }

    EndRows();

    // Reset all values which have no sence anymore
    m_DenseSeg.Reset();
    m_AlnMap.Reset();
    m_IsTrivial = true;
    m_LastType = 0;
    m_Frame = -1;
    m_RefRange = TRange::GetEmpty();
    m_RefSign = 1;
    m_TargetRange = TRange::GetEmpty();
    m_TargetSign = 1;
}


void CCIGAR_Formatter::FormatByReferenceId(const CSeq_id& ref_id)
{
    m_FormatBy = eFormatBy_ReferenceId;
    m_RefId.Reset(&ref_id);
    m_TargetId.Reset();
    m_RefRow = -1;
    m_TargetRow = -1;
    x_FormatAlignmentRows();
}


void CCIGAR_Formatter::FormatByTargetId(const CSeq_id& target_id)
{
    m_FormatBy = eFormatBy_TargetId;
    m_RefId.Reset();
    m_TargetId.Reset(&target_id);
    m_RefRow = -1;
    m_TargetRow = -1;
    x_FormatAlignmentRows();
}


void CCIGAR_Formatter::FormatByReferenceRow(TNumrow ref_row)
{
    m_FormatBy = eFormatBy_ReferenceId;
    m_RefId.Reset();
    m_TargetId.Reset();
    m_RefRow = ref_row;
    m_TargetRow = -1;
    x_FormatAlignmentRows();
}


void CCIGAR_Formatter::FormatByTargetRow(TNumrow target_row)
{
    m_FormatBy = eFormatBy_TargetId;
    m_RefId.Reset();
    m_TargetId.Reset();
    m_RefRow = -1;
    m_TargetRow = target_row;
    x_FormatAlignmentRows();
}


void CCIGAR_Formatter::AddSegment(CNcbiOstream& cigar,
                                  char seg_type,
                                  TSeqPos seg_len)
{
    cigar << seg_len << seg_type;
}


END_SCOPE(objects)
END_NCBI_SCOPE
