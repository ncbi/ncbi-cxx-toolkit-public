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

/// @file magicblast.hpp
/// Declares CMagicBlast, the C++ API for the BLAST RNA-Seq mapping engine.

#ifndef ALGO_BLAST_API___MAGICBLAST__HPP
#define ALGO_BLAST_API___MAGICBLAST__HPP

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/magicblast_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declarations
class IQueryFactory;

/// BLAST RNA-Seq mapper
class NCBI_XBLAST_EXPORT CMagicBlast : public CObject, public CThreadable
{
public:

public:
    /// Constructor to map short reads as queries to a genome as BLAST database
    /// @param query_factory 
    ///     Short reads sequence to map [in]
    /// @param blastdb
    ///     Adapter to the BLAST database [in]
    /// @param options
    ///     MAGIC-BLAST options [in]
    CMagicBlast(CRef<IQueryFactory> query_factory,
                CRef<CLocalDbAdapter> blastdb,
                CRef<CMagicBlastOptionsHandle> options);

    /// Destructor
    ~CMagicBlast() {}

    /// Run the RNA-Seq mapping
    CRef<CSeq_align_set> Run(void);

    TSearchMessages GetSearchMessages(void) const
    {return m_Messages;}


protected:
    /// Prohibit copy constructor
    CMagicBlast(const CMagicBlast& rhs);
    /// Prohibit assignment operator
    CMagicBlast& operator=(const CMagicBlast& rhs);

    /// Perform sanity checks on input arguments
    void x_Validate(void);

    /// Create results
    CRef<CSeq_align_set> x_CreateSeqAlignSet(BlastMappingResults* results);

private:
    /// Queries
    CRef<IQueryFactory> m_Queries;

    /// Reference to a BLAST subject/database object
    CRef<CLocalDbAdapter> m_LocalDbAdapter;

    /// Options to configure the search
    CRef<CBlastOptions> m_Options;

    /// Internal data strctures
    CRef<SInternalData> m_InternalData;

    /// Warning and error messages
    TSearchMessages m_Messages;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___MAGICBLAST__HPP */
