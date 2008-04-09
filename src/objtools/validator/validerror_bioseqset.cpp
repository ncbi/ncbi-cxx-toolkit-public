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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of bioseq_set 
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include "validatorp.hpp"

#include <objmgr/util/sequence.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseqset::CValidError_bioseqset(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_bioseqset::~CValidError_bioseqset(void)
{
}


void CValidError_bioseqset::ValidateBioseqSet(const CBioseq_set& seqset)
{
    int protcnt = 0;
    int nuccnt  = 0;
    int segcnt  = 0;
    
    // Validate Set Contents
    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {
        
        if ( seqit->IsAa() ) {
            protcnt++;
        } else if ( seqit->IsNa() ) {
            nuccnt++;
        }
        
        if (seqit->GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            segcnt++;
        }
    }
    
    switch ( seqset.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        ValidateNucProtSet(seqset, nuccnt, protcnt);
        break;
        
    case CBioseq_set::eClass_segset:
        ValidateSegSet(seqset, segcnt);
        break;
        
    case CBioseq_set::eClass_parts:
        ValidatePartsSet(seqset);
        break;
        
    case CBioseq_set::eClass_genbank:
        ValidateGenbankSet(seqset);
        break;
        
    case CBioseq_set::eClass_pop_set:
        ValidatePopSet(seqset);
        break;
        
    case CBioseq_set::eClass_mut_set:
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_eco_set:
    case CBioseq_set::eClass_wgs_set:
        ValidatePhyMutEcoWgsSet(seqset);
        break;
        
    case CBioseq_set::eClass_gen_prod_set:
        ValidateGenProdSet(seqset);
        break;
    /*
    case CBioseq_set::eClass_other:
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GenomicProductPackagingProblem, 
            "Genomic product set class incorrectly set to other", seqset);
        break;
    */
    default:
        if ( nuccnt == 0  &&  protcnt == 0 )  {
            PostErr(eDiag_Error, eErr_SEQ_PKG_EmptySet, 
                "No Bioseqs in this set", seqset);
        }
        break;
    }
}


// =============================================================================
//                                     Private
// =============================================================================


bool CValidError_bioseqset::IsMrnaProductInGPS(const CBioseq& seq)
{
    if ( m_Imp.IsGPS() ) {
        CFeat_CI mrna(
            m_Scope->GetBioseqHandle(seq), 
            SAnnotSelector(CSeqFeatData::e_Rna)
            .SetByProduct());
        return (bool)mrna;
    }
    return true;
}


bool CValidError_bioseqset::IsCDSProductInGPS(const CBioseq& seq)
{
    if ( m_Imp.IsGPS() ) {
        CFeat_CI cds(
            m_Scope->GetBioseqHandle(seq), 
            SAnnotSelector(CSeqFeatData::e_Cdregion)
            .SetByProduct());
        return (bool)cds;
    }
    return true;
}


void CValidError_bioseqset::ValidateNucProtSet
(const CBioseq_set& seqset,
 int nuccnt, 
 int protcnt)
{
    if ( nuccnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No nucleotides in nuc-prot set", seqset);
    }
    if ( protcnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No proteins in nuc-prot set", seqset);
    }

    ITERATE ( CBioseq_set::TSeq_set, se_list_it, seqset.GetSeq_set() ) {
        if ( (*se_list_it)->IsSeq() ) {
            const CBioseq& seq = (*se_list_it)->GetSeq();
            if ( seq.IsNa()  &&  !IsMrnaProductInGPS(seq) ) {
                PostErr(eDiag_Warning,
                    eErr_SEQ_PKG_GenomicProductPackagingProblem,
                    "Nucleotide bioseq should be product of mRNA "
                    "feature on contig, but is not",
                    seq);
            } else if ( seq.IsAa()  &&  !IsCDSProductInGPS(seq) ) {
                PostErr(eDiag_Warning,
                    eErr_SEQ_PKG_GenomicProductPackagingProblem,
                    "Protein bioseq should be product of CDS "
                    "feature on contig, but is not",
                    seq);
            }
        }

        if ( !(*se_list_it)->IsSet() )
            continue;

        const CBioseq_set& set = (*se_list_it)->GetSet();
        if ( set.GetClass() != CBioseq_set::eClass_segset ) {

            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class = tv->FindName(set.GetClass(), true);

            PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtNotSegSet,
                     "Nuc-prot Bioseq-set contains wrong Bioseq-set, "
                     "its class is \"" + set_class + "\"", set);
            break;
        }
    }
}


void CValidError_bioseqset::ValidateSegSet(const CBioseq_set& seqset, int segcnt)
{
    if ( segcnt == 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_SegSetProblem,
            "No segmented Bioseq in segset", seqset);
    }

    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;
    
    ITERATE ( CBioseq_set::TSeq_set, se_list_it, seqset.GetSeq_set() ) {
        if ( (*se_list_it)->IsSeq() ) {
            const CSeq_inst& seq_inst = (*se_list_it)->GetSeq().GetInst();
            
            if ( mol == CSeq_inst::eMol_not_set ||
                 mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else if ( (seq_inst_mol = seq_inst.GetMol()) != CSeq_inst::eMol_other) {
                if ( seq_inst.IsNa() != CSeq_inst::IsNa(mol) ) {
                    PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetMixedBioseqs,
                        "Segmented set contains mixture of nucleotides"
                        "and proteins", seqset);
                    break;
                }
            }
        } else if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();

            if ( set.IsSetClass()  &&  
                 set.GetClass() != CBioseq_set::eClass_parts ) {
                const CEnumeratedTypeValues* tv = 
                    CBioseq_set::GetTypeInfo_enum_EClass();
                const string& set_class_str = 
                    tv->FindName(set.GetClass(), true);
                
                PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetNotParts,
                    "Segmented set contains wrong Bioseq-set, "
                    "its class is \"" + set_class_str + "\"", set);
                break;
            }
        } // else if
    } // iterate
    
    CTypeConstIterator<CMolInfo> miit(ConstBegin(seqset));
    const CMolInfo* mol_info = 0;
    
    for (; miit; ++miit) {
        if (mol_info == 0) {
            mol_info = &(*miit);
        } else if (mol_info->GetBiomol() != miit->GetBiomol() ) {
            PostErr(eDiag_Error, eErr_SEQ_PKG_InconsistentMolInfoBiomols,
                "Segmented set contains inconsistent MolInfo biomols",
                seqset);
            break;
        }
    } // for
}


void CValidError_bioseqset::ValidatePartsSet(const CBioseq_set& seqset)
{
    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;

    ITERATE ( CBioseq_set::TSeq_set, se_list_it, seqset.GetSeq_set() ) {
        if ( (*se_list_it)->IsSeq() ) {
            const CSeq_inst& seq_inst = (*se_list_it)->GetSeq().GetInst();

            if ( mol == CSeq_inst::eMol_not_set  ||
                 mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else  {
                seq_inst_mol = seq_inst.GetMol();
                if ( seq_inst_mol != CSeq_inst::eMol_other) {
                    if ( seq_inst.IsNa() != CSeq_inst::IsNa(mol) ) {
                        PostErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetMixedBioseqs,
                                 "Parts set contains mixture of nucleotides "
                                 "and proteins", seqset);
                        break;
                    }
                }
            }
        } else if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();
            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class_str = 
                tv->FindName(set.GetClass(), true);

            PostErr(eDiag_Error, eErr_SEQ_PKG_PartsSetHasSets,
                    "Parts set contains unwanted Bioseq-set, "
                    "its class is \"" + set_class_str + "\".", set);
            break;
        } // else if
    } // for
}


void CValidError_bioseqset::ValidateGenbankSet(const CBioseq_set& seqset)
{
}


void CValidError_bioseqset::ValidatePopSet(const CBioseq_set& seqset)
{
    const CBioSource*   biosrc  = 0;
    const string        *first_taxname = 0;
    static const string influenza = "Influenza virus ";
    static const string sp = " sp. ";

    ITERATE ( CBioseq_set::TSeq_set, se_list_it, seqset.GetSeq_set() ) {
        if ( (*se_list_it)->IsSet() ) {
            const CBioseq_set& set = (*se_list_it)->GetSet();
            if ( set.GetClass() == CBioseq_set::eClass_genbank ) {

                PostErr(eDiag_Error, eErr_SEQ_PKG_InternalGenBankSet,
                         "Bioseq-set contains internal GenBank Bioseq-set",
                         set);
                break;
            }
        }
    }
    
    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {
        
        biosrc = 0;
        
        // Will get the first biosource either from the descriptor
        // or feature.
        CTypeConstIterator<CBioSource> biosrc_it(ConstBegin(*seqit));
        if (biosrc_it) {
            biosrc = &(*biosrc_it);
        } 
        
        if (biosrc == 0)
            continue;
        
        const string& taxname = biosrc->GetOrg().GetTaxname();
        if (first_taxname == 0) {
            first_taxname = &taxname;
            continue;
        }
        
        // Make sure all the taxnames in the set are the same.
        if ( NStr::CompareNocase(*first_taxname, taxname) == 0 ) {
            continue;
        }
        
        // if the names differ issue an error with the exception of Influenza
        // virus, where we allow different types of it in the set.
        if ( NStr::StartsWith(taxname, influenza, NStr::eNocase)         &&
            NStr::StartsWith(*first_taxname, influenza, NStr::eNocase)  &&
            NStr::CompareNocase(*first_taxname, 0, influenza.length() + 1, taxname) == 0 ) {
            continue;
        }
        
        // drops severity if first mismatch is same up to sp.
        EDiagSev sev = eDiag_Error;
        SIZE_TYPE pos = NStr::Find(taxname, sp);
        if ( pos != NPOS ) {
            SIZE_TYPE len = pos + sp.length();
            if ( NStr::strncasecmp(first_taxname->c_str(), 
                                   taxname.c_str(),
                                   len) == 0 ) {
                sev = eDiag_Warning;
            }
        }

        PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentBioSources,
            "Population set contains inconsistent organisms.",
            *seqit);
        break;
    }
}


void CValidError_bioseqset::ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset)
{
}


void CValidError_bioseqset::ValidateGenProdSet(const CBioseq_set& seqset)
{
    bool                id_no_good = false;
    CSeq_id::E_Choice   id_type = CSeq_id::e_not_set;
    
    CBioseq_set::TSeq_set::const_iterator se_list_it =
        seqset.GetSeq_set().begin();
    
    if ( !(**se_list_it).IsSeq() ) {
        return;
    }
    
    const CBioseq& seq = (*se_list_it)->GetSeq();
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CFeat_CI fi(bsh, CSeqFeatData::e_Rna);
    for (; fi; ++fi) {
        if ( fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
            if ( fi->IsSetProduct() ) {
                CBioseq_Handle cdna = 
                    m_Scope->GetBioseqHandle(fi->GetProduct());
                if ( !cdna ) {
                    try {
                        const CSeq_id& id = GetId(fi->GetProduct(), m_Scope);
                        id_type = id.Which();
                    }
                    catch (...) {
                        id_no_good = true;
                    }
                    
                    // okay to have far RefSeq product
                    if ( id_no_good  ||  (id_type != CSeq_id::e_Other) ) {
                        string loc_label;
                        fi->GetProduct().GetLabel(&loc_label);
                        
                        if (loc_label.empty()) {
                            loc_label = "?";
                        }
                        
                        PostErr(eDiag_Error,
                            eErr_SEQ_PKG_GenomicProductPackagingProblem,
                            "Product of mRNA feature (" + loc_label +
                            ") not packaged in genomic product set", seq);
                        
                    }
                } // if (cdna == 0)
            } else {
                PostErr(eDiag_Error,
                    eErr_SEQ_PKG_GenomicProductPackagingProblem,
                    "Product of mRNA feature (?) not packaged in"
                    "genomic product set", seq);
            }
        }
    } // for 
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
