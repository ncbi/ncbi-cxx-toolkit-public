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
 * Author:  Jian Ye
 *
 * File Description:
 *   Unit tests for showalign
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <corelib/ncbistl.hpp>
#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/align_format/showalign.hpp>


#include "blast_test_util.hpp"
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_format);
using namespace TestUtil;

BOOST_AUTO_TEST_SUITE(showalign)

BOOST_AUTO_TEST_CASE(TestPerformance)
{
    CTmpFile tmp_file;
    const string seqAlignFileName_in = "data/in_showalign_aln";
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     
  
    const string kDbName("nucl_dbs");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    CNcbiOfstream dumpster(tmp_file.GetFileName());  // we don't care about the output
    ds.DisplaySeqalign(dumpster);
    scope->GetObjectManager().RevokeAllDataLoaders();        
}
#ifdef _DEBUG
const int kPerformanceTimeout = 120;
#else
const int kPerformanceTimeout = 30;
#endif
BOOST_AUTO_TEST_CASE_TIMEOUT(TestPerformance, kPerformanceTimeout);

BOOST_AUTO_TEST_CASE(TestAccConversion)
{
    const string seqAlignFileName_in = "data/in_showalign_use_this_gi.asn";
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     

    const string kDbName("nr");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);      
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);        
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    ds.SetDbName(kDbName);
    ds.SetDbType((kDbType == CBlastDbDataLoader::eProtein));
    int flags = CDisplaySeqalign::eShowBlastInfo |
        CDisplaySeqalign::eShowBlastStyleId;
    ds.SetAlignOption(flags);
    ds.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    CNcbiOstrstream output_stream;
    ds.DisplaySeqalign(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);    
    BOOST_REQUIRE(output.find("XP_011512980.1") != NPOS);
    BOOST_REQUIRE(output.find("NP_004146.1") != NPOS);
    BOOST_REQUIRE(output.find("AAH96218.1") != NPOS);
    scope->GetObjectManager().RevokeAllDataLoaders();            
}

BOOST_AUTO_TEST_CASE(TestAccConversionExt)
{
    const string seqAlignFileName_in = "data/in_showalign_use_this_gi_ext.asn";
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     

    const string kDbName("nr");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);      
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);        
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    ds.SetDbName(kDbName);
    ds.SetDbType((kDbType == CBlastDbDataLoader::eProtein));
    int flags = CDisplaySeqalign::eShowBlastInfo |
        CDisplaySeqalign::eShowBlastStyleId;
    ds.SetAlignOption(flags);
    ds.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    CNcbiOstrstream output_stream;
    ds.DisplaySeqalign(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);
    BOOST_REQUIRE(output.find("NP_001263315.1") != NPOS);
    BOOST_REQUIRE(output.find("XP_015137661.1") != NPOS);
    BOOST_REQUIRE(output.find("AUD54591.1") != NPOS);
    BOOST_REQUIRE(output.find("BAM13279.1") != NPOS);
    BOOST_REQUIRE(output.find("P01013.1") != NPOS);
    scope->GetObjectManager().RevokeAllDataLoaders();                
}

bool TestSimpleAlignment(CBlastOM::ELocation location, bool long_seqids)
{
    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     

    const string kDbName("nt");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, location);
    {{  // to limit the scope of the objects declared in this block
    CRef<CScope> scope = tmp_data_loader.NewScope();

    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    ds.SetDbName(kDbName);
    ds.SetDbType((kDbType == CBlastDbDataLoader::eNucleotide));
    int flags = CDisplaySeqalign::eShowBlastInfo |
        CDisplaySeqalign::eShowBlastStyleId;
    if (long_seqids) {
        flags |= CDisplaySeqalign::eShowGi;
        ds.UseLongSequenceIds();
    }
    ds.SetAlignOption(flags);
    ds.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    CNcbiOstrstream output_stream;
    ds.DisplaySeqalign(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);    
    if (!long_seqids) {
        BOOST_REQUIRE(output.find(">AE000304.1 ") != NPOS);
    }
    else {
        BOOST_REQUIRE(output.find(">gi|1788470|gb|AE000304.1|AE000304 ") != NPOS);
    }
    BOOST_REQUIRE(output.find("Escherichia coli K-12 MG1655 section 9 of 400 of ") != NPOS ||
                  output.find("Escherichia coli K12 MG1655 section 9 of 400 of ") != NPOS ||
                  output.find("Escherichia coli K12 substr. MG1655") != NPOS);
    BOOST_REQUIRE(output.find("Sbjct  259   GCCTGATGCGACGCTGGCGCGTCTTATCAGGCCTAC  294") != NPOS);
    BOOST_REQUIRE(output.find("Length=11852") != NPOS);
    BOOST_REQUIRE(output.find("Query  5636  GTAGG-CAGGATAAGGCGTTCACGCCGCATCCGGCA  5670") != NPOS);
    BOOST_REQUIRE(output.find(" Score = 54.7 bits (29),  Expect = 2e-0") 
                  != NPOS);    
    }}
    tmp_data_loader.RevokeBlastDbDataLoader();            
    return true;
}


// Note: this essentially disables the performance tests for the automated
// toolkit builds, which implies that the BLAST team should be running these
// themselves (CVSROOT/individual/camacho/scripts/autobuild.pl should do this)
NCBITEST_AUTO_INIT()
{
    if (CNcbiApplication::Instance()->GetEnvironment()
        .Get("NCBI_AUTOMATED_BUILD") == "1") {
        // Suppress timeout
        typedef SNcbiTestTCTimeout<BOOST_AUTO_TC_UNIQUE_ID(TestPerformance)> TTimeout;
        static TTimeout new_timeout(kMax_Int);
    }
}

BOOST_AUTO_TEST_CASE(TestSimpleAlignment_LocalBlastDBLoader)
{
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eLocal, false));
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eLocal, true));
}

BOOST_AUTO_TEST_CASE(TestSimpleAlignment_RmtBlastDBLoader)
{
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eRemote, false));
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eRemote, true));
}

#ifndef NCBI_INT4_GI
BOOST_AUTO_TEST_CASE(TestLargeGi)
{
    const string seqAlignFileName_in = "data/large_gi.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);
    fileSeqAlignSet->Set() = san->GetData().GetAlign();

    const string kDbName("data/nucl_32b_gi");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();

    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    ds.SetDbName(kDbName);
    ds.SetDbType((kDbType == CBlastDbDataLoader::eNucleotide));
    int flags = CDisplaySeqalign::eShowBlastInfo |
        CDisplaySeqalign::eShowBlastStyleId |
        CDisplaySeqalign::eShowGi;
    ds.SetAlignOption(flags);
    ds.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    CNcbiOstrstream output_stream;
    ds.DisplaySeqalign(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);
    BOOST_REQUIRE(output.find("2271278971") != NPOS);
    BOOST_REQUIRE(output.find("698036805") != NPOS);
    BOOST_REQUIRE(output.find("4294967295") != NPOS);
    BOOST_REQUIRE(output.find("2966748774") != NPOS);
    scope->GetObjectManager().RevokeAllDataLoaders();
}
#endif


BOOST_AUTO_TEST_SUITE_END()
