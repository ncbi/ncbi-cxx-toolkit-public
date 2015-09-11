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
 * ===========================================================================
 */

/// @file bl2seq.cpp
/// Implementation of CBl2Seq class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include "blast_objmgr_priv.hpp"
#include <algo/blast/api/objmgr_query_data.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBl2Seq::CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p)
    : m_DbScanMode(false), m_InterruptFnx(0), m_InterruptUserData(0)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

void CBl2Seq::x_InitCLocalBlast()
{
    _ASSERT( !m_tQueries.empty() );
    _ASSERT( !m_tSubjects.empty() );
    _ASSERT( !m_OptsHandle.Empty() );
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(m_tQueries));
    CRef<IQueryFactory> subject_factory(new CObjMgr_QueryFactory(m_tSubjects));
    CRef<CLocalDbAdapter> db(new CLocalDbAdapter(subject_factory, m_OptsHandle, m_DbScanMode));
    m_Blast.Reset(new CLocalBlast(query_factory, m_OptsHandle, db));
    if (m_InterruptFnx != NULL) {
        m_Blast->SetInterruptCallback(m_InterruptFnx, m_InterruptUserData);
    }
    // Set the hitlist size to the total number of subject sequences, to 
    // make sure that no hits are discarded (ported from CBl2Seq::SetupSearch
    m_OptsHandle->SetHitlistSize((int) m_tSubjects.size());
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject,
                 CBlastOptionsHandle& opts)
    : m_DbScanMode(false), m_InterruptFnx(0), m_InterruptUserData(0)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 EProgram p, bool dbscan_mode)
    : m_DbScanMode(dbscan_mode), m_InterruptFnx(0), m_InterruptUserData(0)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts, bool dbscan_mode)
    : m_DbScanMode(dbscan_mode), m_InterruptFnx(0), m_InterruptUserData(0)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 EProgram p, bool dbscan_mode)
    : m_DbScanMode(dbscan_mode), m_InterruptFnx(0), m_InterruptUserData(0)
{
    x_Init(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts, bool dbscan_mode)
    : m_DbScanMode(dbscan_mode), m_InterruptFnx(0), m_InterruptUserData(0)
{
    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

void CBl2Seq::x_Init(const TSeqLocVector& queries, const TSeqLocVector& subjs)
{
    m_tQueries = queries;
    m_tSubjects = subjs;
    mi_pDiagnostics = NULL;
}

CBl2Seq::~CBl2Seq()
{ 
    x_ResetInternalDs();
}

void
CBl2Seq::x_ResetInternalDs()
{
    // should be changed if derived classes are created
    m_Messages.clear();
    mi_pDiagnostics = Blast_DiagnosticsFree(mi_pDiagnostics);
    m_AncillaryData.clear();
    m_Results.Reset();
}

extern CRef<CSeq_align_set> CreateEmptySeq_align_set();

TSeqAlignVector
CBl2Seq::CSearchResultSet2TSeqAlignVector(CRef<CSearchResultSet> res)
{
    if (res.Empty()) {
        return TSeqAlignVector();
    }
    _ASSERT(res->GetResultType() == eSequenceComparison);
    TSeqAlignVector retval;
    retval.reserve(res->GetNumResults());
    ITERATE(CSearchResultSet, r, *res) {
        CRef<CSeq_align_set> sa;
        if ((*r)->HasAlignments()) {
            sa.Reset(const_cast<CSeq_align_set*>(&*(*r)->GetSeqAlign()));
        } else {
            sa.Reset(CreateEmptySeq_align_set());
        }
        retval.push_back(sa);
    }
    return retval;
}

TSeqAlignVector
CBl2Seq::Run()
{
    if (m_Results.NotEmpty()) {
        // return cached results from previous run
        return CBl2Seq::CSearchResultSet2TSeqAlignVector(m_Results);
    }

    (void) RunEx();
    x_BuildAncillaryData();
    return CBl2Seq::CSearchResultSet2TSeqAlignVector(m_Results);
}

void
CBl2Seq::x_BuildAncillaryData()
{
    m_AncillaryData.clear();
    m_AncillaryData.reserve(m_Results->size());
    ITERATE(CSearchResultSet, r, *m_Results) {
        m_AncillaryData.push_back((*r)->GetAncillaryData());
    }
}

CRef<CSearchResultSet>
CBl2Seq::RunEx()
{
    x_InitCLocalBlast();
    if (m_Results.NotEmpty()) {
        // return cached results from previous run
        return m_Results;
    }

    //m_OptsHandle->GetOptions().DebugDumpText(cerr, "m_OptsHandle", 1);
    _ASSERT(m_Blast.NotEmpty());
    m_Results = m_Blast->Run();
    m_Messages = m_Blast->GetSearchMessages();
    if (m_Blast->m_InternalData.NotEmpty()) {
        mi_pDiagnostics =
            Blast_DiagnosticsCopy(m_Blast->m_InternalData->m_Diagnostics->GetPointer());
    }
    _ASSERT(m_Results->GetResultType() == eSequenceComparison);
    return m_Results;
}

TSeqLocInfoVector
CBl2Seq::GetFilteredQueryRegions() const
{
    return m_Results->GetFilteredQueryRegions();
}

void
CBl2Seq::GetFilteredSubjectRegions(vector<TSeqLocInfoVector>& retval) const
{
    retval.clear();
    if (m_Results.Empty() || m_Results->empty()) {
        return;
    }
    ITERATE(CSearchResultSet, res, *m_Results) {
        TSeqLocInfoVector subj_masks;
        (*res)->GetSubjectMasks(subj_masks);
        retval.push_back(subj_masks);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
