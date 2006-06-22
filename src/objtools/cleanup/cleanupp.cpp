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
* Author:  Robert Smith
*
* File Description:
*   Implementation of private parts of basic cleanup.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <serial/iterator.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "cleanupp.hpp"
#include "cleanup_utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCleanup_imp::CCleanup_imp(CRef<CCleanupChange> changes, CRef<CScope> scope, Uint4 options)
: m_Changes(changes), m_Options(options), m_Mode(eCleanup_GenBank), m_Scope (scope)
{
}


CCleanup_imp::~CCleanup_imp()
{
}


void CCleanup_imp::Setup(const CSeq_entry& se)
{
    /*** Set cleanup mode. ***/
    
    CConstRef<CBioseq> first_bioseq;
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
        {
            first_bioseq.Reset(&se.GetSeq());
            break;            
        }
        case CSeq_entry::e_Set:
        {
            CTypeConstIterator<CBioseq> seq(ConstBegin(se.GetSet()));
            if (seq) {
                first_bioseq.Reset(&*seq);
            }
            break;            
        }
    }
    
    m_Mode = eCleanup_GenBank;
    if (first_bioseq) {
        ITERATE (CBioseq::TId, it, first_bioseq->GetId()) {
            const CSeq_id& id = **it;
            if (id.IsEmbl()  ||  id.IsTpe()) {
                m_Mode = eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                m_Mode = eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                m_Mode = eCleanup_SwissProt;
            }
        }        
    }
}


void CCleanup_imp::Finish(CSeq_entry& se)
{
    // cleanup for Cleanup.
}


void CCleanup_imp::BasicCleanup(CSeq_entry& se)
{
    Setup(se);
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            BasicCleanup(se.SetSeq());
            break;
        case CSeq_entry::e_Set:
            BasicCleanup(se.SetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
    Finish(se);
}


void CCleanup_imp::BasicCleanup(CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, ss.SetData().SetEntrys()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            NON_CONST_ITERATE(CSeq_submit::TData::TAnnots, it, ss.SetData().SetAnnots()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CBioseq_set& bss)
{
    if (bss.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq_set::TAnnot, it, bss.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bss.IsSetDescr()) {
        BasicCleanup(bss.SetDescr());
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    BasicCleanup(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    BasicCleanup(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


void CCleanup_imp::BasicCleanup(CBioseq& bs)
{    
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bs.IsSetDescr()) {
        BasicCleanup(bs.SetDescr());
    }
}


void CCleanup_imp::BasicCleanup(CSeq_annot& sa)
{
    if (sa.IsSetData()  &&  sa.GetData().IsFtable()) {
        NON_CONST_ITERATE (CSeq_annot::TData::TFtable, it, sa.SetData().SetFtable()) {
            BasicCleanup(**it);
        }
    }
}


void CCleanup_imp::BasicCleanup(CSeq_descr& sdr)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        BasicCleanup(**it);
    }
}



static void s_SeqDescCommentCleanup( CSeqdesc::TComment& comment )
{
    //  Remove trailing periods:
    if ( ! comment.empty() && (comment[ comment.size() -1 ] == '.') ) {
        comment = comment.substr( 0, comment.size() -1 );
    }
};


void CCleanup_imp::BasicCleanup(CSeqdesc& sd)
{
    switch (sd.Which()) {
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            CleanString( sd.SetName() );
            break;
        case CSeqdesc::e_Title:
            CleanString( sd.SetTitle() );
            break;
        case CSeqdesc::e_Org:
            BasicCleanup(sd.SetOrg() );
            break;
        case CSeqdesc::e_Comment:
            s_SeqDescCommentCleanup( sd.SetComment() );
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            BasicCleanup(sd.SetGenbank());
            break;
        case CSeqdesc::e_Pub:
            BasicCleanup(sd.SetPub());
            break;
        case CSeqdesc::e_Region:
            CleanString( sd.SetRegion() );
            break;
        case CSeqdesc::e_User:
            BasicCleanup( sd.SetUser() );
            break;
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            // BasicCleanup( sd.SetEmbl() ); // no BasicCleanup( CEMBL_block& ) yet.
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
            BasicCleanup(sd.SetSource());
            break;
        case CSeqdesc::e_Molinfo:
            break;
            
        default:
            break;
    }
}


// CGB_block data member cleanup
void CCleanup_imp::BasicCleanup(CGB_block& gbb)   
{
    //
    //  origin:
    //  append period if there isn't one already
    //
    if ( gbb.CanGetOrigin() ) {
        const CGB_block::TOrigin& origin = gbb.GetOrigin();
        if ( ! origin.empty() && ! NStr::EndsWith(origin, ".")) {
            gbb.SetOrigin() += '.';
        }
    }
}

// Extended Cleanup Methods
void CCleanup_imp::ExtendedCleanup(CSeq_entry& se)
{
    Setup(se);
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            ExtendedCleanup(se.SetSeq());
            break;
        case CSeq_entry::e_Set:
            ExtendedCleanup(se.SetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
    Finish(se);
}


void CCleanup_imp::ExtendedCleanup(CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, ss.SetData().SetEntrys()) {
                ExtendedCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            NON_CONST_ITERATE(CSeq_submit::TData::TAnnots, it, ss.SetData().SetAnnots()) {
                ExtendedCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::ExtendedCleanup(CBioseq_set& bss)
{
    BasicCleanup(bss);
    x_RemoveEmptyGenbankDesc(bss);
    x_RemoveEmptyFeatures(bss);
    x_MolInfoUpdate(bss);
    x_RemoveEmptyGenbankDesc(bss);
    x_CleanGenbankBlockStrings(bss);
    x_RemoveEmptyFeatures(bss);
    BasicCleanup(bss);

}


void CCleanup_imp::ExtendedCleanup(CBioseq& bs)
{
    BasicCleanup (bs);
    x_RemoveEmptyGenbankDesc(bs);
    x_RemoveEmptyFeatures(bs);    
    x_MolInfoUpdate(bs);
    x_RemoveEmptyGenbankDesc(bs);
    x_CleanGenbankBlockStrings(bs);
    x_RemoveEmptyFeatures(bs);    
    BasicCleanup (bs);
}


void CCleanup_imp::ExtendedCleanup(CSeq_annot& sa)
{
    BasicCleanup (sa);
    x_RemoveEmptyFeatures(sa);
    BasicCleanup (sa);
}


//   For any Bioseq or BioseqSet that has a MolInfo descriptor,
//1) If the Bioseq or BioseqSet also has a MolType descriptor, if the MolInfo biomol value is not set 
//   and the value from the MolType descriptor is MOLECULE_TYPE_GENOMIC, MOLECULE_TYPE_PRE_MRNA, 
//   MOLECULE_TYPE_MRNA, MOLECULE_TYPE_RRNA, MOLECULE_TYPE_TRNA, MOLECULE_TYPE_SNRNA, MOLECULE_TYPE_SCRNA, 
//   MOLECULE_TYPE_PEPTIDE, MOLECULE_TYPE_OTHER_GENETIC_MATERIAL, MOLECULE_TYPE_GENOMIC_MRNA_MIX, or 255, 
//   the value from the MolType descriptor will be used to set the MolInfo biomol value.  The MolType descriptor
//   will be removed, whether its value was copied to the MolInfo descriptor or not.
//2) If the Bioseq or BioseqSet also has a Method descriptor, if the MolInfo technique value has not been set and 
//   the Method descriptor value is “concept_trans”, “seq_pept”, “both”, “seq_pept_overlap”,  “seq_pept_homol”, 
//   “concept_transl_a”, or “other”, then the Method descriptor value will be used to set the MolInfo technique value.  
//   The Method descriptor will be removed, whether its value was copied to the MolInfo descriptor or not.
void CCleanup_imp::x_MolInfoUpdate(CSeq_descr& sdr)
{
    bool has_molinfo = false;
    CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;
    CMolInfo::TTech   tech = CMolInfo::eTech_unknown;
    bool changed = false;
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            if (!has_molinfo) {
                has_molinfo = true;
                const CMolInfo& mol_info = (*it)->GetMolinfo();
                if (mol_info.CanGetBiomol()) {
                    biomol = mol_info.GetBiomol();
                }
                if (mol_info.CanGetTech()) {
                    tech = mol_info.GetTech();
                }
            }          
        } else if ((*it)->Which() == CSeqdesc::e_Mol_type) {
            if (biomol == CMolInfo::eBiomol_unknown) {
                CSeqdesc::TMol_type mol_type = (*it)->GetMol_type();
                if (mol_type == CMolInfo::eBiomol_genomic
                    || mol_type == CMolInfo::eBiomol_pre_RNA
                    || mol_type == CMolInfo::eBiomol_mRNA
                    || mol_type == CMolInfo::eBiomol_rRNA
                    || mol_type == CMolInfo::eBiomol_tRNA
                    || mol_type == CMolInfo::eBiomol_snRNA
                    || mol_type == CMolInfo::eBiomol_scRNA
                    || mol_type == CMolInfo::eBiomol_peptide
                    || mol_type == CMolInfo::eBiomol_other_genetic
                    || mol_type == CMolInfo::eBiomol_genomic_mRNA
                    || mol_type == CMolInfo::eBiomol_other) {
                    biomol = mol_type;
                }
            }
        } else if ((*it)->Which() == CSeqdesc::e_Method) {
            if (tech == CMolInfo::eTech_unknown) {
                CSeqdesc::TMethod method = (*it)->GetMethod();
                switch (method) {
                    case eGIBB_method_concept_trans:
                        tech = CMolInfo::eTech_concept_trans;
                        break;
                    case eGIBB_method_seq_pept:
                        tech = CMolInfo::eTech_seq_pept;
                        break;
                    case eGIBB_method_both:
                        tech = CMolInfo::eTech_both;
                        break;
                    case eGIBB_method_seq_pept_overlap:
                        tech = CMolInfo::eTech_seq_pept_overlap;
                        break;
                    case eGIBB_method_seq_pept_homol:
                        tech = CMolInfo::eTech_seq_pept_homol;
                        break;
                    case eGIBB_method_concept_trans_a:
                        tech = CMolInfo::eTech_concept_trans_a;
                        break;
                    case eGIBB_method_other:
                        tech = CMolInfo::eTech_other;
                        break;
                }
            }
        }
            
    }
    if (!has_molinfo) {
        return;
    }    
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            CSeqdesc::TMolinfo& mi = (*it)->SetMolinfo();
            if (biomol != CMolInfo::eBiomol_unknown) {
                if (!mi.CanGetBiomol() || mi.GetBiomol() != biomol) {
                    changed = true;
                    mi.SetBiomol (biomol);
                }
            }
            if (tech != CMolInfo::eTech_unknown) {
                if (!mi.CanGetTech() || mi.GetTech() != tech) {
                    changed = true;
                    mi.SetTech (tech);
                }
            }
            break;
        }
    }

    bool found = true;
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Method || (*it)->Which() == CSeqdesc::e_Mol_type) {
                sdr.Set().erase(it);
                found = true;
                break;
            }
        }        
    }
}


void CCleanup_imp::x_MolInfoUpdate(CBioseq& bs)
{
    if (bs.IsSetDescr()) {
        x_MolInfoUpdate(bs.SetDescr());
    }
}


void CCleanup_imp::x_MolInfoUpdate(CBioseq_set& bss)
{
    if (bss.IsSetDescr()) {
        x_MolInfoUpdate(bss.SetDescr());
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    x_MolInfoUpdate(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    x_MolInfoUpdate(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }

}


bool IsEmpty (CGB_block& block) 
{
    if ((!block.CanGetExtra_accessions() || block.GetExtra_accessions().size() == 0)
        && (!block.CanGetSource() || NStr::IsBlank(block.GetSource()))
        && (!block.CanGetKeywords() || block.GetKeywords().size() == 0)
        && (!block.CanGetOrigin() || NStr::IsBlank(block.GetOrigin()))
        && (!block.CanGetDate() || NStr::IsBlank (block.GetDate()))
        && (!block.CanGetEntry_date())
        && (!block.CanGetDiv() || NStr::IsBlank(block.GetDiv()))
        && (!block.CanGetTaxonomy() || NStr::IsBlank(block.GetTaxonomy()))) {
        return true;
    } else {
        return false;
    }
}

bool IsEmpty (CGene_ref& gene_ref)
{
    if ((!gene_ref.CanGetLocus() || NStr::IsBlank (gene_ref.GetLocus()))
        && (!gene_ref.CanGetAllele() || NStr::IsBlank (gene_ref.GetAllele()))
        && (!gene_ref.CanGetDesc() || NStr::IsBlank (gene_ref.GetDesc()))
        && (!gene_ref.CanGetMaploc() || NStr::IsBlank (gene_ref.GetMaploc()))
        && (!gene_ref.CanGetLocus_tag() || NStr::IsBlank (gene_ref.GetLocus_tag()))
        && (!gene_ref.CanGetDb() || gene_ref.GetDb().size() == 0)
        && (!gene_ref.CanGetSyn() || gene_ref.GetSyn().size() == 0)) {
        return true;
    } else {
        return false;
    }   
}


bool IsEmpty (CSeq_feat& sf)
{
    if (sf.CanGetData() && sf.SetData().IsGene()) {
        if (IsEmpty(sf.SetData().SetGene())) {
            if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
                return true;
            } else {
                // convert this feature to a misc_feat
                sf.SetData().Reset();
                sf.SetData().SetImp().SetKey("misc_feature");
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsProt()) {
        CProt_ref& prot = sf.SetData().SetProt();
        if (prot.CanGetProcessed()) {
            unsigned int processed = sf.SetData().GetProt().GetProcessed();
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() || prot.GetName().size() == 0)
                && sf.CanGetComment()
                && NStr::StartsWith (sf.GetComment(), "putative")) {
                prot.SetName ().push_back(sf.GetComment());
                sf.ResetComment();
            }

            if (processed == CProt_ref::eProcessed_mature && (!prot.CanGetName() || prot.GetName().size() == 0)) {
                prot.SetName().push_back("unnamed");
            }
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() 
                    || prot.GetName().size() == 0 
                    || NStr::IsBlank(prot.GetName().front()))
                && (!prot.CanGetDesc() || NStr::IsBlank(prot.GetDesc()))
                && (!prot.CanGetEc() || prot.GetEc().size() == 0)
                && (!prot.CanGetActivity() || prot.GetActivity().size() == 0)
                && (!prot.CanGetDb() || prot.GetDb().size() == 0)) {
                return true;
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsComment()) {
        if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
            return true;
        }
    }
    return false;
}


// Was CleanupGenbankCallback in C Toolkit
// removes taxonomy
// remove Genbank Block descriptors when
//      if (gbp->extra_accessions == NULL && gbp->source == NULL &&
//         gbp->keywords == NULL && gbp->origin == NULL &&
//         gbp->date == NULL && gbp->entry_date == NULL &&
//         gbp->div == NULL && gbp->taxonomy == NULL) {

void CCleanup_imp::x_RemoveEmptyGenbankDesc(CSeq_descr& sdr)
{
    bool found = true;
    
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Genbank) {
                CGB_block& block = (*it)->SetGenbank();
                block.ResetTaxonomy();
                (*it)->SetGenbank(block);
                if (IsEmpty(block)) {
                    sdr.Set().erase(it);
                    found = true;
                    break;
                }
            }
        }
    }
}


void CCleanup_imp::x_RemoveEmptyGenbankDesc(CBioseq& bs)
{
    if (bs.IsSetDescr()) {
        x_RemoveEmptyGenbankDesc(bs.SetDescr());
        if (bs.SetDescr().Set().empty()) {
            bs.ResetDescr();
        }
    }
}


void CCleanup_imp::x_RemoveEmptyGenbankDesc(CBioseq_set& bss)
{
    if (bss.IsSetDescr()) {
        x_RemoveEmptyGenbankDesc(bss.SetDescr());
        if (bss.SetDescr().Set().empty()) {
            bss.ResetDescr();
        }
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    x_RemoveEmptyGenbankDesc(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    x_RemoveEmptyGenbankDesc(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


// was EntryCheckGBBlock in C Toolkit
// cleans strings for the GenBankBlock, removes descriptor if block is now empty
void CCleanup_imp::x_CleanGenbankBlockStrings (CSeq_descr& sdr)
{
    bool found = true;
    
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Genbank) {
                CGB_block& block = (*it)->SetGenbank();
                
                // clean extra accessions
                if (block.CanGetExtra_accessions()) {
                    CleanVisStringList(block.SetExtra_accessions());
                    if (block.GetExtra_accessions().size() == 0) {
                        block.ResetExtra_accessions();
                    }
                }
                
                // clean keywords
                if (block.CanGetKeywords()) {
                    CleanVisStringList(block.SetKeywords());
                    if (block.GetKeywords().size() == 0) {
                        block.ResetKeywords();
                    }
                }
                
                // clean source
                if (block.CanGetSource()) {
                    string source = block.GetSource();
                    CleanVisString (source);
                    if (NStr::IsBlank (source)) {
                        block.ResetSource();
                    } else {
                        block.SetSource (source);
                    }
                }
                // clean origin
                if (block.CanGetOrigin()) {
                    string origin = block.GetOrigin();
                    CleanVisString (origin);
                    if (NStr::IsBlank (origin)) {
                        block.ResetOrigin();
                    } else {
                        block.SetOrigin(origin);
                    }
                }
                //clean date
                if (block.CanGetDate()) {
                    string date = block.GetDate();
                    CleanVisString (date);
                    if (NStr::IsBlank (date)) {
                        block.ResetDate();
                    } else {
                        block.SetDate(date);
                    }
                }
                //clean div
                if (block.CanGetDiv()) {
                    string div = block.GetDiv();
                    CleanVisString (div);
                    if (NStr::IsBlank (div)) {
                        block.ResetDiv();
                    } else {
                        block.SetDiv(div);
                    }
                }
                //clean taxonomy
                if (block.CanGetTaxonomy()) {
                    string tax = block.GetTaxonomy();
                    if (NStr::IsBlank (tax)) {
                        block.ResetTaxonomy();
                    } else {
                        block.SetTaxonomy(tax);
                    }
                }
                
                if (IsEmpty(block)) {
                    sdr.Set().erase(it);
                    found = true;
                    break;
                }
            }
        }
    }
}


void CCleanup_imp::x_CleanGenbankBlockStrings(CBioseq& bs)
{
    if (bs.IsSetDescr()) {
        x_CleanGenbankBlockStrings(bs.SetDescr());
        if (bs.SetDescr().Set().empty()) {
            bs.ResetDescr();
        }
    }
}


void CCleanup_imp::x_CleanGenbankBlockStrings(CBioseq_set& bss)
{
    if (bss.IsSetDescr()) {
        x_CleanGenbankBlockStrings(bss.SetDescr());
        if (bss.SetDescr().Set().empty()) {
            bss.ResetDescr();
        }
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    x_CleanGenbankBlockStrings(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    x_CleanGenbankBlockStrings(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


// removes or converts empty features
void CCleanup_imp::x_RemoveEmptyFeatures (CSeq_annot& sa)
{
    if (sa.IsSetData()  &&  sa.GetData().IsFtable()) {
        CSeq_annot::C_Data::TFtable& ftable = sa.SetData().SetFtable();
        CSeq_annot::C_Data::TFtable new_table;
        new_table.clear();
        
        while (ftable.size() > 0) {
            CRef< CSeq_feat > sf = ftable.front();
            ftable.pop_front();
            
            if (!IsEmpty(*sf)) {
                new_table.push_back (sf);
            }
        }
        while (new_table.size() > 0) {
            CRef< CSeq_feat > sf = new_table.front();
            new_table.pop_front();
            ftable.push_back (sf);
        }
    }
}


void CCleanup_imp::x_RemoveEmptyFeatures (CBioseq& bs)
{
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            x_RemoveEmptyFeatures(**it);
        }
    }
}


void CCleanup_imp::x_RemoveEmptyFeatures (CBioseq_set& bss)
{
    if (bss.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq_set::TAnnot, it, bss.SetAnnot()) {
            x_RemoveEmptyFeatures(**it);
        }
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    x_RemoveEmptyFeatures(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    x_RemoveEmptyFeatures(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }    
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.10  2006/06/22 13:28:29  bollin
 * added step to remove empty features to ExtendedCleanup
 *
 * Revision 1.9  2006/06/21 17:21:28  bollin
 * added cleanup of GenbankBlock descriptor strings to ExtendedCleanup
 *
 * Revision 1.8  2006/06/21 14:12:59  bollin
 * added step for removing empty GenBank block descriptors to ExtendedCleanup
 *
 * Revision 1.7  2006/06/20 19:43:39  bollin
 * added MolInfoUpdate to ExtendedCleanup
 *
 * Revision 1.6  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.5  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.4  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.3  2006/03/29 16:33:43  rsmith
 * Cleanup strings in Seqdesc. Move Pub functions to cleanup_pub.cpp
 *
 * Revision 1.2  2006/03/20 14:22:15  rsmith
 * add cleanup for CSeq_submit
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

