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

#include <common/test_assert.h>  /* This header must go last */

#ifndef _DEBUG
# define _DEBUG 1
# ifdef _DEBUG_ARG
#  undef _DEBUG_ARG
# endif
# define _DEBUG_ARG(x) x
#endif
#include <util/static_map.hpp>
#include <util/static_set.hpp>


BEGIN_NCBI_SCOPE

class CTestStaticMap : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    template<class MapType, class StaticType>
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


template<class Type1, class Type2>
void CheckValue(const Type1& value1, const Type2& value2)
{
    _ASSERT(value1 == value2);
}


template<class Type11, class Type12, class Type21, class Type22>
void CheckValue(const pair<Type11, Type12>& value1,
                const pair<Type21, Type22>& value2)
{
    CheckValue(value1.first, value2.first);
    CheckValue(value1.second, value2.second);
}


template<class Type11, class Type12, class Type21, class Type22>
void CheckValue(const pair<Type11, Type12>& value1,
                const SStaticPair<Type21, Type22>& value2)
{
    CheckValue(value1.first, value2.first);
    CheckValue(value1.second, value2.second);
}


template<class Type11, class Type12, class Type21, class Type22>
void CheckValue(const SStaticPair<Type11, Type12>& value1,
                const SStaticPair<Type21, Type22>& value2)
{
    CheckValue(value1.first, value2.first);
    CheckValue(value1.second, value2.second);
}


template<class Type>
inline
int distance(const Type* p1, const Type* p2)
{
    return int(p2 - p1);
}


class CZeroClearer
{
public:
    CZeroClearer(size_t size)
    {
        memset(this, 0, size);
    }
};


template<class Type>
class CZeroCreator : CZeroClearer
{
public:
    CZeroCreator(void)
        : CZeroClearer(sizeof(*this)),
          m_Object()
    {
    }
    template<class Arg1>
    CZeroCreator(Arg1 arg1)
        : CZeroClearer(sizeof(*this)),
          m_Object(arg1)
    {
    }
    template<class Arg1, class Arg2>
    CZeroCreator(Arg1 arg1, Arg2 arg2)
        : CZeroClearer(sizeof(*this)),
          m_Object(arg1, arg2)
    {
    }
    template<class Arg1, class Arg2, class Arg3>
    CZeroCreator(Arg1 arg1, Arg2 arg2, Arg3 arg3)
        : CZeroClearer(sizeof(*this)),
          m_Object(arg1, arg2, arg3)
    {
    }
    template<class Arg1, class Arg2, class Arg3, class Arg4>
    CZeroCreator(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
        : CZeroClearer(sizeof(*this)),
          m_Object(arg1, arg2, arg3, arg4)
    {
    }

    Type& Get(void)
    {
        return m_Object;
    }
    Type& operator*(void)
    {
        return Get();
    }
    Type* operator->(void)
    {
        return &Get();
    }
private:
    Type m_Object;
};


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


template<class TRef, class TTst, class TArrValue>
void TestAll(const typename TRef::key_type& key, const TRef& ref,
             const TTst& tst, const TArrValue arr[])
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


// convert Abort() call to throwing abort_exception
// to verify that Abort() is actually called
class abort_exception : public exception
{
};


static void AbortThrow(void)
{
    throw abort_exception();
}


// verify that the error message is a

class CExpectDiagHandler : public CDiagHandler
{
public:
    CExpectDiagHandler(void)
        : m_ExpectOrder(0),
          m_ExpectUnsafe(0),
          m_ExpectConvert(0),
          m_Active(false)
    {
    }
    ~CExpectDiagHandler(void)
    {
    }
    virtual void Post(const SDiagMessage& mess)
    {
        PostToConsole(mess);
        if ( !m_Active ) {
            return;
        }

        string s;
        mess.Write(s);
        if ( s.find("keys are out of order") != NPOS ) {
            if ( m_ExpectOrder ) {
                --m_ExpectOrder;
                return;
            }
        }
        if ( s.find("static array type is not MT-safe") != NPOS ) {
            if ( m_ExpectUnsafe ) {
                --m_ExpectUnsafe;
                return;
            }
        }
        if ( s.find("converting static array") != NPOS ) {
            if ( m_ExpectConvert ) {
                --m_ExpectConvert;
                return;
            }
        }
        m_UnexpectedMessages.push_back(s);
    }

    void Check(void)
    {
        m_Active = false;
        try {
            if ( m_ExpectOrder ) {
                ERR_POST(Fatal<<
                         "Didn't get expected message about key order");
            }
            if ( m_ExpectUnsafe ) {
                ERR_POST(Fatal<<
                         "Didn't get expected message about MT-safety");
            }
            if ( m_ExpectConvert ) {
                ERR_POST(Fatal<<
                         "Didn't get expected message about conversion");
            }
            if ( !m_UnexpectedMessages.empty() ) {
                ITERATE ( vector<string>, it, m_UnexpectedMessages ) {
                    ERR_POST("Unexpected message: "<<*it);
                }
                ERR_POST(Fatal<<"Got unexpected messages.");
            }
        }
        catch ( exception& ) {
            throw;
        }
    }
    void Start(void)
    {
        m_Active = true;
        SetDiagHandler(this, false);
    }
    void Stop(void)
    {
        SetDiagStream(&cerr);
    }
    
    int m_ExpectOrder, m_ExpectUnsafe, m_ExpectConvert;
    bool m_Active;

    vector<string> m_UnexpectedMessages;
};


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

    CExpectDiagHandler expect;

    if ( error ) {
        expect.m_ExpectOrder = 1;
        ERR_POST("The following fatal error message about key order is expected.");
        SetAbortHandler(AbortThrow);
        expect.Start();
        try {
            CZeroCreator<TTst> tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
        }
        catch ( abort_exception& /*exc*/ ) {
            SetAbortHandler(0);
            expect.Stop();
            LOG_POST(Info << "Test CStaticArraySet correctly detected error");
            expect.Check();
            return;
        }
        catch ( exception& /*exc*/ ) {
            SetAbortHandler(0);
            expect.Stop();
            throw;
        }
        SetAbortHandler(0);
        expect.Stop();
        ERR_POST(Fatal << "Test CStaticArraySet failed to detected error");
    }
    expect.Start();
    CZeroCreator<TTst> tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
    expect.Stop();
    expect.Check();

    _ASSERT(ref.empty() == tst->empty());
    _ASSERT(ref.size() == tst->size());

    for ( int i = 0; i < m_LookupCount; ++i ) {
        int key = rand();

        TestAll(key, ref, *tst, arr);
    }

    LOG_POST(Info << "Test CStaticArraySet passed");
}

void assign(string& dst, const string& src)
{
    dst = src;
}

void assign(const char*& dst, const string& src)
{
    dst = src.c_str();
}

template<class MapType, class StaticType>
void CTestStaticMap::TestStaticMap(void) const
{
    typedef StaticType value_type;
    typedef MapType TTst;

    const bool is_map2 =
        typeid(typename TTst::value_type) == typeid(SStaticPair<int, string>);
    const bool is_map1 = !is_map2;
    const bool is_val2 =
        typeid(value_type) == typeid(SStaticPair<int, string>);
    const bool is_val3 =
        typeid(value_type) == typeid(SStaticPair<int, const char*>);
    const bool is_val1 = !is_val2 && !is_val3;
    LOG_POST(Info<<"Testing CStatic"<<
             (is_map2? "Pair": "")<<"ArrayMap<int, string>("<<
             (is_val1? "std::pair<int, string>":
              (is_val2? "SStaticPair<int, string>":
               "SStaticPair<int, const char*>"))<<")");
    
    typedef map<int, string, greater<int> > TRef;
    TRef ref;
    while ( int(ref.size()) < m_NumberOfElements ) {
        string s(rand()%10, '?');
        ref.insert(TRef::value_type(rand(), s));
    }

    value_type* arr;
    AutoPtr<value_type, ArrayDeleter<value_type> > arr_del
        (arr = new value_type[m_NumberOfElements]);
    
    bool error = false;
    {{
        {{
            int index = 0;
            ITERATE ( TRef, it, ref ) {
                arr[index].first = it->first;
                assign(arr[index].second, it->second);
                ++index;
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

    CExpectDiagHandler expect;

    if ( is_val1 ) {
        expect.m_ExpectUnsafe = 2;
        ERR_POST("The following two error messages about non-MT-safe type is expected.");
    }
    else if ( is_val2 ) {
        expect.m_ExpectUnsafe = 1;
        ERR_POST("The following error message about non-MT-safe type is expected.");
    }
    if ( !(is_map1 && is_val1) && !(is_map2 && is_val2) ) {
        expect.m_ExpectConvert = 1;
        ERR_POST("The following error message about conversion is expected.");
    }

    if ( error ) {
        expect.m_ExpectOrder = 1;
        ERR_POST("The following fatal error message about key order is expected.");
        SetAbortHandler(AbortThrow);
        expect.Start();
        try {
            CZeroCreator<TTst> tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
        }
        catch ( abort_exception& /*exc*/ ) {
            SetAbortHandler(0);
            expect.Stop();
            LOG_POST(Info << "Test CStaticArrayMap correctly detected error");
            expect.Check();
            return;
        }
        catch ( exception& /*exc*/ ) {
            SetAbortHandler(0);
            expect.Stop();
            throw;
        }
        SetAbortHandler(0);
        expect.Stop();
        ERR_POST(Fatal << "Test CStaticArrayMap failed to detected error");
    }
    expect.Start();
    CZeroCreator<TTst> tst(arr, sizeof(*arr)*m_NumberOfElements, __FILE__, __LINE__);
    expect.Stop();
    expect.Check();

    _ASSERT(ref.empty() == tst->empty());
    _ASSERT(ref.size() == tst->size());

    for ( int i = 0; i < m_LookupCount; ++i ) {
        int key = rand();

        TestAll(key, ref, *tst, arr);
    }

    LOG_POST(Info << "Test CStaticArrayMap passed");
}

void CTestStaticMap::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    TParamStaticArrayCopyWarning::SetDefault(true);
    TParamStaticArrayUnsafeTypeWarning::SetDefault(true);

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
                typedef CStaticArrayMap<int, string, greater<int> > TMap1;
                typedef CStaticPairArrayMap<int, string, greater<int> > TMap2;
                typedef pair<int, string> TVal1;
                typedef SStaticPair<int, string> TVal2;
                typedef SStaticPair<int, const char*> TVal3;
                TestStaticMap<TMap1, TVal1>();
                TestStaticMap<TMap1, TVal2>();
                TestStaticMap<TMap1, TVal3>();
                TestStaticMap<TMap2, TVal2>();
                TestStaticMap<TMap2, TVal3>();
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
        typedef CStaticArrayMap<int, string, greater<int> > TMap1;
        typedef CStaticPairArrayMap<int, string, greater<int> > TMap2;
        typedef pair<int, string> TVal1;
        typedef SStaticPair<int, string> TVal2;
        typedef SStaticPair<int, const char*> TVal3;
        TestStaticMap<TMap1, TVal1>();
        TestStaticMap<TMap1, TVal2>();
        TestStaticMap<TMap1, TVal3>();
        TestStaticMap<TMap2, TVal2>();
        TestStaticMap<TMap2, TVal3>();
    }

    LOG_POST("All tests passed.");
    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestStaticMap().AppMain(argc, argv);
}
