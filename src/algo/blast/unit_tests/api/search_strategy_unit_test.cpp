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
*   Unit test module to test search_strategy.cpp
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/api/search_strategy.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>

#include "test_objmgr.hpp"
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/exception.hpp>
#include <util/range.hpp>


using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(search_strategy)

BOOST_AUTO_TEST_CASE(emptyInput)
{
    CRef<CBlast4_request> empty_request(new CBlast4_request);
    BOOST_REQUIRE_THROW(CImportStrategy import_strat(empty_request), CBlastException);
}

BOOST_AUTO_TEST_CASE(testMegablast)
{
    const char* fname = "data/ss.asn";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> request = ExtractBlast4Request(in);
    CImportStrategy import_strat(request);
    BOOST_REQUIRE(import_strat.GetService() == "megablast");
    BOOST_REQUIRE(import_strat.GetProgram() == "blastn");
    BOOST_REQUIRE(import_strat.GetTask() == "megablast");
    BOOST_REQUIRE(import_strat.GetDBFilteringID() == 40);

    CRef<objects::CBlast4_queries> query = import_strat.GetQueries();
    BOOST_REQUIRE(query->IsPssm() == false);
    BOOST_REQUIRE(query->IsSeq_loc_list() == true);

    CRef<blast::CBlastOptionsHandle> opts_handle = import_strat.GetOptionsHandle();
    BOOST_REQUIRE_EQUAL(opts_handle->GetHitlistSize(), 500); 
    BOOST_REQUIRE_EQUAL(opts_handle->GetCullingLimit(), 0); 
    CBlastNucleotideOptionsHandle* blastn_opts = dynamic_cast<CBlastNucleotideOptionsHandle*> (&*opts_handle);
    BOOST_REQUIRE_EQUAL(blastn_opts->GetMatchReward(), 1); 
    BOOST_REQUIRE_EQUAL(blastn_opts->GetMismatchPenalty(), -2); 
}

BOOST_AUTO_TEST_CASE(testBlastp)
{
    const char* fname = "data/ss.blastp.asn";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> request = ExtractBlast4Request(in);
    CImportStrategy import_strat(request);
    BOOST_REQUIRE(import_strat.GetService() == "plain");
    BOOST_REQUIRE(import_strat.GetProgram() == "blastp");

    CRef<objects::CBlast4_queries> query = import_strat.GetQueries();
    BOOST_REQUIRE(query->IsPssm() == false);
    BOOST_REQUIRE(query->IsSeq_loc_list() == false);
    const CBioseq_set& bss = query->GetBioseq_set();
    list<CRef<CSeq_entry> > seq_entry = bss.GetSeq_set();
    BOOST_REQUIRE(seq_entry.front()->GetSeq().GetLength() == 232);

    CRef<objects::CBlast4_subject> subject = import_strat.GetSubject(); 
    BOOST_REQUIRE(subject->IsDatabase() == true);
    BOOST_REQUIRE(subject->GetDatabase() == "refseq_protein");

    CRef<blast::CBlastOptionsHandle> opts_handle = import_strat.GetOptionsHandle();
    BOOST_REQUIRE_EQUAL(opts_handle->GetHitlistSize(), 500); 
    BOOST_REQUIRE_EQUAL(opts_handle->GetCullingLimit(), 0); 
    CBlastAdvancedProteinOptionsHandle* blastp_opts = dynamic_cast<CBlastAdvancedProteinOptionsHandle*> (&*opts_handle);
    BOOST_REQUIRE(!strcmp("BLOSUM62", blastp_opts->GetMatrixName()));
    BOOST_REQUIRE_EQUAL(3, blastp_opts->GetWordSize());

}

BOOST_AUTO_TEST_CASE(testBlastnBl2seq)
{
    const char* fname = "data/ss.bl2seq.asn";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> request = ExtractBlast4Request(in);
    CImportStrategy import_strat(request);
    BOOST_REQUIRE(import_strat.GetService() == "plain");
    BOOST_REQUIRE(import_strat.GetProgram() == "blastn");
    BOOST_REQUIRE(import_strat.GetTask() == "blastn");

    CRef<objects::CBlast4_queries> query = import_strat.GetQueries();
    BOOST_REQUIRE(query->IsPssm() == false);
    BOOST_REQUIRE(query->IsSeq_loc_list() == false);
    const CBioseq_set& bss = query->GetBioseq_set();
    list<CRef<CSeq_entry> > seq_entry = bss.GetSeq_set();
    BOOST_REQUIRE(seq_entry.front()->GetSeq().GetLength() == 2772);

    CRef<objects::CBlast4_subject> subject = import_strat.GetSubject(); 
    BOOST_REQUIRE(subject->IsDatabase() == false);
    list<CRef< CBioseq> > subject_list = subject->GetSequences();
    BOOST_REQUIRE(subject_list.size() == 1);
    BOOST_REQUIRE(subject_list.front()->GetLength() == 9180);

    CRef<blast::CBlastOptionsHandle> opts_handle = import_strat.GetOptionsHandle();
    BOOST_REQUIRE_EQUAL(opts_handle->GetHitlistSize(), 500); 
    BOOST_REQUIRE_EQUAL(opts_handle->GetCullingLimit(), 0); 
    CBlastNucleotideOptionsHandle* blastn_opts = dynamic_cast<CBlastNucleotideOptionsHandle*> (&*opts_handle);
    BOOST_REQUIRE_EQUAL(blastn_opts->GetMatchReward(), 2); 
    BOOST_REQUIRE_EQUAL(blastn_opts->GetMismatchPenalty(), -3); 
}

BOOST_AUTO_TEST_SUITE_END()
