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
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold
                             )
    );

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
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold
                             )
    );

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


BOOST_AUTO_TEST_CASE(TestCaseUcscUnTest_Scaffold)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000003205.2",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold
                             )
    );

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
    BOOST_CHECK_EQUAL(Result->GetId()->GetGi(), GI_FROM(TIntId, 112070986)); // AAFC03080232.1
}


BOOST_AUTO_TEST_CASE(TestCaseUcscUnTest_Comp)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000003205.2",
                              CGCClient_GetAssemblyRequest::eLevel_component
                             )
    );

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
    BOOST_CHECK_EQUAL(Result->GetId()->GetGi(), 112070986); // AAFC03080232.1
}


BOOST_AUTO_TEST_CASE(TestCaseUcscPseudoTest_Scaffold)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.12",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold,
                              0,
                              0,
                              2048, // CGencollAccess::fAttribute_include_UCSC_pseudo_scaffolds
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_top_level;
    
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


BOOST_AUTO_TEST_CASE(TestCaseUcscPseudoTest_Comp)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.12",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_top_level;
    
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


// map down  test
BOOST_AUTO_TEST_CASE(TestCaseDownMapTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_component;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    OrigLoc.SetInt().SetId().Set("NC_000001.10");
    OrigLoc.SetInt().SetFrom(50000000);
    OrigLoc.SetInt().SetTo(50000001);

    CRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);

    // Expected component level result
    CSeq_loc Expected;
    Expected.SetInt().SetId().Set("AL356789.16");
    Expected.SetInt().SetFrom(56981);
    Expected.SetInt().SetTo(56982);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(Expected));
}


// map down scaf  test
BOOST_AUTO_TEST_CASE(TestCaseDownScafMapTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_scaffold;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    OrigLoc.SetInt().SetId().Set("NC_000001.10");
    OrigLoc.SetInt().SetFrom(50000000);
    OrigLoc.SetInt().SetTo(50000001);

    CRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);

    // Expected component level result
    CSeq_loc Expected;
    Expected.SetInt().SetId().Set("GL000006.1");
    Expected.SetInt().SetFrom(19971918);
    Expected.SetInt().SetTo(19971919);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(Expected));
}

// upmap test
BOOST_AUTO_TEST_CASE(TestCaseUpMapTest_RefSeqAssm)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_top_level;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    CSeq_interval& orig_ival = OrigLoc.SetInt();
    orig_ival.SetId().Set(CSeq_id::e_Local, "AL451051.6");
    orig_ival.SetFrom(5000);
    orig_ival.SetTo(5001);

    CConstRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);

    // Expected component level result
    CSeq_loc Expected;
    CSeq_interval& exp_ival = Expected.SetInt();
    exp_ival.SetId().Set("CM000663.1");
    exp_ival.SetFrom(100236283);
    exp_ival.SetTo(100236284);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(Expected));
}


// upmap test
BOOST_AUTO_TEST_CASE(TestCaseUpMapTest_GenBankAssm)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCA_000001405.1",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_top_level;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    CSeq_interval& orig_ival = OrigLoc.SetInt();
    orig_ival.SetId().Set(CSeq_id::e_Local, "AL451051.6");
    orig_ival.SetFrom(5000);
    orig_ival.SetTo(5001);

    CConstRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);

    // Expected component level result
    CSeq_loc Expected;
    CSeq_interval& exp_ival = Expected.SetInt();
    exp_ival.SetId().Set("CM000663.1");
    exp_ival.SetFrom(100236283);
    exp_ival.SetTo(100236284);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(Expected));
}


// upmap scaffold test
BOOST_AUTO_TEST_CASE(TestCaseUpMapScaffoldTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_component,
                              0,
                              0,
                              2048, // pseudo
                              0
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_scaffold;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    OrigLoc.SetInt().SetId().Set("AL451051.6");
    OrigLoc.SetInt().SetFrom(5000);
    OrigLoc.SetInt().SetTo(5001);

    CConstRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);

    // Expected component level result
    CSeq_loc Expected;
    Expected.SetInt().SetId().Set("GL000006.1");
    Expected.SetInt().SetFrom(70208201);
    Expected.SetInt().SetTo(70208202);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(Expected));
}


// pattern text
BOOST_AUTO_TEST_CASE(TestCasePatternMapping)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetLocal().SetStr("1");
    CConstRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");


    OrigLoc->SetWhole().SetLocal().SetStr("C1");
    Result = Mapper.Map(*OrigLoc, MapSpec);
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");


    OrigLoc->SetWhole().SetLocal().SetStr("LG1");
    Result = Mapper.Map(*OrigLoc, MapSpec);
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");


    OrigLoc->SetWhole().SetLocal().SetStr("WAKKAWAKKA1");
    Result = Mapper.Map(*OrigLoc, MapSpec);
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");
}


// Up/Down, Round Trip, Pattern test
BOOST_AUTO_TEST_CASE(TestCaseEverythingTest)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CConstRef<CGC_Assembly> GenColl(
        GCService.GetAssembly("GCF_000001405.13",
                              CGCClient_GetAssemblyRequest::eLevel_scaffold
                             )
    );

    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;
    MapSpec.Role = eGC_SequenceRole_scaffold;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetInt().SetId().SetLocal().SetStr("LG2");
    OrigLoc->SetInt().SetFrom(123456789);
    OrigLoc->SetInt().SetTo(123456798);

    CRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);

    CRef<CSeq_loc> Expected(new CSeq_loc());
    Expected->SetInt().SetId().Set("NT_022135.16");
    Expected->SetInt().SetFrom(13205452);
    Expected->SetInt().SetTo(13205461);

    // Check that Map results meet expectations
    BOOST_CHECK(Result->Equals(*Expected));

    CGencollIdMapper::SIdSpec GuessSpec;
    Mapper.Guess(*OrigLoc, GuessSpec);
    BOOST_CHECK_EQUAL(GuessSpec.ToString(), "NotPrim:Private:NotSet::LG%s:CHRO");

    CRef<CSeq_loc> RoundTrip = Mapper.Map(*Result, GuessSpec);

    // Check that Map results meet expectations
    BOOST_CHECK(RoundTrip->Equals(*OrigLoc));
}


// primary map test
BOOST_AUTO_TEST_CASE(TestCaseSpecPrimaryMap)
{
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.Primary = true;
 
    // Simple ID
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetLocal().SetStr("1");
    

    // Map both RefSeq and Genbank assemblies with the same spec and id
    CGenomicCollectionsService GCService;
   
    {{
        CConstRef<CGC_Assembly> GenColl(
            GCService.GetAssembly("GCF_000001405.13",
                                  CGCClient_GetAssemblyRequest::eLevel_scaffold
                                 )
        );
       
        // Do a Map
        CGencollIdMapper Mapper(GenColl);
        CConstRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
        // Check that Map results meet expectations
        BOOST_CHECK_EQUAL(Result->GetId()->GetGi(), 224589800);
    }}


    {{
        CConstRef<CGC_Assembly> GenColl(
            GCService.GetAssembly("GCA_000001405.1",
                                  CGCClient_GetAssemblyRequest::eLevel_scaffold
                                 )
        );

        // Do a Map
        CGencollIdMapper Mapper(GenColl);
        CConstRef<CSeq_loc> Result = Mapper.Map(*OrigLoc, MapSpec);
        // Check that Map results meet expectations
        BOOST_CHECK_EQUAL(Result->GetId()->GetGi(), 224384768);
    }}

}


// primary guess test
BOOST_AUTO_TEST_CASE(TestCaseSpecPrimaryGuess)
{
    // Simple ID
    CRef<CSeq_loc> OrigLoc(new CSeq_loc());
    OrigLoc->SetWhole().SetGi(224589800);
    

    // Guess both RefSeq and Genbank assemblies with the same spec and id
    CGenomicCollectionsService GCService;
   
    {{
        CConstRef<CGC_Assembly> GenColl(
            GCService.GetAssembly("GCF_000001405.13",
                                  CGCClient_GetAssemblyRequest::eLevel_scaffold
                                 )
        );
       
        // Do a Map
        CGencollIdMapper Mapper(GenColl);
        CGencollIdMapper::SIdSpec GuessSpec;
        bool Result = Mapper.Guess(*OrigLoc, GuessSpec);
        // Check that Guess results meet expectations
        BOOST_CHECK_EQUAL(Result, true);
        BOOST_CHECK_EQUAL(GuessSpec.ToString(), "Prim:RefSeq:Gi:::CHRO");
    }}


    {{
        CConstRef<CGC_Assembly> GenColl(
            GCService.GetAssembly("GCA_000001405.1",
                                  CGCClient_GetAssemblyRequest::eLevel_scaffold
                                 )
        );

        // Do a Map
        CGencollIdMapper Mapper(GenColl);
        CGencollIdMapper::SIdSpec GuessSpec;
        bool Result = Mapper.Guess(*OrigLoc, GuessSpec);
        // Check that Map results meet expectations
        BOOST_CHECK_EQUAL(Result, true);
        BOOST_CHECK_EQUAL(GuessSpec.ToString(), "NotPrim:RefSeq:Gi:::CHRO");
    }}

}





BOOST_AUTO_TEST_SUITE_END();

