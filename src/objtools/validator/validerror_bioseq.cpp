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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat ......
 *
 * File Description:
 *   validation of bioseq 
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimisc.hpp>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIpaa.hpp>
#include <objects/seq/NCBIpna.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Pubdesc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

#include <objtools/error_codes.hpp>
#include <objtools/edit/struc_comm_field.hpp>

#include <algorithm>

#include <objmgr/seq_loc_mapper.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);
USING_SCOPE(feature);

// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseq::CValidError_bioseq(CValidError_imp& imp) :
    CValidError_base(imp), m_AnnotValidator(imp), m_DescrValidator(imp), m_FeatValidator(imp), m_GeneIt(NULL), m_AllFeatIt(NULL)
{
}


CValidError_bioseq::~CValidError_bioseq()
{
}


void CValidError_bioseq::ValidateBioseq (const CBioseq& seq)
{
    try {
        m_CurrentHandle = m_Scope->GetBioseqHandle(seq);

        try {
            m_GeneIt = new CFeat_CI(m_CurrentHandle, CSeqFeatData::e_Gene);
            m_AllFeatIt = new CFeat_CI(m_CurrentHandle);
        } catch ( const exception& ) {
            // sequence might be too broken to validate features
            m_GeneIt = NULL;
            m_AllFeatIt = NULL;
        }
        m_mRNACDSIndex.SetBioseq(m_AllFeatIt, m_CurrentHandle, m_Scope);
        ValidateSeqIds(seq);
        ValidateInst(seq);
        ValidateBioseqContext(seq);
        ValidatemRNAGene(seq);
        ValidateHistory(seq);
        FOR_EACH_ANNOT_ON_BIOSEQ (annot, seq) {
            m_AnnotValidator.ValidateSeqAnnot(**annot);
            m_AnnotValidator.ValidateSeqAnnotContext(**annot, seq);
        }
        if (seq.IsSetDescr()) {
            if (m_CurrentHandle) {
                CSeq_entry_Handle ctx = m_CurrentHandle.GetSeq_entry_Handle();
                if (ctx) {
                    m_DescrValidator.ValidateSeqDescr (seq.GetDescr(), *(ctx.GetCompleteSeq_entry()));
                }
            }
        }
    } catch ( const exception& e ) {
        m_Imp.PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
            string("Exception while validating bioseq. EXCEPTION: ") +
            e.what(), seq);
    }
    m_CurrentHandle.Reset();
    if (m_GeneIt) {
        delete m_GeneIt;
        m_GeneIt = NULL;
    }
    if (m_AllFeatIt) {
        delete m_AllFeatIt;
        m_AllFeatIt = NULL;
    }
}


static bool s_IsSkippableDbtag (const CDbtag& dbt) 
{
    if (!dbt.IsSetDb()) {
        return false;
    }
    const string& db = dbt.GetDb();
    if (NStr::EqualNocase(db, "TMSMART")
        || NStr::EqualNocase(db, "BankIt")
        || NStr::EqualNocase (db, "NCBIFILE")) {
        return true;
    } else {
        return false;
    }
}


// validation for individual Seq-id
void CValidError_bioseq::ValidateSeqId(const CSeq_id& id, const CBioseq& ctx)
{
    // see if ID can be used to find ctx
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(id);
    if (bsh) {
        CConstRef<CBioseq> core = m_Scope->GetBioseqHandle(id).GetBioseqCore();
        if ( !core ) {
            if ( !m_Imp.IsPatent() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                    "BioseqFind (" + id.AsFastaString() + 
                    ") unable to find itself - possible internal error", ctx);
            }
        } else if ( core.GetPointer() != &ctx ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                "SeqID " + id.AsFastaString() + 
                " is present on multiple Bioseqs in record", ctx);
        }
    } else {
        PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
            "BioseqFind (" + id.AsFastaString() + 
            ") unable to find itself - possible internal error", ctx);
    }

    //check formatting
    const CTextseq_id* tsid = id.GetTextseq_Id();

    switch (id.Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            if ( IsHistAssemblyMissing(ctx)  &&  ctx.IsNa() ) {
                PostErr(eDiag_Info, eErr_SEQ_INST_HistAssemblyMissing,
                    "TPA record " + id.AsFastaString() +
                    " should have Seq-hist.assembly for PRIMARY block", 
                    ctx);
            }
        // Fall thru 
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            if ( tsid  &&  tsid->IsSetAccession() ) {
                const string& acc = tsid->GetAccession();
                unsigned int num_digits = 0;
                unsigned int num_letters = 0;
                size_t num_underscores = 0;
                bool letter_after_digit = false;
                bool bad_id_chars = false;
                size_t i = 0;
                    
                for ( ; i < acc.length(); ++i ) {
                    if ( isupper((unsigned char) acc[i]) ) {
                        num_letters++;
                    } else if ( isdigit((unsigned char) acc[i]) ) {
                        num_digits++;
                    } else if ( acc[i] == '_' ) {
                        num_underscores++;
                        if ( num_digits > 0  ||  num_underscores > 1 ) {
                            letter_after_digit = true;
                        }
                    } else {
                        bad_id_chars = true;
                    }
                }


                if ( letter_after_digit || bad_id_chars ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession " + acc, ctx);
                } else if (num_underscores == 1) {
                    if (NStr::CompareNocase(acc, 0, 4, "MAP_") != 0 || num_digits != 6) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, ctx);
                    }
                } else if (num_letters == 1 && num_digits == 5 && ctx.IsNa()) {
                } else if (num_letters == 2 && num_digits == 6 && ctx.IsNa()) {
                } else if (num_letters == 3 && num_digits == 5 && ctx.IsAa()) {
                } else if (num_letters == 2 && num_digits == 6 && ctx.IsAa() &&
                    ctx.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
                } else if ( num_letters == 4  && 
                            (num_digits == 8  ||  num_digits == 9)  && 
                            ctx.IsNa() ) {
                } else {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession " + acc, ctx);
                }
                // Check for secondary conflicts
                ValidateSecondaryAccConflict(acc, ctx, CSeqdesc::e_Genbank);
                ValidateSecondaryAccConflict(acc, ctx, CSeqdesc::e_Embl);
            }
        // Fall thru
        case CSeq_id::e_Other:
            if ( tsid ) {
                if ( tsid->IsSetName() ) {
                    const string& name = tsid->GetName();
                    ITERATE (string, s, name) {
                        if (isspace((unsigned char)(*s))) {
                            PostErr(eDiag_Critical,
                                eErr_SEQ_INST_SeqIdNameHasSpace,
                                "Seq-id.name '" + name + "' should be a single "
                                "word without any spaces", ctx);
                            break;
                        }
                    }
                }

                if ( tsid->IsSetAccession()  &&  id.IsOther() ) {                    
                    const string& acc = tsid->GetAccession();
                    size_t num_letters = 0;
                    size_t num_digits = 0;
                    size_t num_underscores = 0;
                    bool bad_id_chars = false;
                    bool is_NZ = (NStr::CompareNocase(acc, 0, 3, "NZ_") == 0);
                    size_t i = is_NZ ? 3 : 0;
                    bool letter_after_digit = false;

                    for ( ; i < acc.length(); ++i ) {
                        if ( isupper((unsigned char) acc[i]) ) {
                            num_letters++;
                        } else if ( isdigit((unsigned char) acc[i]) ) {
                            num_digits++;
                        } else if ( acc[i] == '_' ) {
                            num_underscores++;
                            if ( num_digits > 0  ||  num_underscores > 1 ) {
                                letter_after_digit = true;
                            }
                        } else {
                            bad_id_chars = true;
                        }
                    }

                    if ( letter_after_digit  ||  bad_id_chars ) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, ctx);
                    } else if ( is_NZ  &&  num_letters == 4  && 
                        num_digits == 8  &&  num_underscores == 0 ) {
                        // valid accession - do nothing!
                    } else if ( num_letters == 2  &&
                        (num_digits == 6  ||  num_digits == 8  || num_digits == 9)  &&
                        num_underscores == 1 ) {
                        // valid accession - do nothing!
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, ctx);
                    }
                }
            }
        // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
            if ( tsid ) {
                if ( ctx.IsNa()  &&  
                     (!tsid->IsSetAccession() || tsid->GetAccession().empty())) {
                    if ( ctx.GetInst().GetRepr() != CSeq_inst::eRepr_seg  ||
                        m_Imp.IsGI()) {
                        if (!id.IsDdbj()  ||
                            ctx.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                            string msg = "Missing accession for " + id.AsFastaString();
                            PostErr(eDiag_Error,
                                eErr_SEQ_INST_BadSeqIdFormat,
                                msg, ctx);
                        }
                    }
                }
            } else {
                PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                    "Seq-id type not handled", ctx);
            }
            break;
        case CSeq_id::e_Gi:
            if (id.GetGi() <= ZERO_GI) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_ZeroGiNumber,
                         "Invalid GI number", ctx);
            }
            break;
        case CSeq_id::e_General:
            if (id.GetGeneral().IsSetDb()) {
                const CDbtag& dbt = id.GetGeneral();
                int dblen = dbt.GetDb().length();
                if (dblen < 1) {
                   PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat, "General identifier missing database field", ctx);
                }
                EDiagSev sev = eDiag_Error;
                if (! m_Imp.IsINSDInSep()) {
                    sev = eDiag_Critical;
                } else if (! m_Imp.IsIndexerVersion()) {
                    sev = eDiag_Error;
                }
                if (dblen > 20) {
                    PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "General database longer than 20 characters", ctx);
                }
                if (! s_IsSkippableDbtag(dbt)) {
                    if (dbt.IsSetTag() && dbt.GetTag().IsStr()) {
                        int idlen = dbt.GetTag().GetStr().length();
                        if (idlen > 64) {
                            PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "General identifier longer than 64 characters", ctx);
                        }
                    }
                }
             }
           break;
        case CSeq_id::e_Local:
            if (id.IsLocal() && id.GetLocal().IsStr() && id.GetLocal().GetStr().length() > 64) {
                EDiagSev sev = eDiag_Error;
                if (! m_Imp.IsINSDInSep()) {
                    sev = eDiag_Critical;
                } else if (! m_Imp.IsIndexerVersion()) {
                    sev = eDiag_Error;
                }
                PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "Local identifier longer than 64 characters", ctx);
            }
            break;
        default:
            break;
    }

#if 0 
        // disabled for now
        if (!IsNCBIFILESeqId(**i)) {
            string label;
            (*i)->GetLabel(&label);
            if (label.length() > 40) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat, 
                         "Sequence ID is unusually long (" + 
                         NStr::IntToString(label.length()) + "): " + label,
                         seq);
            }
        }
#endif

}

static bool x_IsWgsSecondary (const CBioseq& seq)

{
    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (sd, seq) {
        const list< string > *extra_acc = 0;
        const CSeqdesc& desc = **sd;
        switch (desc.Which()) {
            case CSeqdesc::e_Genbank:
                if (desc.GetGenbank().IsSetExtra_accessions()) {
                    extra_acc = &(desc.GetGenbank().GetExtra_accessions());
                }
                break;
            case CSeqdesc::e_Embl:
                if (desc.GetEmbl().IsSetExtra_acc()) {
                    extra_acc = &(desc.GetEmbl().GetExtra_acc());
                }
                break;
            default:
                break;
        }
        if ( extra_acc ) {
            FOR_EACH_STRING_IN_LIST (acc, *extra_acc) {
                CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession (*acc);
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
                     && (info & CSeq_id::fAcc_master) != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}


void CValidError_bioseq::ValidateSeqIds
(const CBioseq& seq)
{
    // Ensure that CBioseq has at least one CSeq_id
    if ( !seq.IsSetId() || seq.GetId().empty() ) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
                 "No ids on a Bioseq", seq);
        return;
    }

    CSeq_inst::ERepr repr = seq.GetInst().GetRepr();

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NG, or NC. Check that the same CSeq_id not included more
    // than once.
    bool has_gi = false;
    bool is_lrg = false;
    bool has_ng = false;
    bool is_nc = false;
    bool wgs_tech_needs_wgs_accession = false;
    bool is_segset_accession = false;
    bool has_wgs_general = false;

    FOR_EACH_SEQID_ON_BIOSEQ (i, seq) {
        // first, do standalone validation
        ValidateSeqId (**i, seq);

        if ((*i)->IsGeneral() && (*i)->GetGeneral().IsSetDb()) {
            if (NStr::EqualNocase((*i)->GetGeneral().GetDb(), "LRG")) {
                is_lrg = true;
            }
            if (NStr::StartsWith((*i)->GetGeneral().GetDb(), "WGS:")) {
                has_wgs_general = true;
            }
        } else if ((*i)->IsOther() && (*i)->GetOther().IsSetAccession()) {
            const string& acc = (*i)->GetOther().GetAccession();
            if (NStr::StartsWith(acc, "NG_")) {
                has_ng = true;
                wgs_tech_needs_wgs_accession = true;
            } else if (NStr::StartsWith(acc, "NC_")) {
                is_nc = true;
            } else if (NStr::StartsWith(acc, "NM_")
                        || NStr::StartsWith(acc, "NP_")
                        || NStr::StartsWith(acc, "NR_")) {
                wgs_tech_needs_wgs_accession = true;
            }
        }

        // Check that no two CSeq_ids for same CBioseq are same type
        CBioseq::TId::const_iterator j;
        for (j = i, ++j; j != seq.GetId().end(); ++j) {
            if ((**i).Compare(**j) != CSeq_id::e_DIFF) {
                CNcbiOstrstream os;
                os << "Conflicting ids on a Bioseq: (";
                (**i).WriteAsFasta(os);
                os << " - ";
                (**j).WriteAsFasta(os);
                os << ")";
                PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingIdsOnBioseq,
                    CNcbiOstrstreamToString (os) /* os.str() */, seq);
            }
        }

        if ( (*i)->IsGenbank()  ||  (*i)->IsEmbl()  ||  (*i)->IsDdbj() ) {
            wgs_tech_needs_wgs_accession = true;
        } 

        if ( (*i)->IsGi() ) {
            has_gi = true;
        }

        if ( (*i)->IdentifyAccession() == CSeq_id::eAcc_segset) {
            is_segset_accession = true;
        }

    }
    if (is_lrg && !has_ng) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_ConflictingIdsOnBioseq,
            "LRG sequence needs NG_ accession", seq);
    }


    // Loop thru CSeq_ids to check formatting
    bool is_wgs = false;
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    FOR_EACH_SEQID_ON_BIOSEQ (k, seq) {
        const CTextseq_id* tsid = (*k)->GetTextseq_Id();
        switch ((**k).Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            if ( tsid  &&  tsid->IsSetAccession() ) {
                const string& acc = tsid->GetAccession();
                    
                is_wgs |= acc.length() == 12  ||  acc.length() == 13;


                if ( has_gi ) {
                    if ( !tsid->IsSetVersion()  ||  tsid->GetVersion() == 0 ) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Accession " + acc + " has 0 version", seq);
                    }
                }
            }
        // Fall thru
        case CSeq_id::e_Other:
            if ( tsid ) {

                if ( has_gi && !tsid->IsSetAccession() && tsid->IsSetName() ) {
                    if ( (*k)->IsDdbj()  &&  repr == CSeq_inst::eRepr_seg ) {
                        // Don't report ddbj segmented sequence missing accessions 
                    } else {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Missing accession for " + tsid->GetName(), seq);
                    }
                }
            }
            // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
            if ( tsid ) {
                accn_count++;
            }
            break;
        
        case CSeq_id::e_Gi:
            gi_count++;
            break;

        default:
            break;
        }
    }

    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));
    if (!SeqIsPatent(seq)) {
        if ( is_wgs ) {
            if ( !mi  ||  !mi->IsSetTech()  ||  
                ( mi->GetTech() != CMolInfo::eTech_wgs  &&  mi->GetTech() != CMolInfo::eTech_tsa ) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                    "WGS accession should have Mol-info.tech of wgs", seq);
            }
        } else if ( mi  &&  mi->IsSetTech()  &&  
                    mi->GetTech() == CMolInfo::eTech_wgs  &&
                    wgs_tech_needs_wgs_accession &&
                    !is_segset_accession &&
                    !has_wgs_general &&
                    !x_IsWgsSecondary(seq)) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                "Mol-info.tech of wgs should have WGS accession", seq);
        }

          if (is_nc && mi && seq.IsNa()
                && (!mi->IsSetBiomol() 
                || (mi->GetBiomol() != CMolInfo::eBiomol_genomic 
                    && mi->GetBiomol() != CMolInfo::eBiomol_cRNA))) {
            PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                     "NC nucleotide should be genomic or cRNA",
                     seq);
        }
    }
    if (seq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        if (mi && mi->IsSetBiomol()) {
            switch (mi->GetBiomol()) {
                case CMolInfo::eBiomol_pre_RNA:
                case CMolInfo::eBiomol_mRNA:
                case CMolInfo::eBiomol_rRNA:
                case CMolInfo::eBiomol_tRNA:
                case CMolInfo::eBiomol_snRNA:
                case CMolInfo::eBiomol_scRNA:
                case CMolInfo::eBiomol_cRNA:
                case CMolInfo::eBiomol_snoRNA:
                case CMolInfo::eBiomol_transcribed_RNA:
                case CMolInfo::eBiomol_ncRNA:
                case CMolInfo::eBiomol_tmRNA:
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolTypeBiomol,
                            "Molecule type (DNA) does not match biomol (RNA)", seq);
                    break;
                default:
                    break;
            }
        }
    }

    // Check that a sequence with a gi number has exactly one accession
    if ( gi_count > 0  &&  accn_count == 0  &&  !m_Imp.IsPDB()  &&  
         repr != CSeq_inst::eRepr_virtual ) {
        PostErr(eDiag_Error, eErr_SEQ_INST_GiWithoutAccession,
            "No accession on sequence with gi number", seq);
    }
    if (gi_count > 0  &&  accn_count > 1) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MultipleAccessions,
            "Multiple accessions on sequence with gi number", seq);
    }

    if ( m_Imp.IsValidateIdSet() ) {
        ValidateIDSetAgainstDb(seq);
    }

    // C toolkit ensures that there is exactly one CBioseq for a CSeq_id
    // Not done here because object manager will not allow
    // the same Seq-id on multiple Bioseqs

}


bool CValidError_bioseq::IsHistAssemblyMissing(const CBioseq& seq)
{
    bool rval = false;
    const CSeq_inst& inst = seq.GetInst();
    CSeq_inst::TRepr repr = inst.CanGetRepr() ?
        inst.GetRepr() : CSeq_inst::eRepr_not_set;

    if ( !inst.CanGetHist()  ||  !inst.GetHist().CanGetAssembly() ) {
        if ( seq.IsNa()  &&  repr != CSeq_inst::eRepr_seg ) {
            rval = true;
            // look for keyword
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
            CSeqdesc_CI genbank_i(bsh, CSeqdesc::e_Genbank);
            if (genbank_i && genbank_i->GetGenbank().IsSetKeywords()) {
                CGB_block::TKeywords::const_iterator keyword = genbank_i->GetGenbank().GetKeywords().begin();
                while (keyword != genbank_i->GetGenbank().GetKeywords().end() && rval) {
                    if (NStr::EqualNocase(*keyword, "TPA:reassembly")) {
                        rval = false;
                    }
                    ++keyword;
                }
            }
            if (rval) {
                CSeqdesc_CI embl_i(bsh, CSeqdesc::e_Embl);
                if (embl_i && embl_i->GetEmbl().IsSetKeywords()) {
                    CEMBL_block::TKeywords::const_iterator keyword = embl_i->GetEmbl().GetKeywords().begin();
                    while (keyword != embl_i->GetEmbl().GetKeywords().end() && rval) {
                        if (NStr::EqualNocase(*keyword, "TPA:reassembly")) {
                            rval = false;
                        }
                        ++keyword;
                    }
                }
            }
        }
    }
    return rval;
}


void CValidError_bioseq::ValidateSecondaryAccConflict
(const string &primary_acc,
 const CBioseq &seq,
 int choice)
{
    CSeqdesc_CI sd(m_Scope->GetBioseqHandle(seq), static_cast<CSeqdesc::E_Choice>(choice));
    for (; sd; ++sd) {
        const list< string > *extra_acc = 0;
        if ( choice == CSeqdesc::e_Genbank  &&
            sd->GetGenbank().IsSetExtra_accessions() ) {
            extra_acc = &(sd->GetGenbank().GetExtra_accessions());
        } else if ( choice == CSeqdesc::e_Embl  &&
            sd->GetEmbl().IsSetExtra_acc() ) {
            extra_acc = &(sd->GetEmbl().GetExtra_acc());
        }

        if ( extra_acc ) {
            FOR_EACH_STRING_IN_LIST (acc, *extra_acc) {
                if ( NStr::CompareNocase(primary_acc, *acc) == 0 ) {
                    // If the same post error
                    PostErr(eDiag_Error,
                        eErr_SEQ_INST_BadSecondaryAccn,
                        primary_acc + " used for both primary and"
                        " secondary accession", seq);
                }
            }
        }
    }
}


void CValidError_bioseq::x_ValidateBarcode(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
    CSeq_entry_Handle seh = bsh.GetSeq_entry_Handle();
    CConstRef<CSeq_entry> ctx = seh.GetCompleteSeq_entry();

    bool has_barcode_tech = false;

    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di && di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetTech() == CMolInfo::eTech_barcode) {
        has_barcode_tech = true;
    }

    bool has_barcode_keyword = false;
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_Genbank); it; ++it) {
        FOR_EACH_KEYWORD_ON_GENBANKBLOCK (k, it->GetGenbank()) {
            if (NStr::EqualNocase (*k, "BARCODE")) {
                has_barcode_keyword = true;
                break;
            }
        }
        if (has_barcode_keyword && !has_barcode_tech) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadKeyword,
                     "BARCODE keyword without Molinfo.tech barcode",
                     *ctx, *it);
        }
    }
    if (has_barcode_tech && !has_barcode_keyword && di) {
        PostErr (eDiag_Info, eErr_SEQ_DESCR_BadKeyword,
                 "Molinfo.tech barcode without BARCODE keyword",
                 *ctx, *di);
    }
}


void CValidError_bioseq::ValidateInst(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();


    // Check representation
    if ( !ValidateRepr(inst, seq) ) {
        return;
    }

    // Check molecule, topology, and strand
    if (!inst.IsSetMol()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol is 0",
            seq);
    } else {
        const CSeq_inst::EMol& mol = inst.GetMol();
        switch (mol) {

            case CSeq_inst::eMol_na:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolNuclAcid,
                         "Bioseq.mol is type na", seq);
                break;

            case CSeq_inst::eMol_aa:
                if ( inst.IsSetTopology()  &&
                     inst.GetTopology() != CSeq_inst::eTopology_not_set  &&
                     inst.GetTopology() != CSeq_inst::eTopology_linear ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_CircularProtein,
                             "Non-linear topology set on protein", seq);
                }
                if ( inst.IsSetStrand()  &&
                     inst.GetStrand() != CSeq_inst::eStrand_ss &&
                     inst.GetStrand() != CSeq_inst::eStrand_not_set) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_DSProtein,
                             "Protein not single stranded", seq);
                }
                break;

            case CSeq_inst::eMol_not_set:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol is 0",
                    seq);
                break;

            case CSeq_inst::eMol_other:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolOther,
                         "Bioseq.mol is type other", seq);
                break;

            default:
                break;
        }
    }

    CSeq_inst::ERepr rp = seq.GetInst().GetRepr();

    if (rp == CSeq_inst::eRepr_raw  ||  rp == CSeq_inst::eRepr_const) {    
        // Validate raw and constructed sequences
        ValidateRawConst(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  ||  rp == CSeq_inst::eRepr_ref) {
        // Validate segmented and reference sequences
        ValidateSegRef(seq);
    }

    if (rp == CSeq_inst::eRepr_delta) {
        // Validate delta sequences
        ValidateDelta(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  &&  seq.GetInst().IsSetExt()  &&
        seq.GetInst().GetExt().IsSeg()) {
        // Validate part of segmented sequence
        ValidateSeqParts(seq);
    }

    if (rp == CSeq_inst::eRepr_raw || rp == CSeq_inst::eRepr_delta) {
        x_ValidateBarcode (seq);
    }
    
    x_ValidateTitle(seq);
    /*if ( seq.IsAa() ) {
         Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }*/
    
    if ( seq.IsNa() ) {
        // check for N bases at start or stop of sequence
        ValidateNsAndGaps(seq);

    }

    // Validate sequence length
    ValidateSeqLen(seq);
}


bool CValidError_bioseq::x_ShowBioProjectWarning(const CBioseq& seq)

{
    bool is_wgs = false;
    bool is_grc = false;

    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
        const CSeqdesc& desc = **it;
        switch ( desc.Which() ) {
            case CSeqdesc::e_User:
                if (desc.GetUser().IsSetType()) {
                    const CUser_object& usr = desc.GetUser();
                    const CObject_id& oi = usr.GetType();
                    if (oi.IsStr() && NStr::CompareNocase(oi.GetStr(), "DBLink") == 0) {
                        FOR_EACH_USERFIELD_ON_USEROBJECT (ufd_it, usr) {
                            const CUser_field& fld = **ufd_it;
                            if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                                const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                                if (NStr::EqualNocase(label_str, "BioProject")) {
                                    return false;
                                }
                            }
                        }
                    }
                }
                break;
            case CSeqdesc::e_Molinfo:
                if (desc.GetMolinfo().IsSetTech()) {
                    if (desc.GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
                        is_wgs = true;
                    }
                }
                break;
            case CSeqdesc::e_Title:
                {
                    string title = desc.GetTitle();
                    if (NStr::StartsWith(title, "GRC")) {
                        is_grc = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    bool is_gb = false, is_refseq = false, is_ng = false;

    FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, seq) {
        const CSeq_id& sid = **sid_itr;
        switch (sid.Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
                is_gb = true;
                break;
            case CSeq_id::e_Other:
                {
                    is_refseq = true;
                    if (sid.GetOther().IsSetAccession()) {
                        string acc = sid.GetOther().GetAccession().substr(0, 3);
                        if (acc == "NG_") {
                            is_ng = true;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    if (is_refseq) {
        if (is_ng) return false;
    } else if (is_gb) {
        if (! is_wgs && ! is_grc) return false;
    } else {
        return false;
    }

    const CSeq_inst & inst = seq.GetInst();
    CSeq_inst::TRepr repr = inst.GetRepr();

    if ( repr == CSeq_inst::eRepr_delta ) {
        if (x_IsDeltaLitOnly(inst)) return false;
    } else {
        return false;
    }

    return true;
}

void CValidError_bioseq::ValidateBioseqContext(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    // Check that proteins in nuc_prot set have a CdRegion
    if ( CdError(bsh) ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", 
            seq);
    }

    bool is_patent = SeqIsPatent (seq);

    try {
        // if there are no Seq-ids, the following tests can't be run
        if (seq.IsSetId()) {

            // Check that gene on non-segmented sequence does not have
            // multiple intervals
            ValidateMultiIntervalGene(seq);

            ValidateSeqFeatContext(seq);

            // Check for duplicate features and overlapping peptide features.
            ValidateDupOrOverlapFeats(seq);

            // check for equivalent source features
            x_ValidateSourceFeatures (bsh);

            // check for equivalen pub features
            x_ValidatePubFeatures (bsh);

            // Check for colliding genes
            ValidateCollidingGenes(seq);
        }

        m_dblink_count = 0;
        m_taa_count = 0;
        m_bs_count = 0;
        m_as_count = 0;
        m_pdb_count = 0;
        m_sra_count = 0;
        m_bp_count = 0;
        m_unknown_count = 0;

        // Validate descriptors that affect this bioseq
        ValidateSeqDescContext(seq);

        if (m_dblink_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                NStr::IntToString(m_dblink_count) + " DBLink user objects apply to a Bioseq", seq);
        }

        if (m_taa_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Trace Assembly Archive entries appear in " + NStr::IntToString(m_taa_count) + " DBLink user objects", seq);
        }

        if (m_bs_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "BioSample entries appear in " + NStr::IntToString(m_bs_count) + " DBLink user objects", seq);
        }

        if (m_as_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Assembly entries appear in " + NStr::IntToString(m_as_count) + " DBLink user objects", seq);
        }

        if (m_pdb_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "ProbeDB entries appear in " + NStr::IntToString(m_pdb_count) + " DBLink user objects", seq);
        }

        if (m_sra_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Sequence Read Archive entries appear in " + NStr::IntToString(m_sra_count) + " DBLink user objects", seq);
        }

        if (m_bp_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "BioProject entries appear in " + NStr::IntToString(m_bp_count) + " DBLink user objects", seq);
        }

        if (m_unknown_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Unrecognized entries appear in " + NStr::IntToString(m_unknown_count) + " DBLink user objects", seq);
        } else if (m_unknown_count > 0) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Unrecognized entries appear in " + NStr::IntToString(m_unknown_count) + " DBLink user object", seq);
        }

        // make sure that there is a pub on this bioseq
        if ( !m_Imp.IsNoPubs() ) {  
            CheckForPubOnBioseq(seq);
        }
        // make sure that there is a source on this bioseq
        if ( !m_Imp.IsNoBioSource() ) {
            CheckSoureDescriptor(bsh);
            //CheckForBiosourceOnBioseq(seq);
        }

        if (x_ShowBioProjectWarning (seq)) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_ScaffoldLacksBioProject,
                "BioProject entries not present on CON record", seq);
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating BioseqContext. EXCEPTION: ") +
                e.what(), seq);
        }
    }

    if (!is_patent) {
        // flag missing molinfo even if not in Sequin
        CheckForMolinfoOnBioseq(seq);
    }

    ValidateGraphsOnBioseq(seq);

    CheckTpaHistory(seq);
    
    // check for multiple publications with identical identifiers
    x_ValidateMultiplePubs(bsh);

    // check feature packaging
    // a feature packaged on a bioseq should have at least one location on the bioseq
    FOR_EACH_SEQANNOT_ON_BIOSEQ (annot_it, seq) {
        FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, **annot_it) {
            if ((*feat_it)->IsSetLocation()) {
                bool found = false;
                for ( CSeq_loc_CI loc_it((*feat_it)->GetLocation()); loc_it; ++loc_it ) {
                    const CSeq_id& id = loc_it.GetSeq_id();
                    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
                        if (id.Compare(**id_it) == CSeq_id::e_YES) {
                            found = true;
                            break;
                        } 
                    }
                    if (!found && seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
                        CBioseq_Handle part = BioseqHandleFromLocation(m_Scope, loc_it.GetEmbeddingSeq_loc());
                        if (part) {
                            CSeq_entry_Handle parent = part.GetParentEntry();
                            if (parent && parent.IsSeq()) {
                                parent = parent.GetParentEntry();
                                if (parent && parent.IsSet()
                                    && parent.GetSet().GetClass() == CBioseq_set::eClass_parts) {
                                    parent = parent.GetParentEntry();
                                    if (parent && parent.IsSet()
                                        && parent.GetSet().GetClass() == CBioseq_set::eClass_segset) {
                                        CBioseq_CI bi(parent);
                                        if (bi && bi->GetCompleteBioseq()->Equals(seq)) {
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (! found && seq.GetInst().GetRepr() == CSeq_inst::eRepr_raw) {
                        CBioseq_Handle part = m_Scope->GetBioseqHandle(seq);
                        if (part) {
                            CSeq_entry_Handle parent = part.GetParentEntry();
                            if (parent && parent.IsSeq()) {
                                parent = parent.GetParentEntry();
                                if (parent && parent.IsSet()
                                    && parent.GetSet().GetClass() == CBioseq_set::eClass_parts) {
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (! found) {
                        if (m_Imp.IsSmallGenomeSet()) {
                            m_Imp.IncrementSmallGenomeSetMisplacedCount();
                            break;
                        }
                    }
                }
                if (!found) {
                    m_Imp.IncrementMisplacedFeatureCount();
                    break;
                }
            }
        }
    }

    // look for orphaned proteins
    if (seq.IsAa() && !GetNucProtSetParent(bsh)) {
        bool is_genbank = false;
        bool is_embl = false;
        bool is_ddbj = false;
        bool is_refseq = false;
        bool is_wp = false;
        bool is_yp = false;
        bool is_gibbmt = false;
        bool is_gibbsq = false;
        bool is_patent = false;
        FOR_EACH_SEQID_ON_BIOSEQ(id_it, seq) {
            const CSeq_id& sid = **id_it;
            switch (sid.Which()) {
                case CSeq_id::e_Genbank:
                    is_genbank = true;
                    break;
                case CSeq_id::e_Embl:
                    is_embl = true;
                    break;
                case CSeq_id::e_Ddbj:
                    is_ddbj = true;
                    break;
                case CSeq_id::e_Other:
                    {
                        is_refseq = true;
                        const CTextseq_id* tsid = sid.GetTextseq_Id ();
                        if (tsid != NULL && tsid->IsSetAccession()) {
                            const string& acc = tsid->GetAccession ();
                            if (NStr::StartsWith (acc, "WP_")) {
                                is_wp = true;
                            } else if (NStr::StartsWith (acc, "YP_")) {
                                is_yp = true;
                            }
                        }
                    }
                    break;
                case CSeq_id::e_Gibbmt:
                    is_gibbmt = true;
                    break;
                case CSeq_id::e_Gibbsq:
                    is_gibbsq = true;
                    break;
                case CSeq_id::e_Patent:
                    is_patent = true;
                    break;
                default:
                    break;
            }
        }
        if ((is_genbank || is_embl || is_ddbj || is_refseq)
            && !is_gibbmt && !is_gibbsq && !is_patent && !is_wp && !is_yp) {
            PostErr(eDiag_Error, eErr_SEQ_PKG_OrphanedProtein,
                    "Orphaned stand-alone protein", seq);
        }
    }
    
    // look for extra protein features
    if (seq.IsAa()) {
        CFeat_CI feat (bsh, SAnnotSelector (CSeqFeatData::eSubtype_prot));
        int num_feat = 0;
        while (feat) {
            num_feat++;
            ++feat;
        }
        if (num_feat > 1) {
            feat.Rewind();
            while (feat) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_ExtraProteinFeature,
                        "Protein sequence has multiple unprocessed protein features", 
                        feat->GetOriginalFeature());
                ++feat;
            }
        }
    }
}


template <class Iterator, class Predicate>
bool lists_match(Iterator iter1, Iterator iter1_stop, Iterator iter2, Iterator iter2_stop, Predicate pred)
{
    while (iter1 != iter1_stop && iter2 != iter2_stop) {
        if (!pred(*iter1, *iter2)) {
            return false;
        }
        ++iter1;
        ++iter2;
    }
    if (iter1 != iter1_stop || iter2 != iter2_stop) {
        return false;
    } else {
        return true;
    }
}


static bool s_OrgModEqual (
    const CRef<COrgMod>& om1,
    const CRef<COrgMod>& om2
)

{
    const COrgMod& omd1 = *(om1);
    const COrgMod& omd2 = *(om2);

    const string& str1 = omd1.GetSubname();
    const string& str2 = omd2.GetSubname();

    if (NStr::CompareNocase (str1, str2) != 0) return false;

    TORGMOD_SUBTYPE chs1 = omd1.GetSubtype();
    TORGMOD_SUBTYPE chs2 = omd2.GetSubtype();

    if (chs1 == chs2) return true;
    if (chs2 == NCBI_ORGMOD(other)) return true;

    return false;
}


bool s_DbtagEqual (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2)
{
    // is dbt1 == dbt2
    return dbt1->Compare(*dbt2) == 0;
}


// Two OrgRefs are identical if the taxnames are identical, the dbxrefs are identical,
// and the orgname orgmod lists are identical
static bool s_OrgrefEquivalent (const COrg_ref& org1, const COrg_ref& org2)
{
    if ((org1.IsSetTaxname() && !org2.IsSetTaxname())
        || (!org1.IsSetTaxname() && org2.IsSetTaxname())
        || (org1.IsSetTaxname() && org2.IsSetTaxname() 
            && !NStr::EqualNocase (org1.GetTaxname(), org2.GetTaxname()))) {
        return false;
    }

    if ((org1.IsSetDb() && !org2.IsSetDb())
        || (!org1.IsSetDb() && org2.IsSetDb())
        || (org1.IsSetDb() && org2.IsSetDb() 
            && !lists_match (org1.GetDb().begin(), org1.GetDb().end(),
                             org2.GetDb().begin(), org2.GetDb().end(),
                             s_DbtagEqual))) {
        return false;
    }

    if ((org1.IsSetOrgname() && !org2.IsSetOrgname())
        || (!org1.IsSetOrgname() && org2.IsSetOrgname())) {
        return false;
    }
    if (org1.IsSetOrgname() && org2.IsSetOrgname()) {
        const COrgName& on1 = org1.GetOrgname();
        const COrgName& on2 = org2.GetOrgname();
        if ((on1.IsSetMod() && !on2.IsSetMod())
            || (!on1.IsSetMod() && on2.IsSetMod())
            || (on1.IsSetMod() && on2.IsSetMod()
                && !lists_match (on1.GetMod().begin(), on1.GetMod().end(),
                                 on2.GetMod().begin(), on2.GetMod().end(),
                                 s_OrgModEqual))) {
            return false;
        }
    }

    return true;
}


// Two SubSources are equal and duplicates if:
// they have the same subtype
// and the same name (or don't require a name).

static bool s_SubsourceEquivalent (
    const CRef<CSubSource>& st1,
    const CRef<CSubSource>& st2
)

{
    const CSubSource& sbs1 = *(st1);
    const CSubSource& sbs2 = *(st2);

    TSUBSOURCE_SUBTYPE chs1 = sbs1.GetSubtype();
    TSUBSOURCE_SUBTYPE chs2 = sbs2.GetSubtype();

    if (chs1 != chs2) return false;
    if (CSubSource::NeedsNoText(chs2)) return true;

    if (sbs1.IsSetName() && sbs2.IsSetName()) {
        if (NStr::CompareNocase (sbs1.GetName(), sbs2.GetName()) == 0) return true;
    }
    if (! sbs1.IsSetName() && ! sbs2.IsSetName()) return true;

    return false;
}


static bool s_BiosrcFullLengthIsOk (const CBioSource& src)
{
    if (src.IsSetIs_focus()) {
        return true;
    }
    FOR_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
        if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_transgenic) {
            return true;
        }
    }
    return false;
}


static bool s_SuppressMultipleEquivBioSources (const CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetTaxname()) {
        //printf ("taxname not set!\n");
        return false;
    }
    if (NStr::EqualNocase(src.GetOrg().GetTaxname(), "unidentified phage")) {
        //printf ("is unidentified phage!\n");
        return true;
    }
    if (src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetLineage()
        && NStr::StartsWith(src.GetOrg().GetOrgname().GetLineage(), "Viruses", NStr::eNocase)) {
        //printf ("Starts with viruses!\n");
        return true;
    }
#if 0
    if (!src.GetOrg().IsSetOrgname()) {
      printf ("Orgname not set!\n");
    } else if (!src.GetOrg().GetOrgname().IsSetLineage()) {
      printf ("Lineage not set!\n");
    } else {
      printf ("Lineage is %s!\n", src.GetOrg().GetOrgname().GetLineage().c_str());
    }
#endif
    return false;
}


void CValidError_bioseq::x_ValidateSourceFeatures(const CBioseq_Handle& bsh)
{
    // don't bother if can't build all feature iterator
    if (!m_AllFeatIt) {
        return;
    }
    try {
        CFeat_CI feat (bsh, SAnnotSelector (CSeqFeatData::e_Biosrc));
        if (feat) {
            if (IsLocFullLength(feat->GetLocation(), bsh) 
                && !s_BiosrcFullLengthIsOk(feat->GetData().GetBiosrc())) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                         "Source feature is full length, should be descriptor",
                         feat->GetOriginalFeature());
            }

            CFeat_CI feat_prev = feat;
            ++feat;
            while (feat) {
                if (IsLocFullLength(feat->GetLocation(), bsh)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                             "Multiple full-length source features, should only be one if descriptor is transgenic",
                             feat->GetOriginalFeature());
                }

                // compare to see if feature sources are identical
                bool are_identical = true;
                if (feat_prev->IsSetComment() && feat->IsSetComment() 
                    && !NStr::EqualNocase (feat_prev->GetComment(), feat->GetComment())) {
                    are_identical = false;
                } else {
                    const CBioSource& src_prev = feat_prev->GetData().GetBiosrc();
                    const CBioSource& src = feat->GetData().GetBiosrc();
                    if ((src.IsSetIs_focus() && !src_prev.IsSetIs_focus())
                        || (!src.IsSetIs_focus() && src_prev.IsSetIs_focus())) {
                        are_identical = false;
                    } else if ((src.IsSetSubtype() && !src_prev.IsSetSubtype())
                               || (!src.IsSetSubtype() && src_prev.IsSetSubtype())
                               || (src.IsSetSubtype() && src_prev.IsSetSubtype() 
                                   && !lists_match (src.GetSubtype().begin(), src.GetSubtype().end(),
                                                    src_prev.GetSubtype().begin(), src_prev.GetSubtype().end(),
                                                    s_SubsourceEquivalent))) {
                        are_identical = false;
                    } else if ((src.IsSetOrg() && !src_prev.IsSetOrg())
                               || (!src.IsSetOrg() && src_prev.IsSetOrg())
                               || (src.IsSetOrg() && src_prev.IsSetOrg()
                                   && !s_OrgrefEquivalent (src.GetOrg(), src_prev.GetOrg()))) {
                        are_identical = false;
                    }                    
                }
                if (are_identical && !s_SuppressMultipleEquivBioSources(feat->GetData().GetBiosrc())) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleEquivBioSources, 
                             "Multiple equivalent source features should be combined into one multi-interval feature",
                             feat->GetOriginalFeature());
                }

                ++feat_prev;
                ++feat;
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating source features. EXCEPTION: ") +
                e.what(), *(bsh.GetCompleteBioseq()));
        }
    }

}


static void s_MakePubLabelString (const CPubdesc& pd, string& label)

{
    label = "";

    FOR_EACH_PUB_ON_PUBDESC (it, pd) {
        if ((*it)->IsGen() && (*it)->GetGen().IsSetCit()
            && !(*it)->GetGen().IsSetCit()
            && !(*it)->GetGen().IsSetJournal()
            && !(*it)->GetGen().IsSetDate()
            && (*it)->GetGen().IsSetSerial_number()) {
            // skip over just serial number
        } else {
            (*it)->GetLabel (&label, CPub::eContent, true);
            break;
        }
    }
}


void CValidError_bioseq::x_ValidatePubFeatures(const CBioseq_Handle& bsh)
{
    // don't bother if can't build feature iterator at all
    if (!m_AllFeatIt) {
        return;
    }
    try {
        CFeat_CI feat (bsh, SAnnotSelector (CSeqFeatData::e_Pub));
        if (feat) {
            if (IsLocFullLength(feat->GetLocation(), bsh)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                         "Publication feature is full length, should be descriptor",
                         feat->GetOriginalFeature());
            }
     
            CFeat_CI feat_prev = feat;
            ++feat;
            while (feat) {
                if (IsLocFullLength(feat->GetLocation(), bsh)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                             "Publication feature is full length, should be descriptor",
                             feat->GetOriginalFeature());
                }
                // compare to see if feature sources are identical
                bool are_identical = true;
                if (feat_prev->IsSetComment() && feat->IsSetComment() 
                    && !NStr::EqualNocase (feat_prev->GetComment(), feat->GetComment())) {
                    are_identical = false;
                } else {
                    string label;
                    s_MakePubLabelString (feat->GetData().GetPub(), label);
                    string prev_label;
                    s_MakePubLabelString(feat_prev->GetData().GetPub(), prev_label);
                    if (!NStr::IsBlank (label) && !NStr::IsBlank(prev_label)
                        && !NStr::EqualNocase (label, prev_label)) {
                        are_identical = false;
                    }
                    // TODO: also check authors
                }

                if (are_identical) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleEquivPublications, 
                             "Multiple equivalent publication features should be combined into one multi-interval feature",
                             feat->GetOriginalFeature());
                }
                ++feat;
                ++feat_prev;
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating pub features. EXCEPTION: ") +
                e.what(), *(bsh.GetCompleteBioseq()));
        }
    }

}


struct SCaseInsensitiveCompare
{
public:
    bool operator()(const string& lhs, const string& rhs) const
    {
        return NStr::CompareNocase (lhs, rhs) < 0;
    }
};


void CValidError_bioseq::x_ReportDuplicatePubLabels (const CBioseq& seq, vector<string>& labels)
{
    if (labels.size() > 1) {
        stable_sort (labels.begin(), labels.end(), SCaseInsensitiveCompare());
        vector<string>::iterator it1 = labels.begin();
        vector<string>::iterator it2 = it1;
        ++it2;
        while (it2 != labels.end()) {
            if (NStr::EqualNocase (*it1, *it2)) {
                string summary = *it1;
                if (summary.length() > 100) {
                    summary.substr(100) = "...";
                }
                PostErr (eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications,
                         "Multiple equivalent publications annotated on this sequence [" 
                         + summary + "]", seq);
                ++it2;
                while (it2 != labels.end() && NStr::EqualNocase(*it1, *it2)) {
                    ++it2;
                }
            }
            it1 = it2;
            if (it2 != labels.end()) {
                ++it2;
            }
        }
    }

}


void CValidError_bioseq::x_ValidateMultiplePubs(const CBioseq_Handle& bsh)
{
    vector<int> muids;
    vector<int> pmids;

    vector<int> all_pmids;
    vector<int> all_muids;
    vector<int> serials;
    vector<string> published_labels;
    vector<string> unpublished_labels;

    CConstRef<CSeq_entry> ctx = bsh.GetSeq_entry_Handle().GetCompleteSeq_entry();

    for (CSeqdesc_CI it(bsh, CSeqdesc::e_Pub); it; ++it) {
        GetPubdescLabels (it->GetPub(), all_pmids, all_muids, serials, published_labels, unpublished_labels);

        int muid = 0;
        int pmid = 0;
        bool otherpub = false;
        FOR_EACH_PUB_ON_PUBDESC (pub_it, it->GetPub()) {
            switch ( (*pub_it)->Which() ) {
            case CPub::e_Muid:
                muid = (*pub_it)->GetMuid();
                break;
            case CPub::e_Pmid:
                pmid = (*pub_it)->GetPmid();
                break;
            default:
                otherpub = true;
                break;
            } 
        }
    
        if ( otherpub ) {
            bool collision = false;
            if ( muid > 0 ) {
                if (muids.size() > 0) {
                    stable_sort (muids.begin(), muids.end());
                }
                if ( binary_search(muids.begin(), muids.end(), muid) ) {
                    collision = true;
                } else {
                    muids.push_back(muid);
                }
            }
            if ( pmid > 0 ) {
                if (pmids.size() > 0) {
                    stable_sort (pmids.begin(), pmids.end());
                }
                if ( binary_search(pmids.begin(), pmids.end(), pmid) ) {
                    collision = true;
                } else {
                    pmids.push_back(pmid);
                }
            }
            if ( collision ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications,
                    "Multiple publications with same identifier", *ctx, *it);
            }
        }
    }

    x_ReportDuplicatePubLabels (*(bsh.GetCompleteBioseq()), unpublished_labels);
    x_ReportDuplicatePubLabels (*(bsh.GetCompleteBioseq()), published_labels);

}


static bool s_EqualGene_ref(const CGene_ref& genomic, const CGene_ref& mrna)
{
    bool locus = (!genomic.CanGetLocus()  &&  !mrna.CanGetLocus())  ||
        (genomic.CanGetLocus()  &&  mrna.CanGetLocus()  &&
        genomic.GetLocus() == mrna.GetLocus());
    bool allele = (!genomic.CanGetAllele()  &&  !mrna.CanGetAllele())  ||
        (genomic.CanGetAllele()  &&  mrna.CanGetAllele()  &&
        genomic.GetAllele() == mrna.GetAllele());
    bool desc = (!genomic.CanGetDesc()  &&  !mrna.CanGetDesc())  ||
        (genomic.CanGetDesc()  &&  mrna.CanGetDesc()  &&
        genomic.GetDesc() == mrna.GetDesc());
    bool locus_tag = (!genomic.CanGetLocus_tag()  &&  !mrna.CanGetLocus_tag())  ||
        (genomic.CanGetLocus_tag()  &&  mrna.CanGetLocus_tag()  &&
        genomic.GetLocus_tag() == mrna.GetLocus_tag());

    return locus  &&  allele  &&  desc  && locus_tag;
}


void CValidError_bioseq::ValidateHistory(const CBioseq& seq)
{
    if ( !seq.GetInst().IsSetHist() ) {
        return;
    }
    
    TGi gi = ZERO_GI;
    FOR_EACH_SEQID_ON_BIOSEQ (id, seq) {
        if ( (*id)->IsGi() ) {
            gi = (*id)->GetGi();
            break;
        }
    }
    if ( gi == ZERO_GI ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst().GetHist();
    if ( hist.IsSetReplaced_by() && hist.GetReplaced_by().IsSetDate() ) {
        const CSeq_hist_rec& rec = hist.GetReplaced_by();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaced by gi (" + 
                        NStr::NumericToString(gi) + ") is same as current Bioseq",
                        seq);
                    break;
                }
            }
        }
    }

    if ( hist.IsSetReplaces() && hist.GetReplaces().IsSetDate() ) {
        const CSeq_hist_rec& rec = hist.GetReplaces();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaces gi (" + 
                        NStr::NumericToString(gi) + ") is same as current Bioseq",
                        seq);
                    break;
                }
            }
        }
    }
}


// =============================================================================
//                                     Private
// =============================================================================




// Is the id contained in the bioseq?
bool CValidError_bioseq::IsIdIn(const CSeq_id& id, const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if (id.Match(**it)) {
            return true;
        }
    }
    return false;
}


TSeqPos CValidError_bioseq::GetDataLen(const CSeq_inst& inst)
{
    if (!inst.IsSetSeq_data()) {
        return 0;
    }

    const CSeq_data& seqdata = inst.GetSeq_data();
    switch (seqdata.Which()) {
    case CSeq_data::e_not_set:
        return 0;
    case CSeq_data::e_Iupacna:
        return seqdata.GetIupacna().Get().size();
    case CSeq_data::e_Iupacaa:
        return seqdata.GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi2na:
        return seqdata.GetNcbi2na().Get().size();
    case CSeq_data::e_Ncbi4na:
        return seqdata.GetNcbi4na().Get().size();
    case CSeq_data::e_Ncbi8na:
        return seqdata.GetNcbi8na().Get().size();
    case CSeq_data::e_Ncbipna:
        return seqdata.GetNcbipna().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return seqdata.GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return seqdata.GetNcbieaa().Get().size();
    case CSeq_data::e_Ncbipaa:
        return seqdata.GetNcbipaa().Get().size();
    case CSeq_data::e_Ncbistdaa:
        return seqdata.GetNcbistdaa().Get().size();
    default:
        return 0;
    }
}


// Returns true if seq derived from translation ending in "*" or
// seq is 3' partial (i.e. the right of the sequence is incomplete)
bool CValidError_bioseq::SuppressTrailingXMsg(const CBioseq& seq)
{

    // Look for the Cdregion feature used to create this aa product
    // Use the Cdregion to translate the associated na sequence
    // and check if translation has a '*' at the end. If it does.
    // message about 'X' at the end of this aa product sequence is suppressed
    try {
        const CSeq_feat* sfp = m_Imp.GetCDSGivenProduct(seq);
        if ( sfp ) {
        
            // Translate na CSeq_data
            string prot;   
            CSeqTranslator::Translate(*sfp, *m_Scope, prot); 
            
            if ( prot[prot.size() - 1] == '*' ) {
                return true;
            }
            return false;
        }

        // Get CMolInfo for seq and determine if completeness is
        // "eCompleteness_no_right or eCompleteness_no_ends. If so
        // suppress message about "X" at end of aa sequence is suppressed
        CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
        if (mi  &&  mi->IsSetCompleteness()) {
            if (mi->GetCompleteness() == CMolInfo::eCompleteness_no_right  ||
              mi->GetCompleteness() == CMolInfo::eCompleteness_no_ends) {
                return true;
            }
        }
    } catch (CException ) {
    } catch (std::exception ) {
    }
    return false;
}


CRef<CSeq_loc> CValidError_bioseq::GetLocFromSeq(const CBioseq& seq)
{
    CRef<CSeq_loc> loc;
    if (!seq.GetInst().IsSetExt()) {
        return loc;
    }

    if (seq.GetInst().GetExt().IsSeg()) {
        CRef<CSeq_loc> nloc(new CSeq_loc());
        loc = nloc;
        CSeq_loc_mix& mix = loc->SetMix();
        ITERATE (list< CRef<CSeq_loc> >, it,
            seq.GetInst().GetExt().GetSeg().Get()) {
            mix.Set().push_back(*it);
        }
    } else if (seq.GetInst().GetExt().IsRef()) {
        CRef<CSeq_loc> nloc(new CSeq_loc());
        loc = nloc;
        loc->Add(seq.GetInst().GetExt().GetRef());
    }
    return loc;
}


// Check if CdRegion required but not found
bool CValidError_bioseq::CdError(const CBioseq_Handle& bsh)
{
    if ( bsh  &&  CSeq_inst::IsAa(bsh.GetInst_Mol()) ) {
        CSeq_entry_Handle nps = 
            bsh.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
        if ( nps ) {
            const CSeq_feat* cds = GetCDSForProduct(bsh);
            if ( cds == 0 ) {
                const CSeq_feat* mat = GetPROTForProduct(bsh);
                if ( mat == 0 ) {
                    return true;
                }
            }
        }
    }

    return false;
}


bool CValidError_bioseq::IsMrna(const CBioseq_Handle& bsh) 
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);

    if ( sd ) {
        const CMolInfo &mi = sd->GetMolinfo();
        if ( mi.IsSetBiomol() ) {
            return mi.GetBiomol() == CMolInfo::eBiomol_mRNA;
        }
    } else if (bsh.GetBioseqMolType() == CSeq_inst::eMol_rna) {
        // if no molinfo, assume rna is mrna
        return true;
    }

    return false;
}


bool CValidError_bioseq::IsPrerna(const CBioseq_Handle& bsh) 
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);

    if ( sd ) {
        const CMolInfo &mi = sd->GetMolinfo();
        if ( mi.IsSetBiomol() ) {
            return mi.GetBiomol() == CMolInfo::eBiomol_pre_RNA;
        }
    }

    return false;
}


size_t CValidError_bioseq::NumOfIntervals(const CSeq_loc& loc) 
{
    size_t counter = 0;
    for ( CSeq_loc_CI slit(loc); slit; ++slit ) {
        if ( !m_Imp.IsFarLocation(slit.GetEmbeddingSeq_loc()) ) {
            ++counter;
        }
    }
    return counter;
}


bool CValidError_bioseq::LocOnSeg(const CBioseq& seq, const CSeq_loc& loc) 
{
    for ( CSeq_loc_CI sli( loc ); sli;  ++sli ) {
        const CSeq_id& loc_id = sli.GetSeq_id();
        FOR_EACH_SEQID_ON_BIOSEQ (seq_id, seq) {
            if ( loc_id.Match(**seq_id) ) {
                return true;
            }
        }
    }
    return false;
}


static bool s_NotPeptideException
(const CSeq_feat& curr,
 const CSeq_feat& prev)
{
    if (curr.IsSetExcept()  &&  curr.GetExcept()  &&  curr.IsSetExcept_text()) {
        if (NStr::FindNoCase(curr.GetExcept_text(), "alternative processing") != NPOS) {
            return false;
        }
    }
    if (prev.IsSetExcept()  &&  prev.GetExcept()  &&  prev.IsSetExcept_text()) {
        if (NStr::FindNoCase(prev.GetExcept_text(), "alternative processing") != NPOS) {
            return false;
        }
    }
    return true;
}


bool CValidError_bioseq::x_IsSameSeqAnnotDesc
(const CSeq_feat_Handle& f1,
 const CSeq_feat_Handle& f2)
{
    CSeq_annot_Handle ah1 = f1.GetAnnot();
    CSeq_annot_Handle ah2 = f2.GetAnnot();

    if (!ah1  ||  !ah2) {
        return true;
    }

    CConstRef<CSeq_annot> sap1 = ah1.GetCompleteSeq_annot();
    CConstRef<CSeq_annot> sap2 = ah2.GetCompleteSeq_annot();
    if (!sap1  ||  !sap2) {
        return true;
    }

    if (!sap1->IsSetDesc()  ||  !sap2->IsSetDesc()) {
        return true;
    }

    CAnnot_descr::Tdata descr1 = sap1->GetDesc().Get();
    CAnnot_descr::Tdata descr2 = sap2->GetDesc().Get();

    // Check only on the first? (same as in C toolkit)
    const CAnnotdesc& desc1 = descr1.front().GetObject();
    const CAnnotdesc& desc2 = descr2.front().GetObject();

    if ( desc1.Which() == desc2.Which() ) {
        if ( desc1.IsName() ) {
            return NStr::EqualNocase(desc1.GetName(), desc2.GetName());
        } else if ( desc1.IsTitle() ) {
            return NStr::EqualNocase(desc1.GetTitle(), desc2.GetTitle());
        }
    }

    return false;
}


#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

void CValidError_bioseq::ValidateSeqLen(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    TSeqPos len = inst.IsSetLength() ? inst.GetLength() : 0;
    if ( seq.IsAa() ) {
        if (len <= 3) {
            bool short_ok = false;
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);        
            if ( bsh ) {
                FOR_EACH_SEQID_ON_BIOSEQ_HANDLE (sid_itr, bsh) {
                    CSeq_id_Handle sid = *sid_itr;
                    switch (sid.Which()) {
                        case NCBI_SEQID(Other):
                        case NCBI_SEQID(Genbank):
                        case NCBI_SEQID(Embl):
                        case NCBI_SEQID(Ddbj):
                        {
                            CConstRef<CSeq_id> id = sid.GetSeqId();
                            const CTextseq_id& tsid = *id->GetTextseq_Id ();
                            if (tsid.IsSetAccession()) {
                                const string& acc = tsid.GetAccession ();
                                TACCN_CHOICE type = CSeq_id::IdentifyAccession (acc);
                                TACCN_CHOICE div = (TACCN_CHOICE) (type & NCBI_ACCN(division_mask));
                                if ( div == NCBI_ACCN(wgs) ) 
                                {
                                    if( (type & CSeq_id::fAcc_master) != 0 ) {
                                        short_ok = true;
                                    }
                                }
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
                CSeqdesc_CI desc( bsh, CSeqdesc::e_Molinfo );
                if (desc && desc->GetMolinfo().IsSetCompleteness()) {
                    CMolInfo::TCompleteness completeness = desc->GetMolinfo().GetCompleteness();
                    if (completeness == CMolInfo::eCompleteness_partial
                        || completeness == CMolInfo::eCompleteness_no_left
                        || completeness == CMolInfo::eCompleteness_no_right
                        || completeness == CMolInfo::eCompleteness_no_ends) {
                        short_ok = true;
                    }
                }
            }

            if (!short_ok) {
                FOR_EACH_SEQID_ON_BIOSEQ (id, seq) {
                    if ((*id)->IsPdb()) {
                        short_ok = true;
                        break;
                    }
                }
            }

            if (!short_ok) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                    NStr::IntToString(len) + " residues", seq);
            }
        }
    } else {
        if ( len <= 10  &&  !m_Imp.IsPDB()) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residues", seq);
        }
    }
    
    /*
    if ( (len <= 350000)  ||  m_Imp.IsNC()  ||  m_Imp.IsNT() ) {
        return;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }
    CSeqdesc_CI desc( bsh, CSeqdesc::e_Molinfo );
    const CMolInfo* mi = desc ? &(desc->GetMolinfo()) : 0;

    if ( inst.GetRepr() == CSeq_inst::eRepr_delta ) {
        if ( mi  &&  m_Imp.IsGED() ) {
            CMolInfo::TTech tech = mi->IsSetTech() ? 
                mi->GetTech() : CMolInfo::eTech_unknown;

            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                PostErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                len = 0;
                bool litHasData = false;
                CTypeConstIterator<CSeq_literal> lit(ConstBegin(seq));
                for (; lit; ++lit) {
                    if (lit->IsSetSeq_data()) {
                        litHasData = true;
                    }
                    len += lit->GetLength();
                }
                if ( len > 500000  && litHasData ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_LongLiteralSequence,
                        "Length of sequence literals exceeds 500kbp limit",
                        seq);
                }
            }
        }
    } else if ( inst.GetRepr() == CSeq_inst::eRepr_raw ) {
        if ( mi ) {
            CMolInfo::TTech tech = mi->IsSetTech() ? 
                mi->GetTech() : CMolInfo::eTech_unknown;
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                PostErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Length of sequence exceeds 350kbp limit", seq);
            }
        }  else {
            PostErr (eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                "Length of sequence exceeds 350kbp limit", seq);
        }
    }
    */
}


// Assumes that seq is segmented and has Seq-ext data
void CValidError_bioseq::ValidateSeqParts(const CBioseq& seq)
{
    // Get parent CSeq_entry of seq and then find the next
    // CSeq_entry in the set. This CSeq_entry should be a CBioseq_set
    // of class parts.
    const CSeq_entry* se = seq.GetParentEntry();
    if (!se) {
        return;
    }
    const CSeq_entry* parent = se->GetParentEntry ();
    if (!parent) {
        return;
    }
    if ( !parent->IsSet() || !parent->GetSet().IsSetClass() || parent->GetSet().GetClass() != CBioseq_set::eClass_segset) {
        return;
    }

    // Loop through seq_set looking for the parts set.
    FOR_EACH_SEQENTRY_ON_SEQSET (it, parent->GetSet()) {
        if ((*it)->Which() == CSeq_entry::e_Set
            && (*it)->GetSet().IsSetClass()
            && (*it)->GetSet().GetClass() == CBioseq_set::eClass_parts) {
            const CBioseq_set::TSeq_set& parts = (*it)->GetSet().GetSeq_set();
            const CSeg_ext::Tdata& locs = seq.GetInst().GetExt().GetSeg().Get();

            // Make sure the number of locations (excluding null locations)
            // match the number of parts
            size_t nulls = 0;
            ITERATE ( CSeg_ext::Tdata, loc, locs ) {
                if ( (*loc)->IsNull() ) {
                    nulls++;
                }
            }
            if ( locs.size() - nulls < parts.size() ) {
                 PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Parts set contains too many Bioseqs", seq);
                 return;
            } else if ( locs.size() - nulls > parts.size() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Parts set does not contain enough Bioseqs", seq);
                return;
            }

            // Now, simultaneously loop through the parts of se_parts and CSeq_locs of 
            // seq's CSseq-ext. If don't compare, post error.
            size_t size = locs.size();  // == parts.size()
            CSeg_ext::Tdata::const_iterator loc_it = locs.begin();
            CBioseq_set::TSeq_set::const_iterator part_it = parts.begin();
            for ( size_t i = 0; i < size; ++i ) {
                try {
                    if ( (*loc_it)->IsNull() ) {
                        ++loc_it;
                        continue;
                    }
                    if ( !(*part_it)->IsSeq() ) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                            "Parts set component is not Bioseq", seq);
                        return;
                    }
                    const CSeq_id& loc_id = GetId(**loc_it, m_Scope);
                    if ( !IsIdIn(loc_id, (*part_it)->GetSeq()) ) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                            "Segmented bioseq seq_ext does not correspond to parts "
                            "packaging order", seq);
                        return;
                    }
                    
                    // advance both iterators
                    ++part_it;
                    ++loc_it;
                } catch (const CObjmgrUtilException&) {
                    ERR_POST_X(4, "Seq-loc not for unique sequence");
                    return;
                } catch (CException &x1) {
                    string err_msg = "Unknown error:";
                    err_msg += x1.what();
                    ERR_POST_X(5, err_msg);
                    return;
                } catch (std::exception &x2) {
                    string err_msg = "Unknown error:";
                    err_msg += x2.what();
                    ERR_POST_X(5, err_msg);
                    return;
                }
            }
        }
    }
}

static bool s_IsConWithGaps(const CBioseq& seq)

{

    if (! seq.IsSetInst ()) return false;
    const CSeq_inst& inst = seq.GetInst();
    if (! inst.IsSetExt ()) return false;
    if (! inst.GetExt().IsDelta()) return false;

    ITERATE(CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
        if (! (*iter)->IsLiteral() ) continue;
        const CSeq_literal& lit = (*iter)->GetLiteral();
        if (!lit.IsSetSeq_data()) return true;
        if  (lit.GetSeq_data().IsGap() && lit.GetLength() > 0) return true;
    }

    return false;
}

void CValidError_bioseq::x_ValidateTitle(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if (!bsh) {
        return;
    }

    string title = sequence::CDeflineGenerator().GenerateDefline(bsh);

    CMolInfo::TTech tech = CMolInfo::eTech_unknown;
    CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo);
    if (desc) {
        const CMolInfo& mi = desc->GetMolinfo();
        tech = mi.GetTech();
        if (mi.GetCompleteness() != CMolInfo::eCompleteness_complete) {
            if (m_Imp.IsGenbank()) {
                if (NStr::Find(title, "complete genome") != NPOS) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteTitleProblem,
                        "Complete genome in title without complete flag set",
                        seq);
                }
            }
            if (bsh.GetInst_Topology() == CSeq_inst::eTopology_circular && (! s_IsConWithGaps (seq))) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteCircleProblem,
                    "Circular topology without complete flag set", seq);
            }
        }
    }

    // warning if title contains complete genome but sequence contains gap features
    if (NStr::FindNoCase (title, "complete genome") != NPOS) {
        bool has_gap = false;
        if ( seq.GetInst().IsSetExt()  &&  seq.GetInst().GetExt().IsDelta() ) {
            ITERATE(CDelta_ext::Tdata, iter, seq.GetInst().GetExt().GetDelta().Get()) {
                if ( ((*iter)->IsLiteral() && (*iter)->GetLiteral().IsSetFuzz())) {
                    has_gap = true;
                    break;
                }
            }
        }
        if (has_gap) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteTitleProblem, 
                    "Title contains 'complete genome' but sequence has gaps", seq);
        }
    }


    // note - test for protein titles was moved to CValidError_bioseqset::ValidateNucProtSet
    // because it only applied for protein sequences in nuc-prot sets and it's more efficient
    // to create the defline generator once per nuc-prot set
}

static bool HasAssemblyOrNullGap (const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    if (inst.CanGetRepr() && inst.GetRepr() == CSeq_inst::eRepr_delta && inst.CanGetExt()  &&  inst.GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
            if ( !(*sg) ) continue;
            if ((**sg).Which() != CDelta_seq::e_Literal) continue;
            const CSeq_literal& lit = (*sg)->GetLiteral();
            if (! lit.IsSetSeq_data()) return true;
            if (lit.GetSeq_data().IsGap()) return true;
        }
    }

    return false;
}


void CValidError_bioseq::ReportBadAssemblyGap (const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    if (inst.CanGetRepr() && inst.GetRepr() == CSeq_inst::eRepr_delta && inst.CanGetExt()  &&  inst.GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
            if ( !(*sg) ) continue;
            if ((**sg).Which() != CDelta_seq::e_Literal) continue;
            const CSeq_literal& lit = (*sg)->GetLiteral();
            if (! lit.IsSetSeq_data()) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapProblem, "TSA Seq_data NULL", seq);
            } else {
                const CSeq_data& data = lit.GetSeq_data();
                if (data.Which() == CSeq_data::e_Gap) {
                    const CSeq_gap& gap = data.GetGap();
                    if (gap.IsSetType()) {
                        int gaptype = gap.GetType();
                        if (gaptype == CSeq_gap::eType_unknown) {
                            PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapProblem, "TSA Seq_gap.unknown", seq);
                        } else if (gaptype == CSeq_gap::eType_other) {
                            PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapProblem, "TSA Seq_gap.other", seq);
                        }
                    } else {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapProblem, "TSA Seq_gap NULL", seq);
                    }
                }
            }
        }
    }
}


static EDiagSev GetBioseqEndWarning (bool is_NC, bool isPatent, bool only_local, bool is_circular, EBioseqEndIsType end_is_char)
{
    EDiagSev sev;

    if (is_NC || isPatent) {
        sev = eDiag_Warning;
    } else if (is_circular) {
        sev = eDiag_Warning;
    } else if (only_local) {
        sev = eDiag_Warning;
    } else if (end_is_char == eBioseqEndIsType_All) {
        sev = eDiag_Error;
    } else {
        sev = eDiag_Warning;
    }
    return sev;
}


void CValidError_bioseq::ValidateNsAndGaps(const CBioseq& seq)
{
    // don't bother checking if length is less than 10
    if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr() 
        || !seq.GetInst().IsSetLength() || seq.GetInst().GetLength() < 10) {
        return;
    }

    CSeq_inst::TRepr repr = seq.GetInst().GetRepr();

    // only check for raw or for delta sequences that are delta lit only
    if ( repr != CSeq_inst::eRepr_raw  && (repr != CSeq_inst::eRepr_delta  ||  !x_IsDeltaLitOnly(seq.GetInst()))) {
        return;
    }
         
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }    
    EBioseqEndIsType begin_n;
    EBioseqEndIsType begin_gap;
    EBioseqEndIsType end_n;
    EBioseqEndIsType end_gap;
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap);

    bool only_local = true;
    bool is_NC = false;
    bool is_patent = false;
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if (!(*id_it)->IsLocal()) {
            only_local = false;
            if ((*id_it)->IsPatent()) {
                is_patent = true;
            } else if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()
                && NStr::StartsWith ((*id_it)->GetOther().GetAccession(), "NC_")) {
                is_NC = true;
            }
        }
    }
    bool is_circular = false;
    if (bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular) {
        is_circular = true;
    }
    EDiagSev sev;
    if (begin_n != eBioseqEndIsType_None) {
        sev = GetBioseqEndWarning(is_NC, is_patent, only_local, is_circular, begin_n);
        PostErr(sev, eErr_SEQ_INST_TerminalNs, "N at beginning of sequence", seq);
    } else if (begin_gap != eBioseqEndIsType_None) {
        sev = GetBioseqEndWarning(is_NC, is_patent, only_local, is_circular, begin_gap);
        PostErr (sev, eErr_SEQ_INST_TerminalGap, "Gap at beginning of sequence", seq);
    }

    if (end_n != eBioseqEndIsType_None) {
        sev = GetBioseqEndWarning(is_NC, is_patent, only_local, is_circular, end_n);
        PostErr(sev, eErr_SEQ_INST_TerminalNs, "N at end of sequence", seq);
    } else if (end_gap != eBioseqEndIsType_None) {
        sev = GetBioseqEndWarning(is_NC, is_patent, only_local, is_circular, end_gap);
        PostErr (sev, eErr_SEQ_INST_TerminalGap, "Gap at end of sequence", seq);
    }

    // don't check N content for patent sequences
    if (is_patent) {
        return;
    }

    try {            
        CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);                        

        // if TSA, check for percentage of Ns and max stretch of Ns
        if (IsBioseqTSA(seq, m_Scope)) {
            ReportBadAssemblyGap (seq);
            if (HasAssemblyOrNullGap (seq)) return;
            bool n5 = false;
            bool n3 = false;
            TSeqPos num_ns = 0, this_stretch = 0, max_stretch = 0;
            for (size_t i = 0; i < vec.size(); i++) {
                if (vec[i] == 'N') {
                    num_ns++;
                    if (vec.IsInGap(i)) {
                        if (max_stretch < this_stretch) {
                            max_stretch = this_stretch;
                        }
                        this_stretch = 0;
                    } else {
                        this_stretch++;
                        if (this_stretch >= 10) {
                            if (i < 20) {
                                n5 = true;
                            } 
                            if (vec.size() > 20 && i > vec.size() - 10) {
                                n3 = true;
                            }
                        }
                    }
                } else {
                    if (max_stretch < this_stretch) {
                        max_stretch = this_stretch;
                    }
                    this_stretch = 0;
                }
            }
            if (max_stretch < this_stretch) {
                max_stretch = this_stretch;
            }

            int pct_n = (num_ns * 100) / seq.GetLength();
            if (pct_n > 10) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentPercent,
                            "Sequence contains " + NStr::IntToString(pct_n) + " percent Ns", seq);
            }

            if (max_stretch >= 15) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentStretch, 
                            "Sequence has a stretch of " + NStr::IntToString(max_stretch) + " Ns", seq);
            } else {
                if (n5) {
                    PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentStretch, 
                                "Sequence has a stretch of at least 10 Ns within the first 20 bases", seq);
                }
                if (n3) {
                    PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentStretch, 
                                "Sequence has a stretch of at least 10 Ns within the last 20 bases", seq);
                }
            }
        } else {
            // not TSA, just check for really high N percent
            TSeqPos num_ns = 0;
            for (size_t i = 0; i < vec.size(); i++) {
                if (vec[i] == 'N' && !vec.IsInGap(i)) {
                    num_ns++;
                }
            }
            int pct_n = (num_ns * 100) / seq.GetLength();
            if (pct_n > 50) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentPercent, 
                            "Sequence contains " + NStr::IntToString(pct_n) + " percent Ns", seq);
            }
        }
    } catch ( exception& ) {
        // just ignore, and continue with the validation process.
    }
}


// Assumes that seq is eRepr_raw or eRepr_inst
void CValidError_bioseq::ValidateRawConst(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);

    if (inst.IsSetFuzz() && (!inst.IsSetSeq_data() || !inst.GetSeq_data().IsGap())) {
        PostErr(eDiag_Error, eErr_SEQ_INST_FuzzyLen,
            "Fuzzy length on " + rpr + " Bioseq", seq);
    }

    if (!inst.IsSetLength()  ||  inst.GetLength() == 0) {
        string len = inst.IsSetLength() ?
            NStr::IntToString(inst.GetLength()) : "0";
        PostErr(eDiag_Error, eErr_SEQ_INST_InvalidLen,
                 "Invalid Bioseq length [" + len + "]", seq);
    }

    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
        const CMolInfo* mi = 0;
        CSeqdesc_CI mi_desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Molinfo);
        if ( mi_desc ) {
            mi = &(mi_desc->GetMolinfo());
        }
        CMolInfo::TTech tech = 
            mi != 0 ? mi->GetTech() : CMolInfo::eTech_unknown;
        if (tech == CMolInfo::eTech_htgs_2  &&
            !GraphsOnBioseq(seq)  &&
            !x_IsActiveFin(seq)) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadHTGSeq,
                "HTGS 2 raw seq has no gaps and no graphs", seq);
        }
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CSeq_data::E_Choice seqtyp = inst.IsSetSeq_data() ?
        inst.GetSeq_data().Which() : CSeq_data::e_not_set;
    if (seqtyp != CSeq_data::e_Gap) {
        switch (seqtyp) {
        case CSeq_data::e_Iupacna:
        case CSeq_data::e_Ncbi2na:
        case CSeq_data::e_Ncbi4na:
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbipna:
            if (inst.IsAa()) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                         "Using a nucleic acid alphabet on a protein sequence",
                         seq);
                return;
            }
            break;
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbi8aa:
        case CSeq_data::e_Ncbieaa:
        case CSeq_data::e_Ncbipaa:
        case CSeq_data::e_Ncbistdaa:
            if (inst.IsNa()) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                         "Using a protein alphabet on a nucleic acid",
                         seq);
                return;
            }
            break;
        case CSeq_data::e_Gap:
            break;
        default:
            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Sequence alphabet not set",
                     seq);
            return;
        }

        bool check_alphabet = false;
        unsigned int factor = 1;
        switch (seqtyp) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Iupacna:
        case CSeq_data::e_Ncbieaa:
        case CSeq_data::e_Ncbistdaa:
            check_alphabet = true;
            break;
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbi8aa:
            break;
        case CSeq_data::e_Ncbi4na:
            factor = 2;
            break;
        case CSeq_data::e_Ncbi2na:
            factor = 4;
            break;
        case CSeq_data::e_Ncbipna:
            factor = 5;
            break;
        case CSeq_data::e_Ncbipaa:
            factor = 21;
            break;
        default:
            // Logically, should not occur
            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Sequence alphabet not set",
                     seq);
            return;
        }
        TSeqPos calc_len = inst.IsSetLength() ? inst.GetLength() : 0;
        
        if (calc_len % factor) {
            calc_len += factor;
        }
        calc_len /= factor;

        string s_len = NStr::UIntToString(inst.GetLength());

        TSeqPos data_len = GetDataLen(inst);
        string data_len_str = NStr::IntToString(data_len * factor);
        if (calc_len > data_len) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                     "Bioseq.seq_data too short [" + data_len_str +
                     "] for given length [" + s_len + "]", seq);
            return;
        } else if (calc_len < data_len) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                     "Bioseq.seq_data is larger [" + data_len_str +
                     "] than given length [" + s_len + "]", seq);
        }

        if (check_alphabet) {
            unsigned int trailingX = 0;
            size_t terminations = 0, dashes = 0;
            bool gap_at_start = false, leading_x = false, found_lower = false;

            CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            if (seqtyp == CSeq_data::e_Ncbieaa || seqtyp == CSeq_data::e_Ncbistdaa) {
                sv.SetCoding (CSeq_data::e_Ncbieaa);
            }
            CSeqVector sv_res = bsh.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);

            size_t bad_cnt = 0;
            TSeqPos pos = 1;
            for ( CSeqVector_CI sv_iter(sv), sv_res_iter(sv_res); (sv_iter) && (sv_res_iter); ++sv_iter, ++sv_res_iter ) {
                CSeqVector::TResidue res = *sv_iter;
                CSeqVector::TResidue n_res = *sv_res_iter;
                if ( !IsResidue(n_res) ) {
                    if (res == 'U' && bsh.IsSetInst_Mol() && bsh.GetInst_Mol() == CSeq_inst::eMol_rna) {
                        // U is ok for RNA
                    } else if (res == '*' && bsh.IsAa()) {
                        terminations++;
                        trailingX = 0;
                    } else {
                        if (res > 250) {                            
                            if ( ++bad_cnt > 10 ) {
                                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                    "More than 10 invalid residues. Checking stopped",
                                    seq);
                                return;
                            } else {
                                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                        "Invalid residue [" + NStr::UIntToString(res) 
                                        + "] at position [" + NStr::UIntToString(pos) + "]",
                                        seq);
                            }
                        } else if (islower (res)) {
                            found_lower = true;
                        } else {
                            string msg = "Invalid";
                            if (seq.IsNa() && strchr ("EFIJLOPQXZ", res) != NULL) {
                                msg += " nucleotide";
                            } else if (seq.IsNa() && res == 'U') {
                                msg += " nucleotide";
                            }
                            msg += " residue '";
                            msg += res;
                            msg += "' at position [" + NStr::UIntToString(pos) + "]";

                            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                                msg, seq);
                        }
                    }
                } else if ( sv.IsInGap(pos - 1) || res == '-') {
                    dashes++;
                    if (pos == 1) {
                        gap_at_start = true;
                    }
                } else if ( res == '*') {
                    terminations++;
                    trailingX = 0;
                } else if ( res == 'X' ) {
                    trailingX++;
                    if (pos == 1) {
                        leading_x = true;
                    }
                } else if (!isalpha (res)) {
                    string msg = "Invalid residue [";
                    msg += res;
                    msg += "] in position [" + NStr::UIntToString(pos) + "]";
                    PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                        msg, seq);
                } else {
                    trailingX = 0;
                }
                ++pos;
            }

            // only show leading or trailing X if product of NNN in nucleotide
            if (seq.IsAa() && (leading_x || trailingX > 0)) {
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
                const CSeq_feat* cds = GetCDSForProduct(bsh);
                if (cds && cds->IsSetLocation()) {
                    size_t dna_len = GetLength (cds->GetLocation(), m_Scope);
                    if (dna_len > 5) {
                        string cds_seq = GetSequenceStringFromLoc (cds->GetLocation(), *m_Scope);
                        if (cds->GetData().GetCdregion().IsSetFrame()) {
                            if (cds->GetData().GetCdregion().GetFrame() == 2) {
                                cds_seq = cds_seq.substr(1);
                            } else if (cds->GetData().GetCdregion().GetFrame() == 3) {
                                cds_seq = cds_seq.substr(2);
                            }
                        }

                        if (!NStr::StartsWith (cds_seq, "NNN")) {
                            leading_x = false;
                        }
                        string lastcodon = cds_seq.substr(cds_seq.length() - 3);
                        if (!NStr::StartsWith (lastcodon, "NNN")) {
                            trailingX = 0;
                        }
                    }
                }
            }

            if (leading_x) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_LeadingX, 
                         "Sequence starts with leading X", seq);
            }
                
            if ( trailingX > 0 && !SuppressTrailingXMsg(seq) ) {
                // Suppress if cds ends in "*" or 3' partial
                string msg = "Sequence ends in " +
                    NStr::IntToString(trailingX) + " trailing X";
                if ( trailingX > 1 ) {
                    msg += "s";
                }
                PostErr(eDiag_Warning, eErr_SEQ_INST_TrailingX, msg, seq);
            }

            if (found_lower) {
                PostErr (eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                         "Sequence contains lower-case characters", seq);
            }

            if (terminations > 0 || dashes > 0) {
                // Post error indicating terminations found in protein sequence
                // if possible, get gene and protein names
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
                // First get gene label
                string gene_label = "";
                try {
                    const CSeq_feat* cds = GetCDSForProduct(bsh);
                    if (cds) {
                        CConstRef<CSeq_feat> gene = GetOverlappingGene (cds->GetLocation(), *m_Scope);
                        if (gene && gene->IsSetData() && gene->GetData().IsGene()) {
                            gene->GetData().GetGene().GetLabel(&gene_label);
                        }
                    }
                } catch (...) {
                }
                // get protein label
                string protein_label = "";
                try {
                    CFeat_CI feat_ci (bsh, SAnnotSelector (CSeqFeatData::e_Prot));
                    if (feat_ci && feat_ci->GetData().IsProt() 
                        && feat_ci->GetData().GetProt().IsSetName()
                        && feat_ci->GetData().GetProt().GetName().size() > 0) {
                        protein_label = feat_ci->GetData().GetProt().GetName().front();
                    }
                } catch (CException ) {
                } catch (std::exception ) {
                }

                string msg = "[" + NStr::SizetToString(terminations) + "] termination symbols in protein sequence";

                if (NStr::IsBlank(gene_label)) {
                    gene_label = "gene?";
                }
                if (NStr::IsBlank(protein_label)) {
                    protein_label = "prot?";
                }
                msg += " (" + gene_label + " - " + protein_label + ")";

                if (dashes > 0) {
                    if (gap_at_start && dashes == 1) {
                      PostErr (eDiag_Error, eErr_SEQ_INST_BadProteinStart, 
                               "gap symbol at start of protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    } else if (gap_at_start) {
                      PostErr (eDiag_Error, eErr_SEQ_INST_BadProteinStart, 
                               "gap symbol at start of protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                      PostErr (eDiag_Error, eErr_SEQ_INST_GapInProtein, 
                               "[" + NStr::SizetToString (dashes - 1) + "] internal gap symbols in protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    } else {
                      PostErr (eDiag_Error, eErr_SEQ_INST_GapInProtein, 
                               "[" + NStr::SizetToString (dashes) + "] internal gap symbols in protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    }
                }

                if (terminations > 0) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_StopInProtein, msg, seq);
                }
            }
        }

        bool is_wgs = false;
        CSeqdesc_CI mi_desc(bsh, CSeqdesc::e_Molinfo);
        if (mi_desc && mi_desc->IsMolinfo() && mi_desc->GetMolinfo().IsSetTech()
            && mi_desc->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
            is_wgs = true;
        }

        if (seq.IsNa() && seq.GetInst().GetRepr() == CSeq_inst::eRepr_raw) {
            // look for runs of Ns and gap characters
            bool has_gap_char = false;
            size_t run_len = 0;
            TSeqPos start_pos = 0;
            TSeqPos pos = 1;
            CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            for ( CSeqVector_CI sv_iter(sv); (sv_iter); ++sv_iter, ++pos ) {
                CSeqVector::TResidue res = *sv_iter;
                if (res == 'N') {
                    if (run_len == 0) {
                        start_pos = pos;
                    }
                    run_len++;
                } else {
                    if (run_len > 0 && start_pos > 1
                        && ((is_wgs && run_len >= 20) || run_len >= 100)) {
                        PostErr (eDiag_Warning, eErr_SEQ_INST_InternalNsInSeqRaw, 
                                 "Run of " + NStr::SizetToString (run_len) + " Ns in raw sequence starting at base "
                                 + NStr::IntToString (start_pos),
                                 seq);
                    }
                    run_len = 0;
                    if (res == '-') {
                        has_gap_char = true;
                    }
                }
            }
            if (has_gap_char) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_InternalGapsInSeqRaw, 
                         "Raw nucleotide should not contain gap characters", seq);
            }
        }
    }
}


// Assumes seq is eRepr_seg or eRepr_ref
void CValidError_bioseq::ValidateSegRef(const CBioseq& seq)
{
    string id_test_label;
    seq.GetLabel(&id_test_label, CBioseq::eContent);

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CRef<CSeq_loc> loc = GetLocFromSeq(seq);
    if (loc) {
        if (inst.IsSetRepr() && inst.GetRepr() == CSeq_inst::eRepr_seg) {
            m_Imp.ValidateSeqLoc(*loc, bsh, "Segmented Bioseq", seq);
        }

        // Validate Length
        try {
            TSeqPos loclen = GetLength(*loc, m_Scope);
            TSeqPos seqlen = inst.IsSetLength() ? inst.GetLength() : 0;
            if (seqlen > loclen) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                    "Bioseq.seq_data too short [" + NStr::IntToString(loclen) +
                    "] for given length [" + NStr::IntToString(seqlen) + "]",
                    seq);
            } else if (seqlen < loclen) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                    "Bioseq.seq_data is larger [" + NStr::IntToString(loclen) +
                    "] than given length [" + NStr::IntToString(seqlen) + "]",
                    seq);
            }
        } catch (const CObjmgrUtilException&) {
            ERR_POST_X(6, Critical << "Unable to calculate length: ");
        }
    }

    // Check for multiple references to the same Bioseq
    if (inst.IsSetExt()  &&  inst.GetExt().IsSeg()) {
        const list< CRef<CSeq_loc> >& locs = inst.GetExt().GetSeg().Get();
        ITERATE(list< CRef<CSeq_loc> >, i1, locs) {
           if (!IsOneBioseq(**i1, m_Scope)) {
                continue;
            }
            const CSeq_id& id1 = GetId(**i1, m_Scope);
            list< CRef<CSeq_loc> >::const_iterator i2 = i1;
            for (++i2; i2 != locs.end(); ++i2) {
                if (!IsOneBioseq(**i2, m_Scope)) {
                    continue;
                }
                const CSeq_id& id2 = GetId(**i2, m_Scope);
                if (IsSameBioseq(id1, id2, m_Scope)) {
                    string sid;
                    id1.GetLabel(&sid);
                    if ((**i1).IsWhole()  &&  (**i2).IsWhole()) {
                        PostErr(eDiag_Error,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid, seq);
                    } else {
                        PostErr(eDiag_Warning,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid + " that are not SEQLOC_WHOLE", seq);
                    }
                }
            }
        }
    }

    // Check that partial sequence info on sequence segments is consistent with
    // partial sequence info on sequence -- aa  sequences only
    int partial = SeqLocPartialCheck(*loc, m_Scope);
    if (seq.IsAa()) {
        bool got_partial = false;
        FOR_EACH_DESCRIPTOR_ON_BIOSEQ (sd, seq) {
            if (!(*sd)->IsMolinfo() || !(*sd)->GetMolinfo().IsSetCompleteness()) {
                continue;
            }
           
            switch ((*sd)->GetMolinfo().GetCompleteness()) {
                case CMolInfo::eCompleteness_partial:
                    got_partial = true;
                    if (!partial) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                                 "Complete segmented sequence with MolInfo partial", seq);
                    }
                    break;
                case CMolInfo::eCompleteness_no_left:
                    if (!(partial & eSeqlocPartial_Start) || (partial & eSeqlocPartial_Stop)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-left inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                case CMolInfo::eCompleteness_no_right:
                    if (!(partial & eSeqlocPartial_Stop) || (partial & eSeqlocPartial_Start)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-right inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                case CMolInfo::eCompleteness_no_ends:
                    if (!(partial & eSeqlocPartial_Start) || !(partial & eSeqlocPartial_Stop)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-ends inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                default:
                    break;
            }
        }
        if (!got_partial) {
            PostErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                "Partial segmented sequence without MolInfo partial", seq);
        }
    }
}


static int s_MaxNsInSeqLitForTech (CMolInfo::TTech tech)
{
    int max_ns = -1;

    switch (tech) {
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_composite_wgs_htgs:
            max_ns = 80;
            break;
        case CMolInfo::eTech_wgs:
            max_ns = 19;
            break;
        default:
            max_ns = 99;
            break;
    }
    return max_ns;
}


static bool s_IsSwissProt (const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsSwissprot()) {
            return true;
        }
    }
    return false;
}

static bool s_LocSortCompare (const CConstRef<CSeq_loc>& q1, const CConstRef<CSeq_loc>& q2)
{
    TIntId cmp = q1->GetId()->CompareOrdered(*(q2->GetId()));
    if (cmp < 0) {
        return true;
    } else if (cmp > 0) {
        return false;
    }

    TSeqPos start1 = q1->GetStart(eExtreme_Positional);
    TSeqPos start2 = q2->GetStart(eExtreme_Positional);
    if (start1 < start2) {
        return true;
    } else if (start2 < start1) {
        return false;
    }
        
    TSeqPos stop1 = q1->GetStop(eExtreme_Positional);
    TSeqPos stop2 = q2->GetStop(eExtreme_Positional);

    if (stop1 < stop2) {
        return true;
    } else {
        return false;
    }
}


void CValidError_bioseq::ValidateDeltaLoc
(const CSeq_loc& loc,
 const CBioseq& seq, 
 TSeqPos& len)
{
    if (loc.IsWhole()) {
        PostErr (eDiag_Error, eErr_SEQ_INST_WholeComponent, 
                 "Delta seq component should not be of type whole", seq);
    }

    const CSeq_id *id = loc.GetId();
    if (id) {
        if (id->IsGi() && loc.GetId()->GetGi() == ZERO_GI) {
            PostErr (eDiag_Critical, eErr_SEQ_INST_DeltaComponentIsGi0, 
                     "Delta component is gi|0", seq);
        }
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
            if ((*id_it)->Compare(*id) == CSeq_id::e_YES) {
                PostErr (eDiag_Critical, eErr_SEQ_INST_SelfReferentialSequence,
                         "Self-referential delta sequence", seq);
            }
        }
        if (!loc.IsWhole()
            && (id->IsGi() 
                || id->IsGenbank() 
                || id->IsEmbl()
                || id->IsDdbj() || id->IsTpg()
                || id->IsTpe()
                || id->IsTpd()
                || id->IsOther())) {
            TSeqPos stop = loc.GetStop(eExtreme_Positional);
            try {
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id, CScope::eGetBioseq_All);
                if (bsh) {
                    TSeqPos seq_len = bsh.GetBioseqLength(); 
                    if (seq_len <= stop) {
                        string id_label;
                        id->GetLabel(&id_label);
                        PostErr (eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                                 "Seq-loc extent (" + NStr::IntToString (stop + 1) 
                                 + ") greater than length of " + id_label
                                 + " (" + NStr::IntToString(seq_len) + ")",
                                seq);
                    }
                }
            } catch (CException ) {
            } catch (std::exception ) {
            }
        }
    }

    try {
        if (seq.IsSetInst ()) {
            const CSeq_inst& inst = seq.GetInst();
            size_t loc_len = GetLength(loc, m_Scope);
            if (loc_len == numeric_limits<TSeqPos>::max()) {
                PostErr (eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                         "-1 length on seq-loc of delta seq_ext", seq);
                string loc_str;
                loc.GetLabel(&loc_str);
                if ( loc_str.empty() ) {
                    loc_str = "?";
                }
                if (x_IsDeltaLitOnly(inst)) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLocLength,
                        "Short length (-1) on seq-loc (" + loc_str + ") of delta seq_ext", seq);
                }
            } else {
                len += loc_len;
            }
            if ( loc_len <= 10 ) {
                string loc_str;
                loc.GetLabel(&loc_str);
                if ( loc_str.empty() ) {
                    loc_str = "?";
                }
                if (x_IsDeltaLitOnly(inst)) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLocLength,
                        "Short length (" + NStr::SizetToString(loc_len) + 
                        ") on seq-loc (" + loc_str + ") of delta seq_ext", seq);
                }
            }
        }

    } catch (const CObjmgrUtilException&) {
        string loc_str;
        loc.GetLabel(&loc_str);
        if ( loc_str.empty() ) {
            loc_str = "?";
        }
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No length for Seq-loc (" + loc_str + ") of delta seq-ext",
            seq);
    }
}


static size_t s_GetDeltaLen (const CDelta_seq& seg, CScope* scope)
{
    if (seg.IsLiteral()) {
        return seg.GetLiteral().GetLength();
    } else if (seg.IsLoc()) {
        return GetLength (seg.GetLoc(), scope);
    } else {
        return 0;
    }
}


static string linkEvStrings [] = {
    "paired-ends",
    "align genus",
    "align xgenus",
    "align trnscpt",
    "within clone",
    "clone contig",
    "map",
    "strobe",
    "unspecified",
    "pcr",
    "other",
    "UNKNOWN VALUE"
};

// Assumes seq is a delta sequence
void CValidError_bioseq::ValidateDelta(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    bool is_circular = false;
    if (inst.GetTopology() == CSeq_inst::eTopology_circular) {
      is_circular = true;
    }

    // Get CMolInfo and tech used for validating technique and gap positioning
    const CMolInfo* mi = 0;
    CSeqdesc_CI mi_desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Molinfo);
    if ( mi_desc ) {
        mi = &(mi_desc->GetMolinfo());
    }
    CMolInfo::TTech tech = 
        mi != 0 ? mi->GetTech() : CMolInfo::eTech_unknown;


    if (!inst.IsSetExt()  ||  !inst.GetExt().IsDelta()  ||
        inst.GetExt().GetDelta().Get().empty()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No CDelta_ext data for delta Bioseq", seq);
    }

    EDiagSev sev = eDiag_Error;
    if ( tech != CMolInfo::eTech_htgs_0  &&
         tech != CMolInfo::eTech_htgs_1  &&
         tech != CMolInfo::eTech_htgs_2  &&
         tech != CMolInfo::eTech_htgs_3  ) {
        sev = eDiag_Warning;
    }

    bool any_tech_ok = false;
    bool has_gi = false;
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()) {
            const string& accession = (*id_it)->GetOther().GetAccession();
            if (NStr::StartsWith(accession, "NT_")
                || NStr::StartsWith(accession, "NC_")) {
                any_tech_ok = true;
                break;
            }
        } else if ((*id_it)->IsGi()) {
            has_gi = true;
        }
    }
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if (!any_tech_ok) {
        if (GetGenProdSetParent (bsh)) {
            any_tech_ok = true;
        }
    }
    if (!any_tech_ok && seq.IsNa()
        && tech != CMolInfo::eTech_htgs_0 && tech != CMolInfo::eTech_htgs_1 
        && tech != CMolInfo::eTech_htgs_2 && tech != CMolInfo::eTech_htgs_3 
        && tech != CMolInfo::eTech_wgs && tech != CMolInfo::eTech_composite_wgs_htgs 
        && tech != CMolInfo::eTech_unknown && tech != CMolInfo::eTech_standard
        && tech != CMolInfo::eTech_htc && tech != CMolInfo::eTech_barcode 
        && tech != CMolInfo::eTech_tsa) {
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq, 
            "Delta seq technique should not be [" + NStr::IntToString(tech) + "]", seq);
    }

   // set severity for first / last gap error
    TSeqPos len = 0;
    TSeqPos seg = 0;
    bool last_is_gap = false;
    size_t num_gaps = 0;
    size_t num_adjacent_gaps = 0;
    bool non_interspersed_gaps = false;
    bool first = true;

    vector<CConstRef<CSeq_loc> > delta_locs;

    ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
        ++seg;
        if ( !(*sg) ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                "NULL pointer in delta seq_ext valnode (segment " +
                NStr::IntToString(seg) + ")", seq);
            continue;
        }
        switch ( (**sg).Which() ) {
        case CDelta_seq::e_Loc:
        {
            const CSeq_loc& loc = (**sg).GetLoc(); 
            CConstRef<CSeq_loc> tmp(&loc);
            delta_locs.push_back (tmp);
            
            ValidateDeltaLoc (loc, seq, len);

            if ( !last_is_gap && !first) {
                non_interspersed_gaps = true;
            }
            last_is_gap = false;
            first = false;
            break;
        }
        case CDelta_seq::e_Literal:
        {
            // The C toolkit code checks for valid alphabet here
            // The C++ object serializaton will not load if invalid alphabet
            // so no check needed here
            const CSeq_literal& lit = (*sg)->GetLiteral();
            TSeqPos start_len = len;
            len += lit.CanGetLength() ? lit.GetLength() : 0;
            if (lit.IsSetSeq_data() && ! lit.GetSeq_data().IsGap()
                && (!lit.IsSetLength() || lit.GetLength() == 0)) {
                PostErr (eDiag_Error, eErr_SEQ_INST_SeqLitDataLength0, 
                         "Seq-lit of length 0 in delta chain", seq);
            }

            // Check for invalid residues
            if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap() ) {
                if ( !last_is_gap && !first) {
                    non_interspersed_gaps = true;
                }
                last_is_gap = false;
                const CSeq_data& data = lit.GetSeq_data();
                vector<TSeqPos> badIdx;
                CSeqportUtil::Validate(data, &badIdx);
                const string* ss = 0;
                switch (data.Which()) {
                case CSeq_data::e_Iupacaa:
                    ss = &data.GetIupacaa().Get();
                    break;
                case CSeq_data::e_Iupacna:
                    ss = &data.GetIupacna().Get();
                    break;
                case CSeq_data::e_Ncbieaa:
                    ss = &data.GetNcbieaa().Get();
                    break;
                case CSeq_data::e_Ncbistdaa:
                    {
                        const vector<char>& c = data.GetNcbistdaa().Get();
                        ITERATE (vector<TSeqPos>, ci, badIdx) {
                            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                "Invalid residue [" +
                                NStr::IntToString((int)c[*ci]) + "] at position [" +
                                NStr::IntToString((*ci) + 1) + "]", seq);
                        }
                        break;
                    }
                default:
                    break;
                }

                if ( ss ) {
                    ITERATE (vector<TSeqPos>, it, badIdx) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            ss->substr(*it, 1) + "] at position [" +
                            NStr::IntToString((*it) + 1) + "]", seq);
                    }
                }
                            
                if (mi) {
                    // Count adjacent Ns in Seq-lit
                    int max_ns = s_MaxNsInSeqLitForTech (tech);
                    int adjacent_ns = x_CountAdjacentNs(lit);
                    if (max_ns > -1 && adjacent_ns > max_ns) {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_InternalNsInSeqLit,
                                "Run of " + NStr::UIntToString(adjacent_ns) + 
                                " Ns in delta component " + NStr::UIntToString(seg) +
                                " that starts at base " + NStr::UIntToString(start_len + 1),
                                seq);
                    }
                }
            } else {
                if ( lit.IsSetSeq_data() && lit.GetSeq_data().IsGap() ) {
                    const CSeq_data& data = lit.GetSeq_data();
                    if (data.Which() == CSeq_data::e_Gap) {
                        const CSeq_gap& gap = data.GetGap();
                        if (gap.IsSetLinkage_evidence()) {
                            int linkcount = 0;
                            int linkevarray [12];
                            for (int i = 0; i < 12; i++) {
                              linkevarray [i] = 0;
                            }
                            ITERATE( CSeq_gap::TLinkage_evidence, ev_itr, gap.GetLinkage_evidence() ) {
                                const CLinkage_evidence & evidence = **ev_itr;
                                if (! evidence.CanGetType() ) continue;
                                int linktype = evidence.GetType();
                                linkcount++;
                                if (linktype == 255) {
                                    (linkevarray [10])++;
                                } else if (linktype < 0 || linktype > 9) {
                                    (linkevarray [11])++;
                                } else {
                                    (linkevarray [linktype])++;
                                }
                            }
                            if (linkevarray [8] > 0 && linkcount > linkevarray [8]) {
                                PostErr(eDiag_Error, eErr_SEQ_INST_SeqGapProblem,
                                    "Seq-gap type has unspecified and additional linkage evidence", seq);
                            }
                            for (int i = 0; i < 12; i++) {
                                if (linkevarray [i] > 1) {
                                    PostErr(eDiag_Error, eErr_SEQ_INST_SeqGapProblem,
                                        "Linkage evidence " + linkEvStrings [i] + " appears " +
                                        NStr::IntToString(linkevarray [i]) + " times", seq);
                                }
                            }
                            if (! gap.IsSetLinkage() || gap.GetLinkage() != CSeq_gap::eLinkage_linked) {
                                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                                    "Seq-gap with linkage evidence must have linkage field set to linked", seq);
                            }
                            if (gap.IsSetType()) {
                                int gaptype = gap.GetType();
                                if (gaptype == CSeq_gap::eType_short_arm ||
                                    gaptype == CSeq_gap::eType_heterochromatin ||
                                    gaptype == CSeq_gap::eType_centromere ||
                                    gaptype == CSeq_gap::eType_telomere ||
                                    gaptype == CSeq_gap::eType_contig ||
                                    gaptype == CSeq_gap::eType_other) {
                                    PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                                        "Seq-gap of type " + NStr::IntToString(gaptype) +
                                        "should not have linkage evidence", seq);
                                }
                            }
                        } else {
                            if (gap.IsSetType()) {
                                int gaptype = gap.GetType();
                                if (gaptype == CSeq_gap::eType_scaffold) {
                                    PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                                        "Seq-gap type == scaffold is missing required linkage evidence", seq);
                                }
                                if (gaptype == CSeq_gap::eType_repeat && gap.IsSetLinkage() && gap.GetLinkage() == CSeq_gap::eLinkage_linked) 
                                {
                                    bool suppress_SEQ_INST_SeqGapProblem = false;
                                    if (has_gi && seq.IsSetDescr()) 
                                    {
                                        ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get())
                                        {
                                            if ((**it).IsCreate_date())
                                            {
                                                CDate threshold_date(CTime(2012, 10, 1));
                                                if ((**it).GetCreate_date().Compare(threshold_date) == CDate::eCompare_before)
                                                    suppress_SEQ_INST_SeqGapProblem = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (!suppress_SEQ_INST_SeqGapProblem)                                  
                                       PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                                          "Seq-gap type == repeat and linkage == linked is missing required linkage evidence", seq);
                                    
                                }
                            }
                        }
                    }
                }
                if ( first ) {
                    sev = eDiag_Error;
                    if (tech == CMolInfo::eTech_htgs_0 || tech == CMolInfo::eTech_htgs_1
                        || tech == CMolInfo::eTech_htgs_2 || tech == CMolInfo::eTech_htgs_3) {
                        sev = eDiag_Warning;
                    }
                    if (! is_circular) {
                        PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
                            "First delta seq component is a gap", seq);
                    }
                }
                if ( last_is_gap ) {
                    ++num_adjacent_gaps;
                }
                if ( !lit.CanGetLength()  ||  lit.GetLength() == 0 ) {
                    if (!lit.IsSetFuzz() || !lit.GetFuzz().IsLim() || lit.GetFuzz().GetLim() != CInt_fuzz::eLim_unk) {
                        PostErr(s_IsSwissProt(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_SeqLitGapLength0,
                            "Gap of length 0 in delta chain", seq);
                    } else {
                        PostErr(s_IsSwissProt(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_SeqLitGapLength0,
                            "Gap of length 0 with unknown fuzz in delta chain", seq);
                    }
                } else if (lit.CanGetLength() && lit.GetLength() != 100) {
                    if (lit.IsSetFuzz()) {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLitGapFuzzNot100,
                            "Gap of unknown length should have length 100", seq);
                    }
                }
                last_is_gap = true;
                ++num_gaps;
            }
            first = false;
            break;
        }
        default:
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed,
                "CDelta_seq::Which() is e_not_set", seq);
        }
    }
    if (inst.GetLength() > len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data too short [" + NStr::IntToString(len) +
            "] for given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    } else if (inst.GetLength() < len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data is larger [" + NStr::IntToString(len) +
            "] than given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    }
    if ( non_interspersed_gaps  &&  !has_gi && mi  &&
        (tech == CMolInfo::eTech_htgs_0  ||  tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2) ) {
        EDiagSev missing_gaps_sev = eDiag_Error;
        CSeqdesc_CI desc_i(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_User);
        while (desc_i) {
            if (IsRefGeneTrackingObject (desc_i->GetUser())) {
                missing_gaps_sev = eDiag_Info;
                break;
            }
            ++desc_i;
        }

        PostErr(missing_gaps_sev, eErr_SEQ_INST_MissingGaps,
            "HTGS delta seq should have gaps between all sequence runs", seq);
    }
    if ( num_adjacent_gaps >= 1 ) {
        string msg = (num_adjacent_gaps == 1) ?
            "There is 1 adjacent gap in delta seq" :
            "There are " + NStr::SizetToString(num_adjacent_gaps) +
            " adjacent gaps in delta seq";
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq, msg, seq);
    }
    if ( last_is_gap ) {
        sev = eDiag_Error;
        if (tech == CMolInfo::eTech_htgs_0 || tech == CMolInfo::eTech_htgs_1
            || tech == CMolInfo::eTech_htgs_2 || tech == CMolInfo::eTech_htgs_3) {
            sev = eDiag_Warning;
        }
        if (! is_circular) {
            PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
                "Last delta seq component is a gap", seq);
        }
    }
    
    // Validate technique
    if (num_gaps == 0  &&  mi) {
        if ( tech == CMolInfo::eTech_htgs_2  &&
             !GraphsOnBioseq(seq)  &&
             !x_IsActiveFin(seq) ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadHTGSeq,
                "HTGS 2 delta seq has no gaps and no graphs", seq);
        }
    }

    // look for multiple delta locs overlapping
    if (delta_locs.size() > 1) {
        stable_sort (delta_locs.begin(), delta_locs.end(), s_LocSortCompare);
        vector<CConstRef<CSeq_loc> >::iterator it1 = delta_locs.begin();
        vector<CConstRef<CSeq_loc> >::iterator it2 = it1;
        ++it2;
        while (it2 != delta_locs.end()) {
            if ((*it1)->GetId()->Compare(*(*it2)->GetId()) == CSeq_id::e_YES
                && Compare (**it1, **it2, m_Scope) != eNoOverlap) {
                string seq_label;
                (*it1)->GetId()->GetLabel(&seq_label);
                PostErr (eDiag_Warning, eErr_SEQ_INST_OverlappingDeltaRange,
                         "Overlapping delta range " + NStr::IntToString((*it2)->GetStart(eExtreme_Positional) + 1)
                         + "-" + NStr::IntToString((*it2)->GetStop(eExtreme_Positional) + 1)
                         + " and " + NStr::IntToString((*it1)->GetStart(eExtreme_Positional) + 1)
                         + "-" + NStr::IntToString((*it1)->GetStop(eExtreme_Positional) + 1)
                         + " on a Bioseq " + seq_label,
                         seq);
            }
            ++it1;
            ++it2;
        }
    }

    // look for Ns next to gaps 
    if (seq.IsNa() && seq.GetLength() > 1) {
        try {
            TSeqPos pos = 0;
            CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            ITERATE (CDelta_ext::Tdata, delta_i, seq.GetInst().GetExt().GetDelta().Get()) {
                if (delta_i->Empty()) {
                    continue; // Ignore NULLs, reported separately above.
                }
                size_t delta_len = s_GetDeltaLen (**delta_i, m_Scope);
                if (sv.IsInGap (pos)) {
                    if (pos > 0) {
                        CSeqVector_CI sv_iter(sv);
                        sv_iter += pos - 1;
                        CSeqVector::TResidue res = *sv_iter;
                        if (res == 'N') {
                            PostErr (eDiag_Error, eErr_SEQ_INST_InternalNsAdjacentToGap,
                                     "Ambiguous residue N is adjacent to a gap around position " + NStr::SizetToString (pos + 1),
                                     seq);
                        }
                    } 
                    if (pos + delta_len < seq.GetLength()) {
                        CSeqVector_CI sv_iter(sv);
                        sv_iter += pos + delta_len;
                        CSeqVector::TResidue res = *sv_iter;
                        if (res == 'N') {
                            PostErr (eDiag_Error, eErr_SEQ_INST_InternalNsAdjacentToGap,
                                     "Ambiguous residue N is adjacent to a gap around position " + NStr::SizetToString (pos + delta_len + 1),
                                     seq);
                        }
                    }
                }
                pos += delta_len;
            }                        
        } catch (CException ) {
        } catch (std::exception ) {
        }
    }

}


bool CValidError_bioseq::ValidateRepr
(const CSeq_inst& inst,
 const CBioseq&   seq)
{
    bool rtn = true;
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    string rpr = tv->FindName(inst.GetRepr(), true);
    if (NStr::Equal(rpr, "ref")) {
        rpr = "reference";
    } else if (NStr::Equal(rpr, "const")) {
        rpr = "constructed";
    }
    const string err0 = "Bioseq-ext not allowed on " + rpr + " Bioseq";
    const string err1 = "Missing or incorrect Bioseq-ext on " + rpr + " Bioseq";
    const string err2 = "Missing Seq-data on " + rpr + " Bioseq";
    const string err3 = "Seq-data not allowed on " + rpr + " Bioseq";
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_virtual:
        if (inst.IsSetExt()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_map:
        if (!inst.IsSetExt() || !inst.GetExt().IsMap()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_ref:
        if (!inst.IsSetExt() || !inst.GetExt().IsRef() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_seg:
        if (!inst.IsSetExt() || !inst.GetExt().IsSeg() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_raw:
    case CSeq_inst::eRepr_const:
        if (inst.IsSetExt()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (!inst.IsSetSeq_data() ||
          inst.GetSeq_data().Which() == CSeq_data::e_not_set
          || inst.GetSeq_data().Which() == CSeq_data::e_Gap)
        {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataNotFound, err2, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_delta:
        if (!inst.IsSetExt() || !inst.GetExt().IsDelta() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    default:
        PostErr(
            eDiag_Critical, eErr_SEQ_INST_ReprInvalid,
            "Invalid Bioseq->repr = " +
            NStr::IntToString(static_cast<int>(inst.GetRepr())), seq);
        rtn = false;
    }
    return rtn;
}


void CValidError_bioseq::CheckSoureDescriptor(const CBioseq_Handle& bsh)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if (!di) {
        // add to list of sources with no descriptor later to be reported
        m_Imp.AddBioseqWithNoBiosource(*(bsh.GetBioseqCore()));
        return;
    }
    _ASSERT(di);

    if (m_Imp.IsTransgenic(di->GetSource())  &&
        CSeq_inst::IsNa(bsh.GetInst_Mol())) {
        if (!CFeat_CI(bsh, CSeqFeatData::e_Biosrc)) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_TransgenicProblem,
                "Transgenic source descriptor requires presence of source feature",
                *(bsh.GetBioseqCore()));
        }
    }
}


void CValidError_bioseq::CheckForMolinfoOnBioseq(const CBioseq& seq)
{
    CSeqdesc_CI sd(m_CurrentHandle, CSeqdesc::e_Molinfo);

    if ( !sd ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoMolInfoFound, 
            "No Mol-info applies to this Bioseq",
            seq);
    }
}


void CValidError_bioseq::CheckForPubOnBioseq(const CBioseq& seq)
{
    string label = "";
    seq.GetLabel(&label, CBioseq::eBoth);

    if ( !m_CurrentHandle ) {
        return;
    }

    if ( !CSeqdesc_CI( m_CurrentHandle, CSeqdesc::e_Pub)) {
        // look for pub or feat with cit
        if (m_AllFeatIt) {
            (*m_AllFeatIt).Rewind();
            while ((*m_AllFeatIt)) {
                if ((*m_AllFeatIt)->IsSetCit() || (*m_AllFeatIt)->GetData().IsPub()) {
                    return;
                }
                ++(*m_AllFeatIt);
            }
        }

        m_Imp.AddBioseqWithNoPub(seq);
    }
}


static bool s_LocIntervalsSpanOrigin (const CSeq_loc& loc, CBioseq_Handle bsh)
{
    CSeq_loc_CI si(loc);
    if (!si) {
        return false;
    }
    if(loc.GetStrand() == eNa_strand_minus) {
        if (si.GetRange().GetFrom() != 0) {
            return false;
        }
        ++si;
        if (!si || si.GetRange().GetTo() != bsh.GetBioseqLength() - 1) {
            return false;
        }
        ++si;
    } else {
        if (si.GetRange().GetTo() != bsh.GetBioseqLength() - 1) {
            return false;
        }
        ++si;
        if (!si || si.GetRange().GetFrom() != 0) {
            return false;
        }        
        ++si;
    }
    if (si) {
        return false;
    } else {
        return true;
    }
}


static bool s_LocIntervalsCoverSegs (const CSeq_loc& loc)
{
    if (loc.GetStrand() == eNa_strand_minus) {
        int start = loc.GetTotalRange().GetTo();
        int stop = loc.GetTotalRange().GetFrom();
        CSeq_loc_CI si(loc);
        while (si) {
            if (si.GetRange().GetTo() != start) {
                return false;
            }
            start = si.GetRange().GetFrom() - 1;
            ++si;
        }
        if (start != stop - 1) {
            return false;
        }
    } else {
        int start = loc.GetTotalRange().GetFrom();
        int stop = loc.GetTotalRange().GetTo();
        CSeq_loc_CI si(loc);
        while (si) {
            if (si.GetRange().GetFrom() != start) {
                return false;
            }
            start = si.GetRange().GetTo() + 1;
            ++si;
        }
        if (start != stop + 1) {
            return false;
        }
    }
    return true;
}


void CValidError_bioseq::ValidateMultiIntervalGene(const CBioseq& seq)
{
    try {
        if (!m_GeneIt) {
            return;
        }
        
        m_GeneIt->Rewind();
        CFeat_CI fi = *m_GeneIt;
        for ( ; fi; ++fi ) {
            const CSeq_loc& loc = fi->GetOriginalFeature().GetLocation();
            CSeq_loc_CI si(loc);
            if ( !(++si) ) {  // if only a single interval
                continue;
            }

            if (fi->IsSetExcept() && fi->IsSetExcept_text()
                && NStr::FindNoCase (fi->GetExcept_text(), "trans-splicing") != string::npos) {
                //ignore - has exception
                continue;
            }

            const CSeq_id* loc_id = loc.GetId();
            const CSeq_id* seq_first_id = seq.GetFirstId();
            if ( !IsOneBioseq(loc, m_Scope) ) {
                if (!seq.IsSetInst() 
                    || !seq.GetInst().IsSetRepr() 
                    || seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                    continue;
                }
                // segmented set - should cover all nucleotides in interval
                CSeq_loc_Mapper mapper (m_CurrentHandle, CSeq_loc_Mapper::eSeqMap_Up);
                CConstRef<CSeq_loc> mapped_loc = mapper.Map(fi->GetLocation());
                if (mapped_loc) {
                    if (s_LocIntervalsCoverSegs(*mapped_loc)) {
                        // covers all the nucleotides
                    } else if (seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular
                            && s_LocIntervalsSpanOrigin (*mapped_loc, m_CurrentHandle)) {
                        // circular and spans the origin, can ignore
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SegmentedGeneProblem,
                                 "Gene feature on segmented sequence should cover all bases within its extremes",
                                 fi->GetOriginalFeature());
                    }                    
                }
            } else if (loc_id && seq_first_id && !IsSameBioseq (*loc_id, *seq_first_id, m_Scope)) {
                // on segment in segmented set, only report once
                continue;
            } else if ( seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular
                 && s_LocIntervalsSpanOrigin (loc, m_CurrentHandle)) {            
                // spans origin
                continue;
            } else if (m_Imp.IsSmallGenomeSet()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultiIntervalGene,
                    "Multiple interval gene feature in small genome set - "
                    "set trans-splicing exception if appropriate", fi->GetOriginalFeature());
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultiIntervalGene,
                    "Gene feature on non-segmented sequence should not "
                    "have multiple intervals", fi->GetOriginalFeature());
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating multi-interval genes. EXCEPTION: ") +
                e.what(), seq);
        }
    }
}


void CValidError_bioseq::x_ValidateCompletness
(const CBioseq& seq,
 const CMolInfo& mi)
{
    if ( !mi.IsSetCompleteness() ) {
        return;
    }
    if ( !seq.IsNa() ) {
        return;
    }

    CMolInfo::TCompleteness comp = mi.GetCompleteness();
    CMolInfo::TBiomol biomol = mi.IsSetBiomol() ? 
        mi.GetBiomol() : CMolInfo::eBiomol_unknown;
    EDiagSev sev = mi.GetTech() == CMolInfo::eTech_htgs_3 ? 
        eDiag_Warning : eDiag_Error;

    bool reported = false;

    if ( comp == CMolInfo::eCompleteness_complete ) {
        if ( biomol == CMolInfo::eBiomol_genomic ) {
            bool is_gb = false;
            FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
                if ( (*it)->IsGenbank() ) {
                    is_gb = true;
                    break;
                }
            }

            if ( is_gb ) {
                CSeqdesc_CI desc(m_CurrentHandle, CSeqdesc::e_Title);
                if ( desc ) {
                    const string& title = desc->GetTitle();
                    if (!NStr::IsBlank(title)
                        && NStr::FindNoCase(title, "complete sequence") == string::npos
                        && NStr::FindNoCase(title, "complete genome") == string::npos) {
                        if (seq.IsSetInst() && seq.GetInst().IsSetTopology()
                            && seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular) {
                            /*
                            const CSeq_entry& ctx = *seq.GetParentEntry();
                            PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteCircleProblem,
                                    "Circular topology has complete flag set, but title should say complete sequence or complete genome",
                                    ctx, *desc);
                            */
                        } else {
                            PostErr(sev, eErr_SEQ_DESCR_UnwantedCompleteFlag,
                                    "Suspicious use of complete", seq);
                            reported = true;
                        }
                    }
                }
            }
        }

        if (!reported) {
            // for SQD-1484
            // warn if completeness = complete, organism not viral, no location set or location is genomic
            CSeqdesc_CI src_desc(m_CurrentHandle, CSeqdesc::e_Source);
            if (src_desc) {
                const CBioSource& biosrc = src_desc->GetSource();
                if ((!biosrc.IsSetLineage() 
                     || (NStr::FindNoCase(biosrc.GetLineage(), "Viruses") == string::npos
                         && NStr::FindNoCase(biosrc.GetLineage(), "Viroids") == string::npos)) // not viral
                    && (!src_desc->GetSource().IsSetGenome()
                        || src_desc->GetSource().GetGenome() == CBioSource::eGenome_genomic)) { // location not set or genomic
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_UnwantedCompleteFlag,
                                        "Suspicious use of complete", seq);
                }
            }
        }
    }
}


static bool s_StandaloneProt(const CBioseq_Handle& bsh)
{
    // proteins are never standalone within the context of a Genbank / Refseq
    // record.

    CSeq_entry_Handle eh = bsh.GetSeq_entry_Handle();
    while ( eh ) {
        if ( eh.IsSet() ) {
            CBioseq_set_Handle bsh = eh.GetSet();
            if ( bsh.IsSetClass() ) {
                CBioseq_set::TClass cls = bsh.GetClass();
                switch ( cls ) {
                case CBioseq_set::eClass_nuc_prot:
                case CBioseq_set::eClass_mut_set:
                case CBioseq_set::eClass_pop_set:
                case CBioseq_set::eClass_phy_set:
                case CBioseq_set::eClass_eco_set:
                case CBioseq_set::eClass_gen_prod_set:
                    return false;
                default:
                    break;
                }
            }
        }
        eh = eh.GetParentEntry();
    }

    return true;
}


static CBioseq_Handle s_GetParent(const CBioseq_Handle& part)
{
    CBioseq_Handle parent;

    if ( part ) {
        CSeq_entry_Handle segset = 
            part.GetExactComplexityLevel(CBioseq_set::eClass_segset);
        if ( segset ) {
            for ( CSeq_entry_CI it(segset); it; ++it ) {
                if ( it->IsSeq()  &&  it->GetSeq().IsSetInst_Repr()  &&
                    it->GetSeq().GetInst_Repr() == CSeq_inst::eRepr_seg ) {
                    parent = it->GetSeq();
                    break;
                }
            }
        }
    }
    return parent;
}


static bool s_SeqIdCompare (const CConstRef<CSeq_id>& q1, const CConstRef<CSeq_id>& q2)
{
    // is q1 < q2
    return (q1->CompareOrdered(*q2) < 0);
}


static bool s_SeqIdMatch (const CConstRef<CSeq_id>& q1, const CConstRef<CSeq_id>& q2)
{
    // is q1 == q2
    return (q1->CompareOrdered(*q2) == 0);
}


void CValidError_bioseq::ValidateMultipleGeneOverlap (const CBioseq_Handle& bsh)
{
    if (!m_GeneIt) {
        return;
    }
    /*
    bool is_circular = bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular;
    */
    try {
        vector< CConstRef < CSeq_feat > > containing_genes;
        vector< int > num_contained;
        m_GeneIt->Rewind();
        CFeat_CI fi = *m_GeneIt;
        for ( ; fi; ++fi ) {
            TSeqPos left = fi->GetLocation().GetStart(eExtreme_Positional);
            vector< CConstRef < CSeq_feat > >::iterator cit = containing_genes.begin();
            vector< int >::iterator nit = num_contained.begin();
            bool go_on = true;
            while (go_on && cit != containing_genes.end() && nit != num_contained.end()) {
                ECompare comp = Compare(fi->GetLocation(), (*cit)->GetLocation(), m_Scope);
                if (comp == eContained  ||  comp == eSame) {
                    (*nit)++;
                }
                if (/* !is_circular && */ (*cit)->GetLocation().GetStop(eExtreme_Positional) < left) {
                    // report if necessary
                    if (*nit > 4) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleGeneOverlap, 
                                 "Gene contains " + NStr::IntToString (*nit) + " other genes",
                                 **cit);
                    }
                    // remove from list
                    cit = containing_genes.erase(cit);
                    nit = num_contained.erase(nit);
                    go_on = false;
                } else {
                    ++cit;
                    ++nit;
                }
            }

            const CSeq_feat& ft = fi->GetOriginalFeature();
            const CSeq_feat* p = &ft;
            CConstRef<CSeq_feat> ref(p);
            containing_genes.push_back (ref);
            num_contained.push_back (0);
        }

        vector< CConstRef < CSeq_feat > >::iterator cit = containing_genes.begin();
        vector< int >::iterator nit = num_contained.begin();
        while (cit != containing_genes.end() && nit != num_contained.end()) {
            if (*nit > 4) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleGeneOverlap, 
                         "Gene contains " + NStr::IntToString (*nit) + " other genes",
                         **cit);
            }
            ++cit;
            ++nit;
        }
    } catch ( const exception& e ) {
        PostErr(eDiag_Error, eErr_INTERNAL_Exception,
            string("Exeption while validating bioseq MultipleGeneOverlap. EXCEPTION: ") +
            e.what(), *(bsh.GetCompleteBioseq()));
    }
}


void CValidError_bioseq::ValidateBadGeneOverlap(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp != 0 ) {
        return;
    }

    const CSeq_loc& loc = feat.GetLocation();

    CConstRef<CSeq_feat> gene = GetOverlappingGene(loc, *m_Scope);
    if (gene) return;

    gene = GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_gene, eOverlap_Simple, *m_Scope);
    if (! gene) return;

    /*
    if (!m_GeneIt) {
        return;
    }

    m_GeneIt->Rewind();
    CFeat_CI feat_it = *m_GeneIt;
    bool has_containing_gene = false;

    while (feat_it) {
        if (TestForOverlapEx (feat_it->GetLocation(), feat.GetLocation(), eOverlap_Contained, m_Scope) >= 0) {
            has_containing_gene = true;
            break;
        }
        ++feat_it;
    }

    if (has_containing_gene) {
        return;
    }

    feat_it.Rewind();
    bool has_simple_overlap = false;
    while (feat_it) {
        if (TestForOverlapEx (feat_it->GetLocation(), feat.GetLocation(), eOverlap_Simple, m_Scope) >= 0) {
            has_simple_overlap = true;
            break;
        }
        ++feat_it;
    }
    if (!has_simple_overlap) {
        return;
    }
    */

    // found an intersecting (but not overlapping) gene
    // set severity level
    EDiagSev sev = eDiag_Warning;

    // report error
    if (feat.GetData().IsCdregion()) {
        PostErr(sev, eErr_SEQ_FEAT_CDSgeneRange, 
            "gene overlaps CDS but does not completely contain it", feat);
    } else if (feat.GetData().IsRna()) {
        if (GetOverlappingOperon(feat.GetLocation(), *m_Scope)) {
            return;
        }
        PostErr(sev, eErr_SEQ_FEAT_mRNAgeneRange,
            "gene overlaps mRNA but does not completely contain it", feat);
    }
}


bool s_IsCDDFeat(const CMappedFeat& feat)
{
    if ( feat.GetData().IsRegion() ) {
        FOR_EACH_DBXREF_ON_FEATURE (db, feat) {
            if ( (*db)->CanGetDb()  &&
                NStr::Compare((*db)->GetDb(), "CDD") == 0 ) {
                return true;
            }
        }
    }
    return false;
}


bool CValidError_bioseq::x_IsPartialAtSpliceSiteOrGap
(const CSeq_loc& loc,
 unsigned int tag,
 bool& bad_seq,
 bool& is_gap)
{
    bad_seq = false;
    is_gap = false;
    if ( tag != eSeqlocPartial_Nostart && tag != eSeqlocPartial_Nostop ) {
        return false;
    }

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) { // EQUIV_IS_ONE not supported
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }

    if ( first.GetStrand() != last.GetStrand() ) {
        return false;
    }
    CSeq_loc_CI temp = (tag == eSeqlocPartial_Nostart) ? first : last;

    if (!m_Scope) {
        return false;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, *temp.GetRangeAsSeq_loc());
    if (!bsh) {
        return false;
    }
    
    TSeqPos acceptor = temp.GetRange().GetFrom();
    TSeqPos donor = temp.GetRange().GetTo();
    TSeqPos start = acceptor;
    TSeqPos stop = donor;

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
        temp.GetStrand());
    TSeqPos len = vec.size();

    if ( temp.GetStrand() == eNa_strand_minus ) {
        swap(acceptor, donor);
        stop = len - donor - 1;
        start = len - acceptor - 1;
    }

    bool result = false;

    try {
        if (tag == eSeqlocPartial_Nostop && stop < len - 1 && vec.IsInGap(stop + 1)) {
            is_gap = true;
            return true;
        } else if (tag == eSeqlocPartial_Nostart && start > 0 && vec.IsInGap(start - 1)) {
            is_gap = true;
            return true;
        }
    } catch ( exception& ) {
        
        return false;
    }

    if ( (tag == eSeqlocPartial_Nostop)  &&  (stop < len - 2) ) {
        try {
            CSeqVector::TResidue res1 = vec[stop + 1];
            CSeqVector::TResidue res2 = vec[stop + 2];

            if ( IsResidue(res1)  &&  IsResidue(res2) && isalpha (res1) && isalpha (res2)) {
                if ( (res1 == 'G'  &&  res2 == 'T')  || 
                    (res1 == 'G'  &&  res2 == 'C') ) {
                    result = true;
                }
            } else {
                bad_seq = true;
            }
        } catch ( exception& ) {
            return false;
        }
    } else if ( (tag == eSeqlocPartial_Nostart)  &&  (start > 1) ) {
        try {
            CSeqVector::TResidue res1 = vec[start - 2];    
            CSeqVector::TResidue res2 = vec[start - 1];
        
            if ( IsResidue(res1)  &&  IsResidue(res2) && isalpha (res1) && isalpha (res2)) {
                if ( (res1 == 'A')  &&  (res2 == 'G') ) { 
                    result = true;
                }
            } else {
                bad_seq = true;
            }
        } catch ( exception& ) {
            return false;
        }
    }

    return result;    
}


bool CValidError_bioseq::x_SplicingNotExpected(const CMappedFeat& feat)
{
    if (!m_CurrentHandle) {
        return false;
    }

    CSeqdesc_CI diter (m_CurrentHandle, CSeqdesc::e_Source);
    if (!diter) {
        return false;
    }
    if (!diter->GetSource().IsSetOrg() || !diter->GetSource().GetOrg().IsSetOrgname()) {
        return false;
    }

    if (diter->GetSource().GetOrg().GetOrgname().IsSetDiv()) {
        string div = diter->GetSource().GetOrg().GetOrgname().GetDiv();
        if (NStr::Equal(div, "BCT") || NStr::Equal(div, "VRL")) {
            return true;
        }
    }
    if (diter->GetSource().GetOrg().GetOrgname().IsSetLineage()) {
        string lineage = diter->GetSource().GetOrg().GetOrgname().GetLineage();
        if (NStr::StartsWith(lineage, "Bacteria; ", NStr::eNocase)
            || NStr::StartsWith(lineage, "Archaea; ", NStr::eNocase)) {
            return true;
        }
    }
    return false;
}


static bool s_MatchPartialType (const CSeq_loc& loc1, const CSeq_loc& loc2, unsigned int partial_type)
{
    bool rval = false;

    switch (partial_type) {
        case eSeqlocPartial_Nostart:
            if (loc1.GetStart(eExtreme_Biological) == loc2.GetStart(eExtreme_Biological)) {
                rval = true;
            }
            break;
        case eSeqlocPartial_Nostop:
            if (loc1.GetStop(eExtreme_Biological) == loc2.GetStop(eExtreme_Biological)) {
                rval = true;
            }
            break;
        default:
            rval = false;
            break;
    }
    return rval;
}


// REQUIRES: feature is either Gene or mRNA
bool CValidError_bioseq::x_IsSameAsCDS(const CMappedFeat& feat)
{
    EOverlapType overlap_type;
    if ( feat.GetData().IsGene() ) {
        overlap_type = eOverlap_Simple;
    } else if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
        overlap_type = eOverlap_CheckIntervals;
    } else {
        return false;
    }

    CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
            feat.GetLocation(),
            CSeqFeatData::e_Cdregion,
            overlap_type,
            *m_Scope);

        if ( cds ) {
            if ( TestForOverlapEx(
                    cds->GetLocation(),
                    feat.GetLocation(),
                    eOverlap_Simple) == 0 ) {
                return true;
            }
        }
    return false;
}


bool CValidError_bioseq::x_MatchesOverlappingFeaturePartial (const CMappedFeat& feat, unsigned int partial_type)
{
    bool rval = false;


    if (feat.GetData().IsGene()) {
        TSeqPos gene_start = feat.GetLocation().GetStart(eExtreme_Biological);
        TSeqPos gene_stop = feat.GetLocation().GetStop(eExtreme_Biological);

        // gene is ok if its partialness matches the overlapping coding region or mRNA
        TFeatScores mRNAs;
        GetOverlappingFeatures (feat.GetLocation(), CSeqFeatData::e_Rna,
            CSeqFeatData::eSubtype_mRNA, eOverlap_Contained, mRNAs, *m_Scope);
        ITERATE (TFeatScores, s, mRNAs) {
            const CSeq_loc& mrna_loc = s->second->GetLocation();
            if (mrna_loc.GetStart(eExtreme_Biological) == gene_start
                && mrna_loc.GetStop(eExtreme_Biological) == gene_stop
                && s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)) {
                rval = true;
                break;
            }
        }
        if (!rval) {
            TFeatScores cds;
            GetOverlappingFeatures (feat.GetLocation(), CSeqFeatData::e_Cdregion,
                CSeqFeatData::eSubtype_cdregion, eOverlap_Contained, cds, *m_Scope);
            ITERATE (TFeatScores, s, cds) {
                const CSeq_loc& cds_loc = s->second->GetLocation();
                if (cds_loc.GetStart(eExtreme_Biological) == gene_start
                    && cds_loc.GetStop(eExtreme_Biological) == gene_stop
                    && s_MatchPartialType(feat.GetLocation(), cds_loc, partial_type)) {
                    rval = true;
                    break;
                }
            }
        }
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        bool look_for_gene = true;
        TSeqPos mrna_start = feat.GetLocation().GetStart(eExtreme_Biological);
        TSeqPos mrna_stop = feat.GetLocation().GetStop(eExtreme_Biological);
        CMatchmRNA * match = m_mRNACDSIndex.FindMatchmRNA(feat);
        if (match) {
          if (match->HasCDSMatch()) {
              look_for_gene = false;
          }
          if (match->MatchesUnderlyingCDS(partial_type)) {
            rval = true;
          }
        }
        if (!rval && look_for_gene) {
            TFeatScores genes;
            GetOverlappingFeatures (feat.GetLocation(), CSeqFeatData::e_Gene,
                CSeqFeatData::eSubtype_gene, eOverlap_Contains, genes, *m_Scope);
            ITERATE (TFeatScores, s, genes) {
                const CSeq_loc& gene_loc = s->second->GetLocation();
                if (gene_loc.GetStart(eExtreme_Biological) == mrna_start
                    && gene_loc.GetStop(eExtreme_Biological) == mrna_stop
                    && s_MatchPartialType(feat.GetLocation(), gene_loc, partial_type)) {
                    rval = true;
                    break;
                }
            }
        }
    } else if (feat.GetData().IsCdregion()) {
        // coding region is ok if same as mRNA AND partial at splice site or gap
        TFeatScores mRNAs;
        GetOverlappingFeatures (feat.GetLocation(), CSeqFeatData::e_Rna,
            CSeqFeatData::eSubtype_mRNA, eOverlap_CheckIntervals, mRNAs, *m_Scope);
        ITERATE (TFeatScores, s, mRNAs) {
            const CSeq_loc& mrna_loc = s->second->GetLocation();
            bool bad_seq = false;
            bool is_gap = false;
            if (s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)
                && x_IsPartialAtSpliceSiteOrGap(feat.GetLocation(), partial_type, bad_seq, is_gap)) {
                rval = true;
                break;
            }
        }
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon) {
        // exon is ok if its partialness and endpoint matches the mRNA endpoint
        TFeatScores mRNAs;
        GetOverlappingFeatures (feat.GetLocation(), CSeqFeatData::e_Rna,
            CSeqFeatData::eSubtype_mRNA, eOverlap_CheckIntRev, mRNAs, *m_Scope);
        ITERATE (TFeatScores, s, mRNAs) {
            const CSeq_loc& mrna_loc = s->second->GetLocation();
            if (s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)) {
                if (partial_type == eSeqlocPartial_Nostart
                    && mrna_loc.IsPartialStart(eExtreme_Biological)) {
                    rval = true;
                } else if (partial_type == eSeqlocPartial_Nostop
                    && mrna_loc.IsPartialStop(eExtreme_Biological)) {
                    rval = true;
                }
                break;
            }
        }
    }

    return rval;
}


void CValidError_bioseq::ValidateCDSAndProtPartials (const CMappedFeat& feat)
{
    if (! feat.IsSetData()) return;

    SWITCH_ON_SEQFEAT_CHOICE(feat) {
        case NCBI_SEQFEAT(Cdregion):
            {
                if (! feat.IsSetProduct()) return;
                const CSeq_loc& loc = feat.GetProduct();
                const CSeq_id* id = loc.GetId();
                if (id == NULL) return;
                CBioseq_Handle prot_bsh = m_Scope->GetBioseqHandle(*id);
                if (!prot_bsh) {
                    return;
                }
                if (m_Scope->GetBioseqHandle(feat.GetLocationId()).GetTopLevelEntry()
                   != prot_bsh.GetTopLevelEntry()) {
                   return;
                }
                CFeat_CI prot(prot_bsh, CSeqFeatData::eSubtype_prot);
                if (!prot) {
                    return;
                }
                if (!PartialsSame(feat.GetLocation(), prot->GetLocation())) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent,
                        "Coding region and protein feature partials conflict",
                        *(feat.GetSeq_feat()));
                }  
            }
            break;
        case NCBI_SEQFEAT(Prot):
            {
                const CSeq_loc& loc = feat.GetLocation();
                const CSeq_id* id = loc.GetId();
                if (id == NULL) return;
                CBioseq_Handle prot_bsh = m_Scope->GetBioseqHandle(*id);
                if (! prot_bsh) return;
                const CBioseq& pbioseq = *(prot_bsh.GetCompleteBioseq());
                const CSeq_feat* cds = m_Imp.GetCDSGivenProduct(pbioseq);
                if (cds) return;
                CFeat_CI prot(prot_bsh, CSeqFeatData::eSubtype_prot);
                if (! prot) return;
        
                CSeqdesc_CI mi_i(prot_bsh, CSeqdesc::e_Molinfo);
                if (! mi_i) return;
                const CMolInfo& mi = mi_i->GetMolinfo();
                if (! mi.IsSetCompleteness()) return;
                int completeness = mi.GetCompleteness();
        
                const CSeq_loc& prot_loc = prot->GetLocation();
                bool prot_partial5 = prot_loc.IsPartialStart(eExtreme_Biological);
                bool prot_partial3 = prot_loc.IsPartialStop(eExtreme_Biological);
        
                bool conflict = false;
                if (completeness == CMolInfo::eCompleteness_partial && ((! prot_partial5) && (! prot_partial3))) {
                  conflict = true;
                } else if (completeness == CMolInfo::eCompleteness_no_left && ((! prot_partial5) || prot_partial3)) {
                  conflict = true;
                } else if (completeness == CMolInfo::eCompleteness_no_right && (prot_partial5 || (! prot_partial3))) {
                  conflict = true;
                } else if (completeness == CMolInfo::eCompleteness_no_ends && ((! prot_partial5) || (! prot_partial3))) {
                  conflict = true;
                } else if ((completeness < CMolInfo::eCompleteness_partial || completeness > CMolInfo::eCompleteness_no_ends) && (prot_partial5 || prot_partial3)) {
                  conflict = true;
                }
        
                if (conflict) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent,
                        "Molinfo completeness and protein feature partials conflict",
                        *(feat.GetSeq_feat()));
                }
            }
            break;
        default:
            break;
    }
}

void CValidError_bioseq::ValidateFeatPartialInContext (const CMappedFeat& feat)
{

    ValidateCDSAndProtPartials (feat);

    static const string parterr[2] = { "PartialProduct", "PartialLocation" };
    static const string parterrs[4] = {
        "Start does not include first/last residue of sequence",
        "Stop does not include first/last residue of sequence",
        "Internal partial intervals do not include first/last residue of sequence",
        "Improper use of partial (greater than or less than)"
    };

    unsigned int  partial_prod = eSeqlocPartial_Complete, 
                  partial_loc  = eSeqlocPartial_Complete;

    bool is_partial = feat.IsSetPartial()  &&  feat.GetPartial();
    // NOTE - have to use original seqfeat in order for this to work correctly 
    // for features on segmented sequences 
    partial_loc  = SeqLocPartialCheck(feat.GetOriginalSeq_feat()->GetLocation(), m_Scope );
    if (feat.IsSetProduct ()) {
        partial_prod = SeqLocPartialCheck(feat.GetProduct (), m_Scope );
    }

    if ( (partial_loc  == eSeqlocPartial_Complete)  &&
         (partial_prod == eSeqlocPartial_Complete)  &&   
         !is_partial ) {
        return;
    }

    string except_text = "";
    bool no_nonconsensus_except = true;
    if (feat.IsSetExcept_text()) {
        except_text = feat.GetExcept_text();
        if (feat.IsSetExcept()) {
            if (NStr::Find (except_text, "nonconsensus splice site") != string::npos ||
                NStr::Find (except_text, "heterogeneous population sequenced") != string::npos ||
                NStr::Find (except_text, "low-quality sequence region") != string::npos ||
                NStr::Find (except_text, "artificial location") != string::npos) {
                no_nonconsensus_except = false;
            }
        }
    }

    string comment_text = "";
    if (feat.IsSetComment()) {
        comment_text = feat.GetComment();
    }

    // partial product
    unsigned int errtype = eSeqlocPartial_Nostart;
    for ( int j = 0; j < 4; ++j ) {
        int i = 0;
        if (partial_prod & errtype) {
            if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
                       && feat.IsSetExcept()
                       && NStr::Find(except_text, "rearrangement required for product") != string::npos) {
                // suppress
            } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 0) {
                if (no_nonconsensus_except) {
                    if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                        // suppress
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 5' partial is not at start AND is not at consensus splice site",
                            *(feat.GetSeq_feat())); 
                    }
                }
            } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 1) {
                if (no_nonconsensus_except) {
                    if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                        // suppress
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 3' partial is not at stop AND is not at consensus splice site",
                            *(feat.GetSeq_feat()));
                    }
                }
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                    parterr[i] + ": " + parterrs[j], *(feat.GetSeq_feat()));
            }
        }
        errtype <<= 1;
    }

    // partial location
    errtype = eSeqlocPartial_Nostart;
    for ( int j = 0; j < 4; ++j ) {
        int i = 1;
        if (partial_loc & errtype) {
            bool bad_seq = false;
            bool is_gap = false;

            if (feat.GetLocation().GetStop(eExtreme_Biological) == 2647) {
                is_gap = false;
            }

            if (j == 3) {
                if (m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
                    m_Imp.SetFarFetchFailure();                       
                } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
                       && feat.IsSetExcept()
                       && NStr::Find(except_text, "rearrangement required for product") != string::npos) {
                    // suppress
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                        "PartialLocation: Improper use of partial (greater than or less than)", *(feat.GetSeq_feat()));
                }
            } else if (j == 2) {
                if (m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
                    m_Imp.SetFarFetchFailure();                       
                } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
                       && feat.IsSetExcept()
                       && NStr::Find(except_text, "rearrangement required for product") != string::npos) {
                    // suppress
                } else if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                    // ignore start/stop not at end in genomic gpipe sequence
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                        "PartialLocation: Internal partial intervals do not include first/last residue of sequence", *(feat.GetSeq_feat()));
                }
            } else {
                if ( s_IsCDDFeat(feat) ) {
                    // supress warning
                } else if ( m_Scope && x_MatchesOverlappingFeaturePartial(feat, errtype)) {
                    // error is suppressed
                } else if ( feat.IsSetProduct() && m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
                    m_Imp.SetFarFetchFailure();
                } else if ( x_IsPartialAtSpliceSiteOrGap(feat.GetLocation(), errtype, bad_seq, is_gap) ) {
                    if (!is_gap) {
                        if (!feat.GetData().IsCdregion() || x_SplicingNotExpected (feat)) {
                            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                                // ignore in genomic gpipe sequence
                            } else {
                                PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                                    parterr[i] + ": " + parterrs[j] + 
                                    " (but is at consensus splice site)", *(feat.GetSeq_feat()));
                            }
                        } else if (feat.GetData().IsCdregion()) {
                            if (m_CurrentHandle) {
                                CSeqdesc_CI diter (m_CurrentHandle, CSeqdesc::e_Molinfo);
                                if (diter && diter->GetMolinfo().IsSetBiomol()
                                    && diter->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
                                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                                        parterr[i] + ": " + parterrs[j] + 
                                        " (but is at consensus splice site, but is on an mRNA that is already spliced)",
                                        *(feat.GetSeq_feat()));
                                }
                            }
                        }
                    }
                } else if ( bad_seq) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                        parterr[i] + ": " + parterrs[j] + 
                        " (and is at bad sequence)",
                        *(feat.GetSeq_feat()));
                } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
                           && feat.IsSetExcept()
                           && NStr::Find(except_text, "rearrangement required for product") != string::npos) {
                    // suppress
                } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 0) {
                    if (no_nonconsensus_except) {
                        if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                            // suppress
                        } else if (s_PartialAtGapOrNs (m_Scope, feat.GetLocation(), errtype)
                                   && feat.IsSetComment()
                                   && NStr::Find(comment_text, "coding region disrupted by sequencing gap") != string::npos) {
                            // suppress
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                                parterr[i] + 
                                ": 5' partial is not at start AND is not at consensus splice site",
                                *(feat.GetSeq_feat())); 
                        }
                    }
                } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 1) {
                    if (no_nonconsensus_except) {
                        if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                            // suppress
                        } else if (s_PartialAtGapOrNs (m_Scope, feat.GetLocation(), errtype)
                                   && feat.IsSetComment()
                                   && NStr::Find(comment_text, "coding region disrupted by sequencing gap") != string::npos) {
                            // suppress
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                                parterr[i] + 
                                ": 3' partial is not at stop AND is not at consensus splice site",
                                *(feat.GetSeq_feat()));
                        }
                    }
                } else if ( (feat.GetData().Which() == CSeqFeatData::e_Gene  ||
                    feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) &&
                     x_IsSameAsCDS(feat) ) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                        parterr[i] + ": " + parterrs[j], *(feat.GetSeq_feat()));
                } else if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                    // ignore start/stop not at end in genomic gpipe sequence
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                        parterr[i] + ": " + parterrs[j], *(feat.GetSeq_feat()));
                }
            }
        }
        errtype <<= 1;
    }

}


void CValidError_bioseq::ValidateSeqFeatContext(const CBioseq& seq)
{
    // test
    string accession = "";
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsGenbank()) {
            if ((*it)->GetGenbank().IsSetAccession()) {
                accession = (*it)->GetGenbank().GetAccession();
                break;
            }
        } else if ((*it)->IsDdbj()) {
            if ((*it)->GetDdbj().IsSetAccession()) {
                accession = (*it)->GetDdbj().GetAccession();
                break;
            }
        } else if ((*it)->IsGi()) {
            accession = NStr::NumericToString((*it)->GetGi());
        }
    }

    try {
        int            numgene = 0, nummrna = 0, num_pseudomrna = 0, numcds = 0, num_pseudocds = 0, num_rearrangedcds = 0;
        vector< CConstRef < CSeq_id > > cds_products, mrna_products;
        
        int num_full_length_prot_ref = 0;

        bool is_mrna = IsMrna(m_CurrentHandle);
        bool is_prerna = IsPrerna(m_CurrentHandle);
        bool is_aa = CSeq_inst::IsAa(m_CurrentHandle.GetInst_Mol());
        bool is_virtual = (m_CurrentHandle.GetInst_Repr() == CSeq_inst::eRepr_virtual);
        TSeqPos len = m_CurrentHandle.IsSetInst_Length() ? m_CurrentHandle.GetInst_Length() : 0;

        bool is_nc = false, is_emb = false, is_refseq = false;
        FOR_EACH_SEQID_ON_BIOSEQ (seq_it, seq) {
            if ((*seq_it)->IsEmbl()) {
                is_emb = true;
            } else if ((*seq_it)->IsOther()) {
                is_refseq = true;
                if ((*seq_it)->GetOther().IsSetAccession() 
                    && NStr::StartsWith((*seq_it)->GetOther().GetAccession(), "NT_")) {
                    is_nc = true;
                }
            }
        }

        int firstcdsgencode = 0;
        bool mixedcdsgencodes = false;
        if (m_AllFeatIt) {
            m_AllFeatIt->Rewind();            
            for ( CFeat_CI fi = *m_AllFeatIt; fi; ++fi ) {
                const CSeq_feat& feat = fi->GetOriginalFeature();
                
                CSeqFeatData::E_Choice ftype = feat.GetData().Which();

                if (ftype == CSeqFeatData::e_Gene) {
                    numgene++;
                    const CGene_ref& gene_ref = feat.GetData().GetGene();
                    if (gene_ref.IsSetLocus()) {
                        string locus = gene_ref.GetLocus();
                        if (m_GeneIt) {
                            m_GeneIt->Rewind();
                            while (*m_GeneIt) {
                                const CSeq_feat& gene_feat = (*m_GeneIt)->GetOriginalFeature();
                                const CGene_ref& other_gene_ref = gene_feat.GetData().GetGene();
                                if (other_gene_ref.IsSetLocus_tag() 
                                    && NStr::EqualCase (other_gene_ref.GetLocus_tag(), locus)
                                    && (!other_gene_ref.IsSetLocus() 
                                    || !NStr::EqualCase(other_gene_ref.GetLocus(), locus))) {
                                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_LocusCollidesWithLocusTag,
                                        "locus collides with locus_tag in another gene", feat);
                                }
                                ++(*m_GeneIt);
                            }
                        }
                    }
                } else if (feat.GetData().IsCdregion()) {
                    numcds++;
                    if (feat.IsSetProduct()) {
                        const CSeq_id* p = feat.GetProduct().GetId();
                        CConstRef<CSeq_id> ref(p);
                        cds_products.push_back (ref);
                    } else if (feat.IsSetPseudo() && feat.GetPseudo()) {
                            num_pseudocds++;
                    } else {
                        CConstRef<CSeq_feat> gene = 
                            GetOverlappingGene(feat.GetLocation(), *m_Scope);
                        if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                            num_pseudocds++;
                        } else if (feat.IsSetExcept_text() 
                                   && NStr::Find (feat.GetExcept_text(), "rearrangement required for product") != string::npos) {
                            num_rearrangedcds++;
                        }
                    }
                    ValidateBadGeneOverlap (feat);

                    const CCdregion& cdregion = feat.GetData().GetCdregion();
                    if ( cdregion.CanGetCode() ) {
                        int cdsgencode = cdregion.GetCode().GetId();
                        if (firstcdsgencode == 0) {
                            firstcdsgencode = cdsgencode;
                        } else if (firstcdsgencode != cdsgencode) {
                            mixedcdsgencodes = true;
                        }
                    }
                } else if (fi->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
                    nummrna++;
                    if (feat.IsSetProduct()) {
                        const CSeq_id* p = feat.GetProduct().GetId();
                        CConstRef<CSeq_id> ref(p);
                        mrna_products.push_back (ref);
                    } else if (feat.IsSetPseudo() && feat.GetPseudo()) {
                            num_pseudomrna++;
                    } else {
                        CConstRef<CSeq_feat> gene = 
                            GetOverlappingGene(feat.GetLocation(), *m_Scope);
                        if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                            num_pseudomrna++;
                        }
                    }                
                    ValidateBadGeneOverlap(feat);
                }

                m_FeatValidator.ValidateSeqFeatContext(feat, seq);
                
                if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                    ValidateFeatPartialInContext (*fi);
                }

                if ( is_aa ) {                // protein
                    switch ( ftype ) {
                    case CSeqFeatData::e_Prot:
                        {
                            if (IsOneBioseq(feat.GetLocation(), m_Scope)) {
                                CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
                            
                                if ( (range.IsWhole()  ||
                                      (range.GetFrom() == 0  &&  range.GetTo() == len - 1)) &&
                                     (!feat.GetData().GetProt().IsSetProcessed() ||
                                      feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set) ){
                                     num_full_length_prot_ref++;
                                }
                            }
                        }
                        break;
                        
                    case CSeqFeatData::e_Gene:
                        // report only if NOT standalone protein
                        if ( !s_StandaloneProt(m_CurrentHandle) ) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                                "Invalid feature for a protein Bioseq.", feat);
                        }
                        break;

                    default:
                        break;
                    }
                }
                
                if ( is_mrna ) {              // mRNA
                    switch ( ftype ) {
                    case CSeqFeatData::e_Cdregion:
                    {{
                        // Test for Multi interval CDS feature
                        if ( NumOfIntervals(feat.GetLocation()) > 1 ) {
                            bool excpet = feat.IsSetExcept()  &&  feat.GetExcept();
                            bool slippage_except = false;
                            if ( feat.IsSetExcept_text() ) {
                                const string& text = feat.GetExcept_text();
                                slippage_except = 
                                    NStr::FindNoCase(text, "ribosomal slippage") != NPOS;
                            }
                            if ( !excpet  ||  !slippage_except ) {
                                EDiagSev sev = is_refseq ? eDiag_Warning : eDiag_Error;
                                PostErr(sev, eErr_SEQ_FEAT_InvalidForType,
                                    "Multi-interval CDS feature is invalid on an mRNA "
                                    "(cDNA) Bioseq.",
                                    feat);
                            }
                        }
                        break;
                    }}    
                    case CSeqFeatData::e_Rna:
                    {{
                        const CRNA_ref& rref = feat.GetData().GetRna();
                        if ( rref.GetType() == CRNA_ref::eType_mRNA ) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                                "mRNA feature is invalid on an mRNA (cDNA) Bioseq.",
                                feat);
                        }
                        
                        break;
                    }}    
                    case CSeqFeatData::e_Imp:
                    {{
                        const CImp_feat& imp = feat.GetData().GetImp();
                        if ( imp.GetKey() == "intron"  ||
                            imp.GetKey() == "CAAT_signal" ) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                                "Invalid feature for an mRNA Bioseq.", feat);
                        }
                        break;
                    }}
                    default:
                        break;
                    }
                } else if ( is_prerna ) { // preRNA
                    switch ( ftype ) {
                    case CSeqFeatData::e_Imp:
                    {{
                        const CImp_feat& imp = feat.GetData().GetImp();
                        if ( imp.GetKey() == "CAAT_signal" ) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                                "Invalid feature for an pre-RNA Bioseq.", feat);
                        }
                        break;
                    }}
                default:
                    break;
                    }
                }    

                if ( !is_nc  &&  !is_emb  &&  m_Imp.IsFarLocation(feat.GetLocation()) ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_FarLocation,
                        "Feature has 'far' location - accession not packaged in record",
                        feat);
                }

                if ( seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
                    if ( LocOnSeg(seq, feat.GetLocation()) ) {
                        if ( !IsDeltaOrFarSeg(fi->GetLocation(), m_Scope) ) {
                            EDiagSev sev = is_nc ? eDiag_Warning : eDiag_Error;
                            PostErr(sev, eErr_SEQ_FEAT_LocOnSegmentedBioseq,
                                "Feature location on segmented bioseq, not on parts",
                                feat);
                        }
                    }
                }
            }  // end of for loop
        } // end of branch where features are present

        if (mixedcdsgencodes) {
            EDiagSev sev = eDiag_Error;
            if (IsSynthetic(seq)) {
                sev = eDiag_Warning;
            }
            PostErr (sev, eErr_SEQ_FEAT_MultipleGenCodes,
                     "Multiple CDS genetic codes on sequence", seq);
        }

        // if no full length prot feature on a part of a segmented bioseq
        // search for such feature on the master bioseq
        if ( is_aa  &&  num_full_length_prot_ref == 0) {
            CBioseq_Handle parent = s_GetParent(m_CurrentHandle);
            if ( parent ) {
                TSeqPos parent_len = 0;
                if (parent.IsSetInst() && parent.GetInst().IsSetLength()) {
                    parent_len = parent.GetInst().GetLength();
                }
                for ( CFeat_CI it(parent, CSeqFeatData::e_Prot); it; ++it ) {
                    try {
                        const CSeq_feat& prot_feat = it->GetOriginalFeature();
                        CSeq_loc::TRange range = prot_feat.GetLocation().GetTotalRange();
                        
                        if ( (range.IsWhole()  ||
                              (range.GetFrom() == 0  &&  range.GetTo() == parent_len - 1) ) &&
                             (!prot_feat.GetData().GetProt().IsSetProcessed() ||
                              prot_feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set)){
                            num_full_length_prot_ref ++;
                        }
                    } catch ( const exception& ) {
                        CSeq_loc::TRange range = it->GetLocation().GetTotalRange();
                        if ( (range.IsWhole()  ||
                              (range.GetFrom() == 0  &&  range.GetTo() == parent_len - 1) ) &&
                             (!it->GetData().GetProt().IsSetProcessed() ||
                              it->GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set)){
                            num_full_length_prot_ref ++;
                        }
                    }
                }
            }
        }

        if ( is_aa  &&  num_full_length_prot_ref == 0  &&  !is_virtual  &&  !m_Imp.IsPDB()   ) {
            m_Imp.AddProtWithoutFullRef(m_CurrentHandle);
        }

        if ( is_aa && num_full_length_prot_ref > 1 && !SeqIsPatent (seq)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleProtRefs, 
                     NStr::IntToString (num_full_length_prot_ref)
                     + " full-length protein features present on protein", seq);
        }

        // validate abutting UTRs for nucleotides
        if ( !is_aa ) {
            x_ValidateAbuttingUTR(m_CurrentHandle);
        }

        // validate abutting RNA features
        x_ValidateAbuttingRNA (m_CurrentHandle);

        // before validating CDS/mRNA matches, determine whether to suppress duplicate messages
        bool cds_products_unique = true;
        if (cds_products.size() > 1) {
            stable_sort (cds_products.begin(), cds_products.end(), s_SeqIdCompare);
            cds_products_unique = seq_mac_is_unique (cds_products.begin(), cds_products.end(), s_SeqIdMatch);
        }

        bool mrna_products_unique = true;
        if (mrna_products.size() > 1) {
            stable_sort (mrna_products.begin(), mrna_products.end(), s_SeqIdCompare);
            mrna_products_unique = seq_mac_is_unique (mrna_products.begin(), mrna_products.end(), s_SeqIdMatch);
        }

        if (numcds > 0 && nummrna > 1) {
            if (cds_products.size() > 0 && cds_products.size() + num_pseudocds + num_rearrangedcds != numcds) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         NStr::SizetToString (numcds) + " CDS features have "
                         + NStr::SizetToString (cds_products.size()) + " product references",
                        seq);
            }
            if (cds_products.size() > 0 && (! cds_products_unique)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         "CDS products are not unique", seq);
            }
            if (mrna_products.size() > 0 && mrna_products.size() + num_pseudomrna != nummrna) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         NStr::SizetToString (nummrna) + " mRNA features have "
                         + NStr::SizetToString (mrna_products.size()) + " product references",
                         seq);
            }
            if (mrna_products.size() > 0 && (! mrna_products_unique)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         "mRNA products are not unique", seq);
            }
        }

        x_ValidateCDSmRNAmatch(m_CurrentHandle, numgene, numcds, nummrna);

        if (m_Imp.IsLocusTagGeneralMatch()) {
            x_ValidateLocusTagGeneralMatch(m_CurrentHandle);
        }

        if (!SeqIsPatent(seq)) {
            ValidateMultipleGeneOverlap (m_CurrentHandle);
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating Seqfeat Context. EXCEPTION: ") +
                e.what(), seq);
        }
    }

}


void CValidError_bioseq::x_ValidateLocusTagGeneralMatch(const CBioseq_Handle& seq)
{
    if (!seq  ||  !CSeq_inst::IsNa(seq.GetInst_Mol())) {
        return;
    }

    // iterate coding regions and mRNAs
    if (!m_AllFeatIt) {
        return;
    }

    CFeat_CI fi = *m_AllFeatIt;
    fi.Rewind();

    while (fi) {
        CSeqFeatData::ESubtype stype = fi->GetData().GetSubtype();
        if (stype != CSeqFeatData::eSubtype_cdregion
            && stype != CSeqFeatData::eSubtype_mRNA) {
            ++fi;
            continue;
        }
        const CSeq_feat& feat = fi->GetOriginalFeature();
        if (!feat.IsSetProduct()) {
            ++fi;
            continue;
        }
        
        // obtain the gene-ref from the feature or the overlapping gene
        const CGene_ref* grp = feat.GetGeneXref();
        if (grp == NULL  ||  grp->IsSuppressed()) {
            CConstRef<CSeq_feat> gene = 
                GetOverlappingGene(feat.GetLocation(), *m_Scope);
            if (gene) {
                grp = &gene->GetData().GetGene();
            }
        }

        if (grp == NULL  ||  !grp->IsSetLocus_tag()  ||  grp->GetLocus_tag().empty()) {
            ++fi;
            continue;
        }
        const string& locus_tag = grp->GetLocus_tag();
   
        
        CBioseq_Handle prod = m_Scope->GetBioseqHandleFromTSE
            (GetId(feat.GetProduct(), m_Scope), m_Imp.GetTSE());
        if (!prod) {
            ++fi;
            continue;
        }
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(prod.GetCompleteBioseq())) {
            if (!(*it)->IsGeneral()) {
                continue;
            }
            const CDbtag& dbt = (*it)->GetGeneral();
            if (!dbt.IsSetDb()) {
                continue;
            }
            if (s_IsSkippableDbtag (dbt)) {
                continue;
            }

            if (dbt.IsSetTag()  &&  dbt.GetTag().IsStr()) {
                SIZE_TYPE pos = dbt.GetTag().GetStr().find('-');
                string str = dbt.GetTag().GetStr().substr(0, pos);
                if (!NStr::EqualNocase(locus_tag, str)) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProductMismatch,
                        "Gene locus_tag does not match general ID of product",
                        feat);
                }
            }
        }
        ++fi;
    }
}



unsigned int CValidError_bioseq::x_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return 0;
    }

    bool match1 = false, match2 = false;
    bool has1 = false, has2 = false;
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId()) {
            has1 = true;
            if (s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
                match1 = true;
            }
        }
    }

            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId()) {
            has2 = true;
            if (s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
                match2 = true;
            }
        }
    }

    if ((has1 || has2) && (!match1 || !match2)) {
        return 1;
    }

    if (!cds.IsSetProduct() || !mrna.IsSetExt()) {
        return 0;
    }
    
    TGi gi = ZERO_GI;
    if (cds.GetProduct().GetId()->IsGi()) {
        gi = cds.GetProduct().GetId()->GetGi();
    } else {
        // TODO: get gi for other kinds of SeqIds
    }

    if (gi == ZERO_GI) {
        return 0;
    }

    if (mrna.IsSetExt() && mrna.GetExt().IsSetType() && mrna.GetExt().GetType().IsStr()
        && NStr::EqualNocase(mrna.GetExt().GetType().GetStr(), "MrnaProteinLink")
        && mrna.GetExt().IsSetData()
        && mrna.GetExt().GetData().front()->IsSetLabel()
        && mrna.GetExt().GetData().front()->GetLabel().IsStr()
        && NStr::EqualNocase (mrna.GetExt().GetData().front()->GetLabel().GetStr(), "protein seqID")
        && mrna.GetExt().GetData().front()->IsSetData()
        && mrna.GetExt().GetData().front()->GetData().IsStr()) {
        string str = mrna.GetExt().GetData().front()->GetData().GetStr();
        try {
            CSeq_id id (str);
            if (id.IsGi()) {
                if (id.GetGi() == gi) {
                    return 0;
                } else {
                    return 2;
                }
            }
        } catch (CException ) {
        } catch (std::exception ) {
        }
    }
    return 0;
    
}


bool CValidError_bioseq::x_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return false;
    }

    bool match = false;

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            match = true;
            break;
        }
    }
    if (!match) {
        return false;
    }
    match = false;
            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            match = true;
            break;
        }
    }

    return match;
}


CMatchmRNA::CMatchmRNA(const CMappedFeat &mrna) : m_Mrna(mrna.GetSeq_feat()), m_AccountedFor(false)
{
}

CMatchmRNA::~CMatchmRNA(void)
{
}


void CMatchmRNA::SetCDS(CConstRef<CSeq_feat> cds)
{
    m_Cds = cds;
    m_AccountedFor = true;
}


bool CMatchmRNA::HasCDSMatch(void)
{
    bool rval = false;

    if (m_Cds) {
        rval = true;
    } else {
        // iterate through underlying cdss that aren't accounted for
        vector < CMatchCDS * >::iterator cds_it = m_UnderlyingCDSs.begin();
        while (cds_it != m_UnderlyingCDSs.end() && !rval) {
            if (!(*cds_it)->HasmRNA()) {
                rval = true;        
            }
            ++cds_it;
        }
    }
    return rval;

}


bool CMatchmRNA::MatchesUnderlyingCDS (unsigned int partial_type)
{
    bool rval = false;

    TSeqPos mrna_start = m_Mrna->GetLocation().GetStart(eExtreme_Biological);
    TSeqPos mrna_stop = m_Mrna->GetLocation().GetStop(eExtreme_Biological);

    if (m_Cds) {
        if (partial_type == eSeqlocPartial_Nostart) {
            if (m_Cds->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                rval = true;
            } else {
                rval = false;
            }
        } else if (partial_type == eSeqlocPartial_Nostop) {            
            if (m_Cds->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                rval = true;
            } else {
                rval = false;
            }
        }
#if 0
    } else {
        // iterate through underlying cdss that aren't accounted for
        vector < CMatchCDS * >::iterator cds_it = m_UnderlyingCDSs.begin();
        while (cds_it != m_UnderlyingCDSs.end() && !rval) {
            if (!(*cds_it)->HasmRNA()) {
                if (partial_type == eSeqlocPartial_Nostart) {
                    if ((*cds_it)->m_Cds->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                        rval = true;
                    } else {
                        rval = false;
                    }
                } else if (partial_type == eSeqlocPartial_Nostop) {            
                    if ((*cds_it)->m_Cds->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                        rval = true;
                    } else {
                        rval = false;
                    }
                }                
            }
            ++cds_it;
        }
#endif
    }
    return rval;
}


bool CMatchmRNA::MatchAnyUnderlyingCDS (unsigned int partial_type)
{
    bool rval = false;

    TSeqPos mrna_start = m_Mrna->GetLocation().GetStart(eExtreme_Biological);
    TSeqPos mrna_stop = m_Mrna->GetLocation().GetStop(eExtreme_Biological);

    // iterate through underlying cdss that aren't accounted for
    vector < CMatchCDS * >::iterator cds_it = m_UnderlyingCDSs.begin();
    while (cds_it != m_UnderlyingCDSs.end() && !rval) {
        if (partial_type == eSeqlocPartial_Nostart) {
            if ((*cds_it)->m_Cds->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                rval = true;
            } else {
                rval = false;
            }
        } else if (partial_type == eSeqlocPartial_Nostop) {            
            if ((*cds_it)->m_Cds->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                rval = true;
            } else {
                rval = false;
            }
        }                
        ++cds_it;
    }
    return rval;
}


CMatchCDS::CMatchCDS(const CMappedFeat &cds) 
  : m_Cds(cds.GetSeq_feat()),
    m_AssignedMrna (NULL),
    m_XrefMatch (false),
    m_NeedsmRNA (true)
{
}

CMatchCDS::~CMatchCDS(void)
{
}


// only assign if the coding region has only one overlapping unaccounted for mRNA
void CMatchCDS::AssignSinglemRNA(void)
{
    CMatchmRNA *match = NULL;

    vector < CMatchmRNA * >::iterator mrna_it = m_OverlappingmRNAs.begin();
    while (mrna_it != m_OverlappingmRNAs.end()) {
        if (!(*mrna_it)->IsAccountedFor()) {
            if (match == NULL) {
                match = *mrna_it;
            } else {
                // found more than one, can't assign either
                match = NULL;
                break;
            }
        }
        ++mrna_it;
    }
    if (match != NULL) {
        m_AssignedMrna = match;
        match->SetCDS(m_Cds);
    }
}


int CMatchCDS::GetNummRNA(bool &loc_unique)
{
    int num = 0;
    loc_unique = true;

    if (HasmRNA()) {
        // count the one assigned
        num++;
    }
    vector < CMatchmRNA * >::iterator mrna_it = m_OverlappingmRNAs.begin();
    vector < string > product_list;
    while (mrna_it != m_OverlappingmRNAs.end()) {
        if (!(*mrna_it)->IsAccountedFor()) {
            // count overlapping unassigned mRNAS
            if ((*mrna_it)->m_Mrna->IsSetProduct()) {
                string label = "";
                (*mrna_it)->m_Mrna->GetProduct().GetLabel(&label);
                product_list.push_back (label);
            }
            num++;
        }
        ++mrna_it;
    }
    if (product_list.size() > 1) {
        stable_sort (product_list.begin(), product_list.end());
        vector < string >::iterator s1 = product_list.begin();
        vector < string >::iterator s2 = s1;
        s2++;
        while (s2 != product_list.end()) {
            if (NStr::Equal (*s1, *s2)) {
                loc_unique = false;
                break;
            }
            ++s1;
            ++s2;
        }
    }
    return num;
}



CmRNAAndCDSIndex::CmRNAAndCDSIndex()
{
    m_CdsList.clear();
    m_mRNAList.clear();
}


CmRNAAndCDSIndex::~CmRNAAndCDSIndex(void)
{
    for (size_t i = 0; i < m_CdsList.size(); i++) {
        delete m_CdsList[i];
    }
    for (size_t i = 0; i < m_mRNAList.size(); i++) {
        delete m_mRNAList[i];
    }
    m_CdsList.clear();
    m_mRNAList.clear();
}


bool s_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return false;
    }

    bool match = false;

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            match = true;
            break;
        }
    }
    if (!match) {
        return false;
    }
    match = false;
            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            match = true;
            break;
        }
    }

    return match;
}


void CmRNAAndCDSIndex::SetBioseq(CFeat_CI * feat_list, CBioseq_Handle bioseq, CScope * scope)
{
    for (size_t i = 0; i < m_CdsList.size(); i++) {
        delete m_CdsList[i];
    }
    for (size_t i = 0; i < m_mRNAList.size(); i++) {
        delete m_mRNAList[i];
    }
    m_CdsList.clear();
    m_mRNAList.clear();

    if (!feat_list) {
        return;
    }

    CFeat_CI it = *feat_list;
    for ( ; it; ++it) {
        if (it->IsSetData()) {
            if (it->GetData().IsCdregion()) {
                m_CdsList.push_back(new CMatchCDS(*it));
            } else if (it->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                m_mRNAList.push_back(new CMatchmRNA(*it));
            }
        }
    }

    // set up xref pairs
    vector < CMatchCDS * >::iterator cds_it = m_CdsList.begin();
    while (cds_it != m_CdsList.end()) {
        CConstRef<CSeq_feat> cds = (*cds_it)->m_Cds;

        EOverlapType overlap_type = eOverlap_CheckIntRev;
        if (cds->IsSetExcept_text()
            && (NStr::FindNoCase (cds->GetExcept_text(), "ribosomal slippage") != string::npos
                || NStr::FindNoCase (cds->GetExcept_text(), "trans-splicing") != string::npos)) {
            overlap_type = eOverlap_SubsetRev;
        }
        vector < CMatchmRNA * >::iterator mrna_it = m_mRNAList.begin();
        while (mrna_it != m_mRNAList.end()) {
            if (!(*mrna_it)->IsAccountedFor()) {
                CConstRef<CSeq_feat> mrna = (*mrna_it)->m_Mrna;
                if (TestForOverlapEx (cds->GetLocation(), mrna->GetLocation(), overlap_type, scope) >= 0) {
                    (*cds_it)->AddmRNA(*mrna_it);  
                    (*mrna_it)->AddCDS(*cds_it);
                    if (s_IdXrefsAreReciprocal(*cds, *mrna)) {
                        (*cds_it)->SetXrefMatch((*mrna_it));
                        (*mrna_it)->SetCDS(cds);
                    }
                }
            }
            ++mrna_it;
        }
        ++cds_it;
    }
    // allocate remaining pairs
    cds_it = m_CdsList.begin();
    while (cds_it != m_CdsList.end()) {
        if (!(*cds_it)->HasmRNA()) {
            (*cds_it)->AssignSinglemRNA();
        }
        ++cds_it;
    }

}


CMatchmRNA * CmRNAAndCDSIndex::FindMatchmRNA (const CMappedFeat& mrna)
{
    vector < CMatchmRNA * >::iterator mrna_it = m_mRNAList.begin();
    while (mrna_it != m_mRNAList.end()) {
        if (mrna.GetOriginalFeature().Equals(*((*mrna_it)->m_Mrna))) {
            return (*mrna_it);
        }
        ++mrna_it;
    }
    return NULL;
}


bool CmRNAAndCDSIndex::MatchmRNAToCDSEnd (const CMappedFeat& mrna, unsigned int partial_type)
{
    bool rval = false;

    CMatchmRNA * match_mrna = FindMatchmRNA(mrna);
    if (match_mrna && match_mrna->MatchesUnderlyingCDS(partial_type)) {
      rval = true;
    }
    return rval;
}


void CValidError_bioseq::x_ValidateGeneCDSmRNACounts (const CBioseq_Handle& seq)
{
    if (m_GeneIt != NULL && m_AllFeatIt != NULL) {
        m_GeneIt->Rewind();
        if (*m_GeneIt) {
            // nothing to validate if there aren't any genes
            // count mRNAs and CDSs for each gene.
            typedef map<CConstRef<CSeq_feat>, SIZE_TYPE> TFeatCount;
            TFeatCount cds_count, mrna_count;

            // create indices for gene labels and locus tags if needed
            typedef map<string, CConstRef<CSeq_feat> > TGeneList;
            TGeneList gene_labels, gene_locus_tags;

            m_AllFeatIt->Rewind();

            CConstRef<CSeq_feat> gene;

            for (CFeat_CI it = *m_AllFeatIt; it; ++it) {
                CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
                if (subtype != CSeqFeatData::eSubtype_cdregion && subtype != CSeqFeatData::eSubtype_mRNA) {
                    continue;
                }
                const CSeq_feat& feat = it->GetOriginalFeature();
                const CGene_ref* gref = feat.GetGeneXref();

                gene.Reset();

                if (gref == NULL) {
                    gene = GetOverlappingGene(it->GetLocation(), seq.GetScope());
                } else if (!gref->IsSuppressed()) {
                    // find gene by locus tag or label
                    if (gene_labels.empty()) {
                        for (CFeat_CI gene_it = *m_GeneIt; gene_it; ++gene_it) {
                            const CGene_ref& thisgene = gene_it->GetData().GetGene();
                            if (thisgene.IsSetLocus_tag()) {
                                if (gene_locus_tags.find(thisgene.GetLocus_tag()) == gene_locus_tags.end()) {
                                    gene_locus_tags[thisgene.GetLocus_tag()] = gene_it->GetSeq_feat();
                                }
                            }
                            string label = "";
                            thisgene.GetLabel(&label);
                            if (!NStr::IsBlank (label)) {
                                if (gene_labels.find(label) == gene_labels.end()) {
                                    gene_labels[label] = gene_it->GetSeq_feat();
                                }
                            }
                        }
                    }

                    if (gref->IsSetLocus_tag()) {
                        TGeneList::iterator found_gene = gene_locus_tags.find(gref->GetLocus_tag());
                        if (found_gene != gene_locus_tags.end()) {
                            gene = found_gene->second;
                        }
                    } else {
                        string label = "";
                        gref->GetLabel(&label);
                        TGeneList::iterator found_gene = gene_labels.find(label);
                        if (found_gene != gene_labels.end()) {
                            gene = found_gene->second;
                        }
                    }
                }

                if (gene) {
                    if (cds_count.find(gene) == cds_count.end()) {
                        cds_count[gene] = mrna_count[gene] = 0;
                    }

                    switch (subtype) {
                        case CSeqFeatData::eSubtype_cdregion:
                            cds_count[gene]++;
                            break;
                        case CSeqFeatData::eSubtype_mRNA:
                            mrna_count[gene]++;
                            break;
                        default:
                            break;
                    }
                }
            }

            ITERATE (TFeatCount, it, cds_count) {
                SIZE_TYPE cds_num = it->second,
                          mrna_num = mrna_count[it->first];
                if (cds_num > 0 && mrna_num > 1 && cds_num != mrna_num) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAmismatch,
                        "mRNA count (" + NStr::SizetToString(mrna_num) + 
                        ") does not match CDS (" + NStr::SizetToString(cds_num) +
                        ") count for gene", *it->first);
                }
            }
        }
    }    
}


void CValidError_bioseq::x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq, int numgene, int numcds, int nummrna)
{
    // skip this step if this is a genbank record for a bacteria or a virus
    bool is_genbank = false;
    FOR_EACH_SEQID_ON_BIOSEQ (it, *(seq.GetCompleteBioseq())) {
        if ((*it)->IsGenbank()) {
            is_genbank = true;
            break;
        }
    }
    if (is_genbank) {
        CSeqdesc_CI sd(seq, CSeqdesc::e_Source);
        if (sd && sd->GetSource().IsSetOrg() && sd->GetSource().GetOrg().IsSetOrgname()
            && sd->GetSource().GetOrg().GetOrgname().IsSetDiv()) {
            const string& div = sd->GetSource().GetOrg().GetOrgname().GetDiv();
            if (!NStr::Equal(div, "BCT") && !NStr::Equal(div, "VRL")) {
                is_genbank = false;
            }
        }
    }

    if (!is_genbank) {
        x_ValidateGeneCDSmRNACounts(seq);
    }

    if (m_AllFeatIt != NULL) {
        m_AllFeatIt->Rewind();
        vector < CMatchCDS * > cds_list;
        vector < CMatchmRNA * > mrna_list;
        CFeat_CI it = *m_AllFeatIt;
        for ( ; it; ++it) {
            if (it->IsSetData()) {
                if (it->GetData().IsCdregion()) {
                    cds_list.push_back(new CMatchCDS(*it));
                } else if (it->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                    mrna_list.push_back(new CMatchmRNA(*it));
                }
            }
        }

        // set up xref pairs
        vector < CMatchCDS * >::iterator cds_it = cds_list.begin();
        while (cds_it != cds_list.end()) {
            CConstRef<CSeq_feat> cds = (*cds_it)->m_Cds;

            EOverlapType overlap_type = eOverlap_CheckIntRev;
            if (cds->IsSetExcept_text()
                && (NStr::FindNoCase (cds->GetExcept_text(), "ribosomal slippage") != string::npos
                    || NStr::FindNoCase (cds->GetExcept_text(), "trans-splicing") != string::npos)) {
                overlap_type = eOverlap_SubsetRev;
            }
            vector < CMatchmRNA * >::iterator mrna_it = mrna_list.begin();
            while (mrna_it != mrna_list.end()) {
                if (!(*mrna_it)->IsAccountedFor()) {
                    CConstRef<CSeq_feat> mrna = (*mrna_it)->m_Mrna;
                    if (TestForOverlapEx (cds->GetLocation(), mrna->GetLocation(), overlap_type, m_Scope) >= 0) {
                        (*cds_it)->AddmRNA(*mrna_it);                    
                        if (x_IdXrefsAreReciprocal(*cds, *mrna)) {
                            (*cds_it)->SetXrefMatch((*mrna_it));
                            (*mrna_it)->SetAccountedFor(true);
                        }
                    }
                }
                ++mrna_it;
            }
            ++cds_it;
        }
        // allocate remaining pairs
        cds_it = cds_list.begin();
        while (cds_it != cds_list.end()) {
            if (!(*cds_it)->HasmRNA()) {
                (*cds_it)->AssignSinglemRNA();
            }
            ++cds_it;
        }

        // now, look for coding regions that have multiple mRNAs
        cds_it = cds_list.begin();
        int num_cds_without_mrna = 0;
        while (cds_it != cds_list.end()) {
            bool unique_products = false;
            int num_mrna = (*cds_it)->GetNummRNA(unique_products);
            if (num_mrna == 0) {
                // coding regions that are pseudo don't need mRNAs
                if ((*cds_it)->m_Cds->IsSetPseudo() && (*cds_it)->m_Cds->GetPseudo()) {
                    (*cds_it)->SetNeedsmRNA(false);
                } else {
                    CConstRef<CSeq_feat> gene = GetOverlappingGene((*cds_it)->m_Cds->GetLocation(), *m_Scope);
                    if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                        (*cds_it)->SetNeedsmRNA(false);
                    } else if (!(*cds_it)->m_Cds->IsSetExcept_text() 
                               || NStr::Find ((*cds_it)->m_Cds->GetExcept_text(), "rearrangement required for product") == string::npos) {
                        num_cds_without_mrna++;
                    }
                }
            } else if (num_mrna == 1) {
                if ((*cds_it)->HasmRNA()) {
                    unsigned int xrefs_match = x_IdXrefsNotReciprocal (*((*cds_it)->m_Cds),
                                                                        *((*cds_it)->GetmRNA()->m_Mrna));
                    if (xrefs_match == 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefNotReciprocal, 
                                 "CDS/mRNA unambiguous pair have erroneous cross-references",
                                 *((*cds_it)->m_Cds));
                    } else if (xrefs_match == 2) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem, 
                               "MrnaProteinLink inconsistent with feature ID cross-references",
                               *((*cds_it)->GetmRNA()->m_Mrna));
                    }
                }
            } else if (num_mrna > 1) {
                if (!is_genbank) {
                    if (!(*cds_it)->IsXrefMatch()) {
                        // if they have unique products, use a different error
                        if (unique_products) {
                            PostErr (eDiag_Info, eErr_SEQ_FEAT_CDSwithMultipleMRNAs,
                                     "CDS overlapped by " + NStr::IntToString (num_mrna)
                                     + " mRNAs, but product locations are unique",
                                     *((*cds_it)->m_Cds));
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDSwithMultipleMRNAs,
                                     "CDS overlapped by " + NStr::IntToString (num_mrna)
                                     + " mRNAs",
                                     *((*cds_it)->m_Cds));
                        }
                    }
                }
            }
            ++cds_it;
        }

        bool have_cds_gene_mrna = (numgene > 0) && (numcds > 0) && (nummrna > 0);
        if (num_cds_without_mrna > 0 && have_cds_gene_mrna && !is_genbank) {

            if (num_cds_without_mrna >= 10) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDSwithNoMRNAOverlap,
                         NStr::IntToString (num_cds_without_mrna)
                         + " out of " + NStr::IntToString (numcds)
                         + " CDSs overlapped by 0 mRNAs",
                         *(seq.GetCompleteBioseq()));
            } else {
                cds_it = cds_list.begin();
                while (cds_it != cds_list.end()) {
                    bool unique_products = false;
                    int num_mrna = (*cds_it)->GetNummRNA(unique_products);
                    if (num_mrna == 0) {
                        CConstRef<CSeq_feat> gene = GetOverlappingGene((*cds_it)->m_Cds->GetLocation(), *m_Scope);
                        if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                            // pseudo, don't report
                        } else if (!(*cds_it)->m_Cds->IsSetExcept_text() 
                                   || NStr::Find ((*cds_it)->m_Cds->GetExcept_text(), "rearrangement required for product") == string::npos) {          
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDSwithNoMRNAOverlap,
                                     "CDS overlapped by 0 mRNAs", *(*cds_it)->m_Cds);
                        }
                    }
                    ++cds_it;
                }
            }
        }

        for (size_t i = 0; i < cds_list.size(); i++) {
            delete cds_list[i];
        }
        for (size_t i = 0; i < mrna_list.size(); i++) {
            delete mrna_list[i];
        }

    }
}


void CValidError_bioseq::x_ValidateAbuttingUTR(const CBioseq_Handle& seq)
{
    // note - if we couldn't build the feature iterator, no point
    // in trying this
    if (!m_AllFeatIt) {
        return;
    }

    // count features of interest, find strand for coding region
    ENa_strand strand = eNa_strand_unknown;

    SAnnotSelector count_sel;
    count_sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    count_sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_3UTR);
    count_sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_5UTR);
    count_sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);

    CFeat_CI cug_it (seq, count_sel);

    int num_cds = 0;
    int num_3utr = 0;
    int num_5utr = 0;
    int num_gene = 0;
    for ( ; cug_it; ++cug_it) {
        CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_cdregion) {
            num_cds++;
            strand = cug_it->GetLocation().GetStrand();
        } else if (subtype == CSeqFeatData::eSubtype_3UTR) {
            num_3utr++;
        } else if (subtype == CSeqFeatData::eSubtype_5UTR) {
            num_5utr++;
        } else if (subtype == CSeqFeatData::eSubtype_gene) {
            num_gene++;
        }
    }

    bool is_mrna = false;
    if (seq.CanGetInst_Mol() && seq.GetInst_Mol() == CSeq_inst::eMol_rna) {
        CSeqdesc_CI sd(seq, CSeqdesc::e_Molinfo);
        if (!sd) {
            // assume RNA sequence is mRNA
            is_mrna = true;
        } else  if (sd->GetMolinfo().IsSetBiomol() && sd->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mrna = true;
        }
    }    

    if (is_mrna) {
        cug_it.Rewind();
        for ( ; cug_it; ++cug_it ) {
            CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
            if (subtype == CSeqFeatData::eSubtype_cdregion) {
                if (cug_it->GetLocation().GetStrand() == eNa_strand_minus) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_CDSonMinusStrandMRNA,
                             "CDS should not be on minus strand of mRNA molecule", cug_it->GetOriginalFeature());
                }
            }
        }
    }

    if (is_mrna || (num_cds == 1 && num_gene < 2)) {

        if (is_mrna) {
            // if this is an mRNA sequence, features should be on the plus strand
            strand = eNa_strand_plus;
        }

        int utr5_right = 0;
        int utr3_right = 0;
        int cds_right = 0;
        bool first_cds = true;

        cug_it.Rewind();

        if (strand == eNa_strand_minus) {
            // minus strand - expect 3'UTR, CDS, 5'UTR
            for ( ; cug_it; ++cug_it ) {
                CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
                int this_left = cug_it->GetLocation().GetStart (eExtreme_Positional);
                int this_right = cug_it->GetLocation().GetStop (eExtreme_Positional);
                if (subtype == CSeqFeatData::eSubtype_3UTR) {
                    if (cug_it->GetLocation().GetStrand() != eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "3'UTR is not on minus strand", cug_it->GetOriginalFeature());
                    } else if (utr5_right > 0 && utr5_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "Previous 5'UTR does not abut next 3'UTR", cug_it->GetOriginalFeature());
                    }
                    utr3_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
                    if (utr3_right > 0 && utr3_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "CDS does not abut 3'UTR", cug_it->GetOriginalFeature());
                    }
                    first_cds = false;
                    cds_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_5UTR && num_5utr < 2) {
                    if (cug_it->GetLocation().GetStrand() != eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "5'UTR is not on minus strand", cug_it->GetOriginalFeature());
                    } else if (cds_right > 0 && cds_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "5'UTR does not abut CDS", cug_it->GetOriginalFeature());
                    }
                    utr5_right = this_right;
                }
            }
        } else {
            // plus strand - expect 5'UTR, CDS, 3'UTR
            for ( ; cug_it; ++cug_it ) {
                CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
                int this_left = cug_it->GetLocation().GetStart (eExtreme_Positional);
                int this_right = cug_it->GetLocation().GetStop (eExtreme_Positional);
                if (subtype == CSeqFeatData::eSubtype_5UTR && num_5utr < 2) {
                    if (cug_it->GetLocation().GetStrand() == eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "5'UTR is not on plus strand", cug_it->GetOriginalFeature());
                    } else if (utr3_right > 0 && utr3_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "Previous 3'UTR does not abut next 5'UTR", cug_it->GetOriginalFeature());
                    }
                    utr5_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
                    if (utr5_right > 0 && utr5_right + 1 != this_left && first_cds ) {

                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "5'UTR does not abut CDS", cug_it->GetOriginalFeature());
                    }
                    first_cds = false;
                    cds_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_3UTR) {
                    if (cug_it->GetLocation().GetStrand() == eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "3'UTR is not on plus strand", cug_it->GetOriginalFeature());
                    } else if (cds_right > 0 && cds_right + 1 != this_left && num_3utr == 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "CDS does not abut 3'UTR", cug_it->GetOriginalFeature());
                    }
                    if (is_mrna && num_cds == 1 && num_3utr == 1 && this_right != seq.GetBioseqLength() - 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotExtendToEnd, 
                                 "3'UTR does not extend to end of mRNA", cug_it->GetOriginalFeature());
                    }
                }
            }
        }
    }
}

enum ERnaPosition {
    e_RnaPosition_Ignore = 0,
    e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_1,
    e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_2,
    e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_X
};

static ERnaPosition s_RnaPosition (const CSeq_feat& feat)
{
    ERnaPosition rval = e_RnaPosition_Ignore;

    if (!feat.IsSetData() || !feat.GetData().IsRna()) {
        return e_RnaPosition_Ignore;
    }
    const CRNA_ref& rna = feat.GetData().GetRna();
    if (!rna.IsSetType()) {
        rval = e_RnaPosition_Ignore;
    } else if (!rna.IsSetExt()) {
        rval = e_RnaPosition_Ignore;
    } else if (rna.GetType() == CRNA_ref::eType_rRNA) {
        const string& product = rna.GetExt().GetName();
        if (NStr::StartsWith (product, "small ", NStr::eNocase)
            || NStr::StartsWith (product, "18S ", NStr::eNocase)
            || NStr::StartsWith (product, "16S ", NStr::eNocase)
            // variant spellings
            || NStr::StartsWith (product, "18 ", NStr::eNocase)
            || NStr::StartsWith (product, "16 ", NStr::eNocase)) {
            rval = e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT;
        } else if (NStr::StartsWith (product, "5.8S ", NStr::eNocase)
                   // variant spellings
                   || NStr::StartsWith (product, "5.8 ", NStr::eNocase)) {
            rval = e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT;
        } else if (NStr::StartsWith (product, "large ", NStr::eNocase)
                   || NStr::StartsWith (product, "26S ", NStr::eNocase)
                   || NStr::StartsWith (product, "28S ", NStr::eNocase)
                   || NStr::StartsWith (product, "23S ", NStr::eNocase)
                   // variant spellings
                   || NStr::StartsWith (product, "26 ", NStr::eNocase)
                   || NStr::StartsWith (product, "28 ", NStr::eNocase)
                   || NStr::StartsWith (product, "23 ", NStr::eNocase)) {
            rval = e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT;
        }
    } else if (rna.GetType() == CRNA_ref::eType_other || rna.GetType() == CRNA_ref::eType_miscRNA) {
        string product = "";
        if (rna.GetExt().IsName()) {
            product = rna.GetExt().GetName();
            if (NStr::EqualNocase (product, "misc_RNA")) {
                FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
                    if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "product")
                        && (*it)->IsSetVal() && !NStr::IsBlank ((*it)->GetVal())) {
                        product = (*it)->GetVal();
                        break;
                    }
                }
            }
        } else if (rna.GetExt().IsGen()) {
            if (rna.GetExt().GetGen().IsSetProduct()) {
                product = rna.GetExt().GetGen().GetProduct();
            }
        }
        if (NStr::EqualNocase (product, "internal transcribed spacer 1")
            || NStr::EqualNocase (product, "internal transcribed spacer1")) {
            rval = e_RnaPosition_INTERNAL_SPACER_1;
        } else if (NStr::EqualNocase (product, "internal transcribed spacer 2")
                   || NStr::EqualNocase (product, "internal transcribed spacer2")) {
            rval = e_RnaPosition_INTERNAL_SPACER_2;
        } else if (NStr::EqualNocase (product, "internal transcribed spacer")
                   || NStr::EqualNocase (product, "ITS")
                   || NStr::EqualNocase (product, "16S-23S ribosomal RNA intergenic spacer")
                   || NStr::EqualNocase (product, "16S-23S intergenic spacer")
                   || NStr::EqualNocase (product, "intergenic spacer")) {
            rval = e_RnaPosition_INTERNAL_SPACER_X;
        }
    }
    return rval;
}


bool CValidError_bioseq::x_IsRangeGap (const CBioseq_Handle& seq, int start, int stop)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr() 
        || seq.GetInst().GetRepr() != CSeq_inst::eRepr_delta
        || !seq.GetInst().IsSetExt()
        || !seq.GetInst().GetExt().IsDelta()
        || !seq.GetInst().GetExt().GetDelta().IsSet()) {
        return false;
    }
    if (start < 0 || (unsigned int) stop >= seq.GetInst_Length() || start > stop) {
        return false;
    }

    int offset = 0;
    ITERATE (CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
        int this_len = 0;
        if ((*it)->IsLiteral()) {
            this_len = (*it)->GetLiteral().GetLength();
        } else if ((*it)->IsLoc()) {
            this_len = GetLength((*it)->GetLoc(), m_Scope);
        }
        if ((*it)->IsLiteral() && 
            (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
              if (start >= offset && stop < offset + this_len) {
                  return true;
              }
        }
        offset += this_len;
        if (offset > start) {
            return false;
        }
    }
    return false;
}


void CValidError_bioseq::x_ValidateAbuttingRNA(const CBioseq_Handle& seq)
{
    // if unable to build feature iterator for this sequence, this will
    // also fail
    if (!m_AllFeatIt) {
        return;
    }
    if (seq.IsSetInst() && seq.GetInst().IsSetMol() && seq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
        CSeqdesc_CI sd(seq, CSeqdesc::e_Molinfo);
        if (sd && sd->GetMolinfo().IsSetBiomol() && sd->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            // do not check for mRNA sequences
            return;
        }
    }

    SAnnotSelector sel (CSeqFeatData::e_Rna);

    CFeat_CI it(seq, sel);


    if (it) {
        
        ENa_strand strand1 = eNa_strand_plus;
        if (it->GetLocation().IsSetStrand() && it->GetLocation().GetStrand() == eNa_strand_minus) {
            strand1 = eNa_strand_minus;
        }
        ERnaPosition pos1 = s_RnaPosition (it->GetOriginalFeature());
        int right1 = it->GetLocation().GetStop(eExtreme_Positional);

        CFeat_CI it2 = it;
        ++it2;
        while (it2) {
            ERnaPosition pos2 = s_RnaPosition (it2->GetOriginalFeature());
            if (pos2 != e_RnaPosition_Ignore) {
                ENa_strand strand2 = eNa_strand_plus;
                if (it2->GetLocation().IsSetStrand() && it2->GetLocation().GetStrand() == eNa_strand_minus) {
                    strand2 = eNa_strand_minus;
                }
                int left2 = it2->GetLocation().GetStart(eExtreme_Positional);
                int right2 = it2->GetLocation().GetStop(eExtreme_Positional);
    
                if ((strand1 == eNa_strand_minus && strand2 != eNa_strand_minus)
                    || (strand1 != eNa_strand_minus && strand2 == eNa_strand_minus)) {
                    // different strands
                    if (pos1 != e_RnaPosition_Ignore && pos2 != e_RnaPosition_Ignore) {
                        if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT || pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT || pos1 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) &&
                            (pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT || pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT || pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
                        } else if ((pos1 == e_RnaPosition_INTERNAL_SPACER_1 || pos1 == e_RnaPosition_INTERNAL_SPACER_2 || pos1 == e_RnaPosition_INTERNAL_SPACER_X) &&
                            (pos2 == e_RnaPosition_INTERNAL_SPACER_1 || pos2 == e_RnaPosition_INTERNAL_SPACER_2 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) {
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_InconsistentRRNAstrands, 
                                     "Inconsistent strands for rRNA components",
                                     it2->GetOriginalFeature());
                        }
                    }
                } else if (pos1 == e_RnaPosition_Ignore || pos2 == e_RnaPosition_Ignore) {
                    // ignore
                } else if (right1 + 1 < left2) {
                    // gap between features
                    if (x_IsRangeGap (seq, right1 + 1, left2 - 1)) {
                        // ignore, gap between features is gap in sequence
                    } else if (strand1 == eNa_strand_minus) {
                        if ((pos1 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_2 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 == e_RnaPosition_INTERNAL_SPACER_1) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ITSdoesNotAbutRRNA, 
                                     "ITS does not abut adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    } else {
                        if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_1 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 == e_RnaPosition_INTERNAL_SPACER_2) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ITSdoesNotAbutRRNA, 
                                     "ITS does not abut adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    }
                } else if (right1 + 1 > left2) {
                    // features overlap
                    if (strand1 == eNa_strand_minus) {
                        // on minus strand
                        if ((pos1 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_2 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 == e_RnaPosition_INTERNAL_SPACER_1) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlap, 
                                     "ITS overlaps adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    } else {
                        // on plus strand
                        if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_1 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 == e_RnaPosition_INTERNAL_SPACER_2) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlap, 
                                     "ITS overlaps adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    }
    
                } else {
                    // features abut
                    if (strand1 == eNa_strand_minus) {
                        // on minus strand
                        if (pos1 == pos2 
                            && it->GetLocation().IsPartialStop (eExtreme_Positional)
                            && it2->GetLocation().IsPartialStart (eExtreme_Positional)
                            && seq.IsSetInst_Repr() && seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
                          /* okay in segmented set */
                        } else if ((pos1 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT && (pos2 != e_RnaPosition_INTERNAL_SPACER_2 && pos2 != e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 != e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 != e_RnaPosition_INTERNAL_SPACER_1) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 != e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 != e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOrder,
                                     "Problem with order of abutting rRNA components",
                                     it2->GetOriginalFeature());
                        }
                    } else {
                        // on plus strand
                        if (pos1 == pos2 
                            && it->GetLocation().IsPartialStop (eExtreme_Positional)
                            && it2->GetLocation().IsPartialStart (eExtreme_Positional)
                            && seq.IsSetInst_Repr() && seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
                          /* okay in segmented set */
                        } else if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT && (pos2 != e_RnaPosition_INTERNAL_SPACER_1 && pos2 != e_RnaPosition_INTERNAL_SPACER_X)) ||
                            (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && pos2 != e_RnaPosition_INTERNAL_SPACER_2) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_1 && pos2 != e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 != e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) ||
                            (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 != e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOrder,
                                     "Problem with order of abutting rRNA components",
                                     it2->GetOriginalFeature());
                        }
                    }
                }
                it = it2;
                pos1 = pos2;
                strand1 = strand2;
                right1 = right2;
            }
            ++it2;
        }
    }

}


static bool s_SpecialFlybaseId (const CSeq_id& id)
{
    bool rval = false;
    if (id.IsOther()) {
        if (id.GetOther().IsSetAccession()) {
            const string& accession = id.GetOther().GetAccession();
            if (NStr::StartsWith(accession, "NT_")
                || NStr::StartsWith(accession, "NC_")
                || NStr::StartsWith(accession, "NG_")
                || NStr::StartsWith(accession, "NW_")) {
                rval = true;
            }
        }
    }
    return rval;
}


static bool s_SpecialFlybaseId (const CSeq_feat& feat, CScope * scope)
{
    bool rval = false;

    CBioseq_Handle bsh = BioseqHandleFromLocation(scope, feat.GetLocation());

    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
            rval = s_SpecialFlybaseId (**id_it);
            if (rval) {
                break;
            }
        }
    } else {
        CSeq_loc_CI loc_ci (feat.GetLocation());
        while (loc_ci && !rval) {
            rval = s_SpecialFlybaseId (loc_ci.GetSeq_id());
            ++loc_ci;
        }
    }
    return rval;
}


static bool s_SpecialDuplicateFeatID (const CSeq_id& id)
{
    bool rval = false;

    if (id.IsOther()) {
        if (id.GetOther().IsSetAccession()) {
            const string& accession = id.GetOther().GetAccession();
            if (NStr::StartsWith(accession, "NT_")
                || NStr::StartsWith(accession, "NC_")
                || NStr::StartsWith(accession, "NG_")
                || NStr::StartsWith(accession, "NW_")) {
                rval = true;
            }
        }
    } else if (id.IsGenbank()) {
        rval = true;
    } else if (id.IsGeneral() && !id.GetGeneral().IsSkippable()) {
        rval = true;
    }

    return rval;
}


static bool s_SpecialDuplicateFeatID (const CSeq_feat& feat, CScope * scope)
{
    bool rval = false;

    CBioseq_Handle bsh = BioseqHandleFromLocation(scope, feat.GetLocation());

    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
            rval = s_SpecialDuplicateFeatID (**id_it);
            if (rval) {
                break;
            }
        }
    } else {
        CSeq_loc_CI loc_ci (feat.GetLocation());
        while (loc_ci && !rval) {
            rval = s_SpecialDuplicateFeatID (loc_ci.GetSeq_id());
            ++loc_ci;
        }
    }
    return rval;
}


static bool s_GeneXrefsDiffer (const CSeq_feat& f1, const CSeq_feat& f2, CScope *scope)
{
    CConstRef <CSeq_feat> g1 = GetGeneForFeature (f1, scope);
    CConstRef <CSeq_feat> g2 = GetGeneForFeature (f2, scope);

    if (!g1 || !g2) {
        return false;
    } else if (g1 != g2) {
        return true;
    } else {
        return false;
    }

}


EDiagSev CValidError_bioseq::x_DupFeatSeverity 
(const CSeq_feat& curr,
 const CSeq_feat& prev, 
 bool is_fruitfly,
 bool is_viral,
 bool is_htgs,
 bool same_annot,
 bool same_label)
{
    if (!same_annot && !same_label) {
        return eDiag_Warning;
    }

    EDiagSev severity = eDiag_Error;
    CSeqFeatData::ESubtype curr_subtype = curr.GetData().GetSubtype();

    if ( curr_subtype == CSeqFeatData::eSubtype_pub          ||
         curr_subtype == CSeqFeatData::eSubtype_region       ||
         curr_subtype == CSeqFeatData::eSubtype_misc_feature ||
         curr_subtype == CSeqFeatData::eSubtype_STS          ||
         curr_subtype == CSeqFeatData::eSubtype_variation ) {
        // lower severity for some features
        severity = eDiag_Warning;
    } else if (s_SpecialDuplicateFeatID(curr, m_Scope)) {
        if (is_fruitfly || s_SpecialFlybaseId(curr, m_Scope)) {
            severity = eDiag_Warning;
        }
    } else {
        severity = eDiag_Warning;
    }

    // if different CDS frames, lower to warning
    if (curr_subtype == CSeqFeatData::eSubtype_cdregion) {
        CCdregion::EFrame       curr_frame, prev_frame;
        curr_frame = curr.GetData().GetCdregion().GetFrame();
        prev_frame = prev.GetData().GetCdregion().GetFrame();
        
        if ( (curr_frame != CCdregion::eFrame_not_set  &&
            curr_frame != CCdregion::eFrame_one)     ||
            (prev_frame != CCdregion::eFrame_not_set  &&
            prev_frame != CCdregion::eFrame_one) ) {
            if ( curr_frame != prev_frame ) {
                severity = eDiag_Warning;
            }
        }
    }

    if ( (prev.IsSetDbxref()  &&
          IsFlybaseDbxrefs(prev.GetDbxref()))  || 
         (curr.IsSetDbxref()  && 
          IsFlybaseDbxrefs(curr.GetDbxref())) ) {
        severity = eDiag_Error;
    }

    if (curr_subtype == CSeqFeatData::eSubtype_repeat_region
        || curr_subtype == CSeqFeatData::eSubtype_site
        || curr_subtype == CSeqFeatData::eSubtype_bond) {
        severity = eDiag_Warning;
    }

    if (same_annot) {
        // lower severity for some pairs of partial features or pseudo features
        if (curr.IsSetPartial() && curr.GetPartial()
            && prev.IsSetPartial() && prev.GetPartial()) {
            if (curr_subtype == CSeqFeatData::eSubtype_gene
                || curr_subtype == CSeqFeatData::eSubtype_mRNA
                || (curr_subtype == CSeqFeatData::eSubtype_cdregion && is_viral)) {
                severity = eDiag_Warning;
            }
        }
        if (curr_subtype == CSeqFeatData::eSubtype_gene
            && curr.IsSetPseudo() && curr.GetPseudo()
            && prev.IsSetPseudo() && prev.GetPseudo()) {
            severity = eDiag_Warning;
        } else if (curr_subtype == CSeqFeatData::eSubtype_gene && is_viral) {
            severity = eDiag_Warning;
        } else if (curr_subtype == CSeqFeatData::eSubtype_cdregion && is_htgs) {
            severity = eDiag_Warning;
        }
        
        if (same_label) {
            if (s_GeneXrefsDiffer(curr, prev, m_Scope)) {
                severity = eDiag_Warning;
            }
        }
    } else {
        /* not same annot */
        if (same_label) {
            if (s_GeneXrefsDiffer(curr, prev, m_Scope)) {
                severity = eDiag_Warning;
            }
        } else {
            severity = eDiag_Warning;
        }
    }

    return severity;
}



void CValidError_bioseq::x_ReportDupOverlapFeaturePair (CSeq_feat_Handle f1, const CSeq_feat& feat1, CSeq_feat_Handle f2, const CSeq_feat& feat2, bool fruit_fly, bool viral, bool htgs)
{
    // Get type of duplication, if any
    EDuplicateFeatureType dup_type = IsDuplicate (f1, feat1, f2, feat2, *m_Scope);

    switch (dup_type) {
        case eDuplicate_Duplicate:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, fruit_fly, viral, htgs, true, true);
                CConstRef <CSeq_feat> g1 = GetGeneForFeature (feat1, m_Scope);
                CConstRef <CSeq_feat> g2 = GetGeneForFeature (feat2, m_Scope);
                if (g1 && g2 && g1 != g2) {
                    severity = eDiag_Warning;
                }
                PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                    "Duplicate feature", feat2);
            }}
            break;
        case eDuplicate_SameIntervalDifferentLabel:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, fruit_fly, viral, htgs, true, false);
                if (feat1.GetData().IsImp()) {
                    severity = eDiag_Warning;
                }
                PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                    "Features have identical intervals, but labels differ",
                    feat2);
            }}
            break;
        case eDuplicate_DuplicateDifferentTable:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, fruit_fly, viral, htgs, false, true);
                PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                    "Duplicate feature (packaged in different feature table)",
                    feat2);
            }}
            break;
        case eDuplicate_SameIntervalDifferentLabelDifferentTable:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, fruit_fly, viral, htgs, false, false);
                PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                    "Features have identical intervals, but labels "
                    "differ (packaged in different feature table)",
                    feat2);
            }}
            break;
        case eDuplicate_Not:
            // no error
            break;
    }
}


void CValidError_bioseq::x_ReportOverlappingPeptidePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, const CBioseq& bioseq, bool& reported_last_peptide)
{
    const CSeq_feat& feat1 = *(f1.GetSeq_feat());
    const CSeq_feat& feat2 = *(f2.GetSeq_feat());
    // subtypes
    CSeqFeatData::ESubtype feat1_subtype = feat1.GetData().GetSubtype();
    CSeqFeatData::ESubtype feat2_subtype = feat2.GetData().GetSubtype();
    // locations
    const CSeq_loc& feat1_loc = feat1.GetLocation();
    const CSeq_loc& feat2_loc = feat2.GetLocation();

    if ( (feat1_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
          feat1_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
          feat1_subtype == CSeqFeatData::eSubtype_transit_peptide_aa)) {
        if ((feat2_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
             feat2_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
             feat2_subtype == CSeqFeatData::eSubtype_transit_peptide_aa) &&
            sequence::Compare(feat1_loc, feat2_loc, m_Scope) != eNoOverlap  &&
            s_NotPeptideException(feat1, feat2)) {
            EDiagSev overlapPepSev = 
                m_Imp.IsOvlPepErr() ? eDiag_Error :eDiag_Warning;
            string msg = "Signal, Transit, or Mature peptide features overlap";

            try {
                const CSeq_feat* cds = m_Imp.GetCDSGivenProduct(bioseq);
                if (cds != NULL) {
                    string cds_loc = "";
                    const CSeq_id* id = cds->GetLocation().GetId();
                    if (id != NULL) {
                        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
                        if (bsh && bsh.GetCompleteBioseq()) {
                            AppendBioseqLabel (cds_loc, *(bsh.GetCompleteBioseq()), true);
                            if (NStr::StartsWith(cds_loc, "BIOSEQ: ")) {
                                cds_loc = cds_loc.substr(8);
                            }
                        } else {
                            id->GetLabel(&cds_loc, CSeq_id::eContent);
                        }
                    }
                    if (!NStr::IsBlank(cds_loc)) {
                        cds_loc = " (parent CDS is on " + cds_loc + ")";
                        msg += cds_loc;
                    }
                }
            } catch ( const exception& ) {
            }

            if (!reported_last_peptide) {
                PostErr( overlapPepSev,
                    eErr_SEQ_FEAT_OverlappingPeptideFeat,
                    msg,
                    feat1); 
            }
            PostErr( overlapPepSev,
                eErr_SEQ_FEAT_OverlappingPeptideFeat,
                msg,
                feat2);
            reported_last_peptide = true;
        } else {
            reported_last_peptide = false;
        }
    }
}


void CValidError_bioseq::ValidateDupOrOverlapFeats(const CBioseq& bioseq)
{
#ifndef GOFAST
    if (!m_AllFeatIt) {
        return;
    }

    try {
        
        bool fruit_fly = false;
        bool viral = false;
        bool htgs = false;

        CSeqdesc_CI di(m_CurrentHandle, CSeqdesc::e_Source);
        if ( di && di->GetSource().IsSetOrg()) {
            if (di->GetSource().GetOrg().IsSetTaxname()
                && NStr::EqualNocase(di->GetSource().GetOrg().GetTaxname(),
                                     "Drosophila melanogaster")) {
                fruit_fly = true;
            }
            if ( di->GetSource().GetOrg().IsSetOrgname()
                 && di->GetSource().GetOrg().GetOrgname().IsSetLineage()
                 && NStr::StartsWith (di->GetSource().GetOrg().GetOrgname().GetLineage(), "Viruses; ")) {
                 viral = true;
            }
        }

        CSeqdesc_CI mi(m_CurrentHandle, CSeqdesc::e_Molinfo);
        if (mi && mi->GetMolinfo().IsSetTech()) {
            CMolInfo::TTech tech = mi->GetMolinfo().GetTech();
            htgs = (tech == CMolInfo::eTech_htgs_1
                    || tech == CMolInfo::eTech_htgs_2
                    || tech == CMolInfo::eTech_htgs_3);
        }

        m_AllFeatIt->Rewind();
        CFeat_CI prev_it = *m_AllFeatIt;
        while (prev_it) {
            CFeat_CI curr_it = prev_it;
            ++curr_it;
            CConstRef<CSeq_feat> prev_feat = prev_it->GetSeq_feat();
            TSeqPos prev_end = prev_feat->GetLocation().GetStop (eExtreme_Positional);
            while (curr_it) {
                CConstRef<CSeq_feat>curr_feat = curr_it->GetSeq_feat();
                TSeqPos curr_start = curr_feat->GetLocation().GetStart(eExtreme_Positional);
                if (curr_start > prev_end) {
                    break;
                }
                x_ReportDupOverlapFeaturePair (prev_it->GetSeq_feat_Handle(), 
                                               *prev_feat, 
                                               curr_it->GetSeq_feat_Handle(), 
                                               *curr_feat,
                                               fruit_fly, viral, htgs);
                ++curr_it;
            }
            ++prev_it;
        }

        m_AllFeatIt->Rewind();
        CFeat_CI prev_prot = *m_AllFeatIt;
        if (prev_prot) {
            CFeat_CI curr_prot = prev_prot;
            ++curr_prot;           
            bool reported_last_peptide = false;
            while (curr_prot) {
                x_ReportOverlappingPeptidePair(prev_prot->GetSeq_feat_Handle(), curr_prot->GetSeq_feat_Handle(), bioseq, reported_last_peptide);
                ++curr_prot;
                ++prev_prot;
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating duplicate/overlapping features. EXCEPTION: ") +     
                e.what(), bioseq);
        }
    }
#endif
}


bool CValidError_bioseq::IsFlybaseDbxrefs(const TDbtags& dbxrefs)
{
    ITERATE (TDbtags, db, dbxrefs) {
        if ( (*db)->CanGetDb() ) {
            if ( NStr::EqualCase((*db)->GetDb(), "FLYBASE") ||
                 NStr::EqualCase((*db)->GetDb(), "FlyBase") ) {
                return true;
            }
        }
    }
    return false;
}


static bool s_IsTPAAssemblyOkForBioseq (const CBioseq& seq)
{
    bool has_local = false, has_genbank = false, has_refseq = false;
    bool has_gi = false, has_tpa = false, has_bankit = false, has_smart = false;

    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        switch ((*it)->Which()) {
            case CSeq_id::e_Local:
                has_local = true;
                break;
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
                has_genbank = true;
                break;
            case CSeq_id::e_Other:
                has_refseq = true;
                break;
            case CSeq_id::e_Gi:
                has_gi = true;
                break;
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                has_tpa = true;
                break;
            case CSeq_id::e_General:
                if ((*it)->GetGeneral().IsSetDb()) {
                    if (NStr::Equal((*it)->GetGeneral().GetDb(), "BankIt", NStr::eNocase)) {
                        has_bankit = true;
                    } else if (NStr::Equal((*it)->GetGeneral().GetDb(), "TMSMART", NStr::eNocase)) {
                        has_smart = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    if (has_genbank) return false;
    if (has_tpa) return true;
    if (has_refseq) return false;
    if (has_bankit) return true;
    if (has_smart) return true;
    if (has_gi) return false;
    if (has_local) return true;

    return false;    
}


static string GetDateString (const CDate& date)
{
    string val = "";

    if (date.IsStr()) {
        val = date.GetStr();
    } else if (date.IsStd()) {
        if (date.GetStd().IsSetYear()) {
            date.GetDate(&val, "%{%N %{%D, %}%}%Y");
            NStr::ReplaceInPlace(val, "January", "Jan");
            NStr::ReplaceInPlace(val, "February", "Feb");
            NStr::ReplaceInPlace(val, "March", "Mar");
            NStr::ReplaceInPlace(val, "April", "Apr");
            NStr::ReplaceInPlace(val, "June", "Jun");
            NStr::ReplaceInPlace(val, "July", "Jul");
            NStr::ReplaceInPlace(val, "August", "Aug");
            NStr::ReplaceInPlace(val, "September", "Sep");
            NStr::ReplaceInPlace(val, "October", "Oct");
            NStr::ReplaceInPlace(val, "November", "Nov");
            NStr::ReplaceInPlace(val, "December", "Dec");
        }
    }
    return val;
}


static string s_GetKeywordForStructuredComment (const CUser_object& obj)
{
    string prefix = CComment_rule::GetStructuredCommentPrefix(obj);
    if (NStr::IsBlank(prefix)) {
        return "";
    }
    string keyword = CComment_rule::KeywordForPrefix(prefix);
    return keyword;
}


void CValidError_bioseq::CheckForMultipleStructuredComments(const CBioseq& seq)
{
    // list of structured comment prefixes
    vector<string> sc_prefixes;

    CSeqdesc_CI di(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_User);
    while (di) {
        const CUser_object& obj = di->GetUser();
        if (CComment_rule::IsStructuredComment(obj)) {
            string prefix = CComment_rule::GetStructuredCommentPrefix(obj);
            if (!NStr::IsBlank(prefix)) {
                sc_prefixes.push_back(prefix);
            }
        }
        ++di;
    }

    sort(sc_prefixes.begin(), sc_prefixes.end());
    int num_seen = 0;
    string previous = "";
    ITERATE(vector<string>, it, sc_prefixes) {
        if (NStr::EqualNocase(previous, *it)) {
            num_seen++;
        } else {
            if (num_seen > 1) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleComments, 
                        "Multiple structured comments with prefix " + previous,
                        seq);
            }
            previous = *it;
            num_seen = 1;
        }
    }
    if (num_seen > 1) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleComments, 
                "Multiple structured comments with prefix " + previous,
                seq);
    } 

}


// Validate CSeqdesc within the context of a bioseq. 
// See: CValidError_desc for validation of standalone CSeqdesc,
// and CValidError_descr for validation of descriptors in the context
// of descriptor list.
void CValidError_bioseq::ValidateSeqDescContext(const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    size_t  num_gb   = 0,
            num_embl = 0,
            num_pir  = 0,
            num_pdb  = 0,
            num_prf  = 0,
            num_sp   = 0;
    CConstRef<CSeqdesc> last_gb, 
                        last_embl,
                        last_pir,
                        last_pdb,
                        last_prf,
                        last_sp;
    CConstRef<CSeqdesc> create_desc, update_desc;
    string create_str;
    int biomol = -1;
      int tech = -1, completeness = -1;
    CConstRef<COrg_ref> org;

      string name_str = "";
      string comment_str = "";

    bool has_chromosome = false;
    bool is_prokaryote = false;
    bool is_organelle = false;

    bool is_genome_assembly = false;
    bool is_assembly = false;
    bool is_finished_status = false;

    // some validation is for descriptors that affect a bioseq, 
    // other validation is only for descriptors _on_ a bioseq
    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
        const CSeqdesc& desc = **it;

        switch ( desc.Which() ) {
            case CSeqdesc::e_Title:
                  {
                      string title = desc.GetTitle();
                      size_t pos = NStr::Find(title, "[");
                      if (pos != string::npos) {
                          pos = NStr::Find(title, "=", pos + 1);
                      }
                      if (pos != string::npos) {
                          pos = NStr::Find(title, "]", pos + 1);
                      }
                      if (pos != string::npos) {
                          bool report_fasta_brackets = true;
                          FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
                              if ((*id_it)->IsGeneral()) {
                                  const CDbtag& dbtag = (*id_it)->GetGeneral();
                                  if (dbtag.IsSetDb()) {
                                      if (NStr::EqualNocase(dbtag.GetDb(), "TMSMART")
                                          || NStr::EqualNocase(dbtag.GetDb(), "BankIt")) {
                                          report_fasta_brackets = false;
                                          break;
                                      }
                                  }
                              }
                          }
                          if (report_fasta_brackets) {
                              IF_EXISTS_CLOSEST_BIOSOURCE (bs_ref, seq, NULL) {
                                  const CBioSource& bsrc = (*bs_ref).GetSource();
                                  if (bsrc.IsSetOrg()) {
                                      const COrg_ref& orgref = bsrc.GetOrg();
                                      if (orgref.IsSetTaxname()) {
                                          string taxname = orgref.GetTaxname();
                                          size_t pos = NStr::Find (taxname, "=");
                                          if (pos != string::npos) {
                                              pos = NStr::Find (title, taxname);
                                              if (pos != string::npos) {
                                                  report_fasta_brackets = false;
                                              }
                                          }
                                      }
                                  }
                              }
                          }
                          if (report_fasta_brackets) {
                              PostErr(eDiag_Warning, eErr_SEQ_DESCR_FastaBracketTitle, 
                                      "Title may have unparsed [...=...] construct",
                                      ctx, desc);
                          }
                      }
            }
            break;
        default:
            break;
        }
    }

    // collect keywords - needed for validating structured comments
    vector<string> keywords;
    for ( CSeqdesc_CI di(m_CurrentHandle, CSeqdesc::e_Genbank); di; ++ di ) {
        FOR_EACH_KEYWORD_ON_GENBANKBLOCK (key, di->GetGenbank()) {
            keywords.push_back (*key);
        }
    }

    for ( CSeqdesc_CI di(m_CurrentHandle); di; ++ di ) {
        const CSeqdesc& desc = *di;

        switch ( desc.Which() ) {

        case CSeqdesc::e_Org:
            if ( !org ) {
                org = &(desc.GetOrg());
            }
            ValidateOrgContext(di, desc.GetOrg(), *org, seq, desc);
            break;

        case CSeqdesc::e_Pir:
            num_pir++;
            last_pir = &desc;
            break;

        case CSeqdesc::e_Genbank:
            num_gb++;
            last_gb = &desc;
                  ValidateGBBlock (desc.GetGenbank(), seq, desc);
            break;

        case CSeqdesc::e_Sp:
            num_sp++;
            last_sp = &desc;
            break;

        case CSeqdesc::e_Embl:
            num_embl++;
            last_embl = &desc;
            break;

        case CSeqdesc::e_Create_date:
            {
            const CDate& current = desc.GetCreate_date();
            if ( create_desc ) {
                if ( create_desc->GetCreate_date().Compare(current) != CDate::eCompare_same ) {
                    string current_str = GetDateString(current);
                    const CSeq_entry *use_ctx = ctx.GetParentEntry();
                    if (use_ctx == NULL || !use_ctx->IsSet() 
                        || !use_ctx->GetSet().IsSetClass()
                        || use_ctx->GetSet().GetClass() != CBioseq_set::eClass_nuc_prot) {
                        use_ctx = &ctx;
                    }
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentDates,
                        "Inconsistent create_dates [" + current_str +
                        "] and [" + create_str + "]", *use_ctx, desc);
                }
            } else {
                create_desc = &desc;
                create_str = GetDateString(create_desc->GetCreate_date());
            }

            if ( update_desc ) {
                ValidateUpdateDateContext(update_desc->GetUpdate_date(), current, seq, *create_desc);
            }
            }
            break;

        case CSeqdesc::e_Update_date:
            if ( create_desc ) {
                ValidateUpdateDateContext(desc.GetUpdate_date(), create_desc->GetCreate_date(), 
                    seq, *create_desc);
            } else {
                update_desc = &desc;
            }
            break;

        case CSeqdesc::e_Prf:
            num_prf++;
            last_prf = &desc;
            break;

        case CSeqdesc::e_Pdb:
            num_pdb++;
            last_pdb = &desc;
            break;

        case CSeqdesc::e_Source:
            {
                const CSeqdesc::TSource& source = desc.GetSource();

                        m_Imp.ValidateBioSourceForSeq (source, desc, &ctx, m_CurrentHandle);

                        // look at orgref in comparison to other descs
                        if (source.IsSetOrg()) {
                            const COrg_ref& orgref = source.GetOrg();
                            if ( !org ) {
                                org = &orgref;
                            }
                            ValidateOrgContext(di, orgref, 
                                                *org, seq, desc);
                        }

                // look for chromosome, prokaryote
                  FOR_EACH_SUBSOURCE_ON_BIOSOURCE (it, source) {
                    if ((*it)->IsSetSubtype()
                        && (*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                        has_chromosome = true;
                        break;
                    }
                }
                if (source.IsSetLineage()) {
                    string lineage = source.GetLineage();
                    if (NStr::StartsWith(lineage, "Viruses; ")
                        || NStr::StartsWith(lineage, "Bacteria; ")
                        || NStr::StartsWith(lineage, "Archaea; ")) {
                        is_prokaryote = true;
                    }
                }
                if (source.IsSetDivision()) {
                    string div = source.GetDivision();
                    if (NStr::Equal(div, "BCT") || NStr::Equal(div, "VRL")) {
                        is_prokaryote = true;
                    }
                }
                
                // check for organelle
                if (source.IsSetGenome()) {
                    is_organelle = m_Imp.IsOrganelle(source.GetGenome());
                }

            }
            break;

        case CSeqdesc::e_Molinfo:
            ValidateMolInfoContext(desc.GetMolinfo(), biomol, tech, completeness, seq, desc);
            break;

        case CSeqdesc::e_User:
            if (desc.GetUser().IsSetType()) {
                const CUser_object& usr = desc.GetUser();
                const CObject_id& oi = usr.GetType();
                if (oi.IsStr() && NStr::CompareNocase(oi.GetStr(), "TpaAssembly") == 0 
                    && !s_IsTPAAssemblyOkForBioseq(seq)) {
                    string id_str;
                    seq.GetLabel(&id_str, CBioseq::eContent, false);
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                        "Non-TPA record " + id_str + " should not have TpaAssembly object", seq);
                }
                if (IsRefGeneTrackingObject(desc.GetUser())
                    && !CValidError_imp::IsWGSIntermediate(seq) 
                    && !m_Imp.IsRefSeq()) {
                              PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingOnNonRefSeq, 
                                      "RefGeneTracking object should only be in RefSeq record", 
                                      ctx, desc);
                } else if (oi.IsStr() && NStr::EqualCase(oi.GetStr(), "StructuredComment")) {
                    const CUser_object& obj = desc.GetUser();
                    string keyword = s_GetKeywordForStructuredComment(obj);
                    if (!NStr::IsBlank(keyword)) {
                        // does sequence have keyword?
                        bool found = false;
                        ITERATE (vector<string>, key, keywords) {
                            if (NStr::EqualNocase(keyword, *key)) {
                                found = true;
                                break;
                            }
                        }
                        // is structured comment valid for this keyword?
                        if (m_DescrValidator.ValidateStructuredComment(desc.GetUser(), desc, false)) {
                            // needs to have keyword
                            if (!found) {
                                /*
                                PostErr(eDiag_Info, eErr_SEQ_DESCR_MissingKeyword, 
                                        "Structured Comment compliant, keyword should be added", ctx, desc);
                                */
                            }
                        } else {
                            // error if keyword is present
                            if (found) {
                                PostErr(eDiag_Info, eErr_SEQ_DESCR_BadKeyword, 
                                        "Structured Comment is non-compliant, keyword should be removed", ctx, desc);
                            }
                        }
                    } else {
                        ITERATE (CUser_object::TData, field, obj.GetData()) {
                            if ((*field)->IsSetLabel() && (*field)->GetLabel().IsStr()) {
                                if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "StructuredCommentPrefix")) {
                                    const string& prefix = (*field)->GetData().GetStr();
                                    if (NStr::EqualCase(prefix, "##Genome-Assembly-Data-START##")) {
                                        is_genome_assembly = true;
                                    } else if (NStr::EqualCase(prefix, "##Assembly-Data-START##")) {
                                        is_assembly = true;
                                    }
                                } else if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "Current Finishing Status")) {
                                    const string& prefix = (*field)->GetData().GetStr();
                                    if (NStr::EqualCase(prefix, "Finished")) {
                                        is_finished_status = true;
                                    }
                                }
                            }
                        }
                    }
                    x_ValidateStructuredCommentContext(desc, seq);
                } else if (oi.IsStr() && NStr::EqualCase(oi.GetStr(), "DBLink")) {
                    m_dblink_count++;
                    FOR_EACH_USERFIELD_ON_USEROBJECT (ufd_it, usr) {
                        const CUser_field& fld = **ufd_it;
                        if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                            const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                            if (NStr::EqualNocase(label_str, "Trace Assembly Archive")) {
                                m_taa_count++;
                            } else if (NStr::EqualNocase(label_str, "BioSample")) {
                                m_bs_count++;
                            } else if (NStr::EqualNocase(label_str, "Assembly")) {
                                m_as_count++;
                            } else if (NStr::EqualNocase(label_str, "ProbeDB")) {
                                m_pdb_count++;
                            } else if (NStr::EqualNocase(label_str, "Sequence Read Archive")) {
                                m_sra_count++;
                            } else if (NStr::EqualNocase(label_str, "BioProject")) {
                                m_bp_count++;
                            } else {
                                m_unknown_count++;
                            }
                        }
                    }
                }
            }
            break;
        case CSeqdesc::e_Title:
            {
                string title = desc.GetTitle();

                // nucleotide refseq sequences should start with the organism name,
                // protein refseq sequences should end with the organism name.
                bool is_refseq = false;
                FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
                    if ((*id_it)->IsOther()) {
                        is_refseq = true;
                        break;
                    }
                }
                if (is_refseq) {
                    string taxname = "";

                    CSeqdesc_CI src_i(m_CurrentHandle, CSeqdesc::e_Source);
                    if (src_i && src_i->GetSource().IsSetOrg() && src_i->GetSource().GetOrg().IsSetTaxname()) {
                        taxname = src_i->GetSource().GetOrg().GetTaxname();
                    }
                    if (seq.IsAa()) {
                        CConstRef<CSeq_feat> cds = m_Imp.GetCDSGivenProduct(seq);
                        if (cds) {
                            CConstRef<CSeq_feat> src_f = GetBestOverlappingFeat(
                                cds->GetLocation(),
                                CSeqFeatData::eSubtype_biosrc,
                                eOverlap_Contained,
                                *m_Scope);
                            if (src_f && src_f->IsSetData() && src_f->GetData().IsBiosrc()
                                && src_f->GetData().GetBiosrc().IsSetTaxname()) {
                                taxname = src_f->GetData().GetBiosrc().GetTaxname();
                            }
                        }
                    }
                    if (!NStr::IsBlank(taxname) && !SeqIsPatent (seq)) {
                        if (NStr::StartsWith(title, "PREDICTED: ")) {
                            title = title.substr (11);
                        }
                        if (seq.IsNa()) {
                            if (!NStr::StartsWith(title, taxname, NStr::eNocase)) {
                                PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrganismInTitle, 
                                        "RefSeq nucleotide title does not start with organism name",
                                        ctx, desc);
                            }
                        } else if (seq.IsAa()) {
                            taxname = "[" + taxname + "]";
                            if (!NStr::EndsWith(title, taxname, NStr::eNocase)) {
                                PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrganismInTitle, 
                                        "RefSeq protein title does not end with organism name",
                                        ctx, desc);
                            }
                        }
                    }
                }
            }
            break;

        case CSeqdesc::e_Name:
            name_str = desc.GetName();
            if (!NStr::IsBlank(name_str)) {                
                CSeqdesc_CI di2 = di;
                ++di2;
                while (di2) {
                    if (di2->IsName()) {
                        if (NStr::EqualNocase(name_str, di2->GetName())) {
                            PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleNames, 
                                    "Undesired multiple name descriptors, identical text",
                                    ctx, desc);
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleNames, 
                                    "Undesired multiple name descriptors, different text",
                                    ctx, desc);
                        }
                    }
                    ++di2;
                }
            }
            break;

        case CSeqdesc::e_Comment:
            if (NStr::IsBlank(comment_str)) {
                comment_str = desc.GetComment();
            } else if (NStr::EqualNocase(comment_str, desc.GetComment())) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleComments, 
                        "Undesired multiple comment descriptors, identical text",
                        ctx, desc);
            }
            break;

        case CSeqdesc::e_Method:
            if (!seq.IsAa()) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                        "Nucleic acid with protein sequence method",
                        ctx, desc);
            }
            break;

        default:
            break;
        }
    }

    if (!has_chromosome && !is_prokaryote && !is_organelle && !SeqIsPatent (seq)) {
        bool is_nc = false;
        bool is_ac = false;
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
            if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()) {
                string accession = (*id_it)->GetOther().GetAccession();
                if (NStr::StartsWith(accession, "NC_")) {
                    is_nc = true;
                    break;
                } else if (NStr::StartsWith(accession, "AC_")) {
                    is_ac = true;
                    break;
                }
            }
        }
        if ((is_nc || is_ac) && ! is_prokaryote && ! is_organelle && ! has_chromosome) {
            PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingChromosome, "Missing chromosome qualifier on NC or AC RefSeq record",
                     seq);
        }
    }

    if (is_genome_assembly && is_finished_status && tech == CMolInfo::eTech_wgs) {
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_FinishedStatusForWGS, "WGS record %s should not have Finished status", seq);
    }

    bool wgs_master = false;
    bool tsa_master = false;
    EDiagSev sev =  eDiag_Error;
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh ) {
        FOR_EACH_SEQID_ON_BIOSEQ_HANDLE (sid_itr, bsh) {
            CSeq_id_Handle sid = *sid_itr;
            switch (sid.Which()) {
                case NCBI_SEQID(Embl):
                case NCBI_SEQID(Ddbj):
                {
                    sev = eDiag_Warning;
                    // flow through
                }
                case NCBI_SEQID(Other):
                case NCBI_SEQID(Genbank):
                {
                    CConstRef<CSeq_id> id = sid.GetSeqId();
                    const CTextseq_id& tsid = *id->GetTextseq_Id ();
                    if (tsid.IsSetAccession()) {
                        const string& acc = tsid.GetAccession ();
                        TACCN_CHOICE type = CSeq_id::IdentifyAccession (acc);
                        TACCN_CHOICE div = (TACCN_CHOICE) (type & NCBI_ACCN(division_mask));
                        if ( div == NCBI_ACCN(wgs) ) 
                        {
                            if( (type & CSeq_id::fAcc_master) != 0 ) {
                                wgs_master = true;
                            }
                        }
                        if ( div == NCBI_ACCN(tsa) )
                        {
                            if( (type & CSeq_id::fAcc_master) != 0 ) {
                                tsa_master = true;
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    if (wgs_master && ! is_genome_assembly) {
       PostErr (sev, eErr_SEQ_INST_WGSMasterLacksStrucComm, "WGS master without Genome Assembly Data user object", seq);
    }
    if (tsa_master && ! is_assembly) {
       PostErr (sev, eErr_SEQ_INST_TSAMasterLacksStrucComm, "TSA master without Assembly Data user object", seq);
    }

    if ( num_gb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple GenBank blocks", ctx, *last_gb);
    }

    if ( num_embl > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple EMBL blocks", ctx, *last_embl);
    }

    if ( num_pir > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PIR blocks", ctx, *last_pir);
    }

    if ( num_pdb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PDB blocks", ctx, *last_pdb);
    }

    if ( num_prf > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PRF blocks", ctx, *last_prf);
    }

    if ( num_sp > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple SWISS-PROT blocks", ctx, *last_sp);
    }

    ValidateModifDescriptors (seq);
    ValidateMoltypeDescriptors (seq);

    CheckForMultipleStructuredComments(seq);
}


void CValidError_bioseq::x_ValidateStructuredCommentContext(const CSeqdesc& desc, const CBioseq& seq)
{
    if (!desc.IsUser() || !desc.GetUser().IsSetType() || !desc.GetUser().GetType().IsStr()
        || !NStr::EqualCase(desc.GetUser().GetType().GetStr(), "StructuredComment")) {
        return;
    }

    // Is a Barcode index number present?
    ITERATE (CUser_object::TData, field, desc.GetUser().GetData()) {
        if ((*field)->IsSetLabel() && (*field)->GetLabel().IsStr()
            && NStr::Equal((*field)->GetLabel().GetStr(), "Barcode Index Number")
            && (*field)->IsSetData() && (*field)->GetData().IsStr()) {
            string bin = (*field)->GetData().GetStr();

            // only check if name contains "sp." or "bacterium", and also BOLD
            CSeqdesc_CI di(m_CurrentHandle, CSeqdesc::e_Source);
            if (di && di->GetSource().IsSetTaxname()) {
                string taxname = di->GetSource().GetTaxname();
                if (NStr::Find(taxname, "BOLD") != string::npos
                    && (NStr::Find(taxname, "sp. ") != string::npos
                        || NStr::Find(taxname, "bacterium ") != string::npos)
                    && !NStr::EndsWith(taxname, " " + bin)) {
                    const CSeq_entry& ctx = *seq.GetParentEntry();
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_BadStrucCommInvalidFieldValue,
                        "Organism name should end with sp. plus Barcode Index Number (" + bin + ")",
                        ctx, desc);
                }
            }
        }
    }
}


void CValidError_bioseq::ValidateGBBlock
(const CGB_block& gbblock,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    bool has_tpa_inf = false, has_tpa_exp = false;
    FOR_EACH_KEYWORD_ON_GENBANKBLOCK (it, gbblock) {
        if (NStr::EqualNocase (*it, "TPA:experimental")) {
            has_tpa_exp = true;
        } else if (NStr::EqualNocase (*it, "TPA:inferential")) {
            has_tpa_inf = true;
        }
    }
    if (has_tpa_inf && has_tpa_exp) {
        PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                 "TPA:experimental and TPA:inferential should not both be in the same set of keywords",
                 ctx, desc);
    }
}


void CValidError_bioseq::ValidateMolInfoContext
(const CMolInfo& minfo,
 int& seq_biomol,
 int& last_tech, 
 int& last_completeness,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    bool is_synthetic_construct = false;
    bool is_artificial = false;

    CSeqdesc_CI src_di(m_CurrentHandle, CSeqdesc::e_Source);
    while (src_di && !is_synthetic_construct) {
        is_synthetic_construct = m_Imp.IsSyntheticConstruct(src_di->GetSource());
        is_artificial = m_Imp.IsArtificial(src_di->GetSource());
        ++src_di;
    }

    if ( minfo.IsSetBiomol() ) {
        int biomol = minfo.GetBiomol();
        if ( seq_biomol < 0 ) {
            seq_biomol = biomol;
        }
        
        switch ( biomol ) {
        case CMolInfo::eBiomol_peptide:
            if ( seq.IsNa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Nucleic acid with Molinfo-biomol = peptide", ctx, desc);
            }
            break;

        case CMolInfo::eBiomol_other_genetic:
            if ( !is_artificial ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol = other genetic", ctx, desc);
            }
            break;
            
        case CMolInfo::eBiomol_other:
            if ( !m_Imp.IsXR() ) {
                if ( !IsSynthetic(seq) ) {
                    if ( !x_IsMicroRNA(seq)) {
                        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                            "Molinfo-biomol other used", ctx, desc);
                    }
                }
            }
            break;
            
        default:  // the rest are nucleic acid
            if ( seq.IsAa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol [" + NStr::IntToString(biomol) +
                    "] used on protein", ctx, desc);
            } else {
                if ( biomol != seq_biomol ) {
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent Molinfo-biomol [" + 
                        NStr::IntToString(seq_biomol) + "] and [" +
                        NStr::IntToString(biomol) + "]", ctx, desc);
                }
            }
        }
        // look for double-stranded mRNA
        if (biomol == CMolInfo::eBiomol_mRNA
            && seq.IsNa() 
            && seq.IsSetInst() && seq.GetInst().IsSetStrand()
            && seq.GetInst().GetStrand() != CSeq_inst::eStrand_not_set
            && seq.GetInst().GetStrand() != CSeq_inst::eStrand_ss) {
            PostErr(eDiag_Error, eErr_SEQ_INST_DSmRNA, 
                    "mRNA not single stranded", seq);
        }
    } else {
        if (is_synthetic_construct && !seq.IsAa()) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_InvalidForType, "synthetic construct should have other-genetic", ctx, desc);
        }
    }


    if ( minfo.IsSetTech() ) {        
        int tech = minfo.GetTech();

        if ( seq.IsNa() ) {
            switch ( tech ) {
            case CMolInfo::eTech_concept_trans:
            case CMolInfo::eTech_seq_pept:
            case CMolInfo::eTech_both:
            case CMolInfo::eTech_seq_pept_overlap:
            case CMolInfo::eTech_seq_pept_homol:
            case CMolInfo::eTech_concept_trans_a:
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Nucleic acid with protein sequence method", ctx, desc);
                break;
            default:
                break;
            }
        } else {
            switch ( tech ) {
            case CMolInfo::eTech_est:
            case CMolInfo::eTech_sts:
            case CMolInfo::eTech_genemap:
            case CMolInfo::eTech_physmap:
            case CMolInfo::eTech_htgs_1:
            case CMolInfo::eTech_htgs_2:
            case CMolInfo::eTech_htgs_3:
            case CMolInfo::eTech_fli_cdna:
            case CMolInfo::eTech_htgs_0:
            case CMolInfo::eTech_htc:
            case CMolInfo::eTech_wgs:
            case CMolInfo::eTech_barcode:
            case CMolInfo::eTech_composite_wgs_htgs:
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Protein with nucleic acid sequence method", ctx, desc);
                break;
            default:
                break;
            }
        }

        switch ( tech) {
        case CMolInfo::eTech_sts:
        case CMolInfo::eTech_survey:
        case CMolInfo::eTech_wgs:
        case CMolInfo::eTech_htgs_0:
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_htgs_3:
        case CMolInfo::eTech_composite_wgs_htgs:
            if (tech == CMolInfo::eTech_sts  &&
                seq.GetInst().GetMol() == CSeq_inst::eMol_rna  &&
                minfo.IsSetBiomol()  &&
                minfo.GetBiomol() == CMolInfo::eBiomol_mRNA) {
                // !!!
                // Ok, there are some STS sequences derived from 
                // cDNAs, so do not report these
            } else if (minfo.IsSetBiomol()  
                       && minfo.GetBiomol() != CMolInfo::eBiomol_genomic) {
                PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                    "HTGS/STS/GSS/WGS sequence should be genomic", seq);
            } else if (seq.GetInst().GetMol() != CSeq_inst::eMol_dna  &&
                seq.GetInst().GetMol() != CSeq_inst::eMol_na) {
                PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                    "HTGS/STS/GSS/WGS sequence should not be RNA", seq);
            } 
            break;
        case CMolInfo::eTech_est:
            if (tech == CMolInfo::eTech_est  &&
                 ((! minfo.IsSetBiomol())  ||
                minfo.GetBiomol() != CMolInfo::eBiomol_mRNA)) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_ConflictingBiomolTech,
                    "EST sequence should be mRNA", seq);
            }
            break;
        case CMolInfo::eTech_tsa:
            if (seq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_ConflictingBiomolTech,
                    "TSA sequence should not be DNA", seq);
            }
            break;
        default:
            break;
        }

        if (tech == CMolInfo::eTech_htgs_3) {
            CSeqdesc_CI gb_i(m_CurrentHandle, CSeqdesc::e_Genbank);
            bool has_draft = false;
            bool has_prefin = false;
            bool has_activefin = false;
            bool has_fulltop = false;
            while (gb_i) {
                if (gb_i->GetGenbank().IsSetKeywords()) {
                    CGB_block::TKeywords::const_iterator key_it = gb_i->GetGenbank().GetKeywords().begin();
                    while (key_it != gb_i->GetGenbank().GetKeywords().end()) {
                        if (NStr::EqualNocase(*key_it, "HTGS_DRAFT")) {
                            has_draft = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_PREFIN")) {
                            has_prefin = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_ACTIVEFIN")) {
                            has_activefin = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_FULLTOP")) {
                            has_fulltop = true;
                        }
                        ++key_it;
                    }
                }
                ++gb_i;
            }
            if (has_draft) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_DRAFT keyword", seq);
            }
            if (has_prefin) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_PREFIN keyword", seq);
            }
            if (has_activefin) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_ACTIVEFIN keyword", seq);
            }
            if (has_fulltop) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_FULLTOP keyword", seq);
            }
        }                        

        if (last_tech > 0) {
            if (last_tech != tech) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                    "Inconsistent Molinfo-tech [" + NStr::IntToString (last_tech)
                    + "] and [" + NStr::IntToString(tech) + "]", ctx, desc);
            }
        } else {
            last_tech = tech;
        }
    } else {
        if (last_tech > -1) {
            if (last_tech != 0) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                    "Inconsistent Molinfo-tech [" + NStr::IntToString (last_tech)
                    + "] and [0]", ctx, desc);
            }
        } else {
            last_tech = 0;
        }
    }

      if (minfo.IsSetCompleteness()) {
            if (last_completeness > 0) {
                  if (last_completeness != minfo.GetCompleteness()) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                              "Inconsistent Molinfo-completeness [" + NStr::IntToString (last_completeness)
                              + "] and [" + NStr::IntToString(minfo.GetCompleteness()) + "]", ctx, desc);
                  }
            } else {
                  last_completeness = minfo.GetCompleteness();
            }
    } else {
        if (last_completeness > -1) {
            if (last_completeness != 0) {
                  PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                          "Inconsistent Molinfo-completeness [" + NStr::IntToString (last_completeness)
                          + "] and [0]", ctx, desc);
                  }
            } else {
                  last_completeness = 0;
            }
    }
    // need to look at closest molinfo descriptor, not all of them
    CConstRef<CSeqdesc> closest_molinfo = seq.GetClosestDescriptor(CSeqdesc::e_Molinfo);
    if (closest_molinfo) {
        const CMolInfo& molinfo = closest_molinfo->GetMolinfo();
        if (molinfo.IsSetCompleteness()) {
            x_ValidateCompletness(seq, molinfo);
        }
    }

}


void CValidError_bioseq::ReportModifInconsistentError (int new_mod, int& old_mod, const CSeqdesc& desc, const CSeq_entry& ctx)
{
    if (old_mod >= 0) {
        if (new_mod != old_mod) {
            PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                     "Inconsistent GIBB-mod [" + NStr::IntToString (old_mod) + "] and ["
                     + NStr::IntToString (new_mod) + "]", ctx, desc);
        }
    } else {
        old_mod = new_mod;
    }
}


void CValidError_bioseq::ValidateModifDescriptors (const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    int last_na_mod = -1;
    int last_organelle = -1;
    int last_partialness = -1;
    int last_left_right = -1;

    CSeqdesc_CI desc_ci(m_CurrentHandle, CSeqdesc::e_Modif);
    while (desc_ci) {
      CSeqdesc::TModif modif = desc_ci->GetModif();
        CSeqdesc::TModif::const_iterator it = modif.begin();
        while (it != modif.end()) {
            int modval = *it;
            switch (modval) {
                case eGIBB_mod_dna:
                case eGIBB_mod_rna:
                    if (m_CurrentHandle && m_CurrentHandle.IsAa()) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForType, 
                                 "Nucleic acid GIBB-mod [" + NStr::IntToString (modval) + "] on protein",
                                 ctx, *desc_ci);
                    } else {
                        ReportModifInconsistentError (modval, last_na_mod, *desc_ci, ctx);
                    }
                    break;
                case eGIBB_mod_mitochondrial:
                case eGIBB_mod_chloroplast:
                case eGIBB_mod_kinetoplast:
                case eGIBB_mod_cyanelle:
                case eGIBB_mod_macronuclear:
                    ReportModifInconsistentError (modval, last_organelle, *desc_ci, ctx);
                    break;
                case eGIBB_mod_partial:
                case eGIBB_mod_complete:
                    ReportModifInconsistentError (modval, last_partialness, *desc_ci, ctx);
                    if (last_left_right >= 0 && modval == eGIBB_mod_complete) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                                 "Inconsistent GIBB-mod [" + NStr::IntToString (last_left_right) + "] and ["
                                 + NStr::IntToString (modval) + "]",
                                 ctx, *desc_ci);
                    }
                    break;
                case eGIBB_mod_no_left:
                case eGIBB_mod_no_right:
                    if (last_partialness == eGIBB_mod_complete) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                                 "Inconsistent GIBB-mod [" + NStr::IntToString (last_partialness) + "] and ["
                                 + NStr::IntToString (modval) + "]",
                                 ctx, *desc_ci);
                    }
                    last_left_right = modval;
                    break;
                default:
                    break;

            }
            ++it;
        }
        ++desc_ci;
    }
}


void CValidError_bioseq::ValidateMoltypeDescriptors (const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    int last_na_mol = 0;
    CSeqdesc_CI desc_ci(m_CurrentHandle, CSeqdesc::e_Mol_type);
    while (desc_ci) {
        int modval = desc_ci->GetMol_type();
        switch (modval) {
            case eGIBB_mol_peptide:
                if (!seq.IsAa()) {
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForType, 
                             "Nucleic acid with GIBB-mol = peptide",
                             ctx, *desc_ci);
                }
                break;
            case eGIBB_mol_unknown:
            case eGIBB_mol_other:
                PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForType, 
                         "GIBB-mol unknown or other used",
                         ctx, *desc_ci);
                break;
            default:                   // the rest are nucleic acid
                if (seq.IsAa()) {
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForType, 
                             "GIBB-mol [" + NStr::IntToString (modval) + "] used on protein",
                             ctx, *desc_ci);
                } else {
                    if (last_na_mol) {
                        if (last_na_mol != modval) {
                            PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                              "Inconsistent GIBB-mol [" + NStr::IntToString (last_na_mol)
                              + "] and [" + NStr::IntToString (modval) + "]",
                              ctx, *desc_ci);
                        }
                    } else {
                        last_na_mol = modval;
                    }
                }
                break;
        }
        ++desc_ci;
    }
}


bool CValidError_bioseq::IsSynthetic(const CBioseq& seq) const
{
    if ( m_CurrentHandle ) {
        CSeqdesc_CI sd(m_CurrentHandle, CSeqdesc::e_Source);
        if ( sd ) {
            const CSeqdesc::TSource& source = sd->GetSource();
            if ( source.CanGetOrigin()  &&
                 source.GetOrigin() == CBioSource::eOrigin_synthetic ) {
                return true;
            }
            if ( source.CanGetOrg()  &&  source.GetOrg().CanGetOrgname() ) {
                const COrgName& org_name = source.GetOrg().GetOrgname();
                if ( org_name.CanGetDiv() ) {
                    if ( NStr::CompareNocase(org_name.GetDiv(), "SYN") == 0 ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


bool CValidError_bioseq::x_IsMicroRNA(const CBioseq& seq) const 
{
    SAnnotSelector selector(CSeqFeatData::e_Rna);
    selector.SetFeatSubtype(CSeqFeatData::eSubtype_otherRNA);
    CFeat_CI fi(m_CurrentHandle, selector);

    for ( ; fi; ++fi ) {
        const CRNA_ref& rna_ref = fi->GetData().GetRna();
        if ( rna_ref.IsSetExt()  &&  rna_ref.GetExt().IsName() ) {
            if ( NStr::Find(rna_ref.GetExt().GetName(), "microRNA") != NPOS ) {
                return true;
            }
        }
    }
    return false;
}


void CValidError_bioseq::ValidateUpdateDateContext
(const CDate& update,
 const CDate& create,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( update.Compare(create) == CDate::eCompare_before ) {
        string create_str = GetDateString(create);
        string update_str = GetDateString(update);

        CSeq_entry *ctx = seq.GetParentEntry();
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentDates,
            "Inconsistent create_date [" + create_str + 
            "] and update_date [" + update_str + "]",
            *ctx, desc);
    }
}


void CValidError_bioseq::ValidateOrgContext
(const CSeqdesc_CI& curr,
 const COrg_ref& this_org,
 const COrg_ref& org,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( this_org.IsSetTaxname()  &&  org.IsSetTaxname() ) {
        if ( this_org.GetTaxname() != org.GetTaxname() ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                "Inconsistent taxnames [" + this_org.GetTaxname() + 
                "] and [" + org.GetTaxname() + "]",
                *seq.GetParentEntry(), desc);
        }
    }
}


static bool s_ReportableCollision (const CGene_ref& g1, const CGene_ref& g2)
{
    if (g1.IsSetLocus() && !NStr::IsBlank(g1.GetLocus())
        && g2.IsSetLocus() && !NStr::IsBlank(g2.GetLocus())
        && NStr::EqualNocase(g1.GetLocus(), g2.GetLocus())) {
        return true;
    } else if (g1.IsSetLocus_tag() && !NStr::IsBlank(g1.GetLocus_tag())
        && g2.IsSetLocus_tag() && !NStr::IsBlank(g2.GetLocus_tag())
        && NStr::EqualNocase(g1.GetLocus_tag(), g2.GetLocus_tag())) {
        return true;
    } else if (g1.IsSetDesc() && !NStr::IsBlank(g1.GetDesc())
        && g2.IsSetDesc() && !NStr::IsBlank(g2.GetDesc())
        && NStr::EqualNocase(g1.GetDesc(), g2.GetDesc())) {
        return false;
    } else {
        return true;
    }
}


void CValidError_bioseq::x_CompareStrings
(const TStrFeatMap& str_feat_map,
 const string& type,
 EErrType err,
 EDiagSev sev)
{
    bool is_gene_locus = NStr::EqualNocase (type, "names");

    // iterate through multimap and compare strings
    bool first = true;
    bool reported_first = false;
    bool lastIsSplit = false;
    const string* strp = 0;
    const CSeq_feat* feat = 0;
    ITERATE (TStrFeatMap, it, str_feat_map) {
        if ( first ) {
            first = false;
            strp = &(it->first);
            feat = it->second;
            lastIsSplit = (bool) (feat->IsSetExcept() &&
                                  feat->IsSetExcept_text() &&
                                  NStr::FindNoCase (feat->GetExcept_text(), "gene split at ") != string::npos);
            continue;
        }

        string message = "";
        if (NStr::Equal (*strp, it->first)) {
            message = "Colliding " + type + " in gene features";
        } else if (NStr::EqualNocase (*strp, it->first)) {
            message = "Colliding " + type + " (with different capitalization) in gene features";
        }

        if (!NStr::IsBlank (message)
            && s_ReportableCollision(feat->GetData().GetGene(), it->second->GetData().GetGene())) {

            bool suppress_message = false;
            if (m_Imp.IsSmallGenomeSet()) {
                if (feat->IsSetExcept() && feat->IsSetExcept_text()
                    && NStr::FindNoCase (feat->GetExcept_text(), "trans-splicing") != string::npos &&
                    it->second->IsSetExcept() && it->second->IsSetExcept_text()
                    && NStr::FindNoCase (it->second->GetExcept_text(), "trans-splicing") != string::npos) {
                    suppress_message = true;
                }
            }

            if (suppress_message) {
                /* suppress for trans-spliced genes on small genome set */
            } else if (is_gene_locus && sequence::Compare(feat->GetLocation(),
                                                   (*it->second).GetLocation(),
                                                   m_Scope) == eSame) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_MultiplyAnnotatedGenes,
                         message + ", but feature locations are identical", *it->second);
            } else if (is_gene_locus 
                       && NStr::Equal (GetSequenceStringFromLoc(feat->GetLocation(), *m_Scope),
                                       GetSequenceStringFromLoc((*it->second).GetLocation(), *m_Scope))) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_ReplicatedGeneSequence,
                         message + ", but underlying sequences are identical", *it->second);
            } else { 
                const CSeq_feat* nfeat = it->second;
                bool isSplit = (bool) (nfeat->IsSetExcept() &&
                                      nfeat->IsSetExcept_text() &&
                                      NStr::FindNoCase (nfeat->GetExcept_text(), "gene split at ") != string::npos);
                if (is_gene_locus || (! isSplit) || (! lastIsSplit)) {
                    if (!reported_first) {
                        // for now, don't report first colliding gene - C Toolkit doesn't
                        // PostErr(sev, err, message, *feat);
                        reported_first = true;
                    }

                    PostErr(sev, err, message, *it->second);
                }
            }
        }
        strp = &(it->first);
        feat = it->second;
        lastIsSplit = (bool) (feat->IsSetExcept() &&
                              feat->IsSetExcept_text() &&
                              NStr::FindNoCase (feat->GetExcept_text(), "gene split at ") != string::npos);
    }
}


void CValidError_bioseq::ValidateCollidingGenes(const CBioseq& seq)
{
    try {
        TStrFeatMap label_map;
        TStrFeatMap locus_tag_map;
        TStrFeatMap locus_map;
        TStrFeatMap syn_map;

        // Loop through genes and insert into multimap sorted by
        // gene label / locus_tag -- case insensitive
        if (m_GeneIt) {
            CFeat_CI fi = *m_GeneIt;
            fi.Rewind();
            for ( ; fi; ++fi ) {
                const CSeq_feat& feat = fi->GetOriginalFeature();
                // record label
                string label;
                GetLabel(feat, &label, feature::fFGL_Content, m_Scope);
                label_map.insert(TStrFeatMap::value_type(label, &feat));
                // record locus_tag
                const CGene_ref& gene = feat.GetData().GetGene();
                if ( gene.IsSetLocus_tag()  &&  !NStr::IsBlank(gene.GetLocus_tag()) ) {
                    locus_tag_map.insert(TStrFeatMap::value_type(gene.GetLocus_tag(), &feat));
                }
                // record locus
                if ( gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())) {
                    locus_map.insert(TStrFeatMap::value_type(gene.GetLocus(), &feat));
                }
                // record synonyms
                FOR_EACH_SYNONYM_ON_GENEREF (syn_it, gene) {
                    syn_map.insert(TStrFeatMap::value_type((*syn_it), &feat));
                }
            }
            x_CompareStrings(label_map, "names", eErr_SEQ_FEAT_CollidingGeneNames,
                eDiag_Warning);
            x_CompareStrings(locus_tag_map, "locus_tags", eErr_SEQ_FEAT_CollidingLocusTags,
                eDiag_Error);
            // look for synonyms on genes that match locus of different genes
            ITERATE (TStrFeatMap, syngene_it, syn_map) {
                TStrFeatMap::iterator gene_it = locus_map.find(syngene_it->first);
                if (gene_it != locus_map.end()) {
                    bool found = false;
                    FOR_EACH_SYNONYM_ON_GENEREF (syn_it, gene_it->second->GetData().GetGene()) {
                        if (NStr::Equal (*syn_it, syngene_it->first)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_IdenticalGeneSymbolAndSynonym, 
                                "gene synonym has same value (" + syngene_it->first + ") as locus of another gene feature",
                                *syngene_it->second);
                    }
                }
            }                
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exeption while validating colliding genes. EXCEPTION: ") +
                e.what(), seq);
        }
    }
}


void CValidError_bioseq::ValidateIDSetAgainstDb(const CBioseq& seq)
{
    const CSeq_id*  gb_id = 0;
    CRef<CSeq_id> db_gb_id;
    TGi             gi = ZERO_GI,
                    db_gi = ZERO_GI;
    const CDbtag*   general_id = 0,
                *   db_general_id = 0;

    FOR_EACH_SEQID_ON_BIOSEQ (id, seq) {
        switch ( (*id)->Which() ) {
        case CSeq_id::e_Genbank:
            gb_id = id->GetPointer();
            break;

        case CSeq_id::e_Gi:
            gi = (*id)->GetGi();
            break;

        case CSeq_id::e_General:
            general_id = &((*id)->GetGeneral());
            break;
    
        default:
            break;
        }
    }

    if ( gi == ZERO_GI  &&  gb_id != 0 ) {
        gi = GetGIForSeqId(*gb_id);
    }

    if ( gi <= ZERO_GI ) {
        return;
    }

    CScope::TIds id_set = GetSeqIdsForGI(gi);
    if ( !id_set.empty() ) {
        ITERATE( CScope::TIds, id, id_set ) {
            switch ( (*id).Which() ) {
            case CSeq_id::e_Genbank: 
                db_gb_id = CRef<CSeq_id> (new CSeq_id);
                db_gb_id->Assign (*(id->GetSeqId()));
                break;
                
            case CSeq_id::e_Gi:
                db_gi = (*id).GetGi();
                break;
                
            case CSeq_id::e_General:
                db_general_id = &((*id).GetSeqId()->GetGeneral());
                break;
                
            default:
                break;
            }
        }

        string gi_str = NStr::NumericToString(gi);

        if ( db_gi != gi ) {
          PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
              "New gi number (" + gi_str + ")" +
              " does not match one in NCBI sequence repository (" + NStr::NumericToString(db_gi) + ")", 
              seq);
        }
        if ( (gb_id != 0)  &&  (db_gb_id != 0) ) {
          if ( !gb_id->Match(*db_gb_id) ) {
              PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                  "New accession (" + gb_id->AsFastaString() + 
                  ") does not match one in NCBI sequence repository (" + db_gb_id->AsFastaString() + 
                  ") on gi (" + gi_str + ")", seq);
          }
        } else if ( gb_id != NULL) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of accession (" + gb_id->AsFastaString() + ") on gi (" +
                gi_str + ") compared to the NCBI sequence repository", seq);
        } else if ( db_gb_id != 0 ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of accession (" + db_gb_id->AsFastaString() + 
                ") on gi (" + gi_str + ") compared to the NCBI sequence repository", seq);
        }

        string new_gen_label, old_gen_label;
        if ( general_id  != 0  &&  db_general_id != 0 ) {
            if ( !general_id->Match(*db_general_id) ) {
                db_general_id->GetLabel(&old_gen_label);
                general_id->GetLabel(&new_gen_label);
                PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                    "New general ID (" + new_gen_label + 
                    ") does not match one in NCBI sequence repository (" + old_gen_label +
                    ") on gi (" + gi_str + ")", seq);
            }
        } else if ( general_id != 0 ) {
            general_id->GetLabel(&new_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of general ID (" + new_gen_label + ") on gi (" + 
                gi_str + ") compared to the NCBI sequence repository", seq);
        } else if ( db_general_id != 0 ) {
            db_general_id->GetLabel(&old_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of general ID (" + old_gen_label + ") on gi (" +
                gi_str + ") compared to the NCBI sequence repository", seq);
        }
    }
}


//look for Seq-graphs out of order
void CValidError_bioseq::ValidateGraphOrderOnBioseq (const CBioseq& seq, vector <CRef <CSeq_graph> > graph_list)
{
    if (graph_list.size() < 2) {
        return;
    }

    TSeqPos last_left = graph_list[0]->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos last_right = graph_list[0]->GetLoc().GetStop(eExtreme_Positional);

    for (size_t i = 1; i < graph_list.size(); i++) {
        TSeqPos left = graph_list[i]->GetLoc().GetStart(eExtreme_Positional);
        TSeqPos right = graph_list[i]->GetLoc().GetStop(eExtreme_Positional);

        if (left < last_left
            || (left == last_left && right < last_right)) {
            PostErr (eDiag_Warning, eErr_SEQ_GRAPH_GraphOutOfOrder, 
                     "Graph components are out of order - may be a software bug",
                     *graph_list[i]);
            return;
        }
        last_left = left;
        last_right = right;
    }
}


bool s_CompareTwoSeqGraphs(const CRef <CSeq_graph> g1,
                                    const CRef <CSeq_graph> g2)
{
    if (!g1->IsSetLoc()) {
        return true;
    } else if (!g2->IsSetLoc()) {
        return false;
    }

    TSeqPos start1 = g1->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos stop1 = g1->GetLoc().GetStop(eExtreme_Positional);
    TSeqPos start2 = g2->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos stop2 = g2->GetLoc().GetStop(eExtreme_Positional);
    
    if (start1 < start2) {
        return true;
    } else if (start1 == start2 && stop1 < stop2) {
        return true;
    } else {
        return false;
    }
}


void CValidError_bioseq::ValidateGraphsOnBioseq(const CBioseq& seq)
{
    if ( !seq.IsNa() || !seq.IsSetAnnot() ) {
        return;
    }

    vector <CRef <CSeq_graph> > graph_list;

    FOR_EACH_ANNOT_ON_BIOSEQ (it, seq) {
        if ((*it)->IsGraph()) {
            FOR_EACH_GRAPH_ON_ANNOT(git, **it) {
                if (IsSupportedGraphType(**git)) {
                    CRef <CSeq_graph> r(*git);
                    graph_list.push_back(r);
                }
            }
        }
    }

    if (graph_list.size() == 0) {
        return;
    }

    int     last_loc = -1;
    bool    overlaps = false;
    const CSeq_graph* overlap_graph = 0;
    SIZE_TYPE num_graphs = 0;
    SIZE_TYPE graphs_len = 0;

    const CSeq_inst& inst = seq.GetInst();  

    ValidateGraphOrderOnBioseq (seq, graph_list);

    // now sort, so that we can look for coverage
    sort (graph_list.begin(), graph_list.end(), s_CompareTwoSeqGraphs);

    SIZE_TYPE Ns_with_score = 0,
        gaps_with_score = 0,
        ACGTs_without_score = 0,
        vals_below_min = 0,
        vals_above_max = 0,
        num_bases = 0;

    int first_N = -1,
        first_ACGT = -1;

    for (vector<CRef <CSeq_graph> >::iterator grp = graph_list.begin(); grp != graph_list.end(); ++grp) {
        const CSeq_graph& graph = **grp;

        // Currently we support only byte graphs
        ValidateByteGraphOnBioseq(graph, seq);
        ValidateGraphValues(graph, seq, first_N, first_ACGT, num_bases, Ns_with_score, gaps_with_score, ACGTs_without_score, vals_below_min, vals_above_max);
        
        if (graph.IsSetLoc() && graph.GetLoc().Which() != CSeq_loc::e_not_set) {
            // Test for overlapping graphs
            const CSeq_loc& loc = graph.GetLoc();
            if ( (int)loc.GetTotalRange().GetFrom() <= last_loc ) {
                overlaps = true;
                overlap_graph = &graph;
            }
            last_loc = loc.GetTotalRange().GetTo();
        }

        graphs_len += graph.GetNumval();
        ++num_graphs;
    }

    if ( ACGTs_without_score > 0 ) {
        if (ACGTs_without_score * 10 > num_bases) {
            double pct = (double) (ACGTs_without_score) * 100.0 / (double) num_bases;
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphACGTScoreMany, 
                    NStr::SizetToString (ACGTs_without_score) + " ACGT bases ("
                    + NStr::DoubleToString (pct, 2) + "%) have zero score value - first one at position "
                    + NStr::IntToString (first_ACGT + 1),
                    seq);
        } else {            
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphACGTScore, 
                NStr::SizetToString(ACGTs_without_score) + 
                " ACGT bases have zero score value - first one at position " +
                NStr::IntToString(first_ACGT + 1), seq);
        }
    }
    if ( Ns_with_score > 0 ) {
        if (Ns_with_score * 10 > num_bases) {
            double pct = (double) (Ns_with_score) * 100.0 / (double) num_bases;
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphNScoreMany,
                    NStr::SizetToString(Ns_with_score) + " N bases ("
                    + NStr::DoubleToString(pct, 2) + "%) have positive score value - first one at position "
                    + NStr::IntToString(first_N + 1),
                    seq);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphNScore,
                NStr::SizetToString(Ns_with_score) +
                " N bases have positive score value - first one at position " + 
                NStr::IntToString(first_N + 1), seq);
        }
    }
    if ( gaps_with_score > 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphGapScore,
            NStr::SizetToString(gaps_with_score) + 
            " gap bases have positive score value", 
            seq);
    }
    if ( vals_below_min > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphBelow,
            NStr::SizetToString(vals_below_min) + 
            " quality scores have values below the reported minimum or 0", 
            seq);
    }
    if ( vals_above_max > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphAbove,
            NStr::SizetToString(vals_above_max) + 
            " quality scores have values above the reported maximum or 100", 
            seq);
    }

    if ( overlaps ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphOverlap,
            "Graph components overlap, with multiple scores for "
            "a single base", seq, *overlap_graph);
    }

    SIZE_TYPE seq_len = GetSeqLen(seq);
    if ( (seq_len != graphs_len)  &&  (inst.GetLength() != graphs_len) ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphBioseqLen,
            "SeqGraph (" + NStr::SizetToString(graphs_len) + ") and Bioseq (" +
            NStr::SizetToString(seq_len) + ") length mismatch", seq);
    }
    
    if ( inst.GetRepr() == CSeq_inst::eRepr_delta  &&  num_graphs > 1 ) {
        ValidateGraphOnDeltaBioseq(seq);
    }

}



void CValidError_bioseq::ValidateByteGraphOnBioseq
(const CSeq_graph& graph,
 const CBioseq& seq) 
{
    if ( !graph.GetGraph().IsByte() ) {
        return;
    }
    const CByte_graph& bg = graph.GetGraph().GetByte();
    
    // Test that min / max values are in the 0 - 100 range.
    ValidateMinValues(bg, graph);
    ValidateMaxValues(bg, graph);

    TSeqPos numval = graph.GetNumval();
    if ( numval != bg.GetValues().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphByteLen,
            "SeqGraph (" + NStr::SizetToString(numval) + ") " + 
            "and ByteStore (" + NStr::SizetToString(bg.GetValues().size()) +
            ") length mismatch", seq, graph);
    }
}


// Currently we support only phrap, phred or gap4 types with byte values.
bool CValidError_bioseq::IsSupportedGraphType(const CSeq_graph& graph) const
{
    string title;
    if ( graph.IsSetTitle() ) {
        title = graph.GetTitle();
    }
    if ( NStr::CompareNocase(title, "Phrap Quality") == 0  ||
         NStr::CompareNocase(title, "Phred Quality") == 0  ||
         NStr::CompareNocase(title, "Gap4") == 0 ) {
        if ( graph.GetGraph().IsByte() ) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::ValidateGraphLocation (const CSeq_graph& graph)
{
    if (!graph.IsSetLoc() || graph.GetLoc().Which() == CSeq_loc::e_not_set) {
        PostErr (eDiag_Error, eErr_SEQ_GRAPH_GraphLocInvalid, "SeqGraph location (Unknown) is invalid", graph);
        return false;
    } else {
        const CSeq_loc& loc = graph.GetLoc();
        CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, loc);
        if (!bsh) {
            string label = "";
            if (loc.GetId() != 0) {
               loc.GetId()->GetLabel(&label, CSeq_id::eContent);
            }
            if (NStr::IsBlank(label)) {
                label = "unknown";
            }
            PostErr (eDiag_Warning, eErr_SEQ_GRAPH_GraphBioseqId, 
                     "Bioseq not found for Graph location " + label, graph);
            return false;
        }
        TSeqPos start = loc.GetStart(eExtreme_Positional);
        TSeqPos stop = loc.GetStop(eExtreme_Positional);
        if (start >= bsh.GetBioseqLength() || stop >= bsh.GetBioseqLength()
            || !loc.IsInt() || loc.GetStrand() == eNa_strand_minus) {
            string label = GetValidatorLocationLabel (loc);
            PostErr (eDiag_Error, eErr_SEQ_GRAPH_GraphLocInvalid, 
                     "SeqGraph location (" + label + ") is invalid", graph);
            return false;
        }
    }
    return true;       
}


void CValidError_bioseq::ValidateGraphValues
(const CSeq_graph& graph,
 const CBioseq& seq,
 int& first_N,
 int& first_ACGT,
 size_t& num_bases,
 size_t& Ns_with_score,
 size_t& gaps_with_score,
 size_t& ACGTs_without_score,
 size_t& vals_below_min,
 size_t& vals_above_max)
{
    string label;
    seq.GetFirstId()->GetLabel(&label);

    if (!ValidateGraphLocation(graph)) {
        return;
    }

    const CByte_graph& bg = graph.GetGraph().GetByte();
    int min = bg.GetMin();
    int max = bg.GetMax();

    const CSeq_loc& gloc = graph.GetLoc();
    CRef<CSeq_loc> tmp(new CSeq_loc());
    tmp->Assign(gloc);
    tmp->SetStrand(eNa_strand_plus);
    CSeqVector vec(*tmp, *m_Scope,
                   CBioseq_Handle::eCoding_Ncbi,
                   GetStrand(gloc, m_Scope));
    vec.SetCoding(CSeq_data::e_Ncbi4na);

    CSeqVector::const_iterator seq_begin = vec.begin();
    CSeqVector::const_iterator seq_end   = vec.end();
    CSeqVector::const_iterator seq_iter  = seq_begin;

    const CByte_graph::TValues& values = bg.GetValues();
    CByte_graph::TValues::const_iterator val_iter = values.begin();
    CByte_graph::TValues::const_iterator val_end = values.end();

    size_t score_pos = 0;
    
    while ( seq_iter != seq_end && score_pos < graph.GetNumval()) {
        CSeqVector::TResidue res = *seq_iter;
        int val;
        if (val_iter == val_end) {
            val = 0;
        } else {
            val = *val_iter;
            ++val_iter;
        }
        if ( IsResidue(res) ) {
            // counting total number of bases, to look for percentage of bases with score of zero
            num_bases++;

            if ( (val < min)  ||  (val < 0) ) {
                vals_below_min++;
            }
            if ( (val > max)  ||  (val > 100) ) {
                vals_above_max++;
            }

            switch ( res ) {
            case 0:     // gap
                if ( val > 0 ) {
                    gaps_with_score++;
                }
                break;

            case 1:     // A
            case 2:     // C
            case 4:     // G
            case 8:     // T
                if ( val == 0 ) {
                    ACGTs_without_score++;
                    if ( first_ACGT == -1 ) {
                        first_ACGT = seq_iter.GetPos() + gloc.GetStart(eExtreme_Positional);
                    }
                }
                break;

            case 15:    // N
                if ( val > 0 ) {
                    Ns_with_score++;
                    if ( first_N == -1 ) {
                        first_N = seq_iter.GetPos() + gloc.GetStart(eExtreme_Positional);
                    }
                }
                break;
            }
        }
        ++seq_iter;
        ++score_pos;
    }

}


void CValidError_bioseq::ValidateMinValues(const CByte_graph& bg, const CSeq_graph& graph)
{
    int min = bg.GetMin();
    if ( min < 0  ||  min > 100 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphMin, 
            "Graph min (" + NStr::IntToString(min) + ") out of range",
            graph);
    }
}


void CValidError_bioseq::ValidateMaxValues(const CByte_graph& bg, const CSeq_graph& graph)
{
    int max = bg.GetMax();
    if ( max <= 0  ||  max > 100 ) {
        EDiagSev sev = (max <= 0) ? eDiag_Error : eDiag_Warning;
        PostErr(sev, eErr_SEQ_GRAPH_GraphMax, 
            "Graph max (" + NStr::IntToString(max) + ") out of range",
            graph);
    }
}


void CValidError_bioseq::ValidateGraphOnDeltaBioseq
(const CBioseq& seq)
{
    const CDelta_ext& delta = seq.GetInst().GetExt().GetDelta();
    CDelta_ext::Tdata::const_iterator curr = delta.Get().begin(),
        next = curr,
        end = delta.Get().end();

    SIZE_TYPE   num_delta_seq = 0;
    TSeqPos offset = 0;

    CGraph_CI grp(m_CurrentHandle);
    while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
        ++grp;
    }
    while ( curr != end && grp ) {
        const CSeq_graph& graph = grp->GetOriginalGraph();
        ++next;
        switch ( (*curr)->Which() ) {
            case CDelta_seq::e_Loc:
                {
                    const CSeq_loc& loc = (*curr)->GetLoc();
                    if ( !loc.IsNull() ) {
                        TSeqPos loclen = GetLength(loc, m_Scope);
                        if ( graph.GetNumval() != loclen ) {
                            PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphSeqLocLen,
                                "SeqGraph (" + NStr::IntToString(graph.GetNumval()) +
                                ") and SeqLoc (" + NStr::IntToString(loclen) + 
                                ") length mismatch", graph);
                        }
                        offset += loclen;
                        ++num_delta_seq;
                    }
                    ++grp;
                    while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
                        ++grp;
                    }
                }
                break;

            case CDelta_seq::e_Literal:
                {
                    const CSeq_literal& lit = (*curr)->GetLiteral();
                    TSeqPos litlen = lit.GetLength(),
                        nextlen = 0;
                    if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap() ) {
                        while (next != end  &&  GetLitLength(**next, nextlen)) {
                            litlen += nextlen;
                            ++next;
                        }
                        if ( graph.GetNumval() != litlen ) {
                            PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphSeqLitLen,
                                "SeqGraph (" + NStr::IntToString(graph.GetNumval()) +
                                ") and SeqLit (" + NStr::IntToString(litlen) + 
                                ") length mismatch", graph);
                        }
                        const CSeq_loc& graph_loc = graph.GetLoc();
                        if ( graph_loc.IsInt() ) {
                            TSeqPos from = graph_loc.GetTotalRange().GetFrom();
                            TSeqPos to = graph_loc.GetTotalRange().GetTo();
                            if (  from != offset ) {
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStartPhase,
                                    "SeqGraph (" + NStr::IntToString(from) +
                                    ") and SeqLit (" + NStr::IntToString(offset) +
                                    ") start do not coincide", 
                                    graph);
                            }
                            
                            if ( to != offset + litlen - 1 ) {
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStopPhase,
                                    "SeqGraph (" + NStr::IntToString(to) +
                                    ") and SeqLit (" + 
                                    NStr::IntToString(litlen + offset - 1) +
                                    ") stop do not coincide", 
                                    graph);
                                
                            }
                        }
                        ++grp;
                        while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
                            ++grp;
                        }
                        ++num_delta_seq;
                    }
                    offset += litlen;
                }
                break;

            default:
                break;
        }
        curr = next;
    }

    // if there are any left, count the remaining delta seqs that should have graphs
    while ( curr != end) {
        ++next;
        switch ( (*curr)->Which() ) {
            case CDelta_seq::e_Loc:
                {
                    const CSeq_loc& loc = (*curr)->GetLoc();
                    if ( !loc.IsNull() ) {
                        ++num_delta_seq;
                    }
                }
                break;

            case CDelta_seq::e_Literal:
                {
                    const CSeq_literal& lit = (*curr)->GetLiteral();
                    TSeqPos litlen = lit.GetLength(),
                        nextlen = 0;
                    if ( lit.IsSetSeq_data() ) {
                        while (next != end  &&  GetLitLength(**next, nextlen)) {
                            litlen += nextlen;
                            ++next;
                        }
                        ++num_delta_seq;
                    }
                }
                break;

            default:
                break;
        }
        curr = next;
    }

    SIZE_TYPE num_graphs = 0;
    grp.Rewind();
    while (grp) {
        if (IsSupportedGraphType(grp->GetOriginalGraph())) {
            ++num_graphs;
        }
        ++grp;
    }

    if ( num_delta_seq != num_graphs ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphDiffNumber,
            "Different number of SeqGraph (" + 
            NStr::SizetToString(num_graphs) + ") and SeqLit (" +
            NStr::SizetToString(num_delta_seq) + ") components",
            seq);
    }
}


bool CValidError_bioseq::GetLitLength(const CDelta_seq& delta, TSeqPos& len)
{
    len = 0;
    if ( delta.IsLiteral() ) {
        const CSeq_literal& lit = delta.GetLiteral();
        if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap()) {
            len = lit.GetLength();
            return true;
        }
    }
    return false;
}


// NOTE - this returns the length of the non-gap portions of the sequence
SIZE_TYPE CValidError_bioseq::GetSeqLen(const CBioseq& seq)
{
    SIZE_TYPE seq_len = 0;
    const CSeq_inst & inst = seq.GetInst();

    if ( inst.GetRepr() == CSeq_inst::eRepr_raw ) {
        seq_len = inst.GetLength();
    } else if ( inst.GetRepr() == CSeq_inst::eRepr_delta ) {
        const CDelta_ext& delta = inst.GetExt().GetDelta();
        ITERATE( CDelta_ext::Tdata, dseq, delta.Get() ) {
            switch( (*dseq)->Which() ) {
            case CDelta_seq::e_Loc:
                seq_len += GetLength((*dseq)->GetLoc(), m_Scope);
                break;
                
            case CDelta_seq::e_Literal:
                if ( (*dseq)->GetLiteral().IsSetSeq_data() && !(*dseq)->GetLiteral().GetSeq_data().IsGap() ) {
                    seq_len += (*dseq)->GetLiteral().GetLength();
                }
                break;
                
            default:
                break;
            }
        }
    }
    return seq_len;
}


bool CValidError_bioseq::GraphsOnBioseq(const CBioseq& seq) const
{
    return CGraph_CI(m_CurrentHandle);
}


bool CValidError_bioseq::x_IsActiveFin(const CBioseq& seq) const
{
    CSeqdesc_CI gb_desc(m_CurrentHandle, CSeqdesc::e_Genbank);
    if ( gb_desc ) {
        const CGB_block& gb = gb_desc->GetGenbank();
        if ( gb.IsSetKeywords() ) {
            FOR_EACH_KEYWORD_ON_GENBANKBLOCK (iter, gb) {
                if ( NStr::CompareNocase(*iter, "HTGS_ACTIVEFIN") == 0 ) {
                    return true;
                }
            }
        }
    }
    return false;
}


size_t CValidError_bioseq::x_CountAdjacentNs(const CSeq_literal& lit)
{
    if ( !lit.CanGetSeq_data() ) {
        return 0;
    }

    const CSeq_data& lit_data = lit.GetSeq_data();
    CSeq_data data;
    size_t count = 0;
    size_t max = 0;
    size_t pos = 0;

    if (lit_data.IsIupacna()
        || lit_data.IsNcbi2na()
        || lit_data.IsNcbi4na()
        || lit_data.IsNcbi8na()) {
        CSeqportUtil::Convert(lit_data, &data, CSeq_data::e_Iupacna);
        ITERATE(string, res, data.GetIupacna().Get() ) {
            if ( *res == 'N' ) {
                ++count;
                if ( count > max ) {
                    max = count;
                }
            }
            else {
                count = 0;
            }
            // CSeqportUtil::Convert will convert more data 
            //than the specified length, if the data is present
            ++pos;
            if (pos >= lit.GetLength()) {
                break;
            }
        }
    } else {
        CSeqportUtil::Convert(lit_data, &data, CSeq_data::e_Iupacaa);
        ITERATE(string, res, data.GetIupacaa().Get() ) {
            if ( *res == 'N' ) {
                ++count;
                if ( count > max ) {
                    max = count;
                }
            }
            else {
                count = 0;
            }
            ++pos;
            // CSeqportUtil::Convert will convert more data 
            //than the specified length, if the data is present
            if (pos >= lit.GetLength()) {
                break;
            }
        }
    }

    return max;
}


bool CValidError_bioseq::x_IsDeltaLitOnly(const CSeq_inst& inst) const
{
    if ( inst.CanGetExt()  &&  inst.GetExt().IsDelta() ) {
        ITERATE(CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
            if ( (*iter)->IsLoc() ) {
                return false;
            }
        }
    }
    return true;
}


bool s_HasTpaUserObject(CBioseq_Handle bsh)
{
    for ( CSeqdesc_CI it(bsh, CSeqdesc::e_User); it; ++it ) {
        const CUser_object& uo = it->GetUser();

        if ( uo.CanGetType()  &&  uo.GetType().IsStr()  &&
             NStr::CompareNocase(uo.GetType().GetStr(), "TpaAssembly") == 0 ) {
            return true;
        }
    }

    return false;
}


void CValidError_bioseq::CheckTpaHistory(const CBioseq& seq)
{
    if ( !s_HasTpaUserObject(m_CurrentHandle) ) {
        return;
    }

    if ( seq.CanGetInst()  &&  
         seq.GetInst().CanGetHist()  &&
         !seq.GetInst().GetHist().GetAssembly().empty() ) {
        m_Imp.IncrementTpaWithHistoryCount();
    } else {
        m_Imp.IncrementTpaWithoutHistoryCount();
    }
}


// check that there is no conflict between the gene on the genomic 
// and the gene on the mrna.
void CValidError_bioseq::ValidatemRNAGene (const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
    if (!bsh) {
        return;
    }
    if (!IsMrna(bsh)) {
        return;
    }

    CConstRef<CSeq_feat> mrna = m_Imp.GetmRNAGivenProduct(seq);

    if (!mrna) {
        return;
    }
    m_FeatValidator.ValidatemRNAGene (*mrna);

}


CValidError_bioseq::CmRNACDSIndex::CmRNACDSIndex() 
{
}


CValidError_bioseq::CmRNACDSIndex::~CmRNACDSIndex()
{
}


bool s_IdXrefsAreReciprocal (const CMappedFeat &cds, const CMappedFeat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return false;
    }

    bool match = false;

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            match = true;
            break;
        }
    }
    if (!match) {
        return false;
    }
    match = false;
            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            match = true;
            break;
        }
    }

    return match;
}


unsigned int s_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return 0;
    }

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && !s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            return 1;
        }
    }

            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && !s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            return 1;
        }
    }

    if (!cds.IsSetProduct() || !mrna.IsSetExt()) {
        return 0;
    }
    
    TGi gi = ZERO_GI;
    if (cds.GetProduct().GetId()->IsGi()) {
        gi = cds.GetProduct().GetId()->GetGi();
    } else {
        // TODO: get gi for other kinds of SeqIds
    }

    if (gi == ZERO_GI) {
        return 0;
    }

    if (mrna.IsSetExt() && mrna.GetExt().IsSetType() && mrna.GetExt().GetType().IsStr()
        && NStr::EqualNocase(mrna.GetExt().GetType().GetStr(), "MrnaProteinLink")
        && mrna.GetExt().IsSetData()
        && mrna.GetExt().GetData().front()->IsSetLabel()
        && mrna.GetExt().GetData().front()->GetLabel().IsStr()
        && NStr::EqualNocase (mrna.GetExt().GetData().front()->GetLabel().GetStr(), "protein seqID")
        && mrna.GetExt().GetData().front()->IsSetData()
        && mrna.GetExt().GetData().front()->GetData().IsStr()) {
        string str = mrna.GetExt().GetData().front()->GetData().GetStr();
        try {
            CSeq_id id (str);
            if (id.IsGi()) {
                if (id.GetGi() == gi) {
                    return 0;
                } else {
                    return 2;
                }
            }
        } catch (CException ) {
        } catch (std::exception ) {
        }
    }
    return 0;
    
}


void CValidError_bioseq::CmRNACDSIndex::SetBioseq(CFeat_CI *feat_list, const CTSE_Handle& tse, CBioseq_Handle bioseq, CScope * scope)
{
    m_PairList.clear();
    m_CDSList.clear();
    m_mRNAList.clear();
    if (feat_list) {
        feat_list->Rewind();

        CMappedFeat current_mrna;
        CMappedFeat current_cds;

        while (*feat_list) {
            if ((*feat_list)->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                current_mrna = **feat_list;
                m_mRNAList.push_back(current_mrna);
            } else if ((*feat_list)->GetData().IsCdregion()) {
                current_cds = **feat_list;
                if (current_mrna && !GetCDSFormRNA(current_mrna)) {
                    m_PairList.push_back(TmRNACDSPair(current_cds, current_mrna));
                }
                m_CDSList.push_back(current_cds);
            }
            ++(*feat_list);
        }

        // now replace pairings by xref etc.
        TPairList::iterator pair_it = m_PairList.begin();
        while (pair_it != m_PairList.end()) {
            EOverlapType overlap_type = eOverlap_CheckIntRev;
            bool featid_matched = false;
            bool ok_to_keep = false;

            if (pair_it->first.IsSetExcept_text()
                && (NStr::FindNoCase (pair_it->first.GetExcept_text(), "ribosomal slippage") != string::npos
                    || NStr::FindNoCase (pair_it->first.GetExcept_text(), "trans-splicing") != string::npos)) {
                overlap_type = eOverlap_SubsetRev;
            }

            if (TestForOverlapEx (pair_it->first.GetLocation(), pair_it->second.GetLocation(), overlap_type, scope) >= 0) {
                if (pair_it->first.IsSetId() && pair_it->second.IsSetId()
                    && s_IdXrefsAreReciprocal(pair_it->first, pair_it->second)) {
                    featid_matched = true;
                }
                ok_to_keep = true;
            }
               
            if (!featid_matched) {
                // look for explicit feature ID match, to catch complicated overlaps marked by feature ID
                FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, pair_it->first) {
                    if ((*itx)->IsSetId() && (*itx)->GetId().IsLocal() 
                        && (*itx)->GetId().GetLocal().IsId()) {
                        vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, 
                                                                                 (*itx)->GetId().GetLocal().GetId());
                        ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                            if (feat_it->IsSetData() 
                                && feat_it->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA
                                && s_IdXrefsAreReciprocal(pair_it->first, *feat_it)) {
                                featid_matched = true;
                                pair_it->second = *feat_it;
                                ok_to_keep = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (ok_to_keep) {
                ++pair_it;
            } else {
                pair_it = m_PairList.erase (pair_it);
            }
        }

    }

}


CMappedFeat CValidError_bioseq::CmRNACDSIndex::GetmRNAForCDS(CMappedFeat cds)
{
    CMappedFeat f;

    ITERATE (TPairList, pair_it, m_PairList) {
        if (pair_it->first == cds) {
            return pair_it->second;
        }
    }
    return f;
}


CMappedFeat CValidError_bioseq::CmRNACDSIndex::GetCDSFormRNA(CMappedFeat mrna)
{
    CMappedFeat f;

    ITERATE (TPairList, pair_it, m_PairList) {
        if (pair_it->second == mrna) {
            return pair_it->first;
        }
    }
    return f;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
