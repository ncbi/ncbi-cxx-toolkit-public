/* $Id$
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
 * Author:  Christiam Camacho
 *
 */

/// @file rps_aux.hpp
/// Declares auxiliary classes to manage RPS-BLAST related C-structures

#ifndef ALGO_BLAST_API__RPS_AUX___HPP
#define ALGO_BLAST_API__RPS_AUX___HPP

#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_aalookup.h> // for BlastRPSLookupTable
#include <algo/blast/core/blast_rps.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class CBlastRPSAuxInfo;
class CRpsAuxFile;
class CRpsLookupTblFile;
class CRpsPssmFile;
class CRpsFreqsFile;
class CRpsObsrFile;
class CRpsFreqRatiosFile;

// The BLAST Engine currently needs the BlastRPSInfo structure for both the
// preliminary stage and the traceback search. In practice, the setup code
// needs the aux_info field to copy the orig_score_matrix field, gap costs and
// scaling factor. The preliminary stage needs its lookup_header and 
// profile_header fields, and the traceback search needs the profile_header
// field and the aux_info's karlin_k field. This suggests that a better
// organization might be needed.

/// Wrapper class to manage the BlastRPSInfo structure, as currently
/// there aren't any allocation or deallocation functions for this structure in
/// the CORE of BLAST. This class is meant to be kept in a CRef<>.
class NCBI_XBLAST_EXPORT CBlastRPSInfo : public CObject {
public:

    /// Flags for opening database files
    enum EOpenFlags {
        // Open lookup table file
        fLookupTableFile = 1,
        /// Open pssm file
        fPssmFile = 2,
        /// Open auxiliary information file
        fAuxInfoFile = 4,
        /// Open residue frequencies file
        fFrequenciesFile = 8,
        /// Open file with numbers of independent observations
        fObservationsFile = 16,
        /// Open file with frequency ratios
        fFreqRatiosFile = 32,

        // The freq ratios file is large and needed only in the traceback.
        // It may be needed to add closing and loading separate files.

        /// Flags set for RPS-BLAST
        fRpsBlast = fLookupTableFile | fPssmFile | fAuxInfoFile,

        /// Flags set for DELTA-BLAST
        fDeltaBlast = fFrequenciesFile | fObservationsFile,

        /// Flags set for RPS-BLAST running in CBS mode
        fRpsBlastWithCBS = fRpsBlast | fFreqRatiosFile
    };

public:
    /// Parametrized constructor
    /// @param rps_dbname name of the RPS-BLAST database
    CBlastRPSInfo(const string& rps_dbname);

    /// Parametrized constructor
    /// @param rps_dbname name of the RPS-BLAST database
    /// @param flags Flags for which database files to open
    CBlastRPSInfo(const string& rps_dbname, int flags);

    /// Destructor
    ~CBlastRPSInfo();

    /// Accessor for the underlying C structure (managed by this class)
    const BlastRPSInfo* operator()() const;

    /// Returns the scaling factor used to build RPS-BLAST database 
    double GetScalingFactor() const;

    /// Returns the name of the scoring matrix used to build the RPS-BLAST
    /// database
    const char* GetMatrixName() const;

    // FIXME: the following two methods are an interface that return the
    // permissible gap costs associated with the matrix used when building the
    // RPS-BLAST database... these could be removed if some other interface
    // provided those given the matrix name.

    /// Returns the gap opening cost associated with the scoring matrix above
    int GetGapOpeningCost() const;

    /// Returns the gap extension cost associated with the scoring matrix above
    int GetGapExtensionCost() const;


protected:

    /// Initialize attributes
    void x_Init(const string& rps_dbname, int flags);

private:
    /// Prohibit copy-constructor
    CBlastRPSInfo(const CBlastRPSInfo& rhs);
    /// Prohibit assignment operator
    CBlastRPSInfo& operator=(const CBlastRPSInfo& rhs);

    /// The auxiliary RPS-BLAST file (.aux)
    CRef<CRpsAuxFile> m_AuxFile;
    /// The PSSM RPS-BLAST file (.rps)
    CRef<CRpsPssmFile> m_PssmFile;
    /// The lookup table RPS-BLAST file (.loo)
    CRef<CRpsLookupTblFile> m_LutFile;

    /// Weighted residue frequencies file (.wcounts)
    /// used by delta-blast
    CRef<CRpsFreqsFile> m_FreqsFile;

    /// Number of independent observations file (.obsr)
    /// used by delta-blast
    CRef<CRpsObsrFile> m_ObsrFile;

    /// Frequency ratios file (.freq)
    /// used for composition based statistics and cobalt
    CRef<CRpsFreqRatiosFile> m_FreqRatiosFile;

    
    /// Pointer which contains pointers to data managed by the data members
    /// above
    unique_ptr<BlastRPSInfo> m_RpsInfo;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__RPS_AUX___HPP */
