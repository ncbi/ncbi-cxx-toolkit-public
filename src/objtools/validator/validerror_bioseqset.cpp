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
#include <objtools/validator/validerror_desc.hpp>
#include <objtools/validator/validerror_descr.hpp>
#include <objtools/validator/validerror_annot.hpp>
#include <objtools/validator/validerror_bioseq.hpp>
#include <objtools/validator/validerror_bioseqset.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objtools/validator/validator_context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_bioseqset::CValidError_bioseqset(CValidError_imp& imp) :
    CValidError_base(imp), m_AnnotValidator(imp), m_DescrValidator(imp), m_BioseqValidator(imp)
{
}


CValidError_bioseqset::~CValidError_bioseqset()
{
}


CConstRef<CUser_object> s_AutoDefUserObjectFromBioseq(const CBioseq& seq)
{
    if (seq.IsNa() && seq.IsSetDescr()) {
        for (const auto& desc : seq.GetDescr().Get()) {
            if (desc->IsUser()) {
                const CUser_object& uo = desc->GetUser();
                if (uo.GetObjectType() == CUser_object::eObjectType_AutodefOptions) {
                    return CConstRef<CUser_object>(&uo);
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}


static bool x_AlmostEquals(CConstRef<CUser_object> aop1, CConstRef<CUser_object> aop2)
{
    bool acceptableDiff = false;
    if (aop1 && aop2) {
        if (aop1->IsSetData() && aop2->IsSetData()) {
            CUser_object::TData::const_iterator iter1 = aop1->GetData().begin();
            CUser_object::TData::const_iterator iter2 = aop2->GetData().begin();
            for (; iter1 != aop1->GetData().end() && iter2 != aop2->GetData().end(); ++iter1, ++iter2) {
                const CUser_field& field1 = **iter1;
                const CUser_field& field2 = **iter2;
                if (! field1.IsSetLabel() || ! field1.GetLabel().IsStr()) {
                    continue;
                }
                if (! field2.IsSetLabel() || ! field2.GetLabel().IsStr()) {
                    continue;
                }
                string fld1 = field1.GetLabel().GetStr();
                string fld2 = field2.GetLabel().GetStr();
                if (fld1 == "FeatureListType" && fld2 == "FeatureListType") {
                    if (! field1.IsSetData() || ! field1.GetData().IsStr()) {
                        continue;
                    }
                    if (! field2.IsSetData() || ! field2.GetData().IsStr()) {
                        continue;
                    }
                    string featlisttype1 = field1.GetData().GetStr();
                    string featlisttype2 = field2.GetData().GetStr();
                    if (featlisttype1 == "Complete Genome" && featlisttype2 == "Partial Genome") {
                        acceptableDiff = true;
                        continue;
                    }
                    if (featlisttype1 == "Partial Genome" && featlisttype2 == "Complete Genome") {
                        acceptableDiff = true;
                        continue;
                    }
                    return false;
                } else if (fld1 != fld2) {
                    return false;
                }
            }
        }
    }
    return acceptableDiff;
}


void CValidError_bioseqset::ValidateBioseqSet(
    const CBioseq_set& seqset)
{
    int protcnt = 0;
    int nuccnt  = 0;
    int segcnt  = 0;

    // Validate Set Contents
    if (seqset.IsSetSeq_set()) {
        for (const auto& se_list_it : seqset.GetSeq_set()) {
            const CSeq_entry& se = *se_list_it;
            if (se.IsSet()) {
                const CBioseq_set& set = se.GetSet();
                // validate member set
                ValidateBioseqSet(set);
            } else if (se.IsSeq()) {
                const CBioseq& seq = se.GetSeq();
                // Validate Member Seq
                m_BioseqValidator.ValidateBioseq(seq);
            }
        }
    }
    // note - need to do this with an iterator, so that we count sequences in subsets
    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {

        if (seqit->IsAa()) {
            protcnt++;
        } else if (seqit->IsNa()) {
            nuccnt++;
        }

        if (seqit->GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            segcnt++;
        }
    }

    switch (seqset.GetClass()) {
    case CBioseq_set::eClass_not_set:
        if (m_Imp.IsHugeFileMode()) {
            call_once(m_Imp.SetContext().ClassNotSetOnceFlag,
                      [this, &seqset]() {
                          PostErr(eDiag_Warning, eErr_SEQ_PKG_BioseqSetClassNotSet,
                                  "Bioseq_set class not set", seqset);
                      });
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_BioseqSetClassNotSet,
                    "Bioseq_set class not set", seqset);
        }
        break;
    case CBioseq_set::eClass_nuc_prot:
        ValidateNucProtSet(seqset, nuccnt, protcnt, segcnt);
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
    case CBioseq_set::eClass_small_genome_set:
        ValidatePhyMutEcoWgsSet(seqset);
        break;
    case CBioseq_set::eClass_gen_prod_set:
        ValidateGenProdSet(seqset);
        break;
    case CBioseq_set::eClass_conset:
        if (! m_Imp.IsRefSeq()) {
            PostErr(eDiag_Error, eErr_SEQ_PKG_ConSetProblem,
                    "Set class should not be conset", seqset);
        }
        break;
    /*
    case CBioseq_set::eClass_other:
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GenomicProductPackagingProblem,
                "Genomic product set class incorrectly set to other", seqset);
        break;
    */
    default:
        if (nuccnt == 0 && protcnt == 0) {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_EmptySet,
                    "No Bioseqs in this set", seqset);
        }
        break;
    }


    // for pop/phy/mut/eco sets, check for consistent Autodef user objects
    bool not_all_autodef  = false;
    bool not_same_autodef = false;
    bool has_any_autodef  = false;

    if (seqset.IsSetClass()) {
        switch (seqset.GetClass()) {
        case CBioseq_set::eClass_pop_set:
        case CBioseq_set::eClass_mut_set:
        case CBioseq_set::eClass_phy_set:
        case CBioseq_set::eClass_eco_set: {
            CConstRef<CUser_object> first_autodef;
            CConstRef<CUser_object> second_autodef;
            for (auto se : seqset.GetSeq_set()) {
                bool                    has_autodef = false;
                CConstRef<CUser_object> aduo;
                if (se->IsSet()) {
                    const CBioseq_set& bss = se->GetSet();
                    for (auto sub : bss.GetSeq_set()) {
                        if (sub->IsSeq()) {
                            const CBioseq& seq = sub->GetSeq();
                            if (seq.IsNa() && seq.IsSetDescr()) {
                                aduo = s_AutoDefUserObjectFromBioseq(seq);
                                if (aduo) {
                                    has_autodef     = true;
                                    has_any_autodef = true;
                                    if (! first_autodef) {
                                        first_autodef = aduo;
                                    } else if (! first_autodef->Equals(*aduo)) {
                                        if (! second_autodef) {
                                            second_autodef = aduo;
                                            // then make sure they only differ by FeatureTypeList complete or partial genome
                                            if (! x_AlmostEquals(first_autodef, second_autodef)) {
                                                not_same_autodef = true;
                                            }
                                        } else if (! second_autodef->Equals(*aduo)) {
                                            not_same_autodef = true;
                                        }
                                    }
                                } else {
                                    has_autodef = false;
                                }
                            }
                        }
                    }
                } else if (se->IsSeq()) {
                    const CBioseq& seq = se->GetSeq();
                    if (seq.IsNa() && seq.IsSetDescr()) {
                        aduo = s_AutoDefUserObjectFromBioseq(seq);
                        if (aduo) {
                            has_autodef     = true;
                            has_any_autodef = true;
                            if (! first_autodef) {
                                first_autodef = aduo;
                            } else if (! first_autodef->Equals(*aduo)) {
                                if (! second_autodef) {
                                    second_autodef = aduo;
                                    // then make sure they only differ by FeatureTypeList complete or partial genome
                                    if (! x_AlmostEquals(first_autodef, second_autodef)) {
                                        not_same_autodef = true;
                                    }
                                } else if (! second_autodef->Equals(*aduo)) {
                                    not_same_autodef = true;
                                }
                            }
                        } else {
                            has_autodef = false;
                        }
                    }
                }
                if (! has_autodef) {
                    not_all_autodef = true;
                }
            }
            if (not_all_autodef && has_any_autodef) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_MissingAutodef,
                        "Not all pop/phy/mut/eco set components have an autodef user object",
                        seqset);
            }
            if (not_same_autodef && has_any_autodef) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_InconsistentAutodef,
                        "Inconsistent autodef user objects in pop/phy/mut/eco set",
                        seqset);
            }
        } break;
        default:
            break;
        }
    }

    bool suppressMissingSetTitle = false;

    if (has_any_autodef && (! not_all_autodef) && (! not_same_autodef)) {
        suppressMissingSetTitle = true;
    }


    ValidateSetElements(seqset, suppressMissingSetTitle);

    if (seqset.IsSetClass()
        && (seqset.GetClass() == CBioseq_set::eClass_pop_set
            || seqset.GetClass() == CBioseq_set::eClass_mut_set
            || seqset.GetClass() == CBioseq_set::eClass_phy_set
            || seqset.GetClass() == CBioseq_set::eClass_eco_set
            || seqset.GetClass() == CBioseq_set::eClass_wgs_set
            || seqset.GetClass() == CBioseq_set::eClass_small_genome_set)) {
        CheckForImproperlyNestedSets(seqset);
    }


    // validate annots
    if (seqset.IsSetAnnot()) {
        for (const auto& annot_it : seqset.GetAnnot()) {
            m_AnnotValidator.ValidateSeqAnnot(*annot_it);
            m_AnnotValidator.ValidateSeqAnnotContext(*annot_it, seqset);
        }
    }

    if ((m_Imp.IsHugeFileMode()) && m_Imp.IsHugeSet(seqset)) {
        call_once(m_Imp.SetContext().DescriptorsOnceFlag,
                  [this, &seqset]() { x_ValidateSetDescriptors(seqset /*, suppressMissingSetTitle */); });
        return;
    }


    x_ValidateSetDescriptors(seqset, suppressMissingSetTitle);
}


// =============================================================================
//                                     Private
// =============================================================================

void CValidError_bioseqset::x_ValidateSetDescriptors(const CBioseq_set& seqset, bool suppressMissingSetTitle)
{
    if (seqset.IsSetClass()
        && (seqset.GetClass() == CBioseq_set::eClass_genbank
            || seqset.GetClass() == CBioseq_set::eClass_pop_set
            || seqset.GetClass() == CBioseq_set::eClass_mut_set
            || seqset.GetClass() == CBioseq_set::eClass_phy_set
            || seqset.GetClass() == CBioseq_set::eClass_eco_set
            || seqset.GetClass() == CBioseq_set::eClass_wgs_set
            || seqset.GetClass() == CBioseq_set::eClass_small_genome_set)) {
        ShouldHaveNoDblink(seqset);
    }


    if (seqset.IsSetDescr()) {
        CBioseq_set_Handle bsh = m_Scope->GetBioseq_setHandle(seqset);
        if (bsh) {
            CSeq_entry_Handle ctx = bsh.GetParentEntry();
            if (ctx) {
                m_DescrValidator.ValidateSeqDescr(seqset.GetDescr(), *(ctx.GetCompleteSeq_entry()));
            }
        }
    }

    SetShouldNotHaveMolInfo(seqset);
    ValidateSetTitle(seqset, suppressMissingSetTitle);
}


bool CValidError_bioseqset::IsMrnaProductInGPS(const CBioseq& seq)
{
    if (m_Imp.IsGPS()) {
        CFeat_CI mrna(
            m_Scope->GetBioseqHandle(seq),
            SAnnotSelector(CSeqFeatData::e_Rna)
                .SetByProduct());
        return (bool)mrna;
    }
    return true;
}


bool CValidError_bioseqset::IsCDSProductInGPS(const CBioseq& seq, const CBioseq_set& gps)
{
    // there should be a coding region on the contig whose product is seq
    if (gps.IsSetSeq_set() && gps.GetSeq_set().size() > 0 && gps.GetSeq_set().front()->IsSeq()) {
        CBioseq_Handle contig = m_Scope->GetBioseqHandle(gps.GetSeq_set().front()->GetSeq());
        CBioseq_Handle prot   = m_Scope->GetBioseqHandle(seq);
        SAnnotSelector sel;
        sel.SetByProduct(true);
        CFeat_CI cds(prot, sel);
        while (cds) {
            CBioseq_Handle cds_seq = m_Scope->GetBioseqHandle(cds->GetLocation());
            if (cds_seq == contig) {
                return true;
            }
            ++cds;
        }
    }

    return false;
}


void CValidError_bioseqset::ValidateNucProtSet(
    const CBioseq_set& seqset,
    int nuccnt,
    int protcnt,
    int segcnt)
{
    if (nuccnt == 0) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                "No nucleotides in nuc-prot set", seqset);
    } else if (nuccnt > 1 && segcnt != 1) {
        PostErr(eDiag_Critical, eErr_SEQ_PKG_NucProtProblem,
                "Multiple unsegmented nucleotides in nuc-prot set", seqset);
    }
    if (protcnt == 0) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                "No proteins in nuc-prot set", seqset);
    }

    int  prot_biosource = 0;
    bool is_nm          = false;

    bool has_Primary    = false;
    bool has_TPA        = false;

    sequence::CDeflineGenerator defline_generator;

    if (seqset.IsSetSeq_set()) {
        for (const auto& se_list_it : seqset.GetSeq_set()) {
            const CSeq_entry& se = *se_list_it;
            if (se.IsSeq()) {
                const CBioseq& seq = se.GetSeq();

                bool hasMetaGenomeSource = false;
                auto closest_biosource   = seq.GetClosestDescriptor(CSeqdesc::e_Source);
                if (closest_biosource) {
                    const CBioSource& src = closest_biosource->GetSource();
                    if (src.IsSetOrgMod()) {
                        for (const auto& omd_itr : src.GetOrgname().GetMod()) {
                            const COrgMod& omd = *omd_itr;
                            if (omd.IsSetSubname() && omd.IsSetSubtype() && omd.GetSubtype() == COrgMod::eSubtype_metagenome_source) {
                                hasMetaGenomeSource = true;
                                break;
                            }
                        }
                    }
                }

                if (seq.IsSetDescr()) {
                    for (const auto& it : seq.GetDescr().Get()) {
                        const CSeqdesc& desc = *it;
                        if (desc.Which() == CSeqdesc::e_User && desc.GetUser().IsSetType()) {
                            const CUser_object& usr = desc.GetUser();
                            const CObject_id&   oi  = usr.GetType();
                            if (oi.IsStr() && NStr::EqualCase(oi.GetStr(), "DBLink")) {
                                PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                                        "DBLink user object should not be on a Bioseq", seq);
                            }
                        }
                    }
                }

                CBioseq_Handle     bsh = m_Scope->GetBioseqHandle(seq);
                CBioseq_set_Handle gps = GetGenProdSetParent(bsh);
                if (seq.IsNa()) {
                    if (gps && ! IsMrnaProductInGPS(seq)) {
                        PostErr(eDiag_Warning,
                                eErr_SEQ_PKG_GenomicProductPackagingProblem,
                                "Nucleotide bioseq should be product of mRNA "
                                "feature on contig, but is not",
                                seq);
                    }
                    if (seq.IsSetId()) {
                        for (const auto& id_it : seq.GetId()) {
                            const CSeq_id& id = *id_it;
                            if (id.IsOther() && id.GetOther().IsSetAccession()) {
                                const string& acc = id.GetOther().GetAccession();
                                if (NStr::StartsWith(acc, "NM_")) {
                                    is_nm = true;
                                }
                            } else if (id.IsGenbank() || id.IsEmbl() || id.IsDdbj()) {
                                has_Primary = true;
                            } else if (id.IsTpg() || id.IsTpe() || id.IsTpd()) {
                                has_TPA = true;
                            }
                        }
                    }
                } else if (seq.IsAa()) {
                    if (gps && ! IsCDSProductInGPS(seq, *(gps.GetCompleteBioseq_set()))) {
                        PostErr(eDiag_Warning,
                                eErr_SEQ_PKG_GenomicProductPackagingProblem,
                                "Protein bioseq should be product of CDS "
                                "feature on contig, but is not",
                                seq);
                    }
                    if (seq.IsSetId()) {
                        for (const auto& id_it : seq.GetId()) {
                            const CSeq_id& id = *id_it;
                            if (id.IsGenbank() || id.IsEmbl() || id.IsDdbj()) {
                                has_Primary = true;
                            } else if (id.IsTpg() || id.IsTpe() || id.IsTpd()) {
                                has_TPA = true;
                            }
                        }
                    }
                    string instantiated;
                    if (seq.IsSetDescr()) {
                        for (const auto& desc : seq.GetDescr().Get()) {
                            if (desc->IsSource()) {
                                prot_biosource++;
                            }
                            if (desc->IsTitle()) {
                                instantiated = desc->GetTitle();
                            }
                        }
                    }
                    // look for instantiated protein titles that don't match

                    if (! NStr::IsBlank(instantiated)) {
                        string generated = defline_generator.GenerateDefline(seq, *m_Scope, sequence::CDeflineGenerator::fIgnoreExisting);
                        if (! NStr::EqualNocase(instantiated, generated)) {
                            generated = defline_generator.GenerateDefline(seq, *m_Scope, sequence::CDeflineGenerator::fIgnoreExisting | sequence::CDeflineGenerator::fAllProteinNames);
                            if (NStr::StartsWith(instantiated, "PREDICTED: ", NStr::eNocase)) {
                                instantiated.erase(0, 11);
                            } else if (NStr::StartsWith(instantiated, "UNVERIFIED: ", NStr::eNocase)) {
                                instantiated.erase(0, 12);
                            } else if (NStr::StartsWith(instantiated, "PUTATIVE PSEUDOGENE: ", NStr::eNocase)) {
                                instantiated.erase(0, 21);
                            }
                            if (NStr::StartsWith(generated, "PREDICTED: ", NStr::eNocase)) {
                                generated.erase(0, 11);
                            } else if (NStr::StartsWith(generated, "UNVERIFIED: ", NStr::eNocase)) {
                                generated.erase(0, 12);
                            } else if (NStr::StartsWith(generated, "PUTATIVE PSEUDOGENE: ", NStr::eNocase)) {
                                generated.erase(0, 21);
                            } else if (NStr::StartsWith(generated, "LOW QUALITY PROTEIN: ", NStr::eNocase)) {
                                generated.erase(0, 21);
                            }
                            //okay if instantiated title has single trailing period
                            if (instantiated.length() == generated.length() + 1 && NStr::EndsWith(instantiated, ".") && ! NStr::EndsWith(instantiated, "..")) {
                                generated += ".";
                            }
                            if (! NStr::EqualNocase(instantiated, generated) && ! NStr::EqualNocase("MAG " + instantiated, generated)) {
                                if (hasMetaGenomeSource && NStr::EqualNocase("MAG: " + instantiated, generated)) {
                                    // allow missing MAG with no other prefix
                                } else if (hasMetaGenomeSource && NStr::EqualNocase("MAG " + instantiated, generated)) {
                                    // allow missing MAG followed by another prefix
                                } else {
                                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentProteinTitle,
                                            "Instantiated protein title does not match automatically "
                                            "generated title", seq);
                                }
                            }
                        }
                    }
                }
            }

            if (! se.IsSet())
                continue;

            const CBioseq_set& set = se.GetSet();
            if (set.GetClass() != CBioseq_set::eClass_segset) {

                const CEnumeratedTypeValues* tv =
                    CBioseq_set::ENUM_METHOD_NAME(EClass)();
                const string& set_class = tv->FindName(set.GetClass(), true);

                PostErr(eDiag_Critical, eErr_SEQ_PKG_NucProtNotSegSet,
                        "Nuc-prot Bioseq-set contains wrong Bioseq-set, "
                        "its class is \"" + set_class + "\".", set);
                break;
            }
        }
    }
    if (prot_biosource > 1) {
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceOnProtein,
                "Nuc-prot set has " + NStr::IntToString(prot_biosource) +
                " proteins with a BioSource descriptor", seqset);
    } else if (prot_biosource > 0) {
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceOnProtein,
                "Nuc-prot set has 1 protein with a BioSource descriptor", seqset);
    }

    bool has_source          = false;
    bool has_title           = false;
    bool has_refgenetracking = false;
    if (seqset.IsSetDescr()) {
        for (const auto& desc : seqset.GetDescr().Get()) {
            if (desc->IsSource()
                && desc->GetSource().IsSetOrg()
                && desc->GetSource().GetOrg().IsSetTaxname()
                && ! NStr::IsBlank (desc->GetSource().GetOrg().GetTaxname())) {
                has_source = true;
            } else if (desc->IsTitle()) {
                has_title = true;
            } else if (desc->IsUser() && desc->GetUser().IsRefGeneTracking()) {
                has_refgenetracking = true;
            }
            /*
            if (has_title && has_source) {
                break;
            }
            */
        }
    }

    if (! has_source) {
        // error if does not have source and is not genprodset
        CBioseq_set_Handle gps = GetGenProdSetParent(m_Scope->GetBioseq_setHandle(seqset));
        if (! gps) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceMissing,
                    "Nuc-prot set does not contain expected BioSource descriptor", seqset);
        }
    }

    if (has_title) {
        PostErr(eDiag_Warning, eErr_SEQ_PKG_NucProtSetHasTitle,
                "Nuc-prot set should not have title descriptor", seqset);
    }

    if (has_refgenetracking && (! is_nm)) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingOnNucProtSet,
                "Nuc-prot set should not have RefGeneTracking user object", seqset);
    }

    if (has_Primary && has_TPA) {
        PostErr(eDiag_Error, eErr_SEQ_INST_PrimaryAndThirdPartyMixture,
                "Nuc-prot set should not have a mixture of Primary and TPA accession types", seqset);
    }
}


void CValidError_bioseqset::CheckForInconsistentBiomols(const CBioseq_set& seqset)
{
    if (! seqset.IsSetClass()) {
        return;
    }

    if (m_Imp.IsHugeFileMode() && m_Imp.IsHugeSet(seqset.GetClass())) {
        return;
    }

    CTypeConstIterator<CMolInfo> miit(ConstBegin(seqset));
    const CMolInfo*              mol_info = nullptr;

    for (; miit; ++miit) {
        if (! miit->IsSetBiomol() || miit->GetBiomol() == CMolInfo::eBiomol_peptide) {
            continue;
        }
        if (! mol_info) {
            mol_info = &(*miit);
        } else if (mol_info->GetBiomol() != miit->GetBiomol()) {
            if (seqset.GetClass() == CBioseq_set::eClass_pop_set
             || seqset.GetClass() == CBioseq_set::eClass_eco_set
             || seqset.GetClass() == CBioseq_set::eClass_mut_set
             || seqset.GetClass() == CBioseq_set::eClass_phy_set
             || seqset.GetClass() == CBioseq_set::eClass_wgs_set
             || seqset.GetClass() == CBioseq_set::eClass_small_genome_set) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_InconsistentMoltypeSet,
                        "Pop/phy/mut/eco set contains inconsistent moltype", seqset);
            }
            break;
        }
    } // for
}


void CValidError_bioseqset::ValidateSegSet(const CBioseq_set& seqset, int segcnt)
{
    if (segcnt == 0) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_SegSetProblem,
                "No segmented Bioseq in segset", seqset);
    }

    CSeq_inst::EMol mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol seq_inst_mol;

    if (seqset.IsSetSeq_set()) {
        for (const auto& se_list_it : seqset.GetSeq_set()) {
            const CSeq_entry& se = *se_list_it;
            if (se.IsSeq()) {
                const CSeq_inst& seq_inst = se.GetSeq().GetInst();

                if (mol == CSeq_inst::eMol_not_set ||
                    mol == CSeq_inst::eMol_other) {
                    mol = seq_inst.GetMol();
                } else if ((seq_inst_mol = seq_inst.GetMol()) != CSeq_inst::eMol_other) {
                    if (seq_inst.IsNa() != CSeq_inst::IsNa(mol)) {
                        PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetMixedBioseqs,
                                "Segmented set contains mixture of nucleotides"
                                " and proteins", seqset);
                        break;
                    }
                }
            } else if (se.IsSet()) {
                const CBioseq_set& set = se.GetSet();

                if (set.IsSetClass() &&
                    set.GetClass() != CBioseq_set::eClass_parts) {
                    const CEnumeratedTypeValues* tv =
                        CBioseq_set::ENUM_METHOD_NAME(EClass)();
                    const string& set_class_str =
                        tv->FindName(set.GetClass(), true);

                    PostErr(eDiag_Critical, eErr_SEQ_PKG_SegSetNotParts,
                            "Segmented set contains wrong Bioseq-set, "
                            "its class is \"" + set_class_str + "\".",
                            set);
                    break;
                }
            }
        }
    }

    CheckForInconsistentBiomols(seqset);
}


void CValidError_bioseqset::ValidatePartsSet(const CBioseq_set& seqset)
{
    CSeq_inst::EMol mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol seq_inst_mol;

    if (seqset.IsSetSeq_set()) {
        for (const auto& se_list_it : seqset.GetSeq_set()) {
            const CSeq_entry& se = *se_list_it;
            if (se.IsSeq()) {
                const CSeq_inst& seq_inst = se.GetSeq().GetInst();
                if (mol == CSeq_inst::eMol_not_set || mol == CSeq_inst::eMol_other) {
                    mol = seq_inst.GetMol();
                } else {
                    seq_inst_mol = seq_inst.GetMol();
                    if (seq_inst_mol != CSeq_inst::eMol_other) {
                        if (seq_inst.IsNa() != CSeq_inst::IsNa(mol)) {
                            PostErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetMixedBioseqs,
                                    "Parts set contains mixture of nucleotides "
                                    "and proteins", seqset);
                        }
                    }
                }
            } else if (se.IsSet()) {
                const CBioseq_set&           set = se.GetSet();
                const CEnumeratedTypeValues* tv =
                    CBioseq_set::ENUM_METHOD_NAME(EClass)();
                const string& set_class_str =
                    tv->FindName(set.GetClass(), true);

                PostErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetHasSets,
                        "Parts set contains unwanted Bioseq-set, "
                        "its class is \"" + set_class_str + "\".",
                        set);
            }
        }
    }
}


void CValidError_bioseqset::ValidateGenbankSet(const CBioseq_set& seqset)
{
}


void CValidError_bioseqset::ValidateSetTitle(const CBioseq_set& seqset, bool suppressMissingSetTitle)
{
    bool has_title   = false;
    bool needs_title = seqset.NeedsDocsumTitle();
    if (seqset.IsSetDescr()) {
        for (const auto& desc : seqset.GetDescr().Get()) {
            if (desc->IsTitle()) {
                if (! needs_title) {
                    CSeq_entry* parent = seqset.GetParentEntry();
                    if (parent) {
                        PostErr(eDiag_Error, eErr_SEQ_DESCR_TitleNotAppropriateForSet,
                                "Only Pop/Phy/Mut/Eco sets should have titles",
                                *parent, *desc);
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_DESCR_TitleNotAppropriateForSet,
                                "Only Pop/Phy/Mut/Eco sets should have titles", seqset);
                    }
                }
                has_title = true;
            }
        }
    }


    if (needs_title && ! has_title && (m_Imp.IsRefSeq() || m_Imp.IsEmbl() || m_Imp.IsDdbj() || m_Imp.IsGenbank())) {
        if (! suppressMissingSetTitle) {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_MissingSetTitle,
                    "Pop/Phy/Mut/Eco set does not have title", seqset);
        }
    }
}


void CValidError_bioseqset::ValidateSetElements(const CBioseq_set& seqset, bool suppressMissingSetTitle)
{
    if (! seqset.IsSetClass()) {
        return;
    }
    if (seqset.GetClass() == CBioseq_set::eClass_eco_set ||
        seqset.GetClass() == CBioseq_set::eClass_phy_set ||
        seqset.GetClass() == CBioseq_set::eClass_pop_set ||
        seqset.GetClass() == CBioseq_set::eClass_mut_set) {

        if (! seqset.IsSetSeq_set() || seqset.GetSeq_set().size() == 0) {
            PostErr(eDiag_Warning, eErr_SEQ_PKG_EmptySet,
                    "Pop/Phy/Mut/Eco set has no components", seqset);
        } else if (seqset.GetSeq_set().size() == 1) {
            bool          has_alignment = false;
            CSeq_annot_CI annot_it(m_Scope->GetBioseq_setHandle(seqset));
            while (annot_it && ! has_alignment) {
                if (annot_it->IsAlign()) {
                    has_alignment = true;
                }
                ++annot_it;
            }
            if (! has_alignment) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_SingleItemSet,
                        "Pop/Phy/Mut/Eco set has only one component and no alignments", seqset);
            }
        }
    }
    if (m_Imp.IsIndexerVersion() && ! suppressMissingSetTitle) {
        if (seqset.GetClass() == CBioseq_set::eClass_eco_set ||
            seqset.GetClass() == CBioseq_set::eClass_phy_set ||
            seqset.GetClass() == CBioseq_set::eClass_pop_set ||
            seqset.GetClass() == CBioseq_set::eClass_mut_set) {
            CBioseq_CI b_i(m_Scope->GetBioseq_setHandle(seqset));
            while (b_i) {
                if (b_i->IsNa()) {
                    const CBioseq& seq       = *(b_i->GetCompleteBioseq());
                    bool           has_title = false;
                    if (seq.IsSetDescr()) {
                        for (const auto& desc : seq.GetDescr().Get()) {
                            if (desc->IsTitle()) {
                                has_title = true;
                                break;
                            }
                        }
                    }
                    if (! has_title && (m_Imp.IsRefSeq() || m_Imp.IsEmbl() || m_Imp.IsDdbj() || m_Imp.IsGenbank())) {
                        PostErr(eDiag_Warning, eErr_SEQ_PKG_ComponentMissingTitle,
                                "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title", seq);
                    }
                }
                ++b_i;
            }
        }
    }
}


void CValidError_bioseqset::SetShouldNotHaveMolInfo(const CBioseq_set& seqset)
{
    string class_name;
    switch (seqset.GetClass()) {
    case CBioseq_set::eClass_pop_set:
        class_name = "Pop set";
        break;
    case CBioseq_set::eClass_mut_set:
        class_name = "Mut set";
        break;
    case CBioseq_set::eClass_genbank:
        class_name = "Genbank set";
        break;
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_wgs_set:
    case CBioseq_set::eClass_eco_set:
        class_name = "Phy/eco/wgs set";
        break;
    case CBioseq_set::eClass_gen_prod_set:
        class_name = "GenProd set";
        break;
    case CBioseq_set::eClass_small_genome_set:
        class_name = "Small genome set";
        break;
    case CBioseq_set::eClass_nuc_prot:
        class_name = "Nuc-prot set";
        break;
    default:
        return;
        break;
    }

    if (seqset.IsSetDescr()) {
        for (const auto& desc : seqset.GetDescr().Get()) {
            if (desc->IsMolinfo()) {
                PostErr(eDiag_Warning, eErr_SEQ_PKG_MisplacedMolInfo,
                        class_name + " has MolInfo on set", seqset);
                return;
            }
        }
    }
}


void CValidError_bioseqset::ValidatePopSet(const CBioseq_set& seqset)
{
    static const string sp = " sp. ";

    if (m_Imp.IsRefSeq()) {
        PostErr(eDiag_Critical, eErr_SEQ_PKG_RefSeqPopSet,
                "RefSeq record should not be a Pop-set", seqset);
    }

    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    string first_taxname;
    bool   is_first = true;
    for (; seqit; ++seqit) {
        string         taxname;
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*seqit);
        // Will get the first biosource either from the descriptor
        // or feature.
        CSeqdesc_CI d(bsh, CSeqdesc::e_Source);
        if (d) {
            if (d->GetSource().IsSetOrg() && d->GetSource().GetOrg().IsSetTaxname()) {
                taxname = d->GetSource().GetOrg().GetTaxname();
            }
        } else {
            CFeat_CI f(bsh, CSeqFeatData::e_Biosrc);
            if (f && f->GetData().GetBiosrc().IsSetOrg() && f->GetData().GetBiosrc().GetOrg().IsSetTaxname()) {
                taxname = f->GetData().GetBiosrc().GetOrg().GetTaxname();
            }
        }

        if (is_first) {
            first_taxname = taxname;
            is_first      = false;
            continue;
        }

        // Make sure all the taxnames in the set are the same.
        if (NStr::CompareNocase(first_taxname, taxname) == 0) {
            continue;
        }

        // drops severity if first mismatch is same up to sp.
        EDiagSev sev = eDiag_Error;
        size_t   pos = NStr::Find(taxname, sp);
        if (pos != NPOS) {
            size_t len = pos + sp.length();
            if (NStr::strncasecmp(first_taxname.c_str(),
                                  taxname.c_str(),
                                  len) == 0) {
                sev = eDiag_Warning;
            }
        }
        // drops severity if one name is subset of the other
        size_t comp_len = min(taxname.length(), first_taxname.length());
        if (NStr::EqualCase(taxname, 0, comp_len, first_taxname)) {
            sev = eDiag_Warning;
        }

        PostErr(sev, eErr_SEQ_DESCR_InconsistentTaxNameSet,
                "Population set contains inconsistent organism names.", seqset);
        break;
    }
    CheckForInconsistentBiomols(seqset);
}


void CValidError_bioseqset::ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset)
{
    CheckForInconsistentBiomols(seqset);
}


void CValidError_bioseqset::ValidateGenProdSet(
    const CBioseq_set& seqset)
{
    bool              id_no_good = false;
    CSeq_id::E_Choice id_type    = CSeq_id::e_not_set;

    // genprodset should not have annotations directly on set
    if (seqset.IsSetAnnot()) {
        PostErr(eDiag_Critical,
                eErr_SEQ_PKG_GenomicProductPackagingProblem,
                "Seq-annot packaged directly on genomic product set",
                seqset);
    }

    CBioseq_set::TSeq_set::const_iterator se_list_it =
        seqset.GetSeq_set().begin();

    if (! (**se_list_it).IsSeq()) {
        return;
    }

    const CBioseq& seq = (*se_list_it)->GetSeq();
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CFeat_CI fi(bsh, CSeqFeatData::e_Rna);
    for (; fi; ++fi) {
        if (fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
            if (fi->IsSetProduct()) {
                CBioseq_Handle cdna = GetCache().GetBioseqHandleFromLocation(
                    m_Scope, fi->GetProduct(), bsh.GetTSE_Handle());
                if (! cdna) {
                    try {
                        const CSeq_id& id = GetId(fi->GetProduct(), m_Scope);
                        id_type           = id.Which();
                    } catch (const CException&) {
                        id_no_good = true;
                    } catch (const std::exception&) {
                        id_no_good = true;
                    }

                    // okay to have far RefSeq product
                    if (id_no_good || (id_type != CSeq_id::e_Other)) {
                        string loc_label;
                        fi->GetProduct().GetLabel(&loc_label);

                        if (loc_label.empty()) {
                            loc_label = "?";
                        }

                        PostErr(eDiag_Warning,
                                eErr_SEQ_PKG_GenomicProductPackagingProblem,
                                "Product of mRNA feature (" + loc_label +
                                    ") not packaged in genomic product set",
                                seq);
                    }
                } // if (cdna == 0)
            } else if (! sequence::IsPseudo(*(fi->GetSeq_feat()), *m_Scope)) {
                PostErr(eDiag_Warning,
                        eErr_SEQ_PKG_GenomicProductPackagingProblem,
                        "Product of mRNA feature (?) not packaged in "
                        "genomic product set",
                        seq);
            }
        }
    } // for
}


void CValidError_bioseqset::CheckForImproperlyNestedSets(const CBioseq_set& seqset)
{
    if (! seqset.IsSetSeq_set())
        return;
    for (const auto& se : seqset.GetSeq_set()) {
        if (se->IsSet()) {
            if (! se->GetSet().IsSetClass()
               || (se->GetSet().GetClass() != CBioseq_set::eClass_nuc_prot
                && se->GetSet().GetClass() != CBioseq_set::eClass_segset
                && se->GetSet().GetClass() != CBioseq_set::eClass_parts)) {
                PostErr(eDiag_Warning,
                        eErr_SEQ_PKG_ImproperlyNestedSets,
                        "Nested sets within Pop/Phy/Mut/Eco/Wgs set",
                        se->GetSet());
            }
            CheckForImproperlyNestedSets(se->GetSet());
        }
    }
}

void CValidError_bioseqset::ShouldHaveNoDblink(const CBioseq_set& seqset)
{
    if (! seqset.IsSetDescr())
        return;
    for (const auto& it : seqset.GetDescr().Get()) {
        const CSeqdesc& desc = *it;
        if (desc.IsUser() && desc.GetUser().GetObjectType() == CUser_object::eObjectType_DBLink) {
            PostErr(eDiag_Error,
                    eErr_SEQ_DESCR_DBLinkOnSet,
                    "DBLink user object should not be on this set",
                    seqset);
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
