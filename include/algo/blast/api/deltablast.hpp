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
 * Author:  Greg Boratyn
 *
 */

/// @file deltablast.hpp
/// Declares CPsiBlast, the C++ API for the PSI-BLAST engine.

#ifndef ALGO_BLAST_API___DELTABLAST__HPP
#define ALGO_BLAST_API___DELTABLAST__HPP

#include <objects/scoremat/PssmWithParameters.hpp>

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/deltablast_options.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_results.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class IQueryFactory;

/// A simple realization of the DELTA-BLAST algorithm: seacrch domain database,
/// compute PSSM, search sequence database
class NCBI_XBLAST_EXPORT CDeltaBlast : public CObject, public CThreadable
{
public:

public:
    /// Constructor to compare a single sequence against a database of protein
    /// sequences, and use RPS-BLAST to search CDD
    /// @param query_factory 
    ///     Protein query sequence to search [in]
    /// @param blastdb
    ///     Adapter to the BLAST database to search [in]
    /// @param domaindb
    ///     Adapter to the BLAST conserved domain database for making Pssm [in]
    /// @param options
    ///     DELTA-BLAST options [in]
    CDeltaBlast(CRef<IQueryFactory> query_factory,
                CRef<CLocalDbAdapter> blastdb,
                CRef<CLocalDbAdapter> domaindb,
                CConstRef<CDeltaBlastOptionsHandle> options);

    /// Destructor
    ~CDeltaBlast() {}

    /// Accessor for PSSM computd from CDD hits and used in protein search
    /// @param index PSSM index corresponding to query
    /// @return PSSM
    CConstRef<objects::CPssmWithParameters> GetPssm(int index = 0) const;

    /// Accessor for PSSM computd from CDD hits and used in protein search
    /// @param index PSSM index corresponding to query
    /// @return PSSM
    CRef<objects::CPssmWithParameters> GetPssm(int index = 0);

    /// Get results of conserved domain search (intermediate results)
    /// @return Conserved domain search results
    CRef<CSearchResultSet> GetDomainResults() {return m_DomainResults;}

    /// Run the DELTA-BLAST engine with one iteration
    CRef<CSearchResultSet> Run();

protected:
    /// Prohibit copy constructor
    CDeltaBlast(const CDeltaBlast& rhs);
    /// Prohibit assignment operator
    CDeltaBlast& operator=(const CDeltaBlast& rhs);

    /// Search domain database
    /// @return Domain database search results
    CRef<CSearchResultSet> x_FindDomainHits(void);

    /// Perform sanity checks on input parameters
    void x_Validate(void);

private:
    /// Queries
    CRef<IQueryFactory> m_Queries;

    /// Reference to a BLAST subject/database object
    CRef<CLocalDbAdapter> m_Subject;

    /// Reference to a BLAST conserved domain database object
    CRef<CLocalDbAdapter> m_DomainDb;

    /// Delta Blast options
    CConstRef<CDeltaBlastOptionsHandle> m_Options;

    /// PSSMs computed for each query
    vector< CRef<CPssmWithParameters> > m_Pssm;

    /// Conseved domain search (intermediate) results
    CRef<CSearchResultSet> m_DomainResults;

    /// Pssm-protein search results
    CRef<CSearchResultSet> m_Results;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___DELTABLAST__HPP */
