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
#include <objects/flat/flat_head.hpp>
#include <objects/flat/flat_feature.hpp>

#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatForehead : public IFlatItem
{
public:
    CFlatForehead(SFlatContext& ctx) : m_Context(&ctx) { }
    void Format(IFlatFormatter& f) const { f.BeginSequence(*m_Context); }

private:
    mutable CRef<SFlatContext> m_Context;
};


// SFlatHead split into flat_head.hpp


struct SFlatKeywords : public IFlatItem
{
public:
    SFlatKeywords(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatKeywords(*this); }

    list<string> m_Keywords;

private:
    template <class TBlock> void x_AddKeys(const TBlock& block);
};


struct SFlatSegment : public IFlatItem
{
public:
    SFlatSegment(const SFlatContext& ctx)
        : m_Num(ctx.m_SegmentNum), m_Count(ctx.m_SegmentCount) { }
    void Format(IFlatFormatter& f) const { f.FormatSegment(*this); }

    unsigned int m_Num, m_Count;
};


// SOURCE/ORGANISM, not source feature
struct SFlatSource : public IFlatItem
{
public:
    SFlatSource(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatSource(*this); }
    string GetTaxonomyURL(void) const;

    int                 m_TaxID;
    string              m_FormalName;
    string              m_CommonName;
    string              m_Lineage; // semicolon-delimited
    CConstRef<CSeqdesc> m_Descriptor;
};


// SFlatReference split into flat_reference.hpp


struct SFlatComment : public IFlatItem
{
public:
    SFlatComment(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatComment(*this); }

    string m_Comment;
};


struct SFlatPrimary : public IFlatItem
{
public:
    SFlatPrimary(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatPrimary(*this); }

    typedef CRange<TSignedSeqPos> TRange;
    struct SPiece {
        TRange             m_Span;
        CConstRef<CSeq_id> m_PrimaryID;
        TRange             m_PrimarySpan;
        bool               m_Complemented;

        // for usual tabular format (even incorporated in GBSet!)
        string& Format(string &s) const;
    };
    typedef list<SPiece> TPieces;

    bool    m_IsRefSeq;
    TPieces m_Pieces;

    // for usual tabular format (even incorporated in GBSet!)
    const char* GetHeader(void) const;
};


// no actual data needed
class CFlatFeatHeader : public IFlatItem
{
public:
    CFlatFeatHeader() { }
    void Format(IFlatFormatter& f) const { f.FormatFeatHeader(); }
};


// SFlatFeature, CFlattishFeature, etc. split into flat_feature.hpp


struct SFlatDataHeader : public IFlatItem
{
    SFlatDataHeader(const SFlatContext& ctx)
        : m_Loc(ctx.m_Location), m_Handle(ctx.m_Handle),
          m_IsProt(ctx.m_IsProt),
          m_As(0), m_Cs(0), m_Gs(0), m_Ts(0), m_Others(0)
        { }
    void Format(IFlatFormatter& f) const
        { f.FormatDataHeader(*this); }
    void GetCounts(TSeqPos& a, TSeqPos& c, TSeqPos& g, TSeqPos& t,
                   TSeqPos& other) const;

    CConstRef<CSeq_loc> m_Loc;
    CBioseq_Handle      m_Handle;
    bool                m_IsProt;
    mutable TSeqPos     m_As, m_Cs, m_Gs, m_Ts, m_Others;
};


struct SFlatData : public IFlatItem
{
public:
    SFlatData(const SFlatContext& ctx) : m_Loc(ctx.m_Location) { }
    void Format(IFlatFormatter& f) const { f.FormatData(*this); }

    CConstRef<CSeq_loc> m_Loc;
};


struct SFlatContig : public IFlatItem
{
    SFlatContig(const SFlatContext& ctx) : m_Loc(ctx.m_Location) { }
    void Format(IFlatFormatter& f) const { f.FormatContig(*this); }

    CConstRef<CSeq_loc> m_Loc;
};


struct SFlatWGSRange : public IFlatItem
{
    SFlatWGSRange(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatWGSRange(*this); }

    string                  m_First, m_Last; // IDs in range
    CConstRef<CUser_object> m_UO;
};


struct SFlatGenomeInfo : public IFlatItem
{
    SFlatGenomeInfo(const SFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatGenomeInfo(*this); }

    string                  m_Accession;
    string                  m_Moltype; // stored as a string in the ASN.1
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
string SFlatSource::GetTaxonomyURL(void) const {
    return "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id="
        + NStr::IntToString(m_TaxID);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_ITEMS__HPP */
