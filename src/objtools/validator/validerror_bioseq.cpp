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

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
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

#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);
USING_SCOPE(feature);

// Maximum number of adjacent Ns in a Seq_lit
const size_t CValidError_bioseq::scm_AdjacentNsThreshold = 80;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseq::CValidError_bioseq(CValidError_imp& imp) :
    CValidError_base(imp),
    m_TpaWithHistory(0), m_TpaWithoutHistory(0)
{
}


CValidError_bioseq::~CValidError_bioseq(void)
{
}


void CValidError_bioseq::ValidateSeqIds
(const CBioseq& seq)
{
    // Ensure that CBioseq has at least one CSeq_id
    if ( seq.GetId().empty() ) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
                 "No ids on a Bioseq", seq);
        return;
    }

    CSeq_inst::ERepr repr = seq.GetInst().GetRepr();

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NT, or NC. Check that the same CSeq_id not included more
    // than once.
    bool has_gi = false;
    ITERATE( CBioseq::TId, i, seq.GetId() ) {

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

        CConstRef<CBioseq> core = m_Scope->GetBioseqHandle(**i).GetBioseqCore();
        if ( !core ) {
            if ( !m_Imp.IsPatent() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                    "BioseqFind (" + (*i)->AsFastaString() + 
                    ") unable to find itself - possible internal error", seq);
            }
        } else if ( core.GetPointer() != &seq ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                "SeqID " + (*i)->AsFastaString() + 
                " is present on multiple Bioseqs in record", seq);
        }

        if ( (*i)->IsGi() ) {
            has_gi = true;
        }
    }

    // Loop thru CSeq_ids to check formatting
    bool is_wgs = false;
    bool is_gb_embl_ddbj = false;
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    ITERATE (CBioseq::TId, k, seq.GetId()) {
        const CTextseq_id* tsid = (*k)->GetTextseq_Id();
        switch ((**k).Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            if ( IsHistAssemblyMissing(seq)  &&  seq.IsNa() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_HistAssemblyMissing,
                    "TPA record " + (*k)->AsFastaString() +
                    " should have Seq-hist.assembly for PRIMARY block", 
                    seq);
            }
        // Fall thru 
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            if ( tsid  &&  tsid->IsSetAccession() ) {
                const string& acc = tsid->GetAccession();
                unsigned int num_digits = 0;
                unsigned int num_letters = 0;
                bool letter_after_digit = false;
                bool bad_id_chars = false;       
                    
                is_wgs = acc.length() == 12  ||  acc.length() == 13;

                ITERATE(string, s, acc) {
                    if (isupper(*s)) {
                        num_letters++;
                        if (num_digits > 0) {
                            letter_after_digit = true;
                        }
                    } else if (isdigit(*s)) {
                        num_digits++;
                    } else {
                        bad_id_chars = true;
                    }
                }
                is_gb_embl_ddbj = 
                    (**k).IsGenbank()  ||  (**k).IsEmbl()  ||  (**k).IsDdbj();

                if ( letter_after_digit || bad_id_chars ) {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession: " + acc, seq);
                } else if (num_letters == 1 && num_digits == 5 && seq.IsNa()) {
                } else if (num_letters == 2 && num_digits == 6 && seq.IsNa()) {
                } else if (num_letters == 3 && num_digits == 5 && seq.IsAa()) {
                } else if (num_letters == 2 && num_digits == 6 && seq.IsAa() &&
                    repr == CSeq_inst::eRepr_seg) {
                } else if ( num_letters == 4  && 
                            (num_digits == 8  ||  num_digits == 9)  && 
                            seq.IsNa()  &&
                            is_gb_embl_ddbj ) {
                } else {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession: " + acc, seq);
                }
                    
                // Check for secondary conflicts
                if ( seq.GetFirstId() ) {
                    ValidateSecondaryAccConflict(acc, seq, CSeqdesc::e_Genbank);
                    ValidateSecondaryAccConflict(acc, seq, CSeqdesc::e_Embl);
                }

                if ( has_gi ) {
                    if ( tsid->IsSetVersion()  &&  tsid->GetVersion() == 0 ) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Accession " + acc + " has 0 version", seq);
                    }
                }
            }
        // Fall thru
        case CSeq_id::e_Other:
            if ( tsid ) {
                if ( tsid->IsSetName() ) {
                    const string& name = tsid->GetName();
                    ITERATE (string, s, name) {
                        if (isspace(*s)) {
                            PostErr(eDiag_Critical,
                                eErr_SEQ_INST_SeqIdNameHasSpace,
                                "Seq-id.name " + name + " should be a single "
                                "word without any spaces", seq);
                            break;
                        }
                    }
                }

                if ( tsid->IsSetAccession()  &&  (*k)->IsOther() ) {
                    const string& acc = tsid->GetAccession();
                    size_t num_letters = 0;
                    size_t num_digits = 0;
                    size_t num_underscores = 0;
                    bool bad_id_chars = false;
                    bool is_NZ = (NStr::CompareNocase(acc, 0, 3, "NZ_") == 0);
                    size_t i = is_NZ ? 3 : 0;
                    bool letter_after_digit = false;

                    for ( ; i < acc.length(); ++i ) {
                        if ( isupper(acc[i]) ) {
                            num_letters++;
                        } else if ( isdigit(acc[i]) ) {
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
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, seq);
                    } else if ( is_NZ  &&  num_letters == 4  && 
                        num_digits == 8  &&  num_underscores == 0 ) {
                        // valid accession - do nothing!
                    } else if ( num_letters == 2  &&
                        (num_digits == 6  ||  num_digits == 8  || num_digits == 9)  &&
                        num_underscores == 1 ) {
                        // valid accession - do nothing!
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, seq);
                    }
                }

                if ( has_gi && !tsid->IsSetAccession() && tsid->IsSetName() ) {
                    EDiagSev sev = eDiag_Critical;
                    // Report ddbj segmented sequence missing accesions as 
                    // warning, not critical.
                    if ( (*k)->IsDdbj()  &&  repr == CSeq_inst::eRepr_seg ) {
                        sev = eDiag_Warning;
                    }
                    PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat,
                        "Missing accession for " + tsid->GetName(), seq);
                }
            }
            // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
            if ( tsid ) {
                if ( seq.IsNa()  &&  
                     (!tsid->IsSetAccession() || tsid->GetAccession().empty())) {
                    if ( repr != CSeq_inst::eRepr_seg  ||
                        m_Imp.IsGI()) {
                        if (!(**k).IsDdbj()  ||
                            repr != CSeq_inst::eRepr_seg) {
                            CNcbiOstrstream os;
                            os << "Missing accession for " << (**k).DumpAsFasta();
                            PostErr(eDiag_Error,
                                eErr_SEQ_INST_BadSeqIdFormat,
                                string(os.str()), seq);
                        }
                    }
                }
                accn_count++;
            } else {
                PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                    "Seq-id type not handled", seq);
            }
            break;
        
        case CSeq_id::e_Patent:
            break;
        case CSeq_id::e_Pdb:
            break;
        case CSeq_id::e_Gi:
            if ((*k)->GetGi() <= 0) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_ZeroGiNumber,
                         "Invalid GI number", seq);
            }
            gi_count++;
            break;
        case CSeq_id::e_General:
            break;
        default:
            break;
        }
    }

    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));
    if ( is_wgs ) {
        if ( !mi  ||  !mi->CanGetTech()  ||  
            mi->GetTech() != CMolInfo::eTech_wgs ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                "WGS accession should have Mol-info.tech of wgs", seq);
        }
    } else if ( mi  &&  mi->CanGetTech()  &&  
                mi->GetTech() == CMolInfo::eTech_wgs  &&  is_gb_embl_ddbj ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Mol-info.tech of wgs should have WGS accession", seq);
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

    // Protein specific checks
    if ( seq.IsAa() ) {
        ITERATE( CBioseq::TId, id, seq.GetId() ) {
            switch ( (*id)->Which() ) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                {
                    const CTextseq_id* tsid = (*id)->GetTextseq_Id();
                    if ( tsid != NULL ) {
                        if ( !tsid->IsSetAccession()  &&  tsid->IsSetName() ) {
                            if ( m_Imp.IsNucAcc(tsid->GetName()) ) {
                                PostErr(eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                                    "Protein bioseq has Textseq-id 'name' that"
                                    "looks like it is derived from a nucleotide"
                                    "accession", seq);
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }
        }
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
    const CSeq_inst& inst = seq.GetInst();
    CSeq_inst::TRepr repr = inst.CanGetRepr() ?
        inst.GetRepr() : CSeq_inst::eRepr_not_set;

    if ( !inst.CanGetHist()  ||  !inst.GetHist().CanGetAssembly() ) {
        if ( seq.IsNa()  &&  repr != CSeq_inst::eRepr_seg ) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::ValidateSecondaryAccConflict
(const string &primary_acc,
 const CBioseq &seq,
 int choice)
{
    CSeq_descr_CI ds(m_Scope->GetBioseqHandle(seq));
    CSeqdesc_CI sd(ds, static_cast<CSeqdesc::E_Choice>(choice));
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
            ITERATE( list<string>, acc, *extra_acc ) {
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


void CValidError_bioseq::ValidateInst(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    // Check representation
    if ( !ValidateRepr(inst, seq) ) {
        return;
    }

    // Check molecule, topology, and strand
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
                 inst.GetStrand() != CSeq_inst::eStrand_ss ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_DSProtein,
                         "Protein not single stranded", seq);
            }
            break;

        case CSeq_inst::eMol_not_set:
            PostErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol not set",
                seq);
            break;

        case CSeq_inst::eMol_other:
            PostErr(eDiag_Error, eErr_SEQ_INST_MolOther,
                     "Bioseq.mol is type other", seq);
            break;

        default:
            break;
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
    
    x_ValidateTitle(seq);
    /*if ( seq.IsAa() ) {
         Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }*/
    
    if ( seq.IsNa() ) {
        // check for N bases at start or stop of sequence
        ValidateNs(seq);
    }

    // Validate sequence length
    ValidateSeqLen(seq);
}


void CValidError_bioseq::ValidateBioseqContext(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    // Get Molinfo
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));

    if ( mi ) {
        x_ValidateCompletness(seq, *mi);

        if ( mi->IsSetTech() ) {
            switch (mi->GetTech()) {
            case CMolInfo::eTech_sts:
            case CMolInfo::eTech_survey:
            case CMolInfo::eTech_wgs:
            case CMolInfo::eTech_htgs_0:
            case CMolInfo::eTech_htgs_1:
            case CMolInfo::eTech_htgs_2:
            case CMolInfo::eTech_htgs_3:
                if (mi->GetTech() == CMolInfo::eTech_sts  &&
                    seq.GetInst().GetMol() == CSeq_inst::eMol_rna  &&
                    mi->IsSetBiomol()  &&
                    mi->GetBiomol() == CMolInfo::eBiomol_mRNA) {
                    // !!!
                    // Ok, there are some STS sequences derived from 
                    // cDNAs, so do not report these
                } else if (mi->IsSetBiomol()  &&
                    mi->GetBiomol() != CMolInfo::eBiomol_genomic) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                        "HTGS/STS/GSS/WGS sequence should be genomic", seq);
                } else if (seq.GetInst().GetMol() != CSeq_inst::eMol_dna  &&
                    seq.GetInst().GetMol() != CSeq_inst::eMol_na) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                        "HTGS/STS/GSS/WGS sequence should not be RNA", seq);
                }
                break;
            default:
                break;
            }
        }
    }
    
    
    // Check that proteins in nuc_prot set have a CdRegion
    if ( CdError(bsh) ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", 
            seq);
    }

    // Check that gene on non-segmented sequence does not have
    // multiple intervals
    ValidateMultiIntervalGene(seq);

    ValidateSeqFeatContext(seq);

    // Check for duplicate features and overlapping peptide features.
    ValidateDupOrOverlapFeats(seq);

    // Check for colliding genes
    ValidateCollidingGenes(seq);

    if ( seq.IsSetDescr() ) {
        ValidateSeqDescContext(seq);
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
    
    // flag missing molinfo even if not in Sequin
    CheckForMolinfoOnBioseq(seq);

    ValidateGraphsOnBioseq(seq);

    CheckTpaHistory(seq);
    
    if ( IsMrna(bsh) ) {
        ValidatemRNABioseqContext(bsh);
    }

    // check for multiple publications with identical identifiers
    x_ValidateMultiplePubs(bsh);
}


void CValidError_bioseq::x_ValidateMultiplePubs(const CBioseq_Handle& bsh)
{
    vector<int> muids;
    vector<int> pmids;

    for (CSeqdesc_CI it(bsh, CSeqdesc::e_Pub); it; ++it) {
        int muid = 0;
        int pmid = 0;
        bool otherpub = false;
        ITERATE (CPubdesc::TPub::Tdata, pub, it->GetPub().GetPub().Get()) {
            switch ( (*pub)->Which() ) {
            case CPub::e_Muid:
                muid = (*pub)->GetMuid();
                break;
            case CPub::e_Pmid:
                pmid = (*pub)->GetPmid();
                break;
            default:
                otherpub = true;
                break;
            } 
        }
    
        if ( otherpub ) {
            bool collision = false;
            if ( muid > 0 ) {
                if ( binary_search(muids.begin(), muids.end(), muid) ) {
                    collision = true;
                } else {
                    muids.push_back(muid);
                }
            }
            if ( pmid > 0 ) {
                if ( binary_search(pmids.begin(), pmids.end(), pmid) ) {
                    collision = true;
                } else {
                    pmids.push_back(pmid);
                }
            }
            if ( collision ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications,
                    "Multiple publications with same identifier", *it);
            }
        }
    }
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


void CValidError_bioseq::ValidatemRNABioseqContext(const CBioseq_Handle& seq)
{
    // check that there is no conflict between the gene on the genomic 
    // and the gene on the mrna.
    const CSeq_feat* mrna = GetmRNAForProduct(seq);
    const CGene_ref* genomicgrp = 0;
    if ( mrna != 0 ) {
        genomicgrp = mrna->GetGeneXref();
        if ( genomicgrp == 0 ) {
            const CSeq_feat* gene = 
                GetOverlappingGene(mrna->GetLocation(), *m_Scope);
            if ( gene != 0 ) {
                genomicgrp = &gene->GetData().GetGene();
            }
        }
        if ( genomicgrp != 0 ) {
            CFeat_CI mrna_gene(seq, CSeqFeatData::e_Gene);
            if ( mrna_gene ) {
                const CGene_ref& mrnagrp = mrna_gene->GetData().GetGene();
                if ( !s_EqualGene_ref(*genomicgrp, mrnagrp) ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenesInconsistent,
                        "Gene on mRNA bioseq does not match gene on genomic bioseq",
                        mrna_gene->GetOriginalFeature());
                }
            }
        }
    }
}


void CValidError_bioseq::ValidateHistory(const CBioseq& seq)
{
    if ( !seq.GetInst().IsSetHist() ) {
        return;
    }
    
    int gi = 0;
    ITERATE( CBioseq::TId, id, seq.GetId() ) {
        if ( (*id)->IsGi() ) {
            gi = (*id)->GetGi();
            break;
        }
    }
    if ( gi == 0 ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst().GetHist();
    if ( hist.IsSetReplaced_by() ) {
        const CSeq_hist_rec& rec = hist.GetReplaced_by();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaced by gi (" + 
                        NStr::IntToString(gi) + ") is same as current Bioseq",
                        seq);
                    break;
                }
            }
        }
    }

    if ( hist.IsSetReplaces() ) {
        const CSeq_hist_rec& rec = hist.GetReplaces();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaces gi (" + 
                        NStr::IntToString(gi) + ") is same as current Bioseq",
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


bool CValidError_bioseq::IsDifferentDbxrefs(const TDbtags& list1,
                                            const TDbtags& list2)
{
    if (list1.empty()  ||  list2.empty()) {
        return false;
    } else if (list1.size() != list2.size()) {
        return true;
    }

    TDbtags::const_iterator it1 = list1.begin();
    TDbtags::const_iterator it2 = list2.begin();
    for (; it1 != list1.end(); ++it1, ++it2) {
        if ((*it1)->GetDb() != (*it2)->GetDb()) {
            return true;
        }
        string str1 =
            (*it1)->GetTag().IsStr() ? (*it1)->GetTag().GetStr() : "";
        string str2 =
            (*it2)->GetTag().IsStr() ? (*it2)->GetTag().GetStr() : "";
        if ( str1.empty()  &&  str2.empty() ) {
            if (!(*it1)->GetTag().IsId()  &&  !(*it2)->GetTag().IsId()) {
                continue;
            } else if ((*it1)->GetTag().IsId()  &&  (*it2)->GetTag().IsId()) {
                if ((*it1)->GetTag().GetId() != (*it2)->GetTag().GetId()) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }
    return false;
}



// Is the id contained in the bioseq?
bool CValidError_bioseq::IsIdIn(const CSeq_id& id, const CBioseq& seq)
{
    ITERATE (CBioseq::TId, it, seq.GetId()) {
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
    const CSeq_feat* sfp = m_Imp.GetCDSGivenProduct(seq);
    if ( sfp ) {
    
        // Get CCdregion 
        CTypeConstIterator<CCdregion> cdr(ConstBegin(*sfp));
        
        // Get location on source sequence
        const CSeq_loc& loc = sfp->GetLocation();

        // Get CBioseq_Handle for source sequence
        CBioseq_Handle hnd = m_Scope->GetBioseqHandle(loc);

        // Translate na CSeq_data
        string prot;        
        CCdregion_translate::TranslateCdregion(prot, hnd, loc, *cdr);
        
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
    return false;
}


bool CValidError_bioseq::GetLocFromSeq(const CBioseq& seq, CSeq_loc* loc)
{
    if (!seq.GetInst().IsSetExt()  ||  !seq.GetInst().GetExt().IsSeg()) {
        return false;
    }

    CSeq_loc_mix& mix = loc->SetMix();
    ITERATE (list< CRef<CSeq_loc> >, it,
        seq.GetInst().GetExt().GetSeg().Get()) {
        mix.Set().push_back(*it);
    }
    return true;
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
                return true;
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
        if ( !m_Imp.IsFarLocation(slit.GetSeq_loc()) ) {
            ++counter;
        }
    }
    return counter;
}


bool CValidError_bioseq::LocOnSeg(const CBioseq& seq, const CSeq_loc& loc) 
{
    for ( CSeq_loc_CI sli( loc ); sli;  ++sli ) {
        const CSeq_id& loc_id = sli.GetSeq_id();
        ITERATE(  CBioseq::TId, seq_id, seq.GetId() ) {
            if ( loc_id.Match(**seq_id) ) {
                return true;
            }
        }
    }
    return false;
}


bool CValidError_bioseq::NotPeptideException
(const CFeat_CI& curr,
 const CFeat_CI& prev)
{
    if ( curr->IsSetExcept()  &&  curr->GetExcept()  &&
         curr->IsSetExcept_text() ) {
        if ( NStr::FindNoCase(curr->GetExcept_text(), "alternative processing") != NPOS ) {
            return false;
        }
    }
    if ( prev->IsSetExcept()  &&  prev->GetExcept()  &&
         prev->IsSetExcept_text() ) {
        if ( NStr::FindNoCase(prev->GetExcept_text(), "alternative processing") != NPOS ) {
            return false;
        }
    }
    return true;
}


bool CValidError_bioseq::IsSameSeqAnnot
(const CFeat_CI& fi1,
 const CFeat_CI& fi2)
{
    return fi1->GetAnnot() == fi2->GetAnnot();
}


bool CValidError_bioseq::IsSameSeqAnnotDesc
(const CFeat_CI& fi1,
 const CFeat_CI& fi2)
{
    const CSeq_annot& annot1 = *fi1->GetAnnot().GetCompleteSeq_annot();
    const CSeq_annot& annot2 = *fi2->GetAnnot().GetCompleteSeq_annot();

    if ( !(annot1.IsSetDesc())  ||  !(annot2.IsSetDesc()) ) {
        return true;
    }

    CAnnot_descr::Tdata descr1 = annot1.GetDesc().Get();
    CAnnot_descr::Tdata descr2 = annot2.GetDesc().Get();

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


void CValidError_bioseq::ValidateSeqLen(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    TSeqPos len = inst.IsSetLength() ? inst.GetLength() : 0;
    if ( seq.IsAa() ) {
        if ( len <= 3  &&  !m_Imp.IsPDB() ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
        }
    } else {
        if ( len <= 10  &&  !m_Imp.IsPDB()) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
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
    const CSeq_entry* parent = seq.GetParentEntry();
    if (!parent) {
        return;
    }
    if ( !parent->IsSet() ) {
        return;
    }

    // Loop through seq_set looking for the bioseq, when found, the next
    // CSeq_entry should have the parts.
    CBioseq_set::TSeq_set::const_iterator se_parts = 
        parent->GetSet().GetSeq_set().end();
    ITERATE ( CBioseq_set::TSeq_set, it, parent->GetSet().GetSeq_set() ) {
        if ( parent == &(**it) ) {
            se_parts = ++it;
            break;
        }
    }
    if ( se_parts == parent->GetSet().GetSeq_set().end() ) {
        return;
    }

    // Check that se_parts is a CBioseq_set of class parts
    if ( !(*se_parts)->IsSet()  ||  !(*se_parts)->GetSet().IsSetClass()  ||
         (*se_parts)->GetSet().GetClass() != CBioseq_set::eClass_parts ) {
        return;
    }

    const CSeg_ext::Tdata& locs = seq.GetInst().GetExt().GetSeg().Get();
    const CBioseq_set::TSeq_set& parts = (*se_parts)->GetSet().GetSeq_set();

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
                continue;
            }
            if ( !(*part_it)->IsSet() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Parts set component is not Bioseq", seq);
                return;
            }
            const CSeq_id& loc_id = GetId(**loc_it, m_Scope);
            if ( !IsIdIn(loc_id, (*part_it)->GetSeq()) ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Segmented bioseq seq_ext does not correspond to parts"
                    "packaging order", seq);
            }
            
            // advance both iterators
            ++loc_it;
            ++part_it;
        } catch (const CObjmgrUtilException&) {
            ERR_POST("Seq-loc not for unique sequence");
            return;
        } catch (...) {
            ERR_POST("Unknown error");
            return;
        }
    }
}


void CValidError_bioseq::x_ValidateTitle(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if (!bsh) {
        return;
    }

    CMolInfo::TTech tech = CMolInfo::eTech_unknown;
    CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo);
    if (desc) {
        const CMolInfo& mi = desc->GetMolinfo();
        tech = mi.GetTech();
        if (mi.GetCompleteness() != CMolInfo::eCompleteness_complete) {
            if (m_Imp.IsGenbank()) {
                string title = GetTitle(bsh);
                if (NStr::Find(title, "complete genome") != NPOS) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteTitleProblem,
                        "Complete genome in title without complete flag set",
                        seq);
                }
            }
            if (bsh.GetInst_Topology() == CSeq_inst::eTopology_circular) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteCircleProblem,
                    "Circular topology without complete flag set", seq);
            }
        }
    }

    // specific test for proteins
    if (seq.IsAa()) {
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Title);
        if (desc) {
            CSeq_entry_Handle entry =
                bsh.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
            if (entry) {
                const string& instantiated = desc->GetTitle();
                string generated = GetTitle(bsh, fGetTitle_Reconstruct);
                if (!NStr::EqualNocase(instantiated, generated)) {
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentProteinTitle,
                        "Instantiated protein title does not match automatically "
                        "generated title. Instantiated: " + instantiated + 
                        " Generated: " + generated, seq);
                }
            }
        }
    }
}


void CValidError_bioseq::ValidateNs(const CBioseq& seq)
{
    try {
        const CSeq_inst& inst = seq.GetInst();
        CSeq_inst::TRepr repr = 
            inst.CanGetRepr() ? inst.GetRepr() : CSeq_inst::eRepr_not_set;
        CSeq_inst::TLength len = inst.CanGetLength() ? inst.GetLength() : 0;

        if ( (repr == CSeq_inst::eRepr_raw  ||  
             (repr == CSeq_inst::eRepr_delta  &&  x_IsDeltaLitOnly(inst))) &&
             len > 10 ) {
            
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
            if ( !bsh ) {
                return;
            }
            
            CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            
            EDiagSev sev = eDiag_Warning;
            string sequence;
            
            // check for Ns at the beginning of the sequence
            if ( (vec[0] == 'N')  ||  (vec[0] == 'n') ) {
                // if 10 or more Ns flag as error (except for NC), 
                // otherwise just warning
                vec.GetSeqData(0, 10, sequence);
                if ( !m_Imp.IsNC()  &&
                     (NStr::CompareNocase(sequence, "NNNNNNNNNN") == 0) ) {
                    sev = eDiag_Error;
                }
                PostErr(sev, eErr_SEQ_INST_TerminalNs, 
                    "N at beginning of sequence", seq);
            }
            
            // check for Ns at the end of the sequence
            sev = eDiag_Warning;
            if ( (vec[vec.size() - 1] == 'N')  ||  (vec[vec.size() - 1] == 'n') ) {
                // if 10 or more Ns flag as error (except for NC), 
                // otherwise just warning
                vec.GetSeqData(vec.size() - 10, vec.size() , sequence);
                if ( !m_Imp.IsNC()  &&
                     (NStr::CompareNocase(sequence, "NNNNNNNNNN") == 0) ) {
                    sev = eDiag_Error;
                }
                PostErr(sev, eErr_SEQ_INST_TerminalNs, 
                    "N at end of sequence", seq);
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

    if (inst.IsSetFuzz()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_FuzzyLen,
            "Fuzzy length on " + rpr + "Bioseq", seq);
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

    CSeq_data::E_Choice seqtyp = inst.IsSetSeq_data() ?
        inst.GetSeq_data().Which() : CSeq_data::e_not_set;
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
        factor = 30;
        break;
    default:
        // Logically, should not occur
        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                 "Sequence alphabet not set",
                 seq);
        return;
    }
    TSeqPos calc_len = inst.IsSetLength() ? inst.GetLength() : 0;
    
    switch (seqtyp) {
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Ncbipaa:
        calc_len *= factor;
        break;
    default:
        if (calc_len % factor) {
            calc_len += factor;
        }
        calc_len /= factor;
    }
    string s_len = NStr::UIntToString(calc_len);

    TSeqPos data_len = GetDataLen(inst);
    string data_len_str = NStr::IntToString(data_len);
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

    unsigned char termination;
    unsigned int trailingX = 0;
    size_t terminations = 0;
    if (check_alphabet) {

        switch (seqtyp) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbieaa:
            termination = '*';
            break;
        case CSeq_data::e_Ncbistdaa:
            termination = 25;
            break;
        default:
            termination = '\0';
        }

        CSeqVector sv = 
            m_Scope->GetBioseqHandle(seq).GetSeqVector();

        size_t bad_cnt = 0;
        TSeqPos pos = 0;
        for ( CSeqVector_CI sv_iter(sv); sv_iter; ++sv_iter ) {
            CSeqVector::TResidue res = *sv_iter;
            if ( !IsResidue(res) ) {
                if ( ++bad_cnt > 10 ) {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                        "More than 10 invalid residues. Checking stopped",
                        seq);
                    return;
                } else {
                    string msg = "Invalid residue [";
                    if ( seqtyp == CSeq_data::e_Ncbistdaa ) {
                        msg += NStr::UIntToString(res);
                    } else {
                        msg += res;
                    }
                    msg += "] in position [" + NStr::UIntToString(pos) + "]";

                    PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                        msg, seq);
                }
            } else if ( res == termination ) {
                terminations++;
                trailingX = 0;
            } else if ( res == 'X' ) {
                trailingX++;
            } else {
                trailingX = 0;
            }
            ++pos;
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

        if (terminations) {
            // Post error indicating terminations found in protein sequence
            // First get gene label

            string glbl;
            seq.GetLabel(&glbl, CBioseq::eContent);
            if ( IsBlankString(glbl) ) {
                glbl = "gene?";
            }

            string plbl;
            const CBioseq* nuc = GetNucleotideParent(seq, m_Scope);
            if ( nuc ) {
                nuc->GetLabel(&plbl, CBioseq::eContent);
            }
            if ( IsBlankString(plbl) ) {
                plbl = "prot?";
            }

            PostErr(eDiag_Error, eErr_SEQ_INST_StopInProtein,
                NStr::IntToString(terminations) +
                " termination symbols in protein sequence (" +
                glbl + string(" - ") + plbl + ")", seq);
            if (!bad_cnt) {
                return;
            }
        }
        return;
    }
}


// Assumes seq is eRepr_seg or eRepr_ref
void CValidError_bioseq::ValidateSegRef(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CSeq_loc loc;
    if ( GetLocFromSeq(seq, &loc) ) {
        m_Imp.ValidateSeqLoc(loc, bsh, "Segmented Bioseq", seq);
    }

    // Validate Length
    try {
        TSeqPos loclen = GetLength(loc, m_Scope);
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
        ERR_POST(Critical << "Unable to calculate length: ");
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
                    CNcbiOstrstream os;
                    os << id1.DumpAsFasta();
                    string sid(os.str());
                    if ((**i1).IsWhole()  &&  (**i2).IsWhole()) {
                        PostErr(eDiag_Error,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid, seq);
                    } else {
                        PostErr(eDiag_Warning,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid + " that are not e_Whole", seq);
                    }
                }
            }
        }
    }

    // Check that partial sequence info on sequence segments is consistent with
    // partial sequence info on sequence -- aa  sequences only
    int partial = SeqLocPartialCheck(loc, m_Scope);
    if (partial  &&  seq.IsAa()) {
        bool got_partial = false;
        CTypeConstIterator<CSeqdesc> sd(ConstBegin(seq.GetDescr()));
        for (; sd; ++sd) {
            if (!(*sd).IsModif()) {
                continue;
            }
            ITERATE(list< EGIBB_mod >, md, (*sd).GetModif()) {
                switch (*md) {
                case eGIBB_mod_partial:
                    got_partial = true;
                    break;
                case eGIBB_mod_no_left:
                    if (partial & eSeqlocPartial_Start) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-left inconsistent with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                case eGIBB_mod_no_right:
                    if (partial & eSeqlocPartial_Stop) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-right inconsistene with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                default:
                    break;
                }
            }
        }
        if (!got_partial) {
            PostErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                "Partial segmented sequence without GIBB-mod", seq);
        }
    }
}


// Assumes seq is a delta sequence
void CValidError_bioseq::ValidateDelta(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

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

   // set severity for first / last gap error
    TSeqPos len = 0;
    TSeqPos seg = 0;
    bool last_is_gap = false;
    size_t num_gaps = 0;
    size_t num_adjacent_gaps = 0;
    bool non_interspersed_gaps = false;
    bool first = true;

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
            try {
                size_t loc_len = GetLength(loc, m_Scope);
                len += loc_len;
                if ( loc_len <= 10 ) {
                    string loc_str;
                    loc.GetLabel(&loc_str);
                    if ( loc_str.empty() ) {
                        loc_str = "?";
                    }
                    PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLocLength,
                        "Short length (" + NStr::IntToString(loc_len) + 
                        ") on seq-loc (" + loc_str + ") of delta seq_ext", seq);
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
            if ( !last_is_gap ) {
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

            // Check for invalid residues
            if ( lit.CanGetSeq_data() ) {
                if ( !last_is_gap ) {
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
                            PostErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                                "Invalid residue [" +
                                NStr::IntToString((int)c[*ci]) + "] in position " +
                                NStr::IntToString(*ci), seq);
                        }
                        break;
                    }
                default:
                    break;
                }

                if ( ss ) {
                    ITERATE (vector<TSeqPos>, it, badIdx) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            ss->substr(*it, 1) + "] in position " +
                            NStr::IntToString(*it), seq);
                    }
                }
                            
                // Count adjacent Ns in Seq-lit for htgs_1 and htgs_2
                if ( tech == CMolInfo::eTech_htgs_1  ||  
                    tech == CMolInfo::eTech_htgs_2 ) {
                    size_t adjacent_ns = x_CountAdjacentNs(lit);
                    if ( adjacent_ns > scm_AdjacentNsThreshold ) {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_InternalNsInSeqLit,
                            "Run of " + NStr::UIntToString(adjacent_ns) + 
                            " Ns in delta component " + NStr::UIntToString(seg) +
                            " that starts at base " + NStr::UIntToString(start_len),
                            seq);
                    }
                }
            } else {
                if ( first ) {
                    PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
                        "First delta seq component is a gap", seq);
                }
                if ( last_is_gap ) {
                    ++num_adjacent_gaps;
                }
                if ( !lit.CanGetLength()  ||  lit.GetLength() == 0 ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_SeqLitGapLength0,
                        "Gap of length 0 in delta chain", seq);
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
            "Bisoeq.seq_data is larger [" + NStr::IntToString(len) +
            "] than given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    }
    if ( non_interspersed_gaps  &&  (!m_Imp.IsGI()) &&  mi  &&
        (tech != CMolInfo::eTech_htgs_0  ||  tech != CMolInfo::eTech_htgs_1  ||
         tech != CMolInfo::eTech_htgs_2  ||  tech != CMolInfo::eTech_htgs_3) ) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MissingGaps,
            "HTGS delta seq should have gaps between all sequence runs", seq);
    }
    if ( num_adjacent_gaps >= 1 ) {
        string msg = (num_adjacent_gaps == 1) ?
            "There is one adjacent gap in delta seq" :
            "There are " + NStr::IntToString(num_adjacent_gaps) +
            " adjacent gaps in delta seq";
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq, msg, seq);
    }
    if ( last_is_gap ) {
        PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
            "Last delta seq component is a gap", seq);
    }
    
    // Validate technique
    if ( mi  &&  !m_Imp.IsNT()  &&  !m_Imp.IsNC()  &&  !m_Imp.IsGPS() ) {
        if (tech != CMolInfo::eTech_unknown   &&
            tech != CMolInfo::eTech_standard  &&
            tech != CMolInfo::eTech_wgs       &&
            tech != CMolInfo::eTech_htgs_0    &&
            tech != CMolInfo::eTech_htgs_1    &&
            tech != CMolInfo::eTech_htgs_2    &&
            tech != CMolInfo::eTech_htgs_3      ) {
            const CEnumeratedTypeValues* tv =
                CMolInfo::GetTypeInfo_enum_ETech();
            const string& stech = tv->FindName(mi->GetTech(), true);
            PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
                "Delta seq technique should not be " + stech, seq);
        }
    }

    if (num_gaps == 0  &&  mi) {
        if ( tech == CMolInfo::eTech_htgs_2  &&
             !GraphsOnBioseq(seq)  &&
             !x_IsActiveFin(seq) ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadHTGSeq,
                "HTGS 2 delta seq has no gaps and no graphs", seq);
        }
    }
}


bool CValidError_bioseq::ValidateRepr
(const CSeq_inst& inst,
 const CBioseq&   seq)
{
    bool rtn = true;
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);
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
          inst.GetSeq_data().Which() == CSeq_data::e_not_set)
        {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotFound, err2, seq);
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
    CSeqdesc_CI sd(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Molinfo);

    if ( !sd ) {
        m_Imp.AddBioseqWithNoMolinfo(seq);
    }
}


void CValidError_bioseq::CheckForPubOnBioseq(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }

    // Check for CPubdesc on the biodseq
    if ( !CSeqdesc_CI( bsh, CSeqdesc::e_Pub)  &&
         !CFeat_CI(bsh, CSeqFeatData::e_Pub) ) {
        m_Imp.AddBioseqWithNoPub(seq);
    }
}


void CValidError_bioseq::ValidateMultiIntervalGene(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    
    for ( CFeat_CI fi(bsh, CSeqFeatData::e_Gene); fi; ++fi ) {
        const CSeq_loc& loc = fi->GetOriginalFeature().GetLocation();
        if ( !IsOneBioseq(loc, m_Scope) ) {  // skip segmented 
            continue;
        }
        CSeq_loc_CI si(loc);
        if ( !(++si) ) {  // if only a single interval
            continue;
        }
        if ( seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular ) {
            continue;
        }

        EDiagSev sev = m_Imp.IsNC() ? eDiag_Warning : eDiag_Error;
        PostErr(sev, eErr_SEQ_FEAT_MultiIntervalGene,
            "Gene feature on non-segmented sequence should not "
            "have multiple intervals", fi->GetOriginalFeature());
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

    if ( comp == CMolInfo::eCompleteness_complete  &&
         biomol == CMolInfo::eBiomol_genomic ) {
        bool is_gb = false;
        ITERATE (CBioseq::TId, it, seq.GetId()) {
            if ( (*it)->IsGenbank() ) {
                is_gb = true;
                break;
            }
        }

        if ( is_gb ) {
            CSeqdesc_CI desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Title);
            if ( desc ) {
                const string& title = desc->GetTitle();
                if ( !title.empty()  &&
                     (NStr::FindNoCase(title, "complete sequence") == NPOS  ||
                      NStr::FindNoCase(title, "complete genome") == NPOS) ) {
                    PostErr(sev, eErr_SEQ_DESCR_UnwantedCompleteFlag,
                        "Suspicious use of complete", *desc);
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


void CValidError_bioseq::ValidateSeqFeatContext(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    
    bool full_length_prot_ref = false;

    bool is_mrna = IsMrna(bsh);
    bool is_prerna = IsPrerna(bsh);
    bool is_aa = CSeq_inst::IsAa(bsh.GetInst_Mol());
    bool is_virtual = (bsh.GetInst_Repr() == CSeq_inst::eRepr_virtual);
    TSeqPos len = bsh.IsSetInst_Length() ? bsh.GetInst_Length() : 0;

    for ( CFeat_CI fi(bsh); fi; ++fi ) {
        const CSeq_feat& feat = fi->GetOriginalFeature();
        
        CSeqFeatData::E_Choice ftype = feat.GetData().Which();

        if ( is_aa ) {                // protein
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
                {
                    CSeq_loc::TRange range = fi->GetLocation().GetTotalRange();
                    
                    if ( range.IsWhole()  ||
                         (range.GetFrom() == 0  &&  range.GetTo() == len - 1) ) {
                         full_length_prot_ref = true;
                    }
                }
                break;
                
            case CSeqFeatData::e_Gene:
                // report only if NOT standalone protein
                if ( !s_StandaloneProt(bsh) ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                        "Invalid feature for a protein Bioseq.", feat);
                }
                break;
            case CSeqFeatData::e_Cdregion:
            case CSeqFeatData::e_Rna:
            case CSeqFeatData::e_Rsite:
            case CSeqFeatData::e_Txinit:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a protein Bioseq.", feat);
                break;

            default:
                break;
            }
        } else {                            // nucleotide
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
            case CSeqFeatData::e_Psec_str:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a nucleotide Bioseq.", feat);
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
                        EDiagSev sev = m_Imp.IsRefSeq() ? eDiag_Warning : eDiag_Error;
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
                const CRNA_ref& rref = fi->GetData().GetRna();
                if ( rref.GetType() == CRNA_ref::eType_mRNA ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                        "mRNA feature is invalid on an mRNA (cDNA) Bioseq.",
                        feat);
                }
                
                break;
            }}    
            case CSeqFeatData::e_Imp:
            {{
                const CImp_feat& imp = fi->GetData().GetImp();
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
                const CImp_feat& imp = fi->GetData().GetImp();
                if ( imp.GetKey() == "CAAT_signal" ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                        "Invalid feature for a pre-RNA Bioseq.", feat);
                }
                break;
            }}
	    default:
	        break;
            }
        }    

        if ( !m_Imp.IsNC()  &&  m_Imp.IsFarLocation(feat.GetLocation()) ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_FarLocation,
                "Feature has 'far' location - accession not packaged in record",
                feat);
        }

        if ( seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
            if ( LocOnSeg(seq, fi->GetOriginalFeature().GetLocation()) ) {
                if ( !IsDeltaOrFarSeg(fi->GetLocation(), m_Scope) ) {
                    EDiagSev sev = m_Imp.IsNC() ? eDiag_Warning : eDiag_Error;
                    PostErr(sev, eErr_SEQ_FEAT_LocOnSegmentedBioseq,
                        "Feature location on segmented bioseq, not on parts",
                        feat);
                }
            }
        }
    }  // end of for loop

    // if no full length prot feature on a part of a segmented bioseq
    // search for such feature on the master bioseq
    if ( is_aa  &&  !full_length_prot_ref ) {
        CBioseq_Handle parent = s_GetParent(bsh);
        if ( parent ) {
            TSeqPos parent_len = parent.IsSetInst_Length() ? bsh.GetInst_Length() : 0;
            for ( CFeat_CI it(parent, CSeqFeatData::e_Prot); it; ++it ) {
                CSeq_loc::TRange range = it->GetLocation().GetTotalRange();
                
                if ( range.IsWhole()  ||
                    (range.GetFrom() == 0  &&  range.GetTo() == parent_len - 1) ) {
                    full_length_prot_ref = true;
                }
            }
        }
    }

    if ( is_aa  &&  !full_length_prot_ref  &&  !is_virtual  &&  !m_Imp.IsPDB()   ) {
        m_Imp.AddProtWithoutFullRef(bsh);
    }

    // validate abutting UTRs for nucleotides
    if ( !is_aa ) {
        x_ValidateAbuttingUTR(bsh);
    }

    x_ValidateCDSmRNAmatch(bsh);

    if (m_Imp.IsLocusTagGeneralMatch()) {
        x_ValidateLocusTagGeneralMatch(bsh);
    }
}


void CValidError_bioseq::x_ValidateLocusTagGeneralMatch(const CBioseq_Handle& seq)
{
    if (!seq  ||  !CSeq_inst::IsNa(seq.GetInst_Mol())) {
        return;
    }

    // iterate coding regions and mRNAs
    SAnnotSelector as;
    as.IncludedFeatType(CSeqFeatData::e_Cdregion);
    as.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);

    for (CFeat_CI it(seq, as); it; ++it) {
        const CSeq_feat& feat = it->GetOriginalFeature();
        if (!feat.IsSetProduct()) {
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
            continue;
        }
        const string& locus_tag = grp->GetLocus_tag();
   
        
        CBioseq_Handle prod = m_Scope->GetBioseqHandleFromTSE
            (GetId(feat.GetProduct(), m_Scope), m_Imp.GetTSE());
        if (!prod) {
            continue;
        }
        ITERATE (CBioseq_Handle::TId, it, prod.GetId()) {
            if (!it->GetSeqId()->IsGeneral()) {
                continue;
            }
            const CDbtag& dbt = it->GetSeqId()->GetGeneral();
            if (!dbt.IsSetDb()) {
                continue;
            }
            const string& db = dbt.GetDb();
            if (NStr::EqualNocase(db, "TMSMART")  ||
                NStr::EqualNocase(db, "BankIt")) {
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
    }
}


void CValidError_bioseq::x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq)
{
    // nothing to validate if there aren't any genes
    if (!CFeat_CI(seq, CSeqFeatData::e_Gene)) {
        return;
    }

    // count mRNAs and CDSs for each gene.
    typedef map<CConstRef<CSeq_feat>, SIZE_TYPE> TFeatCount;
    TFeatCount cds_count, mrna_count;

    SAnnotSelector as;
    as.IncludedFeatType(CSeqFeatData::e_Cdregion);
    as.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);

    CConstRef<CSeq_feat> gene;
    for (CFeat_CI it(seq, as); it; ++it) {
        const CSeq_feat& feat = it->GetOriginalFeature();
        const CGene_ref* gref = feat.GetGeneXref();
        if (gref == NULL  ||  gref->IsSuppressed()) {
            gene = GetOverlappingGene(it->GetLocation(), seq.GetScope());
        }
        if (gene) {
            if (cds_count.find(gene) == cds_count.end()) {
                cds_count[gene] = mrna_count[gene] = 0;
            }

            switch (feat.GetData().GetSubtype()) {
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
        if (cds_num != mrna_num) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAmismatch,
                "mRNA count (" + NStr::IntToString(mrna_num) + 
                ") does not match CDS (" + NStr::IntToString(cds_num) +
                ") count for gene", *it->first);
        }
    }
}


void CValidError_bioseq::x_ValidateAbuttingUTR(const CBioseq_Handle& seq)
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_5UTR);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_3UTR);

    TMappedFeatVec cds_group;

    bool saw_cds = false;
    ENa_strand cds_strand = eNa_strand_unknown;
    for ( CFeat_CI it(seq, sel); it; ++it ) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        bool validate_group = false;
        if ( saw_cds ) {
            if ( subtype == CSeqFeatData::eSubtype_cdregion ) {
                validate_group = true;
                --it;
            } else if ( cds_strand != eNa_strand_minus  &&
                subtype == CSeqFeatData::eSubtype_5UTR ) {
                validate_group = true;
            } else if ( cds_strand == eNa_strand_minus  &&
                subtype == CSeqFeatData::eSubtype_3UTR ) {
                validate_group = true;
            }
        }
        if( validate_group ) {
            x_ValidateAbuttingCDSGroup(cds_group, cds_strand == eNa_strand_minus);
            saw_cds = false;
            cds_strand = eNa_strand_unknown;
            cds_group.clear();
        } else {
            if ( subtype == CSeqFeatData::eSubtype_cdregion ) {
                saw_cds = true;
                cds_strand = GetStrand(it->GetLocation(), m_Scope);
            }
            cds_group.push_back(*it);
        }
    }

    if ( !cds_group.empty() ) {
        x_ValidateAbuttingCDSGroup(cds_group, cds_strand == eNa_strand_minus);
    }
}


static const string& s_FeatName(const CMappedFeat& feat)
{
    static const string utr5 = "5'UTR";
    static const string utr3 = "3'UTR";
    static const string cds  = "CDS";

    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_cdregion:
        return cds;
    case CSeqFeatData::eSubtype_5UTR:
        return utr5;
    case CSeqFeatData::eSubtype_3UTR:
        return utr3;
    default:
        break;
    }
    return kEmptyStr;
}


void CValidError_bioseq::x_ValidateAbuttingCDSGroup
(const TMappedFeatVec& cds_group,
 bool minus)
{
    if ( cds_group.empty() ) {
        return;
    }

    // make sure there is a cds in the group
    const CMappedFeat* cds = 0;
    ITERATE (TMappedFeatVec, it, cds_group) {
        if ( it->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion ) {
            cds = &(*it);
            break;
        }
    }

    // UTR features not connected to any cds
    if ( cds == 0 ) {
        ITERATE (TMappedFeatVec, it, cds_group) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                s_FeatName(*it)  + " feature not associated with CDS feature.",
                it->GetOriginalFeature());
        }
        return;
    }

    if ( cds_group.size() == 1 ) { // only the cds
        return;
    }

    TMappedFeatVec::const_iterator first = cds_group.begin();
    TMappedFeatVec::const_iterator second = first++;
    TMappedFeatVec::const_iterator end = cds_group.end();

    while ( first != end ) {
        const CSeq_loc& first_loc = first->GetLocation();
        const CSeq_loc& second_loc = second->GetLocation();

        if ( minus ) {
        } else {
            TSeqPos secend = second_loc.GetStop(eExtreme_Positional);
            TSeqPos firstart = first_loc.GetStart(eExtreme_Positional);
            if ( secend + 1 != firstart ) {
                const string& first_name = s_FeatName(*first);
                const string& second_name = s_FeatName(*second);
                if ( first_name == second_name ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                        first_name + " does not abut previous " + first_name,
                        first->GetOriginalFeature());
                } else {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                        first_name + " does not abut  " + second_name,
                        first->GetOriginalFeature());
                }
            }
        }
        second = first;
        ++first;
    }
}


void CValidError_bioseq::ValidateDupOrOverlapFeats(const CBioseq& bioseq)
{
    ENa_strand              curr_strand, prev_strand;
    CCdregion::EFrame       curr_frame, prev_frame;
    const CSeq_loc*         curr_loc = 0, *prev_loc = 0;
    EDiagSev                severity;
    CSeqFeatData::ESubtype  curr_subtype = CSeqFeatData::eSubtype_bad, 
                            prev_subtype = CSeqFeatData::eSubtype_bad;
    bool same_label;
    string curr_label, prev_label;
    
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (bioseq);

    bool fruit_fly = false;
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if ( di ) {
        if ( NStr::CompareNocase(di->GetSource().GetOrg().GetTaxname(),
             "Drosophila melanogaster") == 0 ) {
            fruit_fly = true;
        }
    }

    CFeat_CI curr(bsh);
    if ( !curr ) {
        return;
    }

    CFeat_CI prev = curr;
    ++curr;
    while ( curr ) {
        curr_loc = &(curr->GetLocation());
        prev_loc = &(prev->GetLocation());
        curr_subtype = curr->GetData().GetSubtype();
        prev_subtype = prev->GetData().GetSubtype();

        // if same location, subtype and strand
        if ( curr_subtype == prev_subtype  &&
             sequence::Compare(*curr_loc, *prev_loc, m_Scope) == eSame) {

            curr_strand = GetStrand(*curr_loc, m_Scope);
            prev_strand = GetStrand(*prev_loc, m_Scope);
            if ( curr_strand == prev_strand         || 
                 curr_strand == eNa_strand_unknown  ||
                 prev_strand == eNa_strand_unknown  ) {

                if ( IsSameSeqAnnotDesc(curr, prev)  ||
                     IsSameSeqAnnot(curr, prev) ) {
                    severity = eDiag_Error;

                    // compare labels and comments
                    same_label = true;
                    const string &curr_comment = curr->IsSetComment() ? curr->GetComment() : kEmptyStr;
                    const string &prev_comment = prev->IsSetComment() ? prev->GetComment() : kEmptyStr;
                    curr_label.erase();
                    prev_label.erase();
                    feature::GetLabel(curr->GetOriginalFeature(),
                                      &curr_label, feature::eContent, m_Scope);
                    feature::GetLabel(prev->GetOriginalFeature(),
                                      &prev_label, feature::eContent, m_Scope);
                    if ( !NStr::EqualNocase(curr_comment, prev_comment) ||
                         !NStr::EqualNocase(curr_label, prev_label) ) {
                        same_label = false;
                    }

                    // lower sevrity for some cases
                    if ( curr_subtype == CSeqFeatData::eSubtype_pub          ||
                         curr_subtype == CSeqFeatData::eSubtype_region       ||
                         curr_subtype == CSeqFeatData::eSubtype_misc_feature ||
                         curr_subtype == CSeqFeatData::eSubtype_STS          ||
                         curr_subtype == CSeqFeatData::eSubtype_variation ) {
                        severity = eDiag_Warning;
                    } else if ( !(m_Imp.IsGPS() || 
                                  m_Imp.IsNT()  ||
                                  m_Imp.IsNC()) ) {
                        severity = eDiag_Warning;
                    } else if ( fruit_fly ) {
                        // curated fly source still has duplicate features
                        severity = eDiag_Warning;
                    }
                    
                    // if different CDS frames, lower to warning
                    if ( curr->GetData().Which() == CSeqFeatData::e_Cdregion ) {
                        curr_frame = curr->GetData().GetCdregion().GetFrame();
                        prev_frame = CCdregion::eFrame_not_set;
                        if ( prev->GetData().Which() == CSeqFeatData::e_Cdregion ) {
                            prev_frame = prev->GetData().GetCdregion().GetFrame();
                        }
                        
                        if ( (curr_frame != CCdregion::eFrame_not_set  &&
                            curr_frame != CCdregion::eFrame_one)     ||
                            (prev_frame != CCdregion::eFrame_not_set  &&
                            prev_frame != CCdregion::eFrame_one) ) {
                            if ( curr_frame != prev_frame ) {
                                severity = eDiag_Warning;
                            }
                        }
                    }
                    if (m_Imp.IsGPS()  ||  m_Imp.IsNT()  ||  m_Imp.IsNC() ) {
                        severity = eDiag_Warning;
                    }

                    if ( (prev->IsSetDbxref()  &&
                          IsFlybaseDbxrefs(prev->GetDbxref()))  || 
                         (curr->IsSetDbxref()  && 
                          IsFlybaseDbxrefs(curr->GetDbxref())) ) {
                        severity = eDiag_Error;
                    }

                    // Report duplicates
                    if ( curr_subtype == CSeqFeatData::eSubtype_region  &&
                        IsDifferentDbxrefs(curr->GetDbxref(), prev->GetDbxref()) ) {
                        // do not report if both have dbxrefs and they are 
                        // different.
                    } else if ( IsSameSeqAnnot(curr, prev) ) {
                        if (same_label) {
                            PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature", 
                                curr->GetOriginalFeature());
                        } else if ( curr_subtype != CSeqFeatData::eSubtype_pub ) {
                            // do not report if partial flags are different
                            bool curr_partial_start =
                                curr_loc->IsPartialStart(eExtreme_Biological);
                            bool curr_partial_stop =
                                curr_loc->IsPartialStop(eExtreme_Biological);
                            bool prev_partial_start =
                                prev_loc->IsPartialStart(eExtreme_Biological);
                            bool prev_partial_stop =
                                prev_loc->IsPartialStop(eExtreme_Biological);
                            if (curr_partial_start == prev_partial_start  &&
                                curr_partial_stop == prev_partial_stop) {
                                PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                    "Features have identical intervals, but labels differ",
                                    curr->GetOriginalFeature());
                            }
                        }
                    } else {
                        if (same_label) {
                            PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature (packaged in different feature table)",
                                curr->GetOriginalFeature());
                        } else if ( prev_subtype != CSeqFeatData::eSubtype_pub ) {
                            PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                "Features have identical intervals, but labels "
                                "differ (packaged in different feature table)",
                                curr->GetOriginalFeature());
                        }
                    }
                }
            }
        }
        
        
        if ( (curr_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
              curr_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
              curr_subtype == CSeqFeatData::eSubtype_transit_peptide_aa)  &&
             (prev_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
              prev_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
              prev_subtype == CSeqFeatData::eSubtype_transit_peptide_aa) ) {
            if (sequence::Compare(*curr_loc, *prev_loc, m_Scope) != eNoOverlap  &&
                NotPeptideException(curr, prev)) {
                EDiagSev overlapPepSev = 
                    m_Imp.IsOvlPepErr() ? eDiag_Error :eDiag_Warning;
                PostErr( overlapPepSev,
                    eErr_SEQ_FEAT_OverlappingPeptideFeat,
                    "Signal, Transit, or Mature peptide features overlap",
                    curr->GetOriginalFeature());
            }
        }

        ++prev; 
        ++curr;
    }  // end of while loop
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


// Validate CSeqdesc within the context of a bioseq. 
// See: CValidError_desc for validation of standalone CSeqdesc,
// and CValidError_descr for validation of descriptors in the context
// of descriptor list.
void CValidError_bioseq::ValidateSeqDescContext(const CBioseq& seq)
{
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
    CConstRef<CDate> create_date, update_date;
    string create_str;
    int biomol = -1;
    EGIBB_mol seq_na_mol = eGIBB_mol_unknown;
    CConstRef<COrg_ref> org;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    for ( CSeqdesc_CI di(bsh); di; ++ di ) {
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
            if ( create_date ) {
                if ( create_date->Compare(current) != CDate::eCompare_same ) {
                    string current_str;
                    current.GetDate(&current_str);
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent create_date [" + create_str +
                        "] and update_date [" + current_str + "]", seq, desc);
                }
            } else {
                create_date = &(desc.GetCreate_date());
                create_date->GetDate(&create_str);
            }

            if ( update_date ) {
                ValidateUpdateDateContext(*update_date, current, seq, desc);
            }
            }
           break;

        case CSeqdesc::e_Update_date:
            if ( create_date ) {
                ValidateUpdateDateContext(desc.GetUpdate_date(), *create_date, 
                    seq, desc);
            } else {
                update_date = &(desc.GetUpdate_date());
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
                
                if ( source.IsSetIs_focus() ) {
                    // skip proteins, segmented bioseqs, or segmented parts
                    if ( !seq.IsAa()  &&
                        !(seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg)  &&
                        !(m_Imp.GetAncestor(seq, CBioseq_set::eClass_parts) != 0) ) {
                        if ( !CFeat_CI(bsh, CSeqFeatData::e_Biosrc) ) {
                            PostErr(eDiag_Error,
                                eErr_SEQ_DESCR_UnnecessaryBioSourceFocus,
                                "BioSource descriptor has focus, "
                                "but no BioSource feature", seq, desc);
                        }
                    }
                }
                if ( source.CanGetOrigin()  &&  
                     source.GetOrigin() == CBioSource::eOrigin_synthetic ) {
                    if ( !IsOtherDNA(seq) ) {
                        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                            "Molinfo-biomol other should be used if "
                            "Biosource-location is synthetic", seq);
                    }
                }
                if ( !org ) {
                    org = &(desc.GetSource().GetOrg());
                }
                ValidateOrgContext(di, desc.GetSource().GetOrg(), 
                    *org, seq, desc);
            }
            break;

        case CSeqdesc::e_Molinfo:
            ValidateMolInfoContext(desc.GetMolinfo(), biomol, seq, desc);
            break;

        case CSeqdesc::e_Mol_type:
            ValidateMolTypeContext(desc.GetMol_type(), seq_na_mol, seq, desc);

        default:
            break;
        }
    }

    if ( num_gb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple GenBank blocks", seq, *last_gb);
    }

    if ( num_embl > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple EMBL blocks", seq, *last_embl);
    }

    if ( num_pir > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PIR blocks", seq, *last_pir);
    }

    if ( num_pdb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PDB blocks", seq, *last_pdb);
    }

    if ( num_prf > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PRF blocks", seq, *last_prf);
    }

    if ( num_sp > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple SWISS-PROT blocks", seq, *last_sp);
    }
}


void CValidError_bioseq::ValidateMolInfoContext
(const CMolInfo& minfo,
 int& seq_biomol,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( minfo.IsSetBiomol() ) {
        int biomol = minfo.GetBiomol();
        if ( seq_biomol < 0 ) {
            seq_biomol = biomol;
        }
        
        switch ( biomol ) {
        case CMolInfo::eBiomol_peptide:
            if ( seq.IsNa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Nucleic acid with Molinfo-biomol = peptide", seq, desc);
            }
            break;

        case CMolInfo::eBiomol_other_genetic:
            if ( !x_IsArtificial(seq) ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol = other genetic", seq, desc);
            }
            break;
            
        case CMolInfo::eBiomol_unknown:
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol unknown used", seq, desc);
            break;

        case CMolInfo::eBiomol_other:
            if ( !m_Imp.IsXR() ) {
                if ( !IsSynthetic(seq) ) {
                    if ( !x_IsMicroRNA(seq)) {
                        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                            "Molinfo-biomol other used", seq, desc);
                    }
                }
            }
            break;
            
        default:  // the rest are nucleic acid
            if ( seq.IsAa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol [" + NStr::IntToString(biomol) +
                    "] used on protein", seq, desc);
            } else {
                if ( biomol != seq_biomol ) {
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent Molinfo-biomol [" + 
                        NStr::IntToString(seq_biomol) + "] and [" +
                        NStr::IntToString(biomol) + "]", seq, desc);
                }
            }
        }
    }

    if ( !minfo.IsSetTech() ) {
        return;
    }

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
                "Nucleic acid with protein sequence method", seq, desc);
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
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "Protein with nucleic acid sequence method", seq, desc);
            break;
        default:
            break;
        }
    }
}


bool CValidError_bioseq::IsOtherDNA(const CBioseq& seq) const
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh ) {
        CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
        if ( sd ) {
            const CSeqdesc::TMolinfo& molinfo = sd->GetMolinfo();
            if ( molinfo.CanGetBiomol()  &&
                 molinfo.GetBiomol() == CMolInfo::eBiomol_other ) {
                return true;
            }
        }
    }
    return false;
}


bool CValidError_bioseq::IsSynthetic(const CBioseq& seq) const
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh ) {
        CSeqdesc_CI sd(bsh, CSeqdesc::e_Source);
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


bool CValidError_bioseq::x_IsArtificial(const CBioseq& seq) const
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh ) {
        CSeqdesc_CI sd(bsh, CSeqdesc::e_Source);
        if ( sd ) {
            const CSeqdesc::TSource& source = sd->GetSource();
            if ( source.CanGetOrigin() ) {
                CBioSource::TOrigin origin = source.GetOrigin();
                return  
                    origin == CBioSource::eOrigin_artificial  ||
                    origin == CBioSource::eOrigin_mut         ||
                    origin == CBioSource::eOrigin_synthetic;
            }
        }
    }
    return false;
}


bool CValidError_bioseq::x_IsMicroRNA(const CBioseq& seq) const 
{
    SAnnotSelector selector(CSeqFeatData::e_Rna);
    selector.SetFeatSubtype(CSeqFeatData::eSubtype_otherRNA);
    CFeat_CI fi(m_Scope->GetBioseqHandle(seq), selector);

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


void CValidError_bioseq::ValidateMolTypeContext
(const EGIBB_mol& gibb_mol,
 EGIBB_mol& seq_na_mol,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    switch ( gibb_mol ) {
    case eGIBB_mol_peptide:
        if ( seq.IsNa() ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "Nucleic acid with GIBB-mol = peptide", seq, desc);
        }
        break;

    case eGIBB_mol_unknown:
    case eGIBB_mol_other:
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
            "GIBB-mol unknown or other used", seq, desc);
        break;

    default:                   // the rest are nucleic acid
        if ( seq.IsAa() ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "GIBB-mol [" + 
                ENUM_METHOD_NAME(EGIBB_mol)()->FindName(gibb_mol, true) + 
                "] used on protein", seq, desc);
        } else if ( seq_na_mol != eGIBB_mol_unknown ) {
            if ( seq_na_mol != gibb_mol ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                    "Inconsistent GIBB-mol [" + 
                    ENUM_METHOD_NAME(EGIBB_mol)()->FindName(seq_na_mol, true) +
                    "] and [" + 
                    ENUM_METHOD_NAME(EGIBB_mol)()->FindName(gibb_mol, true) +
                    "]", seq, desc);
            } else {
                seq_na_mol = gibb_mol;
            }
        }
        break;
    }
}


void CValidError_bioseq::ValidateUpdateDateContext
(const CDate& update,
 const CDate& create,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( update.Compare(create) == CDate::eCompare_before ) {
        string create_str, update_str;
        update.GetDate(&update_str);
        create.GetDate(&create_str);
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_Inconsistent,
            "Inconsistent create_date [" + create_str + 
            "] and update_date [" + update_str + "]", seq, desc);
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
                "] and [" + org.GetTaxname() + "]", seq, desc);
        }
    }
}

/*
class CNoCaseCompare
{
public:
    bool operator()(const string& s1, const string& s2) const
    {
        return NStr::CompareNocase(s1, s2) < 0;
    }
};
*/
void CValidError_bioseq::x_CompareStrings
(const TStrFeatMap& str_feat_map,
 const string& type,
 EErrType err,
 EDiagSev sev)
{
    // iterate through multimap and compare strings
    bool first = true;
    const string* strp = 0;
    ITERATE (TStrFeatMap, it, str_feat_map) {
        if ( first ) {
            first = false;
            strp = &(it->first);
            continue;
        }
        CNcbiOstrstream msg;
        if ( NStr::Compare(*strp, it->first) == 0 ) {
            msg << "Colliding " << type << " in gene features";
        } else if ( NStr::CompareNocase(*strp, it->first) == 0 ) {
            msg << "Colliding " << type
                << " (with different capitalization) in gene features";
        }
        if ( msg.pcount() > 0 ) {
            PostErr(sev, err, CNcbiOstrstreamToString(msg), *it->second);
        }
        strp = &(it->first);
    }
}


void CValidError_bioseq::ValidateCollidingGenes(const CBioseq& seq)
{
    TStrFeatMap label_map;
    TStrFeatMap locus_tag_map;

    // Loop through genes and insert into multimap sorted by
    // gene label / locus_tag -- case insensitive
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    for ( CFeat_CI fi(bsh, CSeqFeatData::e_Gene); fi; ++fi ) {
        const CSeq_feat& feat = fi->GetOriginalFeature();
        // record label
        string label;
        GetLabel(feat, &label, feature::eContent, m_Scope);
        label_map.insert(TStrFeatMap::value_type(label, &feat));
        // record locus_tag
        const CGene_ref& gene = feat.GetData().GetGene();
        if ( gene.CanGetLocus_tag()  &&  !gene.GetLocus_tag().empty() ) {
            locus_tag_map.insert(TStrFeatMap::value_type(gene.GetLocus_tag(), &feat));
        }
    }
    x_CompareStrings(label_map, "names", eErr_SEQ_FEAT_CollidingGeneNames,
        eDiag_Warning);
    x_CompareStrings(locus_tag_map, "locus_tags", eErr_SEQ_FEAT_CollidingLocusTags,
        eDiag_Error);
}


void CValidError_bioseq::ValidateIDSetAgainstDb(const CBioseq& seq)
{
    const CSeq_id*  gb_id = 0,
                 *  db_gb_id = 0;
    int             gi = 0,
                    db_gi = 0;
    const CDbtag*   general_id = 0,
                *   db_general_id = 0;

    ITERATE( CBioseq::TId, id, seq.GetId() ) {
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

    if ( gi == 0  &&  gb_id != 0 ) {
        gi = GetGIForSeqId(*gb_id);
    }

    if ( gi <= 0 ) {
        return;
    }

    list< CRef< CSeq_id > > id_set = GetSeqIdsForGI(gi);
    if ( !id_set.empty() ) {
        ITERATE( list< CRef< CSeq_id > >, id, id_set ) {
            switch ( (*id)->Which() ) {
            case CSeq_id::e_Genbank:
                db_gb_id = id->GetPointer();
                break;
                
            case CSeq_id::e_Gi:
                db_gi = (*id)->GetGi();
                break;
                
            case CSeq_id::e_General:
                db_general_id = &((*id)->GetGeneral());
                break;
                
            default:
                break;
            }
        }

        string gi_str = NStr::IntToString(gi);

        if ( db_gi != gi ) {
          PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
              "New gi number (" + gi_str + ")" +
              " does not match old one (" + NStr::IntToString(db_gi) + ")", 
              seq);
        }
        if ( (gb_id != 0)  &&  (db_gb_id != 0) ) {
          if ( !gb_id->Match(*db_gb_id) ) {
              PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                  "New accession (" + gb_id->AsFastaString() + 
                  ") does not match old one (" + db_gb_id->AsFastaString() + 
                  ") on gi (" + gi_str + ")", seq);
          }
        } else if ( gb_id != NULL) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of accession (" + gb_id->AsFastaString() + ") on gi (" +
                gi_str + ")", seq);
        } else if ( db_gb_id != 0 ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of accession (" + db_gb_id->AsFastaString() + 
                ") on gi (" + gi_str + ")", seq);
        }

        string new_gen_label, old_gen_label;
        if ( general_id  != 0  &&  db_general_id != 0 ) {
            if ( !general_id->Match(*db_general_id) ) {
                db_general_id->GetLabel(&old_gen_label);
                general_id->GetLabel(&new_gen_label);
                PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                    "New general ID (" + new_gen_label + 
                    ") does not match old one (" + old_gen_label +
                    ") on gi (" + gi_str + ")", seq);
            }
        } else if ( general_id != 0 ) {
            general_id->GetLabel(&new_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of general ID (" + new_gen_label + ") on gi (" + 
                gi_str + ")", seq);
        } else if ( db_general_id != 0 ) {
            db_general_id->GetLabel(&old_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of general ID (" + old_gen_label + ") on gi (" +
                gi_str + ")", seq);
        }
    }
}


void CValidError_bioseq::ValidateGraphsOnBioseq(const CBioseq& seq)
{
    if ( !seq.IsNa()  ) {
        return;
    }

    int     last_loc = -1;
    bool    overlaps = false;
    const CSeq_graph* overlap_graph = 0;
    SIZE_TYPE num_graphs = 0;
    SIZE_TYPE graphs_len = 0;
    bool    validate_values = true;

    const CSeq_inst& inst = seq.GetInst();  
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CGraph_CI grp(bsh);
    if ( !grp ) {
        return;
    }

    for ( ; grp; ++grp ) {
        const CSeq_graph& graph = grp->GetOriginalGraph();
        if ( !IsSuportedGraphType(graph) ) {
            continue;
        }

        // Currently we support only byte graphs
        ValidateByteGraphOnBioseq(graph, seq);
        
        // Test for overlapping graphs
        const CSeq_loc& loc = graph.GetLoc();
        if ( (int)loc.GetTotalRange().GetFrom() <= last_loc ) {
            overlaps = true;
            overlap_graph = &graph;
        }
        last_loc = loc.GetTotalRange().GetTo();

        graphs_len += graph.GetNumval();
        ++num_graphs;
    }

    if ( overlaps ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphOverlap,
            "Graph components overlap, with multiple scores for "
            "a single base", seq, *overlap_graph);
    }

    SIZE_TYPE seq_len = GetSeqLen(seq);
    if ( (seq_len != graphs_len)  &&  (inst.GetLength() != graphs_len) ) {
        validate_values = false;
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphBioseqLen,
            "SeqGraph (" + NStr::IntToString(graphs_len) + ") and Bioseq (" +
            NStr::IntToString(seq_len) + ") length mismatch", seq);
    }
    
    if ( inst.GetRepr() == CSeq_inst::eRepr_delta  &&  num_graphs > 1 ) {
        ValidateGraphOnDeltaBioseq(seq, validate_values);
    }

    if ( validate_values ) {
        for ( CGraph_CI g(bsh); g; ++g ) {
            ValidateGraphValues(g->GetOriginalGraph(), seq);
        }
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
    ValidateMinValues(bg);
    ValidateMaxValues(bg);

    TSeqPos numval = graph.GetNumval();
    if ( numval != bg.GetValues().size() ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphByteLen,
            "SeqGraph (" + NStr::IntToString(numval) + ") " + 
            "and ByteStore (" + NStr::IntToString(bg.GetValues().size()) +
            ") length mismatch", seq, graph);
    }
}


// Currently we support only phrap, phred or gap4 types with byte values.
bool CValidError_bioseq::IsSuportedGraphType(const CSeq_graph& graph) const
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


void CValidError_bioseq::ValidateGraphValues
(const CSeq_graph& graph,
 const CBioseq& seq)
{
    SIZE_TYPE Ns_with_score = 0,
        gaps_with_score = 0,
        ACGTs_without_score = 0,
        vals_below_min = 0,
        vals_above_max = 0;
    int first_N = -1,
        first_ACGT = -1;

    const CByte_graph& bg = graph.GetGraph().GetByte();
    int min = bg.GetMin();
    int max = bg.GetMax();

    const CSeq_loc& gloc = graph.GetLoc();
    CSeqVector vec(gloc, *m_Scope,
                   CBioseq_Handle::eCoding_Ncbi,
                   GetStrand(gloc, m_Scope));
    vec.SetCoding(CSeq_data::e_Ncbi4na);

    CSeqVector::const_iterator seq_begin = vec.begin();
    CSeqVector::const_iterator seq_end   = vec.end();
    CSeqVector::const_iterator seq_iter  = seq_begin;

    const CByte_graph::TValues& values = bg.GetValues();
    CByte_graph::TValues::const_iterator val_iter = values.begin();
    CByte_graph::TValues::const_iterator val_end = values.end();
    
    while ( (seq_iter != seq_end)  &&  (val_iter != val_end) ) {
        CSeqVector::TResidue res = *seq_iter;
        int val;
        if ( IsResidue(res) ) {
            val = *val_iter;
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
                        first_ACGT = seq_iter.GetPos();
                    }
                }
                break;

            case 15:    // N
                if ( val > 0 ) {
                    Ns_with_score++;
                    if ( first_N == -1 ) {
                        first_N = seq_iter.GetPos();
                    }
                }
                break;
            }
        }
        ++seq_iter;
        ++val_iter;
    }

    if ( ACGTs_without_score > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphACGTScore, 
            NStr::IntToString(ACGTs_without_score) + 
            " ACGT bases have zero score value - first one at position " +
            NStr::IntToString(first_ACGT), seq, graph);
    }
    if ( Ns_with_score > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphNScore,
            NStr::IntToString(Ns_with_score) +
            " N bases have positive score value - first one at position " + 
            NStr::IntToString(first_N), seq, graph);
    }
    if ( gaps_with_score > 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphGapScore,
            NStr::IntToString(gaps_with_score) + 
            " gap bases have positive score value", 
            seq, graph);
    }
    if ( vals_below_min > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphBelow,
            NStr::IntToString(vals_below_min) + 
            " quality scores have values below the reported minimum or 0", 
            seq, graph);
    }
    if ( vals_above_max > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphAbove,
            NStr::IntToString(vals_above_max) + 
            " quality scores have values above the reported maximum or 100", 
            seq, graph);
    }
}


void CValidError_bioseq::ValidateMinValues(const CByte_graph& graph)
{
    char min = graph.GetMin();
    if ( min < 0  ||  min > 100 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphMin, 
            "Graph min (" + NStr::IntToString(min) + ") out of range",
            graph);
    }
}


void CValidError_bioseq::ValidateMaxValues(const CByte_graph& graph)
{
    char max = graph.GetMax();
    if ( max <= 0  ||  max > 100 ) {
        EDiagSev sev = (max <= 0) ? eDiag_Error : eDiag_Warning;
        PostErr(sev, eErr_SEQ_GRAPH_GraphMax, 
            "Graph max (" + NStr::IntToString(max) + ") out of range",
            graph);
    }
}


void CValidError_bioseq::ValidateGraphOnDeltaBioseq
(const CBioseq& seq,
 bool& validate_values)
{
    const CDelta_ext& delta = seq.GetInst().GetExt().GetDelta();
    CDelta_ext::Tdata::const_iterator curr = delta.Get().begin(),
        next = curr,
        end = delta.Get().end();

    SIZE_TYPE   num_delta_seq = 0;
    TSeqPos offset = 0;

    CGraph_CI grp(m_Scope->GetBioseqHandle(seq));
    while ( curr != end  &&  grp ) {
        ++next;
        const CSeq_loc& loc = grp->GetLoc();
        switch ( (*curr)->Which() ) {
            case CDelta_seq::e_Loc:
                if ( !loc.IsNull() ) {
                    TSeqPos loclen = GetLength(loc, m_Scope);
                    if ( grp->GetNumval() != loclen ) {
                        validate_values = false;
                        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphSeqLocLen,
                            "SeqGraph (" + NStr::IntToString(grp->GetNumval()) +
                            ") and SeqLoc (" + NStr::IntToString(loclen) + 
                            ") length mismatch", grp->GetOriginalGraph());
                    }
                    offset += loclen;
                    ++num_delta_seq;
                }
                ++grp;
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
                        if ( grp->GetNumval() != litlen ) {
                            validate_values = false;
                            PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphSeqLitLen,
                                "SeqGraph (" + NStr::IntToString(grp->GetNumval()) +
                                ") and SeqLit (" + NStr::IntToString(litlen) + 
                                ") length mismatch", grp->GetOriginalGraph());
                        }
                        if ( loc.IsInt() ) {
                            TSeqPos from = loc.GetTotalRange().GetFrom();
                            TSeqPos to = loc.GetTotalRange().GetTo();
                            if (  from != offset ) {
                                validate_values = false;
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStartPhase,
                                    "SeqGraph (" + NStr::IntToString(from) +
                                    ") and SeqLit (" + NStr::IntToString(offset) +
                                    ") start do not coincide", 
                                    grp->GetOriginalGraph());
                            }
                            
                            if ( to != offset + litlen - 1 ) {
                                validate_values = false;
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStopPhase,
                                    "SeqGraph (" + NStr::IntToString(to) +
                                    ") and SeqLit (" + 
                                    NStr::IntToString(litlen + offset - 1) +
                                    ") stop do not coincide", 
                                    grp->GetOriginalGraph());
                                
                            }
                        }
                        ++grp;
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

    SIZE_TYPE num_graphs = 0;
    for ( CGraph_CI i(m_Scope->GetBioseqHandle(seq)); i; ++i ) {
        ++num_graphs;
    }

    if ( num_delta_seq != num_graphs ) {
        validate_values = false;
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphDiffNumber,
            "Different number of SeqGraph (" + 
            NStr::IntToString(num_delta_seq) + ") and SeqLit (" +
            NStr::IntToString(num_graphs) + ") components",
            seq);
    }
}


bool CValidError_bioseq::GetLitLength(const CDelta_seq& delta, TSeqPos& len)
{
    len = 0;
    if ( delta.IsLiteral() ) {
        const CSeq_literal& lit = delta.GetLiteral();
        if ( lit.IsSetSeq_data() ) {
            len = lit.GetLength();
            return true;
        }
    }
    return false;
}


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
                if ( (*dseq)->GetLiteral().IsSetSeq_data() ) {
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
    return CGraph_CI(m_Scope->GetBioseqHandle(seq));
}


bool CValidError_bioseq::x_IsActiveFin(const CBioseq& seq) const
{
    CSeqdesc_CI gb_desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Genbank);
    if ( gb_desc ) {
        const CGB_block& gb = gb_desc->GetGenbank();
        if ( gb.IsSetKeywords() ) {
            ITERATE( CGB_block::TKeywords, iter, gb.GetKeywords() ) {
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
    CSeqportUtil::Convert(lit_data, &data, CSeq_data::e_Iupacna);

    size_t count = 0;
    size_t max = 0;
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


bool s_HasTpaUserObject(const CBioseq& seq, CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(seq);

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
    if ( !s_HasTpaUserObject(seq, *m_Scope) ) {
        return;
    }

    if ( seq.CanGetInst()  &&  
         seq.GetInst().CanGetHist()  &&
         !seq.GetInst().GetHist().GetAssembly().empty() ) {
        ++m_TpaWithHistory;
    } else {
        ++m_TpaWithoutHistory;
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.95  2005/03/14 18:19:02  grichenk
* Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
* SAnnotSelector.
*
* Revision 1.94  2005/02/18 15:07:10  shomrat
* CSeq_loc interface changes
*
* Revision 1.93  2005/01/24 17:16:48  vasilche
* Safe boolean operators.
*
* Revision 1.92  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.91  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.90  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.89  2004/11/01 19:33:10  grichenk
* Removed deprecated methods
*
* Revision 1.88  2004/09/22 13:53:10  shomrat
* Added eErr_SEQ_INST_BadHTGSeq
*
* Revision 1.87  2004/09/21 18:37:02  shomrat
* Added ValidateLocusTagGeneral, reports eErr_SEQ_FEAT_LocusTagProductMismatch
*
* Revision 1.86  2004/09/21 15:49:41  shomrat
* Added ValidateCDSmRNAmatch reporting eErr_SEQ_FEAT_CDSmRNAmismatch
*
* Revision 1.85  2004/09/01 15:33:44  grichenk
* Check strand in GetStart and GetEnd. Circular length argument
* made optional.
*
* Revision 1.84  2004/08/09 14:55:40  shomrat
* Added title validation
*
* Revision 1.83  2004/07/29 17:11:37  shomrat
* Added test for transgenic problem
*
* Revision 1.82  2004/07/12 13:20:15  shomrat
* suppress DuplicateFeat warning if partialL or partialR are different and the labels are different
*
* Revision 1.81  2004/06/25 16:01:46  shomrat
* Added test for missing gaps in delta sequences
*
* Revision 1.80  2004/06/17 17:07:47  shomrat
* TGA can be used for Selenocysteine without needing modified codon recognition exception; stopped checking for length >350000
*
* Revision 1.79  2004/05/26 14:53:30  shomrat
* removed non-preferred variants ribosome slippage, trans splicing, alternate processing, and non-consensus splice site
*
* Revision 1.78  2004/05/26 14:06:37  shomrat
* SEQ_INST_BadSeqIdFormat allow 2 letters + underscore + 9 digits
*
* Revision 1.77  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.76  2004/05/10 13:57:19  shomrat
* Use CSeq_feat_Handle instead of FeatAnnotMap
*
* Revision 1.75  2004/05/10 13:21:42  shomrat
* Implemented validation for abutting UTRs on any nucleotide bioseq (not just mRNA)
*
* Revision 1.74  2004/04/27 18:41:51  shomrat
* AddProtWithoutFullRef takes CBioseq_Handle
*
* Revision 1.73  2004/04/27 18:26:04  shomrat
* do not report NoProtRefFound if virtual, or if a segmented part where the parent has a best protein feature
*
* Revision 1.72  2004/04/23 16:27:00  shomrat
* Stop using CBioseq_Handle deprecated interface
*
* Revision 1.71  2004/04/23 13:41:33  shomrat
* InternalNsInSeqLit gives more informative message
*
* Revision 1.70  2004/04/05 15:56:15  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.69  2004/04/02 22:20:22  shomrat
* UnwantedCompleteFlag is Warning if tech is htgs_3
*
* Revision 1.68  2004/04/02 22:09:15  shomrat
* clarify explanation of GraphAbove and GraphBelow criteria
*
* Revision 1.67  2004/03/31 18:37:40  shomrat
* Report GraphMax > 100 at WARNING level, <= 0 at ERROR level
*
* Revision 1.66  2004/03/25 18:33:07  shomrat
* + ValidatemRNABioseqContext()
*
* Revision 1.65  2004/03/24 18:58:14  vasilche
* Fixed for new API.
*
* Revision 1.64  2004/03/11 16:37:00  shomrat
* Gene is invalid feature for a protein bioseq only if not in a nuc-prot, mut/phy/pop/eco or gen-prod set
*
* Revision 1.63  2004/03/10 21:23:51  shomrat
* Check for unwanted complete flag
*
* Revision 1.62  2004/02/26 13:58:09  shomrat
* Do not complain about length > 350000 if NT or NC
*
* Revision 1.61  2004/02/25 15:54:43  shomrat
* Added checks for colliding gene locus_tag; Changed illegal GI severity to Critical
*
* Revision 1.60  2004/02/09 19:18:57  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.59  2003/12/16 17:36:27  shomrat
* implemented ERR_SEQ_INST_SeqLocLength test
*
* Revision 1.58  2003/12/15 21:08:16  shomrat
* artificial, mutagenized, or synthetic suppress other-genetic message
*
* Revision 1.57  2003/12/15 20:42:16  shomrat
* segmented bioseq doesn't need TPA user object; Terminal Ns set to warning if NC record
*
* Revision 1.56  2003/11/20 15:56:47  ucko
* Update for new (saner) treatment of ASN.1 NULLs.
*
* Revision 1.55  2003/11/14 15:57:58  shomrat
* added check for TPA assembly problem
*
* Revision 1.54  2003/11/12 20:30:24  shomrat
* added test for multiple cds on mrna
*
* Revision 1.53  2003/10/27 17:02:49  shomrat
* fixes to 3'UTR and 5'UTR checks
*
* Revision 1.52  2003/10/27 14:55:19  shomrat
* added test for eErr_SEQ_FEAT_UTRdoesNotAbutCDS on mRNA bioseqs
*
* Revision 1.51  2003/10/27 14:16:50  shomrat
* added ERR_SEQ_INST_SeqLitGapLength0
*
* Revision 1.50  2003/10/21 13:48:50  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.49  2003/10/20 18:31:19  shomrat
* suppress validator warning for synthetic sequences with molinfo other genetic when origin is artificial
*
* Revision 1.48  2003/10/01 20:21:45  shomrat
* fixed IsSynthetic to look at div if origin was not set
*
* Revision 1.47  2003/09/23 13:26:32  shomrat
* Check DeltaLitOnly and allow test for terminal Ns
*
* Revision 1.46  2003/09/22 20:24:32  shomrat
* IsMicroRNA feature test for molinfo-biomol.other
*
* Revision 1.45  2003/09/04 13:45:58  shomrat
* Changed ValidateGraphValues to work with CSeqVector_CI
*
* Revision 1.44  2003/08/06 15:06:08  shomrat
* Added test for adjacent Ns in Seq-literal
*
* Revision 1.43  2003/07/24 20:16:18  vasilche
* Fixed typedefs for dbxref: list<CRef<CDbtag>> -> vector<CRef<CDbtag>>
*
* Revision 1.42  2003/07/15 18:26:03  shomrat
* suppress BadDeltaSeq - HTGS 2 delta seq has no gaps and no graphs - if HTGS_ACTIVEFIN keyword present
*
* Revision 1.41  2003/07/07 18:48:46  shomrat
* SEQ_INST_TerminalNs is eDiag_Error if 10 or more Ns at either end
*
* Revision 1.40  2003/07/02 21:05:25  shomrat
* removed unused variables
*
* Revision 1.39  2003/06/19 13:55:36  shomrat
* synthetic biosource should have molinfo biomol other
*
* Revision 1.38  2003/06/18 20:50:41  shomrat
* NT-036298 (dash instead of underscore) gives Critical level BadSeqIdFormat error
*
* Revision 1.37  2003/06/17 13:40:30  shomrat
* Use SeqVector_CI to improve performance
*
* Revision 1.36  2003/06/02 16:06:43  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.35  2003/05/28 16:25:22  shomrat
* Error messages contain original feature (not mapped).
*
* Revision 1.34  2003/05/14 21:48:18  shomrat
* Bug fix in ValidateMultiIntervalGene
*
* Revision 1.33  2003/05/14 20:35:11  shomrat
* Bug fixes
*
* Revision 1.32  2003/04/29 14:51:38  shomrat
* Bug fix in ValidateSeqParts
*
* Revision 1.31  2003/04/10 19:32:50  shomrat
* Added ValidateMolTypeContext; UnnecessaryBioSourceFocus not done on proteins, segmented bioseqs, or segmented parts
*
* Revision 1.30  2003/04/07 14:58:16  shomrat
* Added information to error postings
*
* Revision 1.29  2003/04/04 18:43:16  shomrat
* Increased robustness in face of exceptions
*
* Revision 1.28  2003/03/31 14:40:36  shomrat
* $id: -> $id$
*
* Revision 1.27  2003/03/31 14:25:39  shomrat
* Bail if no graphs present
*
* Revision 1.26  2003/03/28 18:03:20  shomrat
* dropped call to min due to compilation error on some platforms
*
* Revision 1.25  2003/03/28 16:31:18  shomrat
* Added Seq-graph validation
*
* Revision 1.24  2003/03/21 21:14:13  shomrat
* Added check for Seq-ids on proteins; Implemented eErr_SEQ_DESCR_UnnecessaryBioSourceFocus
*
* Revision 1.23  2003/03/21 16:27:48  shomrat
* Added ValidateIDSetAgainstDb
*
* Revision 1.22  2003/03/18 21:48:37  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.21  2003/03/11 16:04:09  kuznets
* iterate -> ITERATE
*
* Revision 1.20  2003/02/26 19:45:31  shomrat
* Check for whole range
*
* Revision 1.19  2003/02/24 20:34:47  shomrat
* Added default to supress warning in gcc 2.95
*
* Revision 1.18  2003/02/19 21:59:32  shomrat
* Set coding to ncbieaa, only if bioseq is AA (bug fix)
*
* Revision 1.17  2003/02/14 21:48:47  shomrat
* Implemented check for Bioseqs with no MolInfo; Bug fix in use of CSeqVector
*
* Revision 1.16  2003/02/12 18:01:07  shomrat
* Implemented ValidateSeqDescContext; and a few bug fixes
*
* Revision 1.15  2003/02/10 15:54:02  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.14  2003/02/07 21:22:12  shomrat
* Implemented eErr_SEQ_INST_TerminalNs and bug fixes
*
* Revision 1.13  2003/02/03 18:01:43  shomrat
* Minor fixes & style improvments
*
* Revision 1.12  2003/01/31 18:41:51  shomrat
* Bug fix in CdError
*
* Revision 1.11  2003/01/30 20:26:01  shomrat
* Explicitly call sequence::Compare
*
* Revision 1.10  2003/01/29 21:56:31  shomrat
* Added check for id on multiple bioseqs; Bug fixes
*
* Revision 1.9  2003/01/29 17:19:55  ucko
* Use TTech (with no cast) rather than ETech, in preparation for CIntEnum<>.
*
* Revision 1.8  2003/01/28 15:38:35  shomrat
* Bug fix in ValidateSeqIds; Performance improvments in ValidateDupOrOverlapFeats
*
* Revision 1.7  2003/01/24 20:41:15  shomrat
* Added ValidateHistory and more checks in ValidateSeqIds
*
* Revision 1.6  2003/01/08 18:37:25  shomrat
* Implemented IsSameSeqAnnot and IsSameSeqAnnotDesc
*
* Revision 1.5  2003/01/06 16:43:42  shomrat
* variable intialization
*
* Revision 1.4  2003/01/02 22:14:52  shomrat
* Add GetNucGivenProt; Implemented CdError, ValidateMultiIntervalGene; GetCDSGivenProduct moved to ValidError_imp
*
* Revision 1.3  2002/12/30 23:46:22  vakatov
* Use "CNcbiOstrstreamToString" for more correct strstream-to-string conversion
*
* Revision 1.2  2002/12/24 16:53:11  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:16:11  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
