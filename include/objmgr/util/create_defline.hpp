#ifndef OBJMGR_UTIL___CREATE_DEFLINE__HPP
#define OBJMGR_UTIL___CREATE_DEFLINE__HPP

/*
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
* Author: Jonathan Kans
*
* ===========================================================================
*/

/// @file create_defline.hpp
/// API (CDeflineGenerator) for computing sequences' titles ("definitions").

#include <objects/misc/sequence_macros.hpp>

/** @addtogroup ObjUtilSequence
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CScope;

BEGIN_SCOPE(sequence)

/// Class for computing sequences' titles ("definitions").
///
/// PREFERRED USAGE:
///
/// CDeflineGenerator gen;
///
/// const string& title = gen.GenerateDefline(bioseq, scope, flags);
///
/// Same CDeflineGenerator should be used for all titles within nuc-prot set
/// blob, since it tracks presence or absence of biosource features to speed
/// up protein title generation

class CDeflineGenerator
{
public:
    /// Constructor
    CDeflineGenerator (void);

    /// Destructor
    ~CDeflineGenerator (void);

    /// User-settable flags for tuning behavior
    enum EUserFlags {
        fIgnoreExisting  = 0x1, ///< Generate fresh titles unconditionally.
        fAllProteinNames = 0x2  ///< List all relevant proteins, not just one.
    };
    typedef int TUserFlags; ///< Binary "OR" of EUserFlags

    /// Main method
    string GenerateDefline (
        const CBioseq& bioseq,
        CScope& scope,
        TUserFlags flags = 0
    );

private:
    /// internal methods

    void x_SetFlags (
        const CBioseq& bioseq,
        CScope& scope,
        TUserFlags flags
    );
    void x_SetBioSrc (
        const CBioseq& bioseq,
        CScope& scope
    );

    string x_DescribeClones (void);
    bool x_EndsWithStrain (void);
    void x_FlyCG_PtoR (
        string& s
    );
    string x_OrganelleName (
        bool has_plasmid,
        bool virus_or_phage,
        bool wgs_suffix
    );
    CConstRef<CSeq_feat> x_GetLongestProtein (
        const CBioseq& bioseq,
        CScope& scope
    );
    CConstRef<CGene_ref> x_GetGeneRefViaCDS (
        const CBioseq& bioseq,
        CScope& scope
    );
    bool x_HasSourceFeats (
        const CBioseq& bioseq
    );
    CConstRef<CBioSource> x_GetSourceFeatViaCDS (
        const CBioseq& bioseq,
        CScope& scope
    );

    string x_TitleFromBioSrc (void);
    string x_TitleFromNC (void);
    string x_TitleFromNM (
        const CBioseq& bioseq,
        CScope& scope
    );
    string x_TitleFromNR (
        const CBioseq& bioseq,
        CScope& scope
    );
    string x_TitleFromPatent (void);
    string x_TitleFromPDB (void);
    string x_TitleFromProtein (
        const CBioseq& bioseq,
        CScope& scope
    );
    string x_TitleFromSegSeq (
        const CBioseq& bioseq,
        CScope& scope
    );
    string x_TitleFromWGS (void);

    string x_SetPrefix (void);
    string x_SetSuffix (
        const CBioseq& bioseq,
        CScope& scope,
        const string& title
    );

private:
    /// ignore existing title is forced for certain types
    bool m_Reconstruct;
    bool m_AllProtNames;

    /// seq-inst fields
    bool m_IsNA;
    bool m_IsAA;

    bool m_IsSeg;
    bool m_IsDelta;

    /// seq-id fields
    bool m_IsNC;
    bool m_IsNM;
    bool m_IsNR;
    bool m_IsPatent;
    bool m_IsPDB;
    bool m_ThirdParty;
    bool m_WGSMaster;

    string m_GeneralStr;
    string m_PatentCountry;
    string m_PatentNumber;

    int m_PatentSequence;

    int m_PDBChain;

    /// molinfo fields
    TMOLINFO_BIOMOL m_MIBiomol;
    TMOLINFO_TECH m_MITech;
    TMOLINFO_COMPLETENESS m_MICompleteness;

    bool m_HTGTech;
    bool m_HTGSUnfinished;
    bool m_IsTSA;
    bool m_IsWGS;
    bool m_IsEST_STS_GSS;

    bool m_UseBiosrc;

    /// genbank or embl block keyword fields
    bool m_HTGSCancelled;
    bool m_HTGSDraft;
    bool m_HTGSPooled;
    bool m_TPAExp;
    bool m_TPAInf;

    /// pdb block fields
    string m_PDBCompound;

    /// biosource fields
    string m_Taxname;
    TBIOSOURCE_GENOME m_Genome;

    /// subsource fields
    string m_Chromosome;
    string m_Clone;
    string m_Map;
    string m_Plasmid;
    string m_Segment;

    /// orgmod fields
    string m_Isolate;
    string m_Strain;

    /// persistent state for blob to suppress unnecessary feature indexing
    enum ESourceFeatureStatus {
        eSFS_Unknown = -1,
        eSFS_Absent  = 0,
        eSFS_Present = 1
    };

    ESourceFeatureStatus m_HasBiosrcFeats;
};


#if 0
// PUBLIC FUNCTIONS - will probably remove

// preferred function only does feature indexing if necessary
string CreateDefLine (
    const CBioseq& bioseq,
    CScope& scope,
    CDeflineGenerator::TUserFlags flags = 0
);

// alternative provided for backward compatibility with existing function
string CreateDefLine (
    const CBioseq_Handle& hnd,
    CDeflineGenerator::TUserFlags flags = 0
);
#endif

END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif /* OBJMGR_UTIL___CREATE_DEFLINE__HPP */
