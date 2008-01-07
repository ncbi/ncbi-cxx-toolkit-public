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
#include <algo/blast/api/seqsrc_seqdb.hpp>  // for SeqDbBlastSeqSrcInit
#include <algo/blast/api/seqinfosrc_seqdb.hpp>  // for CSeqDbSeqInfoSrc
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
    : m_SeqSrc(0), m_SeqInfoSrc(0)
{
    m_DbInfo.Reset(new CSearchDatabase(dbinfo));
}

CLocalDbAdapter::CLocalDbAdapter(CRef<CSeqDB> seqdb)
    : m_SeqSrc(0), m_SeqInfoSrc(0), m_SeqDb(seqdb)
{
    if (m_SeqDb.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "NULL CSeqDB");
    }
}

CLocalDbAdapter::CLocalDbAdapter(CRef<IQueryFactory> subject_sequences,
                                 CConstRef<CBlastOptionsHandle> opts_handle)
    : m_SeqSrc(0), m_SeqInfoSrc(0), m_QueryFactory(subject_sequences),
    m_OptsHandle(opts_handle)
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
            x_InitSeqDB();
            m_SeqSrc = SeqDbBlastSeqSrcInit(m_SeqDb.GetNonNullPointer());
        } else if (m_SeqDb.NotEmpty()) {
            m_SeqSrc = SeqDbBlastSeqSrcInit(m_SeqDb.GetNonNullPointer());
        } else if (m_QueryFactory.NotEmpty() && m_OptsHandle.NotEmpty()) {
            m_SeqSrc = QueryFactoryBlastSeqSrcInit(m_QueryFactory,
                           m_OptsHandle->GetOptions().GetProgramType());
        } else {
            abort();
        }
        s_CheckForBlastSeqSrcErrors(m_SeqSrc);
        _ASSERT(m_SeqSrc);
    }
    return m_SeqSrc;
}

void
CLocalDbAdapter::x_InitSeqDB()
{
    _ASSERT(m_DbInfo.NotEmpty());
    if (m_SeqDb) {
        return;
    }

    const CSeqDB::ESeqType type = m_DbInfo->IsProtein()
        ? CSeqDB::eProtein
        : CSeqDB::eNucleotide;

    // FIXME: refactor code in SplitDB/LibEntrezCacheEx.cpp ?
    if ( !m_DbInfo->GetEntrezQueryLimitation().empty() ) {
        NCBI_THROW(CException, eUnknown, "Unimplemented");
    }
    if ( !m_DbInfo->GetGiListLimitation().empty() ) {
        NCBI_THROW(CException, eUnknown, "Unimplemented");
    }

    /*
    CRef<CMySeqDbGiList> g(new
        CMySeqDbGiList(m_DbInfo->GetEntrezQueryLimitation(),
        m_DbInfo->GetGiListLimitation()));

    m_DbHandle.Reset(new CSeqDB(m_DbInfo->GetDatabaseName(), type, &*g));
    */
    m_SeqDb.Reset(new CSeqDB(m_DbInfo->GetDatabaseName(), type));
}

IBlastSeqInfoSrc*
CLocalDbAdapter::MakeSeqInfoSrc()
{
    if ( !m_SeqInfoSrc ) {
        if (m_DbInfo.NotEmpty()) {
            x_InitSeqDB();
            m_SeqInfoSrc = new CSeqDbSeqInfoSrc(m_SeqDb);
        } else if (m_SeqDb.NotEmpty()) {
            m_SeqInfoSrc = new CSeqDbSeqInfoSrc(m_SeqDb);
        } else if (m_QueryFactory.NotEmpty() && m_OptsHandle.NotEmpty()) {
            CRef<IRemoteQueryData> subj_data
                (m_QueryFactory->MakeRemoteQueryData());
            CRef<CBioseq_set> subject_bioseqs(subj_data->GetBioseqSet());
            EBlastProgramType p(m_OptsHandle->GetOptions().GetProgramType());
            bool is_prot = Blast_SubjectIsProtein(p) ? true : false;
            m_SeqInfoSrc = new CBioseqSeqInfoSrc(*subject_bioseqs, is_prot);
        } else {
            abort();
        }
        _ASSERT(m_SeqInfoSrc);
    }
    return m_SeqInfoSrc;
}

END_SCOPE(Blast)
END_NCBI_SCOPE

/* @} */
