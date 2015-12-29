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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   Unit tests for the sequence writting API for BLAST database apps
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/seq_writer.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <corelib/ncbifile.hpp>
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>

USING_NCBI_SCOPE;

BOOST_AUTO_TEST_SUITE(seq_writer)

static const int kConvFlags = 
    NStr::fConvErr_NoThrow |
    NStr::fAllowLeadingSymbols | 
    NStr::fAllowTrailingSymbols;

BOOST_AUTO_TEST_CASE(TestNoFormatSpecifier)
{
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    const string format_spec("hello world!");
    BOOST_REQUIRE_THROW(CSeqFormatter f(format_spec, db, std::cout),
                        CInvalidDataException);
}

BOOST_AUTO_TEST_CASE(TestInvalidFormatSpecifiers_Empty)
{
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    BOOST_REQUIRE_THROW(CSeqFormatter f(kEmptyStr, db, std::cout),
                        CInvalidDataException);
}

BOOST_AUTO_TEST_CASE(TestInvalidFormatSpecifiers_InvalidAlgoId)
{
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    CSeqFormatterConfig config;
    config.m_FiltAlgoId = 15;
    BOOST_REQUIRE_THROW(CSeqFormatter f("%m", db, std::cout, config), 
                        CInvalidDataException);
}

BOOST_AUTO_TEST_CASE(TestValidFormatSpecifiers)
{
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    CSeqFormatterConfig config;

    vector<string> format_specs;
    format_specs.push_back("%o print %% sign");   // escape '%'
    format_specs.push_back("%s");
    format_specs.push_back("%a");
    format_specs.push_back("%g");
    format_specs.push_back("%o");
    format_specs.push_back("%t");
    format_specs.push_back("%l");
    format_specs.push_back("%T");
    format_specs.push_back("%P");
    format_specs.push_back("%m"); // should print all masks for algo ID 20

    config.m_FiltAlgoId = 20;
    ITERATE(vector<string>, itr, format_specs) {
        CSeqFormatter f(*itr, db, std::cout, config);
    }

    config.m_FiltAlgoId = 40;
    CSeqFormatter f("%m", db, std::cout, config);
}

BOOST_AUTO_TEST_CASE(TestRequestGiOidLength)
{
    const int kGi(43123516);
    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    CSeqDB db("data/seqp", CSeqDB::eProtein);
    const string format_spec("The GI %g has oid %o and length %l, 10%%");
    ofstream out(fname.c_str());
    CSeqFormatter f(format_spec, db, out);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Split(string(buffer), " ", tokens, NStr::fSplit_NoMergeDelims);

    int gi = NStr::StringToInt(tokens[2], kConvFlags);
    int oid = NStr::StringToInt(tokens[5], kConvFlags);
    int length = NStr::StringToInt(tokens[8], kConvFlags);
    string perc = tokens[9];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(oid, 99);
    BOOST_REQUIRE_EQUAL(length, 99);
    BOOST_REQUIRE_EQUAL(perc, string("10%"));
}

BOOST_AUTO_TEST_CASE(TestRequestSeqId)
{
    const int kGi(43123516);
    const string kSeqId("gb|EAC27631.1|");
    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    CSeqDB db("data/seqp", CSeqDB::eProtein);
    const string format_spec("%i");
    ofstream out(fname.c_str());
    CSeqFormatter f(format_spec, db, out);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Tokenize(string(buffer), " ", tokens);

    BOOST_REQUIRE_EQUAL(tokens[0], kSeqId);
}


BOOST_AUTO_TEST_CASE(TestRequestAccessionPIGTaxidTitle)
{
    const int kGi(43167137);
    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    CSeqDB db("data/seqp", CSeqDB::eProtein);
    string format_spec("GI %g has accession '%a', PIG %P, taxid %T and ");
    format_spec += "title '%t'.";
    ofstream out(fname.c_str());
    CSeqFormatter f(format_spec, db, out);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Split(string(buffer), " ", tokens, NStr::fSplit_NoMergeDelims);

    int gi = NStr::StringToInt(tokens[1], kConvFlags);
    string accession = tokens[4];
    int pig = NStr::StringToInt(tokens[6], kConvFlags);
    int taxid = NStr::StringToInt(tokens[8], kConvFlags);
    string title = tokens[10];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(pig, -1);   // unassigned PIG in database :(
    BOOST_REQUIRE_EQUAL(taxid, 0);
}

BOOST_AUTO_TEST_CASE(TestRequestGiGivenPIG)
{
    const int kPig(1067150);
    const int kGi(129295);
    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    CSeqDB db("nr", CSeqDB::eProtein);
    const string format_spec("%g");
    ofstream out(fname.c_str());
    CSeqFormatter f(format_spec, db, out);
    CBlastDBSeqId id(kPig);
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    const int gi_read = NStr::StringToInt(buffer, kConvFlags);
    BOOST_REQUIRE_EQUAL(kGi, gi_read);
}

BOOST_AUTO_TEST_CASE(TestRequestSequenceDataLength)
{
    const int kGi(43167137);
    const int kLength = 101;
    const string
        kSeqData("MKALVYVGEEKLDFREVADPVVNSGEQLIKVDSAGICGSDMHAYLGHDERRPAPLILGHEASGVILGGDRNGERVAINPLVTCMKCQACNSRRENICSKRQ");

    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    CSeqDB db("data/seqp", CSeqDB::eProtein);
    const string format_spec("%g|%l|%s");
    ofstream out(fname.c_str());
    CSeqFormatter f(format_spec, db, out);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Split(string(buffer), "|", tokens, NStr::fSplit_NoMergeDelims);

    int gi = NStr::StringToInt(tokens[0], kConvFlags);
    int len = NStr::StringToInt(tokens[1], kConvFlags);
    string seq_data = tokens[2];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(len, kLength);
    BOOST_REQUIRE_EQUAL(kSeqData, seq_data);
}

BOOST_AUTO_TEST_CASE(TestMaskedFasta)
{
    const int kGi(15597722);
    const TSeqPos kLength = 1036;

    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    const string format_spec("%f");
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    ofstream out(fname.c_str());
    CSeqFormatterConfig config;
    config.m_SeqRange = TSeqRange(0, 100);
    config.m_LineWidth = config.m_SeqRange.GetLength();
    config.m_FiltAlgoId = 40;
    //config.m_FiltAlgoIds.push_back(40);
    CSeqFormatter f(format_spec, db, out, config);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[kLength+1] = { '\0' };
    while (in.getline(buffer, sizeof(buffer))) {
        if (NStr::StartsWith(buffer, "MSLS")) {
            // found the sequence data
            break;
        }
    }

    const TSeqRange masked_range(22,72);
    for (TSeqPos i = 0; i < masked_range.GetFrom(); i++) {
        BOOST_REQUIRE(isupper(buffer[i]));
    }
    for (TSeqPos i = masked_range.GetFrom(); i < masked_range.GetTo(); i++) {
        BOOST_REQUIRE(islower(buffer[i]));
    }
    for (TSeqPos i = masked_range.GetTo()+1; i < config.m_SeqRange.GetTo(); i++)
    {
        ostringstream os;
        os << "Failed at position " << i << ": " << buffer[i];
        BOOST_REQUIRE_MESSAGE(isupper(buffer[i]), os.str());
    }
}

BOOST_AUTO_TEST_CASE(TestMaskedSequenceData)
{
    const int kGi(15597722);
    const TSeqPos kLength = 1036;

    CTmpFile tmpfile;
    const string& fname = tmpfile.GetFileName();
    const string format_spec("%s");
    CSeqDB db("data/mask-data-db", CSeqDB::eProtein);
    ofstream out(fname.c_str());
    CSeqFormatterConfig config;
    config.m_FiltAlgoId = 40;
    //config.m_FiltAlgoIds.push_back(40);
    CSeqFormatter f(format_spec, db, out, config);
    CBlastDBSeqId id(NStr::IntToString(kGi));
    f.Write(id);
    out.close();

    ifstream in(fname.c_str());
    char buffer[kLength+1] = { '\0' };
    in.getline(buffer, sizeof(buffer));     // read the sequence data

    const TSeqRange masked_range(22,72);
    for (TSeqPos i = 0; i < masked_range.GetFrom(); i++) {
        BOOST_REQUIRE(isupper(buffer[i]));
    }
    for (TSeqPos i = masked_range.GetFrom(); i < masked_range.GetTo(); i++) {
        BOOST_REQUIRE(islower(buffer[i]));
    }
    for (TSeqPos i = masked_range.GetTo(); i < kLength; i++) {
        BOOST_REQUIRE(isupper(buffer[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END();
