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
 * Authors:  Melvin Quintos, Dmitry Rudnev
 *
 * File Description:
 *  Provides implementation of NSnp class. See snp_extra.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/snputil/snp_utils.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/VariantProperties.hpp>

#include <objmgr/annot_selector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
bool  NSnp::IsSnp(const CMappedFeat &mapped_feat)
{
    return IsSnp(mapped_feat.GetOriginalFeature());
}

bool  NSnp::IsSnp(const CSeq_feat &feat)
{
    bool isSnp = false;
    if (feat.IsSetData()) {
        isSnp = (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_variation);
    }
    return isSnp;
}



CTime NSnp::GetCreateTime(const CMappedFeat &feat)
{
    CTime time;
    CSeq_annot_Handle h = feat.GetAnnot();

    if (h.Seq_annot_CanGetDesc()) {
        const CAnnot_descr &desc = h.Seq_annot_GetDesc();
        if (desc.CanGet()) {
            ITERATE( CAnnot_descr::Tdata, it, desc.Get() ) {
                const CRef<CAnnotdesc> &d = *it;
                if (d->IsCreate_date()) {
                    time = d->GetCreate_date().AsCTime();
                    break;
                }
            }
        }
    }

    return time;
}

int NSnp::GetRsid(const CMappedFeat &mapped_feat)
{
    return GetRsid(mapped_feat.GetOriginalFeature());
}

int NSnp::GetRsid(const CSeq_feat &feat)
{
    int rsid = 0;

    CConstRef<CDbtag> ref = feat.GetNamedDbxref("dbSNP");
    if (ref) {
        rsid = ref->GetTag().GetId();
    }

    return rsid;
}

int NSnp::GetLength(const CMappedFeat &mapped_feat)
{
    return GetLength(mapped_feat.GetOriginalFeature());
}

int NSnp::GetLength(const CSeq_feat &feat)
{
    int length = 0;

    if (feat.IsSetExt()) {
        CConstRef<CUser_field> field =
            feat.GetExt().GetFieldRef("Extra");
        if (field) {
            string s1, s2;
            const string &str = field->GetData().GetStr();
            if (NStr::SplitInTwo(str, "=", s1, s2)) {
                vector<string> v;

                NStr::Tokenize(str, ",", v);
                if (v.size()==4) {
                    int rc = NStr::StringToInt(v[3], NStr::fConvErr_NoThrow);
                    int lc = NStr::StringToInt(v[2], NStr::fConvErr_NoThrow);
                    length = rc + lc + 1;
                }
            }
        }
    }

    return length;
}

string NSnp::ClinSigAsString(const CVariation_ref& var, ELetterCase LetterCase)
{
    ITERATE (CVariation_ref::TPhenotype, pnt_iter, var.GetPhenotype()) {
        if ((*pnt_iter)->CanGetClinical_significance()) {
            return ClinSigAsString((*pnt_iter)->GetClinical_significance(), LetterCase);
        }
    }
	return "";
}

string NSnp::ClinSigAsString(TClinSigID ClinSigID, ELetterCase LetterCase)
{
	string sResult;
    switch(ClinSigID)
    {
		case CPhenotype::eClinical_significance_non_pathogenic:
			sResult = "Benign";
			break;
		case CPhenotype::eClinical_significance_probable_non_pathogenic:
			sResult = "Likely benign";
			break;
		case CPhenotype::eClinical_significance_probable_pathogenic:
			sResult = "Likely pathogenic";
			break;
		case CPhenotype::eClinical_significance_pathogenic:
			sResult = "Pathogenic";
			break;
		case CPhenotype::eClinical_significance_drug_response:
			sResult = "Drug response";
			break;
		case CPhenotype::eClinical_significance_histocompatibility:
			sResult = "Histocompatibility";
			break;
		case CPhenotype::eClinical_significance_unknown:
			sResult = "Uncertain significance";
			break;
		case CPhenotype::eClinical_significance_untested:
			sResult = "Not tested";
			break;
		case CPhenotype::eClinical_significance_other:
		default:
			sResult = "Other";
			break;
    }
    return LetterCase == eLetterCase_ForceLower ? NStr::ToLower(sResult) : sResult;
}


CSnpBitfield NSnp::GetBitfield(const CMappedFeat &mapped_feat)
{
    return GetBitfield(mapped_feat.GetOriginalFeature());
}

CSnpBitfield NSnp::GetBitfield(const CSeq_feat &feat)
{
    CSnpBitfield b;

    CConstRef<CDbtag> ref = feat.GetNamedDbxref("dbSNP");

    if (ref && feat.IsSetExt() ) {
        CConstRef<CUser_field> field =
            feat.GetExt().GetFieldRef("QualityCodes");
        if (field) {
            b = field->GetData().GetOs();
        }
    }

    return b;
}

bool NSnp::IsSnpKnown( CScope &scope, const CMappedFeat &private_snp, const string &allele)
{
    const CSeq_loc &loc = private_snp.GetLocation();
    return IsSnpKnown(scope, loc, allele);
}

bool NSnp::IsSnpKnown( CScope& scope, const CSeq_loc& loc, const string & allele)
{
    bool isKnown        = false;
    SAnnotSelector      sel;       // annotation selector

    // Prepare Annotation Selection to find the SNPs
    //sel = CSeqUtils::GetAnnotSelector(CSeqFeatData::eSubtype_variation);
    sel .SetOverlapTotalRange()
        .SetResolveAll()
        .AddNamedAnnots("SNP")
        .SetExcludeExternal(false)
        .ExcludeUnnamedAnnots()
        .SetAnnotType(CSeq_annot::TData::e_Ftable)
        .SetFeatSubtype(CSeqFeatData::eSubtype_variation)
        .SetMaxSize(100000);  // In case someone does something silly.

    CFeat_CI feat_it(scope, loc, sel);

    if (allele == kEmptyStr) {
        // Don't check for alleles
        // Existing of any returned SNP means there are known SNPs
        if (feat_it.GetSize()>0) {
            isKnown = true;
        }
    }
    else {
        // Check all the alleles for all the returned SNPs
        for (; feat_it && !isKnown; ++feat_it) {
            const CSeq_feat &       or_feat = feat_it->GetOriginalFeature();
            if (or_feat.CanGetQual()) {
                ITERATE (CSeq_feat::TQual, it, or_feat.GetQual()) {
                    const CRef<CGb_qual> &qual = *it;
                    if (qual->GetQual() == "replace" &&
                        qual->GetVal().find(allele) != string::npos) {
                        isKnown = true;
                        break;
                    }
                }
            }
        }
    }

    return isKnown;
}


const string NSNPVariationHelper::sResourceLink_RsID("%rsid%");

bool NSNPVariationHelper::ConvertFeat(CVariation& Variation, const CSeq_feat& SrcFeat)
{
    if(!x_CommonConvertFeat(&Variation, SrcFeat))
        return false;
	CRef<CVariantPlacement> pPlacement(new CVariantPlacement);
    pPlacement->SetLoc().Assign(SrcFeat.GetLocation());
    Variation.SetPlacements().push_back(pPlacement);

    // save a copy of the bitfield since not every bit
    // currently is adequately represented in Variation
    CSnpBitfield bf(NSnp::GetBitfield(SrcFeat));
    if(bf.GetVersion() > 0) {
        CRef<CUser_object> pExt(new CUser_object());
        CUser_field::C_Data::TOs Os;
        bf.GetBytes(Os);
        pExt->SetField(SNP_VAR_EXT_BITFIELD).SetData().SetOs() = Os;
        pExt->SetClass(SNP_VAR_EXT_CLASS);
        Variation.SetExt().push_back(pExt);
    }
    return true;
}

bool NSNPVariationHelper::ConvertFeat(CVariation_ref& Variation, const CSeq_feat& SrcFeat)
{
    if(!x_CommonConvertFeat(&Variation, SrcFeat))
        return false;

    // save a copy of the bitfield since not every bit
    // currently is adequately represented in Variation
    CSnpBitfield bf(NSnp::GetBitfield(SrcFeat));
    if(bf.GetVersion() > 0) {
        CUser_field::C_Data::TOs Os;
        bf.GetBytes(Os);
        Variation.SetExt().SetField(SNP_VAR_EXT_BITFIELD).SetData().SetOs() = Os;
        Variation.SetExt().SetClass(SNP_VAR_EXT_CLASS);
    }
    return true;
}




void NSNPVariationHelper::DecodeBitfield(CVariantProperties& prop, const CSnpBitfield& bf)
{
    prop.SetVersion(bf.GetVersion());

    /// resource link
    int res_link = 0;
    if (bf.IsTrue(CSnpBitfield::eIsPrecious)) {
        res_link |= CVariantProperties::eResource_link_preserved;
    }
    if (bf.IsTrue(CSnpBitfield::eHasProvisionalTPA)) {
        res_link |= CVariantProperties::eResource_link_provisional;
    }
    if (bf.IsTrue(CSnpBitfield::eHasSnp3D)) {
        res_link |= CVariantProperties::eResource_link_has3D;
    }
    if (bf.IsTrue(CSnpBitfield::eHasLinkOut)) {
        res_link |= CVariantProperties::eResource_link_submitterLinkout;
    }
    if (bf.IsTrue(CSnpBitfield::eIsClinical)) {
        res_link |= CVariantProperties::eResource_link_clinical;
    }
    if (bf.IsTrue(CSnpBitfield::eInGenotypeKit)) {
        res_link |= CVariantProperties::eResource_link_genotypeKit;
    }
    if (res_link) {
        prop.SetResource_link(res_link);
    }

    /// gene function
    int gene_location = 0;
    if (bf.IsTrue(CSnpBitfield::eInGene)) {
        gene_location |= CVariantProperties::eGene_location_in_gene;
    }
    if (bf.IsTrue(CSnpBitfield::eInGene5)) {
        gene_location |= CVariantProperties::eGene_location_near_gene_5;
    }
    if (bf.IsTrue(CSnpBitfield::eInGene3)) {
        gene_location |= CVariantProperties::eGene_location_near_gene_3;
    }
    if (bf.IsTrue(CSnpBitfield::eIntron)) {
        gene_location |= CVariantProperties::eGene_location_intron;
    }
    if (bf.IsTrue(CSnpBitfield::eDonor)) {
        gene_location |= CVariantProperties::eGene_location_donor;
    }
    if (bf.IsTrue(CSnpBitfield::eAcceptor)) {
        gene_location |= CVariantProperties::eGene_location_acceptor;
    }
    if (bf.IsTrue(CSnpBitfield::eInUTR5)) {
        gene_location |= CVariantProperties::eGene_location_utr_5;
    }
    if (bf.IsTrue(CSnpBitfield::eInUTR3)) {
        gene_location |= CVariantProperties::eGene_location_utr_3;
    }
    if (gene_location) {
        prop.SetGene_location(gene_location);
    }

    // effect
    int effect(0);
    if (bf.IsTrue(CSnpBitfield::eSynonymous)) {
        effect |= CVariantProperties::eEffect_synonymous;
    }
    if (bf.IsTrue(CSnpBitfield::eStopGain)) {
        effect |= CVariantProperties::eEffect_stop_gain;
    }
    if (bf.IsTrue(CSnpBitfield::eStopLoss)) {
        effect |= CVariantProperties::eEffect_stop_loss;
    }
    if (bf.IsTrue(CSnpBitfield::eMissense)) {
        effect |= CVariantProperties::eEffect_missense;
    }
    if (bf.IsTrue(CSnpBitfield::eFrameshift)) {
        effect |= CVariantProperties::eEffect_frameshift;
    }
    if (effect) {
        prop.SetEffect(effect);
    }

    /// mapping
    int mapping = 0;
    if (bf.IsTrue(CSnpBitfield::eHasOtherSameSNP)) {
        mapping |= CVariantProperties::eMapping_has_other_snp;
    }
    if (bf.IsTrue(CSnpBitfield::eHasAssemblyConflict)) {
        mapping |= CVariantProperties::eMapping_has_assembly_conflict;
    }
    if (bf.IsTrue(CSnpBitfield::eIsAssemblySpecific)) {
        mapping |= CVariantProperties::eMapping_is_assembly_specific;
    }
    if (mapping) {
        prop.SetMapping(mapping);
    }


    /// DEPRECATED
    /// There is not 1:1 correspondance between Bitfield weight
    /// and VariantProperties map-weight.  See SNP-5729.
    /// So, I am commenting out.  JB Holmes, April 2013
    /// weight
    // int weight = bf.GetWeight();
    // if (weight) {
    //    prop.SetMap_weight(weight);
    // }

    /// allele frequency
    int allele_freq = 0;
    if (bf.IsTrue(CSnpBitfield::eIsMutation)) {
        allele_freq |= CVariantProperties::eFrequency_based_validation_is_mutation;
    }
    if (bf.IsTrue(CSnpBitfield::e5PctMinorAlleleAll)) {
        allele_freq |= CVariantProperties::eFrequency_based_validation_above_5pct_all;
    }
    if (bf.IsTrue(CSnpBitfield::e5PctMinorAllele1Plus)) {
        allele_freq |= CVariantProperties::eFrequency_based_validation_above_5pct_1plus;
    }
    if (bf.IsTrue(CSnpBitfield::eIsValidated)) {
        allele_freq |= CVariantProperties::eFrequency_based_validation_validated;
    }
    if (allele_freq) {
        prop.SetFrequency_based_validation(allele_freq);
    }

    /// genotype
    int genotype = 0;
    if (bf.IsTrue(CSnpBitfield::eInHaplotypeSet)) {
        genotype |= CVariantProperties::eGenotype_in_haplotype_set;
    }
    if (bf.IsTrue(CSnpBitfield::eHasGenotype)) {
        genotype |= CVariantProperties::eGenotype_has_genotypes;
    }
    if (genotype) {
        prop.SetGenotype(genotype);
    }

    /// quality checking
    int qual_check = 0;
    if (bf.IsTrue(CSnpBitfield::eIsContigAlleleAbsent)) {
        qual_check |= CVariantProperties::eQuality_check_contig_allele_missing;
    }
    if (bf.IsTrue(CSnpBitfield::eHasMemberSsConflict)) {
        qual_check |= CVariantProperties::eQuality_check_non_overlapping_alleles;
    }
    if (bf.IsTrue(CSnpBitfield::eIsWithdrawn)) {
        qual_check |= CVariantProperties::eQuality_check_withdrawn_by_submitter;
    }
    if (bf.IsTrue(CSnpBitfield::eIsStrainSpecific)) {
        qual_check |= CVariantProperties::eQuality_check_strain_specific;
    }
    if (bf.IsTrue(CSnpBitfield::eHasGenotypeConflict)) {
        qual_check |= CVariantProperties::eQuality_check_genotype_conflict;
    }
    if (qual_check) {
        prop.SetQuality_check(qual_check);
    }
}

void NSNPVariationHelper::VariantPropAsStrings(list<string>& ResList, const CVariantProperties& prop, ESNPPropTypes ePropType)
{
    ResList.clear();
    switch(ePropType) {
    case eSNPPropName_GeneLocation:
        if(prop.CanGetGene_location()) {
            CVariantProperties::TGene_location gene_loc(prop.GetGene_location());
            if(gene_loc & CVariantProperties::eGene_location_in_gene)
                ResList.push_back("In Gene");
            if(gene_loc & CVariantProperties::eGene_location_near_gene_5)
                ResList.push_back("In 5\' Gene");
            if(gene_loc & CVariantProperties::eGene_location_near_gene_3)
                ResList.push_back("In 3\' Gene");
            if(gene_loc & CVariantProperties::eGene_location_intron)
                ResList.push_back("Intron");
            if(gene_loc & CVariantProperties::eGene_location_donor)
                ResList.push_back("Donor");
            if(gene_loc & CVariantProperties::eGene_location_acceptor)
                ResList.push_back("Acceptor");
            if(gene_loc & CVariantProperties::eGene_location_utr_5)
                ResList.push_back("In 5\' UTR");
            if(gene_loc & CVariantProperties::eGene_location_utr_3)
                ResList.push_back("In 3\' UTR");
            if(gene_loc & CVariantProperties::eGene_location_in_start_codon)
                ResList.push_back("In Start Codon");
            if(gene_loc & CVariantProperties::eGene_location_in_stop_codon)
                ResList.push_back("In Stop Codon");
            if(gene_loc & CVariantProperties::eGene_location_intergenic)
                ResList.push_back("Intergenic");
            if(gene_loc & CVariantProperties::eGene_location_conserved_noncoding)
                ResList.push_back("In Conserved Non-coding region");
        }
        break;
    case eSNPPropName_Effect:
        if(prop.CanGetEffect()) {
            CVariantProperties::TEffect effect(prop.GetEffect());
            if(effect == CVariantProperties::eEffect_no_change)
                ResList.push_back("No change");
            else {
                if(effect & CVariantProperties::eEffect_synonymous)
                    ResList.push_back("Synonymous");
                if(effect & CVariantProperties::eEffect_nonsense)
                    ResList.push_back("Nonsense");
                if(effect & CVariantProperties::eEffect_missense)
                    ResList.push_back("Missense");
                if(effect & CVariantProperties::eEffect_frameshift)
                    ResList.push_back("Frameshift");
                if(effect & CVariantProperties::eEffect_up_regulator)
                    ResList.push_back("Up-regulator");
                if(effect & CVariantProperties::eEffect_down_regulator)
                    ResList.push_back("Down-regulator");
                if(effect & CVariantProperties::eEffect_methylation)
                    ResList.push_back("Methylation");
                if(effect & CVariantProperties::eEffect_stop_gain)
                    ResList.push_back("Stop-gain");
                if(effect & CVariantProperties::eEffect_stop_loss)
                    ResList.push_back("Stop-loss");
            }
        }
        break;
    case eSNPPropName_Mapping:
        if(prop.CanGetMapping()) {
            CVariantProperties::TMapping mapping(prop.GetMapping());
            if(mapping & CVariantProperties::eMapping_has_other_snp)
                ResList.push_back("Has other SNP");
            if(mapping & CVariantProperties::eMapping_has_assembly_conflict)
                ResList.push_back("Has Assembly conflict");
            if(mapping & CVariantProperties::eMapping_is_assembly_specific)
                ResList.push_back("Is assembly specific");
        }
        break;
    case eSNPPropName_FreqValidation:
        if(prop.CanGetFrequency_based_validation()) {
            CVariantProperties::TFrequency_based_validation freq_validation(prop.GetFrequency_based_validation());
            if(freq_validation & CVariantProperties::eFrequency_based_validation_above_1pct_1plus)
                ResList.push_back(">1% minor allele freq in 1+ populations");
            if(freq_validation & CVariantProperties::eFrequency_based_validation_above_1pct_all)
                ResList.push_back(">1% minor allele freq in each and all populations");
            if(freq_validation & CVariantProperties::eFrequency_based_validation_above_5pct_1plus)
                ResList.push_back(">5% minor allele freq in 1+ populations");
            if(freq_validation & CVariantProperties::eFrequency_based_validation_above_5pct_all)
                ResList.push_back(">5% minor allele freq in each and all populations");
            if(freq_validation & CVariantProperties::eFrequency_based_validation_is_mutation)
                ResList.push_back("Is mutation");
            if(freq_validation & CVariantProperties::eFrequency_based_validation_validated)
                ResList.push_back("Validated (has a minor allele in two or more separate chromosomes)");
        }
        break;
    case eSNPPropName_QualityCheck:
        if(prop.CanGetQuality_check()) {
            CVariantProperties::TQuality_check quality_check(prop.GetQuality_check());
            if(quality_check & CVariantProperties::eQuality_check_contig_allele_missing)
                ResList.push_back("Reference allele missing from SNP alleles");
            if(quality_check & CVariantProperties::eQuality_check_genotype_conflict)
                ResList.push_back("Genotype conflict");
            if(quality_check & CVariantProperties::eQuality_check_non_overlapping_alleles)
                ResList.push_back("Non-overlapping allele sets");
            if(quality_check & CVariantProperties::eQuality_check_strain_specific)
                ResList.push_back("Strain specific fixed difference");
            if(quality_check & CVariantProperties::eQuality_check_withdrawn_by_submitter)
                ResList.push_back("Member SS withdrawn by submitter");
        }
        break;
    case eSNPPropName_ResourceLink:
        if(prop.CanGetResource_link()) {
            CVariantProperties::TResource_link resource_link(prop.GetResource_link());
            if(resource_link & CVariantProperties::eResource_link_clinical)
                ResList.push_back("Clinical");
            if(resource_link & CVariantProperties::eResource_link_provisional)
                ResList.push_back("Provisional");
            if(resource_link & CVariantProperties::eResource_link_preserved)
                ResList.push_back("Preserved");
            if(resource_link & CVariantProperties::eResource_link_genotypeKit)
                ResList.push_back("On high density genotyping kit");
            if(resource_link & CVariantProperties::eResource_link_has3D)
                ResList.push_back("SNP3D");
            if(resource_link & CVariantProperties::eResource_link_submitterLinkout)
                ResList.push_back("SubmitterLinkOut");
        }
        break;
    case eSNPPropName_ResourceLinkURL:
        // NB: take care to have the same order as in eSNPPropName_ResourceLink
        if(prop.CanGetResource_link()) {
            CVariantProperties::TResource_link resource_link(prop.GetResource_link());
            if(resource_link & CVariantProperties::eResource_link_clinical)
                ResList.push_back("");
            if(resource_link & CVariantProperties::eResource_link_provisional)
                ResList.push_back("");
            if(resource_link & CVariantProperties::eResource_link_preserved)
                ResList.push_back("");
            if(resource_link & CVariantProperties::eResource_link_genotypeKit)
                ResList.push_back("");
            if(resource_link & CVariantProperties::eResource_link_has3D)
                ResList.push_back("http://www.ncbi.nlm.nih.gov/SNP/snp3D.cgi?rsnum=" + sResourceLink_RsID);
            if(resource_link & CVariantProperties::eResource_link_submitterLinkout)
                ResList.push_back("");
        }
        break;
    default:
        break;
    }
}



END_NCBI_SCOPE
