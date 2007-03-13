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
 * Authors: Christiam Camacho
 *
 */

/** @file blast_unit_test.cpp
 * Unit tests for the CBl2Seq class
 */

#include <ncbi_pch.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include "test_objmgr.hpp"

#ifdef NCBI_OS_DARWIN
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#endif

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_UNIT_TEST(ProteinBlastInvalidSeqIdSelfHit)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetWhole().SetGi(-1);

    CRef<CScope> scope(new CScope(CTestObjMgr::Instance().GetObjMgr()));
    scope->AddDefaults();
    SSeqLoc query(loc, scope);

    TSeqLocVector subjects;
    {
        CRef<CSeq_loc> local_loc(new CSeq_loc());
        local_loc->SetWhole().SetGi(-1);

        CScope* local_scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        local_scope->AddDefaults();
        subjects.push_back(SSeqLoc(local_loc, local_scope));
    }

    // BLAST by concatenating all queries
    CBl2Seq blaster4all(query, subjects, eBlastp);
    TSeqAlignVector sas_v;
    BOOST_CHECK_THROW(sas_v = blaster4all.Run(), CBlastException);
}
