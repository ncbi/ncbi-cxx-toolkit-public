#ifndef OBJECTS_FLAT___FLAT_FEATURE__HPP
#define OBJECTS_FLAT___FLAT_FEATURE__HPP

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
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*/

#include <objtools/flat/flat_formatter.hpp>
#include <objtools/flat/flat_qual_slots.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// low-level formatted qualifier
class CFlatQual : public CObject
{
public:
    enum EStyle {
        eEmpty,   // /name [value ignored]
        eQuoted,  // /name="value"
        eUnquoted // /name=value
    };

    CFlatQual(const string& name, const string& value, EStyle style = eQuoted)
        : m_Name(name), m_Value(value), m_Style(style)
        { }

    const string& GetName (void) const { return m_Name;  }
    const string& GetValue(void) const { return m_Value; }
    EStyle        GetStyle(void) const { return m_Style; }

private:
    string m_Name, m_Value;
    EStyle m_Style;
};
typedef vector<CRef<CFlatQual> > TFlatQuals;


// abstract qualifier value -- see flat_quals.hpp for derived classes
class IFlatQV : public CObject
{
public:
    enum EFlags {
        fIsNote   = 0x1,
        fIsSource = 0x2
    };
    typedef int TFlags; // binary OR of EFlags

    virtual void Format(TFlatQuals& quals, const string& name,
                        CFlatContext& ctx, TFlags flags = 0) const = 0;

protected:
    static void x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                        CFlatQual::EStyle st = CFlatQual::eQuoted)
        { q.push_back(CRef<CFlatQual>(new CFlatQual(n, v, st))); }
};


class CFlatFeature : public CObject
{
public:
    CFlatFeature(const string& key, const CFlatLoc& loc, const CSeq_feat& feat)
        : m_Key(key), m_Loc(&loc), m_Feat(&feat) { }

    typedef vector<CRef<CFlatQual> > TQuals;

    const string&    GetKey  (void) const { return m_Key;   }
    const CFlatLoc&  GetLoc  (void) const { return *m_Loc;  }
    const TQuals&    GetQuals(void) const { return m_Quals; }
    const CSeq_feat& GetFeat (void) const { return *m_Feat; }

    TQuals& SetQuals(void) { return m_Quals; }

private:
    string               m_Key;
    CConstRef<CFlatLoc>  m_Loc;
    TQuals               m_Quals;
    CConstRef<CSeq_feat> m_Feat;
};


class IFlattishFeature : public IFlatItem
{
public:
    CRef<CFlatFeature> Format(void) const;
    void Format(IFlatFormatter& f) const { f.FormatFeature(*this); }
    bool operator<(const IFlattishFeature& f2) const 
        { return m_Feat->Compare(*f2.m_Feat, *m_Loc, *f2.m_Loc) < 0; }
    string GetKey(void) const
        { return m_Feat->GetData().GetKey(CSeqFeatData::eVocabulary_genbank); }
    const CSeq_loc& GetLoc(void) const { return *m_Loc; }

protected:
    IFlattishFeature(const CSeq_feat& feat, CFlatContext& ctx,
                     const CSeq_loc* loc = 0)
        : m_Feat(&feat), m_Context(&ctx),
          m_Loc(loc ? loc : &feat.GetLocation())
        { }

    virtual void x_AddQuals   (void) const = 0;
    virtual void x_FormatQuals(void) const = 0;

    CConstRef<CSeq_feat>        m_Feat;
    mutable CRef<CFlatContext>  m_Context;
    mutable CConstRef<CSeq_loc> m_Loc;
    mutable CRef<CFlatFeature>  m_FF; ///< populated on demand then cached here
};


class CFlattishFeature : public IFlattishFeature
{
public:
    CFlattishFeature(const CSeq_feat& feat, CFlatContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : IFlattishFeature(feat, ctx, loc), m_IsProduct(is_product)
        { }
    CFlattishFeature(const CMappedFeat& feat, CFlatContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : IFlattishFeature(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_IsProduct(is_product)
        { }

private:
    void x_AddQuals(void) const;
    void x_AddQuals(const CGene_ref& gene) const;
    void x_AddQuals(const CCdregion& cds)  const;
    void x_AddQuals(const CProt_ref& prot) const;
    // ...

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual(EFeatureQualifier slot, const IFlatQV* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQV>(value))); }
    void x_ImportQuals(const CSeq_feat::TQual& quals) const;

    void x_FormatQuals   (void) const;
    void x_FormatQual    (EFeatureQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const
        { x_FormatQual(slot, name, flags | IFlatQV::fIsNote); }

    typedef multimap<EFeatureQualifier, CConstRef<IFlatQV> > TQuals;
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    bool                           m_IsProduct;
};


class CFlattishSourceFeature : public IFlattishFeature
{
public:
    CFlattishSourceFeature(const CSeq_feat& feat, CFlatContext& ctx,
                           const CSeq_loc* loc = 0)
        : IFlattishFeature(feat, ctx, loc), m_WasDesc(false)
        { }
    CFlattishSourceFeature(const CMappedFeat& feat, CFlatContext& ctx,
                           const CSeq_loc* loc = 0)
        : IFlattishFeature(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_WasDesc(false)
        { }
    CFlattishSourceFeature(const CBioSource& src, CFlatContext& ctx);
    CFlattishSourceFeature(const COrg_ref& org,   CFlatContext& ctx);

private:
    void x_AddQuals(void)                  const;
    void x_AddQuals(const CBioSource& src) const;
    void x_AddQuals(const COrg_ref&   org) const;

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual (ESourceQualifier slot, const IFlatQV* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQV>(value))); }

    void x_FormatQuals   (void) const;
    void x_FormatQual    (ESourceQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const;
    void x_FormatNoteQual(ESourceQualifier slot, const string& name,
                          IFlatQV::TFlags flags = 0) const
        { x_FormatQual(slot, name, flags | IFlatQV::fIsNote); }

    typedef multimap<ESourceQualifier, CConstRef<IFlatQV> > TQuals;

    bool           m_WasDesc;
    mutable TQuals m_Quals;
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS


inline
CFlattishSourceFeature::CFlattishSourceFeature(const CBioSource& src,
                                               CFlatContext& ctx)
    : IFlattishFeature(*new CSeq_feat, ctx, &ctx.GetLocation()),
      m_WasDesc(true)
{
    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetBiosrc(const_cast<CBioSource&>(src));
    feat.SetLocation().Assign(*m_Loc);
}


inline
CFlattishSourceFeature::CFlattishSourceFeature(const COrg_ref& org,
                                               CFlatContext& ctx)
    : IFlattishFeature(*new CSeq_feat, ctx, &ctx.GetLocation()),
      m_WasDesc(true)
{
    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetOrg(const_cast<COrg_ref&>(org));
    feat.SetLocation().Assign(*m_Loc);
}


inline
void CFlattishFeature::x_FormatQual(EFeatureQualifier slot, const string& name,
                                    IFlatQV::TFlags flags) const
{
    typedef TQuals::const_iterator TQCI;
    pair<TQCI, TQCI> range
        = const_cast<const TQuals&>(m_Quals).equal_range(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(m_FF->SetQuals(), name, *m_Context, flags);
    }
}


inline
void CFlattishSourceFeature::x_FormatQual(ESourceQualifier slot,
                                          const string& name,
                                          IFlatQV::TFlags flags) const
{
    typedef TQuals::const_iterator TQCI;
    pair<TQCI, TQCI> range
        = const_cast<const TQuals&>(m_Quals).equal_range(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(m_FF->SetQuals(), name, *m_Context,
                           flags | IFlatQV::fIsSource);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2003/10/08 21:10:47  ucko
* Add a couple of accessors to IFlattishFeature for the GFF/GTF formatter.
*
* Revision 1.4  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.3  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_FEATURE__HPP */
