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
* Author: Mati Shomrat, NCBI
*
* File Description:
*   Utility class for fleshing out NCBI data model from features.
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/static_map.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/edit/edit_exception.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <objtools/edit/promote.hpp>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Edit


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

USING_SCOPE(sequence);


// constructor
CPromote::CPromote(CBioseq_Handle& seq, TFlags flags, TFeatTypes types) :
    m_Seq(seq), m_Flags(flags), m_Types(types)
{
    if ( !m_Seq ) {
        NCBI_THROW(CEditException, eInvalid, 
            "Cannot initialize with a NULL bioseq handle");
    }
}


// Set / Get promote flags
CPromote::TFlags CPromote::GetFlags(void) const
{
    return m_Flags;
}


void CPromote::SetFlags(TFlags flags)
{
    m_Flags = flags;
}

// Set / Get feature types to promote
CPromote::TFeatTypes CPromote::GetFeatTypes(void) const
{
    return m_Types;
}


void CPromote::SetFeatTypes(TFeatTypes types)
{
    m_Types = types;
}


// Promote features from all attached Seq-annot
void CPromote::PromoteFeatures(void) const
{
    SAnnotSelector sel(CSeq_annot::C_Data::e_Ftable);
    sel.SetResolveNone();

    for (CAnnot_CI it(m_Seq, sel); it; ++it) {
        PromoteFeatures(*it);
    }
}


// Promote features from a specific Seq-annot
void CPromote::PromoteFeatures(const CSeq_annot_Handle& annot) const
{
    _ASSERT(annot);

    // NB: due to lack of feature editing support from the object manger
    // we remove the annotation from scope, edit and then insert it back
    // into scope. (Temporary

    // detach Seq-annot from scope
    CConstRef<CSeq_annot> sap = annot.GetCompleteSeq_annot();
    if ( !sap->GetData().IsFtable() ) {
        NCBI_THROW(CEditException, eInvalid, 
            "Cannot promote a non-Ftable annotation");
    }
    annot.GetEditHandle().Remove();

    // do the actual promotion
    x_PromoteFeatures(const_cast<CSeq_annot&>(*sap));

    // re-attach the annotation
    m_Seq.GetEditHandle().AttachAnnot(const_cast<CSeq_annot&>(*sap));
}


void CPromote::x_PromoteFeatures(CSeq_annot& annot) const
{
    TRnaMap rna_map;

    CSeq_annot::C_Data::TFtable& feats = annot.SetData().SetFtable();

    // if GPS first expand mRNA features into cDNA product sequences
    if ( (m_Flags & fPromote_GenProdSet) != 0  &&  x_DoPromoteRna() ) {
        NON_CONST_ITERATE (CSeq_annot::C_Data::TFtable, it, feats) {
            CSeq_feat& feat = **it;
            if ( feat.CanGetData()  &&  feat.CanGetLocation()  &&
                 feat.GetData().IsRna() ) {
                x_PromoteRna(feat);
                // NB: we build a maping between mRNA to the mRNA feature to be used
                // later when doing coding region promotion.
                // This is done because no obeject manager functionality can be used
                // to locate the MRNA feature, due to the fact that temporarily
                // all feature editing is done outside the object manager.
                if ( feat.CanGetProduct() ) {
                    CBioseq_Handle rna = x_Scope().GetBioseqHandle(feat.GetProduct());
                    if ( rna ) {
                        rna_map[rna] = *it;
                    }
                }
            }
        }
    }

    // promote other feature types
    NON_CONST_ITERATE (CSeq_annot::C_Data::TFtable, it, feats) {    
        CSeq_feat& feat = **it;
        if ( !feat.CanGetData()  ||  !feat.CanGetLocation() ) {
            continue;
        }

        // expand coding region features into protein product sequences
        if ( x_DoPromoteCdregion()  &&  feat.GetData().IsCdregion() ) {
            x_PromoteCdregion(feat, &rna_map);
        }
    
        // promote full lenght pub feature to descriptors
        if ( x_DoPromotePub()  &&  feat.GetData().IsPub() ) {
            x_PromotePub(feat);
        }
    }
}


// Promote a single coding region feature
void CPromote::PromoteCdregion(CSeq_feat_Handle& feat) const
{
    _ASSERT(feat.GetFeatType() == CSeqFeatData::e_Cdregion);

    CSeq_annot_Handle annot = feat.GetAnnot();
    CConstRef<CSeq_annot> sap = annot.GetCompleteSeq_annot();
    annot.GetEditHandle().Remove();
    CConstRef<CSeq_feat> cds = feat.GetSeq_feat();

    x_PromoteCdregion(const_cast<CSeq_feat&>(*cds));

    m_Seq.GetEditHandle().AttachAnnot(const_cast<CSeq_annot&>(*sap));
}


void CPromote::x_PromoteCdregion(CSeq_feat& feat, TRnaMap* rna_map) const
{
    _ASSERT(feat.GetData().IsCdregion());

    CSeq_feat::TXref::iterator it = feat.SetXref().begin();
    while ( it != feat.SetXref().end() ) {
        CSeqFeatXref& xref = **it;
        if ( xref.IsSetData()  &&  xref.GetData().IsProt()  &&
             !feat.CanGetProduct()  &&
             (!feat.IsSetPseudo()  ||  !feat.GetPseudo()) ) {
            CRef<CProt_ref> prp(&xref.SetData().SetProt());
            _ASSERT(prp);
            it = feat.SetXref().erase(it);

            // get protein sequence by translating the coding region
            string data;
            bool include_stop = (m_Flags & fPromote_IncludeStop) != 0;
            bool remove_trailingX = (m_Flags & fPromote_RemoveTrailingX) != 0;
            CCdregion_translate::TranslateCdregion(data,
                                                   feat,
                                                   x_Scope(),
                                                   include_stop,
                                                   remove_trailingX);
            if ( data[data.length() - 1] == '*' ) {
                data.erase(data.length() - 1);
            }

            // find / make the protein's id
            CRef<CSeq_id> pid(x_GetProteinId(feat));
            if ( !pid ) {
                 // !!! Make new protein seq-id
            }
            _ASSERT(pid);

            // create protein bioseq
            CBioseq_EditHandle prot = x_MakeNewProtein(*pid, data);

            //add MolInfo descriptor
            CRef<CSeq_descr> descr(new CSeq_descr);
            CRef<CSeqdesc> desc(x_MakeMolinfoDesc(feat));
            desc->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
            descr->Set().push_back(desc);
            prot.SetDescr(*descr);

            // set feature product
            x_SetSeqFeatProduct(feat, prot);

            // add Prot feature on protein
            const CSeq_loc& loc = feat.GetLocation();
            x_AddProtFeature(
                prot,
                *prp,
                loc.IsPartialStart(eExtreme_Biological),
                loc.IsPartialStop(eExtreme_Biological));

            // find the seq-entry to put the protein in.
            CBioseq_Handle mrna;
            if ( (m_Flags & fPromote_GenProdSet) != 0 ) {
                CRef<CSeq_id> mid(x_GetTranscriptId(feat));
                if ( mid ) {
                    mrna = x_Scope().GetBioseqHandleFromTSE(*mid, m_Seq);
                }
                if ( mrna  &&  (m_Flags & fCopyCdregionTomRNA) != 0 ) {
                    x_CopyCdregionToRNA(feat, mrna, rna_map);
                }
            }
            if ( !mrna ) {
                mrna = m_Seq;
            }
            _ASSERT(mrna);
            CSeq_entry_Handle entry =
                mrna.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
            if ( !entry ) {
                entry = mrna.GetParentEntry();
            }

            // adding a protein might change the structure of the seq-entry,
            // so use AddSeqEntryToSeqEntry
            AddSeqEntryToSeqEntry(entry, prot.GetParentEntry());
        } else {
            ++it;
        }
    }

    if ( feat.GetXref().empty() ) {
        feat.ResetXref();
    }
}


void CPromote::x_CopyCdregionToRNA
(const CSeq_feat& cds,
 CBioseq_Handle& mrna,
 TRnaMap* rna_map) const
{
    _ASSERT(mrna);

    if ( (rna_map == 0)  ||  (rna_map->find(mrna) == rna_map->end()) ) {
        return;
    }

    CSeq_entry_Handle gps = 
        mrna.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
    if ( !gps ) {
        return;
    }

    // create new feature and assign genomic CDS values
    CRef<CSeq_feat> mcds(new CSeq_feat);
    mcds->Assign(cds);

    // create mapper from genomic to mRNA using mRNA feature
    
    CConstRef<CSeq_feat> mfeat = (*rna_map)[mrna];
    if ( !mfeat ) {
        return;
    }
    CSeq_loc_Mapper mapper(*mfeat, CSeq_loc_Mapper::eLocationToProduct, &x_Scope());
    mapper.SetMergeAbutting();

    // update location
    CRef<CSeq_loc> loc = mapper.Map(cds.GetLocation());
    if ( !loc ) {
        return;
    }
    mcds->SetLocation(*loc);

    // update code break locations
    NON_CONST_ITERATE(CCdregion::TCode_break, it, mcds->SetData().SetCdregion().SetCode_break()) {
        CCode_break& cbr = **it;
        cbr.SetLoc(*mapper.Map(cbr.GetLoc()));
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(mcds);
    mrna.GetEditHandle().AttachAnnot(*annot);
}


// Promote a single mRNA feature
void CPromote::PromoteRna(CSeq_feat_Handle& feat) const
{
    _ASSERT(feat.GetFeatType() == CSeqFeatData::e_Rna);

    CSeq_annot_Handle annot = feat.GetAnnot();
    CConstRef<CSeq_annot> sap = annot.GetCompleteSeq_annot();
    annot.GetEditHandle().Remove();
    CConstRef<CSeq_feat> rna = feat.GetSeq_feat();

    x_PromoteRna(const_cast<CSeq_feat&>(*rna));

    m_Seq.GetEditHandle().AttachAnnot(const_cast<CSeq_annot&>(*sap));
}


void CPromote::x_PromoteRna(CSeq_feat& feat) const
{
    // find gen-prod set
    _ASSERT((m_Flags & fPromote_GenProdSet) != 0);
    CSeq_entry_Handle gps = 
        m_Seq.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
    if ( !gps ) {
        return;
    }
    _ASSERT(gps);
    
    // find the mRNA id
    CRef<CSeq_id> sip;
    if ( !feat.IsSetProduct()  &&  (!feat.IsSetPseudo()  ||  !feat.GetPseudo()) ) {
        sip.Reset(x_GetTranscriptId(feat));
    }
    if ( !sip ) {
        return;
    }
    
    // get the actual mRNA sequence
    CSeqVector rnaseq(feat.GetLocation(), m_Seq.GetScope());
    string data;
    rnaseq.GetSeqData(0, rnaseq.size(), data);

    // create mRNA bioseq
    CBioseq_EditHandle mrna =
        x_MakeNewRna(*sip, data, rnaseq.GetCoding(), rnaseq.size());

    // add MolInfo descriptor
    CRef<CSeq_descr> descr(new CSeq_descr);
    CRef<CSeqdesc> desc(x_MakeMolinfoDesc(feat));
    descr->Set().push_back(desc);
    mrna.SetDescr(*descr);

    // set feature pruduct
    x_SetSeqFeatProduct(feat, mrna);

    // move mRNA to genomic product set
    mrna.MoveTo(gps.GetEditHandle());
}


// Prompte a single publication feature
void CPromote::PromotePub(CSeq_feat_Handle& feat) const
{
    _ASSERT(feat.GetFeatType() == CSeqFeatData::e_Pub);

    CSeq_annot_Handle annot = feat.GetAnnot();
    CConstRef<CSeq_annot> sap = annot.GetCompleteSeq_annot();
    annot.GetEditHandle().Remove();
    CConstRef<CSeq_feat> rna = feat.GetSeq_feat();

    x_PromotePub(const_cast<CSeq_feat&>(*rna));

    m_Seq.GetEditHandle().AttachAnnot(const_cast<CSeq_annot&>(*sap));
}


void CPromote::x_PromotePub(CSeq_feat& feat) const
{
    // !!!
}


CBioseq_EditHandle CPromote::x_MakeNewBioseq
(CSeq_id& id,
 CSeq_inst::TMol mol,
 const string& data,
 CSeq_data::E_Choice code,
 size_t length) const
{
    CRef<CBioseq> bsp(new CBioseq);
    bsp->SetId().push_back(CRef<CSeq_id>(&id));
    CBioseq_EditHandle seq = x_Scope().AddBioseq(*bsp).GetEditHandle();
    _ASSERT(seq);

    // set repr, mol, length and id
    seq.SetInst_Repr(CSeq_inst::eRepr_raw);
    seq.SetInst_Mol(mol);

    // set actual sequence and length
    CRef<CSeq_data> sdata(new CSeq_data(data, code));
    seq.SetInst_Seq_data(*sdata);
    seq.SetInst_Length(length);

    return seq;
}


CBioseq_EditHandle CPromote::x_MakeNewRna
(CSeq_id& id,
 const string& data,
 CSeq_data::E_Choice code,
 size_t length) const
{
    return x_MakeNewBioseq(id, CSeq_inst::eMol_rna, data, code, length);
}


CBioseq_EditHandle CPromote::x_MakeNewProtein(CSeq_id& id, const string& data) const
{
    return x_MakeNewBioseq(id, CSeq_inst::eMol_aa, data, 
                           CSeq_data::e_Ncbieaa, data.length());
}


CSeq_id* CPromote::x_GetProductId(CSeq_feat& feat, const string& qval) const
{
    static const string kRNA = "RNA";
    static const string kCDS = "CDS";
    const string* ftype = feat.GetData().IsRna() ? &kRNA : &kCDS;

    CRef<CSeq_id> sip;
    string id;
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while ( it != feat.SetQual().end() ) {
        const CGb_qual& qual = **it;
        if ( qual.IsSetQual()  &&  qual.GetQual() == qval  &&
             qual.IsSetVal()  &&  !qual.GetVal().empty() ) {
            if ( !id.empty() ) {
                LOG_POST_X(1, Warning << *ftype << " " << qval << " " 
                              << qual.GetVal() << " replacing " << id);
            }
            id = qual.GetVal();
            it = feat.SetQual().erase(it);
        } else {
            ++it;
        }
    }
    if ( feat.GetQual().empty() ) {
        feat.ResetQual();
    }
    if ( !id.empty() ) {
        try {
            sip.Reset(new CSeq_id(id));
        } catch (...) {}
    }

    return sip.ReleaseOrNull();
}


CSeq_id* CPromote::x_GetTranscriptId(CSeq_feat& feat) const
{
    return x_GetProductId(feat, "transcript_id");
}


CSeq_id* CPromote::x_GetProteinId(CSeq_feat& feat) const
{
    return x_GetProductId(feat, "protein_id");
}


typedef SStaticPair<CSeqFeatData::ESubtype, CMolInfo::TBiomol> TBiomolPair;
typedef CStaticPairArrayMap<CSeqFeatData::ESubtype, CMolInfo::TBiomol> TBiomolMap;
static const TBiomolPair kBiomolMap[] = {
    // for Cdregion feature
    { CSeqFeatData::eSubtype_cdregion, CMolInfo::eBiomol_peptide },
    // for RNA features
    { CSeqFeatData::eSubtype_preRNA,   CMolInfo::eBiomol_pre_RNA },
    { CSeqFeatData::eSubtype_mRNA,     CMolInfo::eBiomol_mRNA },
    { CSeqFeatData::eSubtype_tRNA,     CMolInfo::eBiomol_tRNA },
    { CSeqFeatData::eSubtype_rRNA,     CMolInfo::eBiomol_rRNA },
    { CSeqFeatData::eSubtype_snRNA,    CMolInfo::eBiomol_snRNA },
    { CSeqFeatData::eSubtype_scRNA,    CMolInfo::eBiomol_scRNA },
    { CSeqFeatData::eSubtype_snoRNA,   CMolInfo::eBiomol_mRNA },
    { CSeqFeatData::eSubtype_otherRNA, CMolInfo::eBiomol_transcribed_RNA }
};
DEFINE_STATIC_ARRAY_MAP(TBiomolMap, sc_BiomolMap, kBiomolMap);


CSeqdesc* CPromote::x_MakeMolinfoDesc(const CSeq_feat& feat) const
{
    // create new MolInfo descriptor
    CRef<CSeqdesc> desc(new CSeqdesc);
    CSeqdesc::TMolinfo& mi = desc->SetMolinfo();

    // set biomol
    TBiomolMap::const_iterator it = 
        sc_BiomolMap.find(feat.GetData().GetSubtype());
    if ( it != sc_BiomolMap.end() ) {
        mi.SetBiomol(it->second);
    }

    // set completeness
    bool partial_left  = feat.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial_right = feat.GetLocation().IsPartialStop(eExtreme_Biological);
    SetMolInfoCompleteness(mi, partial_left, partial_right);

    return desc.Release();
}


void CPromote::x_SetSeqFeatProduct(CSeq_feat& feat, const CBioseq_Handle& prod) const
{
    _ASSERT(prod);

    CConstRef<CSeq_id> prod_id = prod.GetSeqId();
    feat.SetProduct().SetWhole().Assign(*prod_id);    
}


bool CPromote::x_DoPromoteCdregion(void) const
{
    return (m_Types & eFeatType_Cdregion) != 0;
}


bool CPromote::x_DoPromoteRna(void) const
{
    return (m_Types & eFeatType_Rna) != 0;
}


bool CPromote::x_DoPromotePub(void) const
{
    return (m_Types & eFeatType_Pub) != 0;
}


void CPromote::x_AddProtFeature(CBioseq_EditHandle pseq, CProt_ref& prp,
        bool partial_left, bool partial_right) const
{
    // creat new Prot feature
    CRef<CSeq_feat> prot(new CSeq_feat);
    prot->SetData().SetProt(prp);
    
    // set feature location
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole().Assign(*pseq.GetSeqId());
    loc->SetPartialStart(partial_left, eExtreme_Biological);
    loc->SetPartialStop(partial_right, eExtreme_Biological);
    prot->SetLocation(*loc);

    // create new Ftable annotation and insert feature
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(prot);

    // add annotation to sequence
    pseq.AttachAnnot(*annot);
}

CScope& CPromote::x_Scope(void) const
{
    return m_Seq.GetScope();
}


/////////////////////////////////////////////////////////////////////////////
// static functions

// Promote features from all attached Seq-annots
void PromoteFeatures
(CBioseq_Handle& seq,
 CPromote::TFlags flags,
 CPromote::TFeatTypes types)
{
    CPromote(seq, flags, types).PromoteFeatures();
}


// Promote features from a specific Seq-annot
void PromoteFeatures
(CBioseq_Handle& seq,
 CSeq_annot_Handle& annot,
 CPromote::TFlags flags,
 CPromote::TFeatTypes types)
{
    CPromote(seq, flags, types).PromoteFeatures(annot);
}


// Promote a single coding region feature
void PromoteCdregion(CBioseq_Handle& seq, CSeq_feat_Handle& feat,
                     bool include_stop, bool remove_trailingX)
{
    CPromote::TFlags flags = 0;
    if ( include_stop ) {
        flags |= CPromote::fPromote_IncludeStop;
    }
    if ( remove_trailingX ) {
        flags |= CPromote::fPromote_RemoveTrailingX;
    }
    CPromote(seq, flags, CPromote::eFeatType_Cdregion).PromoteCdregion(feat);
}


// Promote a single mRNA feature
void PromoteRna(CBioseq_Handle& seq, CSeq_feat_Handle& feat)
{
    CPromote(seq, 0, CPromote::eFeatType_Rna).PromoteRna(feat);
}


// Prompte a single publication feature
void PromotePub(CBioseq_Handle& seq, CSeq_feat_Handle& feat)
{
    CPromote(seq, 0, CPromote::eFeatType_Pub).PromotePub(feat);
}


bool SetMolInfoCompleteness (CMolInfo& mi, bool partial5, bool partial3)
{
    bool changed = false;
    CMolInfo::ECompleteness new_val;
    if ( partial5  &&  partial3 ) {
        new_val = CMolInfo::eCompleteness_no_ends;
    } else if ( partial5 ) {
        new_val = CMolInfo::eCompleteness_no_left;
    } else if ( partial3 ) {
        new_val = CMolInfo::eCompleteness_no_right;
    } else {
        new_val = CMolInfo::eCompleteness_complete;
    }
    if (!mi.IsSetCompleteness() || mi.GetCompleteness() != new_val) {
        mi.SetCompleteness(new_val);
        changed = true;
    }
    return changed;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
