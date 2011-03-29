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

/** @file psiblast_iteration.hpp
 * Defines class which represents the iteration state in PSI-BLAST
 */

#ifndef ALGO_BLAST_API___PSIBLAST_ITERATION__HPP
#define ALGO_BLAST_API___PSIBLAST_ITERATION__HPP

#include <corelib/ncbiobj.hpp>
#include <algo/blast/core/blast_export.h>
#include <objects/seq/seq_id_handle.hpp>
#include <list>
#include <set>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// Forward declaration
class CPSIBlastOptionsHandle;

/// Represents the iteration state in PSI-BLAST
class NCBI_XBLAST_EXPORT CPsiBlastIterationState
{
public:
    /// Constructor
    /// @param num_iterations 
    ///     Number of iterations to perform. Use 0 to indicate that iterations 
    ///     must take place until convergence or the number of iterations 
    ///     desired [in]
    CPsiBlastIterationState(unsigned int num_iterations = 1);

    /// Destructor
    ~CPsiBlastIterationState();

    /// Allow implicit conversion to a boolean value, returning true if there
    /// are more iterations to perform or false if iterations are done.
    /// @sa HasConverged, HasMoreIterations
    operator bool();

    /// Determines if the PSI-BLAST search has converged (i.e.: no more new
    /// sequences have been found since the last iteration)
    /// @return true if search has converged, otherwise false.
    bool HasConverged();

    /// Determines if more iterations are still needed.
    /// @return true if there are more iterations to do, otherwise false.
    bool HasMoreIterations() const;

    /// List of CSeq_ids
    typedef set<objects::CSeq_id_Handle> TSeqIds;

    /// Retrieve the set of Seq-id's found in the previous iteration
    TSeqIds GetPreviouslyFoundSeqIds() const;

    /// Advance the iterator by passing it the list of Seq-ids which passed the
    /// inclusion criteria for the current iteration
    void Advance(const TSeqIds& list);

    /// Extract the sequence ids from the sequence alignment which identify
    /// those sequences that will be used for PSSM construction.
    /// @param seqalign 
    ///     Sequence alignment [in]
    /// @param opts 
    ///     options containing details for algorithm to select sequences from 
    ///     seqalign [in]
    /// @param retval 
    ///     List of sequence identifiers for those sequences from the
    ///     seqalign which will participate in PSSM construction [out]
    static void GetSeqIds(CConstRef<objects::CSeq_align_set> seqalign,
                          CConstRef<CPSIBlastOptionsHandle> opts,
                          TSeqIds& retval);

    /// Return the number of the current iteration
    unsigned int GetIterationNumber() const;

private:
    // No value semantics

    /// Prohibit copy constructor
    CPsiBlastIterationState(const CPsiBlastIterationState& rhs);

    /// Prohibit assignment operator
    CPsiBlastIterationState& operator=(const CPsiBlastIterationState& rhs);

    /// Number of iterations to perform
    unsigned int        m_TotalNumIterationsToDo;

    /// Number of iterations already done
    unsigned int        m_IterationsDone;

    /// Identifiers for sequences found in the previous iteration
    TSeqIds             m_PreviousData;
    
    /// Identifiers for sequences found in the current iteration
    TSeqIds             m_CurrentData;

    /// After the iteration state object has converged or exhausted its
    /// iterations, it shouldn't be modified, so it throws a CBlastException 
    /// if this happens
    void x_ThrowExceptionOnLogicError();
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___PSIBLAST_ITERATION__HPP */
