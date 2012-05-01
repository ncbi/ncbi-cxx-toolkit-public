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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiatomic.hpp>

#include <common/test_assert.h>  /* This header must go last */

#ifdef NCBI_COMPILER_MSVC
# define NCBI_TLS_VAR __declspec(thread)
#else
# define NCBI_TLS_VAR __thread
#endif
#ifdef NCBI_OS_DARWIN
# undef NCBI_TLS_VAR
# include <pthread.h>
#endif

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

void message(const char* msg,
             const char* msg1, double t1,
             const char* msg2, double t2,
             size_t COUNT)
{
    LOG_POST(msg
             <<'\n'<<setw(40) << msg1 << ": "<<t1*1e9/COUNT<<" usec"
             <<'\n'<<setw(40) << msg2 << ": "<<t2*1e9/COUNT<<" usec");
}

class CObjectWithNew
{
    enum ECounter {
        eCounter_new      = 0xbabeface,
        eCounter_deleted  = 0xdeadbeef,
        eCounter_heap     = 0x1,
        eCounter_static   = 0x0
    };
    enum {
        STACK_THRESHOLD   = 16*1024
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
            return !inStack;
        }
        case 0:
            return false;
        }
        //error("invalid CObjectWithNew::new");
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
    CObjectWithNew() {
        if ( IsNewInHeap(this) ) {
            m_Counter = eCounter_heap;
        }
        else {
            m_Counter = eCounter_static;
        }
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
        if ( IsInHeap(ptr) ) {
            delete ptr;
        }
    }

    static void* operator new(size_t s) {
        alloc_count.Add(1);
        CObjectWithNew* ptr = (CObjectWithNew*)malloc(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr, size_t /*s*/) {
        alloc_count.Add(-1);
        RegisterDelete((CObjectWithNew*)ptr);
        free(ptr);
    }
    
private:
    unsigned m_Counter;

private:
    CObjectWithNew(const CObjectWithNew&);
    void operator=(CObjectWithNew&);
};

#ifdef NCBI_TLS_VAR
static NCBI_TLS_VAR void* s_LastNewPtr;
#else
static pthread_key_t s_LastNewPtr_key;
#endif

class CObjectWithTLS
{
    enum ECounter {
        eCounter_heap     = 0x1,
        eCounter_static   = 0x0
    };
    static bool IsNewInHeap(CObjectWithTLS* ptr) {
        return GetNewPtr() == ptr;
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
        SetNewPtr(ptr);
    }
    static void RegisterDelete(CObjectWithTLS* /*ptr*/) {
        SetNewPtr(0);
    }
public:
    static void SetNewPtr(void* ptr) {
#ifdef NCBI_TLS_VAR
        s_LastNewPtr = ptr;
#else
        pthread_setspecific(s_LastNewPtr_key, ptr);
#endif
    }
    static void* GetNewPtr() {
#ifdef NCBI_TLS_VAR
        return s_LastNewPtr;
#else
        return pthread_getspecific(s_LastNewPtr_key);
#endif
    }
    static void InitNewPtr(void) {
#ifndef NCBI_TLS_VAR
        pthread_key_create(&s_LastNewPtr_key, 0);
        SetNewPtr(0);
#endif
    }

    CObjectWithTLS() {
        if ( IsNewInHeap(this) ) {
            m_Counter = eCounter_heap;
        }
        else {
            m_Counter = eCounter_static;
        }
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
        if ( IsInHeap(ptr) ) {
            delete ptr;
        }
    }

    static void* operator new(size_t s) {
        alloc_count.Add(1);
        CObjectWithTLS* ptr = (CObjectWithTLS*)malloc(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr, size_t /*s*/) {
        alloc_count.Add(-1);
        RegisterDelete((CObjectWithTLS*)ptr);
        free(ptr);
    }
    
private:
    unsigned m_Counter;

private:
    CObjectWithTLS(const CObjectWithTLS&);
    void operator=(CObjectWithTLS&);
};

template<class E, size_t S, bool Zero = true>
class CArray {
public:
    static void* operator new(size_t s) {
        void* ptr = malloc(s);
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
    static void operator delete(void* ptr, size_t /*s*/) {
        free(ptr);
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
            cout << "ERROR: alloc_count: " << alloc_count.Get() << endl;
        if ( object_count.Get() != expected )
            cout << "ERROR: object_count: " << object_count.Get() << endl;
    }
    void *ptr = CObjectWithTLS::GetNewPtr();
    if ( ptr != expected_ptr )
        cout << "ERROR: new ptr: " << ptr << endl;
}


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestTlsObjectApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
};


bool CTestTlsObjectApp::Thread_Run(int /*idx*/)
{
    const size_t COUNT = 100000;
    const size_t OBJECT_SIZE = sizeof(CObjectWithNew);
    {
        // prealloc
        {
            size_t size = (OBJECT_SIZE+16)*COUNT;
            void* p = malloc(size);
            memset(p, 1, size);
            free(p);
        }
        {
            void** p = new void*[COUNT*2];
            for ( size_t i = 0; i < COUNT*2; ++i ) {
                p[i] = malloc(OBJECT_SIZE);
            }
            for ( size_t i = 0; i < COUNT*2; ++i ) {
                free(p[i]);
            }
            delete[] p;
        }
    }
    CStopWatch sw;
    for ( int t = 0; t < 1; ++t ) {
        void** ptr = new void*[COUNT];
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = malloc(OBJECT_SIZE);
        }
        double t1 = sw.Elapsed();
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            free(ptr[i]);
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
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithNew;
        }
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
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithTLS;
        }
        double t1 = sw.Elapsed();
        check_cnts(COUNT, 0, ptr[COUNT-1]);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(ptr[i]->IsInHeap());
        }
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
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithNew();
        }
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
        for ( size_t i = 0; i < COUNT; ++i ) {
            ptr[i] = new CObjectWithTLS();
        }
        double t1 = sw.Elapsed();
        check_cnts(COUNT, 0, ptr[COUNT-1]);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(ptr[i]->IsInHeap());
        }
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
    return true;
}

bool CTestTlsObjectApp::TestApp_Init(void)
{
    //s_NumThreads = 4;
    NcbiCout << "Testing TLS variant of CObject counter init." << NcbiEndl;
    CObjectWithTLS::InitNewPtr();
    return true;
}


bool CTestTlsObjectApp::TestApp_Exit(void)
{
    check_cnts(~0u);
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
