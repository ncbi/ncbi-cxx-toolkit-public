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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Test for limited-size map
 *
 */

#include <ncbi_pch.hpp>
#include <util/limited_size_map.hpp>
#include <corelib/ncbiapp.hpp>
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
};


void CTestApp::Init(void)
{
    //const CArgs& args = GetArgs();
}

typedef limited_size_map<int, string> TMap;

void check(const TMap& m, CTempString elems)
{
    list<CTempString> ee;
    NStr::Split(elems, ",", ee, NStr::fSplit_Tokenize);
    _ASSERT(ee.size() == m.size());
    TMap::const_iterator m_it = m.begin();
    ITERATE ( list<CTempString>, it, ee ) {
        _ASSERT(m_it != m.end());
        CTempString e = *it;
        SIZE_TYPE colon = e.find(':');
        _ASSERT(colon != NPOS);
        _ASSERT(m_it->first == NStr::StringToNumeric<int>(e.substr(0, colon)));
        _ASSERT(m_it->second == e.substr(colon+1));
        ++m_it;
    }
    _ASSERT(m_it == m.end());
}

void check(TMap& m, CTempString elems)
{
    list<CTempString> ee;
    NStr::Split(elems, ",", ee, NStr::fSplit_Tokenize);
    _ASSERT(ee.size() == m.size());
    TMap::iterator m_it = m.begin();
    ITERATE ( list<CTempString>, it, ee ) {
        _ASSERT(m_it != m.end());
        CTempString e = *it;
        SIZE_TYPE colon = e.find(':');
        _ASSERT(colon != NPOS);
        _ASSERT(m_it->first == NStr::StringToNumeric<int>(e.substr(0, colon)));
        _ASSERT(m_it->second == e.substr(colon+1));
        ++m_it;
    }
    _ASSERT(m_it == m.end());
}

int CTestApp::Run(void)
{
    TMap m;
    m[3] = "a"; // 3
    check(m, "3:a");
    m[2] = "b"; // 3,2
    check(m, "2:b,3:a");
    m[10] = "c"; // 3,2,10
    check(m, "2:b,3:a,10:c");
    m.set_size_limit(2); // 2,10
    check(m, "2:b,10:c");
    _VERIFY(m.find(2) != m.end()); // 10,2
    check(m, "2:b,10:c");
    m.set_size_limit(3); // 10,2
    check(m, "2:b,10:c");
    m.insert(TMap::value_type(8,"d")); // 10,2,8
    check(m, "2:b,8:d,10:c");
    m.insert(TMap::value_type(9, "e")); // 2,8,9
    check(m, "2:b,8:d,9:e");
    _VERIFY(!m.insert(TMap::value_type(2, "e")).second); // 8,9,2
    check(m, "2:b,8:d,9:e");
    _VERIFY(m.lower_bound(8) != m.end() &&
            m.lower_bound(8)->first == 8); // 9,2,8
    check(m, "2:b,8:d,9:e");
    m[2]; // 9,8,2
    check(m, "2:b,8:d,9:e");
    m[1] = "f"; // 8,2,1
    check(m, "1:f,2:b,8:d");
    m.erase(m.find(2)); // 8,1
    check(m, "1:f,8:d");
    m.clear();
    check(m, "");
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApp().AppMain(argc, argv);
}
