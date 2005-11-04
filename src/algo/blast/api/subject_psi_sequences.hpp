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

/** @file subject_psi_sequences.hpp
 * Declares an implementation of the IPsiBlastSubject interface which uses
 * sequences as its input
 */

#ifndef ALGO_BLAST_API___SUBJECT_SEQUENCES__HPP
#define ALGO_BLAST_API___SUBJECT_SEQUENCES__HPP

#include "psiblast_subject.hpp"
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Interface to obtain subject data for PSI-BLAST 2 sequences search.
class NCBI_XBLAST_EXPORT CBlastSubjectSeqs : public IPsiBlastSubject
{
public:
    /// Constructor
    /// @param subject_sequences
    ///     Set of sequences which should be used as subjects
    /// @param opts_handle
    ///     Options to be used (needed to create the ILocalQueryData)
    CBlastSubjectSeqs(CRef<IQueryFactory> subject_sequences,
                      CConstRef<CBlastOptionsHandle> opts_handle);

    /// Perform sanity checks on the input data
    virtual void Validate();

protected:
    /// Creates a BlastSeqSrc from the input data
    virtual void x_MakeSeqSrc();

    /// Creates a IBlastSeqInfoSrc from the input data
    virtual void x_MakeSeqInfoSrc();

private:
    /// IQueryFactory containing the subject sequences
    CRef<IQueryFactory> m_QueryFactory;

    /// Options to be used when instantiating the subject sequences
    CConstRef<CBlastOptionsHandle> m_OptsHandle;

    /// Returns true if the sequences provided are proteins, otherwise 
    /// returns false
    bool x_DetermineMoleculeType();

    /// Prohibit copy constructor
    CBlastSubjectSeqs(const CBlastSubjectSeqs& rhs);

    /// Prohibit assignment operator
    CBlastSubjectSeqs& operator=(const CBlastSubjectSeqs& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___SUBJECT_SEQUENCES__HPP */
