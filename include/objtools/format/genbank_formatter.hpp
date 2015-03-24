#ifndef OBJTOOLS_FORMAT___GENBANK_FORMATTER__HPP
#define OBJTOOLS_FORMAT___GENBANK_FORMATTER__HPP

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
*   new (early 2003) flat-file generator -- base class for items
*   (which roughly correspond to blocks/paragraphs in the C version)
*
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/item_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CKeywordsItem;
class CSourceItem;
class CReferenceItem;
class CCommentItem;
class CFeatHeaderItem;
class CBaseCountItem;
class CSequenceItem;
class CPrimaryItem;
class CSegmentItem;
class IFlattishFeature;
class CContigItem;
class CWGSItem;
class CTSAItem;
class CGenomeItem;
class CFlatTextOStream;
class CDateItem;
class CDBSourceItem;
class CBioseqContext;
class COriginItem;
class CGapItem;
class CSeq_loc;


class NCBI_FORMAT_EXPORT CGenbankFormatter : public CFlatItemFormatter
{
public:
    CGenbankFormatter(void);

    virtual SIZE_TYPE GetWidth(void) const { return 79; }

    virtual void EndSection(const CEndSectionItem&, IFlatTextOStream& text_os);

    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os);
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os);
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os);
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os);
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os);
    virtual void FormatSource(const CSourceItem& source, IFlatTextOStream& text_os);
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os);
    virtual void FormatComment(const CCommentItem& keys, IFlatTextOStream& text_os);
    virtual void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os);
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os);
    virtual void FormatFeatHeader(const CFeatHeaderItem& fh, IFlatTextOStream& text_os);
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os);
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os);
    virtual void FormatDBSource(const CDBSourceItem& dbs, IFlatTextOStream& text_os);
    virtual void FormatPrimary(const CPrimaryItem& prim, IFlatTextOStream& text_os);
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os);
    virtual void FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os);
    virtual void FormatTSA(const CTSAItem& tsa, IFlatTextOStream& text_os);
    virtual void FormatGenome(const CGenomeItem& genome, IFlatTextOStream& text_os);
    virtual void FormatOrigin(const COriginItem& origin, IFlatTextOStream& text_os);
    virtual void FormatGap(const CGapItem& gap, IFlatTextOStream& text_os);
    virtual void FormatGenomeProject(const CGenomeProjectItem&, IFlatTextOStream&);
    virtual void FormatHtmlAnchor(const CHtmlAnchorItem&, IFlatTextOStream&);

private:
    // source
    void x_FormatSourceLine(list<string>& l, const CSourceItem& source) const;
    void x_FormatOrganismLine(list<string>& l, const CSourceItem& source) const;

    // reference
    void x_Reference(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Authors(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Consortium(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Title(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Journal(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Medline(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Pubmed(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;
    void x_Remark(list<string>& l, const CReferenceItem& ref, CBioseqContext& ctx) const;

    // HTML
    void   x_LocusHtmlPrefix( std::string &first_line,      CBioseqContext& ctx );
    void x_GetFeatureSpanAndScriptStart(IFlatTextOStream& os, const CTempString& strKey, 
        const CSeq_loc &feat_loc, 
        CBioseqContext& ctx );

    void x_SmartWrapQuals(const class CFeatureItemBase& f, const class CFlatFeature& feat, IFlatTextOStream& text_os);

    // processing data
    unsigned int m_uFeatureCount;

    // HTML data

    // used for ids in <span...> tags
    typedef std::map< std::string, int > TFeatureKeyCountMap;
    TFeatureKeyCountMap m_FeatureKeyToLocMap;

    // used 
    bool m_bHavePrintedSourceFeatureJavascript;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___GENBANK_FORMATTER__HPP */
