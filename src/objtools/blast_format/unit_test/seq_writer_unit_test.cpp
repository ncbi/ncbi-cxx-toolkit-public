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
#include <boost/test/auto_unit_test.hpp>
#include <objtools/blast_format/seq_writer.hpp>
#include <corelib/ncbifile.hpp>

USING_NCBI_SCOPE;

static const int kConvFlags = 
    NStr::fConvErr_NoThrow |
    NStr::fAllowLeadingSymbols | 
    NStr::fAllowTrailingSymbols;

BOOST_AUTO_TEST_CASE(TestValidFormatSpecifiers)
{
    CSeqDB db("data/seqp", CSeqDB::eProtein);

    vector<string> format_specs;
    format_specs.push_back("print %% sign");   // escape '%'
    format_specs.push_back("%s");
    format_specs.push_back("%a");
    format_specs.push_back("%g");
    format_specs.push_back("%o");
    format_specs.push_back("%t");
    format_specs.push_back("%l");
    format_specs.push_back("%T");
    format_specs.push_back("%P");

    ITERATE(vector<string>, itr, format_specs) {
        CSeqFormatter f(*itr, db, std::cout);
    }
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
    bool rv = f.Write(id);
    out.close();
    BOOST_REQUIRE(rv);

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Tokenize(string(buffer), " ", tokens);

    int gi = NStr::StringToInt(tokens[2], kConvFlags);
    int oid = NStr::StringToInt(tokens[5], kConvFlags);
    int length = NStr::StringToInt(tokens[8], kConvFlags);
    string perc = tokens[9];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(oid, 99);
    BOOST_REQUIRE_EQUAL(length, 99);
    BOOST_REQUIRE_EQUAL(perc, "10%");
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
    bool rv = f.Write(id);
    out.close();
    BOOST_REQUIRE(rv);

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Tokenize(string(buffer), " ", tokens);

    int gi = NStr::StringToInt(tokens[1], kConvFlags);
    string accession = tokens[4];
    int pig = NStr::StringToInt(tokens[6], kConvFlags);
    int taxid = NStr::StringToInt(tokens[8], kConvFlags);
    string title = tokens[10];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(pig, -1);   // unassigned PIG in database :(
    BOOST_REQUIRE_EQUAL(taxid, 0);
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
    bool rv = f.Write(id);
    out.close();
    BOOST_REQUIRE(rv);

    ifstream in(fname.c_str());
    char buffer[256] = { '\0' };
    in.getline(buffer, sizeof(buffer));

    vector<string> tokens;
    NStr::Tokenize(string(buffer), "|", tokens);

    int gi = NStr::StringToInt(tokens[0], kConvFlags);
    int len = NStr::StringToInt(tokens[1], kConvFlags);
    string seq_data = tokens[2];
    BOOST_REQUIRE_EQUAL(gi, kGi);
    BOOST_REQUIRE_EQUAL(len, kLength);
    BOOST_REQUIRE_EQUAL(kSeqData, seq_data);
}
