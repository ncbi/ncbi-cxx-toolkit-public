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
class CCommentItem;
class CFeatHeaderItem;
class CFeatureItemBase;
class CBaseCountItem;
class CSequenceItem;
class CPrimaryItem;
class CFeatHeaderItem;
class CContigItem;
class CWGSItem;
class CGenomeItem;
class CFlatTextOStream;
class CDateItem;
class CDBSourceItem;


class CFlatItemFormatter : public IFormatter
{
public:
    
    // virtual constructor
    static CFlatItemFormatter* New(TFormat format);
    
    virtual ~CFlatItemFormatter(void);


    // control methods
    virtual void Start       (IFlatTextOStream& text_os) {}
    virtual void StartSection(IFlatTextOStream& text_os) {}
    virtual void EndSection  (IFlatTextOStream& text_os) {}
    virtual void End         (IFlatTextOStream& text_os) {}

    // Format methods
    void Format(const IFlatItem& item, IFlatTextOStream& text_os);
    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os) {}
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os) {}
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os) {}
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os) {}
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os) {}
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatSource(const CSourceItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os) {}
    virtual void FormatComment(const CCommentItem& comment, IFlatTextOStream& text_os) {}
    virtual void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os) {}
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os) {}
    virtual void FormatFeatHeader(const CFeatHeaderItem& fh, IFlatTextOStream& text_os) {}
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os) {}
    virtual void FormatDate(const CDateItem& seg, IFlatTextOStream& text_os) {}
    virtual void FormatDBSource(const CDBSourceItem& dbs, IFlatTextOStream& text_os) {}
    virtual void FormatPrimary(const CPrimaryItem& prim, IFlatTextOStream& text_os) {}
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os) {}
    virtual void FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os) {}
    virtual void FormatGenome(const CGenomeItem& genome, IFlatTextOStream& text_os) {}

    // Context
    void SetContext(CFFContext& ctx) { m_Ctx.Reset(&ctx); }
    CFFContext& GetContext(void) { return *m_Ctx; }

protected:
    CFlatItemFormatter(void) {} // !!!
    CFlatItemFormatter(const CFlatItemFormatter&);
    CFlatItemFormatter& operator=(const CFlatItemFormatter&);

    enum EPadContext {
        ePara,
        eSubp,
        eFeatHead,
        eFeat
    };

    static const string s_GenbankMol[];

    virtual SIZE_TYPE GetWidth(void) const { return 0; }

    static  string& x_Pad(const string& s, string& out, SIZE_TYPE width,
                        const string& indent = kEmptyStr);
    virtual string& Pad(const string& s, string& out, EPadContext where) const;
    virtual list<string>& Wrap(list<string>& l, SIZE_TYPE width, 
        const string& tag, const string& body, EPadContext where = ePara) const;
    virtual list<string>& Wrap(list<string>& l, const string& tag,
        const string& body, EPadContext where = ePara) const;

    void x_FormatRefLocation(CNcbiOstrstream& os, const CSeq_loc& loc,
        const string& to, const string& delim,
        bool is_prot, CScope& scope) const;
    string x_FormatAccession(const CAccessionItem& acc, char separator) const;

    void x_GetKeywords(const CKeywordsItem& kws, const string& prefix,
        list<string>& l) const;

    const string& GetIndent(void) const { return m_Indent; }
    void SetIndent(const string& indent) { m_Indent = indent; }

    const string& GetFeatIndent(void) const { return m_FeatIndent; }
    void SetFeatIndent(const string& feat_indent) { m_FeatIndent = feat_indent; }

private:
    // data
    string           m_Indent;
    string           m_FeatIndent;
    CRef<CFFContext> m_Ctx;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/01/14 15:56:18  shomrat
* const removed; added control methods
*
* Revision 1.1  2003/12/17 19:53:37  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___ITEM_FORMATTER_HPP */
