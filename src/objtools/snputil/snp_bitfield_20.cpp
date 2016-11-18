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
 * Authors:  Dmitry Rudnev
 *
 * File Description:
 *  Provides implementation of CSnpBitfield20 class. See snp_bitfield_20.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_20.hpp"
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

// #define USE_SNPREAD

#ifdef USE_SNPREAD
#include <sra/readers/sra/snpread.hpp>
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield20::CSnpBitfield20(const CSeq_feat& feat)
{
	if(feat.IsSetExt()) {
		const CUser_object& user(feat.GetExt());
		CConstRef<CUser_field> field(user.GetFieldRef("Bitfield"));
		if(field && field->CanGetData() && field->GetData().IsOs()) {
		    const vector<char>& data(field->GetData().GetOs());

            // check size, msut be exactly 8 bytes
            if (data.size() == 8) {
                m_bits.reset(new NDbSnp::CSnpBitAttributes(data));
            }
        }
		CConstRef<CUser_field> fieldVariationClass(user.GetFieldRef("VariationClass"));
		if(fieldVariationClass && fieldVariationClass->CanGetData() && fieldVariationClass->GetData().IsStr()) {
		    CStringUTF8 sVariationClass(fieldVariationClass->GetData().GetStr());
		    if(!sVariationClass.empty()) {
		        switch(sVariationClass[0]) {
#ifdef USE_SNPREAD
		        case SSNPDb_Defs::eFeatSubtypeChar_unknown: // eFeatSubtype_unknown
		            m_VariationClass = CSnpBitfield::eUnknownVariation;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_identity: // eFeatSubtype_identity
		            m_VariationClass = CSnpBitfield::eIdentity;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_inversion: // eFeatSubtype_inversion
		            m_VariationClass = CSnpBitfield::eInversion;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_single_nucleotide_variation: // eFeatSubtype_single_nucleotide_variation
		            m_VariationClass = CSnpBitfield::eSingleBase;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_multi_nucleotide_variation: // eFeatSubtype_multi_nucleotide_variation
		            m_VariationClass = CSnpBitfield::eMultiBase;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_deletion_insertion: // eFeatSubtype_deletion_insertion
		            m_VariationClass = CSnpBitfield::eDips;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_deletion: // eFeatSubtype_deletion
		            m_VariationClass = CSnpBitfield::eDeletion;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_insertion: // eFeatSubtype_insertion
		            m_VariationClass = CSnpBitfield::eInsertion;
		            break;
		        case SSNPDb_Defs::eFeatSubtypeChar_str: // eFeatSubtype_str
		            m_VariationClass = CSnpBitfield::eMicrosatellite;
		            break;
#else
		        case 'U': // eFeatSubtype_unknown
		            m_VariationClass = CSnpBitfield::eUnknownVariation;
		            break;
		        case '-': // eFeatSubtype_identity
		            m_VariationClass = CSnpBitfield::eIdentity;
		            break;
		        case 'V': // eFeatSubtype_inversion
		            m_VariationClass = CSnpBitfield::eInversion;
		            break;
		        case 'S': // eFeatSubtype_single_nucleotide_variation
		            m_VariationClass = CSnpBitfield::eSingleBase;
		            break;
		        case 'M': // eFeatSubtype_multi_nucleotide_variation
		            m_VariationClass = CSnpBitfield::eMultiBase;
		            break;
		        case 'L': // eFeatSubtype_deletion_insertion
		            m_VariationClass = CSnpBitfield::eDips;
		            break;
		        case 'D': // eFeatSubtype_deletion
		            m_VariationClass = CSnpBitfield::eDeletion;
		            break;
		        case 'I': // eFeatSubtype_insertion
		            m_VariationClass = CSnpBitfield::eInsertion;
		            break;
		        case 'R': // eFeatSubtype_str
		            m_VariationClass = CSnpBitfield::eMicrosatellite;
		            break;
#endif
		        }
		    }
		}
	}
}


bool CSnpBitfield20::IsTrue(CSnpBitfield::EProperty prop) const
{
    bool ret(false);
    size_t Snp20BitIndex(NPOS);

    switch(prop) {
    case CSnpBitfield::eHasPubmedArticle:
        /// Bit is set if this refsnp have PubMed references.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHasPubmedLinks;
        break;

    case CSnpBitfield::eHasProvisionalTPA:
        /// Provisional Third Party Annotation(TPA) Union of
        /// GWAS / Gapplus / PAGE.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eThirdPartyAnnotations;
        break;

    case CSnpBitfield::eIsValidated:
        /// This bit is set if the variant has 2+ minor allele
        /// count based on frequency or genotype data.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eValidated;
        break;

    case CSnpBitfield::eInGenotypeKit:
        /// Marker is on high density genotyping kit (50K density
        /// or greater).  The variant may have phenotype
        /// associations present in dbGaP.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHighDensityGenotypeKit;
        break;

    case CSnpBitfield::eTGPPhase3:
        /// Thousand Genomes Phase III.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eTGPPhase3;
        break;

    case CSnpBitfield::eStatusLive:
        /// Rs status is live.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eStatusLive;
        break;

    case CSnpBitfield::eStatusUnsupported:
        /// Rs status is unsupported (have no support data).
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eStatusUnsupported;
        break;

    case CSnpBitfield::eStatusMerged:
        /// Rs status is merged (become a part of another rs).
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eStatusMerged;
        break;
    }

    if(Snp20BitIndex != NPOS && m_bits) {
        ret = m_bits->ToBitset()[Snp20BitIndex];
    }
    return ret;
}

bool CSnpBitfield20::IsTrue(CSnpBitfield::EFunctionClass prop) const
{
    bool ret(false);
    size_t Snp20BitIndex(NPOS);

    switch(prop) {
    case CSnpBitfield::eInGene3:
        /// In the nucleotide region extending from a gene stop to
        /// 500B downstream (3').
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eGene3;
        break;

    case CSnpBitfield::eInGene5:
        /// In the nucleotide region extending from a gene start to
        /// 2KB upstream (5').
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eGene5;
        break;

    case CSnpBitfield::eInUTR5:
        /// In 5' UTR Location is in an untranslated region (UTR).
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eUTR5;
        break;

    case CSnpBitfield::eInUTR3:
        /// In 3' UTR Location is in an untranslated region (UTR).
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eUTR3;
        break;

    case CSnpBitfield::eIntron:
        /// In Intron.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eIntron;
        break;

    case CSnpBitfield::eDonor:
        /// In donor splice - site.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eSpliceDonor;
        break;

    case CSnpBitfield::eAcceptor:
        /// In acceptor splice site.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eSpliceAcceptor;
        break;

    case CSnpBitfield::eSynonymous:
        /// A coding region variation where one allele in the set
        /// does not change the encoded amino acid.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHasSynonymousAllele;
        break;

    case CSnpBitfield::eStopGain:
        /// A coding region variation where one allele in the set
        /// changes to STOP codon (TER).
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHasStopGainAllele ;
        break;

    case CSnpBitfield::eMissense:
        /// A coding region variation where one allele in the set
        /// changes protein peptide.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHasNonSynonymousMissenseAllele;
        break;

    case CSnpBitfield::eFrameshift:
        /// A coding region variation where one allele in the set
        /// changes all downstream amino acids.
        Snp20BitIndex = NDbSnp::CSnpBitAttributes::eHasNonSynonymousFrameshiftAllele;
        break;
    }

    if(Snp20BitIndex != NPOS && m_bits) {
        ret = m_bits->ToBitset()[Snp20BitIndex];
    }
    return ret;
}

#define GET_FNC_BIT(snp_bit, snp20_bit) if(bits[snp20_bit]) { if(isAnyFound) return CSnpBitfield::eMultipleFxn; isAnyFound = true; fnc = snp_bit; }

CSnpBitfield::EFunctionClass CSnpBitfield20::GetFunctionClass() const
{
    CSnpBitfield::EFunctionClass fnc{CSnpBitfield::eUnknownFxn};
    if(m_bits) {
        bitset<64> bits(m_bits->ToBitset());
        bool isAnyFound{false};

        GET_FNC_BIT(CSnpBitfield::eInGene3, NDbSnp::CSnpBitAttributes::eGene3);
        GET_FNC_BIT(CSnpBitfield::eInGene5, NDbSnp::CSnpBitAttributes::eGene5);
        GET_FNC_BIT(CSnpBitfield::eInUTR5, NDbSnp::CSnpBitAttributes::eUTR5);
        GET_FNC_BIT(CSnpBitfield::eInUTR3, NDbSnp::CSnpBitAttributes::eUTR3);
        GET_FNC_BIT(CSnpBitfield::eIntron, NDbSnp::CSnpBitAttributes::eIntron);
        GET_FNC_BIT(CSnpBitfield::eDonor, NDbSnp::CSnpBitAttributes::eSpliceDonor);
        GET_FNC_BIT(CSnpBitfield::eAcceptor, NDbSnp::CSnpBitAttributes::eSpliceAcceptor);
        GET_FNC_BIT(CSnpBitfield::eSynonymous, NDbSnp::CSnpBitAttributes::eHasSynonymousAllele);
        GET_FNC_BIT(CSnpBitfield::eStopGain, NDbSnp::CSnpBitAttributes::eHasStopGainAllele);
        GET_FNC_BIT(CSnpBitfield::eMissense, NDbSnp::CSnpBitAttributes::eHasNonSynonymousMissenseAllele);
        GET_FNC_BIT(CSnpBitfield::eFrameshift, NDbSnp::CSnpBitAttributes::eHasNonSynonymousFrameshiftAllele);
    }
    return fnc;
}


CSnpBitfield::IEncoding * CSnpBitfield20::Clone()
{
    CSnpBitfield20 * obj = new CSnpBitfield20();

    if(m_bits) {
        obj->m_bits.reset(new NDbSnp::CSnpBitAttributes(m_bits->ToUint()));
    }
    obj->m_VariationClass = m_VariationClass;

    return obj;
}

END_NCBI_SCOPE
