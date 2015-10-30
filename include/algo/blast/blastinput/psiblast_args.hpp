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
 * Author: Christiam Camacho
 *
 */

/** @file psiblast_args.hpp
 * Main argument class for PSI-BLAST application
 */

#ifndef ALGO_BLAST_BLASTINPUT___PSIBLAST_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___PSIBLAST_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle command line arguments for psiblast binary
/// Programs supported: psiblast, phi-blastn, phi-blastp
class NCBI_BLASTINPUT_EXPORT CPsiBlastAppArgs : public CBlastAppArgs
{
public:
    /// PSI-BLAST can only run one query at a time
    static const int kMaxNumQueries = 1;

    /// Constructor
    CPsiBlastAppArgs();

    /// Get the number of iterations to perform
    size_t GetNumberOfIterations() const;

    /// Get the PSSM specified as input from the command line
    CRef<objects::CPssmWithParameters> GetInputPssm() const;

    /// Set the PSSM from the saved search strategy 
    void SetInputPssm(CRef<objects::CPssmWithParameters> pssm);

    /// Set number of iterations from the saved search strategy
    void SetNumberOfIterations(unsigned int num_iters);

    /// Get the query batch size
    virtual int GetQueryBatchSize() const;

    /// Should a PSSM be saved in a checkpoint file?
    bool SaveCheckpoint() const;
    /// Retrieve the stream to write the checkpoint file
    CNcbiOstream* GetCheckpointStream();

    /// Should a PSSM be saved as ASCII in a file?
    bool SaveAsciiPssm() const;
    /// Retrieve the stream to write the ASCII PSSM
    CNcbiOstream* GetAsciiPssmStream();

    /// Should the PSSM after the last database search be saved
    bool GetSaveLastPssm() const;

protected:
    /// Create the options handle based on the command line arguments
    /// @param locality whether the search will be executed locally or remotely
    /// [in]
    /// @param args command line arguments [in]
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);

    /// PSI-BLAST specific argument class
    CRef<CPsiBlastArgs> m_PsiBlastArgs;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___PSIBLAST_ARGS__HPP */
