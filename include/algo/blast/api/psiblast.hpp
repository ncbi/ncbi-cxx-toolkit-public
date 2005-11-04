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
 * Author:  Christiam Camacho
 *
 */

/// @file psiblast.hpp
/// Declares CPsiBlast, the C++ API for the PSI-BLAST engine.

#ifndef ALGO_BLAST_API___PSIBLAST__HPP
#define ALGO_BLAST_API___PSIBLAST__HPP

#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/psiblast_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// Forward declarations
class IQueryFactory;

/// Runs a single iteration of the PSI-BLAST algorithm on a BLAST database
class NCBI_XBLAST_EXPORT CPsiBlast : public CObject
{
public:
    /// Constructor to compare a single sequence against a database of protein
    /// sequences
    /// @param query_factory 
    ///     Protein query sequence to search (only 1 is allowed!) [in]
    /// @param dbinfo
    ///     Description of the database to search [in]
    /// @parm options
    ///     PSI-BLAST options [in]
    CPsiBlast(CRef<IQueryFactory> query_factory,
              const CSearchDatabase& dbinfo, 
              CConstRef<CPSIBlastOptionsHandle> options);

    /// Constructor to compare a PSSM against a database of protein sequences
    /// @param pssm 
    ///     PSSM to use as query. This must contain the query sequence which
    ///     represents the master sequence for the PSSM. PSSM data might be
    ///     provided as scores or as frequency ratios, in which case the PSSM 
    ///     engine will be invoked to convert them to scores (and save them as a
    ///     effect). If both the scores and frequency ratios are provided, the 
    ///     scores are given priority and are used in the search. [in|out]
    ///     @todo how should scaled PSSM scores be handled?
    /// @param dbinfo
    ///     Description of the database to search [in]
    /// @parm options
    ///     PSI-BLAST options [in]
    CPsiBlast(CRef<objects::CPssmWithParameters> pssm,
              const CSearchDatabase& dbinfo, 
              CConstRef<CPSIBlastOptionsHandle> options);

    /// Destructor
    ~CPsiBlast();

    /// This method allows the same object to be reused when performing
    /// multiple iterations. Iteration state is kept in the
    /// CPsiBlastIterationState object
    void SetPssm(CConstRef<objects::CPssmWithParameters> pssm);

    /// Accessor for the most recently used PSSM
    CConstRef<objects::CPssmWithParameters> GetPssm() const;

    /// Run the PSI-BLAST engine for one iteration
    CRef<CSearchResults> Run();

private:

    /// Interface pointer to PSI-BLAST subject abstraction
    class IPsiBlastSubject* m_Subject;

    /// Implementation class
    class CPsiBlastImpl* m_Impl;

    /// Prohibit copy constructor
    CPsiBlast(const CPsiBlast& rhs);

    /// Prohibit assignment operator
    CPsiBlast& operator=(const CPsiBlast& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBLAST__HPP */
