#ifndef OBJTOOLS_FORMAT___SAM_FORMATTER__HPP
#define OBJTOOLS_FORMAT___SAM_FORMATTER__HPP

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
 */

/// @file sam_formatter.hpp
/// Flat formatter for Sequence Alignment/Map (SAM).


#include <objtools/format/formatter.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_FORMAT_EXPORT CSAM_Formatter : public IFormatter
{
public:
    CSAM_Formatter(void) {}
    virtual ~CSAM_Formatter(void) {}

    // control methods
    virtual void Start       (IFlatTextOStream& text_os);
    virtual void StartSection(const CStartSectionItem& ssec, IFlatTextOStream& text_os) {}
    virtual void EndSection  (const CEndSectionItem& esec, IFlatTextOStream& text_os) {}
    virtual void End         (IFlatTextOStream& text_os);

    // format methods
    virtual void Format(const IFlatItem& item, IFlatTextOStream& text_os);

    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os) {}
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os) {}
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os) {}
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os) {}
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatSource(const CSourceItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatComment(const CCommentItem& comment, IFlatTextOStream& text_os) {}
    virtual void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os) {}
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os) {}
    virtual void FormatFeatHeader(const CFeatHeaderItem& fh, IFlatTextOStream& text_os) {}
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os) {}
    virtual void FormatAlignment(const CAlignmentItem& aln, IFlatTextOStream& text_os);
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os) {}
    virtual void FormatDate(const CDateItem& date, IFlatTextOStream& text_os) {}
    virtual void FormatDBSource(const CDBSourceItem& dbs, IFlatTextOStream& text_os) {}
    virtual void FormatPrimary(const CPrimaryItem& prim, IFlatTextOStream& text_os) {}
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os) {}
    virtual void FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os) {}
    virtual void FormatGenome(const CGenomeItem& genome, IFlatTextOStream& text_os) {}
    virtual void FormatOrigin(const COriginItem& origin, IFlatTextOStream& text_os) {}
    virtual void FormatGap(const CGapItem& gap, IFlatTextOStream& text_os) {}
    virtual void FormatGenomeProject(const CGenomeProjectItem&, IFlatTextOStream&) {}
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___SAM_FORMATTER__HPP */
