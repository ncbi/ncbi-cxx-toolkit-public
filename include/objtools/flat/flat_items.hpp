#ifndef OBJECTS_FLAT___FLAT_ITEMS__HPP
#define OBJECTS_FLAT___FLAT_ITEMS__HPP

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
*   new (early 2003) flat-file generator -- item types
*   (mainly of interest to implementors)
*
*/

// Complex enough to get their own headers
// (flat_reference.hpp already included indirectly)
#include <objtools/flat/flat_head.hpp>
#include <objtools/flat/flat_feature.hpp>

#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatForehead : public IFlatItem
{
public:
    CFlatForehead(CFlatContext& ctx) : m_Context(&ctx) { }
    void Format(IFlatFormatter& f) const { f.BeginSequence(*m_Context); }

private:
    mutable CRef<CFlatContext> m_Context;
};


// CFlatHead split into flat_head.hpp


class CFlatKeywords : public IFlatItem
{
public:
    CFlatKeywords(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatKeywords(*this); }
    const list<string>& GetKeywords(void) const { return m_Keywords; }

private:
    list<string> m_Keywords;
};


class CFlatSegment : public IFlatItem
{
public:
    CFlatSegment(const CFlatContext& ctx)
        : m_Num(ctx.GetSegmentNum()), m_Count(ctx.GetSegmentCount()) { }
    void Format(IFlatFormatter& f) const { f.FormatSegment(*this); }

    unsigned int GetNum  (void) const { return m_Num;   }
    unsigned int GetCount(void) const { return m_Count; }

private:
    unsigned int m_Num, m_Count;
};


// SOURCE/ORGANISM, not source feature
class CFlatSource : public IFlatItem
{
public:
    CFlatSource(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatSource(*this); }
    string GetTaxonomyURL(void) const;

    int             GetTaxID     (void) const { return m_TaxID;       }
    const string&   GetFormalName(void) const { return m_FormalName;  }
    const string&   GetCommonName(void) const { return m_FormalName;  }
    const string&   GetLineage   (void) const { return m_Lineage;     }
    const CSeqdesc& GetDescriptor(void) const { return *m_Descriptor; }

private:
    int                 m_TaxID;
    string              m_FormalName;
    string              m_CommonName;
    string              m_Lineage; // semicolon-delimited
    CConstRef<CSeqdesc> m_Descriptor;
};


// CFlatReference split into flat_reference.hpp


class CFlatComment : public IFlatItem
{
public:
    CFlatComment(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatComment(*this); }

    const string& GetComment(void) const { return m_Comment; }

private:
    string m_Comment;
};


class CFlatPrimary : public IFlatItem
{
public:
    CFlatPrimary(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatPrimary(*this); }

    typedef CRange<TSeqPos> TRange;
    struct SPiece {
        TRange             m_Span;
        CConstRef<CSeq_id> m_PrimaryID;
        TRange             m_PrimarySpan;
        bool               m_Complemented;

        // for usual tabular format (even incorporated in GBSet!)
        string& Format(string &s) const;
    };
    typedef list<SPiece> TPieces;

    // for usual tabular format (even incorporated in GBSet!)
    const char*    GetHeader(void) const;
    const TPieces& GetPieces(void) const { return m_Pieces; }

private:
    bool    m_IsRefSeq;
    TPieces m_Pieces;
};


// no actual data needed
class CFlatFeatHeader : public IFlatItem
{
public:
    CFlatFeatHeader() { }
    void Format(IFlatFormatter& f) const { f.FormatFeatHeader(*this); }
};


// CFlatFeature, CFlattishFeature, etc. split into flat_feature.hpp


class CFlatDataHeader : public IFlatItem
{
public:
    CFlatDataHeader(const CFlatContext& ctx)
        : m_Loc(&ctx.GetLocation()), m_Handle(ctx.GetHandle()),
          m_IsProt(ctx.IsProt()),
          m_As(0), m_Cs(0), m_Gs(0), m_Ts(0), m_Others(0)
        { }
    void Format(IFlatFormatter& f) const
        { f.FormatDataHeader(*this); }
    void GetCounts(TSeqPos& a, TSeqPos& c, TSeqPos& g, TSeqPos& t,
                   TSeqPos& other) const;

private:
    CConstRef<CSeq_loc> m_Loc;
    CBioseq_Handle      m_Handle;
    bool                m_IsProt;
    mutable TSeqPos     m_As, m_Cs, m_Gs, m_Ts, m_Others;
};


class CFlatData : public IFlatItem
{
public:
    CFlatData(const CFlatContext& ctx) : m_Loc(&ctx.GetLocation()) { }
    void Format(IFlatFormatter& f) const { f.FormatData(*this); }

    const CSeq_loc& GetLoc(void) const { return *m_Loc; }

private:
    CConstRef<CSeq_loc> m_Loc;
};


class CFlatContig : public IFlatItem
{
public:
    CFlatContig(const CFlatContext& ctx) : m_Loc(&ctx.GetLocation()) { }
    void Format(IFlatFormatter& f) const { f.FormatContig(*this); }

    const CSeq_loc& GetLoc(void) const { return *m_Loc; }

private:
    CConstRef<CSeq_loc> m_Loc;
};


class CFlatWGSRange : public IFlatItem
{
public:
    CFlatWGSRange(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatWGSRange(*this); }

    const string&       GetFirstID   (void) const { return m_First; }
    const string&       GetLastID    (void) const { return m_Last;  }
    const CUser_object& GetUserObject(void) const { return *m_UO;   }

private:
    string                  m_First, m_Last; // IDs in range
    CConstRef<CUser_object> m_UO;
};


class CFlatGenomeInfo : public IFlatItem
{
public:
    CFlatGenomeInfo(const CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatGenomeInfo(*this); }

    const string&       GetAccession (void) const { return *m_Accession; }
    const string&       GetMoltype   (void) const
        { return m_Moltype ? *m_Moltype : kEmptyStr; }
    const CUser_object& GetUserObject(void) const { return *m_UO;        }

private:
    const string*           m_Accession;
    const string*           m_Moltype; // stored as a string in the ASN.1
    CConstRef<CUser_object> m_UO;
};


// no actual data needed
class CFlatTail : public IFlatItem
{
public:
    CFlatTail() { }
    void Format(IFlatFormatter& f) const { f.EndSequence(); }
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS


inline
string CFlatSource::GetTaxonomyURL(void) const {
    return "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id="
        + NStr::IntToString(m_TaxID);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2003/09/19 00:22:45  ucko
* CFlatPrimary: use an unsigned range per CAlnMap::GetSeqRange's new rettype.
*
* Revision 1.5  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.4  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.3  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/10 22:05:13  ucko
* -SFlatKeywords::x_AddKeys (MSVC didn't like it :-/)
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_ITEMS__HPP */
