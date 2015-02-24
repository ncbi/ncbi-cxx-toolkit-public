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
#include <vector>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

#ifdef NCBI_COMPILER_GCC
# define inline __inline__ __attribute__((always_inline))
#endif

static CAtomicCounter alloc_count;
static CAtomicCounter object_count;
static CAtomicCounter::TValue total_steps;
static CAtomicCounter current_step;

static CAtomicCounter operation_count;
static const size_t kMaxOperationCount = 50000000;
struct SOperation {
    int type;
    int size;
    void* ptr;
};
static SOperation operations[kMaxOperationCount];

inline void add_operation(int type, int size, void* ptr)
{
  /*
    size_t index = operation_count.Add(1)-1;
    _ASSERT(index < kMaxOperationCount);
    operations[index].type = type;
    operations[index].size = size;
    operations[index].ptr = ptr;
  */
}

void check_operations()
{
    _ASSERT(current_step.Get() == total_steps);
    typedef map<void*, size_t> TAllocated;
    TAllocated allocated;
    size_t count = operation_count.Get();
    for ( size_t i = 0; i < count; ++i ) {
        const SOperation& op = operations[i];
        _ASSERT(op.type != 0);
        void* ptr = op.ptr;
        if ( op.type > 0 ) {
            void* end = (char*)ptr + op.size;
            TAllocated::iterator it1 = allocated.lower_bound(ptr);
            TAllocated::iterator it2 = allocated.lower_bound(end);
            _ASSERT(it1 == it2);
            allocated[ptr] = op.size;
        }
        else {
            _ASSERT(allocated.find(ptr) != allocated.end());
            allocated.erase(ptr);
        }
    }
    _ASSERT(allocated.empty());
}

inline void add_alloc(int d)
{
    alloc_count.Add(d);
}

inline void add_step()
{
    CAtomicCounter::TValue c = current_step.Add(1);
    if ( c % 1000000 == 0 ) {
        LOG_POST("step "<<c<<" of "<<total_steps);
    }
}

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

static DECLARE_TLS_VAR(const char*, s_CurrentStep);
static DECLARE_TLS_VAR(bool, s_CurrentInHeap);

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
        add_alloc(1);
        CObjectWithNew* ptr = (CObjectWithNew*)::operator new(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr) {
        add_alloc(-1);
        RegisterDelete((CObjectWithNew*)ptr);
        ::operator delete(ptr);
    }
    
private:
    unsigned m_Counter;

private:
    CObjectWithNew(const CObjectWithNew&);
    void operator=(CObjectWithNew&);
};


class CObjectWithRef : public CObject
{
public:
    CObjectWithRef(int = 0) {
        object_count.Add(1);
    }
    virtual ~CObjectWithRef() {
        object_count.Add(-1);
    }
};


class CObjectWithRef2 : public CObjectWithRef
{
public:
    CObjectWithRef2(int = 0) {
        if ( s_CurrentInHeap ) {
            if ( rand() % 10 == 0 ) {
                throw runtime_error("CObjectWithRef2");
            }
        }
    }
    virtual ~CObjectWithRef2() {
    }
private:
    CObjectWithRef m_SubObject;
};


static const CAtomicCounter::TValue kLastNewTypeMultiple = 1;
static DECLARE_TLS_VAR(void*, s_LastNewPtr);
static DECLARE_TLS_VAR(CAtomicCounter::TValue, s_LastNewType);
typedef pair<void*, CAtomicCounter::TValue> TLastNewPtrMultipleInfo;
typedef vector<TLastNewPtrMultipleInfo> TLastNewPtrMultiple;
#ifdef NCBI_NO_THREADS
static TLastNewPtrMultiple s_LastNewPtrMultiple;
#else
static TTlsKey s_LastNewPtrMultiple_key;
#endif

#ifdef NCBI_POSIX_THREADS
static
void sx_EraseLastNewPtrMultiple(void* ptr)
{
    delete (TLastNewPtrMultiple*)ptr;
}
#endif

static
TLastNewPtrMultiple& sx_GetLastNewPtrMultiple(void)
{
#ifdef NCBI_NO_THREADS
    return s_LastNewPtrMultiple;
#else
    if ( !s_LastNewPtrMultiple_key ) {
        DEFINE_STATIC_FAST_MUTEX(s_InitMutex);
        NCBI_NS_NCBI::CFastMutexGuard guard(s_InitMutex);
        if ( !s_LastNewPtrMultiple_key ) {
            TTlsKey key = 0;
            do {
#  ifdef NCBI_WIN32_THREADS
                _VERIFY((key = TlsAlloc()) != DWORD(-1));
#  else
                _VERIFY(pthread_key_create(&key, sx_EraseLastNewPtrMultiple)==0);
#  endif
            } while ( !key );
#  ifndef NCBI_WIN32_THREADS
            pthread_setspecific(key, 0);
#  endif
            s_LastNewPtrMultiple_key = key;
        }
    }
    TLastNewPtrMultiple* set;
#  ifdef NCBI_WIN32_THREADS
    set = (TLastNewPtrMultiple*)TlsGetValue(s_LastNewPtrMultiple_key);
#  else
    set = (TLastNewPtrMultiple*)pthread_getspecific(s_LastNewPtrMultiple_key);
#  endif
    if ( !set ) {
        set = new TLastNewPtrMultiple();
#  ifdef NCBI_WIN32_THREADS
        TlsSetValue(s_LastNewPtrMultiple_key, set);
#  else
        pthread_setspecific(s_LastNewPtrMultiple_key, set);
#  endif
    }
    return *set;
#endif
}


static
void sx_PushLastNewPtrMultiple(void* ptr, CAtomicCounter::TValue type)
{
    _ASSERT(s_LastNewPtr);
    TLastNewPtrMultiple& set = sx_GetLastNewPtrMultiple();
    if ( s_LastNewType != kLastNewTypeMultiple ) {
        set.push_back(TLastNewPtrMultipleInfo(s_LastNewPtr, s_LastNewType));
        s_LastNewType = kLastNewTypeMultiple;
    }
    set.push_back(TLastNewPtrMultipleInfo(ptr, type));
}

static
CAtomicCounter::TValue sx_PopLastNewPtrMultiple(void* ptr)
{
    TLastNewPtrMultiple& set = sx_GetLastNewPtrMultiple();
    NON_CONST_ITERATE ( TLastNewPtrMultiple, it, set ) {
        if ( it->first == ptr ) {
            CAtomicCounter::TValue last_type = it->second;
            swap(*it, set.back());
            set.pop_back();
            if ( set.empty() ) {
                s_LastNewPtr = 0;
            }
            else {
                s_LastNewPtr = set.front().first;
            }
            return last_type;
        }
    }
    return 0;
}

static inline
void sx_PushLastNewPtr(void* ptr, CAtomicCounter::TValue type)
{
    //cerr << "sx_PushLastNewPtr(" << ptr << ", "<<s_LastNewPtr<<")"<<endl;
    if ( s_LastNewPtr ) {
        // multiple
        sx_PushLastNewPtrMultiple(ptr, type);
    }
    else {
        s_LastNewPtr = ptr;
        s_LastNewType = type;
    }
}

static inline
CAtomicCounter::TValue sx_PopLastNewPtr(void* ptr)
{
    //cerr << "sx_PopLastNewPtr(" << ptr << ", "<<s_LastNewPtr<<")"<<endl;
    void* last_ptr = s_LastNewPtr;
    if ( !last_ptr ) {
        return 0;
    }
    CAtomicCounter::TValue last_type = s_LastNewType;
    if ( last_type == kLastNewTypeMultiple ) {
        // multiple ptrs
        return sx_PopLastNewPtrMultiple(ptr);
    }
    else {
        if ( s_LastNewPtr != ptr ) {
            return 0;
        }
        s_LastNewPtr = 0;
        return last_type;
    }
}

inline
bool sx_HaveLastNewPtr(void)
{
    return s_LastNewPtr != 0;
}

class CObjectWithTLS
{
    enum ECounter {
        eCounter_heap     = 0x1,
        eCounter_static   = 0x0
    };
    static bool IsNewInHeap(CObjectWithTLS* ptr) {
        return sx_PopLastNewPtr(ptr);
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
        sx_PushLastNewPtr(ptr, 2);
    }
    static void RegisterDelete(CObjectWithTLS* ptr) {
        sx_PopLastNewPtr(ptr);
    }
public:

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
        add_alloc(1);
        CObjectWithTLS* ptr = (CObjectWithTLS*)::operator new(s);
        RegisterNew(ptr);
        return ptr;
    }
    static void operator delete(void* ptr) {
        add_alloc(-1);
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
    CObjectWithTLS3(int = 0)
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
                size_t expected_static = 0)
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
    _VERIFY(!sx_HaveLastNewPtr());
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


int RecursiveNewRef(int level);
int RecursiveNewTLS(int level);


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


static const size_t COUNT = 2000;

void CTestTlsObjectApp::RunTest(void)
{
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
                add_alloc(1);
                add_step();
                p[i] = ::operator new(OBJECT_SIZE);
            }
            for ( size_t i = 0; i < COUNT2; ++i ) {
                add_alloc(-1);
                add_step();
                ::operator delete(p[i]);
            }
            delete[] p;
        }
        {
            const size_t COUNT2 = COUNT*2;
            int** p = new int*[COUNT2];
            for ( size_t i = 0; i < COUNT2; ++i ) {
                add_alloc(1);
                add_step();
                p[i] = new int(int(i));
            }
            for ( size_t i = 0; i < COUNT2; ++i ) {
                add_alloc(-1);
                add_step();
                delete p[i];
            }
            delete[] p;
        }
    }
    //return;
    CStopWatch sw;
    check_cnts();
    for ( int t = 0; t < 1; ++t ) {
        void** ptr = new void*[COUNT];
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_alloc(1);
            add_step();
            ptr[i] = ::operator new(OBJECT_SIZE);
        }
        double t1 = sw.Elapsed();
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_alloc(-1);
            add_step();
            ::operator delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("plain malloc", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        sw.Start();
        int* ptr = new int;
        sx_PushLastNewPtr(ptr, 2);
        double t1 = sw.Elapsed();
        sw.Start();
        _VERIFY(sx_PopLastNewPtr(ptr));
        delete ptr;
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
            add_step();
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
            add_step();
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
            add_step();
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
            _ASSERT(!sx_HaveLastNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->IsInHeap());
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            CObjectWithTLS::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithTLS", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        CRef<CObjectWithRef>* ptr = new CRef<CObjectWithRef>[COUNT];
        sw.Start();
        s_CurrentStep = "new CObjectWithRef";
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            try {
                switch ( rand()%2 ) {
                case 0: ptr[i] = new CObjectWithRef; break;
                case 1: ptr[i] = new CObjectWithRef2; break;
                }
            }
            catch ( exception& ) {
                ptr[i] = 0;
            }
            _ASSERT(!sx_HaveLastNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->CanBeDeleted());
        }
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            ptr[i].Reset();
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithRef", "create", t1, "delete", t2, COUNT);
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
            add_step();
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
            add_step();
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
            add_step();
            try {
                switch ( rand()%4 ) {
                case 0: ptr[i] = new CObjectWithTLS(); break;
                case 1: ptr[i] = new CObjectWithTLS2(); break;
                case 2: ptr[i] = new CObjectWithTLS3(); break;
                case 3: ptr[i] = new CObjectWithTLS3(RecursiveNewTLS(rand()%4)); break;
                }
            }
            catch ( exception& ) {
                ptr[i] = 0;
            }
            _ASSERT(!sx_HaveLastNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->IsInHeap());
        }
        s_CurrentInHeap = false;
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            CObjectWithTLS::Delete(ptr[i]);
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithTLS()", "create", t1, "delete", t2, COUNT);
        delete[] ptr;
    }
    check_cnts();
    {
        CRef<CObjectWithRef>* ptr = new CRef<CObjectWithRef>[COUNT];
        sw.Start();
        s_CurrentStep = "new CObjectWithRef()";
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            try {
                size_t j = rand()%COUNT;
                switch ( rand()%4 ) {
                case 0: ptr[j] = new CObjectWithRef(); break;
                case 1: ptr[j] = new CObjectWithRef(RecursiveNewRef(rand()%4)); break;
                case 2: ptr[j] = new CObjectWithRef2(); break;
                case 3: ptr[j] = new CObjectWithRef2(RecursiveNewRef(rand()%4)); break;
                }
            }
            catch ( exception& ) {
                ptr[i] = 0;
            }
            _ASSERT(!sx_HaveLastNewPtr());
            _ASSERT(!ptr[i] || ptr[i]->CanBeDeleted());
        }
        double t1 = sw.Elapsed();
        check_cnts(COUNT);
        sw.Start();
        for ( size_t i = 0; i < COUNT; ++i ) {
            add_step();
            ptr[i] = 0;
        }
        double t2 = sw.Elapsed();
        message("new CObjectWithRef()", "create", t1, "delete", t2, COUNT);
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
    {
        sw.Start();
        s_CurrentStep = "CObjectWithRef[]";
        CArray<CObjectWithRef, COUNT, false>* arr =
            new CArray<CObjectWithRef, COUNT, false>;
        double t1 = sw.Elapsed();
        check_cnts(COUNT, COUNT);
        for ( size_t i = 0; i < COUNT; ++i ) {
            _ASSERT(!arr->m_Array[i].CanBeDeleted());
        }
        sw.Start();
        delete arr;
        double t2 = sw.Elapsed();
        message("static CObjectWithRef", "create", t1, "delete", t2, COUNT);
    }
    check_cnts();
}

bool CTestTlsObjectApp::TestApp_Init(void)
{
    static string sss = "xxx";
    s_NumThreads = 20;
    total_steps = s_NumThreads;
    total_steps *= 3; // iterations in thread
    total_steps *= 9; // passes in iteration
    total_steps *= COUNT; // objects in pass
    total_steps *= 2; // alloc/free
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

int RecursiveNewRef(int level)
{
    CRef<CObjectWithRef> ref;
    if ( !level ) {
        ref = new CObjectWithRef();
    }
    else {
        ref = new CObjectWithRef(level-1);
    }
    return 1;
}

int RecursiveNewTLS(int level)
{
    CObjectWithTLS* ptr;
    if ( !level ) {
        ptr = new CObjectWithTLS();
    }
    else {
        ptr = new CObjectWithTLS3(level-1);
    }
    CObjectWithTLS::Delete(ptr);
    return 1;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Execute main application function
    return CTestTlsObjectApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
