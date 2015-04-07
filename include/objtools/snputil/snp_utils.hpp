#ifndef SNP_UTIL___SNP_UTILS__HPP
#define SNP_UTIL___SNP_UTILS__HPP

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
 *   Declares Helper functions in NSnp class
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

#include <objtools/snputil/snp_bitfield.hpp>

#include <objmgr/feat_ci.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Phenotype.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/VariantProperties.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
///
/// NSnp --
///
/// Helper functions for SNP features
class NCBI_SNPUTIL_EXPORT NSnp
{
public:
	// clinical significance
	// values are taken from [human_9606].[dbo].[ClinSigCode] on 05/26/2011
	// and later coordinated with CPhenotype::EClinical_significance (05/14/2013)
	// hopefully they will stay stable
	typedef CPhenotype::EClinical_significance EClinSigID;
	typedef int TClinSigID;

    /// Determine if feature is a SNP
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - true if Subtype is variation
    ///   - false otherwise
    static bool             IsSnp(const CMappedFeat &mapped_feat);
    static bool             IsSnp(const CSeq_feat &feat);

    /// Get Create Time
    /// It will fetch the creation time based on the CAnnotDescr of the feature's
    ///   parent annotation object.
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - CTime object representing Creation time.  A default constructed CTime
    ///     object will be returned if no Create-time was found.
    static CTime            GetCreateTime(const CMappedFeat &mapped_feat);

    /// Return rsid of SNP
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing SNP feature
    /// @return
    ///   - rsid of SNP as set in its Tag data
    ///   - 0 if no rsid found
    static int              GetRsid(const CMappedFeat &mapped_feat);

    /// Return rsid of SNP
    ///
    /// @param feat
    ///   CSeq_feat object representing SNP feature
    /// @return
    ///   - rsid of SNP as set in its Tag data
    ///   - 0 if no rsid found
    static int              GetRsid(const CSeq_feat &feat);

    /// Return distance of neighbors in flanking sequence
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing feature
    /// @return
    ///   - length of neighbors on flanking sequencer
    ///   - 0 if no length information found
    static int              GetLength(const CMappedFeat &);

    /// Return distance of neighbors in flanking sequence
    ///
    /// @param mapped_feat
    ///   CSeq_feat object representing feature
    /// @return
    ///   - length of neighbors on flanking sequencer
    ///   - 0 if no length information found
    static int              GetLength(const CSeq_feat &);

    /// Return bitfield information stored as "QualityCodes"
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing snp feature
    /// @return
    ///   - bitfield created from octect sequence of QualityCodes
    ///   - CSnpBitfield is empty if no "QualityCodes" are found
    static CSnpBitfield     GetBitfield(const CMappedFeat &);

    /// Return bitfield information stored as "QualityCodes"
    ///
    /// @param mapped_feat
    ///   CSeq_feat object representing snp feature
    /// @return
    ///   - bitfield created from octect sequence of QualityCodes
    ///   - CSnpBitfield is empty if no "QualityCodes" are found
    static CSnpBitfield     GetBitfield(const CSeq_feat &feat);

    /// Check if SNP exists in GenBank database
    ///
    /// @param scope
    ///   CScope object representing scope of data
    /// @param mapped_feat
    ///   CMappedFeat object representing snp feature
    /// @param allele
    ///   string object representing allele of SNP (e.g. A or GG or -)
    /// @return
    ///   - true if SNP was found
    ///   - false otherwise
    static bool             IsSnpKnown( CScope &scope, const CMappedFeat &private_snp,  const string &allele=kEmptyStr);

    /// Check if SNP exists in GenBank database
    ///
    /// @param scope
    ///   CScope object representing scope of data
    /// @param loc
    ///   CSeq_loc representing location of SNP
    /// @param allele
    ///   string object representing allele of SNP (e.g. A or GG or -)
    /// @return
    ///   - true if SNP was found
    ///   - false otherwise
    static bool             IsSnpKnown( CScope &scope, const CSeq_loc& loc, const string &allele=kEmptyStr);

    /// list of alleles belonging to particular SNP
    /// a deletion is represented by a "-"
    typedef vector<string> TAlleles;

    /// Return list of alleles encoded in qual."replace"
    ///
    /// @param mapped_feat
    ///   CMappedFeat object representing snp feature
    /// @return
    ///   - list of alleles found in the feature (if any)
    static void     GetAlleles(const CMappedFeat &mapped_feat, TAlleles& Alleles);

    /// Return list of alleles encoded in qual."replace"
    ///
    /// @param feat
    ///   CSeq_feat object representing snp feature
    /// @return
    ///   - list of alleles found in the feature (if any)
    static void     GetAlleles(const CSeq_feat &feat, TAlleles& Alleles);

	/// controls the case of strings returned from ClinSigAsString()
	enum ELetterCase {
		eLetterCase_ForceLower,		///< always use lower case only
		eLetterCase_Mixed			///< return strings in mixes case
	};

	/// get a human-readable text for various clinical significance types
	///
	/// @param var
	///   the clinical significance will be taken from var.phenotype.clinical-significance
	///   if it is defined
	/// @param LetterCase
	///   controls the letter case of the result
	/// @return
	///   string describing the first clinical significance in the first phenotype
	///   - will be empty if clinical-significance is not present
	static string ClinSigAsString(const CVariation_ref& var, ELetterCase LetterCase = eLetterCase_Mixed);

	/// get a human-readable text for various clinical significance types
	///
	/// @param ClinSigID
	///   clinical significance ID
	/// @param LetterCase
	///   controls the letter case of the result
	/// @return
	///   string describing the given clinical significance ID
	static string ClinSigAsString(TClinSigID ClinSigID,  ELetterCase LetterCase = eLetterCase_Mixed);

///////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////
private:

};

#define SNP_VAR_EXT_CLASS "SNPData"
#define SNP_VAR_EXT_BITFIELD "Bitfield"

/// set of functions for dealing with SNP represented as variation objects
class NCBI_SNPUTIL_EXPORT NSNPVariationHelper
{
public:
    /// legacy SNP feature conversion into a variation object
	///
	/// reads a feature that supposedly contains a SNP record (old, up-to-2012, style,
    /// with SNP data encoded as "qual" (alleles) and "ext.data" (bitfield))
    /// and sets Variation to content found in the feature
	/// @param Variation
	///   conversion result will be put here, old contents are destroyed upon conversion success
	/// @param SrcFeat
	///   old format feature
    /// @return
	///   false if a given feature is not a correctly formed SNP feature
    static bool ConvertFeat(CVariation& Variation, const CSeq_feat& SrcFeat);

	/// @sa
	///   same as the other ConvertFeat(), but result put into variation-ref, placement is lost
    static bool ConvertFeat(CVariation_ref& Variation, const CSeq_feat& SrcFeat);

    /// convert SNP bitfield data to respective fields in CVariantProperties
	///
	/// @param prop
	///   The result will be put here
	/// @param bf
	///   Bitfield that will be decoded
	/// @note
	///   Not all of the bitfield bit currently have direct correspondencies within CVariantProperties
	///   so they will be lost during the conversion
    static void DecodeBitfield(CVariantProperties& prop, const CSnpBitfield& bf);

    // list of all possible substitutions in eSNPPropName_ResourceLinkURL
    static const string sResourceLink_RsID;

    /// enums to control getting a string list representation of various CVariantProperties
	/// @sa VariantPropAsStrings
    enum ESNPPropTypes {
        eSNPPropName_Undef,
        eSNPPropName_GeneLocation,  ///< prop.gene-location
        eSNPPropName_Effect,        ///< prop.effect
        eSNPPropName_Mapping,       ///< prop.mapping
        eSNPPropName_FreqValidation,///< prop.frequence-based-validation
        eSNPPropName_QualityCheck,  ///< prop.quality-check
        eSNPPropName_ResourceLink,  ///< prop.resource-link

        /// generate URL templates, with one of sResourceLink_ substrings potentially inside
		///
        /// the user should perform correct substitution of sResourceLink_ substring for the actual value
        /// an empty string will be inserted into the list when the resource URL is not known
        /// the order is guaranteed to be exactly the same as for _ResourceLink for the same prop
        eSNPPropName_ResourceLinkURL///< prop.resource-link
    };

	/// get lists of strings corresponding to a given property type
	///
	/// @param ResList
	///   will be reset to the resulting list of strings
	/// @param prop
	///   property based upon values within which the result will be generated
	/// @param ePropType
	///   type of property requested
	/// @sa ESNPPropTypes
    static void VariantPropAsStrings(list<string>& ResList, const CVariantProperties& prop, ESNPPropTypes ePropType);

    /// add alleles to a list of strings from deltas in variation data
	///
	/// empty deltas are converted to dashes "-"
	/// @param Alleles
	///   list of strings to the end of which the found deltas will be added,
	///   alleles already present in the list are retained
	/// @param pVariation
	///   variation object from which the deltas are read
    template <class TVariation> static void GetDeltas(list<string>& Alleles, const TVariation* pVariation);

private:
    template <class TPVariation> static bool x_CommonConvertFeat(TPVariation pVariation, const CSeq_feat& SrcFeat);
};


// inlines
template <class TVariation> inline void NSNPVariationHelper::GetDeltas(list<string>& Alleles, const TVariation* pVariation)
{
    if(!pVariation || !pVariation->CanGetData())
        return;
    const typename TVariation::TData& Data(pVariation->GetData());
    // if the data is a set, the deltas are located in the components
    if(Data.IsSet()) {
        const typename TVariation::TData::TSet& Set(Data.GetSet());
        if(Set.CanGetVariations()) {
            ITERATE(typename TVariation::TData::TSet::TVariations, iVariations, Set.GetVariations()) {
                GetDeltas(Alleles, iVariations->GetPointer());
            }
        }
    }
    if(Data.IsInstance()) {
        const typename TVariation::TData::TInstance&
            VarInst(Data.GetInstance());
        if(VarInst.CanGetDelta()) {
            ITERATE(typename TVariation::TData::TInstance::TDelta, iDelta, VarInst.GetDelta()) {
                if((*iDelta)->CanGetSeq()) {
                    const CDelta_item::C_Seq& DeltaSeq((*iDelta)->GetSeq());
                    switch(DeltaSeq.Which()) {
                        case CDelta_item::C_Seq::e_Literal:
                        {
                            if(DeltaSeq.GetLiteral().CanGetSeq_data()) {
                                const CSeq_data& Seq_data(DeltaSeq.GetLiteral().GetSeq_data());
                                // variations normally use Iupacna/Iupacaa
                                string sAllele;
                                if(Seq_data.IsIupacna())
                                    sAllele = Seq_data.GetIupacna().Get().empty() ? "-" : Seq_data.GetIupacna().Get();
                                if(Seq_data.IsIupacaa())
                                    sAllele = Seq_data.GetIupacaa().Get().empty() ? "-" : Seq_data.GetIupacaa().Get();

                                if(!sAllele.empty())
                                    Alleles.push_back(sAllele);
                            }
                            break;
                        }
                        case CDelta_item::C_Seq::e_This:
                        default:
                            // no specific processing for other deltas
                            break;
                    }
                }
            }
        }
    }
}


template <class TPVariation> inline bool NSNPVariationHelper::x_CommonConvertFeat(TPVariation pVariation, const CSeq_feat& SrcFeat)
{
    if(!NSnp::IsSnp(SrcFeat))
        return false;

    CConstRef<CDbtag> tag = SrcFeat.GetNamedDbxref("dbSNP");
    if (!tag)
        return false;

    CSnpBitfield bf(NSnp::GetBitfield(SrcFeat));
    CSnpBitfield::EVariationClass VarClass(bf.GetVariationClass());

    // read the alleles from the feature
    // the alleles are encoded in qual with qual=="replace"
    // empty allele strings stand for "-"
    vector<string> alleles;
    NSnp::GetAlleles(SrcFeat, alleles);

    // store the alleles depending on the SNP type sourced from the bitfield
    // have to create a temp CVariation_ref for this, since CVariation lacks useful
    // helper methods like SetSNV(), SetMNP(), etc.
    //!! need to remind Mike to implement those or suggest
    //!! a helper for CVariation_inst (since this is really the data structure those helpers
    //!! work with)
    //!! see email exchange with Mike from 07/15/2011
    CVariation_ref TmpVarRef;
    switch(VarClass) {
        case CSnpBitfield::eSingleBase:
            TmpVarRef.SetSNV(alleles, CVariation_ref::eSeqType_na);
            break;

        case CSnpBitfield::eDips:
        {
            // unfortunately, there is not enough information in the annotation to
            // reliably reconstruct what exactly this DIP represents (either ins, del or delins)
            // so we will lump them together into an indel
            string sAllelesTogether(NStr::Join(alleles, "/"));
            TmpVarRef.SetDeletionInsertion(sAllelesTogether, CVariation_ref::eSeqType_na);
            break;
        }
        case CSnpBitfield::eMultiBase:
            TmpVarRef.SetMNP(alleles, CVariation_ref::eSeqType_na);
            break;

        case CSnpBitfield::eMicrosatellite:
        default:
        {
            //!! catch-all for the types that we cannot support well yet
            //!! the using code will have to rely on extraneous info (e.g., a saved bitfield)
            //!! to recover the original type
            //!! put alleles inside a list without any sophistication, masked as a MNP
            //!! this is against the formal rules, but we currently use it a plain container
            TmpVarRef.SetMNP(alleles, CVariation_ref::eSeqType_na);
            TmpVarRef.SetData().SetInstance().SetType(CVariation_ref::TData::TInstance::eType_other);
        }
        break;
    }
    if(TmpVarRef.GetData().IsInstance())
        pVariation->SetData().SetInstance().Assign(TmpVarRef.GetData().GetInstance());

    pVariation->SetId().Assign(*tag);

    CVariantProperties& prop(pVariation->SetVariant_prop());
    DecodeBitfield(prop, bf);

	pVariation->SetDescription("SNP data");
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SNP_UTIL___SNP_UTILS__HPP

