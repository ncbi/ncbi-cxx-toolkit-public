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
 *   Test and benchmark for String <-> double conversions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbi_system.hpp>

// TLS static variable support in case there is no compiler support
//#undef NCBI_TLS_VAR
#ifdef NCBI_TLS_VAR
# define DECLARE_TLS_VAR(type, var) static NCBI_TLS_VAR type var
#else
# include <pthread.h>
# include <corelib/ncbimtx.hpp>
template<class V> class CStaticTLS {
private:
    typedef pthread_key_t key_type;
    mutable key_type m_Key;
    template<class A> struct SCaster {
        static A FromVoidP(void* p) {
            return A(reinterpret_cast<intptr_t>(p));
        }
        static const void* ToVoidP(A v) {
            return reinterpret_cast<const void*>(intptr_t(v));
        }
    };
    template<class A> struct SCaster<A*> {
        static A* FromVoidP(void* p) {
            return reinterpret_cast<A*>(p);
        }
        static const void* ToVoidP(A* v) {
            return reinterpret_cast<const void*>(v);
        }
    };
    key_type x_GetKey(void) const {
        return m_Key? m_Key: x_GetKeyLong();
    }
    key_type x_GetKeyLong(void) const {
        DEFINE_STATIC_FAST_MUTEX(s_InitMutex);
        NCBI_NS_NCBI::CFastMutexGuard guard(s_InitMutex);
        if ( !m_Key ) {
            _ASSERT(sizeof(value_type) <= sizeof(void*));
            key_type new_key = 0;
            do {
                _VERIFY(pthread_key_create(&new_key, 0) == 0);
            } while ( !new_key );
            pthread_setspecific(new_key, 0);
            m_Key = new_key;
        }
        return m_Key;
    }
public:
    typedef V value_type;
    operator value_type() const {
        return SCaster<value_type>::FromVoidP(pthread_getspecific(x_GetKey()));
    }
    void operator=(const value_type& v) {
        pthread_setspecific(x_GetKey(), SCaster<value_type>::ToVoidP(v));
    }
};
# define DECLARE_TLS_VAR(type, var) static CStaticTLS<type> var
#endif

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

static CAtomicCounter alloc_count;
static CAtomicCounter object_count;

void error(const char* msg)
{
    ERR_POST(Fatal<<msg);
}

void message(const char* msg)
{
    LOG_POST(msg);
}

size_t max_total, max_resident, max_shared;

void message(const char* msg,
             const char* msg1, double t1,
             const char* msg2, double t2,
             size_t COUNT)
{
    if ( 0 ) {
        LOG_POST(msg
                 <<'\n'<<setw(40) << msg1 << ": "<<t1*1e9/COUNT<<" usec"
                 <<'\n'<<setw(40) << msg2 << ": "<<t2*1e9/COUNT<<" usec");
    }
    size_t total, resident, shared;
    if ( GetMemoryUsage(&total, &resident, &shared) ) {
        max_total = max(max_total, total);
        max_resident = max(max_resident, resident);
        max_shared = max(max_shared, shared);
        if ( 0 ) {
            LOG_POST("Alloc: "<<alloc_count.Get()<<
                     " "<<object_count.Get()<<'\n'<<
                     "Current memory: "<<total<<" "<<resident<<" "<<shared);
        }
    }
}

DECLARE_TLS_VAR(const char*, s_CurrentStep);
DECLARE_TLS_VAR(bool, s_CurrentInHeap);

class CObjectWithNew
{
    enum ECounter {
        eCounter_new      = 0xbabeface,
        eCounter_deleted  = 0xdeadbeef,
        eCounter_heap     = 0x1,
        eCounter_static   = 0x0
    };
    enum {
        STACK_THRESHOLD   = 1*1024
    };
    static bool IsNewInHeap(CObjectWithNew* ptr) {
        switch ( ptr->m_Counter ) {
        case eCounter_new:
        {
            char stackObject;
            const char* stackObjectPtr = &stackObject;
            const char* objectPtr = reinterpret_cast<const char*>(ptr);
            bool inStack =
                (objectPtr > stackObjectPtr) &&
                (objectPtr < stackObjectPtr + STACK_THRESHOLD);
            if ( inStack ) {
                ERR_POST(Fatal<<"!!!InStack,"
                         " s_CurrentStep: "<<s_CurrentStep<<
                         " s_CurrentInHeap: "<<s_CurrentInHeap<<
                         " stackObjectPtr: "<<(void*)stackObjectPtr<<
                         " objectPtr: "<<(void*)objectPtr<<
                         CStackTrace());
            }
            _ASSERT(s_CurrentInHeap);
            _ASSERT(!inStack);
            return !inStack;
        }
        case 0:
            _ASSERT(!s_CurrentInHeap);
            return false;
        }
        error("invalid CObjectWithNew::new");
        return false;
    }
    static bool IsInHeap(const CObjectWithNew* ptr) {
        switch ( ptr->m_Counter ) {
        case eCounter_heap: return true;
        case eCounter_static: return false;
        }
        error("invalid CObjectWithNew");
        return false;
    }
    static void RegisterNew(CObjectWithNew* ptr) {
        ptr->m_Counter = eCounter_new;
    }
    static void RegisterDelete(CObjectWithNew* ptr) {
        ptr->m_Counter = eCounter_deleted;
    }
public:
    CObjectWithNew(void) {
        m_Counter = IsNewInHeap(this)? eCounter_heap: eCounter_static;
        _ASSERT(s_CurrentInHeap == IsInHeap());
        object_count.Add(1);
    }
    virtual ~CObjectWithNew() {
        if ( m_Counter != eCounter_heap && m_Counter != eCounter_static ) {
            error("invalid CObjectWithNew::delete");
        }
        object_count.Add(-1);
        m_Counter = eCounter_deleted;
    }

    bool IsInHeap(void) const {
        return IsInHeap(this);
    }
    static void Delete(CObjectWithNew* ptr) {
        if ( ptr && IsInHeap(ptr) ) {
            delete ptr;
        }
    }

    static void* operator new(size_t s) {
        alloc_count.Add(1);
        CObjectWithNew* ptr = (CObjectWithNew*)::operator new(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr) {
        alloc_count.Add(-1);
        RegisterDelete((CObjectWithNew*)ptr);
        ::operator delete(ptr);
    }
    
private:
    unsigned m_Counter;

private:
    CObjectWithNew(const CObjectWithNew&);
    void operator=(CObjectWithNew&);
};

DECLARE_TLS_VAR(void*, s_LastNewPtr);

class CObjectWithTLS
{
    enum ECounter {
        eCounter_heap     = 0x1,
        eCounter_static   = 0x0
    };
    static bool IsNewInHeap(CObjectWithTLS* ptr) {
        if ( GetNewPtr() == ptr ) {
            SetNewPtr(0);
            return true;
        }
        return false;
    }
    static bool IsInHeap(const CObjectWithTLS* ptr) {
        if ( ptr->m_Counter == eCounter_heap ) {
            return true;
        }
        if ( ptr->m_Counter != eCounter_static ) {
            error("invalid CObjectWithTLS");
        }
        return false;
    }
    static void RegisterNew(CObjectWithTLS* ptr) {
        _ASSERT(GetNewPtr() == 0);
        SetNewPtr(ptr);
    }
    static void RegisterDelete(CObjectWithTLS* ptr) {
        if ( GetNewPtr() == ptr ) {
            SetNewPtr(0);
        }
        else {
            _ASSERT(GetNewPtr() == 0);
        }
    }
public:
    static void SetNewPtr(void* ptr) {
        s_LastNewPtr = ptr;
    }
    static void* GetNewPtr() {
        return s_LastNewPtr;
    }

    enum EObjectPlace {
        eUnknown,
        eSubObject
    };
    static bool IsPlaceHeap(EObjectPlace place) {
        return place == eUnknown && s_CurrentInHeap;
    }
    CObjectWithTLS(EObjectPlace place = eUnknown) {
        if ( s_CurrentInHeap ) {
            if ( rand() % 10 == 0 ) {
                throw runtime_error("CObjectWithTLS");
            }
        }
        m_Counter = IsNewInHeap(this)? eCounter_heap: eCounter_static;
        _ASSERT(IsPlaceHeap(place) == IsInHeap());
        object_count.Add(1);
    }
    virtual ~CObjectWithTLS() {
        if ( m_Counter != eCounter_heap && m_Counter != eCounter_static ) {
            error("invalid CObjectWithTLS::delete");
        }
        object_count.Add(-1);
    }

    bool IsInHeap(void) const {
        return IsInHeap(this);
    }
    static void Delete(CObjectWithTLS* ptr) {
        if ( ptr && IsInHeap(ptr) ) {
            delete ptr;
        }
    }

    static void* operator new(size_t s) {
        alloc_count.Add(1);
        CObjectWithTLS* ptr = (CObjectWithTLS*)::operator new(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr) {
        alloc_count.Add(-1);
        RegisterDelete((CObjectWithTLS*)ptr);
        ::operator delete(ptr);
    }
    
private:
    unsigned m_Counter;

private:
    CObjectWithTLS(const CObjectWithTLS&);
    void operator=(CObjectWithTLS&);
};

class CObjectWithTLS2 : public CObjectWithTLS
{
public:
    CObjectWithTLS2(void)
        {
            if ( s_CurrentInHeap ) {
                if ( rand() % 10 == 0 ) {
                    throw runtime_error("CObjectWithTLS2");
                }
            }
        }
};

class CObjectWithTLS3 : public CObjectWithTLS
{
public:
    CObjectWithTLS3(void)
        : m_SubObject(eSubObject)
        {
            if ( s_CurrentInHeap ) {
                if ( rand() % 10 == 0 ) {
                    throw runtime_error("CObjectWithTLS3");
                }
            }
        }

    CObjectWithTLS m_SubObject;
};

template<class E, size_t S, bool Zero = true>
class CArray {
public:
    static void* operator new(size_t s) {
        void* ptr = ::operator new(s);
        if ( Zero ) {
            memset(ptr, 0, s);
        }
        else {
            unsigned* uptr = (unsigned*)ptr;
            for ( size_t i = 0; i < s/sizeof(unsigned); ++i ) {
                uptr[i] = 0xbabeface;
            }
        }
        return ptr;
    }
    static void operator delete(void* ptr) {
        ::operator delete(ptr);
    }

    E m_Array[S];
};


void check_cnts(size_t expected = 0,
                size_t expected_static = 0,
                void* expected_ptr = 0)
{
    if ( expected == ~0u ) {
        expected = 0;
    }
    else {
        expected = 1;
    }
    if ( expected == 0 ) {
        if ( alloc_count.Get() != expected-expected_static )
            ERR_POST(Fatal<<"alloc_count: "<<alloc_count.Get());
        if ( object_count.Get() != expected )
            ERR_POST(Fatal<<"object_count: "<<object_count.Get());
    }
    void *ptr = CObjectWithTLS::GetNewPtr();
    if ( ptr != expected_ptr )
        ERR_POST(Fatal<<"new ptr: " << ptr);
}


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestTlsObjectApp : public CThreadedApp
{
public:
    void RunTest(void);
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
};


bool CTestTlsObjectApp::Thread_Run(int /*idx*/)
{
    try {
        RunTest();
        RunTest();
        RunTest();
        return true;
    }
    NCBI_CATCH_ALL("Test failed");
    size_t total, resident, shared;
    if ( GetMemoryUsage(&total, &resident, &shared) ) {
        ERR_POST("Alloc: "<<alloc_count.Get()<<" "<<object_count.Get()<<'\n'<<
                 "Memory: "<<total<<" "<<resident<<" "<<shared);
    }
    return false;
}


void CTestTlsObjectApp::RunTest(void)
{
    const size_t COUNT = 100000;
    const size_t OBJECT_SIZE = sizeof(CObjectWithNew);
    for ( int t = 0; t < 1; ++t ) {
        // prealloc
        {
            size_t size = (OBJECT_SIZE+16)*COUNT;
            void* p = ::operator new(size);
            memset(p, 1, size);
            ::operator delete(p);
        }
        {
            const size_t COUNT2 = COUNT*2;
            void** p = new void*[COUNT2];
            for ( size_t i = 0; i < COUNT2; ++i ) {
                p[i] = ::operator new(OBJECT_SIZE);
            }
            for ( size_t i = 0; i < COUNT2; ++i ) {
                ::operator delete(p[i]);
            }
            delete[] p;
        }
    }
    CStopWatch sw;
    for ( int t = 0; t < 1; ++t ) {
        void** ptr = new void*[COUNT];
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = ::operator new(OBJECT_SIZE);
        }
        double t1 = sw.Elapsed();
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            ::operator delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("plain malloc", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            CObjectWithTLS::SetNewPtr(0);
        }
        double t1 = sw.Elapsed();
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(CObjectWithTLS::GetNewPtr() == 0);
        }
        double t2 = sw.Elapsed();
        message("tls", "set", t1, "get", t2, COUNT);
    }
    check_cnts();
    {
        CObjectWithNew** ptr = new CObjectWithNew*[COUNT];
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = 0;
        }
        sw.Start();
        s_CurrentStep = "new CObjectWithNew";
        s_CurrentInHeap = true;
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithNew;
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(ptr[i]->IsInHeap());
        }
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            CObjectWithNew::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithNew", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        CObjectWithTLS** ptr = new CObjectWithTLS*[COUNT];
        sw.Start();
        s_CurrentStep = "new CObjectWithTLS";
        s_CurrentInHeap = true;
        for ( size_t i = 0; i < COUNT; ++i ) {
            try {
                switch ( rand()%3 ) {
                case 0: ptr[i] = new CObjectWithTLS; break;
                case 1: ptr[i] = new CObjectWithTLS2; break;
                case 2: ptr[i] = new CObjectWithTLS3; break;
                }
            }
            catch ( exception& ) {
                ptr[i] = 0;
            }
            _ASSERT(!CObjectWithTLS::GetNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->IsInHeap());
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            CObjectWithTLS::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithTLS", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        CObjectWithNew** ptr = new CObjectWithNew*[COUNT];
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = 0;
        }
        sw.Start();
        s_CurrentStep = "new CObjectWithNew()";
        s_CurrentInHeap = true;
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithNew();
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(ptr[i]->IsInHeap());
        }
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            CObjectWithNew::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithNew()", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        CObjectWithTLS** ptr = new CObjectWithTLS*[COUNT];
        sw.Start();
        s_CurrentStep = "new CObjectWithTLS()";
        s_CurrentInHeap = true;
        for ( size_t i = 0; i < COUNT; ++i ) {
            try {
                switch ( rand()%3 ) {
                case 0: ptr[i] = new CObjectWithTLS(); break;
                case 1: ptr[i] = new CObjectWithTLS2(); break;
                case 2: ptr[i] = new CObjectWithTLS3(); break;
                }
            }
            catch ( exception& ) {
                ptr[i] = 0;
            }
            _ASSERT(!CObjectWithTLS::GetNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->IsInHeap());
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            CObjectWithTLS::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithTLS()", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        sw.Start();
        s_CurrentStep = "CObjectWithNew[]";
        CArray<CObjectWithNew, COUNT>* arr =
            new CArray<CObjectWithNew, COUNT>;
        double t1 = sw.Elapsed();
        check_cnts(COUNT, COUNT);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(!arr->m_Array[i].IsInHeap());
        }
        sw.Start();
        delete arr;
        double t2 = sw.Elapsed();
        message("static CObjectWithNew", "create", t1, "delete", t2, COUNT);
    }
    check_cnts();
    {
        sw.Start();
        s_CurrentStep = "CObjectWithTLS[]";
        CArray<CObjectWithTLS, COUNT, false>* arr =
            new CArray<CObjectWithTLS, COUNT, false>;
        double t1 = sw.Elapsed();
        check_cnts(COUNT, COUNT);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(!arr->m_Array[i].IsInHeap());
        }
        sw.Start();
        delete arr;
        double t2 = sw.Elapsed();
        message("static CObjectWithTLS", "create", t1, "delete", t2, COUNT);
    }
    check_cnts();
}

bool CTestTlsObjectApp::TestApp_Init(void)
{
    s_NumThreads = 20;
    NcbiCout << "Testing TLS variant of CObject counter init." << NcbiEndl;
    return true;
}


bool CTestTlsObjectApp::TestApp_Exit(void)
{
    check_cnts(~0u);
    LOG_POST("Max memory VS: "<<max_total/(1024*1024.)<<" MB"<<
             " RSS: "<<max_resident/(1024*1024.)<<" MB"<<
             " SHR: "<<max_shared/(1024*1024.)<<" MB");
    NcbiCout << "Test completed." << NcbiEndl;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Execute main application function
    return CTestTlsObjectApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
