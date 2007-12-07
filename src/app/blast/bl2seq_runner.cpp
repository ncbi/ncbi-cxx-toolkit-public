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
 * Author: Vahram Avagyan
 *
 */

/** @file bl2seq_runner.cpp
 *  Blast 2 sequences support for Blast command line applications
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include "bl2seq_runner.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif


CBl2Seq_Runner::CBl2Seq_Runner(bool is_query_protein,
                               bool is_subject_protein)
                               : m_is_bl2seq(false),
                               m_is_query_protein(is_query_protein),
                               m_is_subject_protein(is_subject_protein)
{}

CBl2Seq_Runner::~CBl2Seq_Runner()
{}

void CBl2Seq_Runner::ProcessDatabaseArgs(CRef<CBlastDatabaseArgs> db_args,
                                         CRef<CObjectManager> obj_mgr,
                                         objects::CScope* scope_formatter)
{
    m_is_bl2seq = db_args->IsSubjectProvided();
    if (!m_is_bl2seq)
        return;

    /*** Get the subject sequence(s) ***/
    SDataLoaderConfig dlconfig(m_is_subject_protein);
    CBlastInputSourceConfig iconfig(dlconfig, objects::eNa_strand_other, false, 
                              false, db_args->GetSubjectRange());
    m_fasta_subject.Reset(new CBlastFastaInputSource(*obj_mgr, 
                                db_args->GetSubjectInputStream(), iconfig,
                                m_subject_id_offset + 1));
    m_input_subject.Reset(new CBlastInput(m_fasta_subject.GetPointer()));
    m_subject_seqs = m_input_subject->GetAllSeqLocs();

    /*** Update the scope ***/
    scope_formatter->AddScope(m_fasta_subject->GetScope().GetObject());

    m_fasta_subject.Reset();
    m_input_subject.Reset();
}

bool CBl2Seq_Runner::IsBl2Seq() const
{
    return m_is_bl2seq;
}

void CBl2Seq_Runner::RunAndFormat(CBlastFastaInputSource* fasta_query,
                                  CBlastInput* input_query,
                                  CBlastOptionsHandle* opts_handle,
                                  CBlastFormat* formatter,
                                  CScope* scope_formatter)
{
    _ASSERT(m_is_bl2seq);

    /*** Process the input ***/
    while ( !fasta_query->End() ) {

        TSeqLocVector query_batch(input_query->GetNextSeqLocBatch());
        CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(query_batch));

        /*** Update the scope ***/
        scope_formatter->AddScope(fasta_query->GetScope().GetObject());

        /*** Perform BLAST search/alignment ***/
        CBl2Seq bl2seq(query_batch, m_subject_seqs, *opts_handle);
        CRef<CSearchResultSet> results = bl2seq.RunEx();

        /*** Output formatted results ***/
        ITERATE(CSearchResultSet, result, *results) {
                formatter->PrintOneResultSet(**result, *scope_formatter);
        }
    }
}
