#ifndef OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP
#define OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP

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
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/format/items/flat_qual_slots.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFeatHeaderItem : public CFlatItem
{
public:
    CFeatHeaderItem(CFFContext& ctx) : CFlatItem(ctx) {}
    void Format(IFormatter& formatter,
        IFlatTextOStream& text_os) const {
        formatter.FormatFeatHeader(*this, text_os);
    }
private:
    void x_GatherInfo(CFFContext& ctx) {}
};


class CFlatFeature : public CObject
{
public:
    CFlatFeature(const string& key, const CFlatSeqLoc& loc, const CSeq_feat& feat)
        : m_Key(key), m_Loc(&loc), m_Feat(&feat) { }

    typedef vector<CRef<CFlatQual> > TQuals;

    const string&      GetKey  (void) const { return m_Key;   }
    const CFlatSeqLoc& GetLoc  (void) const { return *m_Loc;  }
    const TQuals&      GetQuals(void) const { return m_Quals; }
    const CSeq_feat&   GetFeat (void) const { return *m_Feat; }

    TQuals& SetQuals(void) { return m_Quals; }

private:
    string                  m_Key;
    CConstRef<CFlatSeqLoc>  m_Loc;
    TQuals                  m_Quals;
    CConstRef<CSeq_feat>    m_Feat;
};


class CFeatureItemBase : public CFlatItem
{
public:
    CRef<CFlatFeature> Format(void) const;
    void Format(IFormatter& formatter,
        IFlatTextOStream& text_os) const {
        formatter.FormatFeature(*this, text_os);
    }
    bool operator<(const CFeatureItemBase& f2) const 
        { return m_Feat->Compare(*f2.m_Feat, *m_Loc, *f2.m_Loc) < 0; }

    const CSeq_feat& GetFeat(void)  const { return *m_Feat; }
    const CSeq_loc&  GetLoc(void)   const { return *m_Loc; }
    virtual string GetKey(void) const
        { return m_Feat->GetData().GetKey(CSeqFeatData::eVocabulary_genbank); }

protected:

    // constructor
    CFeatureItemBase(const CSeq_feat& feat, CFFContext& ctx,
                     const CSeq_loc* loc = 0)
        : CFlatItem(ctx), m_Feat(&feat), m_Loc(loc ? loc : &feat.GetLocation())
    {}

    virtual void x_AddQuals   (CFFContext& ctx) const = 0;
    virtual void x_FormatQuals(void) const = 0;

    CConstRef<CSeq_feat>    m_Feat;
    CConstRef<CSeq_loc>     m_Loc;

    mutable CRef<CFlatFeature>  m_FF; ///< populated on demand then cached here
};


class CFeatureItem : public CFeatureItemBase
{
public:
    // constructors
    CFeatureItem(const CSeq_feat& feat, CFFContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : CFeatureItemBase(feat, ctx, loc), m_IsProduct(is_product)
    {
        x_GatherInfo(ctx);
    }
    CFeatureItem(const CMappedFeat& feat, CFFContext& ctx,
                     const CSeq_loc* loc = 0, bool is_product = false)
        : CFeatureItemBase(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_IsProduct(is_product)
    {
        x_GatherInfo(ctx);
    }

private:
    void x_GatherInfo(CFFContext& ctx);

    void x_AddQuals(CFFContext& ctx)       const;
    void x_AddQuals(const CGene_ref& gene) const;
    void x_AddQuals(const CCdregion& cds)  const;
    void x_AddQuals(const CProt_ref& prot) const;
    // ...

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual(EFeatureQualifier slot, const IFlatQVal* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQVal>(value))); }
    void x_ImportQuals(const CSeq_feat::TQual& quals) const;

    void x_FormatQuals   (void) const;
    void x_FormatQual(EFeatureQualifier slot, const string& name,
        IFlatQVal::TFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const string& name, 
                          IFlatQVal::TFlags flags = 0) const 
        { x_FormatQual(slot, name, flags | IFlatQVal::fIsNote); }

    typedef multimap<EFeatureQualifier, CConstRef<IFlatQVal> > TQuals;
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    bool                           m_IsProduct;
};


class CSourceFeatureItem : public CFeatureItemBase
{
public:
    typedef CRange<TSeqPos> TRange;

    CSourceFeatureItem(const CBioSource& src, TRange range, CFFContext& ctx);
    CSourceFeatureItem(const CSeq_feat& feat, CFFContext& ctx,
                           const CSeq_loc* loc = 0)
        : CFeatureItemBase(feat, ctx, loc), m_WasDesc(false)
    {
        x_GatherInfo(ctx);
    }
    CSourceFeatureItem(const CMappedFeat& feat, CFFContext& ctx,
                           const CSeq_loc* loc = 0)
        : CFeatureItemBase(feat.GetOriginalFeature(), ctx,
                           loc ? loc : &feat.GetLocation()),
          m_WasDesc(false)
    {
        x_GatherInfo(ctx);
    }

    bool WasDesc(void) const { return m_WasDesc; }
    const CBioSource& GetSource(void) const {
        return m_Feat->GetData().GetBiosrc();
    }
    string GetKey(void) const { return "source"; }

private:
    void x_GatherInfo(CFFContext& ctx) {
        x_AddQuals(ctx);
    }

    void x_AddQuals(CFFContext& ctx)       const;
    void x_AddQuals(const CBioSource& src, CFFContext& ctx) const;
    void x_AddQuals(const COrg_ref& org, CFFContext& ctx) const;

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual (ESourceQualifier slot, const IFlatQVal* value) const
        { m_Quals.insert(TQuals::value_type(slot,CConstRef<IFlatQVal>(value))); }

    void x_FormatQuals   (void) const;
    void x_FormatGBNoteQuals(void) const;
    void x_FormatNoteQuals(void) const;
    void x_FormatQual(ESourceQualifier slot, const string& name,
        CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const;
    void x_FormatNoteQual(ESourceQualifier slot, const string& name,
            CFlatFeature::TQuals& qvec, IFlatQVal::TFlags flags = 0) const {
        x_FormatQual(slot, name, qvec, flags | IFlatQVal::fIsNote); 
    }

    typedef multimap<ESourceQualifier, CConstRef<IFlatQVal> > TQuals;

    bool           m_WasDesc;
    mutable TQuals m_Quals;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/02/11 16:38:51  shomrat
* added methods for gathering and formatting of source features
*
* Revision 1.2  2004/01/14 15:57:23  shomrat
* removed commented code
*
* Revision 1.1  2003/12/17 19:46:43  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP */
