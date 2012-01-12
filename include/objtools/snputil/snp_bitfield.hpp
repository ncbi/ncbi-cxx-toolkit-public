#ifndef SNP_OBJUTILS___SNP_BITFIELD__HPP
#define SNP_OBJUTILS___SNP_BITFIELD__HPP

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
 *
 */

#include <corelib/ncbistd.hpp>

#include <memory>

BEGIN_NCBI_SCOPE

class CSnpBitfieldFactory;

/**
*   CSnpBitfield is a facade for representing any version of the SNP
*   bitfield.  A CSnpBitfield is created from a vector<char> data type.
*
*   Example:
*      vector<char> data = <get data e.g. CUser_field::C_Data::GetOs >
*      CSnpBitfield bitfield = data
*
*   Internally, the CSnpBitfield uses a Factory (CSnpBitfieldFactory)
*   to determine the version/format of the bitfield to create and store.
*   Although it is possible to create bitfields from the Factory, it is
*   best to use this class, CSnpBitfield, instead.
*
*   CSnpBitfield is a facade to the CSnpBitfield::IEncoding interface.
*   The CSnpBitfield::IEncoding and CSnpBitfield::EProperty will evolve to
*   represent the latest SNP bitfield fields.  As newer bitfield versions
*   are introduced, all subclasses of CSnpBitfield::IEncoding are recompiled
*   to ensure the latest features of the bitfield are backwards compatible.
*   Developers that also modify CSnpBitfield and related classes should run the
*   unit_test_snp project to test and make sure nothing was broken.
*
*   For example:
*      CSnpBitfield2 (v2) introduced a byte for version number (Not found in v1.2).
*      CSnpBitfield::IEncoding was modified to get version number (e.g. GetVersion).
*      CSnpBitfield1_2 (v1.2) was forced to be recompiled.
*         Calls to 1.2's implementation of 'GetVersion' return 1
*
**/
class NCBI_SNPUTIL_EXPORT CSnpBitfield
{

///////////////////////////////////////////////////////////////////////////////
// Public Structs/Inner-classes/ Enumerations
///////////////////////////////////////////////////////////////////////////////
public:

    enum EProperty
    {
        // Note: The order of the properties is important.  Explicitly
        //  assigned values are intended.

        // DO NOT MODIFY EXISTING ASSIGNED VALUES.
        // ADD NEW PROPERTIES TO END OF ENUMERATION
        // THIS IS A LIST OF PROPERTIES THAT MAY BE FOUND ON ANY VERSION OF THE SNP BITFIELD FORMAT

        // F1 Link
        eHasLinkOut         = 0,  ///< Has SubmitterLinkOut From SNP->SubSNP->Batch.link_out
        eHasSnp3D           = 1,  ///< Has 3D structure SNP3D
        eHasSTS             = 2,  ///< Has STS Query Entrez to get the current links
        eHasEntrez          = 3,  ///< Has EntrezGene Query Entrez to get the current links
        eHasProbeDB         = 4,  ///< Has ProbeDB Query Entrez to get the current links
        eHasGEO             = 5,  ///< Has GEO Query Entrez to get the current links
        eHasAssembly        = 6,  ///< Has Assembly Query Entrez to get the current links
        eHasTrace           = 7,  ///< Has Trace Query Entrez to get the current links
        eFromMgcClone       = 8,  ///< From MGC clone We have ~20K rs. This bit could be set from specific submitter handle/ batch_id
        eHasOrganism        = 9,  ///< Has OrganismDBLink (Ex. Jackson Lab for mouse)

        // F2 Gene Function is handled separately  See EFunctionClass

        // F3 Map
        eIsAssemblySpecific = 10, // Is Assembly specific. This bit is 1 if the snp only maps to one assembly
        eHasAssemblyConflict= 11, // Has Assembly conflict. This is for weight 1 and 2 snp that maps to different chromosomes on different assemblies
        eHasOtherSameSNP    = 12, // Has other snp with exactly the same set of mapping position on NCBI refernce assembly

        // F4 Freq
        e5PctMinorAllele1Plus   = 13, // >5% minor allele frequency in 1+ populations
        e5PctMinorAlleleAll     = 14, // >5% minor allele frequency in each and all populations.
        eIsDoubleHit            = 15, // Deprecated in v4+.  This bit is set if the rs# is in Jim Mullikin's double hit submission which has been only on human snp.
        eIsMutation             = 16, // Is mutation (journal citation, explicit fact) low frequency variation that is cited in journal and other reputable sources.

        // F5 GTY
        eHasGenotype            = 17, // Genotypes available. The snp has individual genotype (in SubInd table).
        eInHaplotypeSet         = 18, // In Haplotype tagging set
        eInGenotypeKit          = 19, // Marker is on high density genotyping kit (50K density or greater). The snp may have phenotype associations present in dbGaP

        // F6 Hapmap
        ePhase1Attempted        = 20, // Phase 1 attempted all snp in HapMap unfiltered-redundant set
        ePhase1Genotyped        = 21, // Phase 1 genotyped a subset of above: filtered, non-redundant
        ePhase2Attempted        = 22, // Phase 2 attempted
        ePhase2Genotyped        = 23, // Phase 2 genotyped  filtered, non-redundant
        ePhase3Attempted        = 24, // Phase 3 attempted
        ePhase3Genotyped        = 25, // Phase 3 genotyped  filtered, non-redundant

        // F7 Phenotype
        eHasOMIM_OMIA           = 26, // Has OMIM/OMIA
        eHasMicroattribution    = 27, // Microattribution/third-party annotaton (TPA:GWAS,PAGE)
        eHasLodScore            = 28, // Has LOD score >= 2.0 in a dbGaP study genome scan
        eHasPhenoDB             = 29, // Has p-value <= 1x10-3 in a dbGaP study association test
        eHasDiseaseInfo         = 30, // Submitted as a disease-related mutation and/or present in a locus-specific database
        eHasTranscriptionFactor = 31, // Has transcription factor
        eHasClinicalAssay       = 32, // Variation is interrogated in a clinical diagnostic assay Note: Used to be eHasMPO(Mammalian Pheonotype Ontology), but never used
        eHasMeSH                = 33, // Has MeSH is linked to a disease

        // F8 Variation class is handled separately  See EVariationClass

        // F9 Quality Check
        eHasGenotypeConflict            = 34, // Has Genotype Conflict Same (rs, ind), different genotype. N/N is not included
        eIsStrainSpecific               = 35, // Is Strain Specific
        eHasMendelError                 = 36, // Has Mendelian Error
        eHasHardyWeinbergDeviation      = 37, // Has Hardy Weinberg deviation
        eHasMemberSsConflict            = 38, // Has member ss with conflict alleles
        eIsWithdrawn                    = 39, // Is Withdrawn by submitter If one member ss is withdrawn by submitter, then this bit is set. If all member ss' are withdrawn, then the rs is deleted to SNPHistory

        // Version 2 additions
        // F1 Link
        eHasShortReadArchive            = 40,  // Has Short Read Archive link

        // Version 3 additions
        // F9 Quality
        eIsContigAlleleAbsent           = 41,   // Contig allele not present in SNP allele list. The reference sequence allele at the mapped position is not present in the SNP allele list, adjusted for orientation

        // Version 2 & 3 (hidden in F2, gene function properties.  will be moved out of F2 in later bitfield versions)
        eHasReference                   = 42,   // A coding region variation where one allele in the set is identical to the reference sequence. FxnCode = 8

        // Version 4 Additions
        eIsValidated                    = 43,   // This bit is set if the snp has 2+ minor allele count based on frequency of genotype data

        // Version 5 Additions
        // F1 Link
        eHasPubmedArticle               = 44,   // Links exist to PubMed Central Article
        eHasProvisionalTPA              = 45,   // Provisional Third Party Annotations (TPA)
        eIsPrecious                     = 46,   // SNP is Precious (Clinical or Pubmed Cited)
        eIsClinical                     = 47,   // SNP is Clinical (LSDB, OMIM, TPA, Diagnostic)

        // F9 Quality
        eIsSomatic                      = 48,   // SNP is somatic, not germline.  Variation was detected in a Somatic tissue (e.g. cancer tumor).  The variation is not known to exist in heritable DNA

        // F6 Validation by HapMap/TGP properties
        eTGP2009Pilot                   = 49,   // TGP 2009 pilot phase 1 -- obsolete
        eTGP2010Pilot                   = 50,   // TGP 2010 pilot (phases 1, 2, 3) -- obsolete
        eTGPValidated                   = 51,   // TGP_validated (for subset that passed positive second platform validation)
        eTGP2010Production              = 52,   // TGP 2010 production (for data created and released prior to ASHG) -- obsolete
		eTGPPhase1						= 53,	// TGP Phase 1 (include June interim phase 1)
		eTGPPilot						= 54,	// TGP Pilot (1,2,3)
		// the following two settings are exclusive (must be one and not the other)
		eTGPOnly						= 55,   // RS Cluster has TGP Submission as of June 2011 (include all current RS from TGP): VCF - KGPROD
		eTGPNone						= 56,	// RS Cluster has none TGP Submission (set VCF OTHERKG)
		// the following is a combination of the previous two
		eTGPBoth						= 57,	// RS Cluster has both TGP and none TGP Submission
		// the following two settings are not exclusive (must be one, but don't care about the other)
		eTGPOnlyNotExclusive			= 58,   // RS Cluster has TGP Submission as of June 2011 (include all current RS from TGP): VCF - KGPROD
		eTGPNoneNotExclusive			= 59,	// RS Cluster has none TGP Submission (set VCF OTHERKG)

		/// Add additional properties here.
		ePropertyLast
    };

    // A SNP can only be one class of variation
    enum EVariationClass
    {
        eUnknownVariation       = 0,
        eSingleBase             = 1,
        eDips                   = 2,
        eHeterozygous           = 3,
        eMicrosatellite         = 4,
        eNamedSNP               = 5,
        eNoVariation            = 6,
        eMixed                  = 7,
        eMultiBase              = 8
    };

    // Function class (gene_prop in v1.2)
    // A SNP can belong to more than one gene function class
    enum EFunctionClass
    {
        eUnknownFxn             = 0,  // Uknown
        eIntron                 = 1,  // In Intron
        eDonor                  = 2,  // In donor splice-site
        eAcceptor               = 3,  // In acceptor splice site
        eUTR                    = 4,  // In Exon. location is in a spliced transcript. Is "untranslated region" (UTR) if "In CDS" is false
        eSynonymous             = 5,  // In coding region (CDS). A subset of "Exon" excluding "UTR": SYNONYMOUS if bits 5-7 are false
        eNonsense               = 6,  // Is non-synonymous Nonsense. Changes to STOP codon (TER)
        eMissense               = 7,  // Is non-synonymous Missense. Changes protein peptide
        eFrameshift             = 8,  // Is non-synonymous Frameshift. Changes all downstream amino acids

        // Version 2 additions
        eInGene                 = 9,  // In gene segment Defined as sequence intervals covered by a gene ID but not having an aligned transcript. FxnCode = 11
        eInGene5                = 10, // In 5' gene region FxnCode = 15
        eInGene3                = 11, // In 3' gene region FxnCode = 13
        eInUTR5                 = 12, // In 5' UTR Location is in an untranslated region (UTR). FxnCode = 55
        eInUTR3                 = 13, // In 3' UTR Location is in an untranslated region (UTR). FxnCode = 53
        eMultipleFxn            = 14, // Has multiple functions (i.e. fwd strand 5'near gene, rev strand 3'near gene)
                                      // use IsTrue(EFunctionClass) to determine function classes the snp belongs to.

        // version 5.4 additions
        eStopGain               = eNonsense,    // Has STOP-Gain (same as already existing "Is non-synonymous Nonsense.")
        eStopLoss               = 15  // Has STOP-Loss
    };

    class IEncoding
    {
    public:
        enum EBitFlags { fBit0=0x01, fBit1 = 0x02, fBit2=0x04, fBit3=0x08, fBit4=0x10,
                         fBit5=0x20, fBit6 = 0x40, fBit7=0x80 };

        virtual bool                            IsTrue(EProperty e)      const = 0;
        virtual bool                            IsTrue(EFunctionClass e) const = 0;
        virtual int                             GetWeight()              const = 0;
        virtual int                             GetVersion()             const = 0;
        virtual CSnpBitfield::EFunctionClass    GetFunctionClass()       const = 0;
        virtual CSnpBitfield::EVariationClass   GetVariationClass()      const = 0;
        virtual const char *                    GetString()              const = 0;
        virtual IEncoding *                     Clone()                        = 0;
        virtual                                 ~IEncoding(){};
    };

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:

    static const char *     GetString(EVariationClass e);
    static const char *     GetString(EFunctionClass e);
    static bool             IsCompatible(EFunctionClass e1, EFunctionClass e2);

    CSnpBitfield();
    CSnpBitfield(const CSnpBitfield &rhs);
    CSnpBitfield(const std::vector<char> &rhs);

    CSnpBitfield &          operator=( const CSnpBitfield &rhs );
    CSnpBitfield &          operator=( const std::vector<char> &rhs);

    bool                    IsTrue(EProperty prop) const;
    bool                    IsTrue(EFunctionClass fxn)  const;
    bool                    IsTrue(EVariationClass var) const;
    int                     GetWeight()                 const;
    int                     GetVersion()                const;
    EVariationClass         GetVariationClass()         const;
    EFunctionClass          GetFunctionClass()          const;
    const char *            GetGenePropertyString()     const;
    const char *            GetVariationClassString()   const;
    const char *            GetString()                 const;

///////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////
private:

    void x_CreateString();

///////////////////////////////////////////////////////////////////////////////
// Private Data
///////////////////////////////////////////////////////////////////////////////
private:

    std::auto_ptr<IEncoding>     m_bitfield; // inits to null object
    static CSnpBitfieldFactory   sm_Factory; // one shared factory
};

///////////////////////////////////////////////////////////////////////////////
// Inline methods
///////////////////////////////////////////////////////////////////////////////
inline bool CSnpBitfield::IsTrue(CSnpBitfield::EProperty prop) const {
    return m_bitfield->IsTrue(prop);
}

inline bool CSnpBitfield::IsTrue(CSnpBitfield::EFunctionClass fxn)  const {
    return m_bitfield->IsTrue(fxn);
}

inline bool CSnpBitfield::IsTrue(CSnpBitfield::EVariationClass var) const {
    return (m_bitfield->GetVariationClass() == var);
}

inline int  CSnpBitfield::GetWeight() const {
    return m_bitfield->GetWeight();
}

inline CSnpBitfield::EFunctionClass    CSnpBitfield::GetFunctionClass() const {
    return m_bitfield->GetFunctionClass();
}

inline CSnpBitfield::EVariationClass   CSnpBitfield::GetVariationClass() const {
    return m_bitfield->GetVariationClass();
}

inline const char * CSnpBitfield::GetString() const {
    return m_bitfield->GetString();
}

inline int  CSnpBitfield::GetVersion() const {
    return m_bitfield->GetVersion();
}

END_NCBI_SCOPE

#endif // SNP_OBJUTILS___SNP_BITFIELD__HPP

