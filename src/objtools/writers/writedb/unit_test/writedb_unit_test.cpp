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
#include <objtools/readers/fasta.hpp>

#include <sstream>

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

#include <common/test_assert.h>  /* This header must go last */


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

// Fetch sequence and nucleotide data for the given oid as a pair of
// strings (in ncbi2na packed format), one for sequence data and one
// for ambiguities.

void
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

// Return a Seq-id built from the given int (gi).

CRef<CSeq_id> s_GiToSeqId(int gi)
{
    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, gi));
    
    return seqid;
}

// Return a Seq-id built from the given string (accession or FASTA
// format Seq-id).

CRef<CSeq_id> s_AccToSeqId(const char * acc)
{
    CRef<CSeq_id> seqid(new CSeq_id(acc));
    
    return seqid;
}

// HexDump utility
string s_HexDumpText(const string & raw, int per, bool use_hex)
{
    string visible;
    
    string tmp;
    
    Uint8 mask = Uint8(Int8(-1));
    mask >>= (64 - 8*per);
    
    for(int i = 0; i < (int)raw.size(); i += per) {
        int left = raw.size() - i;
        int per1 = (left < per) ? left : per;
        
        string sub(raw, i, per1);
        
        // Read a standard order value into x.
        
        Uint8 x = 0;
        
        for(int by = 0; by < (int)sub.size(); by++) {
            x = (x << 8) + (sub[by] & 0xFF);
        }
        
        if (visible.size())
            visible += " ";
        
        tmp.resize(0);
        NStr::UInt8ToString(tmp, x & mask, 0, use_hex ? 16 : 10);
        
        visible += tmp;
    }
    
    return visible;
}

string s_HexDumpFile(const string & fname, int per, bool use_hex)
{
    ifstream f(fname.c_str());
    
    string raw;
    
    while(f && ! f.eof()) {
        char buf[1024];
        f.read(buf, 1024);
        
        int amt = f.gcount();
        
        if (! amt)
            break;
        
        raw.append(buf, amt);
    }
    
    return s_HexDumpText(raw, per, use_hex);
}

// Copy the sequences listed in 'ids' (integers or FASTA Seq-ids) from
// the CSeqDB object to the CWriteDB object, using CBioseqs as the
// intermediate data.

typedef vector< CRef<CSeq_id> > TIdList;

static void
s_DupIdsBioseq(CWriteDB      & w,
               CSeqDB        & s,
               const TIdList & ids)
{
    ITERATE(TIdList, iter, ids) {
        CRef<CSeq_id> seqid = *iter;
        
        int oid = -1;
        bool found = s.SeqidToOid(*seqid, oid);
        
        CHECK(found);
        
        CRef<CBioseq> bs;
        
        if (seqid->IsGi()) {
            bs = s.GetBioseq(oid, seqid->GetGi());
        } else {
            bs = s.GetBioseq(oid);
        }
        
        CRef<CBlast_def_line_set> bdls = s.GetHdr(oid);
        
        CHECK(bs.NotEmpty());
        CHECK(bdls.NotEmpty());
        
        w.AddSequence(*bs);
        w.SetDeflines(*bdls);
    }
}

// Copy the sequences listed in 'ids' (integers or FASTA Seq-ids) from
// the CSeqDB object to the CWriteDB object, using packed ncbi2na
// strings ('raw' data) as the intermediate data.

static void
s_DupIdsRaw(CWriteDB      & w,
            CSeqDBExpert  & seqdb,
            const TIdList & ids)
{
    bool is_nucl = seqdb.GetSequenceType() == CSeqDB::eNucleotide;
    
    ITERATE(TIdList, iter, ids) {
        CRef<CSeq_id> seqid = *iter;
        
        int oid = -1;
        bool found = seqdb.SeqidToOid(*seqid, oid);
        
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

// Serialize the provided ASN.1 object into a string.

template<class ASNOBJ>
void s_Stringify(const ASNOBJ & a, string & s)
{
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << a;
    s = CNcbiOstrstreamToString(oss);
}

// Deserialize the provided string into an ASN.1 object.

template<class ASNOBJ>
void s_Unstringify(const string & s, ASNOBJ & a)
{
    istringstream iss;
    iss.str(s);
    iss >> MSerial_AsnText >> a;
}

// Duplicate the provided ASN.1 object (via {,de}serialization).

template<class ASNOBJ>
CRef<ASNOBJ> s_Duplicate(const ASNOBJ & a)
{
    CRef<ASNOBJ> newobj(new ASNOBJ);
    
    string s;
    s_Stringify(a, s);
    s_Unstringify(s, *newobj);
    
    return newobj;
}

// Compare the two CBioseqs by comparing their serialized forms.

void s_CompareBioseqs(CBioseq & src, CBioseq & dst)
{
    string s1, s2;
    s_Stringify(src, s1);
    s_Stringify(dst, s2);
    
    CHECK_EQUAL(s1, s2);
}

// Test the database compared to a reference database, usually the
// database that provided the source data.

void
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

// Remove the specified files.

void s_RemoveFiles(const vector<string> & files)
{
    for(unsigned i = 0; i < files.size(); i++) {
        CDirEntry de(files[i]);
        de.Remove(CDirEntry::eOnlyEmpty);
    }
}

// Check if the given file is already sorted.

void s_CheckSorted(const string & fname)
{
    CNcbiIfstream file(fname.c_str());
    
    string s, s2;
    
    while(NcbiGetlineEOL(file, s)) {
        CHECK(s2 <= s);
        s.swap(s2);
    }
}

// Check the files that make up a database volume.
//
// nsd/psd: Check that the file is in sorted order

string s_ExtractLast(const string & data, const string & delim)
{
    size_t pos = data.rfind(delim);
    
    if (pos == string::npos)
        return "";
    
    return string(data,
                  pos+delim.size(),
                  data.size()-(pos + delim.size()));
}

// Check the files that make up a database volume.
//
// nsd/psd: Check that the file is in sorted order

void s_CheckFiles(const vector<string> & files,
                  bool                   need_hash = false)
{
    bool found_hash = false;
    
    for(unsigned i = 0; i < files.size(); i++) {
        string ext = s_ExtractLast(files[i], ".");
        
        if (ext == "nsd" || ext == "psd") {
            s_CheckSorted(files[i]);
        }
        if (ext == "nhd" || ext == "phd") {
            s_CheckSorted(files[i]);
            found_hash = true;
        }
    }
    
    if (need_hash) {
        CHECK(found_hash);
    }
}

// Do sanity checks appropriate for some files, then remove them.

void s_WrapUpFiles(const vector<string> & files)
{
    s_CheckFiles(files);
    s_RemoveFiles(files);
}

// Copy the specified ids (int -> GI, string -> FASTA Seq-id) from the
// source database (src_name) to a new CWriteDB object, then perform
// checks on the resulting database and remove it.

static void
s_DupSequencesTest(const TIdList & ids,
                   bool            is_protein,
                   bool            raw_data,
                   string          src_name,
                   string          dst_name,
                   string          title)
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
    s_WrapUpFiles(files);
}

// Create an object manager, add ID2 loaders to it, and return it.

static CRef<CObjectManager> s_CreateObjMgr()
{
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CRef<CReader> reader(new CId2Reader());
    reader->SetPreopenConnection(false);
    CGBDataLoader::RegisterInObjectManager(*obj_mgr, reader);
    return obj_mgr;
}

// Get and return a CScope with default loaders.

CRef<CScope> s_GetScope()
{
    static CRef<CObjectManager> obj_mgr = s_CreateObjMgr();

    CRef<CScope> scope(new CScope(*obj_mgr));

    // Add default loaders to the scope.
    scope->AddDefaults();

    return scope;
}

static void s_BuildIds(TIdList & ids, int * gis)
{
    for(int * ptr = gis; *ptr; ptr ++) {
        ids.push_back(s_GiToSeqId(*ptr));
    }
}

static void s_BuildIds(TIdList & ids, const char ** gis)
{
    for(const char ** ptr = gis; *ptr; ptr ++) {
        ids.push_back(s_AccToSeqId(*ptr));
    }
}

CRef<CBioseq> s_FastaStringToBioseq(const string & str, bool protein)
{
    istrstream istr(str.data(), str.size());
    
    CRef<ILineReader> lr(new CStreamLineReader(istr));
    
    typedef CFastaReader::EFlags TFlags;
    
    TFlags flags = (TFlags) (CFastaReader::fAllSeqIds |
                             (protein
                              ? CFastaReader::fAssumeProt
                              : CFastaReader::fAssumeNuc));
    
    CFastaReader fr(*lr, flags);
    
    CHECK(! lr->AtEOF());
    CRef<CSeq_entry> entry = fr.ReadOneSeq();
    
    CHECK(! entry.Empty());
    CHECK(entry->IsSeq());
    
    CRef<CBioseq> bs(& entry->SetSeq());
    
    return bs;
}


// Unit test verbosity support.

static void s_UnitTestVerbosity(string s)
{
    static bool enabled = static_cast<bool>(getenv("VERBOSE_UT") != NULL);
    
    if (enabled) {
        cout << "Running test: " << s << endl;
    }
}

#define UNIT_TEST_FILTER(s) \
{ \
    string nm(s); \
    string filter(getenv("FILTER_UT") ? getenv("FILTER_UT") : ""); \
    \
    if (nm.size() && filter.size() && nm.find(filter) == string::npos) { \
        cout << "Skipping test: " << s << endl; \
        return; \
    } \
}

#define START UNIT_TEST_FILTER(BOOST_CURRENT_FUNCTION); \
              s_UnitTestVerbosity(BOOST_CURRENT_FUNCTION)


//
// Actual test cases.
//

BOOST_AUTO_TEST_CASE(NuclBioseqDup)
{
    START;
    
    int gis[] = {
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
    
    TIdList ids;
    s_BuildIds(ids, gis);
    
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

BOOST_AUTO_TEST_CASE(ProtBioseqDup)
{
    START;
    
    int gis[] = {
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
    
    TIdList ids;
    s_BuildIds(ids, gis);
    
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

BOOST_AUTO_TEST_CASE(EmptyBioseq)
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

BOOST_AUTO_TEST_CASE(BioseqHandle)
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
    s_WrapUpFiles(files);
}

BOOST_AUTO_TEST_CASE(BioseqHandleAndSeqVector)
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
    s_WrapUpFiles(files);
}

BOOST_AUTO_TEST_CASE(SetPig)
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
    
    s_WrapUpFiles(files);
}

// Test multiple volume construction and maximum letter limit.

BOOST_AUTO_TEST_CASE(MultiVolume)
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
    
    CHECK_EQUAL(22, (int) f.size());
    
    // Check resulting db.
    
    CRef<CSeqDB> seqdb(new CSeqDB("multivol", CSeqDB::eProtein));
    
    int oids(0);
    Uint8 letters(0);
    
    seqdb->GetTotals(CSeqDB::eUnfilteredAll, & oids, & letters, false);
    
    CHECK_EQUAL(oids, 4);
    CHECK_EQUAL(letter_count, letters);
    
    seqdb.Reset();
    
    s_WrapUpFiles(f);
}

BOOST_AUTO_TEST_CASE(UsPatId)
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
        CHECK(files.size() != 0);
    }
    
    CSeqDB seqdb("uspatid", CSeqDB::eProtein);
    int oid(-1);
    bool found = seqdb.SeqidToOid(*seqid, oid);
    
    CHECK_EQUAL(found, true);
    CHECK_EQUAL(oid,   0);
    
    s_WrapUpFiles(files);
}

BOOST_AUTO_TEST_CASE(IsamSorting)
{
    START;
    
    // This checks whether the following IDs are fetchable from the
    // given database.  It will fail if either the production blast
    // databases (i.e. found at $BLASTDB) are corrupted or if the
    // newly produced database is corrupted.  It will also fail if any
    // of the IDs are legitimately missing (removed by the curators),
    // in which case the given ID must be removed from the list.
    
    // However, the selection of these specific IDs is not arbitrary;
    // these are several sets of IDs which have a common 6 letter
    // prefix.  The test will not work correctly if these IDs are
    // replaced with IDs that don't have this trait, if too many are
    // removed, or if the IDs are put in sorted order.
    
    // A null terminated array of NUL terminated strings.
    
    const char* accs[] = {
        "AAC76335.1", "AAC77159.1", "AAA58145.1", "AAC76880.1",
        "AAC76230.1", "AAC76373.1", "AAC77137.1", "AAC76637.2",
        "AAA58101.1", "AAC76329.1", "AAC76702.1", "AAC77109.1",
        "AAC76757.1", "AAA58162.1", "AAC76604.1", "AAC76539.1",
        "AAA24224.1", "AAC76351.1", "AAC76926.1", "AAC77047.1",
        "AAC76390.1", "AAC76195.1", "AAA57930.1", "AAC76134.1",
        "AAC76586.2", "AAA58123.1", "AAC76430.1", "AAA58107.1",
        "AAC76765.1", "AAA24272.1", "AAC76396.2", "AAA24183.1",
        "AAC76918.1", "AAC76727.1", "AAC76161.1", "AAA57964.1",
        "AAA24251.1", 0
    };
    
    TIdList ids;
    s_BuildIds(ids, accs);
    
    s_DupSequencesTest(ids,
                       true,
                       false,
                       "nr",
                       "w-isam-sort-bs",
                       "test of string ISAM sortedness");
}

BOOST_AUTO_TEST_CASE(HashToOid)
{
    START;
    
    CSeqDBExpert nr("nr", CSeqDB::eProtein);
    CSeqDBExpert nt("nt", CSeqDB::eNucleotide);
    
    int prot_gis[] = { 129295, 129296, 129297, 0 };
    int nucl_gis[] = { 555, 556, 405832, 0 };
    
    TIdList prot_ids, nucl_ids;
    s_BuildIds(prot_ids, prot_gis);
    s_BuildIds(nucl_ids, nucl_gis);
    
    typedef CWriteDB::EIndexType TType;
    
    TType itype = TType(CWriteDB::eFullWithTrace |
                        CWriteDB::eAddHash);
    
    CRef<CWriteDB> prot(new CWriteDB("w-prot-hash",
                                     CWriteDB::eProtein,
                                     "test of hash ISAMs (P)",
                                     itype));
    
    CRef<CWriteDB> nucl(new CWriteDB("w-nucl-hash",
                                     CWriteDB::eNucleotide,
                                     "test of hash ISAMs (N)",
                                     itype));
    
    s_DupIdsBioseq(*prot, nr, prot_ids);
    s_DupIdsBioseq(*nucl, nt, nucl_ids);
    
    prot->Close();
    nucl->Close();
    
    vector<string> files;
    prot->ListFiles(files);
    s_CheckFiles(files, true);
    s_RemoveFiles(files);
    prot.Reset();
    
    files.resize(0);
    nucl->ListFiles(files);
    s_CheckFiles(files, true);
    s_RemoveFiles(files);
    nucl.Reset();
}

BOOST_AUTO_TEST_CASE(FastaReaderBioseq)
{
    START;
    
    vector<string> files;
    
    string title = "from-fasta-reader";
    
    string
        I1("gi|123"), T1("One two three."),
        I2("gi|124"), T2("One two four.");
    
    {
        CRef<CWriteDB> wr(new CWriteDB(title,
                                       CWriteDB::eProtein,
                                       "title",
                                       CWriteDB::eFullIndex));
        
        // Build a multi-defline bioseq and read it with CFastaReader.
        
        string str =
            ">"    + I1 + " " + T1 +
            "\001" + I2 + " " + T2 + "\n" +
            "ELVISLIVES\n";
        
        CRef<CBioseq> bs = s_FastaStringToBioseq(str, true);
        
        wr->AddSequence(*bs);
        wr->Close();
        
        // Clean up.
        
        wr->ListFiles(files);
    }
    
    {
        CSeqDB rd("from-fasta-reader", CSeqDB::eProtein);
        CHECK(rd.GetNumOIDs() == 1);
        
        CRef<CBlast_def_line_set> bdls =
            rd.GetHdr(0);
        
        CHECK(bdls->Get().size() == 2);
        CHECK(bdls->Get().front()->GetTitle() == T1);
        CHECK(bdls->Get().front()->GetSeqid().size() == 1);
        CHECK(bdls->Get().front()->GetSeqid().front()->AsFastaString() == I1);
        
        CHECK(bdls->Get().back()->GetTitle() == T2);
        CHECK(bdls->Get().back()->GetSeqid().size() == 1);
        CHECK(bdls->Get().back()->GetSeqid().front()->AsFastaString() == I2);
    }
    
    s_WrapUpFiles(files);
}

BOOST_AUTO_TEST_CASE(BinaryListBuilder)
{
    START;
    
    string fn4("test4.til"), fn8("test8.til");
    
    {
        CBinaryListBuilder blb4(CBinaryListBuilder::eTi);
        CBinaryListBuilder blb8(CBinaryListBuilder::eTi);
        
        for(int i = 0; i<10; i++) {
            blb4.AppendId(Int8(1) << (i*2));
            blb8.AppendId(Int8(1) << (i*4));
        }
        
        blb4.Write(fn4);
        blb8.Write(fn8);
    }
    
    string h4 = s_HexDumpFile(fn4, 4, true);
    string h8 = s_HexDumpFile(fn8, 4, true);
    
    // The FF...FD symbol indicates a 4 byte TI list; the FF..FC
    // symbol is the eight byte version.
    
    CHECK(h4 ==
          "FFFFFFFD A "
          "1 4 10 40 100 "
          "400 1000 4000 10000 40000");
    
    CHECK(h8 ==
          "FFFFFFFC A "
          "0 1 0 10 0 100 0 1000 0 10000 "
          "0 100000 0 1000000 0 10000000 1 0 10 0");
    
    CFile(h4).Remove();
    CFile(h8).Remove();
}

