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
 * Author: Aleksey Grichenko
 *
 * File Description:
 *   Test program for hash_map and hash_set
 *
 */

#include <ncbi_pch.hpp>
#include <stdlib.h>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/hash_map.hpp>
#include <corelib/hash_set.hpp>


BEGIN_NCBI_SCOPE


class CTestHash : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


int MaxHashSize = 10000;
int ReadLoops = 1000;

void CTestHash::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_hash",
                       "test hash classes");

    d->AddDefaultKey("elements", "Elements",
         "Number of elements in containers",
         CArgDescriptions::eInteger, "10000");

    d->AddDefaultKey("cycles", "Cycles",
                     "Number of read/search cycles over the whole container",
                     CArgDescriptions::eInteger, "1000");

    d->AddFlag("test_std",
               "Compare results to statndard (non-hash) containers");

    SetupArgDescriptions(d.release());
}

inline
string GetKey(int i)
{
    return "test" + NStr::IntToString(i);
}

#define CHECK_RESULT(val) \
    if ( !(val) ) { \
    NcbiCout << "FAILED: " << #val << NcbiEndl; \
        return false; \
    }

#define START_TEST(loops) \
    start = CTime(CTime::eCurrent); \
    for (int loop = 0; loop < (loops); loop++) {

#define STOP_TEST \
    } \
    stop = CTime(CTime::eCurrent); \
    NcbiCout << "done, \t" << stop.DiffTimeSpan(start).AsString() << NcbiEndl;

// #define hash_map map

typedef vector<string> TKeys;

template<class TMap>
bool TestMap(TMap& hm, const string& map_name, const TKeys& keys)
{
    NcbiCout << "*** Testing " << map_name << " ***" << NcbiEndl;

    CTime start, stop;

    NcbiCout << "Populating...         ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        pair<typename TMap::iterator, bool> res = hm.insert(
            typename TMap::value_type(keys[i], i));
        CHECK_RESULT(res.second);
    }
    CHECK_RESULT(int(hm.size()) == MaxHashSize);
    STOP_TEST

    NcbiCout << "Re-populating...      ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        pair<typename TMap::iterator, bool> res = hm.insert(
            typename TMap::value_type(keys[i], i+1));
        CHECK_RESULT(!res.second);
        CHECK_RESULT(hm.count(res.first->first) == 1);
    }
    CHECK_RESULT(int(hm.size()) == MaxHashSize);
    STOP_TEST

    NcbiCout << "Testing values...     ";
    START_TEST(ReadLoops)
    ITERATE(typename TMap, map_it, hm) {
        CHECK_RESULT(map_it->first == keys[map_it->second]);
    }
    STOP_TEST

    NcbiCout << "Changing values...    ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        CHECK_RESULT(hm[keys[i]] == i);
        hm[keys[i]] = i+1;
        CHECK_RESULT(hm[keys[i]] == i+1);
    }
    CHECK_RESULT(int(hm.size()) == MaxHashSize);
    STOP_TEST

    NcbiCout << "Searching...          ";
    START_TEST(ReadLoops)
    for (int i = 0; i < MaxHashSize; i++) {
        typename TMap::const_iterator f = hm.find(keys[i]);
        CHECK_RESULT(f != hm.end());
        CHECK_RESULT(f->second == i+1);
    }
    STOP_TEST

    NcbiCout << "Cleaning...           ";
    START_TEST(1)
    hm.clear();
    CHECK_RESULT(hm.empty());
    STOP_TEST
    NcbiCout << NcbiEndl;
    return true;
}


template<class TMultimap>
bool TestMultimap(TMultimap& hmm, const string& map_name, const TKeys& keys)
{
    NcbiCout << "*** Testing " << map_name << " ***" << NcbiEndl;

    CTime start, stop;

    NcbiCout << "Populating...         ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        hmm.insert(typename TMultimap::value_type(keys[i], i));
        hmm.insert(typename TMultimap::value_type(keys[i], i+1));
        CHECK_RESULT(hmm.count(keys[i]) == 2);
    }
    CHECK_RESULT(int(hmm.size()) == MaxHashSize*2);
    STOP_TEST

    NcbiCout << "Testing values...     ";
    START_TEST(ReadLoops)
    ITERATE(typename TMultimap, map_it, hmm) {
        CHECK_RESULT(map_it->first == keys[map_it->second]  ||
            map_it->first == keys[map_it->second - 1]);
    }
    STOP_TEST

    NcbiCout << "Searching...          ";
    START_TEST(ReadLoops)
    for (int i = 0; i < MaxHashSize; i++) {
        typename TMultimap::const_iterator f = hmm.find(keys[i]);
        CHECK_RESULT(f != hmm.end());
        CHECK_RESULT(f->second == i  ||  f->second == i+1);
    }
    STOP_TEST

    NcbiCout << "Cleaning...           ";
    START_TEST(1)
    hmm.clear();
    CHECK_RESULT(hmm.empty());
    STOP_TEST
    NcbiCout << NcbiEndl;
    return true;
}


template<class TSet>
bool TestSet(TSet& hs, const string& set_name, const TKeys& keys)
{
    NcbiCout << "*** Testing " << set_name << " ***" << NcbiEndl;

    CTime start, stop;

    NcbiCout << "Populating...         ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        pair<typename TSet::const_iterator, bool> res = hs.insert(keys[i]);
        CHECK_RESULT(res.second);
    }
    STOP_TEST

    NcbiCout << "Re-populating...      ";
    START_TEST(1)
    for (int i = 0; i < MaxHashSize; i++) {
        pair<typename TSet::const_iterator, bool> res = hs.insert(keys[i]);
        CHECK_RESULT(!res.second);
    }
    STOP_TEST

    NcbiCout << "Searching...          ";
    START_TEST(ReadLoops)
    for (int i = 0; i < MaxHashSize; i++) {
        typename TSet::const_iterator f = hs.find(keys[i]);
        CHECK_RESULT(f != hs.end());
    }
    STOP_TEST

    NcbiCout << "Cleaning...           ";
    START_TEST(1)
    hs.clear();
    CHECK_RESULT(hs.empty());
    STOP_TEST
    NcbiCout << NcbiEndl;
    return true;
}


int CTestHash::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    MaxHashSize = args["elements"].AsInteger();
    ReadLoops = args["cycles"].AsInteger();
    bool test_std = args["test_std"].HasValue();

    NcbiCout << "Testing with " << MaxHashSize << " elements, "
        << ReadLoops << " read cycles" << NcbiEndl;

    TKeys keys;
    set<int> hashes;
    hash_set<string> b_hs;
    for (int i = 0; i < MaxHashSize + 1; i++) {
        int key = i;
        keys.push_back(GetKey(key));
        hashes.insert(hash<string>()(keys.back()));
        b_hs.insert(keys.back());
    }
    random_shuffle(keys.begin(), keys.end());

    set<int> bhashes;
    for (int i = 0; i < MaxHashSize + 1; i++) {
        bhashes.insert(hash<string>()(keys[i])%b_hs.bucket_count());
    }
    NcbiCout << "keys: " << keys.size()
        << ", hashes: " << hashes.size()
        << ", bucket: " << b_hs.bucket_count()
        << ", bhashes: " << bhashes.size()
        << NcbiEndl << NcbiEndl;

    {{
        hash_map<string, int> hm;
        if ( !TestMap(hm, "hash_map", keys) ) {
            return 1;
        }
    }}
    if ( test_std ) {
        map<string, int> stdm;
        if ( !TestMap(stdm, "map", keys) ) {
            return 1;
        }
    }

    {{
        hash_multimap<string, int> hmm;
        if ( !TestMultimap(hmm, "hash_multimap", keys) ) {
            return 1;
        }
    }}
    if ( test_std ) {
        multimap<string, int> stdmm;
        if ( !TestMultimap(stdmm, "multimap", keys) ) {
            return 1;
        }
    }

    {{
        hash_set<string> hs;
        if ( !TestSet(hs, "hash_set", keys) ) {
            return 1;
        }
    }}
    if ( test_std ) {
        set<string> stds;
        if ( !TestSet(stds, "set", keys) ) {
            return 1;
        }
    }

    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestHash().AppMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/02/09 16:33:02  grichenk
 * Improved test
 *
 * Revision 1.1  2005/02/08 18:45:51  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
