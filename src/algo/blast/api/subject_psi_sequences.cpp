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

/** @file subject_psi_sequences.cpp
 * Defines an implementation of the IPsiBlastSubject interface which uses
 * sequences as its input
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "subject_psi_sequences.hpp"
#include <algo/blast/api/seqsrc_query_factory.hpp>
#include "psiblast_aux_priv.hpp"
#include "seqinfosrc_bioseq.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastSubjectSeqs::CBlastSubjectSeqs(CRef<IQueryFactory> subject_sequences, 
                                     CConstRef<CBlastOptionsHandle> opts_handle)
: m_QueryFactory(subject_sequences), m_OptsHandle(opts_handle)
{}

/// This function makes sure that none of the required data is NULL or "empty"
/// @param query_factory Sequence data [in]
/// @param opts_handle handle to BLAST options [in]
/// @throw CBlastException in case of validation failure
void s_CheckAgainstNullData(CRef<IQueryFactory> query_factory,
                            CConstRef<CBlastOptionsHandle> opts_handle)
{
    if (query_factory.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing subject sequence data");
    }
    if (opts_handle.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }
}

void
CBlastSubjectSeqs::Validate()
{
    s_CheckAgainstNullData(m_QueryFactory, m_OptsHandle);

    CPsiBlastValidate::QueryFactory(m_QueryFactory, *m_OptsHandle,
                                    CPsiBlastValidate::eQFT_Subject);
}

bool
CBlastSubjectSeqs::x_DetermineMoleculeType()
{
    s_CheckAgainstNullData(m_QueryFactory, m_OptsHandle);

    EBlastProgramType program(m_OptsHandle->GetOptions().GetProgramType());
    return Blast_SubjectIsProtein(program) ? true : false;
}

void
CBlastSubjectSeqs::x_MakeSeqSrc()
{
    s_CheckAgainstNullData(m_QueryFactory, m_OptsHandle);

    m_SeqSrc = QueryFactoryBlastSeqSrcInit(m_QueryFactory,
                           m_OptsHandle->GetOptions().GetProgramType());
    m_OwnSeqSrc = true;
}

void
CBlastSubjectSeqs::x_MakeSeqInfoSrc()
{
    s_CheckAgainstNullData(m_QueryFactory, m_OptsHandle);

    CRef<IRemoteQueryData> subj_data(m_QueryFactory->MakeRemoteQueryData());
    CRef<CBioseq_set> subject_bioseqs(subj_data->GetBioseqSet());
    m_SeqInfoSrc = new CBioseqSeqInfoSrc(*subject_bioseqs,
                                         x_DetermineMoleculeType());
    m_OwnSeqInfoSrc = true;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

