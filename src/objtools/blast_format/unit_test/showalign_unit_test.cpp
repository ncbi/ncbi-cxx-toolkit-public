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
// Use this defines to run this test in isolation
//#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#include <corelib/ncbifile.hpp>

#include <corelib/ncbistl.hpp>
#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/blast_format/showalign.hpp>


#include "blast_test_util.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);
using namespace TestUtil;


#ifndef _DEBUG  // Don't run this in debug mode
BOOST_AUTO_TEST_CASE(TestPerformance)
{
    const string seqAlignFileName_in = "data/in_showalign_aln";
    const time_t kTimeMax = getenv("UNIT_TEST_TIMEOUT")
        ? atoi(getenv("UNIT_TEST_TIMEOUT")) : 30;
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     
  
    CBlastScopeSource scope_src(false);
    CRef<CScope> scope(scope_src.NewScope());
    time_t start_time = CTime(CTime::eCurrent).GetTimeT();
    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    CNcbiOfstream dumpster("/dev/null");  // we don't care about the output
    ds.DisplaySeqalign(dumpster);
    time_t end_time = CTime(CTime::eCurrent).GetTimeT();  ostringstream os;
    os << "formatting took " 
       << end_time-start_time << " seconds, i.e.: more than the "
       << kTimeMax << " second timeout";
    BOOST_REQUIRE_MESSAGE((end_time-start_time) < kTimeMax, os.str());

}
#endif

bool TestSimpleAlignment(CBlastOM::ELocation location)
{
    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);
  
    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();
    
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);  
    fileSeqAlignSet->Set() = san->GetData().GetAlign();     

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, location);
    {{  // to limit the scope of the objects declared in this block
    CRef<CScope> scope = tmp_data_loader.NewScope();

    CDisplaySeqalign ds(*fileSeqAlignSet, *scope);
    ds.SetDbName(kDbName);
    ds.SetDbType((kDbType == CBlastDbDataLoader::eNucleotide));
    int flags = CDisplaySeqalign::eShowBlastInfo |
        CDisplaySeqalign::eShowGi | 
        CDisplaySeqalign::eShowBlastStyleId;
    ds.SetAlignOption(flags);
    ds.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    ostrstream output_stream;
    ds.DisplaySeqalign(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111 Escherichia "
                              "coli K-12 MG1655 section 1 of 400 o") != NPOS);
    BOOST_REQUIRE(output.find("Sbjct  259   GCCTGATGCGACGCTGGCGCGTCTTATCAGGCCTAC  294") != NPOS);
    BOOST_REQUIRE(output.find("Length=11852") != NPOS);
    BOOST_REQUIRE(output.find("Query  5636  GTAGG-CAGGATAAGGCGTTCACGCCGCATCCGGCA  5670") != NPOS);
    BOOST_REQUIRE(output.find(" Score = 54.7 bits (29),  Expect = 2e-06") 
                  != NPOS);
    }}
    tmp_data_loader.RevokeBlastDbDataLoader();
    return true;
}

BOOST_AUTO_TEST_CASE(TestSimpleAlignment_LocalBlastDBLoader)
{
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eLocal));
}

BOOST_AUTO_TEST_CASE(TestSimpleAlignment_RmtBlastDBLoader)
{
    BOOST_REQUIRE(TestSimpleAlignment(CBlastOM::eRemote));
}

