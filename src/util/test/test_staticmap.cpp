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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Test program for CStaticArraySet<> and CStaticArrayMap<>
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiutil.hpp>
#include <stdlib.h>

#include <set>
#include <map>

#include <test/test_assert.h>  /* This header must go last */

#ifndef _DEBUG
# define _DEBUG 1
#endif
#include <util/static_map.hpp>
#include <util/static_set.hpp>

BEGIN_NCBI_SCOPE

class CTestStaticMap : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    void TestStaticMap(void) const;
    void TestStaticSet(void) const;

    bool m_Silent;
    int m_NumberOfElements;
    int m_LookupCount;

    enum ETestBadData {
        eBad_swap_at_begin,
        eBad_equal_at_begin,
        eBad_swap_in_middle,
        eBad_equal_in_middle,
        eBad_swap_at_end,
        eBad_equal_at_end,
        eBad_none
    };

    ETestBadData m_TestBadData;
};


void CheckValue(int value1, int value2)
{
    _ASSERT(value1 == value2);
}


void CheckValue(const pair<int, int>& value1, const pair<int, int>& value2)
{
    CheckValue(value1.first, value2.first);
    CheckValue(value1.second, value2.second);
}


void CheckValue(const pair<const int, int>& value1, 
                const pair<int, int>& value2)
{
    CheckValue(value1.first, value2.first);
    CheckValue(value1.second, value2.second);
}


inline
int distance(const int* p1, const int* p2)
{
    return int(p2 - p1);
}


inline
int distance(const pair<int, int>* p1, const pair<int, int>* p2)
{
    return int(p2 - p1);
}


template<class TRef, class TTst>
void CheckIter(const TRef& ref, const TTst& tst,
               const typename TRef::const_iterator& ref_iter,
               const typename TTst::const_iterator& tst_iter)
{
    _ASSERT((ref_iter == ref.end()) == (tst_iter == tst.end()));
    if ( ref_iter != ref.end() ) {
        CheckValue(*ref_iter, *tst_iter);
    }
}


template<class TRef, class TTst>
void TestAll(const typename TRef::key_type& key, const TRef& ref,
             const TTst& tst, const typename TTst::value_type arr[])
{
    CheckIter(ref, tst, ref.find(key), tst.find(key));

    _ASSERT(ref.count(key) == tst.count(key));

    {{ // test index_of()
        typename TRef::const_iterator ref_iter = ref.find(key);
        typename TTst::const_iterator tst_iter = tst.find(key);
        ssize_t tst_index = tst.index_of(key);
        _ASSERT((ref_iter == ref.end()) == (tst_index == tst.eNpos));
        if ( ref_iter != ref.end() ) {
            _ASSERT(tst_index >= 0 && tst_index < int(ref.size()));
            CheckValue(*ref_iter, arr[tst_index]);
            _ASSERT(tst_index == distance(tst.begin(), tst_iter));
            _ASSERT(tst_iter == tst.lower_bound(key));
        }
    }}
    
    CheckIter(ref, tst, ref.lower_bound(key), tst.lower_bound(key));
    CheckIter(ref, tst, ref.upper_bound(key), tst.upper_bound(key));
    
    CheckIter(ref, tst,
              ref.equal_range(key).first, tst.equal_range(key).first);
    CheckIter(ref, tst,
              ref.equal_range(key).second, tst.equal_range(key).second);
    
    CheckIter(tst, tst, tst.lower_bound(key), tst.equal_range(key).first);
    CheckIter(tst, tst, tst.upper_bound(key), tst.equal_range(key).second);
    
    _ASSERT(int(tst.count(key)) == distance(tst.lower_bound(key),
                                            tst.upper_bound(key)));
}

namespace {
    class abort_exception : public exception
    {
    };
    
    void AbortThrow()
    {
        throw abort_exception();
    }
}

void CTestStaticMap::TestStaticSet(void) const
{
    LOG_POST(Info << "Testing CStaticArraySet<int>");

    typedef set<int> TRef;
    typedef CStaticArraySet<int, less<int> > TTst;

    TRef ref;
    while ( int(ref.size()) < m_NumberOfElements ) {
        ref.insert(rand());
    }

    typedef int value_type;
    value_type* arr;
    AutoPtr<value_type, ArrayDeleter<value_type> > arr_del
        (arr = new value_type[m_NumberOfElements]);
    
    bool error = false;
    {{
        {{
            int index = 0;
            ITERATE ( TRef, it, ref ) {
                arr[index++] = *it;
            }
        }}
        if ( m_TestBadData != eBad_none && m_NumberOfElements >= 2 ) {
            size_t index = m_NumberOfElements;
            bool assign = false;
            switch ( m_TestBadData ) {
            case eBad_swap_at_begin:
                index = 0;
                assign = false;
                break;
            case eBad_swap_at_end:
                index = m_NumberOfElements-2;
                assign = false;
                break;
            case eBad_swap_in_middle:
                index = m_NumberOfElements/2;
                assign = false;
                break;
            case eBad_equal_at_begin:
                index = 0;
                assign = true;
                break;
            case eBad_equal_at_end:
                index = m_NumberOfElements-2;
                assign = true;
                break;
            case eBad_equal_in_middle:
                index = m_NumberOfElements/2;
                assign = true;
                break;
            default:
                break;
            }
            if ( assign ) {
                arr[index] = arr[index+1];
            }
            else {
                swap(arr[index], arr[index+1]);
            }
            error = true;
        }
    }}

    if ( error ) {
        SetAbortHandler(AbortThrow);
        try {
            TTst tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
        }
        catch ( abort_exception& /*exc*/ ) {
            LOG_POST(Info << "Test CStaticArraySet correctly detected error");
            return;
        }
        SetAbortHandler(0);
        ERR_POST(Fatal << "Test CStaticArraySet failed to detected error");
    }
    TTst tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);

    _ASSERT(ref.empty() == tst.empty());
    _ASSERT(ref.size() == tst.size());

    for ( int i = 0; i < m_LookupCount; ++i ) {
        int key = rand();

        TestAll(key, ref, tst, arr);
    }

    LOG_POST(Info << "Test CStaticArraySet passed");
}

void CTestStaticMap::TestStaticMap(void) const
{
    LOG_POST(Info << "Testing CStaticArrayMap<int, int>");

    typedef map<int, int, greater<int> > TRef;
    typedef CStaticArrayMap<int, int, greater<int> > TTst;

    TRef ref;
    while ( int(ref.size()) < m_NumberOfElements ) {
        ref.insert(TRef::value_type(rand(), rand()));
    }

    typedef pair<int, int> value_type;
    value_type* arr;
    AutoPtr<value_type, ArrayDeleter<value_type> > arr_del
        (arr = new value_type[m_NumberOfElements]);
    
    bool error = false;
    {{
        {{
            int index = 0;
            ITERATE ( TRef, it, ref ) {
                arr[index++] = TTst::value_type(it->first, it->second);
            }
        }}
        if ( m_TestBadData != eBad_none && m_NumberOfElements >= 2 ) {
            size_t index = m_NumberOfElements;
            bool assign = false;
            switch ( m_TestBadData ) {
            case eBad_swap_at_begin:
                index = 0;
                assign = false;
                break;
            case eBad_swap_at_end:
                index = m_NumberOfElements-2;
                assign = false;
                break;
            case eBad_swap_in_middle:
                index = m_NumberOfElements/2;
                assign = false;
                break;
            case eBad_equal_at_begin:
                index = 0;
                assign = true;
                break;
            case eBad_equal_at_end:
                index = m_NumberOfElements-2;
                assign = true;
                break;
            case eBad_equal_in_middle:
                index = m_NumberOfElements/2;
                assign = true;
                break;
            default:
                break;
            }
            if ( assign ) {
                arr[index] = arr[index+1];
            }
            else {
                swap(arr[index], arr[index+1]);
            }
            error = true;
        }
    }}

    if ( error ) {
        SetAbortHandler(AbortThrow);
        try {
            TTst tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
        }
        catch ( abort_exception& /*exc*/ ) {
            LOG_POST(Info << "Test CStaticArrayMap correctly detected error");
            return;
        }
        SetAbortHandler(0);
        ERR_POST(Fatal << "Test CStaticArrayMap failed to detected error");
    }
    TTst tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);

    _ASSERT(ref.empty() == tst.empty());
    _ASSERT(ref.size() == tst.size());

    for ( int i = 0; i < m_LookupCount; ++i ) {
        int key = rand();

        TestAll(key, ref, tst, arr);
    }

    LOG_POST(Info << "Test CStaticArrayMap passed");
}

void CTestStaticMap::Init(void)
{
    SetDiagPostLevel(eDiag_Info);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_staticmap",
                       "test CStaticArraySet and CStaticArrayMap classes");

    d->AddDefaultKey("t", "type",
                     "type of container to use",
                     CArgDescriptions::eString, "both");
    d->SetConstraint("t", (new CArgAllow_Strings)->
                     Allow("set")->
                     Allow("map")->
                     Allow("both"));

    d->AddFlag("s",
               "silent operation - do not print log");

    d->AddDefaultKey("n", "numberOfElements",
                     "number of elements to put into container",
                     CArgDescriptions::eInteger, "100");
    d->AddDefaultKey("c", "count",
                     "how may times to run lookup test",
                     CArgDescriptions::eInteger, "100000");

    d->AddDefaultKey("b", "badDataType",
                     "type of bad data to test",
                     CArgDescriptions::eString, "all");
    d->SetConstraint("b", (new CArgAllow_Strings)->
                     Allow("all")->
                     Allow("swap_at_begin")->
                     Allow("swap_in_middle")->
                     Allow("swap_at_end")->
                     Allow("equal_at_begin")->
                     Allow("equal_in_middle")->
                     Allow("equal_at_end")->
                     Allow("none"));

    SetupArgDescriptions(d.release());
}

int CTestStaticMap::Run(void)
{
    const CArgs& args = GetArgs();

    m_Silent = args["s"];
    m_NumberOfElements = args["n"].AsInteger();
    m_LookupCount = args["c"].AsInteger();

    string type = args["t"].AsString();

    string bad_type = args["b"].AsString();
    if ( bad_type == "all" ) {
        for ( int i = 0; i != eBad_none; ++i ) {
            m_TestBadData = ETestBadData(i);
            if ( type == "set" || type == "both" ) {
                TestStaticSet();
            }
            if ( type == "map" || type == "both" ) {
                TestStaticMap();
            }
        }
        m_TestBadData = eBad_none;
    }
    else {
        if ( bad_type == "swap_at_begin" )
            m_TestBadData = eBad_swap_at_begin;
        else if ( bad_type == "swap_in_middle" )
            m_TestBadData = eBad_swap_in_middle;
        else if ( bad_type == "swap_at_end" )
            m_TestBadData = eBad_swap_at_end;
        else if ( bad_type == "equal_at_begin" )
            m_TestBadData = eBad_equal_at_begin;
        else if ( bad_type == "equal_in_middle" )
            m_TestBadData = eBad_equal_in_middle;
        else if ( bad_type == "equal_at_end" )
            m_TestBadData = eBad_equal_at_end;
        else
            m_TestBadData = eBad_none;
    }
    
    if ( type == "set" || type == "both" ) {
        TestStaticSet();
    }
    if ( type == "map" || type == "both" ) {
        TestStaticMap();
    }

    LOG_POST("All tests passed.");
    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestStaticMap().AppMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/01/09 12:53:37  ivanov
 * Use LOG_POST instead of cout
 *
 * Revision 1.9  2005/10/27 15:02:32  vasilche
 * Fixed warning about unused exception variable.
 *
 * Revision 1.8  2005/10/27 13:30:12  vasilche
 * Check detection of CStaticMap/Set in runtime.
 *
 * Revision 1.7  2005/04/25 19:05:24  ivanov
 * Fixed compilation warnings on 64-bit Worshop compiler
 *
 * Revision 1.6  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/01/23 18:59:18  vasilche
 * Added test of order validation.
 *
 * Revision 1.4  2004/01/23 18:50:48  vasilche
 * Fixed for MSVC compiler.
 *
 * Revision 1.3  2004/01/23 18:15:48  vasilche
 * Fixed initialization of test map.
 *
 * Revision 1.2  2004/01/23 18:09:42  vasilche
 * Fixed for WorkShop compiler.
 *
 * Revision 1.1  2004/01/23 18:02:25  vasilche
 * Cleaned implementation of CStaticArraySet & CStaticArrayMap.
 * Added test utility test_staticmap.
 *
 * ===========================================================================
 */
