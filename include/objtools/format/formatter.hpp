#ifndef OBJTOOLS_FORMAT___FORMATTER_HPP
#define OBJTOOLS_FORMAT___FORMATTER_HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*           
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatItem;
class IFlatTextOStream;
class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CKeywordsItem;
class CSourceItem;
class CReferenceItem;
class CCommentItem;
class CFeatHeaderItem;
class CAlignmentItem;
class CBaseCountItem;
class CSequenceItem;
class CPrimaryItem;
class CSegmentItem;
class CFeatureItemBase;
class CContigItem;
class CWGSItem;
class CTSAItem;
class CGenomeItem;
class CEndSectionItem;
class CFlatTextOStream;
class CDateItem;
class CDBSourceItem;
class COriginItem;
class CStartSectionItem;
class CEndSectionItem;
class CGapItem;
class CGenomeProjectItem;
class CHtmlAnchorItem;

class IFormatter : public CObject
{
public:
    
    // control methods
    virtual void Start       (IFlatTextOStream& text_os) = 0;
    virtual void StartSection(const CStartSectionItem& ssec, IFlatTextOStream& text_os) = 0;
    virtual void EndSection  (const CEndSectionItem& esec, IFlatTextOStream& text_os) = 0;
    virtual void End         (IFlatTextOStream& text_os) = 0;

    // format methods
    virtual void Format(const IFlatItem& item, IFlatTextOStream& text_os) = 0;

    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os) = 0;
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os) = 0;
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os) = 0;
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os) = 0;
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os) = 0;
    virtual void FormatSource(const CSourceItem& keys, IFlatTextOStream& text_os) = 0;
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os) = 0;
    virtual void FormatComment(const CCommentItem& comment, IFlatTextOStream& text_os) = 0;
    virtual void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os) = 0;
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os) = 0;
    virtual void FormatFeatHeader(const CFeatHeaderItem& fh, IFlatTextOStream& text_os) = 0;
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os) = 0;
    virtual void FormatAlignment(const CAlignmentItem& aln, IFlatTextOStream& text_os) = 0;
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os) = 0;
    virtual void FormatDate(const CDateItem& date, IFlatTextOStream& text_os) = 0;
    virtual void FormatDBSource(const CDBSourceItem& dbs, IFlatTextOStream& text_os) = 0;
    virtual void FormatPrimary(const CPrimaryItem& prim, IFlatTextOStream& text_os) = 0;
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os) = 0;
    virtual void FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os) = 0;
    virtual void FormatTSA(const CTSAItem& wgs, IFlatTextOStream& text_os) = 0;
    virtual void FormatGenome(const CGenomeItem& genome, IFlatTextOStream& text_os) = 0;
    virtual void FormatOrigin(const COriginItem& origin, IFlatTextOStream& text_os) = 0;
    virtual void FormatGap(const CGapItem& gap, IFlatTextOStream& text_os) = 0;
    virtual void FormatGenomeProject(const CGenomeProjectItem&, IFlatTextOStream&) {}
    virtual void FormatHtmlAnchor(const CHtmlAnchorItem&, IFlatTextOStream&) {}
    
    virtual ~IFormatter(void) {}
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FORMATTER_HPP */
