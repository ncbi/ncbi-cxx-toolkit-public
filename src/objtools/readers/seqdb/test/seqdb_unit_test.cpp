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
 * Authors:  Kevin Bealer
 *
 * File Description:
 *   Unit test.
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdbexpert.hpp>
#include <serial/serialbase.hpp>
#include <objects/seq/seq__.hpp>
#include <corelib/ncbifile.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objmgr/util/sequence.hpp>
#include <math.h>

#include <util/sequtil/sequtil_convert.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#include <boost/current_function.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h> // Must be last include directive.

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using boost::unit_test::test_suite;

// Use macros rather than inline functions to get accurate line number reports

#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_ERROR("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_ERROR("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr) \
    CHECK_NO_THROW(BOOST_CHECK(expr))

#define CHECK_EQUAL(x, y) \
    CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))

#define CHECK_EQUAL_TYPED(T, x, y) \
    CHECK_EQUAL(T(x), T(y))

#define CHECK_EQUAL_MESSAGE(M, x, y) \
    CHECK_NO_THROW(BOOST_CHECK_MESSAGE((x) == (y), M))

#define CHECK_EQUAL_MESSAGE_PRINT(M, x, y) \
    CHECK_EQUAL_MESSAGE(s_ToString(M, ": ", x, " != ", y), x, y)

#define CHECK_THROW(s, x) \
    BOOST_CHECK_THROW(s, x)

#define CHECK_THROW_SEQDB(A) CHECK_THROW(A, CSeqDBException)

static void s_UnitTestVerbosity(string s, int ln)
{
    static bool enabled = static_cast<bool>(getenv("VERBOSE_UT") != NULL);
    
    if (enabled) {
        cout << "Running test: " << s << " @ " << ln << endl;
    }
}

#define UNIT_TEST_FILTER(s) \
{ \
    string nm(s); \
    string filter(getenv("FILTER_UT") ? getenv("FILTER_UT") : ""); \
    if (filter == "") filter = (getenv("UT_FILTER") ? getenv("UT_FILTER") : ""); \
    \
    if (nm.size() && filter.size() && nm.find(filter) == string::npos) { \
        cout << "Skipping test: " << s << endl; \
        return; \
    } \
}

#define START UNIT_TEST_FILTER(BOOST_CURRENT_FUNCTION); \
              s_UnitTestVerbosity(BOOST_CURRENT_FUNCTION, __LINE__)


// Helper functions

template<class A, class B, class C, class D, class E>
string s_ToString(const A & a, const B & b, const C & c, const D & d, const E & e)
{
    ostringstream oss;
    oss << a << b << c << d << e;
    return oss.str();
}

enum EMaskingType {
    eAll, eOdd, eEven, ePrime, eEND
};

static void s_TestPartialAmbigRange(CSeqDB & db, int oid, int begin, int end)
{
    const char * slice  (0);
    int          sliceL (0);
    const char * whole (0);
    int          wholeL(0);
    
    sliceL = db.GetAmbigSeq(oid, & slice, kSeqDBNuclNcbiNA8, begin, end);
    wholeL = db.GetAmbigSeq(oid, & whole, kSeqDBNuclNcbiNA8);
    
    string op =
        s_ToString("Checking NcbiNA8 subsequence range [", begin, ",", end, "].");
    
    CHECK_EQUAL_MESSAGE(op, 0, memcmp(slice, whole + begin, sliceL));
    
    db.RetSequence(& whole);
    db.RetSequence(& slice);
}

static void s_TestPartialAmbig(CSeqDB & db, int nt_gi)
{
    int oid(-1);
    bool success = db.GiToOid(nt_gi, oid);
        
    CHECK(success);
        
    int length = db.GetSeqLength(oid);
        
    s_TestPartialAmbigRange(db, oid, 0, length);
    s_TestPartialAmbigRange(db, oid, 0, length/2);
    s_TestPartialAmbigRange(db, oid, length/2, length);
        
    for(int i = 1; i<length; i *= 2) {
        for(int j = 0; j<length; j += i) {
            int endpt = j + i;
            if (endpt > length)
                endpt = length;
                
            s_TestPartialAmbigRange(db, oid, j, endpt);
        }
    }
}

static bool s_MaskingTest(EMaskingType mask, unsigned oid)
{
    switch(mask) {
    case eAll:
        return true;
            
    case eOdd:
        return (oid & 1) != 0;
            
    case eEven:
        return (oid & 1) == 0;
            
    case ePrime:
            
        switch(oid) {
        case 0:
        case 1:
            return false;
                
        case 2:
            return true;
                
        default:
            for(unsigned d = 2; d < oid; d++) {
                if ((oid % d) == 0)
                    return false;
            }
        }
        return true;
            
    default:
        break;
    }
        
    CHECK(0);
        
    return false;
}

static void s_TestMaskingLimits(EMaskingType mask,
                                unsigned     first,
                                unsigned     last,
                                unsigned     lowest,
                                unsigned     highest,
                                unsigned     count)
{
        
    if (first > last) {
        return;
    }
    
    while((first <= last) && (! s_MaskingTest(mask, first))) {
        first++;
        
        if (first > last) {
            return;
        }
    }
    
    while((first <= last) && (! s_MaskingTest(mask, last))) {
        if (last > first) {
            last--;
        } else {
            return;
        }
    }
    
    CHECK(first <= last);
    
    unsigned exp_count(0);
    
    for(unsigned i=first; i<=last; i++) {
        if (s_MaskingTest(mask, i)) {
            exp_count++;
        }
    }
    
    CHECK_EQUAL(first, lowest);
    CHECK_EQUAL(last,  highest);
    CHECK_EQUAL(count, exp_count);
}

template<class NUM, class DIF>
void
s_ApproxEqual(NUM a, NUM b, DIF epsilon, int lineno)
{
        
    if (! ((a <= (b + epsilon)) &&
           (b <= (a + epsilon))) ) {
        
        cout << "\nMismatch: line " << lineno
             << " a " << a
             << " b " << b
             << " eps " << epsilon << endl;
        
        CHECK( a <= (b + epsilon) );
        CHECK( b <= (a + epsilon) );
    }
}

static Uint4 s_BufHash(const char * buf_in, Uint4 length, Uint4 start = 1)
{
    const signed char * buf = (const signed char *) buf_in;
    Uint4 hash = start;
    Uint4 i    = 0;
    
    while(i < length) {
        if (i & 1) {
            hash += buf[i++];
        } else {
            hash ^= buf[i++];
        }
        hash = ((hash << 13) | (hash >> 19)) + length;
    }
    
    return hash;
}

template<class ASNOBJ>
string s_Stringify(CRef<ASNOBJ> a)
{
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *a;
    return CNcbiOstrstreamToString(oss);
}

/// RIAA class for GetSequence/RetSequence.
class CReturnSeqBuffer {
public:
    /// Constructor, remembers a sequence buffer.
    CReturnSeqBuffer(CSeqDB     * seqdb,
                     const char * buffer)
        : m_SeqDB  (seqdb),
          m_Buffer (buffer)
    {
    }
    
    /// Destructor, frees the sequence buffer.
    ~CReturnSeqBuffer()
    {
        if (m_Buffer) {
            m_SeqDB->RetSequence(& m_Buffer);
        }
    }
    
private:
    CSeqDB     * m_SeqDB;
    const char * m_Buffer;
};


// Test Cases

BOOST_AUTO_TEST_CASE(ConstructLocal)
{
    START;
    
    // Test both constructors; make sure sizes are equal and non-zero.
    
    CSeqDB local1("data/seqp", CSeqDB::eProtein);
    CSeqDB local2("data/seqp", CSeqDB::eProtein, 0, 0, false);
    
    Int4 num1(0), num2(0);
    local1.GetTotals(CSeqDB::eFilteredAll, & num1, 0);
    local2.GetTotals(CSeqDB::eFilteredAll, & num2, 0);
    
    CHECK(num1 >= 1);
    CHECK_EQUAL(num1, num2);
}

BOOST_AUTO_TEST_CASE(PathDelimiters)
{
    START;
    
    // Test both constructors; make sure sizes are equal and non-zero.
    
    CSeqDB local1("data\\seqp", CSeqDB::eProtein);
    CSeqDB local2("data/seqp", CSeqDB::eProtein);
    
    Int4 num1(0), num2(0);
    local1.GetTotals(CSeqDB::eFilteredAll, & num1, 0);
    local2.GetTotals(CSeqDB::eFilteredAll, & num2, 0);
    
    CHECK(num1 >= 1);
    CHECK_EQUAL(num1, num2);
}

BOOST_AUTO_TEST_CASE(ConstructMissing)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB local1("data/spork", CSeqDB::eProtein);
        CSeqDB local2("data/spork", CSeqDB::eProtein, 0, 0, false);
        
        Int4 num1(0), num2(0);
        local1.GetTotals(CSeqDB::eFilteredAll, & num1, 0);
        local2.GetTotals(CSeqDB::eFilteredAll, & num2, 0);
        
        CHECK(num1 >= 1);
        CHECK_EQUAL(num1, num2);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("ConstructMissing() did not throw an exception of type  CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(InvalidSeqType)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB local1("data/seqp", CSeqDB::ESeqType(99));
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("InvalidSeqType() did not throw an exception of type    CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(ValidPath)
{
    START;
    
    CSeqDB local1("data/seqp", CSeqDB::eProtein);
    
    Int4 num1(0);
    local1.GetTotals(CSeqDB::eFilteredAll, & num1, 0);
    
    CHECK(num1 >= 1);
}

BOOST_AUTO_TEST_CASE(InvalidPath)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB local1("seqp", CSeqDB::eProtein);
        
        Int4 num1(0);
        local1.GetTotals(CSeqDB::eFilteredAll, & num1, 0);
        
        CHECK(num1 >= 1);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("InvalidPath() did not throw an exception of type  CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(SummaryDataN)
{
    START;
    
    string dbname;
    CSeqDB localN(dbname = "data/seqn", CSeqDB::eNucleotide);
    
    Int4 nseqs(0);
    Uint8 vlength(0);
    localN.GetTotals(CSeqDB::eUnfilteredAll, & nseqs, & vlength);
    
    CHECK_EQUAL(CSeqDB::eNucleotide, localN.GetSequenceType());
    CHECK_EQUAL(int(100),            nseqs);
    CHECK_EQUAL(Uint8(51718),        vlength);
    CHECK_EQUAL(Uint4(875),          Uint4(localN.GetMaxLength()));
    CHECK_EQUAL((string)dbname,      (string)localN.GetDBNameList());
    
    CHECK_EQUAL(string("Another test DB for CPPUNIT, SeqDB."),
                localN.GetTitle());
    
    Uint8 vol1(0);
    int seq1(0);
    localN.GetTotals(CSeqDB::eFilteredRange, & seq1, & vol1);
    
    localN.SetIterationRange(1,0);
    
    Uint8 vol2(0);
    int seq2(0);
    
    localN.GetTotals(CSeqDB::eFilteredRange, & seq2, & vol2);
    
    CHECK(vol2 < vol1);
    CHECK_EQUAL(seq2, seq1 - 1);
    
    localN.SetIterationRange(2,0);
    
    Uint8 vol3(0);
    int seq3(0);
    localN.GetTotals(CSeqDB::eFilteredRange, & seq3, & vol3);
    
    CHECK(vol3 < vol2);
    CHECK_EQUAL(seq3, seq2 - 1);
}

BOOST_AUTO_TEST_CASE(SummaryDataP)
{
    START;
    
    string dbname;
    CSeqDB localP(dbname = "data/seqp", CSeqDB::eProtein);
    
    Int4 nseqs(0), noids(0);
    Uint8 vlength(0), tlength(0);
    
    localP.GetTotals(CSeqDB::eUnfilteredAll, & nseqs, & vlength);
    localP.GetTotals(CSeqDB::eFilteredAll,   & noids, & tlength);
    
    CHECK_EQUAL(CSeqDB::eProtein, localP.GetSequenceType());
    CHECK_EQUAL(int(100),     nseqs);
    CHECK_EQUAL(int(100),     noids);
    CHECK_EQUAL(Uint8(26945), tlength);
    CHECK_EQUAL(Uint8(26945), vlength);
    CHECK_EQUAL(Uint4(1224),  Uint4(localP.GetMaxLength()));
    CHECK_EQUAL((string)dbname, (string)localP.GetDBNameList());
    
    CHECK_EQUAL(string("Test database for BLAST unit tests"),
                localP.GetTitle());
}

BOOST_AUTO_TEST_CASE(GetAmbigSeqAllocN)
{
    START;
    
    CSeqDB seqp("data/seqn", CSeqDB::eNucleotide);
    
    char * bufp_blst = 0;
    char * bufp_ncbi = 0;
    
    Uint4 length_blst =
        seqp.GetAmbigSeqAlloc(0,
                              & bufp_blst,
                              kSeqDBNuclBlastNA8,
                              eNew);
    
    Uint4 length_ncbi =
        seqp.GetAmbigSeqAlloc(0,
                              & bufp_ncbi,
                              kSeqDBNuclNcbiNA8,
                              eMalloc);
    
    Uint4 hashval_blst = s_BufHash(bufp_blst, length_blst);
    Uint4 hashval_ncbi = s_BufHash(bufp_ncbi, length_ncbi);
    
    delete[] bufp_blst;
    free(bufp_ncbi);
    
    CHECK_EQUAL(Uint4(30118382ul), hashval_blst);
    CHECK_EQUAL(Uint4(3084382219ul), hashval_ncbi);
}

BOOST_AUTO_TEST_CASE(GetAmbigSeqAllocP)
{
    START;
    
    CSeqDB seqp("data/seqp", CSeqDB::eProtein);
    
    char * bufp_blst = 0;
    char * bufp_ncbi = 0;
    
    Uint4 length_blst =
        seqp.GetAmbigSeqAlloc(0,
                              & bufp_blst,
                              kSeqDBNuclBlastNA8,
                              eNew);
    
    Uint4 length_ncbi =
        seqp.GetAmbigSeqAlloc(0,
                              & bufp_ncbi,
                              kSeqDBNuclNcbiNA8,
                              eMalloc);
    
    Uint4 hashval_blst = s_BufHash(bufp_blst, length_blst);
    Uint4 hashval_ncbi = s_BufHash(bufp_ncbi, length_ncbi);
    
    delete [] bufp_blst;
    free( bufp_ncbi );
    
    CHECK_EQUAL(Uint4(3219499033ul), hashval_blst);
    CHECK_EQUAL(Uint4(3219499033ul), hashval_ncbi);
}

BOOST_AUTO_TEST_CASE(GetAmbigSeqN)
{
    START;
    
    CSeqDB seqp("data/seqn", CSeqDB::eNucleotide);
    
    const char * bufp1 = 0;
    const char * bufp2 = 0;
    Uint4 length1 = seqp.GetAmbigSeq(0, & bufp1, kSeqDBNuclBlastNA8);
    Uint4 length2 = seqp.GetAmbigSeq(0, & bufp2, kSeqDBNuclNcbiNA8);
    
    Uint4 hashval1 = s_BufHash(bufp1, length1);
    Uint4 hashval2 = s_BufHash(bufp2, length2);
    
    seqp.RetSequence(& bufp1);
    seqp.RetSequence(& bufp2);
    
    CHECK_EQUAL(Uint4(30118382ul), hashval1);
    CHECK_EQUAL(Uint4(3084382219ul), hashval2);
}

BOOST_AUTO_TEST_CASE(GetAmbigSeqP)
{
    START;
    
    CSeqDB seqp("data/seqp", CSeqDB::eProtein);
    
    const char * bufp1 = 0;
    const char * bufp2 = 0;
    Uint4 length1 = seqp.GetAmbigSeq(0, & bufp1, kSeqDBNuclBlastNA8);
    Uint4 length2 = seqp.GetAmbigSeq(0, & bufp2, kSeqDBNuclNcbiNA8);
    
    Uint4 hashval1 = s_BufHash(bufp1, length1);
    Uint4 hashval2 = s_BufHash(bufp2, length2);
    
    seqp.RetSequence(& bufp1);
    seqp.RetSequence(& bufp2);
    
    CHECK_EQUAL(Uint4(3219499033ul), hashval1);
    CHECK_EQUAL(Uint4(3219499033ul), hashval2);
}

BOOST_AUTO_TEST_CASE(GetBioseqN)
{
    START;
    
    string got( s_Stringify(CSeqDB("data/seqn", CSeqDB::eNucleotide).GetBioseq(2)) );
    
    string expected("Bioseq ::= {\n"
                    "  id {\n"
                    "    gi 46071107,\n"
                    "    ddbj {\n"
                    "      accession \"BP722514\",\n"
                    "      version 1\n"
                    "    }\n"
                    "  },\n"
                    "  descr {\n"
                    "    title \"Xenopus laevis NBRP cDNA clone:XL452f07ex, 3' end\",\n"
                    "    user {\n"
                    "      type str \"ASN1_BlastDefLine\",\n"
                    "      data {\n"
                    "        {\n"
                    "          label str \"ASN1_BlastDefLine\",\n"
                    "          num 1,\n"
                    "          data oss {\n"
                    "            '30803080A0801A3158656E6F707573206C6165766973204E4252502063444E4120\n"
                    "636C6F6E653A584C34353266303765782C20332720656E640000A1803080AB80020402BEFD4300\n"
                    "00AC803080A1801A0842503732323531340000A38002010100000000000000000000A280020100\n"
                    "000000000000'H\n"
                    "          }\n"
                    "        }\n"
                    "      }\n"
                    "    }\n"
                    "  },\n"
                    "  inst {\n"
                    "    repr raw,\n"
                    "    mol na,\n"
                    "    length 165,\n"
                    "    seq-data ncbi4na '11428288218841844814141422818811214421121482118428221114\n"
                    "82211121141881228484211141128842148481121112222F882124422141148188842112118488\n"
                    "41114822882844214144144148281181'H\n"
                    "  }\n"
                    "}\n");
    
    CHECK_EQUAL(expected, got);
}

BOOST_AUTO_TEST_CASE(GetBioseqP)
{
    START;
    
    string got( s_Stringify(CSeqDB("data/seqp", CSeqDB::eProtein).GetBioseq(2)) );
    
    string expected("Bioseq ::= {\n"
                    "  id {\n"
                    "    gi 44268131,\n"
                    "    genbank {\n"
                    "      accession \"EAI08555\",\n"
                    "      version 1\n"
                    "    }\n"
                    "  },\n"
                    "  descr {\n"
                    "    title \"unknown [environmental sequence]\",\n"
                    "    user {\n"
                    "      type str \"ASN1_BlastDefLine\",\n"
                    "      data {\n"
                    "        {\n"
                    "          label str \"ASN1_BlastDefLine\",\n"
                    "          num 1,\n"
                    "          data oss {\n"
                    "            '30803080A0801A20756E6B6E6F776E205B656E7669726F6E6D656E74616C207365\n"
                    "7175656E63655D0000A1803080AB80020402A37A630000A4803080A1801A084541493038353535\n"
                    "0000A38002010100000000000000000000A280020100000000000000'H\n"
                    "          }\n"
                    "        }\n"
                    "      }\n"
                    "    }\n"
                    "  },\n"
                    "  inst {\n"
                    "    repr raw,\n"
                    "    mol aa,\n"
                    "    length 64,\n"
                    "    seq-data ncbistdaa '0C0A0A0606090B0909060909060B09060909131004160F090A0A0A\n"
                    "0A0B0A0B0D0D010B0D110D0606090F12090D0A0B0904050D0A160D0B0B05051009100B1005'H\n"
                    "  }\n"
                    "}\n");
    
    CHECK_EQUAL(expected, got);
}

BOOST_AUTO_TEST_CASE(GetHdrN)
{
    START;
    
    string got( s_Stringify(CSeqDB("data/seqn", CSeqDB::eNucleotide).GetHdr(0)) );
    
    string expected = ("Blast-def-line-set ::= {\n"
                       "  {\n"
                       "    title \"Xenopus laevis NBRP cDNA clone:XL452f05ex, 3' end\",\n"
                       "    seqid {\n"
                       "      gi 46071105,\n"
                       "      ddbj {\n"
                       "        accession \"BP722512\",\n"
                       "        version 1\n"
                       "      }\n"
                       "    },\n"
                       "    taxid 0\n"
                       "  }\n"
                       "}\n");
    
    CHECK_EQUAL(expected, got);
}

BOOST_AUTO_TEST_CASE(GetHdrP)
{
    START;
    
    string got( s_Stringify(CSeqDB("data/seqp", CSeqDB::eProtein).GetHdr(0)) );
    
    string expected = ("Blast-def-line-set ::= {\n"
                       "  {\n"
                       "    title \"similar to KIAA0960 protein [Mus musculus]\",\n"
                       "    seqid {\n"
                       "      gi 38083732,\n"
                       "      other {\n"
                       "        accession \"XP_357594\",\n"
                       "        version 1\n"
                       "      }\n"
                       "    },\n"
                       "    taxid 0\n"
                       "  }\n"
                       "}\n");
    
    CHECK_EQUAL(expected, got);
}

BOOST_AUTO_TEST_CASE(GetSeqIDsN)
{
    START;
    
    CSeqDB seqp("data/seqn", CSeqDB::eNucleotide);
    
    list< CRef< CSeq_id > > seqids =
        seqp.GetSeqIDs(0);
    
    Uint4 h = 0;
    
    int cnt =0;
    for(list< CRef< CSeq_id > >::iterator i = seqids.begin();
        i != seqids.end();
        i++) {
        
        string s( s_Stringify(*i) );
        
        cnt++;
        
        h = s_BufHash(s.data(), s.length(), h);
    }
    
    CHECK_EQUAL(Uint4(136774894ul), h);
}

BOOST_AUTO_TEST_CASE(GetSeqIDsP)
{
    START;
    
    CSeqDB seqp("data/seqp", CSeqDB::eProtein);
    
    list< CRef< CSeq_id > > seqids =
        seqp.GetSeqIDs(0);
    
    Uint4 h = 0;
    
    int cnt =0;
    for(list< CRef< CSeq_id > >::iterator i = seqids.begin();
        i != seqids.end();
        i++) {
        
        string s( s_Stringify(*i) );
        
        cnt++;
        
        h = s_BufHash(s.data(), s.length(), h);
    }
    
    CHECK_EQUAL(Uint4(2942938647ul), h);
}

BOOST_AUTO_TEST_CASE(GetSeqLength)
{
    START;
    
    CSeqDB dbp("data/seqp", CSeqDB::eProtein);
    CSeqDB dbn("data/seqn", CSeqDB::eNucleotide);
    
    CHECK_EQUAL_TYPED( int, 330, dbp.GetSeqLength(13) );
    CHECK_EQUAL_TYPED( int, 422, dbp.GetSeqLength(19) );
    CHECK_EQUAL_TYPED( int, 67,  dbp.GetSeqLength(26) );
    CHECK_EQUAL_TYPED( int, 104, dbp.GetSeqLength(27) );
    CHECK_EQUAL_TYPED( int, 282, dbp.GetSeqLength(38) );
    CHECK_EQUAL_TYPED( int, 158, dbp.GetSeqLength(43) );
    CHECK_EQUAL_TYPED( int, 472, dbp.GetSeqLength(54) );
    CHECK_EQUAL_TYPED( int, 207, dbp.GetSeqLength(93) );
    
    CHECK_EQUAL_TYPED( int, 833, dbn.GetSeqLength(9)  );
    CHECK_EQUAL_TYPED( int, 250, dbn.GetSeqLength(26) );
    CHECK_EQUAL_TYPED( int, 708, dbn.GetSeqLength(39) );
    CHECK_EQUAL_TYPED( int, 472, dbn.GetSeqLength(43) );
    CHECK_EQUAL_TYPED( int, 708, dbn.GetSeqLength(39) );
    CHECK_EQUAL_TYPED( int, 448, dbn.GetSeqLength(47) );
    CHECK_EQUAL_TYPED( int, 825, dbn.GetSeqLength(61) );
    CHECK_EQUAL_TYPED( int, 371, dbn.GetSeqLength(70) );
}

BOOST_AUTO_TEST_CASE(GetSeqLengthApprox)
{
    START;
    
    CSeqDB dbp("data/seqp", CSeqDB::eProtein);
    CSeqDB dbn("data/seqn", CSeqDB::eNucleotide);
    
    int plen(0), nlen(0);
    
    dbp.GetTotals(CSeqDB::eFilteredAll, & plen, 0);
    dbn.GetTotals(CSeqDB::eFilteredAll, & nlen, 0);
    
    int i = 0;
    
    Uint8 ptot(0);
    
    // For protein, approximate should be the same as exact.
    for(i = 0; i < plen; i++) {
        Uint4 len = dbp.GetSeqLength(i);
        ptot += len;
        CHECK_EQUAL( len, (Uint4)dbp.GetSeqLengthApprox(i) );
    }
    
    // For nucleotide, approx should be within 3 of exact.
    Uint8 ex_tot = 0;
    Uint8 ap_tot = 0;
    
    for(i = 0; i < nlen; i++) {
        Uint4 ex = dbn.GetSeqLength(i);
        Uint4 ap = dbn.GetSeqLengthApprox(i);
        
        s_ApproxEqual(ex, ap, 3, __LINE__);
        ex_tot += ex;
        ap_tot += ap;
    }
    
    // Test case: guarantee the approximation over 2004 sequences
    // to be between .999 and 1.001 of correct value.
    
    s_ApproxEqual(ex_tot, ap_tot, ex_tot / 1000, __LINE__);
    
    CHECK_EQUAL(int(100), nlen);
    CHECK_EQUAL(int(100), plen);
    CHECK_EQUAL(Uint8(26945), ptot);
    CHECK_EQUAL(Uint8(51718), ex_tot);
    CHECK_EQUAL(Uint8(51726), ap_tot);
}

BOOST_AUTO_TEST_CASE(GetSequenceN)
{
    START;
    
    CSeqDB seqp("data/seqn", CSeqDB::eNucleotide);
    
    const char * bufp = 0;
    
    Uint4 length = seqp.GetSequence(0, & bufp);
    Uint4 hashval = s_BufHash(bufp, length);
    seqp.RetSequence(& bufp);
    
    CHECK_EQUAL(Uint4(1128126064ul), hashval);
}

BOOST_AUTO_TEST_CASE(GetSequenceP)
{
    START;
    
    CSeqDB seqp("data/seqp", CSeqDB::eProtein);
    
    const char * bufp = 0;
    Uint4 length = seqp.GetSequence(0, & bufp);
    Uint4 hashval = s_BufHash(bufp, length);
    seqp.RetSequence(& bufp);
    
    CHECK_EQUAL(Uint4(3219499033ul), hashval);
}

BOOST_AUTO_TEST_CASE(NrAndSwissProt)
{
    START;
    
    CSeqDB nr("nr",        CSeqDB::eProtein);
    CSeqDB sp("swissprot", CSeqDB::eProtein);
    
    int   nr_seqs(0), nr_oids(0), sp_seqs(0), sp_oids(0);
    Uint8 nr_tlen(0), nr_vlen(0), sp_tlen(0), sp_vlen(0);
    
    nr.GetTotals(CSeqDB::eFilteredAll, & nr_seqs, & nr_tlen);
    sp.GetTotals(CSeqDB::eFilteredAll, & sp_seqs, & sp_tlen);
    
    nr.GetTotals(CSeqDB::eUnfilteredAll, & nr_oids, & nr_vlen);
    sp.GetTotals(CSeqDB::eUnfilteredAll, & sp_oids, & sp_vlen);
    
    CHECK(nr_seqs == nr_oids);
    CHECK(nr_tlen == nr_vlen);
    
    CHECK(nr_seqs >  sp_seqs);
    CHECK(nr_oids == sp_oids);
    CHECK(nr_tlen >  sp_tlen);
    CHECK(nr_vlen == sp_vlen);
    
    CHECK(nr.GetMaxLength() >= sp.GetMaxLength());
    
    Uint4 sp_cnt = 1;
    Uint4 nr_cnt = 1;
    
    CSeqDBIter nr_iter = nr.Begin();
    CSeqDBIter sp_iter = sp.Begin();
    
    for(Uint4 i = 0; i<10000; i++) {
        while (nr_iter.GetOID() < sp_iter.GetOID()) {
            ++ nr_iter;
            ++ nr_cnt;
        }
        
        CHECK_EQUAL(nr_iter.GetOID(),    sp_iter.GetOID());
        CHECK_EQUAL(nr_iter.GetLength(), sp_iter.GetLength());
        
        Uint4 nr_hash = s_BufHash(nr_iter.GetData(), nr_iter.GetLength());
        Uint4 sp_hash = s_BufHash(nr_iter.GetData(), nr_iter.GetLength());
        
        CHECK_EQUAL(nr_hash, sp_hash);
        
        ++ sp_iter;
        ++ sp_cnt;
        
        // Without this test, the loop will break because it
        // iterates past the end of swissprot.
        
        if (! sp_iter) {
            break;
        }
    }
    
    // If the first 10000 sequences in nr are all in sp, something is up.
    
    CHECK(nr_cnt > sp_cnt);
}

BOOST_AUTO_TEST_CASE(TranslateIdents)
{
    START;
    
    CSeqDB nr("nr", CSeqDB::eProtein);
    
    Uint4 gi_list[] = {
        329847,   737263,   1708889,  2612955,  2982661,  3115393,
        3318935,  3930059,  4868071,  6573653,  7530437,  9657431,
        9951219,  12044947, 12698889
    };
    
    Uint4 pig_list[] = {
        1153908,  507276,   851580,   200775,   1028308,  939134,
        199107,   511756,   27645,    429124,   575812,   648744,
        421191,   638614,   1128836
    };
    
    Uint4 len_list[] = {
        199,      233,      186,      441,      96,       206,
        277,      205,      110,      206,      510,      293,
        394,      398,      174
    };
    
    Uint4 L_gi  = Uint4(sizeof(gi_list)  / sizeof(gi_list[0]));
    Uint4 L_pig = Uint4(sizeof(pig_list) / sizeof(pig_list[0]));
    Uint4 L_len = Uint4(sizeof(len_list) / sizeof(len_list[0]));
    
    // In case of hand-editing
    CHECK((L_gi == L_len) && (L_len == L_pig));
    
    for(Uint4 i = 0; i<L_gi; i++) {
        int arr_gi(0), arr_pig(0), arr_len(0),
            gi2pig(0), gi2oid(0), pig2gi(0), pig2oid(0),
            oid2len(0), oid2pig(0), oid2gi(0),
            pig2gi2oid(0);
        
        bool b1,b2,b3,b4,b5,b6,b7;
        
        arr_gi  = gi_list[i];
        arr_pig = pig_list[i];
        arr_len = len_list[i];
        
        b1 = nr.GiToPig(arr_gi, gi2pig);
        b2 = nr.GiToOid(arr_gi, gi2oid);
        
        b3 = nr.PigToGi (arr_pig, pig2gi);
        b4 = nr.GiToOid (pig2gi,  pig2gi2oid);
        b5 = nr.PigToOid(arr_pig, pig2oid);
        
        CHECK_EQUAL(arr_pig, gi2pig);
        CHECK_EQUAL(pig2oid, gi2oid);
        CHECK_EQUAL(pig2oid, pig2gi2oid);
        CHECK(pig2oid != int(-1));
        
        oid2len = nr.GetSeqLength(pig2oid);
        b6 = nr.OidToGi (pig2oid, oid2gi);
        b7 = nr.OidToPig(pig2oid, oid2pig);
        
        CHECK_EQUAL(arr_len, oid2len);
        CHECK_EQUAL(arr_pig, oid2pig);
        CHECK_EQUAL(pig2gi,  oid2gi);
        
        CHECK(b1 && b2 && b3 && b4 && b5 && b6 && b7);
    }
}

BOOST_AUTO_TEST_CASE(StringIdentSearch)
{
    START;
    
    CSeqDB nr("nr", CSeqDB::eProtein);
    
    // Sets of equivalent strings
    
    const Uint4 NUM_ITEMS = 6;
    
    const char ** str_list[NUM_ITEMS];
    
    const char * s0[] =
        { "gi|32894304",   "gb|AAP90888.1", "gi|32894290",
          "gb|AAP90875.1", "gi|32894276",   "gb|AAP90862.1",
          "gi|32894262",   "gb|AAP90849.1", "gi|32894248",
          "gb|AAP90836.1", "gi|32894234",   "gb|AAP90823.1",
          "gi|32894220",   "gb|AAP90810.1", "gi|32894206",
          "gb|AAP90797.1", "gi|32894192",   "gb|AAP90784.1",
          "gi|32894178",   "gb|AAP90771.1", "gi|32894164",
          "gb|AAP90758.1", "gi|32894150",   "gb|AAP90745.1",
          "gi|32894136",   "gb|AAP90732.1", "gi|32894122",
          "gb|AAP90719.1", "gi|32894108",   "gb|AAP90706.1",
          "gi|32894066",   "gb|AAP90667.1", "gi|32894052",
          "gb|AAP90654.1", "gi|32894038",   "gb|AAP90641.1",
          "gi|32894024",   "gb|AAP90628.1", "gi|32894010",
          "gb|AAP90615.1", 0 };
    const char * s1[] =
        { "gi|129295", "sp|P01013|OVAX_CHICK", 0 };
    const char * s2[] =
        { "gi|30749669", "pdb|1NPQ|A", "1NPQ",
          "gi|30749670",  "pdb|1NPQ|B", "1npq", 0 };
    const char * s3[] =
        { "1NPQ", 0 };
    const char * s4[] =
        { "gi|157831779", "pdb|1LCT|A", "1LCT", 0 };
    const char * s5[] =
        { "gi|157878050", "gi|28948349", "pdb|1GWB|A", "pdb|1GWB|B", "1GWB", 0 };
    
    str_list[0] = s0;
    str_list[1] = s1;
    str_list[2] = s2;
    str_list[3] = s3;
    str_list[4] = s4;
    str_list[5] = s5;
    
    // Each set of strings should produce a set of GIs.
    
    Uint4 * gi_list[NUM_ITEMS];
    
    Uint4 g0[] =
        { 32894304, 32894290, 32894276, 32894262, 32894248,
          32894234, 32894220, 32894206, 32894192, 32894178,
          32894164, 32894150, 32894136, 32894122, 32894108,
          32894066, 32894052, 32894038, 32894024, 32894010,
          0 };
    Uint4 g1[] =
        { 129295, 0 };
    Uint4 g2[] =
        { 30749669, 30749670, 0 };
    Uint4 g3[] =
        { 30749669, 30749670, 0 };
    Uint4 g4[] =
        { 157831779, 0 };
    Uint4 g5[] =
        { 157878050, 28948349, 0 };
    
    gi_list[0] = g0;
    gi_list[1] = g1;
    gi_list[2] = g2;
    gi_list[3] = g3;
    gi_list[4] = g4;
    gi_list[5] = g5;
    
    Uint4 * len_list[NUM_ITEMS];
    
    Uint4 l0[] = { 261, 0 };
    Uint4 l1[] = { 232, 0 };
    Uint4 l2[] = { 17, 90, 0 };
    Uint4 l3[] = { 17, 90, 0 };
    Uint4 l4[] = { 333, 0 };
    Uint4 l5[] = { 281, 0 };
    
    len_list[0] = l0;
    len_list[1] = l1;
    len_list[2] = l2;
    len_list[3] = l3;
    len_list[4] = l4;
    len_list[5] = l5;
    
    Uint4 L_gi  = Uint4(sizeof(gi_list)  / sizeof(gi_list[0]));
    Uint4 L_str = Uint4(sizeof(gi_list)  / sizeof(gi_list[0]));
    Uint4 L_len = Uint4(sizeof(len_list) / sizeof(len_list[0]));
    
    // Verify lengths in case of typo.
    
    CHECK_EQUAL(L_gi, L_str);
    CHECK_EQUAL(L_gi, L_len);
    CHECK_EQUAL(L_gi, NUM_ITEMS);
    
    for(Uint4 i = 0; i<L_gi; i++) {
        set<int> str_oids;
        set<int> gi_oids;
        
        set<int> exp_len;
        set<int> oid_len;
        
        for(Uint4 * gip = gi_list[i]; *gip; gip++) {
            int oid1(-1);
            bool worked = nr.GiToOid(*gip, oid1);
            
            if (! worked) {
                cerr << "Failing GI is " << *gip << endl;
            }
            
            CHECK(worked);
            
            gi_oids.insert(oid1);
        }
        
        for(const char ** strp = str_list[i]; *strp; strp++) {
            vector<int> oids;
            nr.AccessionToOids(*strp, oids);
            
            CHECK(! oids.empty());
            
            ITERATE(vector<int>, iter, oids) {
                str_oids.insert(*iter);
            }
        }
        
        CHECK_EQUAL(gi_oids.size(), str_oids.size());
        
        set<int>::iterator gi_iter, str_iter;
        
        // Phase 1: compare oids
        
        gi_iter  = gi_oids .begin();
        str_iter = str_oids.begin();
        
        while(gi_iter != gi_oids.end()) {
            CHECK(str_iter != str_oids.end());
            CHECK_EQUAL(*gi_iter, *str_iter);
            
            gi_iter++;
            str_iter++;
        }
        
        // Phase 2: compare lengths
        
        Uint4 * llp = len_list[i];
        
        while(*llp) {
            exp_len.insert(*llp++);
        }
        
        ITERATE(set<int>, iter, gi_oids) {
            oid_len.insert(nr.GetSeqLength(*iter));
        }
        
        set<int>::iterator oid_iter, exp_iter;
        
        oid_iter = oid_len.begin();
        exp_iter = exp_len.begin();
        
        while(oid_iter != oid_len.end()) {
            CHECK(exp_iter != exp_len.end());
            CHECK_EQUAL(*oid_iter, *exp_iter);
            
            oid_iter++;
            exp_iter++;
        }
    }
}

BOOST_AUTO_TEST_CASE(AmbigBioseq)
{
    START;
    
    // Originally, this code compared SeqDB's output to readb's; this
    // is no longer the case because it would create a dependency on
    // the C toolkit.
    
    const char * dbname = "nt";
    
    bool is_prot = false;
    int gi = 12831944;
    
    CNcbiOstrstream oss_fn;
    oss_fn << "." << dbname << "." << gi;
    
    vector<char> seqdb_data;
    vector<char> expected_data;
    
    string seqdb_bs;
    
    {
        CSeqDB db(dbname, is_prot ? CSeqDB::eProtein : CSeqDB::eNucleotide);
        
        int oid(0);
        
        bool gi_trans = db.GiToOid(gi, oid);
        
        CHECK(gi_trans);
        
        CRef<CBioseq> bs = db.GetBioseq(oid);
        
        // These have changed for SeqDB.
        bs->ResetDescr();
        
        seqdb_bs = s_Stringify(bs);
        
        CHECK(! bs.Empty());
        
        seqdb_data = bs->GetInst().GetSeq_data().GetNcbi4na().Get();
        
        CHECK_EQUAL(int(seqdb_data.size()), 872);
    }
    
    string expected_bs =
        "Bioseq ::= {\n"
        "  id {\n"
        "    gi 12831944,\n"
        "    embl {\n"
        "      name \"DBI389663\",\n"
        "      accession \"AJ389663\",\n"
        "      version 1\n"
        "    }\n"
        "  },\n"
        "  inst {\n"
        "    repr raw,\n"
        "    mol na,\n"
        "    length 1744,\n"
        "    seq-data ncbi4na '42184822114812141288821418148411122424118442821881118214\n"
        "824144882288141824882211822512824418112848442118828141428118121842111211428224\n"
        "122228888112244444411141424288881881418211112211842444888848282442118222428211\n"
        "288884484128284418112888484284182421244222824142244241248182888211184828422281\n"
        "821128881482488124841818422811241448848812444811244441182144488241882244141444\n"
        "142184141112442812212182211441144214214424242111881222128222442124444144814841\n"
        "241111181124184244412828182414422224811824411841481212888111822888112414418211\n"
        "884414442114828448422142142242448118822142822118142481818811148848842148811111\n"
        "428248148844182824444411442814244864242248844424822812842824122841228122442244\n"
        "814888484222414484282884128414848282444841224424148881288841111118814148428211\n"
        "142144228848422422241181484484218441181184411414412282448828188884884488882441\n"
        "124841448118418811414441214124444421688248188424424281414484111882884412242242\n"
        "11412441281284241114218884221142184888821881FFF1141124111482141448824114124182\n"
        "141812248244814882841221811124FFF241284424182243241148812812818412824424442142\n"
        "228214441112211148288844488224444411481844884F11142841112881114411884124411444\n"
        "212212214414844142284244288118884128211212444111128212224422244121224841441884\n"
        "121418841414282888282418824484448448448421844224882881488448441424188848284488\n"
        "11882241811241124141282814228428111814822A224188242228182482442144412882881414\n"
        "441241484424818142212424141884142118112144828484184222881418488244442242124242\n"
        "428121284114411821421248284228222844222411144488444811222428411228228824842814\n"
        "441884444288481188488222218411241441188222148114242414821811428242488418812482\n"
        "228422288848121212242224824281281221188414244888128414441211441884422224124144\n"
        "24282244248282842448A88842241411284222211148421284'H\n"
        "  }\n"
        "}\n";
    
    string data_str =
        "GCATGTCCAAGTACAGACTTTCAGATAGTGAAACCGCGAATGGCTCATTAAATCAGTCGA"
        "GGTTCCTTAGATCGTTCCAATCCRACTCGGATAACTGTGGCAATTCTAGAGCTAATACAT"
        "GCAAACAAGCTCCGACCCCTTTTAACCGGGGGGAAAGAGCGCTTTTATTAGATCAAAACC"
        "AATGCGGGTTTTGTCTCGGCAATCCCGCTCAACTTTTGGTGACTCTGGATAACTTTGTGC"
        "TGATCGCACGGCCCTCGAGCCGGCGACGTATCTTTCAAATGTCTGCCCTATCAACTTTAG"
        "TCGTTACGTGATATGCCTAACGAGGTTGTTACGGGTAACGGGGAATCAGGGTTCGATTCC"
        "GGAGAGGGAGCATGAGAAACGGCTACCACATCCAAGGAAGGCAGCAGGCGCGCAAATTAC"
        "CCACTCCCGGCACGGGGAGGTAGTGACGAAAAATAACGATGCGGGACTCTATCGAGGCCC"
        "CGTAATCGGAATGAGTACACTTTAAATCCTTTAACGAGGATCAATTGGAGGGCAAGTCTG"
        "GTGCCAGCAGCCGCGGTAATTCCAGCTCCAATAGCGTATATTAAAGTTGTTGCAGTTAAA"
        "AAGCTCGTAGTTGGATCTCGGGGGAAGGCTAGCGGTSGCGCCGTTGGGCGTCCTACTGCT"
        "CGACCTGACCTACCGGCCGGTAGTTTGTGCCCGAGGTGCTCTTGACTGAGTGTCTCGGGT"
        "GACCGGCGAGTTTACTTTGAAAAAATTAGAGTGCTCAAAGCAGGCCTTGTGCCGCCCGAA"
        "TAGTGGTGCATGGAATAATGGAAGAGGACCTCGGTTCTATTTTGTTGGTTTTCGGAACGT"
        "GAGGTAATGATTAAGAGGGACAGACGGGGGCA";
    
    expected_data.assign(data_str.data(),
                         data_str.data() + data_str.size());
    
    vector<char> seqdb_tmp;
    
    CSeqConvert::Convert(seqdb_data,
                         CSeqUtil::e_Ncbi4na,
                         0,
                         seqdb_data.size(),
                         seqdb_tmp,
                         CSeqUtil::e_Iupacna);
    
    seqdb_tmp.swap(seqdb_data);
    
    CHECK_EQUAL(expected_bs, seqdb_bs);
    CHECK_EQUAL(expected_data.size(), seqdb_data.size());
    
    Uint4 num_diffs = 0;
    
    for(Uint4 i = 0; i < expected_data.size(); i++) {
        unsigned R = unsigned(expected_data[i]) & 0xFF;
        unsigned S = unsigned(seqdb_data[i])  & 0xFF;
        
        if (R != S) {
            if (! num_diffs)
                cout << "\n\n";
            
            cout << "At location " << dec << i << ", Readdb has: " << hex << int(R) << " whereas SeqDB has: " << hex << int(S);
            
            if (R > S) {
                cout << " (R += " << (R - S) << ")\n";
            } else {
                cout << " (S += " << (S - R) << ")\n";
            }
            
            num_diffs ++;
        }
    }
    
    if (num_diffs) {
        cout << "Num diffs: " << dec << num_diffs << endl;
    }
    
    CHECK_EQUAL_TYPED(int, 0, num_diffs);
}

BOOST_AUTO_TEST_CASE(GetLenHighOID)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB dbp("data/seqp", CSeqDB::eProtein);
        int num_seqs(0);
        dbp.GetTotals(CSeqDB::eFilteredAll, & num_seqs, 0);
        
        int len = dbp.GetSeqLength(num_seqs);
        
        CHECK_EQUAL_TYPED(int, 11112222, len);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("GetLenHighOID() did not throw an exception of type        CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(GetLenNegOID)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB dbp("data/seqp", CSeqDB::eProtein);
        Uint4 len = dbp.GetSeqLength(Uint4(-1));
        
        CHECK_EQUAL_TYPED(Uint4, 11112222, len);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("GetLenNegOID() did not throw an exception of type         CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(GetSeqHighOID)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB dbp("data/seqp", CSeqDB::eProtein);
        
        int nseqs(0);
        dbp.GetTotals(CSeqDB::eFilteredAll, & nseqs, 0);
        
        const char * buffer = 0;
        Uint4 len = dbp.GetSequence(nseqs, & buffer);
        
        CHECK_EQUAL_TYPED(Uint4, 11112222, len);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("GetSeqHighOID() did not throw an exception of type        CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(GetSeqNegOID)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB dbp("data/seqp", CSeqDB::eProtein);
        
        const char * buffer = 0;
        Uint4 len = dbp.GetSequence(Uint4(-1), & buffer);
        
        CHECK_EQUAL_TYPED(Uint4, 11112222, len);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("GetSeqNegOID() did not throw an exception of type         CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(Offset2OidBadOffset)
{
    START;
    
    bool caught_exception = false;
    
    try {
        //, CSeqDBException);
        CSeqDB nr("nr", CSeqDB::eProtein);
        
        Uint8 vlength(0);
        nr.GetTotals(CSeqDB::eUnfilteredAll, 0, & vlength);
        
        nr.GetOidAtOffset(0, vlength + 1);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("Offset2OidBadOffset() did not throw an exception of type  CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(Offset2OidBadOid)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB nr("nr", CSeqDB::eProtein);
        
        Int4 noids(0);
        nr.GetTotals(CSeqDB::eUnfilteredAll, & noids, 0);
        
        nr.GetOidAtOffset(noids + 1, 0);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("Offset2OidBadOid() did not throw an exception of type     CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(Offset2OidMonotony)
{
    START;
    
    Uint4 segments = 1000;
    
    for(Uint4 i = 0; i<2; i++) {
        string dbname((i == 0) ? "nr" : "nt");
        CSeqDB::ESeqType sqtype((i == 0)
                                ? CSeqDB::eProtein
                                : CSeqDB::eNucleotide);
        
        CSeqDB db(dbname, sqtype);
        
        int prev_oid = 0;
        int num_oids = 0;
        Uint8 vol_length(0);
        
        db.GetTotals(CSeqDB::eUnfilteredAll, & num_oids, & vol_length);
        
        for(Uint4 j = 0; j < segments; j++) {
            Uint8 range_target = (vol_length * j) / segments;
            
            int oid_here = db.GetOidAtOffset(0, range_target);
            
            double range_ratio  = double(range_target) / vol_length;
            double oid_ratio    = double(oid_here)     / num_oids;
            double percent_diff = 100.0 * fabs(oid_ratio - range_ratio);
            
            // Up to 30 % slack will be permitted.  Normally this runs
            // to a maximum under 5%.  This can break when the db is
            // reorganized (as they are every day) if reorganization
            // moves a lot of heavy sequences to one end.
            //
            // This was 15% - changed on Jan 2, 2008.
            
            CHECK(prev_oid     <= oid_here);
            CHECK(percent_diff <= 30.0);
            
            prev_oid = oid_here;
        }
    }
}

BOOST_AUTO_TEST_CASE(GiLists)
{
    START;
    
    vector<string> names;
    
    names.push_back("p,nr");
    names.push_back("n,nt");
    names.push_back("n,genomes/rice");
    names.push_back("p,genomes/rice");
    names.push_back("n,genomes/tomato");
    
    ITERATE(vector<string>, s, names) {
        CHECK(s->length() > 2);
        
        char prot_nucl = (*s)[0];
        string dbname(*s, 2, s->length()-2);
        
        CSeqDB db(dbname, prot_nucl == 'p' ? CSeqDB::eProtein : CSeqDB::eNucleotide);
    }
}

BOOST_AUTO_TEST_CASE(OidRanges)
{
    START;
    
    // Alias file name prefixes
    
    const char * mask_name[] = {
        "range", "odd", "even", "prime", "ERROR"
    };
    
    // For each of the masking types, an alias file exists that
    // covers each of the OID intervals shown below.  There are
    // several cases in the OID mask / OID range combination code;
    // these intervals (except the 0,0) are intended to measure
    // the ability of that code to deal with edge conditions
    // (edges of bytes and edges of OID ranges for example).
    
    int ranges[] = {
        1, 2,
        1, 5,
        1, 7,
        1, 8,
        1, 9,
        3, 7,
        3, 13,
        1, 1,
        10, 100,
        8, 128,
        10, 130,
        9, 130,
        9, 129,
        8, 130,
        8, 129,
        9, 128,
        1, 10,
        0, 0
    };
    
    vector<int> oids;
    
    for(EMaskingType mask=eAll; mask<eEND; mask = EMaskingType(mask+1)) {
        for(int i = 0; ranges[i]; i += 2) {
            unsigned first = ranges[i];
            unsigned second = ranges[i+1];
            
            if (((mask == eOdd) && (first != 3) || (second != 7)) || (mask == eAll)) {
                continue;
            }
            
            string dbname = s_ToString("data/ranges/", mask_name[mask], first, "_", second);
            CSeqDB db(dbname, CSeqDB::eProtein);
            
            int obegin(0), oend(0);
            
            oids.resize(10);
            
            int count(0);
            int lowest(INT_MAX);
            int highest(0);
            
            // This could be done more cleanly with CheckOrFindOID
            // as in the next example, but this serves as an
            // additional wrinkle (ie for testing purposes).  Or
            // else I forgot that CheckOrFindOID was available.
            
            // This tests that all returned OIDs should in fact be
            // included.  It currently does not make any attempt
            // to verify that no OIDs are missed (but it could).
            
            while(1) {
                CSeqDB::EOidListType range_type =
                    db.GetNextOIDChunk(obegin, oend, oids);
                
                unsigned num_found(0);
                
                if (range_type == CSeqDB::eOidList) {
                    num_found = (int) oids.size();
                    
                    ITERATE(vector<int>, iter, oids) {
                        if ((*iter) > highest) {
                            highest = (*iter);
                        }
                        
                        if ((*iter) < lowest) {
                            lowest = (*iter);
                        }
                        
                        CHECK(s_MaskingTest(mask, *iter));
                    }
                } else {
                    num_found = oend-obegin;
                    
                    if (num_found) {
                        if (oend > highest) {
                            highest = oend;
                        }
                        
                        if (obegin < lowest) {
                            lowest = obegin;
                        }
                    }
                    
                    for(int v = obegin; v < oend; v++) {
                        CHECK(s_MaskingTest(mask, v));
                    }
                }
                
                if (! num_found) {
                    break;
                }
                
                count += num_found;
            }
            
            s_TestMaskingLimits(mask, first-1, second-1, lowest, highest, count);
        }
    }
}

BOOST_AUTO_TEST_CASE(GiListOidRange)
{
    START;
    
    int low_gi  = 20*1000*1000;
    int high_gi = 30*1000*1000;
    
    int low_oid  = 50;
    int high_oid = 150;
    
    vector<string> dbs;
    dbs.push_back("data/seqp");
    dbs.push_back("data/ranges/seqp15");
    dbs.push_back("data/ranges/twenty");
    dbs.push_back("data/ranges/twenty15");
    
    for(Uint4 dbnum = 0; dbnum < dbs.size(); dbnum++) {
        CSeqDB db(dbs[dbnum], CSeqDB::eProtein);
        
        bool all_gis_in_range  = true;
        bool all_oids_in_range = true;
        
        // Get all included OIDs and GIs for this database
        for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
            if (! (all_oids_in_range || all_gis_in_range)) {
                break;
            }
            
            if (all_oids_in_range) {
                if ((oid < (low_oid-1)) || ((high_oid-1)) < oid) {
                    all_oids_in_range = false;
                }
            }
            
            if (all_gis_in_range) {
                list< CRef<CSeq_id> > ids = db.GetSeqIDs(oid);
                
                bool gi_in_range = false;
                
                ITERATE(list< CRef<CSeq_id> >, seqid, ids) {
                    if ((**seqid).IsGi()) {
                        int gi = (**seqid).GetGi();
                        
                        if ((gi > low_gi) && (gi < high_gi)) {
                            gi_in_range = true;
                            break;
                        }
                    }
                }
                
                if (! gi_in_range) {
                    all_gis_in_range = false;
                }
            }
        }
        
        bool gis_confined (false);
        bool oids_confined(false);
        
        switch(dbnum) {
        case 0:
            gis_confined  = false;
            oids_confined = false;
            break;
            
        case 1:
            gis_confined  = false;
            oids_confined = true;
            break;
            
        case 2:
            gis_confined  = true;
            oids_confined = false;
            break;
            
        case 3:
            gis_confined  = true;
            oids_confined = true;
            break;
        }
        
        CHECK_EQUAL(oids_confined, all_oids_in_range);
        CHECK_EQUAL(gis_confined,  all_gis_in_range);
    }
}

BOOST_AUTO_TEST_CASE(EmptyDBList)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDB db("", CSeqDB::eProtein);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("EmptyDBList() did not throw an exception of type  CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(BinaryUserGiList)
{
    START;
    
    CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("data/prot345b.gil"));
    CSeqDB db("data/seqp", CSeqDB::eProtein, 0, 0, true, gi_list);
    
    int found(0);
    
    for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
        found ++;
    }
    
    // The GI list has 471 elements, only 58 of those are in the DB.
    CHECK_EQUAL(29, found);
}

BOOST_AUTO_TEST_CASE(TextUserGiList)
{
    START;
    
    CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("data/prot345t.gil"));
    CSeqDB db("data/seqp", CSeqDB::eProtein, 0, 0, true, gi_list);
    
    int found(0);
    
    for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
        found ++;
    }
    
    // The GI list has 471 elements, only 58 of those are in the DB.
    CHECK_EQUAL(29, found);
}

BOOST_AUTO_TEST_CASE(CSeqDBFileGiList_GetGis)
{
    START;
    
    const string kFileName("data/prot345t.gil");
    
    // Read using CSeqDBFileGiList
    CSeqDBFileGiList seqdbgifile(kFileName);
    vector<int> gis;
    seqdbgifile.GetGiList(gis);
    CHECK_EQUAL_TYPED(size_t, seqdbgifile.GetNumGis(), gis.size());
    sort(gis.begin(), gis.end());
    
    // Read text gi list manually
    string fn = CDirEntry::ConvertToOSPath(kFileName);
    ifstream gifile(fn.c_str());
    CHECK(gifile);
    
    vector<int> reference;
    reference.reserve(gis.size());
    while ( !gifile.eof() ) {
        int gi(-1);     // gis can't be negative
        gifile >> gi;
        if (gi == -1) break;
        reference.push_back(gi);
    }
    sort(reference.begin(), reference.end());
    CHECK_EQUAL(reference.size(), gis.size());
    
    // Compare the contents
    for (size_t i = 0; i < reference.size(); i++) {
        string msg = "Failed on element " + NStr::IntToString(i);
        CHECK_EQUAL_MESSAGE(msg, reference[i], gis[i]);
    }
}

BOOST_AUTO_TEST_CASE(TwoGiListsOneVolume)
{
    START;
    
    vector<string> dbs;
    dbs.push_back("Microbial/83331");
    dbs.push_back("Microbial/83332");
    dbs.push_back(dbs[0] + " " + dbs[1]);
    
    vector< vector<int> >    gis(dbs.size());
    vector< vector<string> > volumes(dbs.size());
    
    for(int i = 0; i < (int)dbs.size(); i++) {
        CSeqDB db(dbs[i], CSeqDB::eNucleotide);
        
        db.FindVolumePaths(volumes[i]);
        
        // Collect all the included gis.
        
        for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
            db.GetGis(oid, gis[i], true);
        }
    }
    
    // Check that the same volume underlies both database aliases.
    
    CHECK(volumes[0] == volumes[1]);
    CHECK(volumes[0] == volumes[2]);
    CHECK_EQUAL(gis[0].size() + gis[1].size(), gis[2].size());
    
    vector<int> zero_one(gis[0]);
    zero_one.insert(zero_one.end(), gis[1].begin(), gis[1].end());
    
    sort(zero_one.begin(), zero_one.end());
    sort(gis[2].begin(), gis[2].end());
    
    CHECK(zero_one == gis[2]);
}

BOOST_AUTO_TEST_CASE(GetTaxIDs)
{
    START;
    
    int gi1a = 129295;
    int tax1 = 9031;
    
    int gi2a  = 5832633;
    int gi2b  = 5832649;
    int tax2a = 82243;
    int tax2b = 82255;
    
    int oid1 = -1;
    int oid2 = -1;
    
    CSeqDB db("nr", CSeqDB::eProtein);
    
    bool success = db.GiToOid(gi1a, oid1);
    CHECK(success);
    
    success = db.GiToOid(gi2a, oid2);
    CHECK(success);
    
    typedef map<int, int> TGTMap;
    map<int, int> gi2taxid;
    
    db.GetTaxIDs(oid1, gi2taxid);
    CHECK_EQUAL((int)gi2taxid.size(), 1);
    CHECK_EQUAL(gi2taxid[gi1a],       tax1);
    
    db.GetTaxIDs(oid2, gi2taxid, false);
    CHECK_EQUAL((int)gi2taxid.size(), 2);
    CHECK_EQUAL(gi2taxid[gi2a],       tax2a);
    CHECK_EQUAL(gi2taxid[gi2b],       tax2b);
    
    db.GetTaxIDs(oid1, gi2taxid, true);
    CHECK_EQUAL((int)gi2taxid.size(), 3);
    CHECK_EQUAL(gi2taxid[gi1a],       tax1);
    CHECK_EQUAL(gi2taxid[gi2a],       tax2a);
    CHECK_EQUAL(gi2taxid[gi2b],       tax2b);
}

BOOST_AUTO_TEST_CASE(GetTaxIDsVector)
{
    START;
    
    int gi1a = 129295;
    int tax1 = 9031;
    
    int gi2a  = 5832633;
    int tax2a = 82243;
    int tax2b = 82255;
    
    int oid1 = -1;
    int oid2 = -1;
    
    CSeqDB db("nr", CSeqDB::eProtein);
    
    bool success = db.GiToOid(gi1a, oid1);
    CHECK(success);
    
    success = db.GiToOid(gi2a, oid2);
    CHECK(success);
    
    typedef map<int, int> TGTMap;
    vector<int> taxids;
    
    db.GetTaxIDs(oid1, taxids);
    sort(taxids.begin(), taxids.end());
    CHECK_EQUAL((int)taxids.size(), 1);
    CHECK_EQUAL(taxids[0],       tax1);
    
    db.GetTaxIDs(oid2, taxids, false);
    sort(taxids.begin(), taxids.end());
    CHECK_EQUAL((int)taxids.size(), 2);
    CHECK_EQUAL(taxids[0],       tax2a);
    CHECK_EQUAL(taxids[1],       tax2b);
    
    db.GetTaxIDs(oid1, taxids, true);
    sort(taxids.begin(), taxids.end());
    CHECK_EQUAL((int)taxids.size(), 3);
    CHECK_EQUAL(taxids[0],       tax1);
    CHECK_EQUAL(taxids[1],       tax2a);
    CHECK_EQUAL(taxids[2],       tax2b);
}

BOOST_AUTO_TEST_CASE(PartialSequences)
{
    START;
    
    // 57340989 - is nicely marbled with ambiguities.
    // 24430781 - has several long ambiguous subsequences, one at the start.
    // 8885782  - has three ambiguities, one at the end.
    
    CSeqDB nt("nt", CSeqDB::eNucleotide);
    
    s_TestPartialAmbig(nt, 57340989);
    s_TestPartialAmbig(nt, 24430781);
    s_TestPartialAmbig(nt, 8885782);
}

BOOST_AUTO_TEST_CASE(GiListInOidRangeIteration)
{
    START;

    const int kNumTestGis = 3;
    const int kGiOids[kNumTestGis] = { 15, 51, 84 };
    CRef<CSeqDBGiList> gi_list(new CSeqDBFileGiList("data/seqn_3gis.gil"));
    
    CSeqDB db("data/seqn", CSeqDB::eNucleotide, gi_list);
    
    int start, end;
    vector<int> oid_list(kNumTestGis);
    
    db.SetIterationRange(0, kGiOids[0]+1);
    
    CSeqDB::EOidListType chunk_type = 
        db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidList);
    
    // One of the 3 gis falls within ordinal id range.
    CHECK_EQUAL(1, (int)oid_list.size());
    
    db.SetIterationRange(kGiOids[0]+1, kGiOids[1]+1);
    
    oid_list.resize(kNumTestGis);
    chunk_type = db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidList);
    
    // Two of the 3 gis falls within ordinal id range.
    CHECK_EQUAL(1, (int)oid_list.size());
    
    db.SetIterationRange(kGiOids[1]+1, 0);
    
    oid_list.resize(kNumTestGis);
    chunk_type = db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidList);
    
    // Two of the 3 gis falls within ordinal id range.
    CHECK_EQUAL(1, (int)oid_list.size());
}

BOOST_AUTO_TEST_CASE(SeqidToOid)
{
    START;
    
    CSeqDB db("nr", CSeqDB::eProtein);
    
    int oid = 0;
    
    vector<int> oids1;
    vector<int> oids2;
    
    CHECK(db.GiToOid(129295, oid));
    oids1.push_back(oid);
    
    CSeq_id seqid("gi|129295");
    
    CHECK(db.SeqidToOid(seqid, oid));
    oids1.push_back(oid);
    
    db.SeqidToOids(seqid, oids2);
    CHECK(! oids2.empty());
    
    ITERATE(vector<int>, iter, oids1) {
        CHECK(*iter == oids2[0]);
    }
}

BOOST_AUTO_TEST_CASE(TestResetInternalChunkBookmark)
{
    START;
    
    CSeqDB db("data/seqp", CSeqDB::eProtein);
    
    const int kFirstOid(0);
    const int kLastOid(100);
    db.SetIterationRange(kFirstOid, kLastOid);
    
    int start, end;
    vector<int> oid_list(kLastOid);
    
    CSeqDB::EOidListType chunk_type = 
        db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidRange);
    CHECK_EQUAL(kFirstOid, start);
    CHECK_EQUAL(kLastOid, end);
    
    chunk_type = db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidRange);
    CHECK_EQUAL(kFirstOid, start);
    CHECK_EQUAL(kFirstOid, end);
    
    db.ResetInternalChunkBookmark();
    chunk_type = db.GetNextOIDChunk(start, end, oid_list);
    CHECK(chunk_type == CSeqDB::eOidRange);
    CHECK_EQUAL(kFirstOid, start);
    CHECK_EQUAL(kLastOid, end);
}

BOOST_AUTO_TEST_CASE(ExpertNullConstructor)
{
    START;
    
    CSeqDBExpert db;
}

BOOST_AUTO_TEST_CASE(ExpertTaxInfo)
{
    START;
    
    CSeqDBExpert db;
    
    SSeqDBTaxInfo info;
    db.GetTaxInfo(57176, info);
    
    CHECK_EQUAL(info.taxid,           57176);
    CHECK_EQUAL((string)info.scientific_name, string("Aotus vociferans"));
    CHECK_EQUAL((string)info.common_name,     string("noisy night monkey"));
    CHECK_EQUAL((string)info.blast_name,      string("primates"));
    CHECK_EQUAL((string)info.s_kingdom,       string("E"));
}

BOOST_AUTO_TEST_CASE(ExpertRawData)
{
    START;
    
    CSeqDBExpert db("nt", CSeqDB::eNucleotide);
    
    int oid(-1);
    db.GiToOid(1465582, oid);
    
    int slen(0),alen(0);
    const char * buffer(0);
    
    db.GetRawSeqAndAmbig(oid, & buffer, & slen, & alen);
    
    unsigned h = s_BufHash(buffer + slen, alen);
    unsigned exp_hash = 705445389u;
    
    db.RetSequence(& buffer);
    
    CHECK_EQUAL((290/4) + 1, slen);
    CHECK_EQUAL(20,          alen);
    CHECK_EQUAL(exp_hash,    h);
}

BOOST_AUTO_TEST_CASE(ExpertRawDataProteinNulls)
{
    START;
    
    // Test the intersequence zero termination bytes.
    
    CSeqDBExpert db("nr", CSeqDB::eProtein);
    
    vector<int> oids;
    oids.push_back(0);
    oids.push_back(db.GetNumOIDs()-1);
    
    // This should not throw any exceptions (or core dump) if the
    // implementation of database reading and writing is correct.
    
    ITERATE(vector<int>, oid, oids) {
        int slen(0),alen(0);
        const char * buffer(0);
        
        db.GetRawSeqAndAmbig(*oid, & buffer, & slen, & alen);
        
        CReturnSeqBuffer riia(& db, buffer);
        
        string S(buffer, slen);
        string A(buffer + slen, alen);
        
        int len = db.GetSeqLength(*oid);
        
        CHECK_EQUAL_TYPED(int,  A.size(),        0);
        CHECK_EQUAL_TYPED(int,  S.size(),        len);
        CHECK_EQUAL_TYPED(int,  *(buffer-1),     0);
        CHECK_EQUAL_TYPED(int,  *(buffer+slen),  0);
    }
}

BOOST_AUTO_TEST_CASE(ExpertRawDataLength)
{
    START;
    
    // Tests that it is possible to get the length without getting
    // the data, and that RetSequence need not be called in this
    // case.
    
    CSeqDBExpert db("nt", CSeqDB::eNucleotide);
    
    int oid(-1);
    db.GiToOid(1465582, oid);
    
    int slen(0),alen(0);
    
    db.GetRawSeqAndAmbig(oid, 0, & slen, & alen);
    
    CHECK_EQUAL((290/4) + 1, slen);
    CHECK_EQUAL(20,          alen);
}

BOOST_AUTO_TEST_CASE(ExpertIdBounds)
{
    START;
    
    CSeqDBExpert nr("nr", CSeqDB::eProtein);
    
    // Tests ID bound functions.
    {
        int low(0), high(0), count(0);
        
        nr.GetGiBounds(& low, & high, & count);
        
        CHECK(low < high);
        CHECK(count);
    }
    
    {
        int low(0), high(0), count(0);
        
        nr.GetPigBounds(& low, & high, & count);
        
        CHECK(low < high);
        CHECK(count);
    }
    
    {
        string low, high;
        int count(0);
        
        nr.GetStringBounds(& low, & high, & count);
        
        CHECK(low < high);
        CHECK(count);
    }
}

BOOST_AUTO_TEST_CASE(ExpertIdBoundsNoPig)
{
    START;
    
    bool caught_exception = false;
    
    try {
        CSeqDBExpert nt("nt", CSeqDB::eNucleotide);
        
        int low(0), high(0), count(0);
        
        nt.GetPigBounds(& low, & high, & count);
        
        CHECK(low < high);
        CHECK(count);
    } catch(CSeqDBException &) {
        caught_exception = true;
    }
    
    if (! caught_exception) {
        BOOST_ERROR("ExpertIdBoundsNoPig() did not throw an exception of type  CSeqDBException.");
    }
}

BOOST_AUTO_TEST_CASE(ResolveDbPath)
{
    START;
    
    typedef pair<bool, string> TStringBool;
    typedef vector< TStringBool > TStringBoolVec;
    
    TStringBoolVec paths;
    paths.push_back(TStringBool(true, "nt.00.nin"));
    paths.push_back(TStringBool(true, "Trace/Zea_mays_EST.nal"));
    paths.push_back(TStringBool(true, "taxdb.bti"));
    paths.push_back(TStringBool(true, "data/seqp.pin"));
    paths.push_back(TStringBool(false, "nr.00")); // missing extension 
    
    // Try to resolve each of the above paths.
    
    ITERATE(TStringBoolVec, iter, paths) {
        string filename = iter->second;
        string resolved = SeqDB_ResolveDbPath(filename);
        bool found = ! resolved.empty();
        
        if (iter->first) {
            int position = resolved.find(filename);
            
            // Should be found.
            CHECK(found);
            
            // Resolved names are longer.
            CHECK(resolved.size() > filename.size());
            
            // Filename must occur at end of resolved name.
            CHECK_EQUAL(position + filename.size(), resolved.size());
        } else {
            CHECK(! found);
        }
    }
}

BOOST_AUTO_TEST_CASE(GlobalMemoryBound)
{
    START;
    
    // No real way to test what this does, so I just check that I can
    // call the method and build a SeqDB object.
    
    CSeqDB::SetDefaultMemoryBound(512 << 20);
    CSeqDB db("wgs", CSeqDB::eNucleotide);
}

class CSimpleGiList : public CSeqDBGiList {
public:
    CSimpleGiList(const vector<int> & gis)
    {
        for(size_t i = 0; i < gis.size(); i++) {
            m_GisOids.push_back(gis[i]);
        }
    }
};

BOOST_AUTO_TEST_CASE(IntersectionGiList)
{
    START;
    
    vector<int> a3; // multiples of 3 from 0..500
    vector<int> a5; // multiples of 5 from 0..500
    
    // The number 41 is added to the front of one set and the end of
    // the other to verify that the code computing the intersection
    // correctly sorts its inputs.
    
    int special = 41;
    
    // Add to start of a3
    a3.push_back(special);
    
    for(int i = 0; (i*3) < 500; i++) {
        a3.push_back(i*3);
        
        if (i*5 < 500) {
            a5.push_back(i*5);
        }
    }
    
    // Add to end of a5
    a5.push_back(special);
    
    CSimpleGiList gi3(a3);
    
    // Intersection == multiples of 15.
    CIntersectionGiList both(gi3, a5);
    
    for(int i = 0; i < 500; i++) {
        if (((i % 15) == 0) || (i == special)) {
            CHECK(true == both.FindGi(i));
        } else {
            CHECK(false == both.FindGi(i));
        }
    }
}

BOOST_AUTO_TEST_CASE(ComputedList)
{
    START;
    
    vector<int> a3; // multiples of 3 from 0..500
    vector<int> a5; // multiples of 5 from 0..500
    
    // The number 41 is added to the front of one set and the end of
    // the other to verify that the code computing the intersection
    // correctly sorts its inputs.
    
    int special = 41;
    
    // Add to start of a3
    a3.push_back(special);
    
    for(int i = 0; (i*3) < 500; i++) {
        a3.push_back(i*3);
        
        if (i*5 < 500) {
            a5.push_back(i*5);
        }
    }
    
    // Add to end of a5
    a5.push_back(special);
    
    CRef<CSeqDBIdSet> calc(new CSeqDBIdSet(a3, CSeqDBIdSet::eGi));
    
    // X and not Y operation : multiples of 3 (or 41) that aren't
    // multiples of 5 (or 41).
    
    calc->Compute(CSeqDBIdSet::eAnd, a5, false);
    
    CHECK(calc->IsPositive());
    CRef<CSeqDBGiList> and_not = calc->GetPositiveList();
    
    for(int i = 0; i < 500; i++) {
        bool is_3 = ((i % 3) == 0) || (i == special);
        bool is_5 = ((i % 5) == 0) || (i == special);
        
        if (is_3 && (! is_5)) {
            CHECK(true == and_not->FindGi(i));
        } else {
            CHECK(false == and_not->FindGi(i));
        }
    }
}

BOOST_AUTO_TEST_CASE(ComplexComputedList)
{
    START;
    
    vector<int> m2; // multiples of 2
    vector<int> m3; // multiples of 3
    vector<int> m5; // multiples of 5
    vector<int> m7; // multiples of 7
    
    // The number 11 is is added to the beginning and end of each list
    // to insure that all lists are sorted and uniqued properly.
    
    int special = 11;
    
    m2.push_back(special);
    m3.push_back(special);
    m5.push_back(special);
    m7.push_back(special);
    
    for(int i = 0; i < 1000; i++) {
        if (! (i % 2)) {
            m2.push_back(i);
        }
        if (! (i % 3)) {
            m3.push_back(i);
        }
        if (! (i % 5)) {
            m5.push_back(i);
        }
        if (! (i % 7)) {
            m7.push_back(i);
        }
    }
    
    m2.push_back(special);
    m3.push_back(special);
    m5.push_back(special);
    m7.push_back(special);
    
    //----------------------------------------
    // c1: (m2 AND NOT m3) OR (m5 AND NOT m7)
    
    CSeqDBIdSet c1(m2, CSeqDBIdSet::eGi);
    c1.Compute(CSeqDBIdSet::eAnd, m3, false);
    
        CSeqDBIdSet m5_not_m7(m5, CSeqDBIdSet::eGi);
        m5_not_m7.Compute(CSeqDBIdSet::eAnd, m7, false);
        
        c1.Compute(CSeqDBIdSet::eOr, m5_not_m7);
    
    //----------------------------------------
    // c2: (NOT m2 OR m3) AND (m5 XOR m7)
    
    CSeqDBIdSet c2(m2, CSeqDBIdSet::eGi, false);
    c2.Compute(CSeqDBIdSet::eOr, m3);
        
    CSeqDBIdSet m5_xor_m7(m5, CSeqDBIdSet::eGi);
    m5_xor_m7.Compute(CSeqDBIdSet::eXor, m7);
    
    c2.Compute(CSeqDBIdSet::eAnd, m5_xor_m7);

    //----------------------------------------
    // c3: (m2 OR NOT m3) AND (NOT m5 OR NOT m7)
    
    CSeqDBIdSet c3(m2, CSeqDBIdSet::eGi);
    c3.Compute(CSeqDBIdSet::eOr, m3, false);
    
    CHECK(! c3.IsPositive());
    
    CSeqDBIdSet not_m5_ornot_m7(m5, CSeqDBIdSet::eGi, false);
    not_m5_ornot_m7.Compute(CSeqDBIdSet::eOr, m7, false);
    
    CHECK(! not_m5_ornot_m7.IsPositive());
    c3.Compute(CSeqDBIdSet::eAnd, not_m5_ornot_m7);
    
    CHECK(! c3.IsPositive());
    
    // check lists.
    
    CRef<CSeqDBGiList> c1p, c2p;
    CRef<CSeqDBNegativeList> c3n;
    
    CHECK(c1.IsPositive());
    CHECK(c2.IsPositive());
    CHECK(! c3.IsPositive());
    
    c1p = c1.GetPositiveList();
    c2p = c2.GetPositiveList();
    c3n = c3.GetNegativeList();
    
    for(int i = 0; i < 1000; i++) {
        bool d2(!(i%2)), d3(!(i%3)), d5(!(i%5)), d7(!(i%7));
        
        if (i == special) {
            d2 = d3 = d5 = d7 = true;
        }
        
        // c1: (m2 AND NOT m3) OR (m5 AND NOT m7)
        // c2: (NOT m2 OR m3) AND (m5 XOR m7)
        // c3: (m2 OR NOT m3) AND (NOT m5 OR NOT m7)
        
        bool in_c1 = ( d2 && !d3) || ( d5 && !d7);
        bool in_c2 = (!d2 ||  d3) && ( d5 !=  d7);
        bool in_c3 = ( d2 || !d3) && (!d5 || !d7);
        
        CHECK_EQUAL(in_c1,   c1p->FindGi(i));
        CHECK_EQUAL(in_c2,   c2p->FindGi(i));
        CHECK_EQUAL(in_c3, ! c3n->FindGi(i));
    }
}

static bool s_DbHasOID(CSeqDB & db, int & count, int oid)
{
    int oid2 = oid;
    bool have = db.CheckOrFindOID(oid) && (oid == oid2);
    
    if (have) {
        count++;
    }
    
    return have;
}

BOOST_AUTO_TEST_CASE(ComputedListFilter)
{
    START;
    
    int v1[] = {
        46071115, 46071116, 46071117, 46071118, 46071119,
        46071120, 46071121, 46071122, 46071123, 46071124,
        46071125, 46071126, 46071127, 46071128, 46071129,
        46071130, 46071131, 46071132, 46071133, 46071134 };
    
    CHECK((sizeof(v1)/sizeof(int)) == 20);
    
    vector<int> all(v1, v1 + 20);
    vector<int> mid(v1 + 5, v1 + 15);
    
    CSeqDBIdSet All(all, CSeqDBIdSet::eGi);
    CSeqDBIdSet Mid(mid, CSeqDBIdSet::eGi);
    CSeqDBIdSet Neg(all, CSeqDBIdSet::eGi, false);
    
    CSeqDBIdSet TopBot(all, CSeqDBIdSet::eGi);
    TopBot.Compute(CSeqDBIdSet::eAnd, mid, false);
    
    // Compute inverse of TopBot, using a different sequence.
    
    CSeqDBIdSet NotTopBot(all, CSeqDBIdSet::eGi, false);
    
    NotTopBot.Compute(CSeqDBIdSet::eOr, mid);
    
    string nm = "data/seqn";
    CSeqDB::ESeqType ty = CSeqDB::eNucleotide;
    
    CSeqDB seqn(nm, ty);
    
    CSeqDB db_A(nm, ty, All);
    CSeqDB db_M(nm, ty, Mid);
    CSeqDB db_N(nm, ty, Neg);
    CSeqDB db_TB(nm, ty, TopBot);
    CSeqDB db_NTB(nm, ty, NotTopBot);
    
    int A_count = 0;
    int M_count = 0;
    int N_count = 0;
    int TB_count = 0;
    int NTB_count = 0;
    
    for(int oid = 0; seqn.CheckOrFindOID(oid); oid++) {
        bool A_have   = s_DbHasOID(db_A, A_count, oid);
        bool M_have   = s_DbHasOID(db_M, M_count, oid);
        bool N_have   = s_DbHasOID(db_N, N_count, oid);
        bool TB_have  = s_DbHasOID(db_TB, TB_count, oid);
        bool NTB_have = s_DbHasOID(db_NTB, NTB_count, oid);
        
        CHECK((! M_have) || A_have);   // M -> A (implies)
        CHECK(A_have != N_have);       // A = ! N
        CHECK((! TB_have) || A_have);  // TB -> A
        
        CHECK((!M_have) || (!N_have));  // M -> !N
        CHECK((!M_have) || (!TB_have)); // M -> !TB
        CHECK((!M_have) || NTB_have);   // M -> NTB
        
        CHECK((!N_have) || (!TB_have)); // N -> !TB
        CHECK((!N_have) || NTB_have);   // N -> NTB
        
        CHECK(TB_have != NTB_have);    // TB != NTB
    }
    
    int NSEQ = seqn.GetNumOIDs();
    
    CHECK_EQUAL(NSEQ, 100);
    
    CHECK_EQUAL(A_count, 20);
    CHECK_EQUAL(M_count, 10);
    CHECK_EQUAL(N_count, NSEQ-A_count);
    CHECK_EQUAL(TB_count, A_count - M_count);
    CHECK_EQUAL(NTB_count + TB_count, 100);
    
    CSeqDBIdSet idset_TB = db_TB.GetIdSet();
    
    CHECK(! idset_TB.Blank());
}

BOOST_AUTO_TEST_CASE(SharedMemoryMaps)
{
    START;
    
    CSeqDB seqdb1("nt", CSeqDB::eNucleotide);
    CSeqDB seqdb2("nt", CSeqDB::eNucleotide);
    
    const char *s1 = 0, *s2 = 0;
    
    seqdb1.GetSequence(0, & s1);
    seqdb2.GetSequence(0, & s2);
    
    try {
        CHECK(s1 == s2);
    }
    catch(...) {
        if (s1)
            seqdb1.RetSequence(& s1);
        if (s2)
            seqdb2.RetSequence(& s2);
        throw;
    }
    
    if (s1)
        seqdb1.RetSequence(& s1);
    if (s2)
        seqdb2.RetSequence(& s2);
}

class CSeqIdList : public CSeqDBGiList {
public:
    // Takes a NULL-terminated list of null-terminated strings.  If these
    // start with '#' they are treated as GIs for the GI list; otherwise they
    // go in the Seq-id list.
    CSeqIdList(const char ** str)
    {
        for(const char ** p = str; *p; p++) {
            if ((*p)[0] == '#') {
                m_GisOids.push_back(atoi((*p) + 1));
            } else {
                m_SeqIdsOids.push_back(new CSeq_id(*p));
            }
        }
    }
    
    CSeqIdList()
    {
    }
    
    void Append(const char * p)
    {
        m_SeqIdsOids.push_back(new CSeq_id(p));
    }
};

BOOST_AUTO_TEST_CASE(SeqIdList)
{
    START;
    
    const char * str[] =
        { "EAL14780.1",
          "BAB38329.1",
          "P66272",
          "NP_854766.1",
          "NP_688815.1",
          "ZP_00714951.1",
          "BAB37428.1",
          "AAG07133.1",
          "YP_651583.1",
          "XP_645408.1",
          NULL };
    
    CRef<CSeqIdList> ids(new CSeqIdList(str));
    
    CHECK_EQUAL((int)ids->GetNumSeqIds(), 10);
    
    // Check that all IDs are initially unresolved:
    
    for(int i = 0; i < ids->GetNumSeqIds(); i++) {
        CHECK(ids->GetSeqIdOid(i).oid == -1);
    }
    
    // Check that SeqDB construction has resolved all IDs:
    
    CSeqDB db("nr", CSeqDB::eProtein, &*ids);
    
    for(int i = 0; i < ids->GetNumSeqIds(); i++) {
        CHECK(ids->GetSeqIdOid(i).oid != -1);
    }
    
    // Check that the set of returned ids is constrained to the same
    // size as the SeqIdList set.
    
    int k = 0;
    
    for(int i = 0; db.CheckOrFindOID(i); i++) {
        k += db.GetHdr(i)->Get().size();
    }
    
    CHECK_EQUAL(k, ids->GetNumSeqIds());
}


BOOST_AUTO_TEST_CASE(SeqIdListAndGiList)
{
    START;
    
    const char * str[] = {
        // Non-existant (fake):
        "ref|XP_12345.1|", // s0-2
        "gi|11223344|",
        "gb|EAH98765.9|",
        "#123456",         // g0,1
        "#3142007",
        
        // GIs found in volume but not volume list:
        "gi|38083732",  // s3-5
        "gi|671595|",
        "gi|43544756|",
        "#45917153",    // gi2,3
        "#15705575",
        
        // Non-GIs found in volume but not volume list:
        "ref|NP_912855.1|", // s6-10
        "gb|EAF49211.1|",
        "sp|Q63931|CCKR_CAVPO",
        "emb|CAE61105.1|",
        "gb|AAL05711.1|",  // Note: same as "#15705575"
        
        // GIs Found in volume and volume list:
        "gi|28378617",  // s11-13
        "gi|23474175",
        "gi|27364740",
        "#23113886",    // gi4,5
        "#28563952",
        
        // Non-GIs Found in volume and volume list:
        "gb|AAP03339.1|",    // s14-18
        "ref|NP_760268.1|",
        "ref|NP_817911.1|",
        "emb|CAD70761.1|",
        "gb|AAM45611.1|",
        NULL
    };
    
    CRef<CSeqIdList> ids(new CSeqIdList(str));
    
    // (Need to +1 for the terminating NULL.)
    CHECK_EQUAL((int)ids->GetNumSeqIds(), 19);
    CHECK_EQUAL((int)ids->GetNumGis(), 6);
    
    // Check that all IDs are initially unresolved:
    int i;
    
    for(i = 0; i < ids->GetNumSeqIds(); i++) {
        CHECK(ids->GetSeqIdOid(i).oid == -1);
    }
    for(i = 0; i < ids->GetNumGis(); i++) {
        CHECK(ids->GetGiOid(i).oid == -1);
    }
    
    CSeqDB db("data/ranges/twenty", CSeqDB::eProtein, &*ids);
    
    // Check that SeqDB construction resolves needed GIs/Seq-ids, but does not
    // resolve fake ids; other ids can be resolved or not discretionally.
    
    for(i = 0; str[i]; i++) {
        bool found = false;
        int oid = -1;
        
        if (str[i][0] == '#') {
            int gi = atoi(str[i] + 1);
            found = ids->GiToOid(gi, oid);
        } else {
            CSeq_id seqid(str[i]);
            found = ids->SeqIdToOid(seqid, oid);
        }
        
        CHECK_EQUAL(found, true);
        
        if (i >= 0 && i < 4) {
            CHECK_EQUAL(oid, -1);
        } else if (i >= 15 && i < 25) {
            if (oid == -1) {
                cout << "oid = -1, id=" << str[i] << endl;
            }
            
            CHECK(oid != -1);
        }
    }
    
    // Set of Seq-ids that we want: the Seq-ids found in the deflines that are
    // the intersection of the deflines associated with the Seq-ids in each of
    // the user and volume GI lists.
    
    const char * inter[] = {
        // This is the set of all Seq-ids that should be found on iteration;
        // it includes all Seq-ids from the selected deflines.  A defline is
        // selected if it has one or more GIs matching a database volume GI
        // list and one or more GIs or Seq-ids from the User GI List.
        
        "gi|28378617", "ref|NP_785509.1|",
        "gi|23474175", "ref|ZP_00129469.1|",
        "gi|27364740", "ref|NP_760268.1|",
        "gi|23113886", "ref|ZP_00099225.1|",
        "gi|28563952", "ref|NP_788261.1|",
        "gi|29788717", "gb|AAP03339.1|",
        "gi|29566344", "ref|NP_817911.1|",
        "gi|28950006", "emb|CAD70761.1|",
        "gi|21305377", "gb|AAM45611.1|",
        NULL
    };
    
    set<string> need;
    
    for(const char ** p = inter; *p; p++)
        need.insert(*p);
    
    // For each id found in iteration, verify that it is found in the "need"
    // list and then remove it from that list.
    
    for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
        typedef list< CRef<CSeq_id> > TIds;
        
        TIds the_ids = db.GetSeqIDs(oid);
        
        ITERATE(TIds, iter, the_ids) {
            CRef<CSeq_id> seqid(*iter);
            string afs = seqid->AsFastaString();
            set<string>::iterator itr = need.find(afs);
            CHECK(itr != need.end());
            need.erase(itr);
        }
    }
    
    // We should have emptied the 'need' set at this point.
    
    CHECK(need.empty());
}


BOOST_AUTO_TEST_CASE(EmptyVolume)
{
    START;
    
    CSeqDB db("data/empty", CSeqDB::eProtein);
    
    CHECK_EQUAL(db.GetNumSeqs(), 0);
    CHECK_EQUAL(db.GetNumOIDs(), 0);
    CHECK_EQUAL((string)db.GetTitle(), string("empty test database"));
    
    CHECK_THROW_SEQDB(db.GetSeqLength(0));
    CHECK_THROW_SEQDB(db.GetSeqLengthApprox(0));
    CHECK_THROW_SEQDB(db.GetHdr(0));
    
    map<int, int> gi_to_taxid;
    vector<int>   taxids;
    vector<int>   gis;
    
    CHECK_THROW_SEQDB(db.GetTaxIDs(0, gi_to_taxid));
    CHECK_THROW_SEQDB(db.GetTaxIDs(0, taxids));
    CHECK_THROW_SEQDB(db.GetBioseq(0));
    CHECK_THROW_SEQDB(db.GetBioseqNoData(0, 129295));
    CHECK_THROW_SEQDB(db.GetBioseq(0, 129295));
    
    const char * buffer = 0;
    char * ncbuffer = 0;
    
    CHECK_THROW_SEQDB(db.GetSequence(0, & buffer));
    CHECK_THROW_SEQDB(db.GetAmbigSeq(0, & buffer, kSeqDBNuclBlastNA8));
    CHECK_THROW_SEQDB(db.GetAmbigSeq(0, & buffer, kSeqDBNuclBlastNA8, 10, 20));
    CHECK_THROW_SEQDB(db.GetAmbigSeqAlloc(0,
                                          & ncbuffer,
                                          kSeqDBNuclBlastNA8,
                                          eAtlas));
    
    // Don't check CSeqDB::RetSequence, because it uses an assert(),
    // which is more helpful from a debugging POV.
    
    CHECK_THROW_SEQDB(db.GetSeqIDs(0));
    CHECK_THROW_SEQDB(db.GetGis(0, gis));
    CHECK_EQUAL(db.GetSequenceType(), CSeqDB::eProtein);
    CHECK_EQUAL((string)db.GetTitle(), string("empty test database"));
    CHECK_EQUAL((string)db.GetDate(), string("Mar 19, 2007 11:38 AM"));
    CHECK_EQUAL(db.GetNumSeqs(), 0);
    CHECK_EQUAL(db.GetNumOIDs(), 0);
    CHECK_EQUAL(db.GetTotalLength(), Uint8(0));
    CHECK_EQUAL(db.GetVolumeLength(), Uint8(0));
    
    int oid_count = 0;
    Uint8 seq_total = 0;
    
    CHECK_NO_THROW(db.GetTotals(CSeqDB::eUnfilteredAll,
                                & oid_count,
                                & seq_total,
                                false));
    
    CHECK_EQUAL(oid_count, 0);
    CHECK_EQUAL(seq_total, Uint8(0));
    
    CHECK_EQUAL(db.GetMaxLength(), 0);
    CHECK_NO_THROW(db.Begin());
    
    int oid = 0;
    
    CHECK_EQUAL(false, db.CheckOrFindOID(oid));
    
    int begin(0), end(0);
    vector<int> oids;
    oids.resize(100);
    
    CSeqDB::EOidListType ol_type = CSeqDB::eOidList;
    CHECK_NO_THROW(ol_type = db.GetNextOIDChunk(begin, end, oids, NULL));
    
    if (ol_type == CSeqDB::eOidList) {
        CHECK_EQUAL(size_t(0), oids.size());
    } else {
        CHECK_EQUAL(begin, end);
    }
    
    CHECK_NO_THROW(db.ResetInternalChunkBookmark());
    CHECK_EQUAL((string)db.GetDBNameList(), string("data/empty"));
    CHECK_EQUAL(db.GetGiList(), (CSeqDBGiList*)NULL);
    CHECK_NO_THROW(db.SetMemoryBound(1024*1024*512));
    
    int pig(123), gi(129295);
    string acc("P01013");
    CSeq_id seqid("sp|P01013|OVALX_CHICK");
    
    // This looks assymetric, but its logically consistent.  Looking
    // up a non-existant GI, PIG, or Seq-id always returns a failure,
    // but never throws an exception.  Since OIDs must be in range,
    // the OidToXyz functions will all throw exceptions (there are no
    // valid OIDs for an empty db).
    
    CHECK_THROW_SEQDB(db.OidToPig(oid, pig));
    CHECK_THROW_SEQDB(db.OidToGi(oid, gi));
    
    CHECK_EQUAL(false, db.PigToOid(pig, oid));
    CHECK_EQUAL(false, db.GiToOid(gi, oid));
    CHECK_EQUAL(false, db.GiToPig(gi, pig));
    CHECK_EQUAL(false, db.PigToGi(pig, gi));
    CHECK_NO_THROW(db.AccessionToOids(acc, oids));
    CHECK(oids.size() == 0);
    CHECK_NO_THROW(db.SeqidToOids(seqid, oids));
    CHECK(oids.size() == 0);
    CHECK_EQUAL(false, db.SeqidToOid(seqid, oid));
    
    Uint8 residue(12345);
    
    // GetOidAtOffset() must throw.  The specified starting OID must
    // be valid (and of course can't be, for an empty DB.)
    
    CHECK_THROW_SEQDB(db.GetOidAtOffset(0, residue));
    CHECK(db.GiToBioseq(gi).Empty());
    CHECK(db.PigToBioseq(pig).Empty());
    CHECK(db.SeqidToBioseq(seqid).Empty());
    
    vector<string> paths1;
    vector<string> paths2;
    
    CHECK_NO_THROW(CSeqDB::FindVolumePaths("data/empty",
                                           CSeqDB::eProtein,
                                           paths1));
    
    CHECK_NO_THROW(db.FindVolumePaths(paths2));
    
    CHECK_EQUAL(paths1.size(), size_t(1));
    CHECK_EQUAL(paths2.size(), size_t(1));
    CHECK_EQUAL((string)paths1[0], (string)paths2[0]);
    
    // The end OID is higher than GetNumOIDs(), but as stated in the
    // documentation, this function silently adjusts the end value to
    // the number of OIDs if it is out of range.
    
    CHECK_NO_THROW(db.SetIterationRange(0, 100));
    
    CSeqDB::TAliasFileValues afv;
    CHECK_NO_THROW(db.GetAliasFileValues(afv));
    
    int taxid(57176);
    
    // An empty database should still be able to look up the
    // vociferans taxid.
    
    SSeqDBTaxInfo info;
    CHECK_NO_THROW(db.GetTaxInfo(taxid, info));
    
    CHECK_THROW_SEQDB(db.GetSeqData(0, 10, 20));
}

BOOST_AUTO_TEST_CASE(OidRewriting)
{
    START;
    
    CSeqDB db56("data/f555 data/f556", CSeqDB::eNucleotide);
    CSeqDB db65("data/f556 data/f555", CSeqDB::eNucleotide);
    
    for(int di = 0; di < 2; di++) {
        CSeqDB & db = di ? db65 : db56;
        
        for(int oi = 0; oi < 2; oi++) {
            list< CRef<CSeq_id> > ids = db.GetSeqIDs(oi);
            
            int count = 0;
            int oid = -1;
            
            while(! ids.empty()) {
                const CSeq_id & id = *ids.front();
                
                if (id.Which() == CSeq_id::e_General &&
                    id.GetGeneral().GetDb() == "BL_ORD_ID") {
                    
                    oid = id.GetGeneral().GetTag().GetId();
                    count ++;
                }
                
                ids.pop_front();
            }
            
            CHECK(count == 1);
            CHECK(oid == oi);
        }
    }
}

BOOST_AUTO_TEST_CASE(GetSequenceAsString)
{
    START;
    
    CSeqDB N("data/seqn", CSeqDB::eNucleotide);
    CSeqDB P("data/seqp", CSeqDB::eProtein);
    
    string nucl, prot;
    
    int nucl_gi = 46071107;
    string nucl_str = ("AAGCTCTTCATTGATGGTAGAGAGCCTATTAACAGGCAAC"
                       "AGTCAATGCTCCAAAGTCCAAACAAGATTACCTGTGCAAA"
                       "GAACTTGCAGTGTAACAAACCCCNTTCACGGCCAGAAGTA"
                       "TTTGCAACAATGTTGAAAGTCCTTCTGGCAGAGGAGGAGT"
                       "CTAAT");
    
    int prot_gi = 43914529;
    string prot_str = "MINKSGYEAKYKKSIKNNEEFWRKEGKRITWIKPYKKIKNVRYS";
    
    int nucl_oid(-1), prot_oid(-1);
    
    N.GiToOid(nucl_gi, nucl_oid);
    P.GiToOid(prot_gi, prot_oid);
    
    string nstr, pstr;
    N.GetSequenceAsString(nucl_oid, nstr);
    P.GetSequenceAsString(prot_oid, pstr);
    
    CHECK_EQUAL((string)nstr, (string)nucl_str);
    CHECK_EQUAL((string)pstr, (string)prot_str);
}

BOOST_AUTO_TEST_CASE(TotalLengths)
{
    START;
    
    // Test both constructors; make sure sizes are equal and non-zero.
    
    CSeqDB local("data/totals", CSeqDB::eNucleotide);
    CSeqDB seqn("data/seqn", CSeqDB::eNucleotide);
    
    CHECK_EQUAL((int)local.GetTotalLength(),      12345);
    CHECK_EQUAL((int)local.GetTotalLengthStats(), 23456);
    CHECK_EQUAL((int)local.GetNumSeqs(),          123);
    CHECK_EQUAL((int)local.GetNumSeqsStats(),     234);
    CHECK_EQUAL((int)seqn.GetNumSeqsStats(),      0);
    CHECK_EQUAL((int)seqn.GetTotalLengthStats(),  0);
}

class CNegativeIdList : public CSeqDBNegativeList {
public:
    CNegativeIdList(const int * ids, bool use_tis)
    {
        while(*ids) {
            if (use_tis) {
                m_Tis.push_back(*ids);
            } else {
                m_Gis.push_back(*ids);
            }
            ++ ids;
        }
    }
    
    ~CNegativeIdList()
    {
    }
};

static void s_ModifyMap(map<int,int> & m, int key, int c, int & total)
{
    int & amt = m[key];
    amt += c;
    total += c;
    
    if (! amt) {
        m.erase(key);
    }
}

static void s_MapAllGis(CSeqDB       & db,
                        map<int,int> & m,
                        int            change,
                        int          & total)
{
    total = 0;
    vector<int> gis;
    
    for(int oid = 0; db.CheckOrFindOID(oid); oid++) {
        gis.clear();
        
        db.GetGis(oid, gis, false);
        
        ITERATE(vector<int>, iter, gis) {
            s_ModifyMap(m, *iter, change, total);
        }
    }
}

BOOST_AUTO_TEST_CASE(NegativeGiList)
{
    START;
    
    // 15 ids from the middle of the seqp database.
    
    int gis[] = {
        23058829,
        9910844,
        23119763,
        7770223,
        15705575,
        9651810,
        27364740,
        23113886,
        21593385,
        15217498,
        39592435,
        22126577,
        44281419,
        14325807,
        15605992,
        0
    };
    
    int seqp_gis = 146;
    int nlist_gis = 15;
    
    CRef<CSeqDBNegativeList> neg(new CNegativeIdList(gis, false));
    
    CSeqDB have_got("data/seqp", CSeqDB::eProtein);
    CSeqDB have_not("data/seqp", CSeqDB::eProtein, &* neg);
    
    CHECK_EQUAL((int)have_got.GetTotalLength(), 26945);
    CHECK_EQUAL((int)have_got.GetNumSeqs(),     100);
    
    // From 100 original OIDs, 15 GIs were removed, but 4 of the OIDs
    // had multiple deflines, leaving a final count of 89 OIDs.
    
    CHECK_EQUAL((int)have_not.GetTotalLength(), 23602);
    CHECK_EQUAL((int)have_not.GetNumSeqs(),     89);
    
    map<int, int> id_pop;
    
    int total = 0;
    
    // Add all 'negated' IDs to the map; verify that the map size is
    // correct.
    
    for(int * idp = gis; *idp; ++idp) {
        s_ModifyMap(id_pop, *idp, 1, total);
    }
    
    CHECK_EQUAL((int) id_pop.size(), nlist_gis);
    CHECK_EQUAL(total, nlist_gis);
    
    // Add all filtered IDs to the map; verify that the map size is
    // correct and that the total change is seqp_gis-nlist_gis
    
    s_MapAllGis(have_not, id_pop, 1, total);
    
    CHECK_EQUAL((int) id_pop.size(), seqp_gis);
    CHECK_EQUAL(total, seqp_gis-nlist_gis);
    
    // Remove all unfiltered IDs from the map; the result should be a
    // negative change of (the number of gis in seqp) and cause the
    // map to be empty.  This verifies that the negative GI list and
    // the set of GIs in the filtered DB are an exact partition of the
    // unfiltered database.
    
    s_MapAllGis(have_got, id_pop, -1, total);
    
    CHECK_EQUAL((int) id_pop.size(), 0);
    CHECK_EQUAL(total, -seqp_gis);
    
    // One last thing: since there is some non-redundancy in the seqp
    // database, I want to check that it affects the header data that
    // is reported from SeqDB::GetHdr().
    
    int gi1 = 27360885;
    int oid1 = -1;
    
    bool ok = have_got.GiToOid(gi1, oid1);
    CHECK(ok);
    
    list< CRef<CSeq_id> > got_ids = have_got.GetSeqIDs(oid1);
    list< CRef<CSeq_id> > not_ids = have_not.GetSeqIDs(oid1);
    
    int diff = 0;
    
    ITERATE(list< CRef<CSeq_id> >, iter, got_ids) {
        diff ++;
    }
    ITERATE(list< CRef<CSeq_id> >, iter, not_ids) {
        diff --;
    }
    CHECK_EQUAL(diff, 2);
}

BOOST_AUTO_TEST_CASE(NegativeListNr)
{
    START;
    
    int gis[] = {
        129296, 0
    };
    
    CRef<CSeqDBNegativeList> neg(new CNegativeIdList(gis, false));
    
    string db = "nr";
    
    CSeqDB have_got(db, CSeqDB::eProtein);
    CSeqDB have_not(db, CSeqDB::eProtein, &* neg);
    
    CHECK_EQUAL(have_got.GetTotalLength(), have_not.GetTotalLength());
    CHECK_EQUAL(have_got.GetNumSeqs(),     have_not.GetNumSeqs());
    
    int oid = -1;
    bool found = have_got.GiToOid(gis[0], oid);
    CHECK(found);
    
    vector<int> gis_w, gis_wo;
    have_got.GetGis(oid, gis_w);
    have_not.GetGis(oid, gis_wo);
    
    // Check that exactly 1 GI was removed.
    
    int count_w = (int) gis_w.size();
    int count_wo = (int) gis_wo.size();
    CHECK_EQUAL(count_w, (count_wo+1));
}

BOOST_AUTO_TEST_CASE(NegativeListSwissprot)
{
    START;
    
    // 1 id from the swissprot database.
    
    int gis[] = {
        81723792, 0
    };
    
    CRef<CSeqDBNegativeList> neg(new CNegativeIdList(gis, false));
    
    string db = "swissprot";
    
    CSeqDB have_got(db, CSeqDB::eProtein);
    CSeqDB have_not(db, CSeqDB::eProtein, &* neg);
    
    CHECK_EQUAL(have_got.GetTotalLength(), have_not.GetTotalLength());
    CHECK_EQUAL(have_got.GetNumSeqs(),     have_not.GetNumSeqs());
    
    int oid = -1;
    bool found = have_got.GiToOid(gis[0], oid);
    CHECK(found);
    
    vector<int> gis_w, gis_wo;
    have_got.GetGis(oid, gis_w);
    have_not.GetGis(oid, gis_wo);
    
    // Check that exactly 1 GI was removed.
    
    int count_w = (int) gis_w.size();
    int count_wo = (int) gis_wo.size();
    CHECK_EQUAL(count_w, (count_wo+1));
}

BOOST_AUTO_TEST_CASE(HashToOid)
{
    START;
    
    CSeqDBExpert seqp("data/seqp", CSeqDB::eProtein);
    CSeqDBExpert seqn("data/seqn", CSeqDB::eNucleotide);
    
    int oid(0);
    
    for(oid = 0; oid < 10 && seqp.CheckOrFindOID(oid); oid++) {
        unsigned h = seqp.GetSequenceHash(oid);
        
        vector<int> oids;
        seqp.HashToOids(h, oids);
        
        bool found = false;
        
        ITERATE(vector<int>, iter, oids) {
            if (*iter == oid) {
                found = true;
                break;
            }
        }
        
        CHECK(found);
    }
    
    for(oid = 0; oid < 10 && seqn.CheckOrFindOID(oid); oid++) {
        unsigned h = seqn.GetSequenceHash(oid);
        
        vector<int> oids;
        seqn.HashToOids(h, oids);
        
        bool found = false;
        
        ITERATE(vector<int>, iter, oids) {
            if (*iter == oid) {
                found = true;
                break;
            }
        }
        
        CHECK(found);
    }
}

BOOST_AUTO_TEST_CASE(FilteredHeaders)
{
    START;
    
    CSeqDB p1("nr", CSeqDB::eProtein);
    CSeqDB p2("refseq_protein", CSeqDB::eProtein);
    
    // Use a pig in case of GI evaporation.
    
    int pig = 1401930;
    
    int oid1(-1), oid2(-1);
    bool okay1 = p1.PigToOid(pig, oid1);
    bool okay2 = p2.PigToOid(pig, oid2);
    
    CHECK(okay1);
    CHECK(okay2);
    CHECK(oid1 > 0);
    CHECK(oid2 > 0);
    CHECK(oid1 == oid2); // same underlying volumes -> same OID
    
    int size1 = p1.GetHdr(oid1)->Get().size();
    int size2 = p2.GetHdr(oid2)->Get().size();
    
    // Currently there are 15 matching GIs in nr, and only one in
    // refseq_protein.  This can drift over time (in either direction)
    // so the criteria here are less strict; I'm assuming we will gain
    // at least one redundant GI for each GI that evaporates.  I'm
    // also assuming that at least 5 more redundant GIs will exist
    // than we have proteins in refseq for this PIG.
    
    CHECK(size1);
    CHECK(size2);
    CHECK(size1 >= 14);
    CHECK(size1 > (size2 + 5));
}

static void s_CheckIdLookup(CSeqDB & db, const string & acc, size_t exp_oids, size_t exp_size)
{
    list<string> ids;
    NStr::Split(acc, ", ", ids);
    
    vector<int> oids;
    
    ostringstream fasta;
    CFastaOstream fos(fasta);
    fos.SetWidth(80);
    
    ITERATE(list<string>, iter, ids) {
        // For each ID, check that:
        // 1. SeqDB can find it.
        
        vector<int> tmp_oids;
        db.AccessionToOids(*iter, tmp_oids);
        
        BOOST_CHECK_MESSAGE(tmp_oids.size(),
                            string("No OIDs found for ")+(*iter));
        
        oids.insert(oids.end(), tmp_oids.begin(), tmp_oids.end());
    }
    
    // sort/unique
    
    sort(oids.begin(), oids.end());
    oids.erase(unique(oids.begin(), oids.end()), oids.end());
    
    ITERATE(vector<int>, iter, oids) {
        fos.Write(*db.GetBioseq(*iter));
    }
    
    string all_fasta = fasta.str();
    string msg = string("Error for accession: ") + acc;
    
    if (oids.size() != exp_oids) {
        cout << "\nacc: " << acc << ": #oids " << oids.size() << endl;
    }
    
    CHECK_EQUAL_MESSAGE_PRINT(msg, all_fasta.size(), exp_size);
    CHECK_EQUAL_MESSAGE_PRINT(msg, exp_oids, oids.size());
}

BOOST_AUTO_TEST_CASE(ProtOldTest)
{
    START;
    CSeqDB db("data/nrshort.old", CSeqDB::eUnknown);
    
    s_CheckIdLookup(db, "gi|67472376", 1, 6590);
    s_CheckIdLookup(db, "sp|P0A7U1|RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "sp||RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "sp|P0A7U1|", 1, 6590);
    s_CheckIdLookup(db, "P0A7U1", 1, 6590);
    s_CheckIdLookup(db, "RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "ref|NP_313205.1|", 1, 6590);
    s_CheckIdLookup(db, "NP_313205.1", 1, 6590);
    s_CheckIdLookup(db, "pir||AI1052", 1, 6590);
    s_CheckIdLookup(db, "AI1052", 1, 6590);
    s_CheckIdLookup(db, "NP_268346, XP_642837.1, 30262378, ABD21303.1", 4, 5411);
    s_CheckIdLookup(db, "pdb|1VS7|R", 1, 6590);
    s_CheckIdLookup(db, "1VS7", 1, 6590);
    s_CheckIdLookup(db, "prf||2202317B", 1, 628);
    s_CheckIdLookup(db, "2202317B", 1, 628);
    s_CheckIdLookup(db, "tr|Q4QBU6|Q4QBU6_LEIMA", 1, 609);
    s_CheckIdLookup(db, "Q4QBU6", 1, 609);
}

BOOST_AUTO_TEST_CASE(ProtTest)
{
    START;
    CSeqDB db("data/nrshort", CSeqDB::eUnknown);
    
    s_CheckIdLookup(db, "gi|67472376", 1, 6590);
    s_CheckIdLookup(db, "sp|P0A7U1|RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "sp||RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "sp|P0A7U1|", 1, 6590);
    s_CheckIdLookup(db, "P0A7U1", 1, 6590);
    s_CheckIdLookup(db, "RS18_SALTI", 1, 6590);
    s_CheckIdLookup(db, "ref|NP_313205.1|", 1, 6590);
    s_CheckIdLookup(db, "ref|NP_313205|", 1, 6590);
    s_CheckIdLookup(db, "NP_313205.1", 1, 6590);
    s_CheckIdLookup(db, "pir||AI1052", 1, 6590);
    s_CheckIdLookup(db, "AI1052", 1, 6590);
    s_CheckIdLookup(db, "NP_268346, XP_642837.1, 30262378, ABD21303.1", 4, 5411);
    s_CheckIdLookup(db, "pdb|1VS7|R", 1, 6590);
    s_CheckIdLookup(db, "1VS7", 1, 6590);
    s_CheckIdLookup(db, "prf||2202317B", 1, 628);
    s_CheckIdLookup(db, "2202317B", 1, 628);
    s_CheckIdLookup(db, "tr|Q4QBU6|Q4QBU6_LEIMA", 1, 609);
    s_CheckIdLookup(db, "Q4QBU6", 1, 609);
}

BOOST_AUTO_TEST_CASE(NuclOldTest)
{
    START;
    CSeqDB db("data/ntshort.old", CSeqDB::eUnknown);
    
    s_CheckIdLookup(db, "gi|2695850", 1, 683);
    s_CheckIdLookup(db, "2695850", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260.1|ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb||ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260.1|", 1, 683);
    s_CheckIdLookup(db, "gb|Y13260.1|ABY13260", 1, 683);
    s_CheckIdLookup(db, "Y13260.1", 1, 683);
    s_CheckIdLookup(db, "ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260|ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260|", 1, 683);
    s_CheckIdLookup(db, "gb|Y13260|ABY13260", 1, 683);
    s_CheckIdLookup(db, "Y13260", 1, 683);
    s_CheckIdLookup(db, "gnl|ti|43939557", 1, 972);
}

BOOST_AUTO_TEST_CASE(NuclTest)
{
    START;
    CSeqDB db("data/ntshort", CSeqDB::eUnknown);
    
    s_CheckIdLookup(db, "gi|2695850", 1, 683);
    s_CheckIdLookup(db, "2695850", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260.1|ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb||ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260.1|", 1, 683);
    s_CheckIdLookup(db, "gb|Y13260.1|ABY13260", 1, 683);
    s_CheckIdLookup(db, "Y13260.1", 1, 683);
    s_CheckIdLookup(db, "ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260|ABY13260", 1, 683);
    s_CheckIdLookup(db, "emb|Y13260|", 1, 683);
    s_CheckIdLookup(db, "gb|Y13260|ABY13260", 1, 683);
    s_CheckIdLookup(db, "Y13260", 1, 683);
    s_CheckIdLookup(db, "gnl|ti|43939557", 1, 972);
}

BOOST_AUTO_TEST_CASE(PdbIdWithChain)
{
    START;
    
    CSeqDB nr("nr", CSeqDB::eProtein);
    
    string acc("1QCFA");
    
    vector<int> oids;
    nr.AccessionToOids(acc, oids);
    
    CHECK(oids.size());
}

BOOST_AUTO_TEST_CASE(VersionedSparseId)
{
    START;
    
    CSeqDB db("data/sparse_id", CSeqDB::eNucleotide);
    
    string good("Z12841.1");
    string bad ("Z12842.1");
    string both("Z12843.1");
    
    vector<int> o1, o2, o3;
    db.AccessionToOids(good, o1);
    db.AccessionToOids(bad,  o2);
    db.AccessionToOids(both, o3);
    
    CHECK(o1.size() == 1);
    CHECK(o2.size() == 0);
    CHECK(o3.size() == 1);
}

struct SDbSumInfo {
public:
    SDbSumInfo(CSeqDB & db)
    {
        total_length    = db.GetVolumeLength();
        total_oids      = db.GetNumOIDs();
        
        filtered_length = db.GetTotalLength();
        filtered_oids   = db.GetNumSeqs();
        
        measured_length = 0;
        measured_oids   = 0;
        
        for(int oid=0; db.CheckOrFindOID(oid); oid++) {
            measured_length += db.GetSeqLength(oid);
            measured_oids ++;
        }
    }
    
    void CompareField(Int8 X, Int8 Y, string & sum, char ch)
    {
        if (X > Y) {
            sum.append("+");
        } else if (X < Y) {
            sum.append("-");
        } else {
            sum.append("=");
        }
        sum.push_back(ch);
    }
    
    string Compare(SDbSumInfo & other)
    {
        // This compares the fields for two databases to each other;
        // it tells us which database has more sequences and bases
        // before and after filtering.
        
        string sum;
        sum.reserve(20);
        
        CompareField(total_length,    other.total_length,    sum, 'T');
        CompareField(filtered_length, other.filtered_length, sum, 'F');
        CompareField(measured_length, other.measured_length, sum, 'M');
        
        CompareField(total_oids,    other.total_oids,    sum, 't');
        CompareField(filtered_oids, other.filtered_oids, sum, 'f');
        CompareField(measured_oids, other.measured_oids, sum, 'm');
        
        return sum;
    }
    
    string CompareSelf()
    {
        // This compares the fields within a database to each other;
        // it tells us if filtering has any effect and whether the
        // measured values equal the expected totals.
        
        string sum;
        sum.reserve(20);
        
        CompareField(total_length,    filtered_length, sum, 'A');
        CompareField(total_length,    measured_length, sum, 'B');
        CompareField(filtered_length, measured_length, sum, 'C');
        
        CompareField(total_oids,    filtered_oids, sum, 'a');
        CompareField(total_oids,    measured_oids, sum, 'b');
        CompareField(filtered_oids, measured_oids, sum, 'c');
        
        return sum;
    }
    
    /// Total length, sum of all volume lengths.
    Int8 total_length;
    
    /// Filtered length, result of all filtering.
    Int8 filtered_length;
    
    /// Measured length should equal filtered if alias files are correct.
    Int8 measured_length;
    
    /// Total oid count, sum of all volume oid counts.
    int total_oids;
    
    /// Filtered oid count, result of all filtering.
    int filtered_oids;
    
    /// Measured oid count should equal filtered if alias files are correct.
    int measured_oids;
};

BOOST_AUTO_TEST_CASE(OidAndGiLists)
{
    START;
    
    CSeqDB nr("nr", CSeqDB::eProtein);
    CSeqDB sp("swissprot", CSeqDB::eProtein);
    CSeqDB sc("data/swiss_cheese", CSeqDB::eProtein);
    CSeqDB ac("data/all_cheese", CSeqDB::eProtein);
    
    SDbSumInfo nr_sum(nr);
    SDbSumInfo sp_sum(sp);
    SDbSumInfo ac_sum(ac);
    SDbSumInfo sc_sum(sc);
    
    CHECK_EQUAL_TYPED(string, nr_sum.CompareSelf(), "=A=B=C=a=b=c");
    CHECK_EQUAL_TYPED(string, sp_sum.CompareSelf(), "+A+B=C+a+b=c");
    CHECK_EQUAL_TYPED(string, ac_sum.CompareSelf(), "+A+B=C+a+b=c");
    CHECK_EQUAL_TYPED(string, sc_sum.CompareSelf(), "+A+B=C+a+b=c");
    
    CHECK_EQUAL_TYPED(string, nr_sum.Compare(sp_sum), "=T+F+M=t+f+m");
    CHECK_EQUAL_TYPED(string, nr_sum.Compare(ac_sum), "=T+F+M=t+f+m");
    CHECK_EQUAL_TYPED(string, nr_sum.Compare(sc_sum), "=T+F+M=t+f+m");
    
    CHECK_EQUAL_TYPED(string, sp_sum.Compare(sc_sum), "=T+F+M=t+f+m");
    CHECK_EQUAL_TYPED(string, ac_sum.Compare(sc_sum), "=T+F+M=t+f+m");
}

BOOST_AUTO_TEST_CASE(DeltaSequenceHash)
{
    START;
    
    // Get hash #1
    
    CSeqDBExpert nucl("nucl_dbs", CSeqDB::eNucleotide);
    int oid(-1);
    nucl.GiToOid(4512300, oid);
    unsigned h1 = nucl.GetSequenceHash(oid);
    
    // Get hash #2
    
    char ch = CFile::GetPathSeparator();
    string path = string("data") + ch + "deltaseq";
    ifstream f(path.c_str());
    
    CBioseq bs;
    f >> MSerial_AsnText >> bs;
    
    unsigned h2 = SeqDB_SequenceHash(bs);
    
    // Check that we don't have a real Seq-data.
    
    CHECK(! bs.GetInst().CanGetSeq_data());
    
    // Check that the hash values match.
    
    CHECK_EQUAL(h1, h2);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

