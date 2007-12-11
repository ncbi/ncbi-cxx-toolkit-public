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
#include <objects/seqfeat/Gene_ref.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/format/items/flat_qual_slots.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_FORMAT_EXPORT CFeatHeaderItem : public CFlatItem
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


class NCBI_FORMAT_EXPORT CFlatFeature : public CObject
{
public:
    CFlatFeature(const string& key, const CFlatSeqLoc& loc, const CSeq_feat& feat)
        : m_Key(key), m_Loc(&loc), m_Feat(&feat) { }

    typedef vector<CRef<CFormatQual> > TQuals;

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


class NCBI_FORMAT_EXPORT CFeatureItemBase : public CFlatItem
{
public:
    CConstRef<CFlatFeature> Format(void) const;
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const {
        formatter.FormatFeature(*this, text_os);
    }
    bool operator<(const CFeatureItemBase& f2) const {
        return m_Feat->Compare(*f2.m_Feat, GetLoc(), f2.GetLoc()) < 0; 
    }
    const CSeq_feat& GetFeat(void)  const { return *m_Feat; }
    const CSeq_loc&  GetLoc(void)   const { return *m_Loc; }

    virtual string GetKey(void) const { 
        return m_Feat->GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
    }

protected:

    // constructor
    CFeatureItemBase(const CSeq_feat& feat, CBioseqContext& ctx,
                     const CSeq_loc* loc = 0);

    virtual void x_AddQuals(CBioseqContext& ctx) = 0;
    virtual void x_FormatQuals(CFlatFeature& ff) const = 0;

    CConstRef<CSeq_feat>    m_Feat;
    CConstRef<CSeq_loc>     m_Loc;
};


class NCBI_FORMAT_EXPORT CFeatureItem : public CFeatureItemBase
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
    CFeatureItem(const CSeq_feat& feat, CBioseqContext& ctx,
        const CSeq_loc* loc, EMapped mapped = eMapped_not_mapped);

    // fetaure key (name)
    string GetKey(void) const;

    // mapping
    bool IsMapped           (void) const { return m_Mapped != eMapped_not_mapped;   }
    bool IsMappedFromGenomic(void) const { return m_Mapped == eMapped_from_genomic; }
    bool IsMappedFromCDNA   (void) const { return m_Mapped == eMapped_from_cdna;    }
    bool IsMappedFromProt   (void) const { return m_Mapped == eMapped_from_prot;    }

private:
    typedef CGene_ref::TSyn TGeneSyn;

    void x_GatherInfo(CBioseqContext& ctx);
    //void x_FixLocation(CBioseqContext& ctx);

    bool x_ExceptionIsLegalForFeature() const;
    void x_GetAssociatedGeneInfo( CBioseqContext& ctx, const CGene_ref*&,
        CConstRef<CSeq_feat>& );
    void x_AddQualCitation( CBioseqContext& );
    void x_AddQualExceptions( CBioseqContext& ) const;
    void x_AddQualNote( const CGene_ref*, CConstRef<CSeq_feat> ) const;
    void x_AddQualOldLocusTag( const CGene_ref*, CConstRef<CSeq_feat> ) const;
    void x_AddQualDb( const CGene_ref*, CConstRef<CSeq_feat> ) const;

    // qualifier collection
    void x_AddQuals( CBioseqContext& ctx );
    void x_AddQuals(const CCdregion& cds)  const;
    void x_AddQuals(const CProt_ref& prot) const;
    const TGeneSyn* x_AddGeneQuals(const CSeq_feat& gene, bool& pseudo) const;
    void x_AddCdregionQuals(const CSeq_feat& cds, CBioseqContext& ctx,
        bool& pseudo, bool& had_prot_desc) const;
    const CProt_ref* x_AddProteinQuals(CBioseq_Handle& prot) const;
    void x_AddProductIdQuals(CBioseq_Handle& prod, EFeatureQualifier slot) const;
    void x_AddRnaQuals(const CSeq_feat& feat, CBioseqContext& ctx,
        bool& pseudo) const;
    void x_AddProtQuals(const CSeq_feat& feat, CBioseqContext& ctx,
        bool& pseudo, bool& had_prot_desc, string& precursor_comment) const;
    void x_AddRegionQuals(const CSeq_feat& feat, CBioseqContext& ctx) const;
    void x_AddSiteQuals(const CSeq_feat& feat, CBioseqContext& ctx) const;
    void x_AddBondQuals(const CSeq_feat& feat, CBioseqContext& ctx) const;
    const TGeneSyn* x_AddQuals(const CGene_ref& gene, bool& pseudo,
        CSeqFeatData::ESubtype subtype, bool from_overlap) const;
    void x_AddExtQuals(const CSeq_feat::TExt& ext) const;
    void x_AddGoQuals(const CUser_object& uo) const;
    void x_ImportQuals(CBioseqContext& ctx) const;
    void x_AddRptUnitQual(const string& rpt_unit) const;
    void x_AddRptTypeQual(const string& rpt_type, bool check_qual_syntax) const;
    void x_CleanQuals(bool& had_prot_desc, const TGeneSyn* gene_syn) const;
    const CFlatStringQVal* x_GetStringQual(EFeatureQualifier slot) const;
    CFlatStringListQVal* x_GetStringListQual(EFeatureQualifier slot) const;
    // feature table quals
    typedef vector< CRef<CFormatQual> > TQualVec;
    void x_AddFTableQuals(CBioseqContext& ctx) const;
    bool x_AddFTableGeneQuals(const CSeqFeatData::TGene& gene) const;
    void x_AddFTableRnaQuals(const CSeq_feat& feat, CBioseqContext& ctx) const;
    void x_AddFTableCdregionQuals(const CSeq_feat& feat, CBioseqContext& ctx) const;
    void x_AddFTableProtQuals(const CSeq_feat& prot) const;
    void x_AddFTableRegionQuals(const CSeqFeatData::TRegion& region) const;
    void x_AddFTableBondQuals(const CSeqFeatData::TBond& bond) const;
    void x_AddFTableSiteQuals(const CSeqFeatData::TSite& site) const;
    void x_AddFTablePsecStrQuals(const CSeqFeatData::TPsec_str& psec_str) const;
    void x_AddFTablePsecStrQuals(const CSeqFeatData::THet& het) const;
    void x_AddFTableBiosrcQuals(const CBioSource& src) const;
    void x_AddFTableDbxref(const CSeq_feat::TDbxref& dbxref) const;
    void x_AddFTableExtQuals(const CSeq_feat::TExt& ext) const;
    void x_AddFTableQual(const string& name, const string& val = kEmptyStr) const {
        CFormatQual::EStyle style = val.empty() ? CFormatQual::eEmpty : CFormatQual::eQuoted;
        m_FTableQuals.push_back(CRef<CFormatQual>(new CFormatQual(name, val, style)));
    }
    
    // typdef
    typedef CQualContainer<EFeatureQualifier> TQuals;
    typedef TQuals::iterator                  TQI;
    typedef TQuals::const_iterator            TQCI;
    typedef IFlatQVal::TFlags                 TQualFlags;
    
    // qualifiers container
    void x_AddQual(EFeatureQualifier slot, const IFlatQVal* value) const {
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

    // format
    void x_FormatQuals(CFlatFeature& ff) const;
    void x_FormatNoteQuals(CFlatFeature& ff) const;
    void x_FormatQual(EFeatureQualifier slot, const char* name,
        CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;
    void x_FormatNoteQual(EFeatureQualifier slot, const char* name, 
            CFlatFeature::TQuals& qvec, TQualFlags flags = 0) const;

    // data
    mutable CSeqFeatData::ESubtype m_Type;
    mutable TQuals                 m_Quals;
    mutable TQualVec               m_FTableQuals;
    EMapped                        m_Mapped;
    mutable string                         m_Gene;
};


class NCBI_FORMAT_EXPORT CSourceFeatureItem : public CFeatureItemBase
{
public:
    typedef CRange<TSeqPos> TRange;

    CSourceFeatureItem(const CBioSource& src, TRange range, CBioseqContext& ctx);
    CSourceFeatureItem(const CSeq_feat& feat, CBioseqContext& ctx,
        const CSeq_loc* loc = NULL);
    CSourceFeatureItem(const CMappedFeat& feat, CBioseqContext& ctx,
        const CSeq_loc* loc = NULL);

    bool WasDesc(void) const { return m_WasDesc; }
    const CBioSource& GetSource(void) const {
        return m_Feat->GetData().GetBiosrc();
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
};

//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualNote(
    const CGene_ref* gene_ref,
    CConstRef<CSeq_feat> gene_feat ) const
//
//  For non-gene features, add the /comment qualifier if the necessary info
//  exists.
//  ----------------------------------------------------------------------------
{
    if ( gene_feat && gene_feat->CanGetComment() ) {
        x_AddQual( eFQ_gene_note, new CFlatStringQVal( 
            gene_feat->GetComment() ) );
    }
}

//  ----------------------------------------------------------------------------
inline void CFeatureItem::x_AddQualDb(
    const CGene_ref* gene_ref,
    CConstRef<CSeq_feat> gene_feat ) const
//
//  Add DB qualifiers, if they exist.
//  ----------------------------------------------------------------------------
{
    if ( gene_ref != NULL && gene_ref->CanGetDb() ) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQVal( gene_ref->GetDb() ) );
    }
}
    

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_FEATURE__HPP */
