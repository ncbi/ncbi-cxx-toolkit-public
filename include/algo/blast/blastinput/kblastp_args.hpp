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
 * Author: Tom Madden
 *
 */

/** @file kblastp_args.hpp
 * Main argument class for KBLASTP application
 */

#ifndef ALGO_BLAST_BLASTINPUT___KBLASTP_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___KBLASTP_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handles command line arguments for blastp binary
class NCBI_BLASTINPUT_EXPORT CKBlastpAppArgs : public CBlastAppArgs
{
public:
    /// Constructor
    CKBlastpAppArgs();

     /// Obtain Jaccard distance
     double GetJaccardDistance() { return m_KBlastpArgs->GetJaccardDistance(); }

     /// Number of matches in LSH phase
     int GetMinHits() { return m_KBlastpArgs->GetMinHits(); }

     /// Number of sequences to attempt BLASTP on.
     int GetCandidateSeqs() {return m_KBlastpArgs->GetCandidateSeqs(); }

    /// @inheritDoc
    virtual int GetQueryBatchSize() const;

protected:
    /// @inheritDoc
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);

    /// KBlastp search specific arguments
    CRef<CKBlastpArgs> m_KBlastpArgs;

};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___KBLASTP_ARGS__HPP */
