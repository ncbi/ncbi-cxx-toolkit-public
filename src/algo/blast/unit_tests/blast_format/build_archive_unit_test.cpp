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
*   Unit test module to test building a blast archive
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
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqset/Bioseq_set.hpp>

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>


#include <algo/blast/format/build_archive.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>


using namespace ncbi;
using namespace ncbi::blast;
using namespace ncbi::objects;


BOOST_AUTO_TEST_SUITE(blast_archive)

BOOST_AUTO_TEST_CASE(BuildArchiveWithDB)
{
    // First read in the data to use.
    const char* fname = "data/archive.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);

    rb.LoadFromArchive();

    CRef<objects::CBlast4_queries> queries = rb.GetQueries();
 
    CConstRef<objects::CBioseq_set> bss_ref(&(queries->SetBioseq_set()));
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bss_ref));

    CBlastNucleotideOptionsHandle nucl_opts(CBlastOptions::eBoth);
    Int8 effective_search_space = nucl_opts.GetEffectiveSearchSpace();

    CRef<objects::CBlast4_archive> archive = 
		BlastBuildArchive(*query_factory,
				nucl_opts,
				*(rb.GetResultSet()),
				"nr");
   
    BOOST_REQUIRE(effective_search_space == nucl_opts.GetEffectiveSearchSpace());

    const CBlast4_request& request = archive->GetRequest();
    const CBlast4_request_body& body = request.GetBody();
    const CBlast4_queue_search_request& queue_search = body.GetQueue_search();
    
    BOOST_REQUIRE(queue_search.GetService() == "megablast");
    BOOST_REQUIRE(queue_search.GetProgram() == "blastn");
    BOOST_REQUIRE(queue_search.GetSubject().GetDatabase() == "nr");

    const CBlast4_get_search_results_reply& reply = archive->GetResults();

    BOOST_REQUIRE(reply.CanGetAlignments() == true);

}

BOOST_AUTO_TEST_CASE(BuildArchiveWithBl2seq)
{
    // First read in the data to use.
    const char* fname = "data/archive.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);

    rb.LoadFromArchive();

    CRef<objects::CBlast4_queries> queries = rb.GetQueries();
 
    CConstRef<objects::CBioseq_set> bss_ref(&(queries->SetBioseq_set()));
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bss_ref));

    CBlastNucleotideOptionsHandle nucl_opts(CBlastOptions::eBoth);

    CRef<objects::CBlast4_archive> archive = 
		BlastBuildArchive(*query_factory,
				nucl_opts,
				*(rb.GetResultSet()),
				*query_factory);
   
    const CBlast4_request& request = archive->GetRequest();
    const CBlast4_request_body& body = request.GetBody();
    const CBlast4_queue_search_request& queue_search = body.GetQueue_search();
    
    BOOST_REQUIRE(queue_search.GetService() == "megablast");
    BOOST_REQUIRE(queue_search.GetProgram() == "blastn");
    BOOST_REQUIRE(queue_search.GetSubject().IsSequences() == true);

    const CBlast4_get_search_results_reply& reply = archive->GetResults();

    BOOST_REQUIRE(reply.CanGetAlignments() == true);

}

BOOST_AUTO_TEST_SUITE_END()
