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
class COriginItem;


class CFlatItemFormatter : public IFormatter
{
public:
    
    // virtual constructor
    static CFlatItemFormatter* New(CFlatFileConfig::TFormat format);
    
    virtual ~CFlatItemFormatter(void);


    // control methods
    virtual void Start       (IFlatTextOStream&) {}
    virtual void StartSection(const CStartSectionItem&, IFlatTextOStream&) {}
    virtual void EndSection  (const CEndSectionItem&, IFlatTextOStream&)   {}
    virtual void End         (IFlatTextOStream&) {}

    // Format methods
    void Format(const IFlatItem& item, IFlatTextOStream& text_os);
    virtual void FormatLocus     (const CLocusItem&, IFlatTextOStream&)       {}
    virtual void FormatDefline   (const CDeflineItem&, IFlatTextOStream&)     {}
    virtual void FormatAccession (const CAccessionItem&, IFlatTextOStream&)   {}
    virtual void FormatVersion   (const CVersionItem&, IFlatTextOStream&)     {}
    virtual void FormatSegment   (const CSegmentItem&, IFlatTextOStream&)     {}
    virtual void FormatKeywords  (const CKeywordsItem&, IFlatTextOStream&)    {}
    virtual void FormatSource    (const CSourceItem&, IFlatTextOStream&)      {}
    virtual void FormatReference (const CReferenceItem&, IFlatTextOStream&)   {}
    virtual void FormatComment   (const CCommentItem&, IFlatTextOStream&)     {}
    virtual void FormatBasecount (const CBaseCountItem&, IFlatTextOStream&)   {}
    virtual void FormatSequence  (const CSequenceItem&, IFlatTextOStream&)    {}
    virtual void FormatFeatHeader(const CFeatHeaderItem&, IFlatTextOStream&)  {}
    virtual void FormatFeature   (const CFeatureItemBase&, IFlatTextOStream&) {}
    virtual void FormatDate      (const CDateItem&, IFlatTextOStream&)        {}
    virtual void FormatDBSource  (const CDBSourceItem&, IFlatTextOStream&)    {}
    virtual void FormatPrimary   (const CPrimaryItem&, IFlatTextOStream&)     {}
    virtual void FormatContig    (const CContigItem&, IFlatTextOStream&)      {}
    virtual void FormatWGS       (const CWGSItem&, IFlatTextOStream&)         {}
    virtual void FormatGenome    (const CGenomeItem&, IFlatTextOStream&)      {}
    virtual void FormatOrigin    (const COriginItem&, IFlatTextOStream&)      {}
    virtual void FormatGap       (const CGapItem&, IFlatTextOStream&)         {}
    virtual void FormatAlignment (const CAlignmentItem& , IFlatTextOStream&)  {}

    // Context
    void SetContext(CFlatFileContext& ctx);
    CFlatFileContext& GetContext(void) { return *m_Ctx; }

protected:
    typedef NStr::TWrapFlags    TWrapFlags;

    CFlatItemFormatter(void) : m_WrapFlags(NStr::fWrap_FlatFile) {}
    CFlatItemFormatter(const CFlatItemFormatter&);
    CFlatItemFormatter& operator=(const CFlatItemFormatter&);

    enum EPadContext {
        ePara,
        eSubp,
        eFeatHead,
        eFeat
    };

    static const string s_GenbankMol[];

    virtual SIZE_TYPE GetWidth(void) const { return 78; }

    static  string& x_Pad(const string& s, string& out, SIZE_TYPE width,
                        const string& indent = kEmptyStr);
    virtual string& Pad(const string& s, string& out, EPadContext where) const;
    virtual list<string>& Wrap(list<string>& l, SIZE_TYPE width, 
        const string& tag, const string& body, EPadContext where = ePara) const;
    virtual list<string>& Wrap(list<string>& l, const string& tag,
        const string& body, EPadContext where = ePara) const;

    void x_FormatRefLocation(CNcbiOstrstream& os, const CSeq_loc& loc,
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

    TWrapFlags& SetWrapFlags(void) { return m_WrapFlags; }

private:
    // data
    string                 m_Indent;
    string                 m_FeatIndent;
    TWrapFlags             m_WrapFlags;
    CRef<CFlatFileContext> m_Ctx;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2005/02/09 14:53:57  shomrat
* x_FormatRefJournal take a context instead of config as parameter
*
* Revision 1.10  2005/02/07 14:56:51  shomrat
* Added WrapFlags
*
* Revision 1.9  2005/01/12 16:43:16  shomrat
* Added FormatAlignment; Changed journal formatting
*
* Revision 1.8  2004/11/24 16:48:02  shomrat
* Handle gap items
*
* Revision 1.7  2004/04/22 15:46:39  shomrat
* Changes in context
*
* Revision 1.6  2004/04/13 16:43:41  shomrat
* + x_FormatRefJournal()
*
* Revision 1.5  2004/03/18 15:31:19  shomrat
* Changed default width value
*
* Revision 1.4  2004/02/19 17:58:53  shomrat
* Added method to format Origin item
*
* Revision 1.3  2004/02/11 16:48:14  shomrat
* removed variable names to supress compiler warnings
*
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
