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

/** @file local_db_adapter.cpp
 * Defines class which provides internal BLAST database representations to the
 * internal BLAST APIs
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/objmgr_query_data.hpp> // for CObjMgr_QueryFactory
#include <algo/blast/api/seqsrc_seqdb.hpp>  // for SeqDbBlastSeqSrcInit
#include <algo/blast/api/seqinfosrc_seqdb.hpp>  // for CSeqDbSeqInfoSrc
#include <algo/blast/api/seqinfosrc_seqvec.hpp> // for CSeqVecSeqInfoSrc
#include <algo/blast/api/setup_factory.hpp>
#include "seqsrc_query_factory.hpp"  // for QueryFactoryBlastSeqSrcInit
#include "psiblast_aux_priv.hpp"    // for CPsiBlastValidate
#include "seqinfosrc_bioseq.hpp"    // for CBioseqInfoSrc

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CLocalDbAdapter::CLocalDbAdapter(const CSearchDatabase& dbinfo)
    : m_SeqSrc(0), m_SeqInfoSrc(0), m_DbName(dbinfo.GetDatabaseName())
{
    m_DbInfo.Reset(new CSearchDatabase(dbinfo));
}

CLocalDbAdapter::CLocalDbAdapter(CRef<IQueryFactory> subject_sequences,
                                 CConstRef<CBlastOptionsHandle> opts_handle)
    : m_SeqSrc(0), m_SeqInfoSrc(0), m_SubjectFactory(subject_sequences),
    m_OptsHandle(opts_handle), m_DbName(kEmptyStr)
{
    if (subject_sequences.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Missing subject sequence data");
    }
    if (opts_handle.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }
    if (opts_handle->GetOptions().GetProgram() == ePSIBlast) {
        CPsiBlastValidate::QueryFactory(subject_sequences, *opts_handle,
                                        CPsiBlastValidate::eQFT_Subject);
    }

    CObjMgr_QueryFactory* objmgr_qf = NULL;
    if ( (objmgr_qf = dynamic_cast<CObjMgr_QueryFactory*>(&*m_SubjectFactory)) )
    {
        m_Subjects = objmgr_qf->GetTSeqLocVector();
        _ASSERT(!m_Subjects.empty());
    }
}

CLocalDbAdapter::CLocalDbAdapter(BlastSeqSrc* seqSrc,
                                 CRef<IBlastSeqInfoSrc> seqInfoSrc)
    : m_SeqSrc(seqSrc), m_SeqInfoSrc(seqInfoSrc), m_DbName(kEmptyStr)
{
}

CLocalDbAdapter::~CLocalDbAdapter()
{
    if (m_SeqSrc) {
        m_SeqSrc = BlastSeqSrcFree(m_SeqSrc);
    }
}

void
CLocalDbAdapter::ResetBlastSeqSrcIteration()
{
    if (m_SeqSrc) {
        BlastSeqSrcResetChunkIterator(m_SeqSrc);
    }
}

/// Checks if the BlastSeqSrc initialization succeeded
/// @throws CBlastException if BlastSeqSrc initialization failed
static void
s_CheckForBlastSeqSrcErrors(const BlastSeqSrc* seqsrc)
{
    if ( !seqsrc ) {
        return;
    }

    char* error_str = BlastSeqSrcGetInitError(seqsrc);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
}

BlastSeqSrc*
CLocalDbAdapter::MakeSeqSrc()
{
    if ( ! m_SeqSrc ) {
        if (m_DbInfo.NotEmpty()) {
            m_SeqSrc = CSetupFactory::CreateBlastSeqSrc(*m_DbInfo);
        } else if (m_SubjectFactory.NotEmpty() && m_OptsHandle.NotEmpty()) {
            const EBlastProgramType program =
                               m_OptsHandle->GetOptions().GetProgramType();
            if ( !m_Subjects.empty() ) {
                m_SeqSrc = QueryFactoryBlastSeqSrcInit(m_Subjects, program);
            } else {
                m_SeqSrc = QueryFactoryBlastSeqSrcInit(m_SubjectFactory,
                                                       program);
            }
            _ASSERT(m_SeqSrc);
        } else {
            abort();
        }
        s_CheckForBlastSeqSrcErrors(m_SeqSrc);
        _ASSERT(m_SeqSrc);
    }
    return m_SeqSrc;
}

IBlastSeqInfoSrc*
CLocalDbAdapter::MakeSeqInfoSrc()
{
    if ( !m_SeqInfoSrc ) {
        if (m_DbInfo.NotEmpty()) {
            m_SeqInfoSrc = new CSeqDbSeqInfoSrc(m_DbInfo->GetSeqDb());
            dynamic_cast<CSeqDbSeqInfoSrc *>(&*m_SeqInfoSrc)
                 ->SetFilteringAlgorithmId(m_DbInfo->GetFilteringAlgorithm());
        } else if (m_SubjectFactory.NotEmpty() && m_OptsHandle.NotEmpty()) {
            EBlastProgramType p(m_OptsHandle->GetOptions().GetProgramType());
            if ( !m_Subjects.empty() ) {
                m_SeqInfoSrc = new CSeqVecSeqInfoSrc(m_Subjects);
            } else {
                CRef<IRemoteQueryData> subj_data
                    (m_SubjectFactory->MakeRemoteQueryData());
                CRef<CBioseq_set> subject_bioseqs(subj_data->GetBioseqSet());
                bool is_prot = Blast_SubjectIsProtein(p) ? true : false;
                m_SeqInfoSrc = new CBioseqSeqInfoSrc(*subject_bioseqs, is_prot);
            }
        } else {
            abort();
        }
        _ASSERT(m_SeqInfoSrc);
    }
    return m_SeqInfoSrc;
}

bool
CLocalDbAdapter::IsProtein() const
{
    bool retval = false;
    if (m_DbInfo) {
        retval = m_DbInfo->IsProtein();
    } else if (m_OptsHandle) {
        const EBlastProgramType p = m_OptsHandle->GetOptions().GetProgramType();
        retval = Blast_SubjectIsProtein(p) ? true : false;
    } else if (m_SeqSrc) {
        retval = (bool)BlastSeqSrcGetIsProt(m_SeqSrc);
    } else {
        // Data type provided in a constructor, but not handled here
        abort();
    }
    return retval;
}

END_SCOPE(Blast)
END_NCBI_SCOPE

/* @} */
