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
* Author:  Tom Madden
*
* File Description:
*   Unit test module for CBlastFormat and associated classes.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>


#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/format/blast_async_format.hpp>


#include <algo/blast/format/build_archive.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace ncbi::blast;
using namespace ncbi::objects;


BOOST_AUTO_TEST_SUITE(blast_format)

BOOST_AUTO_TEST_CASE(BlastFormatTest)
{
    // First read in the data to use.
    const char* fname = "data/archive.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);

    rb.LoadFromArchive();

    CRef<objects::CBlast4_queries> queries = rb.GetQueries();
 
    CConstRef<objects::CBioseq_set> bss_ref(&(queries->SetBioseq_set()));
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bss_ref));
    const CBioseq& bs = bss_ref->GetSeq_set().front()->GetSeq();

    CBlastNucleotideOptionsHandle nucl_opts(CBlastOptions::eBoth);

    const bool kIsProtein = false;
    SDataLoaderConfig dlconfig("refseq_rna", kIsProtein);
    CRef<CScope> scope(CBlastScopeSource(dlconfig).NewScope());
    scope->AddBioseq(bs);
    CRef<CSearchDatabase> target_db( new CSearchDatabase("refseq_rna", CSearchDatabase::eBlastDbIsNucleotide));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*target_db));
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*(bs.GetFirstId()));
    loc->SetWhole(*id);
    CRef<CBlastSearchQuery> query(new CBlastSearchQuery(*loc, *scope));
    CRef<CBlastQueryVector> q_vec(new CBlastQueryVector);
    q_vec->push_back(query);
    CRef<CSearchResultSet> blast_results = rb.GetResultSet();

    
    CNcbiOstrstream streamBuffer;
    CBlastFormat formatter(nucl_opts.GetOptions(), *db_adapter,
                          ncbi::blast::CFormattingArgs::EOutputFormat::ePairwise, 
			  false, streamBuffer, 10, 10, *scope);

    BOOST_REQUIRE(formatter.GetDbTotalLength() > 0);

    formatter.PrintOneResultSet((*blast_results)[0], q_vec);
    string myReport = CNcbiOstrstreamToString(streamBuffer);

    BOOST_REQUIRE(myReport.length() > 0);
    BOOST_REQUIRE(myReport.find("Query=") != std::string::npos);
}

#ifdef NCBI_THREADS
BOOST_AUTO_TEST_CASE(BlastAsyncFormatTest)
{
    // First read in the data to use.
    const char* fname = "data/archive.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);

    rb.LoadFromArchive();

    CRef<objects::CBlast4_queries> queries = rb.GetQueries();
 
    CConstRef<objects::CBioseq_set> bss_ref(&(queries->SetBioseq_set()));
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bss_ref));
    const CBioseq& bs = bss_ref->GetSeq_set().front()->GetSeq();

    CBlastNucleotideOptionsHandle nucl_opts(CBlastOptions::eBoth);

    const bool kIsProtein = false;
    SDataLoaderConfig dlconfig("refseq_rna", kIsProtein);
    CRef<CScope> scope(CBlastScopeSource(dlconfig).NewScope());
    scope->AddBioseq(bs);
    CRef<CSearchDatabase> target_db( new CSearchDatabase("refseq_rna", CSearchDatabase::eBlastDbIsNucleotide));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*target_db));
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*(bs.GetFirstId()));
    loc->SetWhole(*id);
    CRef<CBlastSearchQuery> query(new CBlastSearchQuery(*loc, *scope));
    CRef<CBlastQueryVector> q_vec(new CBlastQueryVector);
    q_vec->push_back(query);
    CRef<CSearchResultSet> blast_results = rb.GetResultSet();

    CBlastAsyncFormatThread* formatThr = new CBlastAsyncFormatThread();
    formatThr->Run();
    
    CNcbiOstrstream streamBuffer;
    CRef<CBlastFormat> formatter(new CBlastFormat(nucl_opts.GetOptions(), *db_adapter,
                          ncbi::blast::CFormattingArgs::EOutputFormat::ePairwise, 
			  false, streamBuffer, 10, 10, *scope));

    vector<SFormatResultValues> results_v;
    results_v.push_back(SFormatResultValues(q_vec, blast_results, formatter));
    formatThr->QueueResults(0, results_v);
    formatThr->Join();
    
    string myReport = CNcbiOstrstreamToString(streamBuffer);

    BOOST_REQUIRE(myReport.length() > 0);
    BOOST_REQUIRE(myReport.find("Query=") != std::string::npos);
}

// Insert results after call to Finalize.
BOOST_AUTO_TEST_CASE(BlastAsyncFormatFinalizeThrow)
{
    CBlastAsyncFormatThread* formatThr = new CBlastAsyncFormatThread();
    formatThr->Run();
    
    formatThr->Finalize();
    vector<SFormatResultValues> results_v;
    BOOST_REQUIRE_THROW(formatThr->QueueResults(0, results_v), CException);
    formatThr->Finalize();
    formatThr->Join();
}

// Attempt to insert the same batch number twice.
BOOST_AUTO_TEST_CASE(BlastAsyncFormatDuplicateThrow)
{
    CBlastAsyncFormatThread* formatThr = new CBlastAsyncFormatThread();
    formatThr->Run();
    
    vector<SFormatResultValues> results_v;
    formatThr->QueueResults(0, results_v);
    BOOST_REQUIRE_THROW(formatThr->QueueResults(0, results_v), CException);
    formatThr->Finalize();
    formatThr->Join();
}
#endif // NCBI_THREADS
BOOST_AUTO_TEST_SUITE_END()
