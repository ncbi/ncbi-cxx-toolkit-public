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
 * Author: Amelia Fong
 *
 */

/** @file TBLASTN_VDB_ARGS.hpp
 * Main argument class for tblastn_vdb application
 */

#ifndef ALGO_BLAST_VDB___TBLASTN_VDB_ARGS__HPP
#define ALGO_BLAST_VDB___TBLASTN_VDB_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>
#include "blastn_vdb_args.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handles command line arguments for blastx binary
class NCBI_VDB2BLAST_EXPORT CTblastnVdbAppArgs : public CBlastAppArgs
{
public:
    /// Constructor
    CTblastnVdbAppArgs();

    /// Get the query batch size
    virtual int GetQueryBatchSize() const;

    /// Get the PSSM
    /// @return non-NULL PSSM if it's psi-tblastn
    CRef<objects::CPssmWithParameters> GetInputPssm() const;

    /// Set the PSSM from the saved search strategy
    void SetInputPssm(CRef<objects::CPssmWithParameters> pssm);

protected:
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);
    CRef<CSRASearchModeArgs> m_VDBSearchModeArgs;
    CRef<CPsiBlastArgs> m_PsiBlastArgs;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_VDB___TBLASTN_VDB_ARGS__HPP */
