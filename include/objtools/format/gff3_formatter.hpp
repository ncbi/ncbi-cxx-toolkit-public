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


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;

class NCBI_FORMAT_EXPORT CGFF3_Formatter : public CGFFFormatter
{
public:
    CGFF3_Formatter()
    {
        m_CurrentId = 1;
    }

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

    static CNcbiOstream& x_AppendEncoded(CNcbiOstream& os, const string& s);

    void x_FormatAlignment(const CAlignmentItem& aln,
                           IFlatTextOStream& text_os, const CSeq_align& sa);
    void x_FormatDenseg(const CAlignmentItem& aln,
                           IFlatTextOStream& text_os, const CDense_seg& ds);

};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___GFF3_FORMATTER__HPP */
