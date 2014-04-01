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
* Maintainer: Frank Ludwig
*
* File Description:
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objtools/format/items/flat_qual_slots.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//  ============================================================================
class NCBI_FORMAT_EXPORT CFeatHeaderItem : 
    public CFlatItem
//  ============================================================================
{
public:
    CFeatHeaderItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter,
        IFlatTextOStream& text_os) const {
        formatter.FormatFeatHeader(*this, text_os);
    }

    const CSeq_id& GetId(void) const { return *m_Id; }  // for FTable format

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    CConstRef<CSeq_id>  m_Id;  // for FTable format
};


//  ============================================================================
class NCBI_FORMAT_EXPORT CFlatFeature : 
    public CObject
//  ============================================================================
{
public:
    CFlatFeature(const string& key,
                 const CFlatSeqLoc& loc,
                 const CMappedFeat& feat)
        : m_Key(key), m_Loc(&loc), m_Feat(feat) { }

    typedef vector<CRef<CFormatQual> > TQuals;

    const string&      GetKey  (void) const { return m_Key;   }
    const CFlatSeqLoc& GetLoc  (void) const { return *m_Loc;  }
    const TQuals&      GetQuals(void) const { return m_Quals; }
    //const CSeq_feat&   GetFeat (void) const { return m_Feat.GetOriginalFeature(); }
    CMappedFeat        GetFeat (void) const { return m_Feat; }

    TQuals& SetQuals(void) { return m_Quals; }

private:
    string                  m_Key;
    CConstRef<CFlatSeqLoc>  m_Loc;
    TQuals                  m_Quals;
    CMappedFeat             m_Feat;
};


//  ============================================================================
class NCBI_FORMAT_EXPORT CFeatureItemBase: 
    public CFlatItem
//  ============================================================================
{
public:
    CConstRef<CFlatFeature> Format(void) const;
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const {
        formatter.FormatFeature(*this, text_os);
    }
    bool operator<(const CFeatureItemBase& f2) const {
        //return m_Feat.Compare(*f2.m_Feat, GetLoc(), f2.GetLoc()) < 0; 
        return m_Feat < f2.m_Feat;
    }
    CMappedFeat      GetFeat(void)  const { return m_Feat; }
    const CSeq_loc&  GetLoc(void)   const { return *m_Loc; }

    virtual string GetKey(void) const { 
        return m_Feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
    }

protected:

    // constructor
    CFeatureItemBase(const CMappedFeat& feat, CBioseqContext& ctx,
                     CRef<feature::CFeatTree> ftree, const CSeq_loc* loc = 0);

    virtual void x_AddQuals(CBioseqContext& ctx) = 0;
    virtual void x_FormatQuals(CFlatFeature& ff) const = 0;

    CMappedFeat         m_Feat;
    CConstRef<CSeq_loc> m_Loc;
    CRef<feature::CFeatTree> m_Feat_Tree;
};


//  ============================================================================
class NCBI_FORMAT_EXPORT CFeatureItem: 
    public CFeatureItemBase
//  ============================================================================
{
public:
    enum EMapped
    {
        eMapped_not_mapped,
        eMapped_from_genomic,
        eMapped_from_cdna,
        eMapped_from_prot
    };

    // constructors
    CFeatureItem(const CMappedFeat& feat,
                 CBioseqContext& ctx,
                 CRef<feature::CFeatTree> ftree,
                 const CSeq_loc* loc,
                 EMapped mapped = eMapped_not_mapped,
                 CConstRef<CFeatureItem> parentFeatureItem = CConstRef<CFeatureItem>() );

    virtual ~CFeatureItem() {};

    // fetaure key (name)
    string GetKey(void) const;

    // mapping
    bool IsMapped           (void) const { return m_Mapped != eMapped_not_mapped;   }
    bool IsMappedFromGenomic(void) const { return m_Mapped == eMapped_from_genomic; }
    bool IsMappedFromCDNA   (void) const { return m_Mapped == eMapped_from_cdna;    }
    bool IsMappedFromProt   (void) const { return m_Mapped == eMapped_from_prot;    }

protected:
    typedef CGene_ref::TSyn TGeneSyn;

    void x_GatherInfoWithParent(CBioseqContext& ctx, CConstRef<CFeatureItem> parentFeatureItem );

    void x_GetAssociatedProtInfo( CBioseqContext&, CBioseq_Handle&,
        const CProt_ref*&, CMappedFeat& protFeat, CConstRef<CSeq_id>& );
    void x_AddQualPartial( CBioseqContext& );
    void x_AddQualDbXref(
        CBioseqContext& );
    void x_AddQualCitation();
    void x_AddQualExt();
    void x_AddQualExpInv( CBioseqContext& );
    void x_AddQualGeneXref( const CGene_ref*, const CConstRef<CSeq_feat>& );
    void x_AddQualOperon( CBioseqContext&, CSeqFeatData::ESubtype );
    void x_AddQualPseudo( CBioseqContext&, CSeqFeatData::E_Choice, 
        CSeqFeatData::ESubtype, bool );
    void x_AddQualExceptions( CBioseqContext& );
    void x_AddQualNote( CConstRef<CSeq_feat> );
    void x_AddQualOldLocusTag( CConstRef<CSeq_feat> );
    void x_AddQualDb( const CGene_ref* );
    void x_AddQualSeqfeatNote( CBioseqContext & );
    void x_AddQualTranslation( CBioseq_Handle&, CBioseqContext&, bool );
    void x_AddQualTranslationTable( const CCdregion&, CBioseqContext& );
    void x_AddQualCodonStart( const CCdregion&, CBioseqContext& );
    void x_AddQualTranslationException( const CCdregion&, CBioseqContext& );
    void x_AddQualProteinConflict( const CCdregion&, CBioseqContext& );
    void x_AddQualCodedBy( CBioseqContext& );
    void x_AddQualProteinId( CBioseqContext&, const CBioseq_Handle&, CConstRef<CSeq_id> );  
    void x_AddQualProtComment( const CBioseq_Handle& );
    void x_AddQualProtMethod( const CBioseq_Handle& );
    void x_AddQualProtNote( const CProt_ref*, const CMappedFeat& );
    void x_AddQualCdsProduct( CBioseqContext&, const CProt_ref* );
    void x_AddQualProtDesc( const CProt_ref* );
    void x_AddQualProtActivity( const CProt_ref* );
    void x_AddQualProtEcNumber( CBioseqContext&, const CProt_ref* );
    void x_AddQualsGb( CBioseqContext& );
    bool x_GetPseudo(  const CGene_ref* =0, const CSeq_feat* =0 ) const;

    // qualifier collection
    void x_AddQualsCdregion(const CMappedFeat& cds, CBioseqContext& ctx,
        bool pseudo);
    virtual void x_AddQualsRna(const CMappedFeat& feat, CBioseqContext& ctx,
         bool pseudo);
    void x_AddQualsExt( const CSeq_feat::TExt& );
    void x_AddQualsBond( CBioseqContext& );
    void x_AddQualsSite( CBioseqContext& );
    void x_AddQualsRegion( CBioseqContext& );
    void x_AddQualsProt( CBioseqContext&, bool );
    void x_AddQualsPsecStr( CBioseqContext& );
    void x_AddQualsHet( CBioseqContext& ctx );
    void x_AddQualsVariation( CBioseqContext& ctx );

    void x_AddQuals( CBioseqContext& ctx, CConstRef<CFeatureItem> parentFeatureItem );
    void x_AddQuals( CBioseqContext& ctx ) { x_AddQuals( ctx, CConstRef<CFeatureItem>() ); }
    void x_AddQuals(const CProt_ref& prot);
    void x_AddProductIdQuals(CBioseq_Handle& prod, EFeatureQualifier slot);
    void x_AddQualsGene(const CGene_ref*, CConstRef<CSeq_feat>&,
        bool from_overlap);
    void x_AddGoQuals(const CUser_object& uo);
    void x_ImportQuals(CBioseqContext& ctx);
    void x_AddRptUnitQual(const string& rpt_unit);
    void x_AddRptTypeQual(const string& rpt_type, bool check_qual_syntax);
    void x_CleanQuals( const CGene_ref* );
    const CFlatStringQVal* x_GetStringQual(EFeatureQualifier slot) const;
    CFlatStringListQVal* x_GetStringListQual(EFeatureQualifier slot) const;
    CFlatProductNamesQVal * x_GetFlatProductNamesQual(EFeatureQualifier slot) const;
    // feature table quals
    typedef vector< CRef<CFormatQual> > TQualVec;
    void x_AddFTableQuals(CBioseqContext& ctx);
    bool x_AddFTableGeneQuals(const CSeqFeatData::TGene& gene);
    void x_AddFTableRnaQuals(const CMappedFeat& feat, CBioseqContext& ctx);
    void x_AddFTableCdregionQuals(const CMappedFeat& feat, CBioseqContext& ctx);
    void x_AddFTableProtQuals(const CMappedFeat& prot);
    void x_AddFTableRegionQuals(const CSeqFeatData::TRegion& region);
    void x_AddFTableBondQuals(const CSeqFeatData::TBond& bond);
    void x_AddFTableSiteQuals(const CSeqFeatData::TSite& site);
    void x_AddFTablePsecStrQuals(const CSeqFeatData::TPsec_str& psec_str);
    void x_AddFTablePsecStrQuals(const CSeqFeatData::THet& het);
    void x_AddFTableBiosrcQuals(const CBioSource& src);
    void x_AddFTableDbxref(const CSeq_feat::TDbxref& dbxref);
    void x_AddFTableExtQuals(const CSeq_feat::TExt& ext);
    void x_AddFTableQual(const string& name, const string& val = kEmptyStr, 
        CFormatQual::ETrim trim = CFormatQual::eTrim_Normal) 
    {
        CFormatQual::EStyle style = val.empty() ? CFormatQual::eEmpty : CFormatQual::eQuoted;
        m_FTableQuals.push_back(CRef<CFormatQual>(new CFormatQual(name, val, style, 0, trim)));
    }
    
    // typdef
    typedef CQualContainer<EFeatureQualifier> TQuals;
    typedef TQuals::iterator                  TQI;
    typedef TQuals::const_iterator            TQCI;
    typedef IFlatQVal::TFlags                 TQualFlags;
     
    // qualifiers container
    void x_AddQual(EFeatureQualifier slot, const IFlatQVal* value) {
        m_Quals.AddQual(slot, value);
    }
    void x_RemoveQuals(EFeatureQualifier slot) const {
        m_Quals.RemoveQuals(slot);
    }
    bool x_HasQual(EFeatureQualifier slot) const { 
        return m_Quals.HasQual(slot);
    }
    /*pair<TQCI, TQCI> x_GetQual(EFeatureQualifier slot) const {
        return const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    }*/
    TQCI x_GetQual(EFeatureQualifier slot) const {
        return const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    }
    void x_DropIllegalQuals(void) const;
    bool x_IsSeqFeatDataFeatureLegal( CSeqFeatData::EQualifier qual );
    bool x_GetGbValue(
        const string&,
        string& ) const;
    bool x_HasMethodtRNAscanSE(void) const;

    // format
    void x_FormatQuals(CFlatFeature& ff) const;
    void x_FormatNoteQuals(CFlatFeature& ff) const;
    void x_FormatQual(EFeatureQualifier slot, const string & name,
        CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const string & name, 
            CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;
    void x_FormatGOQualCombined( EFeatureQualifier slot, const string & name,
        CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;

    // data
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    mutable TQualVec               m_FTableQuals;
    EMapped                        m_Mapped;
    mutable string                 m_Gene;
    // Note that this holds the gene xref as specified in the original
    // ASN file.  It does NOT hold any genes found by overlap, etc.
    mutable CConstRef<CGene_ref>   m_GeneRef;
};

//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualDb(
    const CGene_ref* gene_ref )
//  ----------------------------------------------------------------------------
{
    if ( ! gene_ref || ! gene_ref->CanGetDb() ) {
        return;
    }
    x_AddQual(eFQ_gene_xref, new CFlatXrefQVal( gene_ref->GetDb() ) );
}
    
//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualCitation()
//  ----------------------------------------------------------------------------
{
    if ( ! m_Feat.IsSetCit() ) {
        return;
    }
    x_AddQual( eFQ_citation, new CFlatPubSetQVal( m_Feat.GetCit() ) );
}

//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualsGb( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if (m_Feat.IsSetQual()) {
        x_ImportQuals(ctx);
    }
}

//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualExt()
//  ----------------------------------------------------------------------------
{
    if ( m_Feat.IsSetExt() ) {
        x_AddQualsExt( m_Feat.GetExt() );
    }
}

//    =============================================================================
class CFeatureItemGff: public CFeatureItem
//    =============================================================================
{
public:
    CFeatureItemGff(
        const CMappedFeat& feat,
        CBioseqContext& ctx,
        const CSeq_loc* loc,
        CRef<feature::CFeatTree> ftree,
        EMapped mapped )
        : CFeatureItem( feat, ctx, ftree, loc, mapped ) {};

    virtual ~CFeatureItemGff() {};

protected:
    virtual void x_AddQualsRna(const CMappedFeat& feat, CBioseqContext& ctx,
         bool pseudo);
};

//  ============================================================================
class NCBI_FORMAT_EXPORT CSourceFeatureItem: 
    public CFeatureItemBase
//  ============================================================================
{
public:
    typedef CRange<TSeqPos> TRange;

    CSourceFeatureItem(const CBioSource& src, TRange range,
        CBioseqContext& ctx, CRef<feature::CFeatTree> ftree);
    CSourceFeatureItem(const CMappedFeat& feat, CBioseqContext& ctx,
        CRef<feature::CFeatTree> ftree, const CSeq_loc* loc = NULL);

    bool WasDesc(void) const { return m_WasDesc; }
    const CBioSource& GetSource(void) const {
        return m_Feat.GetData().GetBiosrc();
    }
    string GetKey(void) const { return "source"; }

    bool IsFocus    (void) const { return m_IsFocus;     }
    bool IsSynthetic(void) const { return m_IsSynthetic; }
    void Subtract(const CSourceFeatureItem& other, CScope& scope);

    void SetLoc(const CSeq_loc& loc);

private:
    typedef CQualContainer<ESourceQualifier> TQuals;
    typedef TQuals::const_iterator           TQCI;
    typedef IFlatQVal::TFlags                TQualFlags;

    void x_GatherInfo(CBioseqContext& ctx);

    void x_AddQuals(CBioseqContext& ctx);
    void x_AddQuals(const CBioSource& src, CBioseqContext& ctx) const;
    void x_AddQuals(const COrg_ref& org, CBioseqContext& ctx) const;
    void x_AddPcrPrimersQuals(const CBioSource& src, CBioseqContext& ctx) const;

    // XXX - massage slot as necessary and perhaps sanity-check value's type
    void x_AddQual (ESourceQualifier slot, const IFlatQVal* value) const {
        m_Quals.AddQual(slot, value); 
    }

    void x_FormatQuals(CFlatFeature& ff) const;
    void x_FormatGBNoteQuals(CFlatFeature& ff) const;
    void x_FormatNoteQuals(CFlatFeature& ff) const;
    void x_FormatQual(ESourceQualifier slot, const string& name,
            CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;
    void x_FormatNoteQual(ESourceQualifier slot, const char* name,
            CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const {
        x_FormatQual(slot, name, qvec, flags | IFlatQVal::fIsNote);
    }

    bool           m_WasDesc;
    mutable TQuals m_Quals;
    bool           m_IsFocus;
    bool           m_IsSynthetic;
    CRef<feature::CFeatTree> m_Feat_Tree;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP */
