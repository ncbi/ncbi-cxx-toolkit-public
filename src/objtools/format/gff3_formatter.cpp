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
* Author:  Aaron Ucko
*
* File Description:
*   Flat formatter for Generic Feature Format version 3.
*   (See http://song.sourceforge.net/gff3-jan04.shtml .)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/format/gff3_formatter.hpp>
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/cigar_formatter.hpp>

#include <serial/iterator.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Score.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_GFF

//#define GFF3_USE_CIGAR_FORMATTER 1


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


static const string& s_GetMatchType(
        const CSeq_id& ref_id, const CSeq_id& tgt_id,
        bool flybase)
{
    static const string kMatch     = "match";  // generic match
    static const string kEST       = "EST_match";
    static const string kcDNA      = "cDNA_match";
    static const string kProt      = "protein_match";
    static const string kTransNuc  = "translated_nucleotide_match";
    static const string kNucToProt = "nucleotide_to_protein_match";
    
    CSeq_id::EAccessionInfo ref_info = ref_id.IdentifyAccession();
    CSeq_id::EAccessionInfo tgt_info = tgt_id.IdentifyAccession();
    if (flybase) {
        if ((ref_info & CSeq_id::fAcc_prot)  ||  (tgt_info & CSeq_id::fAcc_prot)) {
            return kNucToProt; // NOT a valid SOFA term!!!
        } else if (((ref_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) ||
                   ((tgt_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est)) {
            return kEST;
        }
        // HACK HACK HACK
        // we should provide a check for cDNA and retuen kMatch as the default.
        return kcDNA;
    }
    // Rules according to GFF3 specifications using a more strict
    // interpretation. If we can't reliably tell the kind of match,
    // then don't add possibly incorrect levels of detail.
    // Note how none of these categorizations currently in SOFA
    // state anything about the reference side of the alignment!
    if ( tgt_info & CSeq_id::fAcc_prot ) {
        return kProt; // "A match against a protein sequence."
    }
    if ( (tgt_info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_est) {
        return kEST; // "A match against an EST sequence."
    }
    if ( ref_info & CSeq_id::fAcc_prot  &&  ! (tgt_info & CSeq_id::fAcc_prot) ) {
        return kTransNuc; // "A match against a translated sequence."
    }
    // Should check for more refined categorization.
    return kMatch;
}


class CGFF3_CIGAR_Formatter : public CCIGAR_Formatter
{
public:
    CGFF3_CIGAR_Formatter(CGFF3_Formatter& gff3,
                          const CAlignmentItem& aln,
                          IFlatTextOStream&     text_os);

protected:
    virtual void EndRows(void);
    virtual void EndSubAlignment(void);
    virtual void StartRow(void);
    virtual void AddRow(const string& cigar);
    virtual void EndRow(void);
    virtual void AddSegment(CNcbiOstream& cigar,
                            char seg_type,
                            TSeqPos seg_len);

private:
    const CAlignmentItem&       m_Alignment;
    IFlatTextOStream&           m_Out;
    CGFF3_Formatter&            m_GFF3_Fmt;
    auto_ptr<CNcbiOstrstream>   m_Attrs;
    list<string>                m_Lines;
};


CGFF3_CIGAR_Formatter::CGFF3_CIGAR_Formatter(CGFF3_Formatter&       gff3,
                                             const CAlignmentItem&  aln,
                                             IFlatTextOStream&      text_os)
    : CCIGAR_Formatter(aln.GetAlign(),
                       &aln.GetContext()->GetScope()),
      m_Alignment(aln),
      m_Out(text_os),
      m_GFF3_Fmt(gff3),
      m_Attrs(new CNcbiOstrstream)
{
}


void CGFF3_CIGAR_Formatter::EndSubAlignment(void)
{
    if (GetSeq_align().GetSegs().IsDisc() &&
        m_Alignment.GetContext()->Config().GffGenerateIdTags()) {
        ++m_GFF3_Fmt.m_CurrentId; // ?????
    }
}


void CGFF3_CIGAR_Formatter::StartRow(void)
{
    const CFlatFileConfig& config = m_Alignment.GetContext()->Config();

    // We can't use x_FormatAttr because we seem to need a mix
    // of literal pluses, which we otherwise avoid due to ambiguity,
    // as well as two kinds of escapes for spaces, one with pluses,
    // and one with %09. Really. Read the GFF3 specs. :-/
    if ( config.GffGenerateIdTags() ) {
        *m_Attrs << "ID=" << m_GFF3_Fmt.m_CurrentId << ";";
    }
    *m_Attrs << "Target=";
    // GFF3 specs require %09 escape for spaces in the Target,
    // not + or any other!
    CGFF3_Formatter::x_AppendEncoded(*m_Attrs,
        GetTargetId().GetSeqIdString(true), "%09");
    // We are allowed spaces here, so we'll make use of them.
    // It's more pleasing to the eye.
    *m_Attrs << ' ' << (GetTargetRange().GetFrom() + 1) << ' '
        << (GetTargetRange().GetTo() + 1);

    ///
    /// HACK HACK HACK
    /// optional strand on the end
    if (GetTargetSign() == 1) {
        // By prior versions of GFF3 specs (current is 1.14),
        // + had special meaning (as a space), wheras - didn't.
        // That made + ambiguous. However, even if interpreted as
        // a space, the strand default to positive, so it's not
        // a problem.
        //
        // DETAILS:
        // In older versions of the specs, they discussed URL encoding,
        // specifically mentionned + as space. In subsequent versions,
        // + was explicitly listed amongst the allowable characters
        // for the Seqid column. I believe the issue arised from
        // confusion between URL encoding (which only does % escaping)
        // vs application/x-www-form-urlencoded which is similar,
        // but adds things like + to represent spaces. 
        *m_Attrs << " +";
    } else {
        // A minus is unambiguous. So, the only question is,
        // do we escape the space? Hmmm... "+-" looks strange (and
        // likely wrong, given discussion above about confusion with
        // URL Encoding in GFF3 specs), and things like "%09%2D"
        // are totally ugly.
        *m_Attrs << " -";
    }
}


void CGFF3_CIGAR_Formatter::AddRow(const string& cigar)
{
    if ( !IsTrivial()  ||  GetLastType() != 'M' ) {
        *m_Attrs << ";Gap=" << cigar;
    }
}


void CGFF3_CIGAR_Formatter::EndRow(void)
{
    CBioseqContext& ctx = *m_Alignment.GetContext();
    // XXX - should supply appropriate score, if any
    CSeq_loc loc(*ctx.GetPrimaryId(),
        GetRefRange().GetFrom(), GetRefRange().GetTo(),
        (GetRefSign() == 1 ? eNa_strand_plus
        : eNa_strand_minus));

    // HACK HACK HACK
    // add score attributes
    const CSeq_align& seq_align = GetSeq_align();
    if (IsFirstSubalign()  &&  seq_align.IsSetScore()) {
        ITERATE (CDense_seg::TScores, score_it, seq_align.GetScore()) {
            const CScore& score = **score_it;
            if (score.IsSetId() && score.GetId().IsStr() && score.IsSetValue()) {
                *m_Attrs << ';';
                // Not one of the special cases of escaping, so space ok.
                CGFF3_Formatter::x_AppendEncoded(
                    *m_Attrs, score.GetId().GetStr(), " ");
                *m_Attrs << '=';
                if (score.GetValue().IsInt()) {
                    *m_Attrs << score.GetValue().GetInt();
                } else {
                    *m_Attrs << score.GetValue().GetReal();
                }
            }
        }
    }

    // HACK HACK HACK
    // add score attributes
    const CDense_seg& ds = GetDense_seg();
    if (ds.IsSetScores()) {
        ITERATE (CDense_seg::TScores, score_it, ds.GetScores()) {
            const CScore& score = **score_it;
            if (score.IsSetId() && score.GetId().IsStr() && score.IsSetValue()) {
                *m_Attrs << ';';
                // Not one of the special cases of escaping, so space ok.
                CGFF3_Formatter::x_AppendEncoded(
                    *m_Attrs, score.GetId().GetStr(), " ");
                *m_Attrs << '=';
                if (score.GetValue().IsInt()) {
                    *m_Attrs << score.GetValue().GetInt();
                } else {
                    *m_Attrs << score.GetValue().GetReal();
                }
            }
        }
    }

    string attr_string = CNcbiOstrstreamToString(*m_Attrs);
    m_Attrs.reset(new CNcbiOstrstream);

    // Phase has a different interpretation in GFF3 for Flybase.
    // Seriously. Adjust the phase for display, as appropriate.
    // Note that the API expects frame, which is not the same as
    // the phase, and it also wants that frame to be 0-based, with
    // values 0, 1, 2, or -1 for undefined, which is not the same
    // as the frame in ASN.1. Confused? Convert as appropriate.
    string source = m_GFF3_Fmt.x_GetSourceName(ctx);
    const CFlatFileConfig& config = ctx.Config();
    int frame = GetFrame();
    m_GFF3_Fmt.x_AddFeature(m_Lines, loc, source,
        s_GetMatchType(GetRefId(), GetTargetId(), config.GffForFlybase()),
        "." /*score*/,
        config.GffForFlybase() ?
        /* frame vs phase inverted for flybase! */
        (frame > 0 ? 3 - frame : frame) 
        /* frame for everybody else... undefined! */
        : -1,
        attr_string, false /*gtf*/, ctx);
}


void CGFF3_CIGAR_Formatter::EndRows(void)
{
    m_Out.AddParagraph(m_Lines, &GetDense_seg());
    m_Lines.clear();
}


void CGFF3_CIGAR_Formatter::AddSegment(CNcbiOstream& cigar,
                                       char seg_type,
                                       TSeqPos seg_len)
{
    // In GFF3 type and length are swapped and '+' is used between segments
    if ( cigar.tellp() > 0) {
        cigar << '+';
    }
    cigar << seg_type << seg_len;
}


void CGFF3_Formatter::Start(IFlatTextOStream& text_os)
{
    list<string> l;
    // The GFF version is rquired to be 3, with no minor revisions.
    // This is unfortunate, since there are multiple revisions of the
    // specifications (up to GFF3 specs version 1.14 as of 6/5/2009),
    // and some of them hav minor differences of significance,
    // such as refinement of allowable character code vs escape.
    //
    // Also, there are at least 3 flavours of GFF3, discounting multiple
    // versions of each:
    // - "Official specifications" by the Sequence Ontology group (SO),
    //   see http://www.sequenceontology.org/gff3.shtml
    // - GFF3 as modified by the Interoperability Working Group (IOWG),
    //   see http://www.pathogenportal.org/gff3-usage-conventions.html
    // - GFF3 as modified for exchange with Flybase. This alters
    //   such critical information as how phase is represented.
    //
    l.push_back("##gff-version 3");

    // Add unofficial "metadata". According to GFF3 specifications (v1.15),
    // lines preceeded by ## are metadata or directives. However, according
    // to the de-facto standard of BioPerl's implementation, there are
    // only directives, no metadata, with the directives using a controlled
    // vocabulary, and it is an error to use an unsupported directive.
    //
    // Thus, we are forced to use comment lines instead (# followed by
    // any character other than #). To distinguish true comments from
    // metadata we wish to emit, we are free to adopt our own conventions.
    // That is, comments do not have any enforced structure according to
    // GFF3 specifications. Arbitrarily, we adopt #! to indicate such
    // metadata-as-comments.
    //
    // Also, not that a comment may NOT be empty, according to BioPerl.
    // (Regexp used is /^#[^#]/ which requires 2 characters.)
    //
    // @see _handle_directive and next_feature in
    //  svn://code.open-bio.org/bioperl/bioperl-live/trunk/Bio/FeatureIO/gff.pm

    l.push_back("#!gff-spec-version 1.14"); // A comment, not a directive.
    if ( GetContext().GetConfig().GffForFlybase() ) {
        l.push_back("#!gff-variant flybase"); // A comment, not a directive.
        l.push_back("# This variant of GFF3 interprets ambiguities in the");
        l.push_back("# GFF3 specifications in accordance with the views of Flybase.");
        l.push_back("# This impacts the feature tag set, and meaning of the phase.");
    }
    // All comments, not a directives.
    l.push_back("#!source-version NCBI C++ formatter 0.2");
//    l.push_back("#!date " + CurrentTime().AsString("Y-M-D"));
    text_os.AddParagraph(l);
}

void CGFF3_Formatter::EndSection(const CEndSectionItem&,
                                 IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("###");
    text_os.AddParagraph(l);
}


void CGFF3_Formatter::FormatAlignment(const CAlignmentItem& aln,
                                      IFlatTextOStream& text_os)
{
#ifdef GFF3_USE_CIGAR_FORMATTER
    CGFF3_CIGAR_Formatter cigar(*this, aln, text_os);
    cigar.FormatByReferenceId(*aln.GetContext()->GetPrimaryId());
#else
    x_FormatAlignment(aln, text_os, aln.GetAlign(), true, false);
#endif
}

void CGFF3_Formatter::x_FormatAlignment(const CAlignmentItem& aln,
                                        IFlatTextOStream& text_os,
                                        const CSeq_align& sa,
                                        bool first,
                                        bool width_inverted)
{
    const CFlatFileConfig& config = aln.GetContext()->Config();

    switch (sa.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        x_FormatDenseg(aln, text_os, sa.GetSegs().GetDenseg(),
            first, width_inverted);
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
        } STD_CATCH_ALL_X(4, "CGFF3_Formatter::x_FormatAlignment")
        if (sa2) {
            // HACK HACK HACK WORKAROUND
            // Conversion from Spliced to Disc inverts meaning of width!!!
            x_FormatAlignment(aln, text_os, *sa2, first, true);
        }
        break;
    }

    case CSeq_align::TSegs::e_Std:
    {
        CRef<CSeq_align> sa2;
        try {
             sa2 = sa.CreateDensegFromStdseg();
        } STD_CATCH_ALL_X(4, "CGFF3_Formatter::x_FormatAlignment")
        if (sa2.NotEmpty()  &&  sa2->GetSegs().IsDenseg()) {
            x_FormatDenseg(aln, text_os, sa2->GetSegs().GetDenseg(),
                           first, width_inverted);
        }
        break;
    }

    case CSeq_align::TSegs::e_Disc:
    {
         ITERATE (CSeq_align_set::Tdata, it, sa.GetSegs().GetDisc().Get()) {
             x_FormatAlignment(aln, text_os, **it,
                               first, width_inverted);
             first = false;
         }
        break;
    }

    default: // dendiag or packed; unsupported
        NCBI_THROW(CFlatException, eNotSupported,
                   "Conversion of alignments of type dendiag and packed "
                   "not supported in current GFF3 CIGAR output");
    }
    if ( config.GffGenerateIdTags() ) {
        ++m_CurrentId;
    }
}


static CConstRef<CSeq_id> s_GetTargetId(const CSeq_id& id, CScope& scope)
{
    try {
        return sequence::GetId(id, scope, sequence::eGetId_ForceAcc).GetSeqId();
    }
    catch (CException&) {
    }
    return CConstRef<CSeq_id>(&id);
}


void CGFF3_Formatter::x_FormatDenseg(const CAlignmentItem& aln,
                                     IFlatTextOStream& text_os,
                                     const CDense_seg& ds,
                                     bool first,
                                     bool width_inverted)
{
    // cerr << "DENSEG:\n" << MSerial_AsnText << ds << endl;

    // Frame, as a value of 0, 1, 2, or -1 for undefined.
    // This is NOT the same frame as the frame in ASN.1!
    int frame(-1);

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

    CRef<CDense_seg> ds_filled = ds.FillUnaligned(); //keep ds_for_alnmix
                                                   // alive through scope!
    const CDense_seg* ds_for_alnmix = ds_filled.GetNCPointerOrNull();
    if ( 0 == ds_for_alnmix ) {
        ds_for_alnmix = &ds;
    }
    CDense_seg ds_no_widths;
    if (width_inverted) {
        ds_no_widths.Assign(*ds_for_alnmix);
        ds_no_widths.ResetWidths();
        ds_for_alnmix = &ds_no_widths;
    }

    typedef CAlnMap::TNumrow      TNumrow;
    typedef CAlnMap::TNumchunk    TNumchunk;
    typedef CAlnMap::TSignedRange TRange;

    CBioseqContext* ctx     = aln.GetContext();
    list<string>    l;
    string          source  = x_GetSourceName(*ctx);
    CAlnMap         alnmap(*ds_for_alnmix);
    TNumrow         ref_row = -1;
    CScope&         scope = ctx->GetScope();
    const CFlatFileConfig& config = ctx->Config();

    const CSeq_id& ref_id = *ctx->GetPrimaryId();
    for (TNumrow row = 0;  row < alnmap.GetNumRows();  ++row) {
        if (sequence::IsSameBioseq(alnmap.GetSeqId(row), ref_id, &scope)) {
            ref_row = row;
            break;
        }
    }
    if (ref_row < 0) {
        ERR_POST_X(3, "CGFF3_Formatter::FormatAlignment: "
                      "no row with a matching ID found!");
        return;
    }

    TSeqPos ref_width =
            (static_cast<size_t>(ref_row) < ds.GetWidths().size()) ?
                        ds.GetWidths()[ref_row] : 1;

    TSeqPos ref_start(0);
    int     ref_sign  = alnmap.StrandSign(ref_row);
    for (TNumrow tgt_row = 0;  tgt_row < alnmap.GetNumRows();  ++tgt_row) {
        if (tgt_row == ref_row) {
            continue;
        }
        CNcbiOstrstream cigar;
        TSeqPos         tgt_width =
                (static_cast<size_t>(tgt_row) < ds.GetWidths().size()) ?
                                    ds.GetWidths()[tgt_row] : 1;
        int             tgt_sign = alnmap.StrandSign(tgt_row);
        TRange          ref_range, tgt_range;
        bool            trivial = true;
        
        if (! width_inverted  &&  (ref_width != 1  ||  tgt_width != 1)) {
            // Supporting widths ONLY in the unamiguous case when we
            // know they are WRONG and put there incorrectly from conversion
            // from Spliced-seg. If we didn't get widths that way, we don't
            // know what they mean, so punt if not all widths are 1.
            NCBI_THROW(CFlatException, eNotSupported,
                       "Widths in alignments do not have clear semantics, "
                       "and thus are not supported in current GFF3 CIGAR output");
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
        TSeqPos         width = max(ref_width, tgt_width);

        for (TNumchunk i0 = 0;  i0 < alnmap.GetNumSegs();  ++i0) {
            TRange                        ref_piece = alnmap.GetRange(ref_row, i0);
            TRange                        tgt_piece = alnmap.GetRange(tgt_row, i0);
            CAlnMap::TSegTypeFlags        ref_flags = alnmap.GetSegType(ref_row, i0);
            CAlnMap::TSegTypeFlags        tgt_flags = alnmap.GetSegType(tgt_row, i0);

            //The type and count are guaranteed set by one of the if/else cases below.  
            char                          type = 'X'; // Guaranteed set. Pacify compiler.
            TSeqPos                       count = 0;  // Guaranteed set. Pacify compiler.
            TSignedSeqPos                 frameshift = 0;

            // cerr << "TARGET PIECE " << tgt_piece.GetFrom()
            //      << " to " << tgt_piece.GetTo() << endl;
            // cerr << "REF    PIECE " << ref_piece.GetFrom() << " to " << ref_piece.GetTo()
            //       << " + " << ref_start << "\n" << endl;

            if (  (tgt_flags & CAlnMap::fSeq)  &&
                ! (ref_flags & CAlnMap::fSeq)) {
            // See MakeGapString() in:
                // /panfs/pan1/gpipe07/ThirdParty/ProSplignForFlyBase/production/prosplign2gff3
                //
                //         elif starts[2 * i] == -1:
                //            # gap in prot
                //            if (starts[2 * (i - 1)] + lens[i - 1]) % 3 and i != 0:
                //                raise 'non-initial prot gap does not start on aa boundary'
                //            if seg_len / 3:
                //                l.append('I%d' % (seg_len / 3))
                //            if seg_len % 3:
                //                l.append('R%d' % (seg_len % 3))
                //
                // TODO: Handle non-initial protein gap that does not start
                //       on an aa boundary.
                //
                type       = 'I';
                if (i0 == 0  &&  config.GffForFlybase()  &&  tgt_width == 3) {
                    // See comments about frame and phase, below.
                    frame = (tgt_piece.GetFrom()            ) % tgt_width;
                }
                count      = tgt_piece.GetLength() / width;
                frameshift = -(TSignedSeqPos)(tgt_piece.GetLength() % width);
                tgt_piece.SetFrom(tgt_piece.GetFrom() / tgt_width);
                tgt_piece.SetTo  (tgt_piece.GetTo()   / tgt_width);
                tgt_range += tgt_piece;

            } else if (! (tgt_flags & CAlnMap::fSeq)  &&
                         (ref_flags & CAlnMap::fSeq)) {

                // See MakeGapString() in:
                // /panfs/pan1/gpipe07/ThirdParty/ProSplignForFlyBase/production/prosplign2gff3
                //
                //        else:
                //            # gap in nuc
                //            if starts[2 * i] % 3:
                //                raise 'nuc gap does not start on aa boundary'
                //            if seg_len / 3:
                //                l.append('D%d' % (seg_len / 3))
                //            if seg_len % 3:
                //                l.append('F%d' % (seg_len % 3))
                //
                // TODO: Handle gap that does not start on an aa boundary.
                //
                type       = 'D';
                if (i0 == 0  &&  config.GffForFlybase()  &&  ref_width == 3) {
                    // See comments about frame and phase, below.
                    frame = (ref_piece.GetFrom()            ) % ref_width;
                }
                count      = ref_piece.GetLength() / width;
                frameshift = +(ref_piece.GetLength() % width);
                // Adjusting for start position, converting to natural cordinates
                // (aa for protein locations, which would imply divide by 3).
                ref_piece.SetFrom((ref_piece.GetFrom() + ref_start) / ref_width);
                ref_piece.SetTo  ((ref_piece.GetTo()   + ref_start) / ref_width);
                ref_range += ref_piece;
            } else if (  (tgt_flags & CAlnMap::fSeq)  &&
                         (ref_flags & CAlnMap::fSeq)) {
                // Hanlde case when sequences aligned.
                // The remaining case is when both don't align at all,
                // which shouldn't happen in a pairwise alignment. If we
                // happen to have a multiple alignment, the remaining case
                // would be one that aligns unrelated sequences, thus has
                // no affect on the current GFF3 output.

                // See MakeGapString() in:
                // /panfs/pan1/gpipe07/ThirdParty/ProSplignForFlyBase/production/prosplign2gff3
                //
                //        if starts[2 * i] != -1 and starts[2 * i + 1] != -1:
                //            # non-gap
                //
                //            # for internal segs, length is easy
                //            if numseg != 1 and i != numseg - 1:
                //                # One end should be a codon boundary
                //                # Check this
                //                if starts[2 * i] % 3 != 0 and (starts[2 * i] + seg_len) % 3 != 0:
                //                    raise 'a bad thing happened; i = %d' % i
                //                length = (seg_len + 2) / 3
                //            else:
                //                # single segment or last segment
                //                length = (starts[2 * i] % 3 + seg_len + 2) / 3
                //            l.append('M%d' % length)
                //
                // TODO: Resolve why the following implementation is different
                //       from the above historic implementation. The difference
                //       will be in rounding down vs up on single or last
                //       segment.
                //
                type       = 'M';
                if (ref_piece.GetLength()  !=  tgt_piece.GetLength()) {
                    // There's a frameshift.. somewhere. Is this valid? Bail.
                    NCBI_THROW(CFlatException, eNotSupported,
                               "Frameshift(s) in Spliced-exon-chunk's diag "
                               "not supported in current GFF3 CIGAR output");
                }
                if (i0 == 0  &&  config.GffForFlybase()) {
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
                    if (ref_width == 3) {
                        frame = (ref_piece.GetFrom() + ref_start) % ref_width;
                    } else if (tgt_width == 3) {
                        frame = (tgt_piece.GetFrom()            ) % tgt_width;
                    }
                }
                // Adjusting for start position, converting to natural cordinates
                // (aa for protein locations, which would imply divide by 3).
                count      = ref_piece.GetLength() / width;
                ref_piece.SetFrom((ref_piece.GetFrom() + ref_start) / ref_width);
                ref_piece.SetTo  ((ref_piece.GetTo()   + ref_start) / ref_width);
                ref_range += ref_piece;
                tgt_piece.SetFrom(tgt_piece.GetFrom() / tgt_width);
                tgt_piece.SetTo  (tgt_piece.GetTo()   / tgt_width);
                tgt_range += tgt_piece;
            }
            if (count) {
                if ( 0 != cigar.pcount() ) {
                   cigar << '+';
                }
                cigar << type << count;
                trivial = false;
            }
            if (frameshift) {
                trivial = false;
                cigar << (frameshift < 0 ? 'F' : 'R') << abs(frameshift) << '+';
            }
        }
        // We can't use x_FormatAttr because we seem to need a mix
        // of literal pluses, which we otherwise avoid due to ambiguity,
        // as well as two kinds of escapes for spaces, one with pluses,
        // and one with %09. Really. Read the GFF3 specs. :-/
        CNcbiOstrstream attrs;
        CConstRef<CSeq_id> tgt_id =
            s_GetTargetId(alnmap.GetSeqId(tgt_row), scope);
        if ( config.GffGenerateIdTags() ) {
            attrs << "ID=" << m_CurrentId++ << ";";
        }
        attrs << "Target=";
        // GFF3 specs require %09 escape for spaces in the Target,
        // not + or any other!
        x_AppendEncoded(attrs, tgt_id->GetSeqIdString(true), "%09");
        // We are allowed spaces here, so we'll make use of them.
        // It's more pleasing to the eye.
        attrs << ' ' << (tgt_range.GetFrom() + 1) << ' '
              << (tgt_range.GetTo() + 1);

        ///
        /// HACK HACK HACK
        /// optional strand on the end
        if (tgt_sign == 1) {
            // By prior versions of GFF3 specs (current is 1.14),
            // + had special meaning (as a space), wheras - didn't.
            // That made + ambiguous. However, even if interpreted as
            // a space, the strand default to positive, so it's not
            // a problem.
            //
            // DETAILS:
            // In older versions of the specs, they discussed URL encoding,
            // specifically mentionned + as space. In subsequent versions,
            // + was explicitly listed amongst the allowable characters
            // for the Seqid column. I believe the issue arised from
            // confusion between URL encoding (which only does % escaping)
            // vs application/x-www-form-urlencoded which is similar,
            // but adds things like + to represent spaces. 
            attrs << " +";
        } else {
            // A minus is unambiguous. So, the only question is,
            // do we escape the space? Hmmm... "+-" looks strange (and
            // likely wrong, given discussion above about confusion with
            // URL Encoding in GFF3 specs), and things like "%09%2D"
            // are totally ugly.
            attrs << " -";
        }

        if ( !trivial ) {
            string cigar_string = CNcbiOstrstreamToString(cigar);
            attrs << ";Gap=" << cigar_string;
        }
        // XXX - should supply appropriate score, if any
        CSeq_loc loc(*ctx->GetPrimaryId(),
                     ref_range.GetFrom(), ref_range.GetTo(),
                     (ref_sign == 1 ? eNa_strand_plus
                      : eNa_strand_minus));

        // HACK HACK HACK
        // add score attributes
        if (first  &&  aln.GetAlign().IsSetScore()) {
            ITERATE (CDense_seg::TScores, score_it, aln.GetAlign().GetScore()) {
                const CScore& score = **score_it;
                if (score.IsSetId()  &&  score.GetId().IsStr()  &&  score.IsSetValue()) {
                    attrs << ';';
                    // Not one of the special cases of escaping, so space ok.
                    x_AppendEncoded(attrs, score.GetId().GetStr(), " ");
                    attrs << '=';
                    if (score.GetValue().IsInt()) {
                        attrs << score.GetValue().GetInt();
                    } else {
                        attrs << score.GetValue().GetReal();
                    }
                }
            }
        }

        // HACK HACK HACK
        // add score attributes
        string score_text(".");
        if (ds.IsSetScores()) {
            ITERATE (CDense_seg::TScores, score_it, ds.GetScores()) {
                const CScore& score = **score_it;
                if (score.IsSetId()  &&  score.GetId().IsStr()  &&  score.IsSetValue()) {
                    if (score.GetId().GetStr() == "score") {
                        // The generic 'score' score, if present,
                        // goes to the 6th column.
                        if (score.GetValue().IsInt()) {
                            score_text = NStr::IntToString(score.GetValue().GetInt());
                        } else {
                            score_text = NStr::DoubleToString(score.GetValue().GetReal());
                        }
                    } else {
                        attrs << ';';
                        // Not one of the special cases of escaping, so space ok.
                        x_AppendEncoded(attrs, score.GetId().GetStr(), " ");
                        attrs << '=';
                        if (score.GetValue().IsInt()) {
                            attrs << score.GetValue().GetInt();
                        } else {
                            attrs << score.GetValue().GetReal();
                        }
                    }
                }
            }
        }

        string attr_string = CNcbiOstrstreamToString(attrs);

        // Phase has a different interpretation in GFF3 for Flybase.
        // Seriously. Adjust the phase for display, as appropriate.
        // Note that the API expects frame, which is not the same as
        // the phase, and it also wants that frame to be 0-based, with
        // values 0, 1, 2, or -1 for undefined, which is not the same
        // as the frame in ASN.1. Confused? Convert as appropriate.
        x_AddFeature(l, loc, source,
                     s_GetMatchType(ref_id, *tgt_id, config.GffForFlybase()),
                     score_text,
                     config.GffForFlybase() ?
                        /* frame vs phase inverted for flybase! */
                        (frame > 0 ? 3 - frame : frame) 
                        /* frame for everybody else... undefined! */
                        : -1,
                     attr_string, false /*gtf*/, *ctx);
    }
    text_os.AddParagraph(l, &ds);
}


string CGFF3_Formatter::x_FormatAttr(const string& name, const string& value)
    const
{
    CNcbiOstrstream oss;
    oss << name << '=';
    // Not one of the special cases of escaping, so space ok.
    x_AppendEncoded(oss, value, " ");
    return CNcbiOstrstreamToString(oss);
}


void CGFF3_Formatter::x_AddGeneID(list<string>& attr_list,
                                  const string& gene_id,
                                  const string& transcript_id) const
{
    if (transcript_id.empty()) {
        attr_list.push_front(x_FormatAttr("ID", gene_id));
    } else {
        attr_list.push_front(x_FormatAttr("Parent", gene_id));
        attr_list.push_front(x_FormatAttr("ID", transcript_id));
    }
}


CNcbiOstream& CGFF3_Formatter::x_AppendEncoded(CNcbiOstream& os,
                                               const string& s,
                                               const char* space)
{
    // Encode space as %20 rather than +, whose status is ambiguous.
    // Officially, [a-zA-Z0-9.:^*$@!+_?-|] are okay, but we punt [*+?]
    // to be extra safe.
    static const char s_Table[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "%20", "!",   "%22", "%23", "$",   "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   ":",   "%3B", "%3C", "%3D", "%3E", "%3F",
        "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "^",   "_",
        "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
        "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
        "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
        "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
        "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
        "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
        "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
        "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
        "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
        "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
        "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
        "%F8", "%F9", "%FA", "%FB", "|", "%FD", "%FE", "%FF"
    };
    for (SIZE_TYPE i = 0;  i < s.size();  ++i) {
        if (s[i] == ' ') {
            os << space;
        } else {
            os << s_Table[static_cast<unsigned char>(s[i])];
        }
    }
    return os;
}


END_SCOPE(objects)
END_NCBI_SCOPE
