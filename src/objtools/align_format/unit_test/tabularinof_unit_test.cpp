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
*   Unit test module to test CBlastTabularInfo
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/tabular.hpp>

#include "blast_test_util.hpp"
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::align_format;
using namespace TestUtil;

BOOST_AUTO_TEST_SUITE(tabularinfo)

BOOST_AUTO_TEST_CASE(StandardOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    ostrstream output_stream;
    CBlastTabularInfo ctab(output_stream);
    // CBlastTabularInfo ctab(cout);

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }
    ctab.PrintNumProcessed(1);

    string output = CNcbiOstrstreamToString(output_stream);

    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1786181|gb|AE000111.1|AE000111	100.00	10596	0	0	1	10596") != NPOS);
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1787084|gb|AE000188.1|AE000188	97.06	34	1	0	5567	5600	1088") != NPOS);
    BOOST_REQUIRE(output.find("# BLAST processed 1 queries") != NPOS);
}

BOOST_AUTO_TEST_CASE(QuerySubjectScoreOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    ostrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qseqid sseqid bitscore");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);

    // First result should be "gi|1786181|gb|AE000111.1|AE000111        gi|1786181|gb|AE000111.1|AE000111       1.957e+04".
    // but some windows binaires print the bit score as 1.957e+004.  Hence, we drop the exponent.
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1786181|gb|AE000111.1|AE000111	1.957e+") != NPOS);
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1788899|gb|AE000341.1|AE000341	91.6") != NPOS);
}

BOOST_AUTO_TEST_CASE(QueryAccSubjectAccIdentScoreOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    ostrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qacc sacc pident bitscore");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);

    // First result should be "gi|1786181|gb|AE000111.1|AE000111        gi|1786181|gb|AE000111.1|AE000111       1.957e+04".
    // but some windows binaires print the bit score as 1.957e+004.  Hence, we drop the exponent.
    BOOST_REQUIRE(output.find("AE000111	AE000111	100.00	1.957e+") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000310	94.59	56.5") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000509	80.51	76.8") != NPOS);
}

BOOST_AUTO_TEST_CASE(QueryAccSubjectAccIdentBTOPOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    ostrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qacc sacc score pident btop");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);
    
    BOOST_REQUIRE(output.find("AE000111	AE000111	10596	100.00	10596") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000447	57	89.41	15T-TA2TA2-T6AG8CT1GT4CA10AC28") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000116	48	98.04	12GA38") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000183	36	82.02	14G-TATGAG2-A3-G4-C1A-4-AGA3C-6CATATC7C-1-C28") != NPOS);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================

* ===========================================================================
*/

