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
 * Author: Greg Boratyn
 *
 */

/** @file deltablast_args.hpp
 * Main argument class for DELTA-BLAST application
 */

#ifndef ALGO_BLAST_BLASTINPUT___DELTABLAST_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___DELTABLAST_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle command line arguments for deltablast binary
class NCBI_BLASTINPUT_EXPORT CDeltaBlastAppArgs : public CBlastAppArgs
{
public:

    /// Constructor
    CDeltaBlastAppArgs();

    /// Get conserved domain database
    CRef<CSearchDatabase> GetDomainDatabase(void)
    {return m_DeltaBlastArgs->GetDomainDatabase();}

    /// Was printing domain hits requested
    bool GetShowDomainHits(void) const
    {return m_DeltaBlastArgs->GetShowDomainHits();}

    /// Was saving Pssm requested
    bool SaveCheckpoint(void) const
    {return m_PsiBlastArgs->RequiresCheckPointOutput();}

    /// Get stream for saving Pssm
    CNcbiOstream* GetCheckpointStream(void)
    {return m_PsiBlastArgs->GetCheckPointOutputStream();}

    /// Was saving ascii Pssm requested
    bool SaveAsciiPssm(void) const
    {return m_PsiBlastArgs->RequiresAsciiPssmOutput();}

    /// Get stream for saving ascii Pssm
    CNcbiOstream* GetAsciiPssmStream(void)
    {return m_PsiBlastArgs->GetAsciiMatrixOutputStream();}

    /// Get number of PSI-BLAST iterations
    size_t GetNumberOfPsiBlastIterations(void) const
    {return m_PsiBlastArgs->GetNumberOfIterations();}

    /// Get query batch size
    virtual int GetQueryBatchSize(void) const;

    /// Should PSSM after last database search be saved
    bool GetSaveLastPssm(void) const;

protected:
    /// Create the options handle based on the command line arguments
    /// @param locality whether the search will be executed locally or remotely
    /// [in]
    /// @param args command line arguments [in]
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);

    /// Delta-Blast specific argument class
    CRef<CDeltaBlastArgs> m_DeltaBlastArgs;

    /// Pssm search and Psi-Blast sepcific arguments
    CRef<CPsiBlastArgs> m_PsiBlastArgs;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___DELTABLAST_ARGS__HPP */
