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
#include <objtools/format/gff3_formatter.hpp>
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/text_ostream.hpp>

#include <serial/iterator.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CGFF3_Formatter::Start(IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("##gff-version 3");
    l.push_back("##source-version NCBI C++ formatter 0.2");
    l.push_back("##date " + CurrentTime().AsString("Y-M-D"));
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
    x_FormatAlignment(aln, text_os, aln.GetAlign());
}

void CGFF3_Formatter::x_FormatAlignment(const CAlignmentItem& aln,
                                        IFlatTextOStream& text_os,
                                        const CSeq_align& sa)
{
    switch (sa.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        x_FormatDenseg(aln, text_os, sa.GetSegs().GetDenseg());
        break;

    case CSeq_align::TSegs::e_Std:
    {
        CRef<CSeq_align> sa2;
        try {
             sa2 = sa.CreateDensegFromStdseg();
        } STD_CATCH_ALL("CGFF3_Formatter::x_FormatAlignment")
        if (sa2.NotEmpty()  &&  sa2->GetSegs().IsDenseg()) {
            x_FormatDenseg(aln, text_os, sa2->GetSegs().GetDenseg());
        }
        break;
    }

    case CSeq_align::TSegs::e_Disc:
        ITERATE (CSeq_align_set::Tdata, it, sa.GetSegs().GetDisc().Get()) {
            x_FormatAlignment(aln, text_os, **it);
        }
        break;

    default: // dendiag or packed; unsupported
        break;
    }
}

void CGFF3_Formatter::x_FormatDenseg(const CAlignmentItem& aln,
                                     IFlatTextOStream& text_os,
                                     const CDense_seg& ds)
{
    typedef CAlnMap::TNumrow      TNumrow;
    typedef CAlnMap::TNumchunk    TNumchunk;
    typedef CAlnMap::TSignedRange TRange;

    CBioseqContext* ctx     = aln.GetContext();
    list<string>    l;
    string          source  = x_GetSourceName(*ctx);
    CAlnMap         alnmap(ds);
    TNumrow         ref_row = -1;
    for (TNumrow row = 0;  row < alnmap.GetNumRows();  ++row) {
        if (sequence::IsSameBioseq(alnmap.GetSeqId(row), *ctx->GetPrimaryId(),
                                   &ctx->GetScope())) {
            ref_row = row;
            break;
        }
    }
    if (ref_row < 0) {
        ERR_POST("CGFF3_Formatter::FormatAlignment: "
                 "no row with a matching ID found!");
        return;
    }
    alnmap.SetAnchor(ref_row);
    TSeqPos ref_width = alnmap.GetWidth(ref_row);
    TSeqPos ref_start = alnmap.GetSeqStart(ref_row);
    int     ref_sign  = alnmap.StrandSign(ref_row);
    for (TNumrow tgt_row = 0;  tgt_row < alnmap.GetNumRows();  ++tgt_row) {
        CNcbiOstrstream cigar;
        char            last_type = 0;
        TSeqPos         last_count = 0, tgt_width = alnmap.GetWidth(tgt_row);
        int             tgt_sign = alnmap.StrandSign(tgt_row);
        TRange          ref_range, tgt_range;
        bool            trivial = true;
        if (tgt_row == ref_row) {
            continue;
        }
        CRef<CAlnMap::CAlnChunkVec> chunks
            = alnmap.GetAlnChunks(tgt_row, alnmap.GetSeqAlnRange(tgt_row),
                                  CAlnMap::fAddUnalignedChunks);
        for (TNumchunk i0 = 0;  i0 < chunks->size();  ++i0) {
            TNumchunk i = (tgt_sign > 0) ? i0 : (chunks->size() - i0 - 1);
            CConstRef<CAlnMap::CAlnChunk> chunk     = (*chunks)[i];
            TRange                        ref_piece = chunk->GetAlnRange();
            TRange                        tgt_piece = chunk->GetRange();
            char                          type;
            TSeqPos                       count;
            CAlnMap::TSegTypeFlags        flags     = chunk->GetType();
            ref_piece.SetFrom(ref_piece.GetFrom() + ref_start);
            ref_piece.SetTo  (ref_piece.GetTo()   + ref_start);
            if (flags & CAlnMap::fNotAlignedToSeqOnAnchor) {
                _ASSERT(flags & CAlnMap::fSeq);
                type       = 'I';
                count      = tgt_piece.GetLength() / tgt_width;
                tgt_range += tgt_piece;
            } else if ( !(flags & CAlnMap::fSeq) ) {
                type       = 'D';
                count      = ref_piece.GetLength() / ref_width;
                ref_range += ref_piece;
            } else {
                type       = 'M';
                count      = ref_piece.GetLength() / ref_width;
                ref_range += ref_piece;
                tgt_range += tgt_piece;
            }
            if (type == last_type) {
                last_count += count;
            } else {
                if (last_type) {
                    cigar << last_type << last_count << '+';
                    trivial = false;
                }
                last_type  = type;
                last_count = count;
            }
        }
        // We can't use x_FormatAttr because we seem to need literal
        // pluses, which we otherwise avoid due to ambiguity. :-/
        CNcbiOstrstream attrs;
        attrs << "Target=";
        x_AppendEncoded(attrs,
                        ctx->GetPreferredSynonym(alnmap.GetSeqId(tgt_row))
                        .GetSeqIdString(true));
        attrs << '+' << (tgt_range.GetFrom() + 1) << '+'
              << (tgt_range.GetTo() + 1);
        if ( !trivial  ||  last_type != 'M' ) {
            cigar << last_type << last_count;
            string cigar_string = CNcbiOstrstreamToString(cigar);
            attrs << ";Gap=" << cigar_string;
        }
        // XXX - should supply appropriate score, if any
        CSeq_loc loc(*ctx->GetPrimaryId(), ref_range.GetFrom(),
                     ref_range.GetTo(),
                     (ref_sign == tgt_sign ? eNa_strand_plus
                      : eNa_strand_minus));
        string attr_string = CNcbiOstrstreamToString(attrs);
        x_AddFeature(l, loc, source, "match", "." /*score*/, -1 /*frame*/,
                     attr_string, false /*gtf*/, *ctx);
    }
    text_os.AddParagraph(l, &ds);
}


string CGFF3_Formatter::x_FormatAttr(const string& name, const string& value)
    const
{
    CNcbiOstrstream oss;
    oss << name << '=';
    x_AppendEncoded(oss, value);
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
                                               const string& s)
{
    // Encode space as %20 rather than +, whose status is ambiguous.
    // Officially, [a-zA-Z0-9.:^*$@!+_?-] are okay, but we punt [*+?]
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
        "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
    };
    for (SIZE_TYPE i = 0;  i < s.size();  ++i) {
        os << s_Table[static_cast<unsigned char>(s[i])];
    }
    return os;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2005/04/14 14:38:22  shomrat
* Changed CAlnMap flag
*
* Revision 1.2  2004/06/22 15:31:40  ucko
* Remove debugging output.
*
* Revision 1.1  2004/06/21 18:53:52  ucko
* New formatter for GFF version 3.
*
*
* ===========================================================================
*/
