#ifndef OBJTOOLS_FORMAT___ITEM_FORMATTER_HPP
#define OBJTOOLS_FORMAT___ITEM_FORMATTER_HPP

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

#include <objtools/format/formatter.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatItem;
class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CKeywordsItem;
class CSegmentItem;
class CSourceItem;
class CReferenceItem;
class CCacheItem;
class CCommentItem;
class CFeatHeaderItem;
class CFeatureItemBase;
class CBaseCountItem;
class CSequenceItem;
class CPrimaryItem;
class CFeatHeaderItem;
class CContigItem;
class CWGSItem;
class CTSAItem;
class CGenomeItem;
class CFlatTextOStream;
class CDateItem;
class CDBSourceItem;
class COriginItem;


class NCBI_FORMAT_EXPORT CFlatItemFormatter : public IFormatter
{
public:

    // virtual constructor
    static CFlatItemFormatter* New(CFlatFileConfig::TFormat format);

    ~CFlatItemFormatter() override;

    // control methods
    void Start(IFlatTextOStream&) override;
    void StartSection(const CStartSectionItem&, IFlatTextOStream&) override {}
    void EndSection(const CEndSectionItem&, IFlatTextOStream&) override {}
    void End(IFlatTextOStream&) override;

    // Format methods
    void Format(const IFlatItem& item, IFlatTextOStream&) override;
    void FormatLocus(const CLocusItem&, IFlatTextOStream&) override {}
    void FormatDefline(const CDeflineItem&, IFlatTextOStream&) override {}
    void FormatAccession(const CAccessionItem&, IFlatTextOStream&) override {}
    void FormatVersion(const CVersionItem&, IFlatTextOStream&) override {}
    void FormatSegment(const CSegmentItem&, IFlatTextOStream&) override {}
    void FormatKeywords(const CKeywordsItem&, IFlatTextOStream&) override {}
    void FormatSource(const CSourceItem&, IFlatTextOStream&) override {}
    void FormatReference(const CReferenceItem&, IFlatTextOStream&) override {}
    void FormatCache(const CCacheItem&, IFlatTextOStream&) override {}
    void FormatComment(const CCommentItem&, IFlatTextOStream&) override {}
    void FormatBasecount(const CBaseCountItem&, IFlatTextOStream&) override {}
    void FormatSequence(const CSequenceItem&, IFlatTextOStream&) override {}
    void FormatFeatHeader(const CFeatHeaderItem&, IFlatTextOStream&) override {}
    void FormatFeature(const CFeatureItemBase&, IFlatTextOStream&) override {}
    void FormatDate(const CDateItem&, IFlatTextOStream&) override {}
    void FormatDBSource(const CDBSourceItem&, IFlatTextOStream&) override {}
    void FormatPrimary(const CPrimaryItem&, IFlatTextOStream&) override {}
    void FormatContig(const CContigItem&, IFlatTextOStream&) override {}
    void FormatWGS(const CWGSItem&, IFlatTextOStream&) override {}
    void FormatTSA(const CTSAItem&, IFlatTextOStream&) override {}
    void FormatGenome(const CGenomeItem&, IFlatTextOStream&) override {}
    void FormatOrigin(const COriginItem&, IFlatTextOStream&) override {}
    void FormatGap(const CGapItem&, IFlatTextOStream&) override {}
    void FormatAlignment(const CAlignmentItem&, IFlatTextOStream&) override {}
    void FormatGenomeProject(const CGenomeProjectItem&, IFlatTextOStream&) override {}

    // Context
    void SetContext(CFlatFileContext& ctx);
    const CFlatFileContext& GetContext(void) const
    { return *m_Ctx; }

protected:
    typedef NStr::TWrapFlags    TWrapFlags;

    CFlatItemFormatter(void) : m_WrapFlags(NStr::fWrap_FlatFile) {}

    enum EPadContext {
        ePara,
        eSubp,
        eFeatHead,
        eFeat,
        eBarcode
    };

    static const string s_GenbankMol[];
    static const string s_EmblMol[];

    virtual SIZE_TYPE GetWidth(void) const { return 78; }

    static  string& x_Pad(const string& s, string& out, SIZE_TYPE width,
                        const string& indent = kEmptyStr);
    virtual string& Pad(const string& s, string& out, EPadContext where) const;
    virtual list<string>& Wrap(list<string>& l, SIZE_TYPE width,
        const string& tag, const string& body, EPadContext where = ePara, bool htmlaware = false) const;
    virtual list<string>& Wrap(list<string>& l, const string& tag,
        const string& body, EPadContext where = ePara, bool htmlaware = false, int internalIndent = 0 ) const;

    void x_FormatRefLocation(ostream& os, const CSeq_loc& loc,
        const string& to, const string& delim,
        CBioseqContext& ctx) const;
    void x_FormatRefJournal(const CReferenceItem& ref, string& journal,
        CBioseqContext& ctx) const;
    string x_FormatAccession(const CAccessionItem& acc, char separator) const;

    void x_GetKeywords(const CKeywordsItem& kws, const string& prefix,
        list<string>& l) const;

    const string& GetIndent(void) const { return m_Indent; }
    void SetIndent(const string& indent) { m_Indent = indent; }

    const string& GetFeatIndent(void) const { return m_FeatIndent; }
    void SetFeatIndent(const string& feat_indent) { m_FeatIndent = feat_indent; }

    const string& GetBarcodeIndent(void) const { return m_BarcodeIndent; }
    void SetBarcodeIndent(const string& barcode_indent) { m_BarcodeIndent = barcode_indent; }

    TWrapFlags& SetWrapFlags(void) { return m_WrapFlags; }

private:
    CFlatItemFormatter(const CFlatItemFormatter&);
    CFlatItemFormatter& operator=(const CFlatItemFormatter&);

    // data
    string                 m_Indent;
    string                 m_FeatIndent;
    string                 m_BarcodeIndent;
    TWrapFlags             m_WrapFlags;
    CRef<CFlatFileContext> m_Ctx;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___ITEM_FORMATTER_HPP */
