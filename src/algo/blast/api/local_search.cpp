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

/** @file local_search.cpp
 * This file implements the Uniform Blast Search Interface in terms of
 * the local BLAST database search class
 * NOTE: This is OBJECT MANAGER DEPENDANT because of its use of CDbBlast!
 */

#include <ncbi_pch.hpp>

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

// Object manager dependencies
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <objmgr/object_manager.hpp>

// BLAST includes
#include <algo/blast/api/local_search.hpp>
#include <algo/blast/api/psiblast.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include "psiblast_aux_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Supporting elements

//
// Factory
//

CRef<ISeqSearch>
CLocalSearchFactory::GetSeqSearch()
{
    return CRef<ISeqSearch>(new CLocalSeqSearch());
}

CRef<IPssmSearch>
CLocalSearchFactory::GetPssmSearch()
{
    return CRef<IPssmSearch>(new CLocalPssmSearch());
}

CRef<CBlastOptionsHandle>
CLocalSearchFactory::GetOptions(EProgram program)
{
    // FIXME: should do some validation for acceptable programs by the
    // implementation (i.e.: CDbBlast)
    return CRef<CBlastOptionsHandle>(CBlastOptionsFactory::Create(program));
}

//
// Seq Search
//

// NOTE: Local search object is re-created every time it is run.
CRef<CSearchResultSet>
CLocalSeqSearch::Run()
{
    if ( m_QueryFactory.Empty() ) {
        NCBI_THROW(CSearchException, eConfigErr, "No queries specified");
    }
    if ( m_Database.Empty() ) {
        NCBI_THROW(CSearchException, eConfigErr, "No database name specified");
    }
    if ( !m_SearchOpts ) {
        NCBI_THROW(CSearchException, eConfigErr, "No options specified");
    }
    // This is delayed to this point to guarantee that the options are
    // populated
    
    m_LocalBlast.Reset(new CLocalBlast(m_QueryFactory, m_SearchOpts,
                                       *m_Database));
    
    return m_LocalBlast->Run();
}

void 
CLocalSeqSearch::SetOptions(CRef<CBlastOptionsHandle> opts)
{
    m_SearchOpts = opts;
}

void 
CLocalSeqSearch::SetSubject(CConstRef<CSearchDatabase> subject)
{
    m_Database = subject;
}

void 
CLocalSeqSearch::SetQueryFactory(CRef<IQueryFactory> query_factory)
{
    m_QueryFactory = query_factory;
}

//
// Psi Search
//

void 
CLocalPssmSearch::SetOptions(CRef<CBlastOptionsHandle> opts)
{
    m_SearchOpts = opts;
}

void 
CLocalPssmSearch::SetSubject(CConstRef<CSearchDatabase> subject)
{
    m_Subject = subject;
}

void 
CLocalPssmSearch::SetQuery(CRef<objects::CPssmWithParameters> pssm)
{
    CPsiBlastValidate::Pssm(*pssm);
    m_Pssm = pssm;
}

CRef<CSearchResultSet>
CLocalPssmSearch::Run()
{

    CConstRef<CPSIBlastOptionsHandle> psi_opts;
    psi_opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*m_SearchOpts));
    if (psi_opts.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Options for CLocalPssmSearch are not PSI-BLAST");
    }

    CConstRef<CBioseq> query(&m_Pssm->GetPssm().GetQuery().GetSeq());
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(query)); /* NCBI_FAKE_WARNING */

    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_Subject));
    CPsiBlast psiblast(query_factory, dbadapter, psi_opts);
    CRef<CSearchResultSet> retval = psiblast.Run();

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
