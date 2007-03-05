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
 *   CWriteDB unit test.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/seqdb/seqdbexpert.hpp>
#include <objtools/writers/writedb/writedb.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>

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

#include <test/test_assert.h>  /* This header must go last */


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

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))
#define CHECK_THROW(s, x) BOOST_CHECK_THROW(s, x)

static void s_UnitTestVerbosity(string s)
{
    static bool enabled = static_cast<bool>(getenv("VERBOSE_UT") != NULL);
    
    if (enabled) {
        cout << "Running test: " << s << endl;
    }
}

#define START s_UnitTestVerbosity(BOOST_CURRENT_FUNCTION)

static void
s_FetchRawData(CSeqDBExpert & seqdb,
               int            oid,
               string       & sequence,
               string       & ambig)
{
    const char * buffer (0);
    int          slength(0);
    int          alength(0);
    
    seqdb.GetRawSeqAndAmbig(oid, & buffer, & slength, & alength);
    
    sequence.assign(buffer, slength);
    ambig.assign(buffer + slength, alength);
    
    seqdb.RetSequence(& buffer);
}

static void s_DupIdsBioseq(CWriteDB & w, CSeqDBExpert & s, int * ids)
{
    for(unsigned i = 0; ids[i]; i++) {
        int gi = ids[i];
        int oid = -1;
        bool found = s.GiToOid(gi, oid);
        
        CHECK(found);
        
        CRef<CBioseq> bs = s.GetBioseq(oid, gi);
        CRef<CBlast_def_line_set> bdls = s.GetHdr(oid);
        
        CHECK(bs.NotEmpty());
        CHECK(bdls.NotEmpty());
        
        w.AddSequence(*bs);
        w.SetDeflines(*bdls);
    }
}

static void
s_DupIdsRaw(CWriteDB & w, CSeqDBExpert & seqdb, int * ids)
{
    bool is_nucl = seqdb.GetSequenceType() == CSeqDB::eNucleotide;
    
    for(unsigned i = 0; ids[i]; i++) {
        int gi = ids[i];
        int oid = -1;
        bool found = seqdb.GiToOid(gi, oid);
        
        CHECK(found);
        
        string seq, ambig;
        
        s_FetchRawData(seqdb, oid, seq, ambig);
        CRef<CBlast_def_line_set> bdls = seqdb.GetHdr(oid);
        
        CHECK(! seq.empty());
        CHECK(ambig.empty() || is_nucl);
        CHECK(bdls.NotEmpty());
        
        w.AddSequence(seq, ambig);
        w.SetDeflines(*bdls);
    }
}

template<class ASNOBJ>
void s_Stringify(const ASNOBJ & a, string & s)
{
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << a;
    s = CNcbiOstrstreamToString(oss);
}

template<class ASNOBJ>
void s_Unstringify(const string & s, ASNOBJ & a)
{
    istringstream iss;
    iss.str(s);
    iss >> MSerial_AsnText >> a;
}

template<class ASNOBJ>
CRef<ASNOBJ> s_Duplicate(const ASNOBJ & a)
{
    CRef<ASNOBJ> newobj(new ASNOBJ);
    
    string s;
    s_Stringify(a, s);
    s_Unstringify(s, *newobj);
    
    return newobj;
}

void s_CompareBioseqs(CBioseq & src, CBioseq & dst)
{
    string s1, s2;
    s_Stringify(src, s1);
    s_Stringify(dst, s2);
    
    CHECK_EQUAL(s1, s2);
}

static void
s_TestDatabase(CSeqDBExpert & src,
               const string & name,
               const string & title)
{
    CSeqDBExpert dst(name, src.GetSequenceType());
    
    for(int oid = 0; dst.CheckOrFindOID(oid); oid++) {
        int gi(0), src_oid(0);
        
        bool rv1 = dst.OidToGi(oid, gi);
        bool rv2 = src.GiToOid(gi, src_oid);
        
        CHECK(rv1);
        CHECK(rv2);
        
        CRef<CBioseq> bss = src.GetBioseq(src_oid);
        CRef<CBioseq> bsd = dst.GetBioseq(oid);
        
        s_CompareBioseqs(*bss, *bsd);
    }
    
    CHECK_EQUAL(dst.GetTitle(), title);
}

void s_RemoveFiles(vector<string> & files)
{
    for(unsigned i = 0; i < files.size(); i++) {
        CHECK(NStr::StartsWith(files[i], ""));
        CDirEntry de(files[i]);
        de.Remove(CDirEntry::eOnlyEmpty);
    }
}

static void
s_DupSequencesTest(int    * ids,
                   bool     is_protein,
                   bool     raw_data,
                   string   src_name,
                   string   dst_name,
                   string   title)
{
    CSeqDBExpert src(src_name, (is_protein
                                ? CSeqDB::eProtein
                                : CSeqDB::eNucleotide));
    
    vector<string> files;
    
    CRef<CWriteDB> db;
    
    db.Reset(new CWriteDB(dst_name,
                          (is_protein
                           ? CWriteDB::eProtein
                           : CWriteDB::eNucleotide),
                          title,
                          CWriteDB::eFullIndex));
    
    if (raw_data) {
        s_DupIdsRaw(*db, src, ids);
    } else {
        s_DupIdsBioseq(*db, src, ids);
    }
    
    db->Close();
    db->ListFiles(files);
    db.Reset();
    
    s_TestDatabase(src, dst_name, title);
    s_RemoveFiles(files);
}

static CRef<CObjectManager> s_CreateObjMgr()
{
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CRef<CReader> reader(new CId2Reader());
    reader->SetPreopenConnection(false);
    CGBDataLoader::RegisterInObjectManager(*obj_mgr, reader);
    return obj_mgr;
}

CRef<CScope> s_GetScope()
{
    static CRef<CObjectManager> obj_mgr = s_CreateObjMgr();

    CRef<CScope> scope(new CScope(*obj_mgr));

    // Add default loaders to the scope.
    scope->AddDefaults();

    return scope;
}

//
// Actual test cases.
//

BOOST_AUTO_UNIT_TEST(s_NuclBioseqDup)
{
    START;
    
    int ids[] = {
        78883515, 78883517, 71143095, 24431485, 19110479, 15054463,
        15054465, 15054467, 15054469, 15054471, 19570808, 18916476,
        1669608,  1669610,  1669612,  1669614,  1669616,  10944307,
        10944309, 10944311, 19909844, 19909846, 19909860, 19911180,
        19911220, 19911222, 19911224, 57472140, 20126670, 20387092,
        57639630, 57639632, 7670507,  2394289,  21280378, 21327938,
        6518520,  20086356, 20086357, 21392391, 20086359, 19110509,
        21623739, 21623761, 38303844, 38197377, 56788779, 57032781,
        57870443, 56789136, 0
    };
    
    s_DupSequencesTest(ids,
                       false,
                       false,
                       "nt",
                       "w-nucl-bs",
                       "bioseq nucleotide dup");
    
    s_DupSequencesTest(ids,
                       false,
                       true,
                       "nt",
                       "w-nucl-raw",
                       "raw nucleotide dup");
}

BOOST_AUTO_UNIT_TEST(s_ProtBioseqDup)
{
    START;
    
    int ids[] = {
        1477444,  1669609,  1669611,  1669615, 1669617, 7544146,
        22652804, 1310870,  3114354,  3891778, 3891779, 81294290,
        81294330, 49089974, 62798905, 3041810, 7684357, 7684359,
        7684361,  7684363,  7544148,  3452560, 3452564, 6681587,
        6681590,  6729087,  7259315,  2326257, 3786310, 3845607,
        13516469, 2575863,  4049591,  3192363, 1871126, 2723484,
        6723181,  11125717, 2815400,  1816433, 3668177, 6552408,
        13365559, 8096667,  3721768,  9857600, 2190043, 3219276,
        10799943, 10799945, 0
    };
    
    s_DupSequencesTest(ids,
                       true,
                       false,
                       "nr",
                       "w-prot-bs",
                       "bioseq protein dup");
    
    s_DupSequencesTest(ids,
                       true,
                       true,
                       "nr",
                       "w-prot-raw",
                       "raw protein dup");
}

BOOST_AUTO_UNIT_TEST(s_EmptyBioseq)
{
    START;
    
    CWriteDB fails("failing-db",
                   CWriteDB::eProtein,
                   "title",
                   CWriteDB::eFullIndex);
    
    CRef<CBioseq> bs(new CBioseq);
    fails.AddSequence(*bs);
    
    CHECK_THROW(fails.Close(), CWriteDBException);
}

BOOST_AUTO_UNIT_TEST(s_BioseqHandle)
{
    START;
    
    CWriteDB db("from-loader",
                CWriteDB::eProtein,
                "title",
                CWriteDB::eFullIndex);
    
    CRef<CScope> scope = s_GetScope();
    
    // Normal bioseq handle.
    
    CRef<CSeq_id> id1(new CSeq_id("gi|129295"));
    CBioseq_Handle bsh1 = scope->GetBioseqHandle(*id1);
    db.AddSequence(bsh1);
    
    // Clean up.
    
    vector<string> files;
    db.Close();
    db.ListFiles(files);
    s_RemoveFiles(files);
}

BOOST_AUTO_UNIT_TEST(s_BioseqHandleAndSeqVector)
{
    START;
    
    CWriteDB db("from-loader",
                CWriteDB::eProtein,
                "title",
                CWriteDB::eFullIndex);
    
    CRef<CScope> scope = s_GetScope();
    
    // Bioseq + CSeqVector.
    
    CRef<CSeq_id> id2(new CSeq_id("gi|129296"));
    CBioseq_Handle bsh2 = scope->GetBioseqHandle(*id2);
    CConstRef<CBioseq> bs1c = bsh2.GetCompleteBioseq();
    
    CRef<CBioseq> bs1 = s_Duplicate(*bs1c);
    CSeqVector sv(bsh2);
    
    // Make sure CSeqVector is exercised by removing the Seq-data.
    
    bs1->SetInst().ResetSeq_data();
    db.AddSequence(*bs1, sv);
    
    // Clean up.
    
    vector<string> files;
    db.Close();
    db.ListFiles(files);
    s_RemoveFiles(files);
}

BOOST_AUTO_UNIT_TEST(s_SetPig)
{
    START;
    
    string nm = "pigs";
    vector<string> files;
    
    {
        CSeqDB nr("nr", CSeqDB::eProtein);
        
        CWriteDB db(nm,
                    CWriteDB::eProtein,
                    "title",
                    CWriteDB::eFullIndex);
        
        db.AddSequence(*nr.GiToBioseq(129295));
        db.SetPig(101);
        
        db.AddSequence(*nr.GiToBioseq(129296));
        db.SetPig(102);
        
        db.AddSequence(*nr.GiToBioseq(129297));
        db.SetPig(103);
        
        db.Close();
        db.ListFiles(files);
    }
    
    CSeqDB db2(nm, CSeqDB::eProtein);
    
    int oid = 0;
    
    for(; db2.CheckOrFindOID(oid); oid++) {
        int pig(0);
        vector<int> gis;
        
        bool rv1 = db2.OidToPig(oid, pig);
        db2.GetGis(oid, gis, false);
        
        bool found_gi = false;
        for(unsigned i = 0; i < gis.size(); i++) {
            if (gis[i] == (129295 + oid)) {
                found_gi = true;
            }
        }
        
        CHECK(rv1);
        CHECK(found_gi);
        CHECK_EQUAL(pig-oid, 101);
    }
    
    CHECK_EQUAL(oid, 3);
    
    s_RemoveFiles(files);
}

// Test multiple volume construction and maximum letter limit.

BOOST_AUTO_UNIT_TEST(s_MultiVolume)
{
    START;
    
    CSeqDB nr("nr", CSeqDB::eProtein);
    
    CWriteDB db("multivol",
                CWriteDB::eProtein,
                "title",
                CWriteDB::eFullIndex);
    
    db.SetMaxVolumeLetters(500);
    
    int gis[] = { 129295, 129296, 129297, 129299, 0 };
    
    Uint8 letter_count = 0;
    
    for(int i = 0; gis[i]; i++) {
        int oid(0);
        nr.GiToOid(gis[i], oid);
        
        db.AddSequence(*nr.GetBioseq(oid));
        letter_count += nr.GetSeqLength(oid);
    }
    
    db.Close();
    
    vector<string> v;
    vector<string> f;
    db.ListVolumes(v);
    db.ListFiles(f);
    
    CHECK_EQUAL(3, (int) v.size());
    CHECK_EQUAL(v[0], string("multivol.00"));
    CHECK_EQUAL(v[1], string("multivol.01"));
    CHECK_EQUAL(v[2], string("multivol.02"));
    
    CHECK_EQUAL(28, (int) f.size());
    
    // Check resulting db.
    
    CRef<CSeqDB> seqdb(new CSeqDB("multivol", CSeqDB::eProtein));
    
    int oids(0);
    Uint8 letters(0);
    
    seqdb->GetTotals(CSeqDB::eUnfilteredAll, & oids, & letters, false);
    
    CHECK_EQUAL(oids, 4);
    CHECK_EQUAL(letter_count, letters);
    
    seqdb.Reset();
    
    s_RemoveFiles(f);
}

BOOST_AUTO_UNIT_TEST(s_UsPatId)
{
    START;
    
    CRef<CSeq_id> seqid(new CSeq_id("pat|us|123|456"));
    vector<string> files;
    
    {
        CRef<CWriteDB> writedb
            (new CWriteDB("uspatid",
                          CWriteDB::eProtein,
                          "patent id test",
                          CWriteDB::eFullIndex));
        
        CSeqDB seqdb("nr", CSeqDB::eProtein);
        
        CRef<CBioseq> bs = seqdb.GiToBioseq(129297);
        
        CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
        CRef<CBlast_def_line> dl(new CBlast_def_line);
        bdls->Set().push_back(dl);
        
        dl->SetTitle("Some protein sequence");
        dl->SetSeqid().push_back(seqid);
        dl->SetTaxid(12345);
        
        writedb->AddSequence(*bs);
        writedb->SetDeflines(*bdls);
        
        writedb->Close();
        writedb->ListFiles(files);
        CHECK(files.size());
    }
    
    CSeqDB seqdb("uspatid", CSeqDB::eProtein);
    int oid(-1);
    bool found = seqdb.SeqidToOid(*seqid, oid);
    
    CHECK_EQUAL(found, true);
    CHECK_EQUAL(oid,   0);
    
    s_RemoveFiles(files);
}
