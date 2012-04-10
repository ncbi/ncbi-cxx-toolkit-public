#ifndef OBJTOOLS_FORMAT___GFF3_FORMATTER__HPP
#define OBJTOOLS_FORMAT___GFF3_FORMATTER__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file gff3_formatter.hpp
/// Flat formatter for Generic Feature Format version 3.
///
/// See http://song.sourceforge.net/gff3-jan04.shtml .


#include <objtools/format/gff_formatter.hpp>
#include <objtools/format/items/ctrl_items.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;

//  ============================================================================
/// Flat file generator support for generating GFF3 output has been deprecated,
/// and therefore, class CGFF3_Formatter has been deprecated as well. DO NOT USE
/// this class for future projects as it is no longer maintained and produces
/// bad GFF.
/// For future projects (or to upgrade your existing project), use CGff3Writer 
/// (or an appropriate derivative) instead.
//  ============================================================================
NCBI_DEPRECATED_CLASS NCBI_FORMAT_EXPORT CGFF3_Formatter : public CGFFFormatter
{
public:
    NCBI_DEPRECATED_CTOR(
    CGFF3_Formatter()
    {
        m_CurrentId = 1;
    })

    void Start       (IFlatTextOStream& text_os);
    void EndSection  (const CEndSectionItem& esec, IFlatTextOStream& text_os);
    void FormatAlignment(const CAlignmentItem& aln, IFlatTextOStream& text_os);
protected:
    string x_GetAttrSep(void) const { return ";"; }
    string x_FormatAttr(const string& name, const string& value) const;
    void   x_AddGeneID(list<string>& attr_list, const string& gene_id,
                       const string& transcript_id) const;

private:
    unsigned int m_CurrentId;

    /// Encodes according to GFF3's URL-like encoding rules.
    ///
    /// There are some 4 different subtly different encoding rules in GFF3:
    ///
    /// 1) General rules, which allow all characters except:
    ///       tab newline carriage-return control  ; = % & , . \ "
    ///
    ///    Note that the above is specified in 3 parts, confusingly.
    ///    "The following characters must be escaped" in one paragraph,
    ///    "The following characters have reserved meanings and
    ///    must be escaped" in a second paragraph, then a third
    ///    paragraph gives a few more that "are explicitly forbidden".
    ///
    /// 2) Seqid rules, which are more restrictive and apply only
    ///    to the first column, allowing only:
    ///       a-z A-Z 0-9 . : ^ * $ @ ! + _ ? - |
    ///
    /// 3) The attribute rules, which effectively clarify the general rule's
    ///    second paragraph about reserved meanings, but give a
    ///    reduced list:
    ///       , = ;
    ///
    ///    Also, spaces are exlictly allowed, unescaped.
    ///
    /// 4) The Target tag rules, which are quite simply that spaces
    ///    must be escaped with %09 (and not with +).
    ///
    /// Wow! We'll forget about the more lax vs strict distinction
    /// since stict should be compativle with lax, and concentrate
    /// on the 3 different ways that spaces are represented.
    ///
    /// By prior versions of GFF3 specs (current is 1.14),
    /// + had special meaning (as a space). Those older versions
    /// of the GFF3 spcs discussed URL encoding, specifically mentionned
    /// + as space, and used such encoding in examples. In subsequent
    /// versions, + was explicitly listed amongst the allowable characters
    /// for the Seqid column. I believe the issue arised from
    /// confusion between URL encoding (which only does % escaping)
    /// versus application/x-www-form-urlencoded which is similar,
    /// but adds things like + to represent spaces. 
    ///
    /// In general, we'll prefer "%09", but recommend " " be used for
    /// the last column, attributes, but beweare the special exception
    /// in rule 4 requiring "%09" for Target. If generating output for
    /// older GFF3 parsers, we might get away with "+" as the escape
    /// for a space, which reads better, but is now rather dangerous.
    ///
    /// @param os the destination stream
    /// @param s the string to be encoded
    /// @param space the representation of spaces, which should be
    ///        one of "+" (default), " ", or "%09".
    static CNcbiOstream& x_AppendEncoded(CNcbiOstream& os,
                                         const string& s,
                                         const char* space = "%09");

    /// Formats any pairwise alignment into GFF3 format with CIGAR notation.
    ///
    /// @param width_inverted See x_FormatDenseg().
    void x_FormatAlignment(const CAlignmentItem& aln,
                           IFlatTextOStream& text_os, const CSeq_align& sa,
                           bool first, bool width_inverted);
    /// Formats a Dense-seg alignment into GFF3 format with CIGAR notation.
    ///
    /// @attention There is a disagreement in the meaning of widths
    ///            for a Dense-seg, as multiplier vs divisor relative to
    ///            the target sequence length. This function tries to
    ///            accommodate this problem.
    /// @param width_inverted If false, as in most cases, the widths are
    ///        a multiplier, i.e. width 3 means every 1 unit (aa) in the
    ///        alignment corresponds to an alignment of 3 units (na) on
    ///        the sequence. If true, as is the case with 
    ///        CSpliced_seg::s_ExonToDenseg, the widths act as divisors,
    ///        i.e. width 3 means every 3 units (na) in the alignment
    ///        correspond to 1 unit (aa) on the sequence.
    void x_FormatDenseg(const CAlignmentItem& aln,
                        IFlatTextOStream& text_os, const CDense_seg& ds,
                        bool first,
                        bool width_inverted);

    friend class CGFF3_CIGAR_Formatter;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___GFF3_FORMATTER__HPP */
