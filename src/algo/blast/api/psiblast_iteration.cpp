#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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

/// @file psiblast_iteration.cpp
/// Defines class which represents the iteration state in PSI-BLAST

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/psiblast_iteration.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include "psiblast_aux_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBlastIterationState::CPsiBlastIterationState(unsigned int num_iterations)
    : m_TotalNumIterationsToDo(num_iterations), m_IterationsDone(0)
{}

CPsiBlastIterationState::~CPsiBlastIterationState()
{}

bool
CPsiBlastIterationState::HasMoreIterations() const
{
    if (m_TotalNumIterationsToDo == 0 ||
        m_IterationsDone < m_TotalNumIterationsToDo) {
        return true;
    }
    return false;
}

bool
CPsiBlastIterationState::HasConverged() const
{
    // For an uninitialized object (i.e.: one that hasn't been 'advanced'), 
    // it doesn't make sense to have converged
    if (m_PreviousData.empty() && m_CurrentData.empty()) {
        return false;
    }
    // if the most current list of ids is empty, that means that no new
    // sequences were found and therefore we have converged.
    if ( !m_PreviousData.empty() && m_CurrentData.empty() ) {
        return true;
    }
    // if the size differs, they're obviously not the same :)
    if (m_PreviousData.size() != m_CurrentData.size()) {
        return false;
    }

    //std::sort(m_PreviousData.begin(), m_PreviousData.end());
    //std::sort(m_CurrentData.begin(), m_CurrentData.end());

    // Element by element comparison
    const TSeqIds::const_iterator end = m_PreviousData.end();
    TSeqIds::const_iterator prev = m_PreviousData.begin();
    TSeqIds::const_iterator curr = m_CurrentData.begin();
    bool retval = true;
    for (; prev != end; ++prev, ++curr) {
        if ( !(**prev).Match(**curr) ) {
            retval = false;
            break;
        }
    }

    return retval;
}

CPsiBlastIterationState::operator bool() const
{
    return (HasMoreIterations() && !HasConverged());
}

void
CPsiBlastIterationState::x_ThrowExceptionOnLogicError()
{
    if (!(*this)) {
        string msg("Should not modify a PSI-BLAST iteration after it has "
                   "converged or exhausted its iterations");
        NCBI_THROW(CBlastException, eNotSupported, msg);
    }
}

void
CPsiBlastIterationState::Advance(const TSeqIds& list)
{
    x_ThrowExceptionOnLogicError();

    m_PreviousData = m_CurrentData;
    m_CurrentData = list;
    m_IterationsDone++;
}

unsigned int
CPsiBlastIterationState::GetIterationNumber() const
{
    return m_IterationsDone+1;
}

void 
CPsiBlastIterationState::GetSeqIds(CConstRef<objects::CSeq_align_set> seqalign, 
                                   CConstRef<CPSIBlastOptionsHandle> opts, 
                                   TSeqIds& retval)
{
    retval.clear();

    CPsiBlastAlignmentProcessor proc;
    CPsiBlastAlignmentProcessor::THitIdentifiers hit_ids;
    proc(*seqalign, opts->GetInclusionThreshold(), hit_ids);

    retval.reserve(hit_ids.size());
    ITERATE(CPsiBlastAlignmentProcessor::THitIdentifiers, itr, hit_ids) {
        retval.push_back(itr->second);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
