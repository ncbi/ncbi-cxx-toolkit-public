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
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/hash_map.hpp>
#include <corelib/hash_set.hpp>


BEGIN_NCBI_SCOPE


template<> struct hash<const string>
{
    size_t operator()(const string& s) const
    {
        return __stl_hash_string(s.c_str());
    }
};


template<> struct hash<string>
{
    size_t operator()(const string& s) const
    {
        return __stl_hash_string(s.c_str());
    }
};


class CTestHash : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestHash::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_hash",
                       "test hash classes");

    SetupArgDescriptions(d.release());
}

inline
string GetKey(int i)
{
    return "test" + NStr::IntToString(i);
}

const int kMaxHashSize = 10000;


#define CHECK_RESULT(val) \
    if ( !(val) ) { \
    NcbiCout << "FAILED: " << #val << NcbiEndl; \
        return 1; \
    }


int CTestHash::Run(void)
{
    {{
        typedef hash_map<string, int> THashMap;
        THashMap hm;

        NcbiCout << "Populating hash_map... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            pair<THashMap::const_iterator, bool> res = hm.insert(
                THashMap::value_type(GetKey(i), i));
            CHECK_RESULT(res.second);
        }
        CHECK_RESULT(int(hm.size()) == kMaxHashSize);
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Re-populating hash_map... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            pair<THashMap::const_iterator, bool> res = hm.insert(
                THashMap::value_type(GetKey(i), i+1));
            CHECK_RESULT(!res.second);
            CHECK_RESULT(hm.count(res.first->first) == 1);
        }
        CHECK_RESULT(int(hm.size()) == kMaxHashSize);
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Testing mapped values... ";
        ITERATE(THashMap, map_it, hm) {
            CHECK_RESULT(map_it->first == GetKey(map_it->second));
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Changing values... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            CHECK_RESULT(hm[GetKey(i)] == i);
            hm[GetKey(i)] = i+1;
            CHECK_RESULT(hm[GetKey(i)] == i+1);
        }
        CHECK_RESULT(int(hm.size()) == kMaxHashSize);
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Searching hash_map... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            THashMap::const_iterator f = hm.find(GetKey(i));
            CHECK_RESULT(f != hm.end());
            CHECK_RESULT(f->second == i+1);
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Cleaning hash_map... ";
        hm.clear();
        CHECK_RESULT(hm.empty());
        NcbiCout << "OK" << NcbiEndl;
    }}

    {{
        typedef hash_multimap<string, int> THashMultiap;
        THashMultiap hmm;

        NcbiCout << "Populating hash_multimap... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            hmm.insert(THashMultiap::value_type(GetKey(i), i));
            hmm.insert(THashMultiap::value_type(GetKey(i), i+1));
            CHECK_RESULT(hmm.count(GetKey(i)) == 2);
        }
        CHECK_RESULT(hmm.size() == kMaxHashSize*2);
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Testing mapped values... ";
        ITERATE(THashMultiap, map_it, hmm) {
            CHECK_RESULT(map_it->first == GetKey(map_it->second)  ||
                map_it->first == GetKey(map_it->second - 1));
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Searching hash_multimap... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            THashMultiap::const_iterator f = hmm.find(GetKey(i));
            CHECK_RESULT(f != hmm.end());
            CHECK_RESULT(f->second == i  ||  f->second == i+1);
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Cleaning hash_multimap... ";
        hmm.clear();
        CHECK_RESULT(hmm.empty());
        NcbiCout << "OK" << NcbiEndl;
    }}

    {{
        typedef hash_set<string> THashSet;
        THashSet hs;

        NcbiCout << "Populating hash_set... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            pair<THashSet::const_iterator, bool> res = hs.insert(GetKey(i));
            CHECK_RESULT(res.second);
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Re-populating hash_set... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            pair<THashSet::const_iterator, bool> res = hs.insert(GetKey(i));
            CHECK_RESULT(!res.second);
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Searching hash_set... ";
        for (int i = 0; i < kMaxHashSize; i++) {
            THashSet::const_iterator f = hs.find(GetKey(i));
            CHECK_RESULT(f != hs.end());
            CHECK_RESULT(*f == GetKey(i));
        }
        NcbiCout << "OK" << NcbiEndl;

        NcbiCout << "Cleaning hash_set... ";
        hs.clear();
        CHECK_RESULT(hs.empty());
        NcbiCout << "OK" << NcbiEndl;
    }}
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
 * Revision 1.1  2005/02/08 18:45:51  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
