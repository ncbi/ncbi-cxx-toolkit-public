/* $Id$
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
 * Author:  Robert G. Smith
 *
 * File Description:
 *   Implementation of BasicCleanup for Seq-feat and sub-objects.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <util/static_map.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <vector>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_annot_ci.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::



// ==========================================================================
//                             CSeq_feat Cleanup section
// ==========================================================================

void CCleanup_imp::x_CleanupExcept_text(string& except_text)
{
    if (NStr::Find(except_text, "ribosome slippage") == NPOS     &&
        NStr::Find(except_text, "trans splicing") == NPOS        &&
        NStr::Find(except_text, "alternate processing") == NPOS  &&
        NStr::Find(except_text, "adjusted for low quality genome") == NPOS  &&
        NStr::Find(except_text, "non-consensus splice site") == NPOS &&
        NStr::Find(except_text, "reasons cited in publication") == NPOS) {
        return;
    }

    vector<string> exceptions;
    NStr::Tokenize(except_text, ",", exceptions);

    NON_CONST_ITERATE(vector<string>, it, exceptions) {
        string& text = *it;
        size_t tlen = text.length();
        NStr::TruncateSpacesInPlace(text);
        if (text.length() != tlen) {
            ChangeMade(CCleanupChange::eTrimSpaces);
        }
        if (!text.empty()) {
            if (text == "ribosome slippage") {
                text = "ribosomal slippage";
                ChangeMade(CCleanupChange::eChangeException);
            } else if (text == "trans splicing") {
                text = "trans-splicing";
                ChangeMade(CCleanupChange::eChangeException);
            } else if (text == "alternate processing") {
                text = "alternative processing";
                ChangeMade(CCleanupChange::eChangeException);
            } else if (text == "adjusted for low quality genome") {
                text = "adjusted for low-quality genome";
                ChangeMade(CCleanupChange::eChangeException);
            } else if (text == "non-consensus splice site") {
                text = "nonconsensus splice site";
                ChangeMade(CCleanupChange::eChangeException);
            } else if (NStr::Equal(except_text, "reasons cited in publication")) {
                text = "reasons given in citation";
                ChangeMade(CCleanupChange::eChangeException);
            }
        }
    }
    except_text = NStr::Join(exceptions, ",");
}


// === Gene

static bool s_IsEmptyGeneRef(const CGene_ref& gref)
{
    return (!gref.IsSetLocus()  &&  !gref.IsSetAllele()  &&
        !gref.IsSetDesc()  &&  !gref.IsSetMaploc()  &&  !gref.IsSetDb()  &&
        !gref.IsSetSyn()  &&  !gref.IsSetLocus_tag());
}


// === Site

typedef pair<const string, CSeqFeatData::TSite>  TSiteElem;
static const TSiteElem sc_site_map[] = {
    TSiteElem("acetylation", CSeqFeatData::eSite_acetylation),
    TSiteElem("active", CSeqFeatData::eSite_active),
    TSiteElem("amidation", CSeqFeatData::eSite_amidation),
    TSiteElem("binding", CSeqFeatData::eSite_binding),
    TSiteElem("blocked", CSeqFeatData::eSite_blocked),
    TSiteElem("cleavage", CSeqFeatData::eSite_cleavage),
    TSiteElem("dna binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("dna-binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("gamma carboxyglutamic acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("gamma-carboxyglutamic-acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("glycosylation", CSeqFeatData::eSite_glycosylation),
    TSiteElem("hydroxylation", CSeqFeatData::eSite_hydroxylation),
    TSiteElem("inhibit", CSeqFeatData::eSite_inhibit),
    TSiteElem("lipid binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("lipid-binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("metal binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("metal-binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("methylation", CSeqFeatData::eSite_methylation),
    TSiteElem("modified", CSeqFeatData::eSite_modified),
    TSiteElem("mutagenized", CSeqFeatData::eSite_mutagenized),
    TSiteElem("myristoylation", CSeqFeatData::eSite_myristoylation),
    TSiteElem("np binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("np-binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("oxidative deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("oxidative-deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("phosphorylation", CSeqFeatData::eSite_phosphorylation),
    TSiteElem("pyrrolidone carboxylic acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("pyrrolidone-carboxylic-acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("signal peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("signal-peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("sulfatation", CSeqFeatData::eSite_sulfatation),
    TSiteElem("transit peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transit-peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transmembrane region", CSeqFeatData::eSite_transmembrane_region),
    TSiteElem("transmembrane-region", CSeqFeatData::eSite_transmembrane_region)
};
typedef CStaticArrayMap<const string, CSeqFeatData::TSite>   TSiteMap;
DEFINE_STATIC_ARRAY_MAP(TSiteMap, sc_SiteMap, sc_site_map);


// === Prot

static const string uninf_names[] = {
    "peptide", "putative", "signal", "signal peptide", "signal-peptide",
    "signal_peptide", "transit", "transit peptide", "transit-peptide",
    "transit_peptide", "unknown", "unnamed"
};
typedef CStaticArraySet<string, PNocase> TUninformative;
DEFINE_STATIC_ARRAY_MAP(TUninformative, sc_UninfNames, uninf_names);

static bool s_IsInformativeName(const string& name)
{
    return sc_UninfNames.find(name) == sc_UninfNames.end();
}



// === Imp

void CCleanup_imp::x_AddReplaceQual(CSeq_feat& feat, const string& str)
{
    if (!NStr::EndsWith(str, ')')) {
        return;
    }

    SIZE_TYPE start = str.find_first_of('\"');
    if (start != NPOS) {
        SIZE_TYPE end = str.find_first_of('\"', start + 1);
        if (end != NPOS) {
            feat.AddQualifier("replace", str.substr(start + 1, end));
            ChangeMade(CCleanupChange::eChangeQualifiers);
        }
    }
}

typedef pair<const string, CRNA_ref::TType> TRnaTypePair;
static const TRnaTypePair sc_rna_type_map[] = {
    TRnaTypePair("mRNA", CRNA_ref::eType_premsg),
    TRnaTypePair("misc_RNA", CRNA_ref::eType_other),
    TRnaTypePair("precursor_RNA", CRNA_ref::eType_premsg),
    TRnaTypePair("rRNA", CRNA_ref::eType_tRNA),
    TRnaTypePair("scRNA", CRNA_ref::eType_scRNA),
    TRnaTypePair("snRNA", CRNA_ref::eType_snRNA),
    TRnaTypePair("snoRNA", CRNA_ref::eType_snoRNA),
    TRnaTypePair("tRNA", CRNA_ref::eType_mRNA)
};
typedef CStaticArrayMap<const string, CRNA_ref::TType> TRnaTypeMap;
DEFINE_STATIC_ARRAY_MAP(TRnaTypeMap, sc_RnaTypeMap, sc_rna_type_map);


// === Seq-feat.data

void CCleanup_imp::BasicCleanup(CSeqFeatData& data) 
{    
    // basic localized cleanup of kinds of CSeqFeatData.
    switch (data.Which()) {
        case CSeqFeatData::e_Gene:
            BasicCleanup(data.SetGene());
            break;
        case CSeqFeatData::e_Org:
            BasicCleanup(data.SetOrg());
            break;
        case CSeqFeatData::e_Cdregion:
            break;
        case CSeqFeatData::e_Prot:
            BasicCleanup(data.SetProt());
            break;
        case CSeqFeatData::e_Rna:
            BasicCleanup(data.SetRna());
            break;
        case CSeqFeatData::e_Pub:
            BasicCleanup(data.SetPub());
            break;
        case CSeqFeatData::e_Seq:
            break;
        case CSeqFeatData::e_Imp:
            BasicCleanup(data.SetImp());
            break;
        case CSeqFeatData::e_Region:
            break;
        case CSeqFeatData::e_Comment:
            break;
        case CSeqFeatData::e_Bond:
            break;
        case CSeqFeatData::e_Site:
            break;
        case CSeqFeatData::e_Rsite:
            break;
        case CSeqFeatData::e_User:
            BasicCleanup(data.SetUser());
            break;
        case CSeqFeatData::e_Txinit:
            break;
        case CSeqFeatData::e_Num:
            break;
        case CSeqFeatData::e_Psec_str:
            break;
        case CSeqFeatData::e_Non_std_residue:
            break;
        case CSeqFeatData::e_Het:
            break;
        case CSeqFeatData::e_Biosrc:
            BasicCleanup(data.SetBiosrc());
            break;
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CSeq_feat& feat, CSeqFeatData& data) 
{
    // change things in the feat based on what is in data and vice versa.
    // does not call any other BasicCleanup routine.

    switch (data.Which()) {
    case CSeqFeatData::e_Gene:
        {
            CSeqFeatData::TGene& gene = data.SetGene();
            
            // remove feat.comment if equal to gene.locus
            if (gene.IsSetLocus()  &&  feat.IsSetComment()) {
                if (feat.GetComment() == gene.GetLocus()) {
                    feat.ResetComment();
                    ChangeMade(CCleanupChange::eChangeComment);
                }
            }
                
            // move Gene-ref.db to the Seq-feat.dbxref
            if (gene.IsSetDb()) {
                copy(gene.GetDb().begin(), gene.GetDb().end(), back_inserter(feat.SetDbxref()));
                gene.ResetDb();
                ChangeMade(CCleanupChange::eChangeDbxrefs);
            }
            
            // move db_xref from gene xrefs to feature
            if (feat.IsSetXref()) {
                CSeq_feat::TXref& xrefs = feat.SetXref();
                CSeq_feat::TXref::iterator it = xrefs.begin();
                while (it != xrefs.end()) {
                    CSeqFeatXref& xref = **it;
                    if (xref.IsSetData()  &&  xref.GetData().IsGene()) {
                        CGene_ref& gref = xref.SetData().SetGene();
                        if (gref.IsSetDb()) {
                            copy(gref.GetDb().begin(), gref.GetDb().end(), 
                                 back_inserter(feat.SetDbxref()));
                            gref.ResetDb();
                            ChangeMade(CCleanupChange::eChangeDbxrefs);
                        }
                        // remove gene xref if it has no values set
                        if (s_IsEmptyGeneRef(gref)) {
                            it = xrefs.erase(it);
                            ChangeMade(CCleanupChange::eChangeDbxrefs);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Org:
        break;
    case CSeqFeatData::e_Cdregion:
        break;
    case CSeqFeatData::e_Prot:
        {
            CSeq_feat::TData::TProt& prot = data.SetProt();
            
            if (prot.IsSetProcessed()  &&  prot.IsSetName()) {
                CProt_ref::TProcessed processed = prot.GetProcessed();
                CProt_ref::TName& name = prot.SetName();
                if (processed == CProt_ref::eProcessed_signal_peptide  ||
                    processed == CProt_ref::eProcessed_transit_peptide) {
                    CProt_ref::TName::iterator it = name.begin();
                    while (it != name.end()) {
                        if (!feat.IsSetComment()) {
                            if (NStr::Find(*it, "putative") != NPOS  ||
                                NStr::Find(*it, "put. ") != NPOS) {
                                feat.SetComment("putative");
                                ChangeMade(CCleanupChange::eChangeComment);
                            }
                        }
                        // remove uninformative names
                        if (!s_IsInformativeName(*it)) {
                            it = name.erase(it);
                            ChangeMade(CCleanupChange::eChangeQualifiers);
                        } else {
                            ++it;
                        }
                    }
                }
            }
                
            // move Prot-ref.db to Seq-feat.dbxref
            if (prot.IsSetDb()) {
                copy(prot.GetDb().begin(), prot.GetDb().end(),
                     back_inserter(feat.SetDbxref()));
                prot.ResetDb();
                ChangeMade(CCleanupChange::eChangeDbxrefs);
            }
        }
        break;
    case CSeqFeatData::e_Rna:
        break;
    case CSeqFeatData::e_Pub:
        break;
    case CSeqFeatData::e_Seq:
        break;
    case CSeqFeatData::e_Imp:
        {
            CSeqFeatData::TImp& imp = data.SetImp();
            
            if (imp.IsSetLoc()  &&  (NStr::Find(imp.GetLoc(), "replace") != NPOS)) {
                x_AddReplaceQual(feat, imp.GetLoc());
                imp.ResetLoc();
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
            
/*
            if (imp.IsSetKey()) {
                const CImp_feat::TKey& key = imp.GetKey();
                
                if (key == "CDS") {

                    if ( ! (m_Mode == eCleanup_EMBL  ||  m_Mode == eCleanup_DDBJ) ) {
                        data.SetCdregion();
                        ChangeMade(CCleanupChange::eChangeFeatureKey);
                        //s_CleanupCdregion(feat);
                    }
                } else if (!imp.IsSetLoc()  ||  NStr::IsBlank(imp.GetLoc())) {
                    TRnaTypeMap::const_iterator rna_type_it = sc_RnaTypeMap.find(key);
                    if (rna_type_it != sc_RnaTypeMap.end()) {
                        CSeqFeatData::TRna& rna = data.SetRna();
                        ChangeMade(CCleanupChange::eChangeFeatureKey);
                        rna.SetType(rna_type_it->second);
                        BasicCleanup(rna);
                    } else {
                        // !!! need to find protein bioseq without object manager
                    }
                }
            }
*/
        }
        break;
    case CSeqFeatData::e_Region:
        {            
            string &region = data.SetRegion();
            if (CleanString(region)) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            if (ConvertDoubleQuotes(region)) {
                ChangeMade(CCleanupChange::eCleanDoubleQuotes);
            }
/*
            if (region.empty()) {
                feat.SetData().SetComment();
                ChangeMade(CCleanupChange::eChangeFeatureKey);
            }
 */
        }
        break;
    case CSeqFeatData::e_Comment:
        break;
    case CSeqFeatData::e_Bond:
        break;
    case CSeqFeatData::e_Site:
        {
            CSeqFeatData::TSite site = data.GetSite();
            if (feat.IsSetComment()  &&
                (site == CSeqFeatData::TSite(0)  ||  site == CSeqFeatData::eSite_other)) {
                const string& comment = feat.GetComment();
                ITERATE (TSiteMap, it, sc_SiteMap) {
                    if (NStr::StartsWith(comment, it->first, NStr::eNocase)) {
                        feat.SetData().SetSite(it->second);
                        if (NStr::IsBlank(comment, it->first.length())  ||
                            NStr::EqualNocase(comment, it->first.length(), NPOS, " site")) {
                            feat.ResetComment();
                            ChangeMade(CCleanupChange::eChangeComment);
                        }
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Rsite:
        break;
    case CSeqFeatData::e_User:
        break;
    case CSeqFeatData::e_Txinit:
        break;
    case CSeqFeatData::e_Num:
        break;
    case CSeqFeatData::e_Psec_str:
        break;
    case CSeqFeatData::e_Non_std_residue:
        break;
    case CSeqFeatData::e_Het:
        break;
    case CSeqFeatData::e_Biosrc:
        break;
    default:
        break;
    }
}


void CCleanup_imp::BasicCleanup(const CSeq_feat_Handle& sfh)
{
    CSeq_feat& feat = const_cast<CSeq_feat&> (*sfh.GetSeq_feat());
    
    CSeqFeatData& data = feat.SetData();
    switch (data.Which()) {
    case CSeqFeatData::e_Imp:
        {
            CSeqFeatData::TImp& imp = data.SetImp();
            
            if (imp.IsSetKey()) {
                const CImp_feat::TKey& key = imp.GetKey();
                
                if (key == "CDS") {
                    if ( ! (m_Mode == eCleanup_EMBL  ||  m_Mode == eCleanup_DDBJ) ) {
                        data.SetCdregion();
                        ChangeMade(CCleanupChange::eChangeFeatureKey);
                    }
                } else if (!imp.IsSetLoc()  ||  NStr::IsBlank(imp.GetLoc())) {
                    TRnaTypeMap::const_iterator rna_type_it = sc_RnaTypeMap.find(key);
                    if (rna_type_it != sc_RnaTypeMap.end()) {
                        CSeqFeatData::TRna& rna = data.SetRna();
                        ChangeMade(CCleanupChange::eChangeFeatureKey);
                        rna.SetType(rna_type_it->second);
                    } else {
                        // !!! need to find protein bioseq with object manager
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Region:
        {            
            string &region = data.SetRegion();
            if (CleanString(region)) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            if (region.empty()) {
                feat.SetData().SetComment();
                ChangeMade(CCleanupChange::eChangeFeatureKey);
            }
        }
        break;
    default:
        break;
    }
}



// === Seq-feat.dbxref



struct SGb_QualCompare
{
    // is q1 < q2
    bool operator()(const CRef<CGb_qual>& q1, const CRef<CGb_qual>& q2) {
        return (q1->Compare(*q2) < 0);
    }
};


struct SGb_QualEqual
{
    // is q1 == q2
    bool operator()(const CRef<CGb_qual>& q1, const CRef<CGb_qual>& q2) {
        return (q1->Compare(*q2) == 0);
    }
};


typedef pair<string, CRef<CPub> >   TCit;
struct TCitSort {
    bool operator ()(const TCit& c1, const TCit& c2) {
        return ( c1.first < c2.first ) &&  // labels match.
            CitGenTitlesMatch(*c1.second, *c2.second);
    }
    bool CitGenTitlesMatch(const CPub& p1, const CPub& p2) {
        if ( ! p1.IsGen()  || ! p2.IsGen() ) {
            return true;
        }
        const CCit_gen& g1 = p1.GetGen();
        const CCit_gen& g2 = p2.GetGen();
        if ( ! g1.IsSetTitle()  ||  ! g2.IsSetTitle() ) {
            return true;
        }
        return g1.GetTitle() == g2.GetTitle();
    }
};


static
bool cmpSortedvsOld(const TCit& e1, const CRef<CPub>& e2) {
    return e1.second == e2;
}


// BasicCleanup
void CCleanup_imp::BasicCleanup(CSeq_feat& f)
{
    if (!f.IsSetData()) {
        return;
    }

    CLEAN_STRING_MEMBER(f, Comment);
    if (f.IsSetComment()) {
        CleanDoubleQuote(f.SetComment());        
    }
    if (f.IsSetComment()  &&  f.GetComment() == ".") {
        f.ResetComment();
        ChangeMade(CCleanupChange::eChangeComment);
    }
    CLEAN_STRING_MEMBER(f, Title);
    CLEAN_STRING_MEMBER(f, Except_text);
    if (f.IsSetExcept_text()) {
        x_CleanupExcept_text(f.SetExcept_text());
    }

    BasicCleanup(f.SetData());
    BasicCleanup(f, f.SetData());

    if (f.IsSetDbxref()) {
        CSeq_feat::TDbxref& dbxref = f.SetDbxref();
        
        // dbxrefs cleanup
        CSeq_feat::TDbxref::iterator it = dbxref.begin();
        while (it != dbxref.end()) {
            if (it->Empty()) {
                it = dbxref.erase(it);
                ChangeMade(CCleanupChange::eCleanDbxrefs);
                continue;
            }
            BasicCleanup(**it);
            
            ++it;
        }
        
        // sort/unique db_xrefs
        if ( ! objects::is_sorted(dbxref.begin(), dbxref.end(),
                                  SDbtagCompare()) ) {
            ChangeMade(CCleanupChange::eCleanDbxrefs);
            stable_sort(dbxref.begin(), dbxref.end(), SDbtagCompare());
        }
        size_t n_dbxref = dbxref.size();
        it = unique(dbxref.begin(), dbxref.end(), SDbtagEqual());
        dbxref.erase(it, dbxref.end());
        if (dbxref.size() != n_dbxref) {
            ChangeMade(CCleanupChange::eCleanDbxrefs);
        }
    }
    if (f.IsSetQual()) {
        
        CSeq_feat::TQual::iterator it = f.SetQual().begin();
        CSeq_feat::TQual::iterator it_end = f.SetQual().end();
        while (it != it_end) {
            CGb_qual& gb_qual = **it;
            // clean up this qual.
            BasicCleanup(gb_qual);
            // cleanup the feature given this qual.
            if (BasicCleanup(f, gb_qual)) {
                it = f.SetQual().erase(it);
                it_end = f.SetQual().end();
                ChangeMade(CCleanupChange::eCleanQualifiers);
            } else {
                ++it;
            }
        }
        // expand out all combined quals.
        x_ExpandCombinedQuals(f.SetQual());
        
        // sort/uniquequalsdb_xrefs
        CSeq_feat::TQual& quals = f.SetQual();
        if ( ! objects::is_sorted(quals.begin(), quals.end(),
                                  SGb_QualCompare()) ) {
            ChangeMade(CCleanupChange::eCleanDbxrefs);
            stable_sort(quals.begin(), quals.end(), SGb_QualCompare());
        }
        size_t n_quals = quals.size();
        CSeq_feat::TQual::iterator erase_it = unique(quals.begin(), quals.end(), SGb_QualEqual());
        quals.erase(erase_it, quals.end());
        if (n_quals != quals.size()) {
            ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }
    if (f.IsSetCit()) {
        CPub_set& ps = f.SetCit();

        // The Pub-set should always be pub. Ignore if not.
        if (ps.IsPub()) {
            // sort and unique by putting everything into a set
            // indexed by a label generated for each CPub.
            typedef set<TCit, TCitSort> TCitSet;
            TCitSet cit_set;
            bool dup_rejected = false;
            ITERATE (CPub_set::TPub, cit_it, ps.GetPub()) {
                string label;
                (*cit_it)->GetLabel(&label, CPub::eBoth, true);
                dup_rejected |= cit_set.insert( TCit(label, *cit_it) ).second;
            }
            // Has anything been deleted, or has the order changed?
            if ( dup_rejected  ||
                 ! equal(cit_set.begin(), cit_set.end(), ps.SetPub().begin(), cmpSortedvsOld) ) {
                
                // put everything left back into the feature's citation list.
                ps.SetPub().clear();
                ITERATE (TCitSet, citset_it, cit_set) {
                    ps.SetPub().push_back(citset_it->second);
                }
                ChangeMade(CCleanupChange::eCleanCitonFeat);
            }
        }
    }
    BasicCleanup(f.SetLocation());
}


// ==========================================================================
//                             end of Seq_feat cleanup section
// ==========================================================================




void CCleanup_imp::BasicCleanup(CGene_ref& gene_ref)
{
    CLEAN_STRING_MEMBER(gene_ref, Locus);
    CLEAN_STRING_MEMBER(gene_ref, Allele);
    CLEAN_STRING_MEMBER(gene_ref, Desc);
    CLEAN_STRING_MEMBER(gene_ref, Maploc);
    CLEAN_STRING_MEMBER(gene_ref, Locus_tag);
    CLEAN_STRING_LIST(gene_ref, Syn);
    
    // remove synonyms equal to locus
    if (gene_ref.IsSetLocus()  &&  gene_ref.IsSetSyn()) {
        const CGene_ref::TLocus& locus = gene_ref.GetLocus();
        CGene_ref::TSyn& syns = gene_ref.SetSyn();
        
        CGene_ref::TSyn::iterator it = syns.begin();
        while (it != syns.end()) {
            if (locus == *it) {
                it = syns.erase(it);
                ChangeMade(CCleanupChange::eChangeQualifiers);
            } else {
                ++it;
            }
        }
    }
}


// perform basic cleanup functionality (trim spaces from strings etc.)
void CCleanup_imp::BasicCleanup(CProt_ref& prot_ref)
{
    CLEAN_STRING_MEMBER(prot_ref, Desc);
    CLEAN_STRING_LIST_JUNK(prot_ref, Name);
//    CLEAN_STRING_LIST_JUNK(prot_ref, Ec);
    CLEAN_STRING_LIST(prot_ref, Activity);
    
    if (prot_ref.IsSetProcessed()  &&  !prot_ref.IsSetName()) {
        CProt_ref::TProcessed processed = prot_ref.GetProcessed();
        if (processed == CProt_ref::eProcessed_preprotein  ||  
            processed == CProt_ref::eProcessed_mature) {
            prot_ref.SetName().push_back("unnamed");
            ChangeMade(CCleanupChange::eChangeQualifiers);
        }
    }
}


// RNA_ref basic cleanup 
void CCleanup_imp::BasicCleanup(CRNA_ref& rr)
{
    if (rr.IsSetExt()) {
        CRNA_ref::TExt& ext = rr.SetExt();
        switch (ext.Which()) {
            case CRNA_ref::TExt::e_Name:
                {
                    static const string rRNA = " rRNA";
                    static const string kRibosomalrRna = " ribosomal rRNA";
                    
                    _ASSERT(rr.IsSetExt()  &&  rr.GetExt().IsName());
                    
                    string& name = rr.SetExt().SetName();
                    if (CleanString(name)) {
                        ChangeMade(CCleanupChange::eTrimSpaces);
                    }
                    
                    if (name.empty()) {
                        rr.ResetExt();
                        ChangeMade(CCleanupChange::eChangeQualifiers);
                    } else if (rr.IsSetType()) {
                        switch (rr.GetType()) {
                            case CRNA_ref::eType_rRNA:
                            {{
                                size_t len = name.length();
                                if (len >= rRNA.length()                       &&
                                    NStr::EndsWith(name, rRNA, NStr::eNocase)  &&
                                    !NStr::EndsWith(name, kRibosomalrRna, NStr::eNocase)) {
                                    name.replace(len - rRNA.length(), name.size(), kRibosomalrRna);
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                }
                                break;
                            }}
                            case CRNA_ref::eType_tRNA:
                            {{
                                // !!! parse tRNA string. lines 6791:6827, api/sqnutil1.c
                                break;
                            }}
                            case CRNA_ref::eType_other:
                            {{
                                if (NStr::EqualNocase(name, "its1")) {
                                    name = "internal transcribed spacer 1";
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                } else if (NStr::EqualNocase(name, "its2")) {
                                    name = "internal transcribed spacer 2";
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                } else if (NStr::EqualNocase(name, "its3")) {
                                    name = "internal transcribed spacer 3";
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                } else if (NStr::EqualNocase(name, "its")) {
                                    name = "internal transcribed spacer";
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                    break;
                                }
                            }}
                            default:
                                break;
                        }
                    }
                }
                break;
            case CRNA_ref::TExt::e_TRNA:
                // CleanupTrna (SeqFeatPtr sfp, tRNAPtr trp), line 3117, api/sqnutil1.c
                break;
            default:
                break;
        }
    }
}



// Imp_feat cleanup
void CCleanup_imp::BasicCleanup(CImp_feat& imf)
{
    CLEAN_STRING_MEMBER_JUNK(imf, Key);
    CLEAN_STRING_MEMBER(imf, Loc);
    CLEAN_STRING_MEMBER(imf, Descr);
    
    if (imf.IsSetKey()) {
        const CImp_feat::TKey& key = imf.GetKey();
        if (key == "allele"  ||  key == "mutation") {
            imf.SetKey("variation");
            ChangeMade(CCleanupChange::eChangeKeywords);
        } else if (NStr::EqualNocase(key, "import")) {
            imf.SetKey("misc_feature");
            ChangeMade(CCleanupChange::eChangeKeywords);
        }
    }
}


// Extended Cleanup methods

// move GenBank qualifiers named "db_xref" on a feature to real dbxrefs
// Was SeqEntryMoveDbxrefs in C Toolkit
void CCleanup_imp::x_MoveDbxrefs(CSeq_feat& feat)
{
    if (feat.CanGetQual()) {
        CSeq_feat::TQual& current_list = feat.SetQual();
        CSeq_feat::TQual new_list;
        new_list.clear();
        
        ITERATE (CSeq_feat::TQual, it, current_list) {            
            if ((*it)->CanGetQual() && NStr::Equal((*it)->GetQual(), "db_xref")) {
                CRef<CDbtag> tag(new CDbtag());
                string qval = (*it)->GetVal();
                string::size_type pos = NStr::Find (qval, ":");
                if (pos == NCBI_NS_STD::string::npos) {
                    tag->SetDb("?");
                    tag->SetTag().Select(CObject_id::e_Str);
                    tag->SetTag().SetStr(qval);
                } else {
                    tag->SetDb(qval.substr(0, pos));
                    tag->SetTag().Select(CObject_id::e_Str);
                    tag->SetTag().SetStr(qval.substr(pos + 1));
                }
                feat.SetDbxref().push_back(tag);
            } else {
                new_list.push_back (*it);
            }
        }
        current_list.clear();

        ITERATE (CSeq_feat::TQual, it, new_list) {
            current_list.push_back(*it);
        }
        if (current_list.size() == 0) {
            feat.ResetQual();
        }        
    }
}


void CCleanup_imp::x_MoveDbxrefs (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            x_MoveDbxrefs(const_cast<CSeq_feat &> (feat_ci->GetOriginalFeature()));
            ++feat_ci;                
        }
    }

}


CSeq_feat_Handle GetSeq_feat_Handle(CScope& scope, const CSeq_feat& feat)
{
    SAnnotSelector sel(feat.GetData().GetSubtype());
    sel.SetResolveAll().SetNoMapping().SetSortOrder(sel.eSortOrder_None);
    for (CFeat_CI mf(scope, feat.GetLocation(), sel); mf; ++mf) {
        if (mf->GetOriginalFeature().Equals(feat)) {
            return mf->GetSeq_feat_Handle();
        }
    }
    return CSeq_feat_Handle();
}


// Was CdEndCheck in C Toolkit
// Attempts to adjust the length of a coding region so that it will translate to
// the specified product
// returns true if 
bool CCleanup_imp::x_CheckCodingRegionEnds (CSeq_feat_Handle ofh)
{
    if (!ofh.IsSetData() 
        || ofh.GetData().Which() != CSeqFeatData::e_Cdregion
        || !ofh.IsSetProduct()) {
        return false;
    }
    
    if (ofh.GetSeq_feat().IsNull()) {
        return false;
    }
    
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->Assign(*(ofh.GetSeq_feat()));
    
    const CCdregion& crp = feat->GetData().GetCdregion();    

    unsigned int feat_len = sequence::GetLength(feat->GetLocation(), m_Scope);
    unsigned int frame_adjusted_len = feat_len;
    if (crp.CanGetFrame()) {
        if (crp.GetFrame() == 2) {
            frame_adjusted_len -= 1;
        } else if (crp.GetFrame() == 3) {
            frame_adjusted_len -= 2;
        }
    }
    
    unsigned int remainder = frame_adjusted_len % 3;
    unsigned int translation_len = frame_adjusted_len / 3;
    CBioseq_Handle product;
    
    try {
        product = m_Scope->GetBioseqHandle(feat->GetProduct());
    } catch (...) {
        return false;
    }
    
    if (!product) {
        return false;
    }
    
    if (product.GetBioseqLength() + 1 == translation_len && remainder == 0) {
        return false;
    }

    if (crp.CanGetCode_break()) {
        TSeqPos feat_len = sequence::GetLength (feat->GetLocation(), m_Scope);
        // if code break is length 0 or 1 and ends at end of feature, allow partial end codon
        ITERATE (list< CRef< CCode_break > >, it, crp.GetCode_break()) {
            TSeqPos codon_length = sequence::GetLength((*it)->GetLoc(), m_Scope);
            if (codon_length == 1 || codon_length == 2) {
                /* code break has correct length */
                SRelLoc rl(feat->GetLocation(), (*it)->GetLoc(), m_Scope);
                ITERATE (SRelLoc::TRanges, rit, rl.m_Ranges) {
                    if ((*rit)->GetTo() == feat_len - 1) {
                        /* code break ends */ 
                        return false;
                    }
                }
            }
        }
    }
    
    // create a copy of the feature location called new_location
    CRef<CSeq_loc> new_location(new CSeq_loc);
    new_location->Assign(feat->GetLocation());
    
    // adjust the last piece of new_location to be the right length to
    // generate the desired protein length
    CSeq_loc_CI loc_it(*new_location);
    CSeq_loc_CI last_it = loc_it;
    
    while (loc_it) {
        last_it = loc_it;
        ++loc_it;
    }
    
    if (!last_it || !last_it.GetSeq_loc().IsInt()) {
        return false;
    }
    
    CBioseq_Handle last_seq = m_Scope->GetBioseqHandle(last_it.GetSeq_loc());
    
    switch (remainder)
    {
        case 0:
            remainder = 3;
            break;
        case 1:
            remainder = 2;
            break;
        case 2:
            remainder = 1;
            break;
    }

    TSeqPos old_from = last_it.GetRange().GetFrom();
    TSeqPos old_to = last_it.GetRange().GetTo();

    if (last_it.GetStrand() == eNa_strand_minus) {
        if (old_from < remainder) {
            return false;
        } else if (last_it.GetFuzzFrom() != NULL) {
            return false;
        } else {
            (const_cast <CSeq_loc& > (last_it.GetSeq_loc())).SetInt().SetFrom (old_from - remainder);
        }
    }
    else
    {
        if (old_to >= last_seq.GetBioseqLength() - remainder) {
            return false;
        } else if (last_it.GetFuzzTo() != NULL) {
            return false;
        } else {
            (const_cast <CSeq_loc& > (last_it.GetSeq_loc())).SetInt().SetTo (old_to + remainder);
        }
    }

    // get new protein sequence by translating the coding region
    CBioseq_Handle nuc_bsh = m_Scope->GetBioseqHandle(feat->GetLocation());
    string data;
    CCdregion_translate::TranslateCdregion(data, nuc_bsh,
                                           *new_location,
                                           crp,
                                           true,
                                           true);

    // if the translation is the wrong length, give up
    if (data.length() != (frame_adjusted_len + remainder) / 3) {
        return false;
    }

    // if the translation doesn't end with stop codon, give up
    if (!NStr::Equal(data.substr(data.length() - 1), "*")) {
        return false;
    }
    
    // get existing protein data
    string prot_buffer;
    product.GetSeqVector(CBioseq_Handle::eCoding_Iupac).GetSeqData(0, product.GetBioseqLength() - 1, prot_buffer);
    // if the translation doesn't match the existing protein, give up
    if (!NStr::Equal(data.substr(0, data.length() - 2), prot_buffer)) {
        return false;
    }

    // fix location for overlapping gene
    const CGene_ref* grp = feat->GetGeneXref();
    if (grp == NULL) { // NOTE - in C Toolkit also do this if grp is not suppressed
        CConstRef<CSeq_feat> cr = sequence::GetOverlappingGene(feat->GetLocation(), *m_Scope);
        if (!cr.IsNull()) {        
            CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, *cr);
            
            if (!fh.GetSeq_feat().IsNull()) {
                CRef<CSeq_feat> gene_feat(new CSeq_feat);
            
                gene_feat->Assign(*cr);
                sequence::ECompare loc_compare = sequence::Compare(*new_location, gene_feat->GetLocation(), m_Scope);

                if (loc_compare != sequence::eContained && loc_compare != sequence::eSame) {
                    CSeq_loc& gene_loc = gene_feat->SetLocation();
            
                    CSeq_loc_CI tmp(gene_loc);
                    bool has_nulls = false;
                    while (tmp && !has_nulls) {
                        if (tmp.GetSeq_loc().IsNull()) {
                            has_nulls = true;
                        }
                        ++tmp;
                    }
                    CRef<CSeq_loc> new_gene_loc = sequence::Seq_loc_Add(gene_loc, *new_location, 
                                                        CSeq_loc::fMerge_SingleRange, m_Scope);
                                                
                    new_gene_loc->SetPartialStart (new_location->IsPartialStart(eExtreme_Biological) | gene_loc.IsPartialStart(eExtreme_Biological), eExtreme_Biological);
                    new_gene_loc->SetPartialStop (new_location->IsPartialStop(eExtreme_Biological) | gene_loc.IsPartialStop(eExtreme_Biological), eExtreme_Biological);
                    // Note - C version pushes gene location to segset parts         
  
                    gene_feat->SetLocation (*new_gene_loc);    
                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*gene_feat);
                    ChangeMade (CCleanupChange::eChangeFeatureLocation);
                }        
            }
        }
    }
    
    // fix location of coding region
    feat->SetLocation(*new_location);
    
    CSeq_feat_EditHandle efh(ofh);
    efh.Replace(*feat);
    ChangeMade (CCleanupChange::eChangeFeatureLocation);
    return true;
}


void CCleanup_imp::x_CheckCodingRegionEnds (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            x_CheckCodingRegionEnds(feat_ci->GetSeq_feat_Handle());
            ++feat_ci;                
        }
    }
}


// Was ExtendSingleGeneOnMRNA in C Toolkit
// Will change the location of a gene on an mRNA sequence to
// cover the entire sequence, as long as there is only one gene
// present and zero or one coding regions present.
bool CCleanup_imp::x_ExtendSingleGeneOnmRNA (CBioseq_Handle bsh, bool is_master_seq)
{
    bool rval = false;
    if (!bsh.CanGetId()) {
        return false;
    }

    int num_genes = 0;
    int num_cdss = 0;
    
    for (CFeat_CI feat_ci(bsh); num_genes < 2 && num_cdss < 2 && feat_ci; ++feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene) {
            num_genes ++;
        } else if (feat_ci->GetFeatType() == CSeqFeatData::e_Cdregion) {
            num_cdss++;
        }
    }
    
    if (num_genes == 1 && (is_master_seq || num_cdss < 2)) {
        objects::SAnnotSelector sel(CSeqFeatData::eSubtype_gene);

        CFeat_CI gene_it(bsh, sel);

        if (gene_it->GetLocation().GetId()) {
            CBioseq_Handle gene_bsh = m_Scope->GetBioseqHandle(gene_it->GetLocation());
            if (gene_bsh == bsh && !IsFeatureFullLength(gene_it->GetOriginalFeature(), m_Scope)) {
                CSeq_feat_Handle fh = gene_it->GetSeq_feat_Handle();
                if (!fh.GetSeq_feat().IsNull()) {
                    CRef<CSeq_feat> new_gene(new CSeq_feat);
                    new_gene->Assign(gene_it->GetOriginalFeature());
                    CRef<CSeq_loc> new_loc = MakeFullLengthLocation(gene_it->GetLocation(), m_Scope);
                    new_loc->SetPartialStart (gene_it->GetLocation().IsPartialStart(eExtreme_Biological), eExtreme_Biological);
                    new_loc->SetPartialStop (gene_it->GetLocation().IsPartialStop(eExtreme_Biological), eExtreme_Biological);
                    
                    new_gene->SetLocation(*new_loc);
                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*new_gene);
                    ChangeMade(CCleanupChange::eChangeFeatureLocation);
                    rval = true;
                }
            }
        }  
    }
    return rval;
}


void CCleanup_imp::x_ExtendSingleGeneOnmRNA (CBioseq_set_Handle bssh)
{
    // the first bioseq from a segmented set will be the master sequence
    bool is_master_seq = false;
    bool did_extend;

    if (bssh.CanGetClass() && bssh.GetClass() == CBioseq_set::eClass_segset) {
        if (!IsmRNA(bssh) || IsArtificialSyntheticConstruct(bssh)) {
            return;
        } else {
            is_master_seq = true;
        }
    }

    CConstRef<CBioseq_set> b = bssh.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
    ITERATE (list< CRef< CSeq_entry > >, it, set) {
        if ((*it)->IsSet()) {
            x_ExtendSingleGeneOnmRNA (m_Scope->GetBioseq_setHandle((*it)->GetSet()));
        } else if ((*it)->IsSeq()) {
            did_extend = false;
            CBioseq_Handle bh = m_Scope->GetBioseqHandle((*it)->GetSeq());
            if (IsmRNA (bh) && ! IsArtificialSyntheticConstruct (bh)) {
                did_extend = x_ExtendSingleGeneOnmRNA(bh, is_master_seq);
            }
            // if we extended the gene to cover the master sequence, we are done
            if (did_extend && is_master_seq) return;
            // sequences after the first from a segmented set are parts
            is_master_seq = false;
        }
    }
}


void CCleanup_imp::x_RemovePseudoProducts (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        objects::SAnnotSelector prot_sel(CSeqFeatData::eSubtype_prot);
        objects::SAnnotSelector cds_sel(CSeqFeatData::eSubtype_cdregion);
        CFeat_CI feat_ci(sa, cds_sel);
        while (feat_ci) {
            if (feat_ci->GetOriginalFeature().CanGetPseudo()
                && feat_ci->GetPseudo() 
                && feat_ci->IsSetProduct()) {
                CSeq_feat_Handle fh = feat_ci->GetSeq_feat_Handle();
                if (!fh.GetSeq_feat().IsNull()) {
                    CRef<CSeq_feat> new_cds(new CSeq_feat);            
                    new_cds->Assign(feat_ci->GetOriginalFeature());
                    CBioseq_Handle product = m_Scope->GetBioseqHandle(feat_ci->GetProduct());
                    CFeat_CI product_ci(product, prot_sel);
                    if (product_ci) {
                        const CProt_ref& prot_ref = product_ci->GetOriginalFeature().GetData().GetProt();
                        if (prot_ref.CanGetName() && prot_ref.GetName().size() > 0
                            && !NStr::IsBlank(prot_ref.GetName().front())) {
                            if (new_cds->IsSetComment()) {
                                string old_comment = new_cds->SetComment();
                                old_comment += "; ";
                                old_comment += prot_ref.GetName().front();
                            } else {
                                new_cds->SetComment(prot_ref.GetName().front());
                            }
                        }        
                    }
                    product.GetParentEntry().GetEditHandle().Remove();
                    new_cds->ResetProduct();
                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*new_cds);
                }
            }
            ++feat_ci;
        }
    }
}


void CCleanup_imp::x_RemoveGeneXref(CRef<CSeq_feat> feat) 
{
    if (!feat->IsSetXref()) {
        return;
    }
    CSeq_feat::TXref& xref_list = feat->SetXref();

    NON_CONST_ITERATE ( CSeq_feat::TXref, it, xref_list) {
        const CSeqFeatXref& xref = **it;
        if ( xref.IsSetData() && xref.GetData().Which() == CSeqFeatData::e_Gene) {
            it = xref_list.erase(it);
            ChangeMade(CCleanupChange::eRemoveGeneXref);
            break;
        }
    }
    if (xref_list.empty()) {
        feat->ResetXref();
    }
}


void CCleanup_imp::x_RemoveUnnecessaryGeneXrefs(CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        objects::SAnnotSelector gene_sel(CSeqFeatData::eSubtype_gene);
        
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            if (feat_ci->GetFeatSubtype() != CSeqFeatData::eSubtype_gene) {
                const CSeq_feat& feat = feat_ci->GetOriginalFeature();
                const CGene_ref* gene_xref = feat.GetGeneXref();
                if (gene_xref != NULL && ! gene_xref->IsSuppressed()) {
                    CConstRef<CSeq_feat> overlap = sequence::GetOverlappingGene(feat.GetLocation(), *m_Scope);
                    if (!overlap.IsNull() && !overlap->GetData().IsGene()) {
                        // this is weird!
//                        CSeqFeatData::ESubtype stype = overlap->GetData().GetSubtype();
                    }
                    if (!overlap.IsNull() && overlap->GetData().IsGene()) {
                        const CGene_ref& overlap_ref = overlap->GetData().GetGene();
                        CFeat_CI other_genes(*m_Scope, overlap->GetLocation(), gene_sel);
                        int num_in_place = 0;
                        while (other_genes && num_in_place < 2) {
                            if (sequence::Compare(overlap->GetLocation(),
                                                     other_genes->GetOriginalFeature().GetLocation(), 
                                                      m_Scope) == sequence::eSame) {
                                num_in_place++;
                            }
                            ++other_genes;
                        }
                        if (num_in_place == 1) {
                            bool redundant = false;
                            if (overlap_ref.CanGetLocus() && !NStr::IsBlank(overlap_ref.GetLocus())
                                && gene_xref->CanGetLocus() && !NStr::IsBlank(gene_xref->GetLocus())) {
                                if (NStr::EqualNocase(overlap_ref.GetLocus(), gene_xref->GetLocus())) {
                                    redundant = true;
                                }
                            } else if(overlap_ref.CanGetSyn() && gene_xref->CanGetSyn()
                                      && !NStr::IsBlank(overlap_ref.GetSyn().front())
                                      && !NStr::IsBlank(gene_xref->GetSyn().front())
                                      && NStr::EqualNocase (overlap_ref.GetSyn().front(),
                                                            gene_xref->GetSyn().front())) {
                                redundant = true;
                            }
                            if (redundant) {
                                CSeq_feat_Handle fh = feat_ci->GetSeq_feat_Handle();
                                CRef<CSeq_feat> new_feat(new CSeq_feat);
                                new_feat->Assign(feat);
                                x_RemoveGeneXref(new_feat);
                                CSeq_feat_EditHandle efh(fh);
                                efh.Replace(*new_feat);
                            }
                        }
                    }
                }
            }
            ++feat_ci;
        } 
    }
}


bool CCleanup_imp::x_ChangeNoteQualToComment(CSeq_feat& feat)
{
    bool rval = false;
    if (!feat.CanGetQual() || feat.GetQual().empty()) {
        return rval;
    }
    
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while (it != feat.SetQual().end()) {
        CGb_qual& gb_qual = **it;
                
        if (gb_qual.CanGetQual()
            && NStr::EqualNocase(gb_qual.GetQual(), "note")
            && gb_qual.CanGetVal()) {
            string note = gb_qual.GetVal();
            if (!NStr::IsBlank(note)) {
                string& comment = feat.SetComment();
                if (NStr::IsBlank(comment)) {
                    comment = note;
                } else {
                    comment += "; ";
                    comment += note;
                }
                ChangeMade(CCleanupChange::eChangeComment);
            }
            it = feat.SetQual().erase(it);
            ChangeMade(CCleanupChange::eRemoveQualifier);
            rval = true;
        } else {
            ++it;
        }
    }
    
    if ( feat.GetQual().empty()) {
        feat.ResetQual();
        rval = true;
    }
    return rval;
}

// Was GetFrameFromLoc in C Toolkit (api/seqport.c)
// returns 1, 2, or 3 if it can find the frame,
// 0 otherwise
CCdregion::EFrame CCleanup_imp::x_FrameFromSeqLoc(const CSeq_loc& loc)
{
    CCdregion::EFrame frame = CCdregion::eFrame_not_set;

    if (!loc.IsPartialStart(eExtreme_Biological)) {
        frame = CCdregion::eFrame_one;
    } else if (!loc.IsPartialStop(eExtreme_Biological)) {
        unsigned int len = sequence::GetLength(loc, m_Scope);
        unsigned int remainder = len % 3;
        switch (remainder) {
            case 0:
                frame = CCdregion::eFrame_one;
                break;
            case 1:
                frame = CCdregion::eFrame_two;
                break;
            case 2:
                frame = CCdregion::eFrame_three;
                break;
            default:
                break;
        }
    }
    return frame;
}


bool CCleanup_imp::x_ImpFeatToCdRegion (CSeq_feat& feat)
{
    
    if (!feat.CanGetData() || !feat.GetData().IsImp()
        || feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_Imp_CDS) {
        return false;
    }

    try {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
        ITERATE (list< CRef< CSeq_id > >, it, bsh.GetCompleteBioseq()->GetId()) {
            if ((*it)->Which() == CSeq_id::e_Ddbj 
                || (*it)->Which() == CSeq_id::e_Embl) {
                return false;
            }
        }
    } catch (...) {
        return false;
    }

    feat.ResetData();
    feat.SetData().SetCdregion();
    
    int frame = -1;
    
    if (feat.CanGetQual()) {
        NON_CONST_ITERATE (CSeq_feat::TQual, it, feat.SetQual()) {
            CGb_qual& gb_qual = **it;
                
            if (gb_qual.CanGetQual() && (gb_qual.CanGetVal() || NStr::Equal(gb_qual.GetQual(), "translation"))) {
                string qual_name = gb_qual.GetQual();
                if (NStr::Equal(qual_name, "transl_table")) {
                    unsigned int gc = NStr::StringToUInt(gb_qual.GetVal());
                    CRef<CGenetic_code::C_E> ce(new CGenetic_code::C_E);
                    ce->Select(CGenetic_code::C_E::e_Id);
                    ce->SetId(gc);
                    feat.SetData().SetCdregion().SetCode().Set().push_back(ce);
                    it = feat.SetQual().erase(it);
                } else if (NStr::Equal(qual_name, "translation")) {
                    it = feat.SetQual().erase(it);
                } else if (NStr::Equal(qual_name, "transl_except")) {
                    x_ParseCodeBreak(feat, feat.SetData().SetCdregion(), gb_qual.GetVal());
                    it = feat.SetQual().erase(it);
                } else if (NStr::Equal(qual_name, "codon_start")) {
                    frame = NStr::StringToInt(gb_qual.GetVal());
                    switch (frame) {
                        case 1:
                            feat.SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
                            break;
                        case 2:
                            feat.SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
                            break;
                        case 3:
                            feat.SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
                            break;
                        default:
                            feat.SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
                            break;
                    }
                    it = feat.SetQual().erase(it);
                } else if (NStr::Equal(qual_name, "exception")) {
                    feat.SetExcept(true);
                    it = feat.SetQual().erase(it);
                } else {
                    ++ it;
                }            
            } else {
                ++it;
            }
        }
    }
    if (frame == -1) {
        feat.SetData().SetCdregion().SetFrame(x_FrameFromSeqLoc(feat.GetLocation()));
    }

    return true;    
}


// Was ChangeImpFeat in C Toolkit
void CCleanup_imp::x_ChangeImpFeatToCDS(CSeq_annot_Handle sa)
{
    if (!sa.IsFtable()) {
        return;
    }
    
    CFeat_CI feat_ci(sa);

    while (feat_ci) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        const CSeq_feat& orig_feat = feat_ci->GetOriginalFeature();
        feat->Assign(orig_feat);
        bool changed = false;
    
        changed |= x_ChangeNoteQualToComment(*feat);
        
        if (feat->CanGetData() && feat->GetData().IsImp()) {
            CImp_feat& ifp = feat->SetData().SetImp();
            
            if (ifp.CanGetLoc()) {
                string::size_type pos = NStr::Find(ifp.GetLoc(), "replace");
                if (pos != NCBI_NS_STD::string::npos) {
                    x_AddReplaceQual(*feat, ifp.GetLoc());
                    ifp.ResetLoc();
                    changed = true;
                }
            }
            
            x_ImpFeatToCdRegion(*feat);
        }
                        
        if (changed) {
            CSeq_feat_Handle ofh = feat_ci->GetSeq_feat_Handle();
            CSeq_feat_EditHandle efh(ofh);
            efh.Replace(*feat);
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_ChangeImpFeatToProt(CSeq_annot_Handle sa)
{
    // iterate through features.
    CFeat_CI feat_ci(sa);
    
    while (feat_ci) {
        bool need_convert = false;
        bool is_site = false;
        bool is_bond = false;
        string key;
        CSeqFeatData::ESite site_type = CSeqFeatData::eSite_other;
        CSeqFeatData::EBond bond_type = CSeqFeatData::eBond_other;
        
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Imp) {
            key = feat_ci->GetOriginalFeature().GetData().GetImp().GetKey();
            if (NStr::Equal(key, "mat_peptide") 
                || NStr::Equal(key, "sig_peptide")
                || NStr::Equal(key, "transit_peptide")) {
                need_convert = true;
            }
        } else if (feat_ci->GetFeatSubtype() == CSeqFeatData::eSubtype_misc_feature) {
            if (CSeqFeatData::GetSiteList()->IsSiteName(feat_ci->GetComment(), site_type)) {
                is_site = true;
                need_convert = true;
            } else if (CSeqFeatData::GetBondList()->IsBondName(feat_ci->GetComment(), bond_type)) {
                is_bond = true;
                need_convert = true;
            }
        }
        
        if (need_convert) {
            const CSeq_feat& orig_feat = feat_ci->GetOriginalFeature();
            // find the best overlapping coding region that isn't pseudo and doesn't have a pseudogene
            CConstRef<CSeq_feat> cds = sequence::GetBestOverlappingFeat(feat_ci->GetLocation(),
                                                              CSeqFeatData::eSubtype_cdregion,
                                                              sequence::eOverlap_CheckIntRev,
                                                              *m_Scope);
            if (!cds.IsNull() && cds->CanGetProduct()) {
                CBioseq_Handle product_h = m_Scope->GetBioseqHandle(cds->GetProduct());
                if (product_h) {

                    // get location for feature on protein sequence                                                                  
                    CSeq_loc_Mapper mapper(*cds,
                                           CSeq_loc_Mapper::eLocationToProduct,
                                           m_Scope);
                    CRef<CSeq_loc> product_loc = mapper.Map(feat_ci->GetLocation());
            
                    product_loc->SetPartialStart (feat_ci->GetLocation().IsPartialStart(eExtreme_Biological), eExtreme_Biological);
                    product_loc->SetPartialStop (feat_ci->GetLocation().IsPartialStop(eExtreme_Biological), eExtreme_Biological);
                                                                               
                    CSeq_feat_Handle fh = feat_ci->GetSeq_feat_Handle();
                
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    feat->Assign(orig_feat);
                
                    feat->SetLocation(*product_loc);
                    ChangeMade(CCleanupChange::eChangeFeatureLocation);
              
                    if (NStr::Equal(key, "mat_peptide") 
                        || NStr::Equal(key, "sig_peptide")
                        || NStr::Equal(key, "transit_peptide")) {
                        feat->SetData().Reset();
                        CProt_ref& prot_ref = feat->SetData().SetProt();
                        if (NStr::Equal(key, "mat_peptide")) {
                            prot_ref.SetProcessed(CProt_ref::eProcessed_mature);
                            // copy product qualifiers to product names
                            if (feat->IsSetQual()) {
                                CSeq_feat::TQual::iterator it = feat->SetQual().begin();
                                while (it != feat->SetQual().end()) {
                                    if (NStr::Equal((*it)->GetQual(), "product")) {
                                        prot_ref.SetName().push_back((*it)->GetVal());
                                        it = feat->SetQual().erase(it);
                                        ChangeMade(CCleanupChange::eRemoveQualifier);
                                    } else {
                                        ++it;
                                    }
                                }
                            }
                        } else if (NStr::Equal(key, "sig_peptide")) {
                            prot_ref.SetProcessed(CProt_ref::eProcessed_signal_peptide);
                        } else if (NStr::Equal(key, "transit_peptide")) {
                            prot_ref.SetProcessed(CProt_ref::eProcessed_transit_peptide);
                        }
                    
                        if (feat->CanGetComment() 
                            && NStr::Equal(feat->GetComment(), "putative", NStr::eNocase)
                            && (NStr::Equal(key, "sig_peptide") || !prot_ref.CanGetName() || prot_ref.GetName().empty())) {
                            prot_ref.SetName().push_back(feat->GetComment());
                            feat->ResetComment();
                        }
                    } else if (is_bond) {
                        feat->SetData().Reset();
                        feat->SetData().SetSite(site_type);
                        feat->ResetComment();
                        // TODO
                        // for any bond features, strip NULLs from the location
                        CRef<CSeq_loc> new_loc(new CSeq_loc);
                        bool changed = false;
                        for ( CSeq_loc_CI loc_iter(feat->GetLocation()); loc_iter; ++loc_iter ) {
                            if (loc_iter.GetSeq_loc().IsNull()) {
                                changed = true;
                            } else {
                                new_loc->SetMix().AddSeqLoc(loc_iter.GetSeq_loc());
                            }
                        }
                        if (changed) {
                            feat->SetLocation(*new_loc);
                            ChangeMade(CCleanupChange::eChangeFeatureLocation);
                        }   
                        
                    } else if (is_site) {
                        feat->SetData().Reset();
                        feat->SetData().SetBond(bond_type);
                    }

                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*feat);
                    ChangeMade(CCleanupChange::eConvertFeature);
                
                    // Move feature to protein SeqAnnot
                    CSeq_annot_CI annot_it(product_h.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
                    while (annot_it && 
                           (! (*annot_it).IsFtable()
                            || ((*annot_it).Seq_annot_CanGetId() && (*annot_it).Seq_annot_GetId().size() != 0)
                            || ((*annot_it).Seq_annot_CanGetName() && !NStr::IsBlank((*annot_it).Seq_annot_GetName()))
                            || ((*annot_it).Seq_annot_CanGetDb() && (*annot_it).Seq_annot_GetDb() != 0)
                            || ((*annot_it).Seq_annot_CanGetDesc() && (*annot_it).Seq_annot_GetDesc().Get().size() != 0))) {
                        ++annot_it;
                    }
                    if (annot_it) {
                        CSeq_annot_EditHandle annot_eh = (*annot_it).GetEditHandle();
                        annot_eh.TakeFeat(efh);
                    } else {
                        CBioseq_EditHandle product_eh = product_h.GetEditHandle();
                        CRef<CSeq_annot> new_annot(new CSeq_annot);
                        new_annot->SetData().SetFtable();                        
                        product_eh.AttachAnnot(*new_annot);
                        CSeq_annot_EditHandle annot_eh = m_Scope->GetSeq_annotEditHandle(*new_annot);
                        annot_eh.TakeFeat(efh);
                    }
                    ChangeMade(CCleanupChange::eMoveFeat);
                }
            }   
        }
        
        ++feat_ci;
    }
#if 0
static void ImpFeatToProtRef(SeqFeatArr sfa)
{  
  SeqFeatPtr f1, f2, best_cds, sfp;
  SeqLocPtr loc, slp;
  ImpFeatPtr ifp;
  ProtRefPtr prot;
  BioseqPtr bsp;
  SeqAnnotPtr sap;
  Int4 diff_lowest, diff_current, frame;
  ValNodePtr tmp1, tmp2;
  Uint2 retval;
  Int2 i;
  Boolean lfree = FALSE, partial5, partial3;
  CharPtr p, q;
  GBQualPtr qu, qunext;
  
  for (tmp1 = sfa.pept; tmp1; tmp1 = tmp1->next) {
    lfree = FALSE;
    f1 = (SeqFeatPtr) tmp1->data.ptrvalue;
    loc = f1->location;
    if (tmp1->choice == SEQFEAT_BOND) {
      loc = fake_bond_loc(f1->location);
      lfree = TRUE;
    }
    diff_lowest = -1;
    best_cds = NULL;
    for (tmp2=sfa.cds; tmp2; tmp2=tmp2->next) {
      f2 = tmp2->data.ptrvalue;
      diff_current = SeqLocAinB(loc, f2->location);
      if (! diff_current)   /* perfect match */ {
        best_cds = f2;
        break;
      } else if (diff_current > 0) {
        if ((diff_lowest == -1) || (diff_current < diff_lowest)) {
          diff_lowest = diff_current;
          best_cds = f2;
        }
      }
    }
/*    if (lfree)
      SeqLocFree(loc);
*/
    if (best_cds == NULL) { 
      p = SeqLocPrint(f1->location);
      ErrPostEx(SEV_WARNING, ERR_FEATURE_CDSNotFound, 
      "CDS for the peptide feature [%s] not found", p);
      MemFree(p);
    } else {
      if (OutOfFramePeptideButEmblOrDdbj (f1, best_cds))
        continue;
      CheckSeqLocForPartial (f1->location, &partial5, &partial3);
      slp = dnaLoc_to_aaLoc(best_cds, f1->location, TRUE, &frame, FALSE);
      if (slp == NULL) {
      p = SeqLocPrint(f1->location);
      q = SeqLocPrint(best_cds->location);
      ErrPostEx(SEV_ERROR, ERR_FEATURE_CannotMapDnaLocToAALoc, "peptide location:%s| CDS location:%s", p, q);
      MemFree(p);
      MemFree(q);
        continue;
      }
      SetSeqLocPartial (slp, partial5, partial3);
      ifp = (ImpFeatPtr) f1->data.value.ptrvalue;
      sfp = SeqFeatNew();
      sfp->location = slp;

      sfp->partial = (Boolean) (f1->partial || partial5 || partial3);
      sfp->excpt = f1->excpt;
      sfp->exp_ev = f1->exp_ev;
      sfp->pseudo = f1->pseudo;

      sfp->comment = f1->comment;
      f1->comment = NULL;
      sfp->qual = f1->qual;
      f1->qual = NULL;
      sfp->title = f1->title;
      f1->title = NULL;
      sfp->ext = f1->ext;
      f1->ext = NULL;
      sfp->cit = f1->cit;
      f1->cit = NULL;

      sfp->xref = f1->xref;
      f1->xref = NULL;
      sfp->dbxref = f1->dbxref;
      f1->dbxref = NULL;
      sfp->except_text = f1->except_text;
      f1->except_text = NULL;

      if (f1->qual != NULL) {
        sfp->qual = f1->qual;
        f1->qual = NULL;
      }
      if (tmp1->choice == SEQFEAT_PROT) {
        sfp->data.choice = SEQFEAT_PROT;
        prot = ProtRefNew();
        sfp->data.value.ptrvalue = prot;
        if (StringCmp(ifp->key, "mat_peptide") == 0) {
          prot->processed = 2;
          for (qu=sfp->qual; qu; qu=qunext) {
            qunext = qu->next;
            if (StringCmp(qu->qual, "product") == 0) {
              ValNodeAddStr(&(prot->name), 0,StringSave(qu->val));
              sfp->qual = remove_qual(sfp->qual, qu); 
            }
          }
        }
        if (StringCmp(ifp->key, "sig_peptide") == 0)
          prot->processed = 3;
        if (StringCmp(ifp->key, "transit_peptide") == 0)
          prot->processed = 4;
        if (f1->comment != NULL) {
          if ((prot->processed == 2 || prot->name == NULL) && StringICmp (f1->comment, "putative") != 0) {
            ValNodeAddStr(&(prot->name), 0,StringSave(f1->comment));
          } else {
            sfp->comment = StringSave(f1->comment);
          }
        }
      } else if (tmp1->choice == SEQFEAT_SITE) {
        sfp->data.choice = SEQFEAT_SITE;
        if ((i = FindStr(feat_site, num_site, f1->comment)) != -1) {
          sfp->data.value.intvalue = i;
        } else {
          sfp->data.value.intvalue = 255;
        }
      } else if (tmp1->choice == SEQFEAT_BOND) {
        sfp->data.choice = SEQFEAT_BOND;
        if ((i = FindStr(feat_bond, num_bond, f1->comment)) != -1) {
          sfp->data.value.intvalue = i;
        } else {
          sfp->data.value.intvalue = 255;
        }
      }
      if (f1->title)
      {
        if(sfp->comment != NULL)
          MemFree(sfp->comment);
        sfp->comment = StringSave(f1->title);
      }
      CheckSeqLocForPartial (f1->location, &partial5, &partial3);
      sfp->excpt = f1->excpt;
      sfp->partial = (Boolean) (f1->partial || partial5 || partial3);
      sfp->exp_ev = f1->exp_ev;
      sfp->pseudo = f1->pseudo;
      if(sfp->location)
        SeqLocFree(sfp->location);
      sfp->location = 
        dnaLoc_to_aaLoc(best_cds, f1->location, TRUE, &frame, FALSE);
      if (sfp->location == NULL) {
      p = SeqLocPrint(f1->location);
      q = SeqLocPrint(best_cds->location);
      ErrPostEx(SEV_ERROR, ERR_FEATURE_CannotMapDnaLocToAALoc, "peptide location:%s| CDS location:%s", p, q);
        MemFree(sfp);
        MemFree(p);
        MemFree(q);
        continue;
      }
      SetSeqLocPartial (sfp->location, partial5, partial3);
      if(f1->comment != NULL)
        MemFree(f1->comment);
      f1->comment = StringSave("FeatureToBeDeleted");
      if (sfp->partial == FALSE) {
        retval = SeqLocPartialCheck(sfp->location);
        if (retval > SLP_COMPLETE && retval < SLP_NOSTART) {
          sfp->partial = TRUE;
        }
      }
      bsp = BioseqLockById(SeqLocId(best_cds->product));
      if (bsp) {
        if (bsp->annot == NULL) {
          sap = SeqAnnotNew();
          sap->type = 1;
          bsp->annot = sap;
        } else {
          sap = bsp->annot;
        }
        sap->data = tie_feat(sap->data, sfp);
        BioseqUnlock(bsp); 
      }
    }
  }

#endif
}

static const string sc_ExtendedCleanupGeneQual("EXTENDEDCLEANUPGENEQUAL:");

// The x_MoveGeneQuals and x_RemoveMarkedGeneXrefs functions replace the MarkMovedGeneGbquals and
// DeleteBadMarkedGeneXrefs functions from the C Toolkit SeriousSeqEntryCleanupEx function.
void CCleanup_imp::x_MoveGeneQuals(const CSeq_feat& orig_feat)
{
    bool found_gene_qual = false;
    ITERATE (CSeq_feat::TQual, it, orig_feat.GetQual()) {
        const CGb_qual& gb_qual = **it;
        if (gb_qual.CanGetQual()
            && NStr::Equal(gb_qual.GetQual(), "gene")) {
            found_gene_qual = true;
        }
    }
    if (found_gene_qual) {
        // if the feature has a gene qualfier, 
        //      create a gene xref
        //      and remove the gene qualifier
                  
        CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, orig_feat);
        CSeq_feat_EditHandle efh(fh);

        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->Assign(orig_feat);
        CSeq_feat::TQual::iterator it = feat->SetQual().begin();
        CSeq_feat::TQual::iterator it_end = feat->SetQual().end();
        while (it != it_end) {
            CGb_qual& gb_qual = **it;
            if (gb_qual.CanGetQual()
                && NStr::Equal(gb_qual.GetQual(), "gene")) {
                // create a gene xref
                if (gb_qual.CanGetVal()) {
                    CGene_ref& grp = feat->SetGeneXref ();
                    grp.SetLocus (sc_ExtendedCleanupGeneQual + gb_qual.GetVal());
                    // note - the change is not recorded here, because the genexref may
                    // be removed later.  The change will be reported in x_RemoveMarkedGeneQuals
                    // when the ExtendedCleanup tag is removed from the gene xref text
                }
                //remove the qual
                it = feat->SetQual().erase(it);
                it_end = feat->SetQual().end();
                ChangeMade(CCleanupChange::eRemoveQualifier);
            } else {
                ++it;
            }                    
        }
        efh.Replace(*feat);
    }
}


void CCleanup_imp::x_MoveGeneQuals(CSeq_annot_Handle sa)
{
    // iterate through features.
    CFeat_CI feat_ci (sa);
    
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene
            || ! feat_ci->IsSetQual()) {
            // do nothing
        } else {
            x_MoveGeneQuals(feat_ci->GetOriginalFeature());
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_MoveGeneQuals(CSeq_entry_Handle seh)
{
    // iterate through features.
    CFeat_CI feat_ci (seh);
    
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene
            || ! feat_ci->IsSetQual()) {
            // do nothing
        } else {
            x_MoveGeneQuals(feat_ci->GetOriginalFeature());
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_MoveGeneQuals(CBioseq_Handle bs)
{
    CSeq_entry_Handle seh = bs.GetSeq_entry_Handle();
    x_MoveGeneQuals (seh);
}


void CCleanup_imp::x_MoveGeneQuals(CBioseq_set_Handle bss)
{
    CSeq_entry_Handle seh = bss.GetParentEntry();
    x_MoveGeneQuals (seh);
}


void CCleanup_imp::x_RemoveMarkedGeneXref(const CSeq_feat& orig_feat)
{
    bool found_flagged_genexref = false;
    ITERATE (CSeq_feat::TXref, it, orig_feat.GetXref()) {
        if ((*it)->IsSetData () && (*it)->GetData ().IsGene ()
            && (*it)->GetData().GetGene().IsSetLocus()
            && NStr::StartsWith ((*it)->GetData().GetGene().GetLocus(), sc_ExtendedCleanupGeneQual)) {
            found_flagged_genexref = true;
        }
    }
    // we found a flagged genexref.
    // either remove the entire genexref, if there is an overlapping gene,
    // or remove the flag if there is no overlapping gene
    if (found_flagged_genexref) {   
        CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, orig_feat);
        CSeq_feat_EditHandle efh(fh);
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->Assign(orig_feat);

        CConstRef<CSeq_feat> gene = sequence::GetBestOverlappingFeat(fh.GetLocation(),
                                                                     CSeqFeatData::eSubtype_gene,
                                                                     sequence::eOverlap_Contains,
                                                                     *m_Scope);
        if (gene) {
            x_RemoveGeneXref(feat);
        } else {
            NON_CONST_ITERATE(CSeq_feat::TXref, it, feat->SetXref ()) {
                if ((*it)->IsSetData () && (*it)->GetData ().IsGene ()
                    && (*it)->GetData().GetGene().IsSetLocus()
                    && NStr::StartsWith ((*it)->GetData().GetGene().GetLocus(), sc_ExtendedCleanupGeneQual)) {
                    (*it)->SetData ().SetGene ().SetLocus ((*it)->GetData().GetGene().GetLocus().substr(sc_ExtendedCleanupGeneQual.length()));
                    ChangeMade(CCleanupChange::eCreateGeneXref);
                }
            }
        }
        efh.Replace (*feat); 
    }
}


void CCleanup_imp::x_RemoveMarkedGeneXrefs(CSeq_annot_Handle sa)
{
    // iterate through features.
    CFeat_CI feat_ci (sa);
    
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene
            || ! feat_ci->IsSetXref()) {
            // do nothing
        } else {
            x_RemoveMarkedGeneXref(feat_ci->GetOriginalFeature());
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_RemoveMarkedGeneXrefs (CSeq_entry_Handle seh)
{
    // iterate through features.
    CFeat_CI feat_ci (seh);
    
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene
            || ! feat_ci->IsSetXref()) {
            // do nothing
        } else {
            x_RemoveMarkedGeneXref(feat_ci->GetOriginalFeature());
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_RemoveMarkedGeneXrefs (CBioseq_Handle bs)
{
    // iterate through features.
    CFeat_CI feat_ci (bs);
    
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene
            || ! feat_ci->IsSetXref()) {
            // do nothing
        } else {
            x_RemoveMarkedGeneXref(feat_ci->GetOriginalFeature());
        }
        ++feat_ci;
    }
}


void CCleanup_imp::x_RemoveMarkedGeneXrefs (CBioseq_set_Handle bss)
{
    CSeq_entry_Handle seh = bss.GetParentEntry();
    x_RemoveMarkedGeneXrefs (seh);
}


// The following two functions replace the move_cds_ex step in the C Toolkit SeriousSeqEntryCleanupEx function
// note that the doPseudo argument does not appear - this is because this value is not actually used by move_cds_ex
// or any of the underlying functions.
void CCleanup_imp::x_MoveCodingRegionsToNucProtSets (CSeq_entry_Handle seh, CSeq_annot_EditHandle parent_sah)
{
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    vector<CSeq_feat_EditHandle> feat_list;

    CFeat_CI feat_ci (seh, sel);
  
    while (feat_ci) {
        if (!feat_ci->IsSetPseudo()
            && feat_ci->IsSetProduct()
            && sequence::GetLength(feat_ci->GetLocation(), m_Scope) >= 6) {
            feat_list.push_back (CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
        }
        ++feat_ci;
    }
 
    ITERATE (vector<CSeq_feat_EditHandle>, it, feat_list) {
        parent_sah.TakeFeat (*it);
        ChangeMade(CCleanupChange::eMoveFeat);
    }
}


void CCleanup_imp::MoveCodingRegionsToNucProtSets (CBioseq_set_Handle bss)
{
    // nothing to do if this set has no members
    if (bss.IsEmptySeq_set()) return;
    
    CBioseq_set_EditHandle bsseh = bss.GetEditHandle();
    // if this is a nuc-prot set, move coding regions from the members to this set
    if (bss.CanGetClass() && bss.GetClass() == CBioseq_set::eClass_nuc_prot) {
        CSeq_annot_CI annot_it(bss.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
        bool found_feat_annot = false;
        for(; annot_it && !found_feat_annot; ++annot_it) {
            if (annot_it->IsFtable()) {
                CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
                list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
            
                ITERATE (list< CRef< CSeq_entry > >, it, set) {
                    x_MoveCodingRegionsToNucProtSets(m_Scope->GetSeq_entryHandle(**it), annot_it->GetEditHandle());
                }
                found_feat_annot = true;
            }
        }
        if (!found_feat_annot) {
            CRef<CSeq_annot> annot(new CSeq_annot);
            annot->SetData().SetFtable();
            bsseh.AttachAnnot(*annot);
            CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
            list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
            
            ITERATE (list< CRef< CSeq_entry > >, it, set) {
                x_MoveCodingRegionsToNucProtSets(m_Scope->GetSeq_entryHandle(**it), 
                                                 m_Scope->GetSeq_annotEditHandle(*annot));
            }
        }
    } else {
        CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
        list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
        ITERATE (list< CRef< CSeq_entry > >, it, set) {
            if ((**it).Which() == CSeq_entry::e_Set) {
                MoveCodingRegionsToNucProtSets (m_Scope->GetBioseq_setHandle((**it).GetSet()));
            }
        }
    }
}


void CCleanup_imp::x_RemoveFeaturesBySubtype (const CSeq_entry& se, CSeqFeatData::ESubtype subtype)
{
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            x_RemoveFeaturesBySubtype(m_Scope->GetBioseqHandle(se.GetSeq()), subtype);
            break;
        case CSeq_entry::e_Set:
            x_RemoveFeaturesBySubtype(m_Scope->GetBioseq_setHandle(se.GetSet()), subtype);
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::x_RemoveFeaturesBySubtype (CBioseq_Handle bs, CSeqFeatData::ESubtype subtype)
{
    vector<CSeq_feat_EditHandle> feat_list;
    SAnnotSelector sel(subtype);
    CFeat_CI feat_ci(bs, subtype);
    while (feat_ci) {
        feat_list.push_back (CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
        ++feat_ci;
    }
    ITERATE (vector<CSeq_feat_EditHandle>, it, feat_list) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveFeat);
    }
}


void CCleanup_imp::x_RemoveFeaturesBySubtype (CBioseq_set_Handle bss, CSeqFeatData::ESubtype subtype)
{
    vector<CSeq_feat_EditHandle> feat_list;
    SAnnotSelector sel(subtype);
    CFeat_CI feat_ci(bss.GetParentEntry(), subtype);
    while (feat_ci) {
        feat_list.push_back (CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
        ++feat_ci;
    }
    ITERATE (vector<CSeq_feat_EditHandle>, it, feat_list) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveFeat);
    }
}


void CCleanup_imp::x_RemoveImpSourceFeatures (CSeq_annot_Handle sa) 
{
    vector<CSeq_feat_EditHandle> feat_list;
    if (sa.IsFtable()) {
        SAnnotSelector sel(CSeqFeatData::e_Imp);
        CFeat_CI feat_ci(sa, sel);
        while (feat_ci) {
            if (NStr::Equal(feat_ci->GetData().GetImp().GetKey(), "source")) {
                CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                feat_list.push_back (CSeq_feat_EditHandle(feat_ci->GetSeq_feat_Handle()));
            }
            ++feat_ci;
        }    
    }
    ITERATE (vector<CSeq_feat_EditHandle>, it, feat_list) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveFeat);
    }    
}


void CCleanup_imp::x_RemoveSiteRefImpFeats(CSeq_annot_Handle sa)
{
    if (!sa.IsFtable()) {
        return;
    }
    SAnnotSelector sel(CSeqFeatData::e_Imp);
    CFeat_CI feat_ci(sa, sel);

    vector<CSeq_feat_EditHandle> feat_list;
    
    while (feat_ci) {
        if (NStr::Equal(feat_ci->GetData().GetImp().GetKey(), "Site-ref")) {
            feat_list.push_back (CSeq_feat_EditHandle(feat_ci->GetSeq_feat_Handle()));
        }
        ++feat_ci;
    }
    ITERATE (vector<CSeq_feat_EditHandle>, it, feat_list) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveFeat);
    }    
}


static void s_AddFeat(CBioseq_EditHandle bh, CRef<CSeq_feat> new_feat) 
{
    CSeq_annot_EditHandle feat_annot;
    CSeq_annot_CI annot_it(bh);
    for(; annot_it; ++annot_it) {
        const CSeq_annot_Handle& annot = *annot_it;
        if (annot.IsFtable()) {
            feat_annot = annot.GetEditHandle();
            break;
        }
    }
    if (feat_annot) {
        feat_annot.AddFeat(*new_feat);
    } else {
        CRef<CSeq_annot> new_annot(new CSeq_annot);
        new_annot->SetData().SetFtable().push_back(new_feat);
        bh.AttachAnnot(*new_annot);
    }
}


// This function was StripProtXref in the C Toolkit
// Note - in the C Toolkit, this function stopped after
// finding the first feature on the sequence that was NOT
// a coding region.  This is believed to be an error,
// this function will handle all coding region features
// on every sequence.
void CCleanup_imp::x_StripProtXrefs(CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        objects::SAnnotSelector cds_sel(CSeqFeatData::eSubtype_cdregion);
        
        CFeat_CI feat_ci(sa, cds_sel);
        while (feat_ci) {
            if (feat_ci->IsSetProduct()
                && feat_ci->IsSetXref()) {
                try {
                    CBioseq_EditHandle eh = (m_Scope->GetBioseqHandle(feat_ci->GetProduct())).GetEditHandle();
                    CSeq_feat_Handle fh = feat_ci->GetSeq_feat_Handle();
                    CRef<CSeq_feat> new_feat(new CSeq_feat);
                    new_feat->Assign(feat_ci->GetOriginalFeature());
                    bool change_made = false;                                
                    CSeq_feat::TXref& xrefs = new_feat->SetXref();
                    CSeq_feat::TXref::iterator it = xrefs.begin();
                    while (it != xrefs.end()) {
                        CSeqFeatXref& xref = **it;
                        if (xref.IsSetData()  &&  xref.GetData().IsProt()) {
                            CRef<CSeq_feat> new_prot(new CSeq_feat);
                            new_prot->SetData().SetProt(xref.SetData().SetProt());
                            CRef<CSeq_loc> new_prot_loc(new CSeq_loc);
                            new_prot_loc->Assign(feat_ci->GetProduct());
                            new_prot->SetLocation(*new_prot_loc);
                        
                            s_AddFeat(eh, new_prot);    
                            ChangeMade(CCleanupChange::eAddProtFeat);   
                            change_made = true;
                            it = xrefs.erase(it);                 
                        } else {
                            ++it;
                        }
                    }
                    if (change_made) {
                        CSeq_feat_EditHandle efh(fh);
                        efh.Replace(*new_feat);
                        ChangeMade(CCleanupChange::eRemoveProtXref);
                    }
                } catch (...) {
                    // skip this one
                }
            }
            ++feat_ci;
        } 
    }
}


// This function includes code that was GetAnticodonFromObject in the C Toolkit
void CCleanup_imp::x_ConvertUserObjectToAnticodon(CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        return;
    }
    
    objects::SAnnotSelector trna_sel(CSeqFeatData::eSubtype_tRNA);
        
    CFeat_CI feat_ci(sa, trna_sel);
    while (feat_ci) {
        if (!feat_ci->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon()
            && feat_ci->IsSetExt()
            && feat_ci->GetExt().IsSetClass()
            && NStr::EqualNocase(feat_ci->GetExt().GetClass(), "NCBI")
            && feat_ci->GetExt().CanGetData()
            && feat_ci->GetExt().GetData().front()->CanGetData()
            && feat_ci->GetExt().GetData().front()->GetData().IsInt()
            && feat_ci->GetExt().GetData().front()->GetData().GetInts().size() > 1) {
        
            CSeq_feat_EditHandle efh(feat_ci->GetSeq_feat_Handle());
            CRef<CSeq_feat> new_feat(new CSeq_feat);
            new_feat->Assign(feat_ci->GetOriginalFeature());
            CRef<CSeq_id> id (new CSeq_id);
            id->Assign(*(new_feat->GetLocation().GetId()));
        
            CRef<CSeq_loc> anticodon_loc(new CSeq_loc(*id, 
                                                      feat_ci->GetExt().GetData().front()->GetData().GetInts()[0],
                                                      feat_ci->GetExt().GetData().front()->GetData().GetInts()[1],
                                                      feat_ci->GetLocation().GetStrand()));
        
            new_feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon(*anticodon_loc);
        
            // remove the first user object
            new_feat->SetExt().SetData().erase(new_feat->SetExt().SetData().begin());
            if (new_feat->GetExt().GetData().size() == 0) {
                new_feat->SetExt().ResetData();
            }
            efh.Replace(*new_feat);
            ChangeMade(CCleanupChange::eChangeAnticodon);
        }
        
        ++feat_ci;
    }
}


// this function was MapsToGenRef in the C Toolkit
// For every gene feature, it checks all of the features whose locations are 
// contained in the gene feature location for map qualifiers.
// If all of the features with map qualifiers have the same map qualifier value,
// if the gene feature has no map loc, the map value will be copied to the gene
// feature maploc.  If all of the features with map qualifiers have the same map
// qualifier value, the map qualifiers will be removed.
void CCleanup_imp::x_MoveMapQualsToGeneMaploc (CSeq_annot_Handle sa)
{
    objects::SAnnotSelector gene_sel(CSeqFeatData::eSubtype_gene);
    CFeat_CI gene_ci(sa, gene_sel);
    while (gene_ci) {
        CFeat_CI overlapped_feat_ci (*m_Scope, gene_ci->GetOriginalFeature().GetLocation());
        string map = "";
        bool   same = true;
        bool   any = false;
        while (overlapped_feat_ci && same) {
            // check to make sure feature is contained in (or same location as) gene
            sequence::ECompare loc_compare = sequence::Compare(overlapped_feat_ci->GetOriginalFeature().GetLocation(),
                                                               gene_ci->GetOriginalFeature().GetLocation(),
                                                               m_Scope);
            if ((loc_compare == sequence::eContained
                  || loc_compare == sequence::eSame)
                  && !overlapped_feat_ci->GetData().IsGene() 
                  && overlapped_feat_ci->IsSetQual()) {
                ITERATE (CSeq_feat::TQual, qual_it, overlapped_feat_ci->GetQual()) {
                    if ((*qual_it)->CanGetQual() 
                        && NStr::Equal((*qual_it)->GetQual(), "map")
                        && (*qual_it)->CanGetVal()) {
                        if (any) {
                            if (!NStr::Equal(map, (*qual_it)->GetVal())) {
                                same = false;
                            }
                        } else {
                            any = true;
                            map = (*qual_it)->GetVal();
                        }
                    }
                }
            }
            ++overlapped_feat_ci;
        }
        if (any && same) {
            string gene_map = "";
            if(!gene_ci->GetData().GetGene().IsSetMaploc()
               || NStr::IsBlank(gene_ci->GetData().GetGene().GetMaploc())) {
                CSeq_feat_EditHandle efh(gene_ci->GetSeq_feat_Handle());
                CRef<CSeq_feat> new_feat(new CSeq_feat);
                new_feat->Assign(gene_ci->GetOriginalFeature());
                new_feat->SetData().SetGene().SetMaploc(map);
                efh.Replace(*new_feat);
                ChangeMade(CCleanupChange::eChangeOther);                    
                gene_map = map;
            } else {
                gene_map = gene_ci->GetData().GetGene().GetMaploc();
            }
        
            if (NStr::Equal (gene_map, map)) {
                overlapped_feat_ci.Rewind();
                while (overlapped_feat_ci) {
                    bool changed = false;
                    // check to make sure feature is contained in (or same location as) gene
                    sequence::ECompare loc_compare = 
                                sequence::Compare(overlapped_feat_ci->GetOriginalFeature().GetLocation(),
                                                  gene_ci->GetOriginalFeature().GetLocation(),
                                                  m_Scope);
                    if ((loc_compare == sequence::eContained
                         || loc_compare == sequence::eSame)
                        && !overlapped_feat_ci->GetData().IsGene()
                        && overlapped_feat_ci->IsSetQual()) {
                        CSeq_feat_EditHandle efh(overlapped_feat_ci->GetSeq_feat_Handle());
                        CRef<CSeq_feat> new_feat(new CSeq_feat);
                        new_feat->Assign(overlapped_feat_ci->GetOriginalFeature());
                        CSeq_feat::TQual::iterator qual_it = new_feat->SetQual().begin();
                        while (qual_it != new_feat->SetQual().end()) {
                            if ((*qual_it)->CanGetQual() 
                                && NStr::Equal((*qual_it)->GetQual(), "map")) {
                                qual_it = new_feat->SetQual().erase(qual_it);
                                changed = true;
                            } else {
                                ++qual_it;
                            }
                        }
                        if (changed) {
                            efh.Replace(*new_feat);
                            ChangeMade(CCleanupChange::eRemoveQualifier);
                        }
                    }
                    ++overlapped_feat_ci;   
                }
            }
            
        }
        ++gene_ci;
    }
}


void CCleanup_imp::x_ConvertOrgFeatToSource(CSeq_annot_Handle sa)
{
    if (!sa.IsFtable()) {
        return;
    }
    
    SAnnotSelector sel(CSeqFeatData::e_Org);
    CFeat_CI feat_ci (sa, sel);

    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Org) {
            // convert Org feat to Source feat
            CRef<CSeq_feat> feat(new CSeq_feat);
            const CSeq_feat& orig_feat = feat_ci->GetOriginalFeature();
            feat->Assign(orig_feat);
            feat->SetData().SetBiosrc().SetOrg(feat->SetData().SetOrg());
            CSeq_feat_Handle ofh = feat_ci->GetSeq_feat_Handle();
            CSeq_feat_EditHandle efh(ofh);
            efh.Replace(*feat);
            ChangeMade (CCleanupChange::eConvertFeature);
        }
        ++feat_ci;
    }
    
}


void CCleanup_imp::x_ConvertOrgFeatToSource(CBioseq_set_Handle bh)
{
    for (CSeq_annot_CI annot_it(bh.GetParentEntry(), CSeq_annot_CI::eSearch_entry); annot_it; ++annot_it) {
        x_ConvertOrgFeatToSource(*annot_it);
    }
    return;
}


void CCleanup_imp::x_ConvertOrgFeatToSource(CBioseq_Handle bh)
{
    for (CSeq_annot_CI annot_it(bh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry); annot_it; ++annot_it) {
        x_ConvertOrgFeatToSource(*annot_it);
    }
}


// This was a step in CheckGCode in the C Toolkit
bool CCleanup_imp::x_RemovePIDXrefs (CSeq_feat& feat)
{
    bool change_made = false;
    if (feat.IsSetProduct() && feat.IsSetDbxref()) {
        CSeq_feat::TDbxref& xrefs = feat.SetDbxref();
        CSeq_feat::TDbxref::iterator it = xrefs.begin();
        while (it != xrefs.end()) {
            CDbtag& xref = **it;
            if (xref.CanGetDb() && NStr::StartsWith(xref.GetDb(), "PID")) {
                it = xrefs.erase(it);
                ChangeMade(CCleanupChange::eChangeDbxrefs);
                change_made = true;
            } else {
                ++it;
            }
        }
    }
    return change_made;
}


bool CCleanup_imp::x_FixPIDDbtag (CSeq_id& id)
{
    bool change_made = false;
    
    if (id.IsGeneral()) {
        CDbtag& dbtag = id.SetGeneral();
        if (dbtag.IsSetDb()) {
            if (NStr::StartsWith(dbtag.GetDb(), "PIDe")) {
                string new_str = "e" + NStr::DoubleToString(dbtag.GetTag().GetId());
                dbtag.SetDb("PID");
                dbtag.SetTag().SetStr(new_str);
                change_made = true;
            } else if (NStr::StartsWith(dbtag.GetDb(), "PIDd")) {
                string new_str = "d" + NStr::DoubleToString(dbtag.GetTag().GetId());
                dbtag.SetDb("PID");
                dbtag.SetTag().SetStr(new_str);
                change_made = true;
            }
        }
    }
    return change_made;
}


bool CCleanup_imp::x_FixPIDDbtag (CSeq_loc& loc)
{
    bool change_made = false;
    switch (loc.Which()) {
        case CSeq_loc::e_Int:
            if (loc.GetInt().CanGetId()) {
                change_made = x_FixPIDDbtag (loc.SetInt().SetId());
            }
            break; 
        case CSeq_loc::e_Pnt:
            if (loc.GetPnt().CanGetId()) {
                change_made = x_FixPIDDbtag (loc.SetPnt().SetId());
            }
            break;
        case CSeq_loc::e_Bond:
        {
            CSeq_bond& bond = loc.SetBond();
            if (bond.CanGetA() && bond.GetA().CanGetId()) {
                change_made = x_FixPIDDbtag (bond.SetA().SetId());
            }
            if (bond.CanGetB() && bond.GetB().CanGetId()) {
                change_made |= x_FixPIDDbtag (bond.SetA().SetId());
            }
            break;
        }
        case CSeq_loc::e_Empty:
            change_made = x_FixPIDDbtag(loc.SetEmpty());
            break;
        case CSeq_loc::e_Whole:
            change_made = x_FixPIDDbtag(loc.SetWhole());
            break;
        case CSeq_loc::e_Equiv:
            NON_CONST_ITERATE (CSeq_loc::TLocations, loc_it, loc.SetEquiv().Set()) {
                change_made |= x_FixPIDDbtag(**loc_it);
            }
            break;
        case CSeq_loc::e_Mix:
            NON_CONST_ITERATE (CSeq_loc::TLocations, loc_it, loc.SetMix().Set()) {
                change_made |= x_FixPIDDbtag(**loc_it);
            }
            break;
        case CSeq_loc::e_Packed_int:
            NON_CONST_ITERATE (CSeq_loc::TIntervals, loc_it, loc.SetPacked_int().Set()) {
                if ((*loc_it)->CanGetId()) {
                    change_made |= x_FixPIDDbtag((*loc_it)->SetId());
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (loc.GetPacked_pnt().CanGetId()) {
                change_made = x_FixPIDDbtag(loc.SetPacked_pnt().SetId());
            }
            break;
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Null:
            break;
    }
    if (change_made) {
        ChangeMade(CCleanupChange::eChangeSeqloc);
    }
    return change_made;
}


bool CCleanup_imp::x_FixPIDDbtag (CSeq_feat& feat)
{
    bool feat_change_made = false;
    
    if (feat.IsSetProduct()) {
        CSeq_loc& product_loc = feat.SetProduct();
        CBioseq_EditHandle product = (m_Scope->GetBioseqHandle(product_loc)).GetEditHandle();
        feat_change_made = x_FixPIDDbtag(product_loc);
        if (feat_change_made) {
            feat.SetProduct(product_loc);
        }
        
        if (product.IsSetId()) {
            list <CSeq_id_Handle> remove_list;
            list <CSeq_id_Handle> add_list;
            for (unsigned int i = 0; i < product.GetId().size(); i++) {
                if (product.GetId()[i].GetSeqId()->IsGeneral()) {
                    CRef<CSeq_id> new_id(new CSeq_id);
                    new_id->Assign(*(product.GetId()[i].GetSeqId()));
                    if (x_FixPIDDbtag(*new_id)) {
                        feat_change_made = true;
                        add_list.push_back(CSeq_id_Handle::GetHandle (*new_id));
                        remove_list.push_back(product.GetId()[i]);
                        ChangeMade(CCleanupChange::eChangeSeqId);
                    }
                }
            }
            if (remove_list.size() > 0 || add_list.size() > 0) {
                ITERATE (list <CSeq_id_Handle>, id_it, remove_list) {
                    product.RemoveId(*id_it);
                }
                ITERATE (list <CSeq_id_Handle>, id_it, add_list) {
                    product.AddId(*id_it);
                }
            }
            CFeat_CI feat_ci(product);
            while (feat_ci) {
                CSeq_feat_EditHandle feath(feat_ci->GetSeq_feat_Handle());
                CRef <CSeq_feat> new_feat(new CSeq_feat);
                new_feat->Assign (feat_ci->GetOriginalFeature());
                if (x_FixPIDDbtag(*new_feat)) {
                    feath.Replace(*new_feat);
                }
                ++feat_ci;
            }
        }
    }
    return feat_change_made;
}


// This step was part of CheckGCode in the C Toolkit
void CCleanup_imp::x_FixProteinIDs (CSeq_annot_Handle sa)
{
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);

    CFeat_CI feat_ci(sa, sel);
    while (feat_ci) {
        bool change_made = false;
        CSeq_feat_EditHandle feath(feat_ci->GetSeq_feat_Handle());
        CRef <CSeq_feat> new_feat(new CSeq_feat);
        new_feat->Assign (feat_ci->GetOriginalFeature());
        change_made = x_RemovePIDXrefs (*new_feat);
        change_made |= x_FixPIDDbtag(*new_feat);
        if (change_made) {
            feath.Replace(*new_feat);
        }
        ++feat_ci;
    }
}


// This step was part of CheckGCode in the C Toolkit
bool CCleanup_imp::x_CheckConflictFlag (CSeq_feat& feat)
{
    bool change_made = false;
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() 
        || !feat.GetData().GetCdregion().IsSetConflict()
        || !feat.GetData().GetCdregion().GetConflict()
        || !feat.IsSetProduct()) {
        return false;
    }
    CBioseq_Handle product = m_Scope->GetBioseqHandle(feat.GetProduct());
    CSeqVector prot_vector = product.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    
    string product_str;
    prot_vector.GetSeqData (0, product.GetBioseqLength(), product_str);    
    string transl_prot;   // translated protein
    bool alt_start = false;
    try {
        CCdregion_translate::TranslateCdregion(
            transl_prot, 
            feat, 
            *m_Scope,
            false,   // do not include stop codons
            true,  // remove trailing X/B/Z
            &alt_start);
    } catch (CException&) {
    }
    if (!transl_prot.empty() 
        && NStr::EqualNocase(transl_prot, product_str)) {
        feat.SetData().SetCdregion().ResetConflict();
        change_made = true;
    }
    return change_made;
}


void CCleanup_imp::x_CheckConflictFlag(CSeq_annot_Handle sa)
{
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);

    CFeat_CI feat_ci(sa, sel);
    while (feat_ci) {
        CSeq_feat_EditHandle feath(feat_ci->GetSeq_feat_Handle());
        CRef <CSeq_feat> new_feat(new CSeq_feat);
        new_feat->Assign (feat_ci->GetOriginalFeature());
        if (x_CheckConflictFlag (*new_feat)) {
            ChangeMade (CCleanupChange::eChangeOther);
            feath.Replace(*new_feat);
        }
        ++feat_ci;
    }
}


static const string sc_Asn2FFAuthor("Author-given protein sequence is in conflict with the conceptual translation.");
static const string sc_Asn2FFMethod("Method: conceptual translation supplied by author.");

bool CCleanup_imp::x_RemoveAsn2ffGeneratedComments (CSeq_feat& feat)
{
    if (!feat.IsSetComment()) return false;
    string comment = feat.GetComment();
    
    NStr::ReplaceInPlace (comment, sc_Asn2FFAuthor, "");
    NStr::ReplaceInPlace (comment, sc_Asn2FFMethod, "");
    NStr::TruncateSpacesInPlace (comment);
    if (comment[comment.length() - 1] == ';') {
        comment = comment.substr(0, comment.length() - 1);
    }
    if (!NStr::Equal(comment, feat.GetComment())) {
        feat.SetComment(comment);
        ChangeMade (CCleanupChange::eChangeComment);
        return true;
    } else {
        return false;
    }
}


void CCleanup_imp::x_RemoveAsn2ffGeneratedComments(CSeq_annot_Handle sa)
{
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);

    CFeat_CI feat_ci(sa, sel);
    while (feat_ci) {
        CSeq_feat_EditHandle feath(feat_ci->GetSeq_feat_Handle());
        CRef <CSeq_feat> new_feat(new CSeq_feat);
        new_feat->Assign (feat_ci->GetOriginalFeature());
        if (x_RemoveAsn2ffGeneratedComments (*new_feat)) {
            feath.Replace(*new_feat);
        }
        ++feat_ci;
    }
}

void CCleanup_imp::x_MoveFeaturesOnPartsToCorrectSeqAnnots(CBioseq_set_Handle bsh)
{
    CConstRef<CBioseq_set> bs = bsh.GetCompleteBioseq_set();
    if (!bs || !bs->IsSetSeq_set()) {
        return;
    }

    list< CRef< CSeq_entry > > set = (*bs).GetSeq_set();
       
    if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
        // find master sequence and parts set
        CBioseq_Handle     master_seq;
        CBioseq_set_Handle parts_set;
        bool               found_master_seq = false, found_parts_set = false;

        ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    master_seq = m_Scope->GetBioseqHandle ((*it)->GetSeq());
                    found_master_seq = true;
                    break;
                case CSeq_entry::e_Set:
                    parts_set = m_Scope->GetBioseq_setHandle((*it)->GetSet());
                    found_parts_set = true;
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
        if (!found_master_seq || !found_parts_set) {
            return;
        }

        // copy seqfeat handles to not break iterator while moving
        // also, don't need to create SeqAnnot on master sequence unless there
        // are features to put in it
        vector<CSeq_feat_EditHandle> sfh; 

        CFeat_CI feat_ci(parts_set.GetParentEntry());
        while (feat_ci) {
            const CSeq_loc& feat_loc = feat_ci->GetOriginalFeature().GetLocation();
            if (!feat_loc.GetId()) {
                // add to list to move
                sfh.push_back(CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
            }
            ++feat_ci;
        } 

        if (sfh.size() > 0) {
            CSeq_annot_EditHandle sah;
            CSeq_annot_CI annot_it(master_seq.GetParentEntry(), CSeq_annot_CI::eSearch_entry);

            while (annot_it && !annot_it->IsFtable()) {
                ++annot_it;
            }
            if (annot_it) {
                sah = (*annot_it).GetEditHandle();
            } else {
                CRef<CSeq_annot> new_annot(new CSeq_annot);
                new_annot->SetData().SetFtable();                        
                CBioseq_EditHandle master_edit = master_seq.GetEditHandle();
                master_edit.AttachAnnot(*new_annot);
                sah = m_Scope->GetSeq_annotEditHandle(*new_annot);
            }

            ITERATE (vector<CSeq_feat_EditHandle>, it, sfh) {
                sah.TakeFeat (*it);
                ChangeMade(CCleanupChange::eMoveFeat);
            }
        }
    } else {
        // look for segsets in this set       
        ITERATE (list< CRef< CSeq_entry > >, it, set) {
            if ((*it)->IsSet()) {
                x_MoveFeaturesOnPartsToCorrectSeqAnnots (m_Scope->GetBioseq_setHandle((*it)->GetSet()));
            }
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
