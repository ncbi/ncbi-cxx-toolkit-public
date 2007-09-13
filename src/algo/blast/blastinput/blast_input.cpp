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
 * Author:  Jason Papadopoulos
 *
 */

/** @file algo/blast/blastinput/blast_input.cpp
 * Convert sources of sequence data into blast sequence input
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/blastinput/blast_input.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CBlastInputConfig::CBlastInputConfig(const SDataLoaderConfig& dlconfig,
                                     objects::ENa_strand strand,
                                     bool lowercase,
                                     bool believe_defline,
                                     TSeqRange range,
                                     bool retrieve_seqs)
: m_Strand(strand), m_LowerCaseMask(lowercase), 
  m_BelieveDeflines(believe_defline), m_Range(range), m_DLConfig(dlconfig),
  m_RetrieveSeqData(retrieve_seqs)
{}

CBlastInput::CBlastInput(const CBlastInput& rhs)
{
    do_copy(rhs);
}

CBlastInput&
CBlastInput::operator=(const CBlastInput& rhs)
{
    do_copy(rhs);
    return *this;
}

void
CBlastInput::do_copy(const CBlastInput& rhs)
{
    if (this != &rhs) {
        m_Source = rhs.m_Source;
        m_BatchSize = rhs.m_BatchSize;
        if (rhs.m_AllSeqLocs.get() != 0) {
            m_AllSeqLocs.reset(new TSeqLocVector(rhs.m_AllSeqLocs->size()));
            copy(rhs.m_AllSeqLocs->begin(),
                 rhs.m_AllSeqLocs->end(),
                 m_AllSeqLocs->begin());
        }
        m_AllQueries = rhs.m_AllQueries;
    }
}

TSeqLocVector
CBlastInput::GetNextSeqLocBatch()
{
    TSeqLocVector retval;
    TSeqPos size_read = 0;

    while (size_read < m_BatchSize) {

        if (m_Source->End())
            break;

        retval.push_back(m_Source->GetNextSSeqLoc());

        SSeqLoc& loc = retval.back();

        if (loc.seqloc->IsInt()) {
            size_read += sequence::GetLength(loc.seqloc->GetInt().GetId(), 
                                            loc.scope);
        } else if (loc.seqloc->IsWhole()) {
            size_read += sequence::GetLength(loc.seqloc->GetWhole(),
                                            loc.scope);
        } else {
            // programmer error, CBlastInputSource should only return Seq-locs
            // of type interval or whole
            abort();
        }

        if (m_AllSeqLocs.get() == 0) {
            m_AllSeqLocs.reset(new TSeqLocVector());
        }
        m_AllSeqLocs->push_back(loc);
    }

    return retval;
}


CRef<CBlastQueryVector>
CBlastInput::GetNextSeqBatch()
{
    CRef<CBlastQueryVector> retval(new CBlastQueryVector);
    TSeqPos size_read = 0;

    while (size_read < m_BatchSize) {

        if (m_Source->End())
            break;

        CRef<CBlastSearchQuery> q(m_Source->GetNextSequence());
        CConstRef<CSeq_loc> loc = q->GetQuerySeqLoc();

        if (loc->IsInt()) {
            size_read += sequence::GetLength(loc->GetInt().GetId(),
                                             q->GetScope());
        } else if (loc->IsWhole()) {
            size_read += sequence::GetLength(loc->GetWhole(), q->GetScope());
        } else {
            // programmer error, CBlastInputSource should only return Seq-locs
            // of type interval or whole
            abort();
        }

        retval->AddQuery(q);

        if (m_AllQueries.Empty()) {
            m_AllQueries.Reset(new CBlastQueryVector());
        }
        m_AllQueries->AddQuery(q);
    }

    return retval;
}


TSeqLocVector
CBlastInput::GetAllSeqLocs()
{
    if (m_AllSeqLocs.get() == 0) {
        m_AllSeqLocs.reset(new TSeqLocVector());
    }

    while (!m_Source->End()) {
        m_AllSeqLocs->push_back(m_Source->GetNextSSeqLoc());
    }

    if (m_AllSeqLocs->empty() ) {
        // Copy from cached CBlastQueryVector
        if (m_AllQueries.NotEmpty() && !m_AllQueries->Empty()) {
            m_AllSeqLocs.reset(new TSeqLocVector(m_AllQueries->Size()));
            for (size_t i = 0; i < m_AllQueries->Size(); i++) {
                CRef<CBlastSearchQuery> bq =
                    m_AllQueries->GetBlastSearchQuery(i);
                (*m_AllSeqLocs)[i].seqloc = bq->GetQuerySeqLoc();
                (*m_AllSeqLocs)[i].scope = bq->GetScope();

                TMaskedQueryRegions mqr = bq->GetMaskedRegions();
                (*m_AllSeqLocs)[i].mask = MaskedQueryRegionsToPackedSeqLoc(mqr);
            }
        }
    }

    return *m_AllSeqLocs;
}


CRef<CBlastQueryVector>
CBlastInput::GetAllSeqs()
{
    if (m_AllQueries.Empty()) {
        m_AllQueries.Reset(new CBlastQueryVector());
    }

    while (!m_Source->End()) {
        CRef<CBlastSearchQuery> q(m_Source->GetNextSequence());
        m_AllQueries->AddQuery(q);
    }

    if (m_AllQueries->Empty()) {
        // Copy from cached TSeqLocVector
        if (m_AllSeqLocs.get() && !m_AllSeqLocs->empty()) {
            // it's safe to add empty query masks because these are
            // available from the blast::CSearchResultsSet class
            TMaskedQueryRegions empty_masks;
            ITERATE(TSeqLocVector, itr, *m_AllSeqLocs) {
                CRef<CBlastSearchQuery> bq (new CBlastSearchQuery(*itr->seqloc, 
                                                                  *itr->scope, 
                                                                  empty_masks));
                m_AllQueries->AddQuery(bq);
            }
        }
    }

    return m_AllQueries;
}


CBlastInputSource::CBlastInputSource(objects::CObjectManager& objmgr)
    : m_ObjMgr(objmgr),
      m_Scope(new CScope(objmgr))
{
    m_Scope->AddDefaults();
}


END_SCOPE(blast)
END_NCBI_SCOPE
