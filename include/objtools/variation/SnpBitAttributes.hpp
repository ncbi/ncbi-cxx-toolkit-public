#pragma once
/* ===========================================================================
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
 */

#include <corelib/ncbistl.hpp>

#include <bitset>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDbSnp)

/// SNP bit attribute container.
class NCBI_VARIATION_UTILS_EXPORT CSnpBitAttributes
{
public:
    /// Bits in the 64-bit bit field of the SNP VDB product.
    /// Enum values represent bit positions in the bit field.
    ///
    enum ESnpBitAttribute
    {
        /// Bit is set if this refsnp have PubMed references.
        eHasPubmedLinks = 0,

        /// Provisional Third Party Annotation(TPA) Union of
        /// GWAS / Gapplus / PAGE.
        eThirdPartyAnnotations = 1,

        /// In the nucleotide region extending from a gene stop to
        /// 500B downstream (3').
        eGene3 = 2,

        /// In the nucleotide region extending from a gene start to
        /// 2KB upstream (5').
        eGene5 = 3,

        /// In 5' UTR Location is in an untranslated region (UTR).
        eUTR5 = 4,

        /// In 3' UTR Location is in an untranslated region (UTR).
        eUTR3 = 5,

        /// In Intron.
        eIntron = 6,

        /// In donor splice - site.
        eSpliceDonor = 7,

        /// In acceptor splice site.
        eSpliceAcceptor = 8,

        /// A coding region variation where one allele in the set
        /// does not change the encoded amino acid.
        eHasSynonymousAllele = 9,

        /// A coding region variation where one allele in the set
        /// changes to STOP codon (TER).
        eHasStopGainAllele = 10,

        /// A coding region variation where one allele in the set
        /// changes protein peptide.
        eHasNonSynonymousMissenseAllele = 11,

        /// A coding region variation where one allele in the set
        /// changes all downstream amino acids.
        eHasNonSynonymousFrameshiftAllele = 12,

        /// This bit is set if the variant has 2+ minor allele
        /// count based on frequency or genotype data.
        eValidated = 13,

        /// Marker is on high density genotyping kit (50K density
        /// or greater).  The variant may have phenotype
        /// associations present in dbGaP.
        eHighDensityGenotypeKit = 14,

        /// Thousand Genomes Phase III.
        eTGPPhase3 = 15,

        /// Rs status is live.
        eStatusLive = 16,

        /// Rs status is unsupported (have no support data).
        eStatusUnsupported = 17,

        /// Rs status is merged (become a part of another rs).
        eStatusMerged = 18,

        /// Rs is clinical
        eClinical = 19,

        /// Rs is clinical and it has at least one pathogenic variant
        eSeverityHigh = 20,

        /// Rs is clinical, it's not high, but has at least one probably pathogenic variant
        eSeverityMedium = 21,

        /// Rs is clinical but neither high nor medium
        eSeverityOther = 22,

        /// Set if both a RefSNP has any frequency data from any study
        /// and there is a rare allele in any of such studies
        eRare = 23,

        /// Whether a RefSNP is considered common as determined by the
        /// frequency of its MAF for the "global" population.
        eCommon = 24,

        /// Flags for specific studies
        eALSPAC = 25,
        eExAC = 26,
        eGNOMAD = 27,
        eGO_ESP = 28,
        eTOPMED = 29,
        eTWINSUK = 30,

        /// Stop bits
        eHasStopLossAllele = 31,
        eHasStopLossOrGainAllele = 32,

        /// In-frame bits
        eInframeDeletion = 33,
        eInframeInsertion = 34,
        eInframeIndel = 35,
        eInframeAny = 36
    };

    /// Initialize the bitset with a 64-bit integer.
    ///
    CSnpBitAttributes(Uint8 bits);

    /// Initialize the bitset with an octet string.
    ///
    CSnpBitAttributes(const vector<char>& octet_string);

    /// Return a bitset representation of this
    /// attribute container.
    ///
    bitset<64> ToBitset() const
    {
        return bitset<64>{m_BitSet};
    }

    /// Return the bits as a 64-bit integer.
    ///
    Uint8 ToUint() const
    {
        return m_BitSet;
    }

private:
    /// Internal storage for bits.
    Uint8 m_BitSet;
};

inline CSnpBitAttributes::CSnpBitAttributes(Uint8 bits) : m_BitSet(bits)
{
}

inline CSnpBitAttributes::CSnpBitAttributes(const vector<char>& octet_string)
{
    auto count = sizeof(m_BitSet);
    auto byte = octet_string.end();

    do
        m_BitSet = (m_BitSet << 8) | *--byte;
    while (--count > 0);
}

END_SCOPE(NDbSnp)
END_NCBI_SCOPE
