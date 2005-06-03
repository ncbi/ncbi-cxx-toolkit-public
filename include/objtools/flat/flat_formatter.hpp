#ifndef OBJECTS_FLAT___FLAT_FORMATTER__HPP
#define OBJECTS_FLAT___FLAT_FORMATTER__HPP

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
*
* File Description:
*   new (early 2003) flat-file generator -- main public interface
*
*/

#include <objtools/flat/flat_context.hpp>

#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

class NCBI_FLAT_EXPORT IFlatFormatter : public IFlatItemOStream
{
public:
    enum EDatabase {
        // determines appropriate division codes and a few other things
        eDB_NCBI,
        eDB_EMBL,
        eDB_DDBJ
    };

    enum EMode {
        // determines the tradeoff between strictness and completeness
        eMode_Release, // strict -- for official public releases
        eMode_Entrez,  // somewhat laxer -- for CGIs
        eMode_GBench,  // even laxer -- for editing submissions
        eMode_Dump     // shows everything, regardless of validity
    };

    enum EStyle {
        // determines handling of segmented records
        eStyle_Normal,  // default -- show segments iff they're near
        eStyle_Segment, // always show segments
        eStyle_Master,  // merge segments into a single virtual record
        eStyle_Contig   // just an index of segments -- no actual sequence
    };

    enum EFlags {
        // various generic options
        fProduceHTML          = 0x2,
        fShowContigFeatures   = 0x4, // not just source features
        fShowContigSources    = 0x8, // not just focus
        fShowFarTranslations  = 0x10,
        fTranslateIfNoProduct = 0x20,
        fAlwaysTranslateCDS   = 0x40,
        fOnlyNearFeatures     = 0x80,
        fFavorFarFeatures     = 0x100, // ignore near feats on segs w/far feats
        fCopyCDSFromCDNA      = 0x200, // these two are for gen. prod. sets
        fCopyGeneToCDNA       = 0x400,
        fShowContigInMaster   = 0x800,
        fHideImportedFeatures = 0x10000,
        fHideRemoteImpFeats   = 0x20000,
        fHideSNPFeatures      = 0x40000,
        fHideExonFeatures     = 0x80000,
        fHideIntronFeatures   = 0x100000,
        fHideMiscFeatures     = 0x200000,
        fHideCDDFeatures      = 0x400000,
        fHideCDSProdFeatures  = 0x800000,
        fShowTranscriptions   = 0x1000000,
        fShowPeptides         = 0x2000000
    };
    typedef int TFlags; // binary OR of "EFlags"

    enum EFilterFlags {
        // determines which Bioseqs in an entry to skip
        fSkipNucleotides = 0x1,
        fSkipProteins    = 0x2
    };
    typedef int TFilterFlags; // binary or of "EFilterFlags"

    IFlatFormatter(CScope& scope, EMode mode, EStyle style, TFlags flags)
        : m_Scope(&scope), m_Mode(mode), m_Style(style), m_Flags(flags)
        { }
    virtual ~IFlatFormatter() { }

    // Users should *not* normally supply context!
    void Format(const CSeq_entry& entry, IFlatItemOStream& out,
                TFilterFlags flags = 0, CFlatContext* context = 0);
    void Format(const CBioseq& seq, IFlatItemOStream& out,
                CFlatContext* context = 0);
    void Format(const CSeq_loc& loc, bool adjust_coords,
                IFlatItemOStream& out, CFlatContext* context = 0);

    bool              DoHTML     (void) const
        { return (m_Flags & fProduceHTML) != 0; }
    virtual EDatabase GetDatabase(void) const = 0;
    EMode             GetMode    (void) const { return m_Mode; }

    void AddItem(CConstRef<IFlatItem> i) { i->Format(*this); }

    // callbacks for the items to use
    virtual void BeginSequence   (CFlatContext& ctx)
        { m_Context.Reset(&ctx);  m_Context->m_Formatter = this; }
    virtual void FormatHead      (const CFlatHead& head)       = 0;
    virtual void FormatKeywords  (const CFlatKeywords& keys)   = 0;
    virtual void FormatSegment   (const CFlatSegment& seg)     = 0;
    virtual void FormatSource    (const CFlatSource& source)   = 0;
    virtual void FormatReference (const CFlatReference& ref)   = 0;
    virtual void FormatComment   (const CFlatComment& comment) = 0;
    virtual void FormatPrimary   (const CFlatPrimary& prim)    = 0; // TPAs
    virtual void FormatFeatHeader(const CFlatFeatHeader& fh)   = 0;
    virtual void FormatFeature   (const IFlattishFeature& f)   = 0;
    virtual void FormatDataHeader(const CFlatDataHeader& dh)   = 0;
    virtual void FormatData      (const CFlatData& data)       = 0;
    // alternatives to DataHeader + Data...
    virtual void FormatContig    (const CFlatContig& contig)   = 0;
    virtual void FormatWGSRange  (const CFlatWGSRange& range)  = 0;
    virtual void FormatGenomeInfo(const CFlatGenomeInfo& g)    = 0; // NS_*
    virtual void EndSequence     (void)                        = 0;

    enum ETildeStyle {
        eTilde_tilde,  // no-op
        eTilde_space,  // '~' -> ' ', except before /[ (]?\d/
        eTilde_newline // '~' -> '\n' but "~~" -> "~"
    };
    static string ExpandTildes(const string& s, ETildeStyle style);

    // appends date to s, as DD-Mon-YYYY
    void FormatDate(const CDate& date, string& s) const;
    string GetAccnLink(const string& acc) const;

protected:
    CScope*            m_Scope;
    EMode              m_Mode;
    EStyle             m_Style;
    TFlags             m_Flags;
    CRef<CFlatContext> m_Context; // for sequence currently being formatted

private:
    bool x_FormatSegments(const CBioseq& seq, IFlatItemOStream& out,
                          CFlatContext& ctx);
    void x_FormatReferences(CFlatContext& ctx, IFlatItemOStream& out);
    void x_FormatFeatures(CFlatContext& ctx, IFlatItemOStream& out,
                          bool source);
};


///////////////////////////////////////////////////////////////////////////
//
// Inline functions


inline
void IFlatFormatter::FormatDate(const CDate& date, string& s) const
{
    SIZE_TYPE pos = s.size();
    if (m_Mode == eMode_Dump) {
        date.GetDate(&s, "%{%2D%|\?\?%}-%{%3N%|\?\?\?%}-%Y");
    } else {
        // complain if not std
        date.GetDate(&s, "%2D-%3N-%Y");
    }
    for (SIZE_TYPE i = pos;  i < s.size();  ++i) {
        if (islower((unsigned char)(s[i]))) {
            s[i] = toupper((unsigned char) s[i]);
        }
    }
}


inline
string IFlatFormatter::GetAccnLink(const string& acc) const
{
    if (DoHTML()) {
        return "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?val="
            + acc + "\">" + acc + "</a>";
    } else {
        return acc;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2005/06/03 16:21:34  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.6  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.5  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.4  2003/03/28 17:45:36  dicuccio
* Added (very judicious) use of Win32 exports - only needed in external classes
* CFlatTextFormatter and IFlatFormatter
*
* Revision 1.3  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/10 22:00:20  ucko
* Add a redundant "!= 0" to DoHTML to suppress a MSVC warning.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_FORMATTER__HPP */
