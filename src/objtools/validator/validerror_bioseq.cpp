/*  $Id:
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
 *   validation of bioseq 
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Bioseq.hpp>
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

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>

#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/seq_vector.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/util/sequence.hpp>
#include <objects/util/feature.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;
using namespace feature;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseq::CValidError_bioseq(CValidError_imp& imp) :
    CValidError_base(imp)
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

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NT, or NC. Check that the same CSeq_id not included more
    // than once.
    bool has_gi = false;
    iterate( CBioseq::TId, i, seq.GetId() ) {

        // Check that no two CSeq_ids for same CBioseq are same type
        list< CRef < CSeq_id > >::const_iterator j;
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
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    iterate (CBioseq::TId, k, seq.GetId()) {
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
                    
                iterate (string, s, acc) {
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
                if ( letter_after_digit || bad_id_chars ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession: " + acc, seq);
                } else if (num_letters == 1 && num_digits == 5 && seq.IsNa()) {
                } else if (num_letters == 2 && num_digits == 6 && seq.IsNa()) {
                } else if (num_letters == 3 && num_digits == 5 && seq.IsAa()) {
                } else if (num_letters == 2 && num_digits == 6 && seq.IsAa() &&
                    seq.GetInst().GetRepr() == CSeq_inst:: eRepr_seg) {
                } else if (num_letters == 4  &&  num_digits == 8  &&
                    ((**k).IsGenbank()  ||  (**k).IsEmbl()  ||
                    (**k).IsDdbj())) {
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
                    iterate (string, s, name) {
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
                    } else if ( num_letters == 2  &&  num_digits == 6  &&
                        num_underscores == 1 ) {
                    } else if ( num_letters == 2  &&  num_digits == 8  &&
                        num_underscores == 1 ) {
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, seq);
                    }
                }

                if ( has_gi && !tsid->IsSetAccession() && tsid->IsSetName() ) {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
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
                    if ( seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg  ||
                        m_Imp.IsGI()) {
                        if (!(**k).IsDdbj()  ||
                            seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
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
            if ((**k).GetGi() == 0) {
                PostErr(eDiag_Error, eErr_SEQ_INST_ZeroGiNumber,
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

    // Check that a sequence with a gi number has exactly one accession
    if (gi_count > 0  &&  accn_count == 0) {
        PostErr(eDiag_Error, eErr_SEQ_INST_GiWithoutAccession,
            "No accession on sequence with gi number", seq);
    }
    if (gi_count > 0  &&  accn_count > 1) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MultipleAccessions,
            "Multiple accessions on sequence with gi number", seq);
    }

    // C toolkit ensures that there is exactly one CBioseq for a CSeq_id
    // Not done here because object manager will not allow
    // the same Seq-id on multiple Bioseqs

}


bool CValidError_bioseq::IsHistAssemblyMissing(const CBioseq& seq)
{
    return (seq.GetInst().IsSetHist()  &&
           seq.GetInst().GetHist().IsSetAssembly());
}


void CValidError_bioseq::ValidateSecondaryAccConflict
(const string &primary_acc,
 const CBioseq &seq,
 int choice)
{
    CDesc_CI ds(m_Scope->GetBioseqHandle(seq));
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
            iterate( list<string>, acc, *extra_acc ) {
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
            if (inst.IsSetTopology()  &&
              inst.GetTopology() != CSeq_inst::eTopology_linear)
            {
                PostErr(eDiag_Error, eErr_SEQ_INST_CircularProtein,
                         "Non-linear topology set on protein", seq);
            }
            if (inst.IsSetStrand()  &&
              inst.GetStrand() != CSeq_inst::eStrand_ss)
            {
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

    if (seq.IsAa()) {
        // Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }

    // Validate sequence length
    ValidateSeqLen(seq);
}


void CValidError_bioseq::ValidateBioseqContext(const CBioseq& seq)
{
    // Get Molinfo
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));

    if ( mi  &&  mi->IsSetTech()) {
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
    
    // Check that proteins in nuc_prot set have a CdRegion
    if ( seq.IsAa()  &&  CdError(seq) ) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", seq);
    }

    // Check that gene on non-segmented sequence does not have
    // multiple intervals
    ValidateMultiIntervalGene(seq);

    ValidateSeqFeatContext(seq);

    // Check for duplicate features and overlapping peptide features.
    ValidateDupOrOverlapFeats(seq);

    // Check for colliding gene names
    ValidateCollidingGeneNames(seq);

    ValidateSeqDescContext(seq);

    // make sure that there is a pub on this bioseq
    if ( !m_Imp.IsNoPubs() ) {  
        CheckForPubOnBioseq(seq);
    }
    // make sure that there is a source on this bioseq
    if ( !m_Imp.IsNoBioSource() ) { 
        CheckForBiosourceOnBioseq(seq);
    }
}


void CValidError_bioseq::ValidateHistory(const CBioseq& seq)
{
    if ( !seq.GetInst().IsSetHist() ) {
        return;
    }
    
    int gi = 0;
    iterate( CBioseq::TId, id, seq.GetId() ) {
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
        iterate( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
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
        iterate( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
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


bool CValidError_bioseq::IsDifferentDbxrefs(const list< CRef< CDbtag > >& list1,
                        const list< CRef< CDbtag > >& list2)
{
    if (list1.empty()  ||  list2.empty()) {
        return false;
    } else if (list1.size() != list2.size()) {
        return true;
    }

    list< CRef<CDbtag> >::const_iterator it1 = list1.begin();
    list< CRef<CDbtag> >::const_iterator it2 = list2.begin();
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
    iterate (CBioseq::TId, it, seq.GetId()) {
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
    iterate (list< CRef<CSeq_loc> >, it,
        seq.GetInst().GetExt().GetSeg().Get()) {
        mix.Set().push_back(*it);
    }
    return true;
}


// Check if CdRegion required but not found
bool CValidError_bioseq::CdError(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh  &&  seq.IsAa() ) {
        const CSeq_entry* nps = 
            m_Imp.GetAncestor(seq, CBioseq_set::eClass_nuc_prot);
        if ( nps ) {
            CFeat_CI cds(bsh, 
                         0, 0,
                         CSeqFeatData::e_Cdregion,
                         CAnnot_CI::eOverlap_Intervals,
                         CFeat_CI::eResolve_TSE,
                         CFeat_CI::e_Product,
                         nps);  // restrict search to the nuc-prot set.
            if ( cds ) {
                return false;
            }
        }
    }

    return true;
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
        ++counter;
    }
    return counter;
}


bool CValidError_bioseq::LocOnSeg(const CBioseq& seq, const CSeq_loc& loc) 
{
    for ( CSeq_loc_CI sli( loc ); sli;  ++sli ) {
        const CSeq_id& loc_id = sli.GetSeq_id();
        iterate(  CBioseq::TId, seq_id, seq.GetId() ) {
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
    string  alternative = "alternative processing",
            alternate   = "alternate processing";
    
    if ( curr->GetExcept() ) {
        if ( NStr::FindNoCase(curr->GetExcept_text(), alternative) != string::npos ||
             NStr::FindNoCase(curr->GetExcept_text(), alternate) != string::npos) {
            return false;
        }
    }
    if ( prev->GetExcept() ) {
        if ( NStr::FindNoCase(prev->GetExcept_text(), alternative) != string::npos ||
             NStr::FindNoCase(prev->GetExcept_text(), alternate) != string::npos ) {
            return false;
        }
    }
    return true;
}


bool CValidError_bioseq::IsSameSeqAnnot
(const CFeat_CI& fi1,
 const CFeat_CI& fi2)
{
    CValidError_imp::TFeatAnnotMap fa_map = m_Imp.GetFeatAnnotMap();

    return fa_map[&(*fi1)] == fa_map[&(*fi2)];
}


bool CValidError_bioseq::IsSameSeqAnnotDesc
(const CFeat_CI& fi1,
 const CFeat_CI& fi2)
{
    CValidError_imp::TFeatAnnotMap fa_map = m_Imp.GetFeatAnnotMap();
    const CSeq_annot* annot1 = fa_map[&(*fi1)];
    const CSeq_annot* annot2 = fa_map[&(*fi2)];

    if ( !(annot1->IsSetDesc())  ||  !(annot2->IsSetDesc()) ) {
        return true;
    }

    CAnnot_descr::Tdata descr1 = annot1->GetDesc().Get();
    CAnnot_descr::Tdata descr2 = annot2->GetDesc().Get();

    // !!! Check only on the first? (same as in C toolkit)
    const CAnnotdesc& desc1 = descr1.begin()->GetObject();
    const CAnnotdesc& desc2 = descr2.begin()->GetObject();

    if ( desc1.Which() == desc2.Which() ) {
        if ( desc1.Which() == CAnnotdesc::e_Title   ||
            desc1.Which() == CAnnotdesc::e_Name ) {
            string str1, str2;
            if ( desc1.IsName() ) {
                str1 = desc1.GetName();
                str2 = desc2.GetName();
            }
            if ( desc1.IsTitle() ) {
                str1 = desc1.GetTitle();
                str2 = desc2.GetTitle();
            }

            return (NStr::CompareNocase(str1, str2) == 0);
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

    if (len <= 350000) {
        return;
    }

    CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
    if (inst.GetRepr() == CSeq_inst::eRepr_delta) {
        if ( mi  &&  mi->IsSetTech()  &&  m_Imp.IsGED() ) {
            CMolInfo::TTech tech = mi->GetTech();
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
                if (len > 350000  && litHasData) {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_LongLiteralSequence,
                        "Length of sequence literals exceeds 350kbp limit",
                        seq);
                }
            }
        }

    } else if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
        if (mi  &&  mi->IsSetTech()) {
            CMolInfo::TTech tech = mi->GetTech();
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
        }
    }
}


// Assumes that seq is segmented and has Seq-ext data
void CValidError_bioseq::ValidateSeqParts(const CBioseq& seq)
{
    // Get parent CSeq_entry of seq
    const CSeq_entry* parent = seq.GetParentEntry();
    if (!parent) {
        return;
    }

    // Need to get seq-set containing parent and then find the next
    // CSeq_entry in the set. This CSeq_entry should be a CBioseq_set
    // of class parts.
    const CSeq_entry* grand_parent = parent->GetParentEntry();
    if (!grand_parent  ||  !grand_parent->IsSet()) {
        return;
    }
    const CBioseq_set& set = grand_parent->GetSet();
    const list< CRef<CSeq_entry> >& seq_set = set.GetSeq_set();

    // Loop through seq_set looking for parent. When found, the next
    // CSeq_entry should have the parts
    const CSeq_entry* se = 0;
    iterate (list< CRef<CSeq_entry> >, it, seq_set) {
        if (parent == &(**it)) {
            ++it;
            se = &(**it);
            break;
        }
    }
    if (!se) {
        return;
    }

    // Check that se is a CBioseq_set of class parts
    if (!se->IsSet()  ||  !se->GetSet().IsSetClass()  ||
        se->GetSet().GetClass() != CBioseq_set::eClass_parts) {
        return;
    }

    // Get iterator for CSeq_entries in se
    CTypeConstIterator<CSeq_entry> se_parts(ConstBegin(*se));

    // Now, simultaneously loop through the parts of se and CSeq_locs of seq's
    // CSseq-ext. If don't compare, post error
    const list< CRef<CSeq_loc> > locs = seq.GetInst().GetExt().GetSeg().Get();
    iterate (list< CRef<CSeq_loc> >, lit, locs) {
        if ((**lit).Which() == CSeq_loc::e_Null) {
            continue;
        }
        if (!se_parts) {
            PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set does not contain enough Bioseqs", seq);
        }
        if (se_parts->Which() == CSeq_entry::e_Seq) {
            try {
                const CBioseq& part = se_parts->GetSeq();
                const CSeq_id& id = GetId(**lit, m_Scope);
                if (!IsIdIn(id, part)) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                        "Segmented bioseq seq_ext does not correspond to parts"
                        "packaging order", seq);
                }
            } catch (const CNotUnique&) {
                ERR_POST("Seq-loc not for unique sequence");
                return;
            } catch (...) {
                ERR_POST("Unknown error");
                return;
            }
        } else {
            PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set component is not Bioseq", seq);
            return;
        }
        ++se_parts;
    }
    if (se_parts) {
        PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
            "Parts set contains too many Bioseqs", seq);
    }
}


// Assumes seq is an amino acid sequence
void CValidError_bioseq::ValidateProteinTitle(const CBioseq& seq)
{
    const CSeq_id* id = seq.GetFirstId();
    if (!id) {
        return;
    }

    CBioseq_Handle hnd = m_Scope->GetBioseqHandle(*id);
    string title_no_recon = GetTitle(hnd);
    string title_recon = GetTitle(hnd, fGetTitle_Reconstruct);
    if (title_no_recon != title_recon) {
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentProteinTitle,
            "Instantiated protein title does not match automatically "
            "generated title", seq);
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
    string s_len = NStr::IntToString(calc_len);
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
    TSeqPos data_len = GetDataLen(inst);
    string s_data_len = NStr::IntToString(data_len);
    if (calc_len > data_len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data too short [" + s_data_len +
                 "] for given length [" + s_len + "]", seq);
        return;
    } else if (calc_len < data_len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data is larger [" + s_data_len +
                 "] than given length [" + s_len + "]", seq);
    }

    unsigned char termination;
    unsigned int trailingX = 0;
    unsigned int terminations = 0;
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

        CSeqVector sv = m_Scope->GetBioseqHandle(seq).GetSeqVector();

        unsigned int bad_cnt = 0;
        for (TSeqPos pos = 0; pos < sv.size(); pos++) {
            if (sv[pos] > 250) {
                if (++bad_cnt > 10) {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                        "More than 10 invalid residues. Checking stopped",
                        seq);
                    return;
                } else {
                    if (seqtyp == CSeq_data::e_Ncbistdaa) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    } else {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    }
                }
            } else if (sv[pos] == termination) {
                terminations++;
                trailingX = 0;
            } else if (sv[pos] == 'X') {
                trailingX++;
            } else {
                trailingX = 0;
            }
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

        if (terminations  && seqtyp != CSeq_data::e_Iupacna) {
            // Post error indicating terminations found in protein sequence
            // First get gene label
            string glbl;
            seq.GetLabel(&glbl, CBioseq::eContent);         
            string plbl;
            const CBioseq* nuc = GetNucGivenProt(seq);
            if ( nuc ) {
                nuc->GetLabel(&plbl, CBioseq::eContent);
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
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CSeq_loc loc;
    if ( GetLocFromSeq(seq, &loc) ) {
        m_Imp.ValidateSeqLoc(loc, seq, "Segmented Bioseq");
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
    } catch (const CNoLength&) {
        ERR_POST(Critical << "Unable to calculate length: ");
    }

    // Check for multiple references to the same Bioseq
    if (inst.IsSetExt()  &&  inst.GetExt().IsSeg()) {
        const list< CRef<CSeq_loc> >& locs = inst.GetExt().GetSeg().Get();
        iterate(list< CRef<CSeq_loc> >, i1, locs) {
           if (!IsOneBioseq(**i1, m_Scope)) {
                continue;
            }
            const CSeq_id& id1 = GetId(**i1);
            list< CRef<CSeq_loc> >::const_iterator i2 = i1;
            for (++i2; i2 != locs.end(); ++i2) {
                if (!IsOneBioseq(**i2, m_Scope)) {
                    continue;
                }
                const CSeq_id& id2 = GetId(**i2);
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
            iterate(list< EGIBB_mod >, md, (*sd).GetModif()) {
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

    if (!inst.IsSetExt()  ||  !inst.GetExt().IsDelta()  ||
        inst.GetExt().GetDelta().Get().empty()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No CDelta_ext data for delta Bioseq", seq);
    }

    TSeqPos len = 0;
    iterate(list< CRef<CDelta_seq> >, sg, inst.GetExt().GetDelta().Get()) {
        switch ((**sg).Which()) {
        case CDelta_seq::e_Loc:
            try {
                len += GetLength((**sg).GetLoc(), m_Scope);
            } catch (const CNoLength&) {
                PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                    "No length for Seq-loc of delta seq-ext", seq);
            }
            break;
        case CDelta_seq::e_Literal:
        {
            // The C toolkit code checks for valid alphabet here
            // The C++ object serializaton will not load if invalid alphabet
            // so no check needed here

            // Check for invalid residues
            if (!(**sg).GetLiteral().IsSetSeq_data()) {
                break;
            }
            const CSeq_data& data = (**sg).GetLiteral().GetSeq_data();
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
                iterate (vector<TSeqPos>, ci, badIdx) {
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
            if (ss) {
                iterate (vector<TSeqPos>, it, badIdx) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                        "Invalid residue [" +
                        ss->substr(*it, 1) + "] in position " +
                        NStr::IntToString(*it), seq);
                }
            }
            len += (**sg).GetLiteral().GetLength();
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

    // Get CMolInfo and tech used for validating technique and gap positioning
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq.GetDescr()));
    int tech = mi->IsSetTech() ? mi->GetTech() : 0;

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

    // Validate positioning of gaps
    CTypeConstIterator<CDelta_seq> ds(ConstBegin(inst));
    if (ds  &&  (*ds).IsLiteral()  &&  !(*ds).GetLiteral().IsSetSeq_data()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "First delta seq component is a gap", seq);
    }
    bool last_is_gap = false;
    unsigned int num_adjacent_gaps = 0;
    unsigned int num_gaps = 0;
    for (++ds; ds; ++ds) {
        if ((*ds).IsLiteral()) {
            if (!(*ds).GetLiteral().IsSetSeq_data()) {
                if (last_is_gap) {
                    num_adjacent_gaps++;
                }
                last_is_gap = true;
                num_gaps++;
            } else {
                last_is_gap = false;
            }
        } else {
            last_is_gap = false;
        }
    }
    if (num_adjacent_gaps > 1) {
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "There is (are) " + NStr::IntToString(num_adjacent_gaps) +
            " adjacent gap(s) in delta seq", seq);
    }
    if (last_is_gap) {
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "Last delta seq component is a gap", seq);
    }
    if (num_gaps == 0  &&  mi) {
        if (tech == CMolInfo::eTech_htgs_1  ||
            tech == CMolInfo::eTech_htgs_2) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadDeltaSeq,
                "HTGS 1 or 2 delta seq has no gaps", seq);
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


void CValidError_bioseq::CheckForBiosourceOnBioseq(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if ( !di ) {
        m_Imp.AddBioseqWithNoBiosource(seq);
    }
}


void CValidError_bioseq::CheckForPubOnBioseq(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }

    // Check for CPubdesc on the biodseq
    CSeqdesc_CI desc( bsh, CSeqdesc::e_Pub );
    if ( desc ) {
        return;
    }
    
    if ( !seq.GetInst().IsSetLength() ) {
        return;
    }

    // check for full cover of the bioseq by pub features.
    TSeqPos len = bsh.GetSeqVector().size();
    bool covered = false;
    CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();

    for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_Pub); fi; ++fi ) {
        range += fi->GetLocation().GetTotalRange();;
        if ( (fi->GetLocation().IsWhole())  ||
             ((range.GetFrom() == 0)  &&  (range.GetTo() == len - 1)) ) {
            covered = true;
            break;
        }
    }
    if ( !covered ) {
        m_Imp.AddBioseqWithNoPub(seq);
    }
}


void CValidError_bioseq::ValidateMultiIntervalGene(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    
    for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_Gene); fi; ++fi ) {
        CSeq_loc_CI si(fi->GetLocation());

        if ( !(++si) ) {  // if only a single interval
            continue;
        }
        
        EDiagSev sev = eDiag_Error;
        if ( m_Imp.IsNC() ) {
            sev = eDiag_Warning;
        }
        if ( seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular ) {
            sev = eDiag_Info;
        }

        if ( sev != eDiag_Info ) {
            PostErr(sev, eErr_SEQ_FEAT_MultiIntervalGene,
              "Gene feature on non-segmented sequence should not "
              "have multiple intervals", *fi);
        }
    }
}


void CValidError_bioseq::ValidateSeqFeatContext(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    EDiagSev sev = eDiag_Warning;
    bool full_length_prot_ref = false;

    for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_not_set); fi; ++fi ) {
        
        CSeqFeatData::E_Choice ftype = fi->GetData().Which();
        
        if ( seq.IsAa() ) {                // protein
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
                {
                    TSeqPos len = bsh.GetBioseq().GetInst().GetLength();
                    CSeq_loc::TRange range = fi->GetLocation().GetTotalRange();
                    
                    if ( range.GetFrom() == 0 && range.GetTo() == len - 1 ) {
                        full_length_prot_ref = true;
                    }
                }
                break;
                
            case CSeqFeatData::e_Cdregion:
            case CSeqFeatData::e_Rna:
            case CSeqFeatData::e_Rsite:
            case CSeqFeatData::e_Txinit:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a protein Bioseq.", *fi);
                break;
            }
        } else {                            // nucleotide
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
            case CSeqFeatData::e_Psec_str:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a nucleotide Bioseq.", *fi);
                break;
            }
        }
        
        if ( IsMrna(bsh) ) {              // mRNA
            switch ( ftype ) {
            case CSeqFeatData::e_Cdregion:
                if ( NumOfIntervals(fi->GetLocation()) > 1 ) {
                    bool excpet = fi->IsSetExcept()  &&  !fi->GetExcept();
                    string except_text;
                    if ( fi->IsSetExcept_text() ) {
                        except_text = fi->GetExcept_text();
                    }
                    if ( excpet  ||
                         (NStr::FindNoCase(except_text, "ribosomal slippage") == string::npos  &&
                          NStr::FindNoCase(except_text, "ribosome slippage") == string::npos) ) {
                        PostErr(sev, eErr_SEQ_FEAT_InvalidForType,
                            "Multi-interval CDS feature is invalid on an mRNA "
                            "(cDNA) Bioseq.", *fi);
                    }
                }
                break;
                
            case CSeqFeatData::e_Rna:
                {
                    const CRNA_ref& rref = fi->GetData().GetRna();
                    if ( rref.GetType() == CRNA_ref::eType_mRNA ) {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "mRNA feature is invalid on an mRNA (cDNA) Bioseq.",
                            *fi);
                    }
                }
                break;
                
            case CSeqFeatData::e_Imp:
                {
                    const CImp_feat& imp = fi->GetData().GetImp();
                    if ( imp.GetKey() == "intron"  ||
                        imp.GetKey() == "CAAT_signal" ) {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "Invalid feature for an mRNA Bioseq.", *fi);
                    }
                }
                break;
            }
        } else if ( IsPrerna(bsh) ) { // preRNA
            switch ( ftype ) {
            case CSeqFeatData::e_Imp:
                {
                    const CImp_feat& imp = fi->GetData().GetImp();
                    if ( imp.GetKey() == "CAAT_signal" ) {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "Invalid feature for a pre-RNA Bioseq.", *fi);
                    }
                }
                break;
            }
        }

        if ( m_Imp.IsFarLocation(fi->GetLocation())  &&  !m_Imp.IsNC() ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_FarLocation,
                "Feature has 'far' location - accession not packaged in record",
                *fi);
        }

        if ( seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
            if ( LocOnSeg(seq, fi->GetLocation()) ) {
                if ( !IsDeltaOrFarSeg(fi->GetLocation(), m_Scope) ) {
                    sev = m_Imp.IsNC() ? eDiag_Warning : eDiag_Error;
                    PostErr(sev, eErr_SEQ_FEAT_LocOnSegmentedBioseq,
                        "Feature location on segmented bioseq, not on parts", *fi);
                }
            }
        }

    }  // end of for loop

    if ( seq.IsAa()  && !full_length_prot_ref  &&  !m_Imp.IsPDB() ) {
        m_Imp.AddProtWithoutFullRef(seq);
    }
}


void CValidError_bioseq::ValidateDupOrOverlapFeats(const CBioseq& bioseq)
{
    ENa_strand              curr_strand, prev_strand;
    CCdregion::EFrame       curr_frame, prev_frame;
    const CSeq_loc*         curr_location = 0, *prev_location = 0;
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

    CFeat_CI curr(bsh, 0, 0, CSeqFeatData::e_not_set);
    CFeat_CI prev = curr;
    ++curr;
    while ( curr ) {
        curr_location = &(curr->GetLocation());
        prev_location = &(prev->GetLocation());
        curr_subtype = curr->GetData().GetSubtype();
        prev_subtype = prev->GetData().GetSubtype();

        // if same location, subtype and strand
        if ( curr_subtype == prev_subtype  &&
             sequence::Compare(*curr_location, *prev_location, m_Scope) == eSame) {

            curr_strand = GetStrand(*curr_location, m_Scope);
            prev_strand = GetStrand(*prev_location, m_Scope);
            if ( curr_strand == prev_strand         || 
                 curr_strand == eNa_strand_unknown  ||
                 prev_strand == eNa_strand_unknown  ) {

                if ( IsSameSeqAnnot(curr, prev)  ||
                     IsSameSeqAnnotDesc(curr, prev) ) {
                    severity = eDiag_Error;

                    // compare labels and comments
                    same_label = true;
                    const string &curr_comment = curr->GetComment();
                    const string &prev_comment = prev->GetComment();
                    curr_label.erase();
                    prev_label.erase();
                    feature::GetLabel(*curr, &curr_label, feature::eContent, m_Scope);
                    feature::GetLabel(*prev, &prev_label, feature::eContent, m_Scope);
                    if ( NStr::CompareNocase(curr_comment, prev_comment) != 0  ||
                         NStr::CompareNocase(curr_label, prev_label) != 0 ) {
                        same_label = false;
                    }

                    // lower sevrity for the following cases:

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

                    // Report duplicates
                    if ( curr_subtype == CSeqFeatData::eSubtype_region  &&
                        IsDifferentDbxrefs(curr->GetDbxref(), prev->GetDbxref()) ) {
                        // do not report if both have dbxrefs and they are 
                        // different.
                    } else if ( IsSameSeqAnnot(curr, prev) ) {
                        if (same_label) {
                            PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature", *curr);
                        } else if ( curr_subtype != CSeqFeatData::eSubtype_pub ) {
                            PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                "Features have identical intervals, but labels"
                                "differ", *curr);
                        }
                    } else {
                        if (same_label) {
                            PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature (packaged in different feature table)",
                                *curr);
                        } else if ( prev_subtype != CSeqFeatData::eSubtype_pub ) {
                            PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                "Features have identical intervals, but labels"
                                "differ (packaged in different feature table)",
                                *curr);
                        }
                    }
                }

                if ( (curr_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
                      curr_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
                      curr_subtype == CSeqFeatData::eSubtype_transit_peptide_aa)  &&
                     (prev_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
                      prev_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
                      prev_subtype == CSeqFeatData::eSubtype_transit_peptide_aa) ) {
                    if ( sequence::Compare(*curr_location,
					   *prev_location,
					   m_Scope) == eOverlap &&
			 NotPeptideException(curr, prev) ) {
		        EDiagSev overlapPepSev = 
                            m_Imp.IsOvlPepErr()? eDiag_Error :eDiag_Warning;
                        PostErr( overlapPepSev,
                            eErr_SEQ_FEAT_OverlappingPeptideFeat,
                            "Signal, Transit, or Mature peptide features overlap",
                            *curr);
                    }
                }
            }
        }
        ++prev; 
        ++curr;
    }  // end of while loop
}


void CValidError_bioseq::ValidateSeqDescContext(const CBioseq& seq)
{
    // !!! is everything done?
    bool prf_found = false;
    const CDate* create_date_prev = 0;
    
    for (CTypeConstIterator<CSeqdesc> dt(ConstBegin(seq)); dt; ++dt) {
        switch (dt->Which()) {
        case CSeqdesc::e_Prf:
            if (prf_found) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                    "Multiple PRF blocks", *dt);
            } else {
                prf_found = true;
            }
            break;
        case CSeqdesc::e_Create_date:
            if (create_date_prev) {
                if (create_date_prev->Compare(dt->GetCreate_date()) !=
                    CDate::eCompare_same) {
                    string str_date_prev;
                    string str_date;
                    create_date_prev->GetDate(&str_date_prev);
                    dt->GetCreate_date().GetDate(&str_date);
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent create_dates " + str_date +
                        " and " + str_date_prev, *dt);
                }
            } else {
                create_date_prev = &dt->GetCreate_date();
            }

            break;
        default:
            break;
        }
    }
}


class CNoCaseCompare
{
public:
    bool operator ()(const string& s1, const string& s2) const
    {
        return NStr::CompareNocase(s1, s2) < 0;
    }
};


void CValidError_bioseq::ValidateCollidingGeneNames(const CBioseq& seq)
{
    typedef multimap<string, const CSeq_feat*, CNoCaseCompare> TStrFeatMap;

    // Loop through genes and insert into multimap sorted by
    // gene label--case insensitive
    CFeat_CI fi(m_Scope->GetBioseqHandle(seq),
                0, 0,
                CSeqFeatData::e_Gene);
    
    TStrFeatMap label_map;
    string label;
    for (; fi; ++fi) {
        label.erase();
        GetLabel(*fi, &label, feature::eContent, m_Scope);
        label_map.insert(TStrFeatMap::value_type(label, &(*fi)));
    }

    // Iterate through multimap and compare labels
    bool first = true;
    const string* plabel = 0;
    iterate (TStrFeatMap, it, label_map) {
        if (first) {
            first = false;
            plabel = &(it->first);
            continue;
        }
        if ( NStr::Compare(*plabel, it->first) == 0 ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names in gene features", *it->second);
        } else if ( NStr::CompareNocase(*plabel, it->first) == 0 ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names (with different capitalization) in gene"
                "features", *it->second);
        }
        plabel = &(it->first);
    }
}


const CBioseq* CValidError_bioseq::GetNucGivenProt(const CBioseq& prot)
{
    const CSeq_feat* cds = m_Imp.GetCDSGivenProduct(prot);
    if ( cds ) {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(cds->GetLocation());
        if ( bsh ) {
            return &(bsh.GetBioseq());
        }
    }

    return 0;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
