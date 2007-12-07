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

/// @file effsearchspace_calc.hpp
/// Declares auxiliary class to calculate the effective search space

#ifndef ALGO_BLAST_API___EFFSEARCHSPACE_CALC__HPP
#define ALGO_BLAST_API___EFFSEARCHSPACE_CALC__HPP

#include <corelib/ncbistd.hpp>

#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_options.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

/// Auxiliary class to compute the effective search space
class NCBI_XBLAST_EXPORT CEffectiveSearchSpaceCalculator {
public:
    /// Constructor
    /// @param query_factory source for query sequence(s) [in]
    /// @param options BLAST options [in]
    /// @param db_num_seqs number of sequences in the database/subjects to
    /// search [in]
    /// @param db_num_bases number of bases/residues in the database/subjects to
    /// search [in]
    /// @param sbp BlastScoreBlk to be used.  If NULL another instance will be created [in]
    CEffectiveSearchSpaceCalculator(CRef<IQueryFactory> query_factory,
                                    const CBlastOptions& options,
                                    Int4 db_num_seqs,
                                    Int8 db_num_bases,
                                    BlastScoreBlk* sbp = NULL);

    /// Retrieve the effective search space calculated for a given query
    /// @param query_index index of the query sequence of interest
    Int8 GetEffSearchSpace(size_t query_index = 0) const;

    /// Retrieve the effective search space calculated for a given query
    /// context. This is needed because translated searches might have slightly
    /// different search spaces for each of its contexts.
    /// @param ctx_index index of the query sequence of interest
    Int8 GetEffSearchSpaceForContext(size_t ctx_index) const;

private:
    CRef<IQueryFactory> m_QueryFactory; ///< source of query sequence(s)
    EBlastProgramType m_Program;        ///< BLAST program
    BlastQueryInfo* m_QueryInfo;        ///< struct to store eff. search spaces
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___EFFSEARCHSPACE_CALC__HPP */
