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
#include <util/static_map.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/seqport_util.hpp>
#include <vector>

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
        NStr::Find(except_text, "non-consensus splice site") == NPOS) {
        return;
    }

    vector<string> exceptions;
    NStr::Tokenize(except_text, ",", exceptions);

    NON_CONST_ITERATE(vector<string>, it, exceptions) {
        string& text = *it;
        NStr::TruncateSpacesInPlace(text);
        if (!text.empty()) {
            if (text == "ribosome slippage") {
                text = "ribosomal slippage";
            } else if (text == "trans splicing") {
                text = "trans-splicing";
            } else if (text == "alternate processing") {
                text = "alternative processing";
            } else if (text == "adjusted for low quality genome") {
                text = "adjusted for low-quality genome";
            } else if (text == "non-consensus splice site") {
                text = "nonconsensus splice site";
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
static const TSiteMap sc_SiteMap(sc_site_map, sizeof(sc_site_map));


// === Prot

static const string uninf_names[] = {
    "peptide", "putative", "signal", "signal peptide", "signal-peptide",
    "signal_peptide", "transit", "transit peptide", "transit-peptide",
    "transit_peptide", "unknown", "unnamed"
};
typedef CStaticArraySet<string, PNocase> TUninformative;
static const TUninformative sc_UninfNames(uninf_names, sizeof(uninf_names));

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
static const TRnaTypeMap sc_RnaTypeMap(sc_rna_type_map, sizeof(sc_rna_type_map));


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
                }
            }
                
                // move db_xrefs from the Gene-ref or gene Xrefs to the feature
                // move db_xref from Gene-ref to feature
            if (gene.IsSetDb()) {
                copy(gene.GetDb().begin(), gene.GetDb().end(), back_inserter(feat.SetDbxref()));
                gene.ResetDb();
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
                        }
                        // remove gene xref if it has no values set
                        if (s_IsEmptyGeneRef(gref)) {
                            it = xrefs.erase(it);
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
                            }
                        }
                        // remove uninformative names
                        if (!s_IsInformativeName(*it)) {
                            it = name.erase(it);
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
            }
            
            if (imp.IsSetKey()) {
                const CImp_feat::TKey& key = imp.GetKey();
                
                if (key == "CDS") {
                    if ( ! x_IsEmblDdbj() ) {
                        data.SetCdregion();
                        //s_CleanupCdregion(feat);
                    }
                } else if (!imp.IsSetLoc()  ||  NStr::IsBlank(imp.GetLoc())) {
                    TRnaTypeMap::const_iterator rna_type_it = sc_RnaTypeMap.find(key);
                    if (rna_type_it != sc_RnaTypeMap.end()) {
                        CSeqFeatData::TRna& rna = data.SetRna();
                        rna.SetType(rna_type_it->second);
                        BasicCleanup(rna);
                    } else {
                        // !!! need to find protein bioseq without object manager
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Region:
        {            
            string &region = data.SetRegion();
            CleanString(region);
            ConvertDoubleQuotes(region);
            if (region.empty()) {
                feat.SetData().SetComment();
            }
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


// === Seq-feat.dbxref


struct SDbtagCompare
{
    // is dbt1 < dbt2
    bool operator()(const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2) {
        return dbt1->Compare(*dbt2) < 0;
    }
};


struct SDbtagEqual
{
    // is dbt1 < dbt2
    bool operator()(const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2) {
        return dbt1->Compare(*dbt2) == 0;
    }
};


void CCleanup_imp::x_CleanupDbxref(CSeq_feat& feat)
{
    _ASSERT(feat.IsSetDbxref());

    CSeq_feat::TDbxref& dbxref = feat.SetDbxref();

    // dbxrefs cleanup
    CSeq_feat::TDbxref::iterator it = dbxref.begin();
    while (it != dbxref.end()) {
        if (it->Empty()) {
            it = dbxref.erase(it);
            continue;
        }
        BasicCleanup(**it);

        ++it;
    }

    // sort/unique db_xrefs
    stable_sort(dbxref.begin(), dbxref.end(), SDbtagCompare());
    it = unique(dbxref.begin(), dbxref.end(), SDbtagEqual());
    dbxref.erase(it, dbxref.end());
}   


// BasicCleanup
void CCleanup_imp::BasicCleanup(CSeq_feat& f)
{
    if (!f.IsSetData()) {
        return;
    }

    CLEAN_STRING_MEMBER(f, Comment);
    if (f.IsSetComment()  &&  f.GetComment() == ".") {
        f.ResetComment();
    }
    CLEAN_STRING_MEMBER(f, Title);
    CLEAN_STRING_MEMBER(f, Except_text);
    if (f.IsSetExcept_text()) {
        x_CleanupExcept_text(f.SetExcept_text());
    }

    BasicCleanup(f.SetData());
    BasicCleanup(f, f.SetData());

    if (f.IsSetDbxref()) {
       x_CleanupDbxref(f);
    }
    if (f.IsSetQual()) {
        x_SortUniqueQuals(f.SetQual());
        
        CSeq_feat::TQual::iterator it = f.SetQual().begin();
        CSeq_feat::TQual::iterator it_end = f.SetQual().end();
        while (it != it_end) {
            CGb_qual& gb_qual = **it;
            // clean up this qual.
            BasicCleanup(gb_qual);
            // cleanup the feature given this qual.
            if (BasicCleanup(f, gb_qual)) {
                it = f.SetQual().erase(it);
            } else {
                ++it;
            }
        }
        // expand out all combined quals.
        x_ExpandCombinedQuals(f.SetQual());
    }
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
    CLEAN_STRING_LIST(prot_ref, Name);
    CLEAN_STRING_LIST(prot_ref, Ec);
    CLEAN_STRING_LIST(prot_ref, Activity);
    
    if (prot_ref.IsSetProcessed()  &&  !prot_ref.IsSetName()) {
        CProt_ref::TProcessed processed = prot_ref.GetProcessed();
        if (processed == CProt_ref::eProcessed_preprotein  ||  
            processed == CProt_ref::eProcessed_mature) {
            prot_ref.SetName().push_back("unnamed");
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
                    CleanString(name);
                    
                    if (name.empty()) {
                        rr.ResetExt();
                    } else if (rr.IsSetType()) {
                        switch (rr.GetType()) {
                            case CRNA_ref::eType_rRNA:
                            {{
                                size_t len = name.length();
                                if (len >= rRNA.length()                       &&
                                    NStr::EndsWith(name, rRNA, NStr::eNocase)  &&
                                    !NStr::EndsWith(name, kRibosomalrRna, NStr::eNocase)) {
                                    name.replace(len - rRNA.length(), name.size(), kRibosomalrRna);
                                }
                                break;
                            }}
                            case CRNA_ref::eType_tRNA:
                            {{
                                // !!!
                                break;
                            }}
                            case CRNA_ref::eType_other:
                            {{
                                if (NStr::EqualNocase(name, "its1")) {
                                    name = "internal transcribed spacer 1";
                                } else if (NStr::EqualNocase(name, "its2")) {
                                    name = "internal transcribed spacer 2";
                                } else if (NStr::EqualNocase(name, "its3")) {
                                    name = "internal transcribed spacer 3";
                                } else if (NStr::EqualNocase(name, "its")) {
                                    name = "internal transcribed spacer";
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
                break;
            default:
                break;
        }
    }
}



// Imp_feat cleanup
void CCleanup_imp::BasicCleanup(CImp_feat& imf)
{
    CLEAN_STRING_MEMBER(imf, Key);
    CLEAN_STRING_MEMBER(imf, Loc);
    CLEAN_STRING_MEMBER(imf, Descr);
    
    if (imf.IsSetKey()) {
        const CImp_feat::TKey& key = imf.GetKey();
        if (key == "allele"  ||  key == "mutation") {
            imf.SetKey("variation");
        }
    }
}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.6  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.5  2006/03/29 19:40:13  rsmith
 * Consolidate BasicCleanup(CRNA_ref) stuff.
 *
 * Revision 1.4  2006/03/29 16:36:56  rsmith
 * Move all gbqual stuff to its own file.
 * Lots of other refactoring.
 *
 * Revision 1.3  2006/03/23 18:33:32  rsmith
 * Separate straight BasicCleanup of SFData and GBQuals from more complicated
 * changes to various parts of the Seq_feat.
 *
 * Revision 1.2  2006/03/20 14:21:25  rsmith
 * move Biosource related cleanup to its own file.
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */
