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
 * Author: Nathan Bouk
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <algo/id_mapper/id_mapper.hpp>


#include <boost/test/output_test_stream.hpp> 
using boost::test_tools::output_test_stream;

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BOOST_AUTO_TEST_SUITE(TestSuiteGencollIdMapper)

BOOST_AUTO_TEST_CASE(TestCaseUcscToRefSeqMapping)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CRef<CGC_Assembly> GenColl = GCService.GetAssembly("GCF_000001405.13", 
                                    CGCClient_GetAssemblyRequest::eLevel_scaffold);
    
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetLocal().SetStr("chr1");
    CRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
    
    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");
}


BOOST_AUTO_TEST_CASE(TestCaseUcscToRefSeqToUcscMapping)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CRef<CGC_Assembly> GenColl = GCService.GetAssembly("GCF_000001405.13", 
                                    CGCClient_GetAssemblyRequest::eLevel_scaffold);
    
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetLocal().SetStr("chr1");

    CRef<CSeq_loc> Mapped = Mapper.Map(*OrigLoc, MapSpec);
    
    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(Mapped->GetId()->GetSeqIdString(true), "NC_000001.10");

    // Guess the original ID's spec
    CGencollIdMapper::SIdSpec GuessSpec;
    Mapper.Guess(*OrigLoc, GuessSpec);

    // Map back with the guessed spec
    CRef<CSeq_loc> RoundTripped = Mapper.Map(*Mapped, GuessSpec);

    // Check that Round tripped is equal to original
    BOOST_CHECK(RoundTripped->Equals(*OrigLoc));
}


BOOST_AUTO_TEST_CASE(TestCaseUcscUnTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CRef<CGC_Assembly> GenColl = GCService.GetAssembly("GCF_000003205.2", 
                                    CGCClient_GetAssemblyRequest::eLevel_scaffold);
    
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Gi;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetLocal().SetStr("chrUn.004.10843");

    CRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
    
    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(Result->GetId()->GetGi(), 112070986);
}


BOOST_AUTO_TEST_CASE(TestCaseUcscPseudoTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CRef<CGC_Assembly> GenColl = GCService.GetAssembly("GCF_000001405.12", 
                                    CGCClient_GetAssemblyRequest::eLevel_component,
                                    0, 0, 2048 /* pseudo*/, 0);
    
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetInt().SetId().SetLocal().SetStr("chr1_random");
    OrigLoc->SetInt().SetFrom(500000);
    OrigLoc->SetInt().SetTo(510000);

    CRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
    
    CRef<CSeq_loc> Expected(new CSeq_loc());
    Expected->SetInt().SetId().Set("NT_113872.1");
    Expected->SetInt().SetFrom(57066);
    Expected->SetInt().SetTo(67066);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(*Expected));
}

BOOST_AUTO_TEST_SUITE_END();

