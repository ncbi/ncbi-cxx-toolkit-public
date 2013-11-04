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
 * Author: 
 *   Eugene Vasilchenko
 *
 * File Description:
 *   Standard CObject and CRef classes for reference counter based GC
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbimempool.hpp>

//#define USE_SINGLE_ALLOC
//#define USE_DEBUG_NEW

// There was a long and bootless discussion:
// is it possible to determine whether the object has been created
// on the stack or on the heap.
// Correct answer is "it is impossible"
// Still, we try to... (we know it is not 100% bulletproof)
//
// Attempts include:
//
// 1. operator new puts a pointer to newly allocated memory in the list.
//    Object constructor scans the list, if it finds itself there -
//    yes, it has been created on the heap. (If it does not find itself
//    there, it still may have been created on the heap).
//    This method requires thread synchronization.
//
// 2. operator new puts a special mask (eMagicCounterNew two times) in the
//    newly allocated memory. Object constructor looks for this mask,
//    if it finds it there - yes, it has been created on the heap.
//
// 3. operator new puts a special mask (single eMagicCounterNew) in the
//    newly allocated memory. Object constructor looks for this mask,
//    if it finds it there, it also compares addresses of a variable
//    on the stack and itself (also using STACK_THRESHOLD). If these two
//    are "far enough from each other" - yes, the object is on the heap.
//
// From these three methods, the first one seems to be most reliable,
// but also most slow.
// Method #2 is hopefully reliable enough
// Method #3 is unreliable at all (we saw this)
//


#define USE_HEAPOBJ_LIST  0
#define USE_TLS_PTR 1
#if USE_TLS_PTR
#  include <corelib/ncbithr.hpp>
#  include <vector>
#elif USE_HEAPOBJ_LIST
#  include <corelib/ncbi_safe_static.hpp>
#  include <list>
#  include <algorithm>
#else
#  define USE_COMPLEX_MASK  1
#  if USE_COMPLEX_MASK
#    define STACK_THRESHOLD (64)
#  else
#    define STACK_THRESHOLD (16*1024)
#  endif
#endif


#include <corelib/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Corelib_Object


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CObject::
//


#if !USE_TLS_PTR
DEFINE_STATIC_FAST_MUTEX(sm_ObjectMutex);
#endif

#if defined(NCBI_COUNTER_NEED_MUTEX)
// CAtomicCounter doesn't normally have a .cpp file of its own, so this
// goes here instead.
DEFINE_STATIC_FAST_MUTEX(sm_AtomicCounterMutex);

CAtomicCounter::TValue CAtomicCounter::Add(int delta) THROWS_NONE
{
    CFastMutexGuard LOCK(sm_AtomicCounterMutex);
    return m_Value += delta;
}

#endif

#if USE_TLS_PTR

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

#endif

#if USE_HEAPOBJ_LIST
static CSafeStatic< list<const void*> > s_heap_obj;
#endif

#if USE_COMPLEX_MASK
static inline CAtomicCounter* GetSecondCounter(CObject* ptr)
{
  return reinterpret_cast<CAtomicCounter*> (ptr + 1);
}
#endif


#ifdef USE_SINGLE_ALLOC
#define SINGLE_ALLOC_THRESHOLD (   2*1024)
#define SINGLE_ALLOC_POOL_SIZE (1024*1024)

DEFINE_STATIC_FAST_MUTEX(sx_SingleAllocMutex);

static char*  single_alloc_pool = 0;
static size_t single_alloc_pool_size = 0;

void* single_alloc(size_t size)
{
    if ( size > SINGLE_ALLOC_THRESHOLD ) {
        return ::operator new(size);
    }
    sx_SingleAllocMutex.Lock();
    size_t pool_size = single_alloc_pool_size;
    char* pool;
    if ( size > pool_size ) {
        pool_size = SINGLE_ALLOC_POOL_SIZE;
        pool = (char*) malloc(pool_size);
        if ( !pool ) {
            sx_SingleAllocMutex.Unlock();
            throw bad_alloc();
        }
        single_alloc_pool = pool;
        single_alloc_pool_size = pool_size;
    }
    else {
        pool = single_alloc_pool;
    }
    single_alloc_pool      = pool      + size;
    single_alloc_pool_size = pool_size - size;
    sx_SingleAllocMutex.Unlock();
    return pool;
}
#endif


static CObject::EAllocFillMode sm_AllocFillMode;
static bool sm_AllocFillMode_IsSet;
#if defined(_DEBUG) && !defined(NCBI_COMPILER_MSVC)
# define ALLOC_FILL_MODE_INIT        CObject::eAllocFillPattern
# define ALLOC_FILL_MODE_DEFAULT     CObject::eAllocFillPattern
#else
# define ALLOC_FILL_MODE_INIT        CObject::eAllocFillZero
# define ALLOC_FILL_MODE_DEFAULT     CObject::eAllocFillNone
#endif
#if defined(NCBI_COMPILER_MSVC)
# define ALLOC_FILL_BYTE_PATTERN 0xcd
#else
# define ALLOC_FILL_BYTE_PATTERN 0xaa
#endif

static CObject::EAllocFillMode sx_InitFillNewMemoryMode(void)
{
    CObject::EAllocFillMode mode = ALLOC_FILL_MODE_INIT;
    const char* env = ::getenv("NCBI_MEMORY_FILL");
    if ( env && *env ) {
        bool is_set = true;
        if ( NStr::CompareNocase(env, "NONE") == 0 )
            mode = CObject::eAllocFillNone;
        else if ( NStr::CompareNocase(env, "ZERO") == 0 )
            mode = CObject::eAllocFillZero;
        else if ( NStr::CompareNocase(env, "PATTERN") == 0 )
            mode = CObject::eAllocFillPattern;
        else
            is_set = false;
        sm_AllocFillMode_IsSet = is_set;
    }
    sm_AllocFillMode = mode;
    return mode;
}


CObject::EAllocFillMode CObject::GetAllocFillMode(void)
{
    return sm_AllocFillMode;
}


void CObject::SetAllocFillMode(CObject::EAllocFillMode mode)
{
    sm_AllocFillMode = mode;
}


void CObject::SetAllocFillMode(const string& value)
{
    EAllocFillMode mode = sm_AllocFillMode;
    if ( NStr::CompareNocase(value, "NONE") == 0 )
        mode = eAllocFillNone;
    else if ( NStr::CompareNocase(value, "ZERO") == 0 )
        mode = eAllocFillZero;
    else if ( NStr::CompareNocase(value, "PATTERN") == 0 )
        mode = eAllocFillPattern;
    else if ( !sm_AllocFillMode_IsSet )
        mode = ALLOC_FILL_MODE_DEFAULT;
    sm_AllocFillMode = mode;
}


static inline void sx_FillNewMemory(void* ptr, size_t size)
{
    CObject::EAllocFillMode mode = sm_AllocFillMode;
    if ( !mode ) {
        mode = sx_InitFillNewMemoryMode();
    }
    if ( mode == CObject::eAllocFillZero ) {
        memset(ptr, 0, size);
    }
    else if ( mode == CObject::eAllocFillPattern ) {
        memset(ptr, ALLOC_FILL_BYTE_PATTERN, size);
    }
}

// CObject local new operator to mark allocation in heap
void* CObject::operator new(size_t size)
{
    _ASSERT(size >= sizeof(CObject));
    size = max(size, sizeof(CObject) + sizeof(TCounter));

#ifdef USE_SINGLE_ALLOC
    void* ptr = single_alloc(size);
    sx_FillNewMemory(ptr, size);
    //static_cast<CObject*>(ptr)->m_Counter.Set(0);
    return ptr;
#else
    void* ptr = ::operator new(size);

#if USE_TLS_PTR
    // just remember pointer in TLS
    sx_PushLastNewPtr(ptr, eMagicCounterNew);
#else// !USE_TLS_PTR
#if USE_HEAPOBJ_LIST
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        s_heap_obj->push_front(ptr);
    }}
#else// !USE_HEAPOBJ_LIST
    sx_FillNewMemory(ptr, size);
#  if USE_COMPLEX_MASK
    GetSecondCounter(static_cast<CObject*>(ptr))->Set(eMagicCounterNew);
#  endif// USE_COMPLEX_MASK
#endif// USE_HEAPOBJ_LIST
    static_cast<CObject*>(ptr)->m_Counter.Set(eMagicCounterNew);
#endif// USE_TLS_PTR
    return ptr;
#endif
}


void CObject::operator delete(void* ptr)
{
    // Can be called either from regular destruction (with counter initialized)
    // or before CObject constructor is called (counter is not set yet).
#if USE_TLS_PTR
# ifdef _DEBUG
    TCount magic = sx_PopLastNewPtr(ptr);
    if ( !magic ) { // counter already initialized
        magic = static_cast<CObject*>(ptr)->m_Counter.Get();
    }
# else// !_DEBUG
    // Just remove saved operator new info.
    sx_PopLastNewPtr(ptr);
# endif// _DEBUG
#else// !USE_TLS_PTR
# ifdef _DEBUG
    TCount magic = static_cast<CObject*>(ptr)->m_Counter.Get();
# endif// _DEBUG
#endif// USE_TLS_PTR

    // magic can be equal to:
    // 1. eMagicCounterDeleted when memory is freed after CObject destructor.
    // 2. eMagicCounterNew when memory is freed before CObject constructor.
    _ASSERT(magic == eMagicCounterDeleted  || magic == eMagicCounterNew);
    ::operator delete(ptr);
}


// CObject local new operator to mark allocation in other memory chunk
void* CObject::operator new(size_t size, void* place)
{
    _ASSERT(size >= sizeof(CObject));
    sx_FillNewMemory(place, size);
    //static_cast<CObject*>(ptr)->m_Counter.Set(0);
    return place;
}

// complement placement delete operator -> do nothing
void CObject::operator delete(void* _DEBUG_ARG(ptr), void* /*place*/)
{
    // Can be called either from regular destruction (with counter initialized)
    // or before CObject constructor is called (counter is not set yet).
#if !USE_TLS_PTR
# ifdef _DEBUG
    CObject* objectPtr = static_cast<CObject*>(ptr);
    TCount magic = objectPtr->m_Counter.Get();
    // magic can be equal to:
    // 1. eMagicCounterDeleted when memory is freed after CObject destructor.
    // 2. 0 when memory is freed before CObject constructor.
    _ASSERT(magic == eMagicCounterDeleted  || magic == 0);
# endif// _DEBUG
#endif// USE_TLS_PTR
}


// CObject new operator from memory pool
void* CObject::operator new(size_t size, CObjectMemoryPool* memory_pool)
{
    _ASSERT(size >= sizeof(CObject));
    if ( !memory_pool ) {
        return operator new(size);
    }
    void* ptr = memory_pool->Allocate(size);
    if ( !ptr ) {
        return operator new(size);
    }
#if USE_TLS_PTR
    // just remember pointer in TLS
    sx_PushLastNewPtr(ptr, eMagicCounterPoolNew);
#else// !USE_TLS_PTR
#  if USE_COMPLEX_MASK
    GetSecondCounter(static_cast<CObject*>(ptr))->Set(eMagicCounterPoolNew);
#  endif// USE_COMPLEX_MASK
    static_cast<CObject*>(ptr)->m_Counter.Set(eMagicCounterPoolNew);
#endif// USE_TLS_PTR
    return ptr;
}

// complement pool delete operator
void CObject::operator delete(void* ptr, CObjectMemoryPool* memory_pool)
{
    // Can be called either from regular destruction (with counter initialized)
    // or before CObject constructor is called (counter is not set yet).
#if USE_TLS_PTR
# ifdef _DEBUG
    TCount magic = sx_PopLastNewPtr(ptr);
    if ( !magic ) { // counter already initialized
        magic = static_cast<CObject*>(ptr)->m_Counter.Get();
    }
# else// !_DEBUG
    // Just remove saved operator new info.
    sx_PopLastNewPtr(ptr);
# endif// _DEBUG
#else// !USE_TLS_PTR
# ifdef _DEBUG
    TCount magic = static_cast<CObject*>(ptr)->m_Counter.Get();
# endif//_DEBUG
#endif// USE_TLS_PTR

    // magic can be equal to:
    // 1. eMagicCounterPoolDeleted when freed after CObject destructor.
    // 2. eMagicCounterPoolNew when freed before CObject constructor.
    _ASSERT(magic == eMagicCounterPoolDeleted ||
            magic == eMagicCounterPoolNew);
    memory_pool->Deallocate(ptr);
}


// CObject local new operator to mark allocation in heap
void* CObject::operator new[](size_t size)
{
# ifdef NCBI_OS_MSWIN
    void* ptr = ::operator new(size);
# else
    void* ptr = ::operator new[](size);
# endif
    sx_FillNewMemory(ptr, size);
    return ptr;
}


void CObject::operator delete[](void* ptr)
{
#ifdef NCBI_OS_MSWIN
    ::operator delete(ptr);
#else
    ::operator delete[](ptr);
#endif
}


#ifdef _DEBUG
# define ObjFatal Fatal
#else
# define ObjFatal Critical
#endif

// initialization in debug mode
inline
void CObject::InitCounter(void)
{
#if USE_TLS_PTR
    if ( CAtomicCounter::TValue type = sx_PopLastNewPtr(this) ) {
        switch ( type ) {
        case eMagicCounterNew:
            // allocated in heap
            m_Counter.Set(eInitCounterInHeap);
            break;
        case eMagicCounterPoolNew:
            // allocated in memory pool
            m_Counter.Set(eInitCounterInPool);
            break;
        default:
            ERR_POST_X(1, ObjFatal << "CObject::InitCounter: "
                       "Bad s_LastNewType="<<type<<
                       " at "<<StackTrace);
            // something is broken in TLS data
            // mark as not in heap
            m_Counter.Set(eInitCounterNotInHeap);
            break;
        }
    }
    else {
        // surely not in heap
        m_Counter.Set(eInitCounterNotInHeap);
    }
#else
    // This code can't use Get(), which may block waiting for an
    // update that will never happen.
    // ATTENTION:  this code can cause UMR (Uninit Mem Read) -- it's okay here!
    TCount main_counter = m_Counter.m_Value;
    if ( main_counter != eMagicCounterNew &&
         main_counter != eMagicCounterPoolNew ) {
        // takes care of statically allocated case
        m_Counter.Set(eInitCounterNotInHeap);
    }
    else {
        bool inStack = false;
#if USE_HEAPOBJ_LIST
        const void* ptr = dynamic_cast<const void*>(this);
        {{
            CFastMutexGuard LOCK(sm_ObjectMutex);
            list<const void*>::iterator i =
                find( s_heap_obj->begin(), s_heap_obj->end(), ptr);
            inStack = (i == s_heap_obj->end());
            if (!inStack) {
                s_heap_obj->erase(i);
            }
        }}
#else // USE_HEAPOBJ_LIST
#  if USE_COMPLEX_MASK
        inStack = GetSecondCounter(this)->m_Value != main_counter;
#  endif // USE_COMPLEX_MASK
        // m_Counter == main_counter -> possibly in heap
        if ( !inStack ) {
            char stackObject;
            const char* stackObjectPtr = &stackObject;
            const char* objectPtr = reinterpret_cast<const char*>(this);
#  if defined STACK_GROWS_UP
            inStack =
                (objectPtr < stackObjectPtr) && 
                (objectPtr > stackObjectPtr - STACK_THRESHOLD);
#  elif defined STACK_GROWS_DOWN
            inStack =
                (objectPtr > stackObjectPtr) &&
                (objectPtr < stackObjectPtr + STACK_THRESHOLD);
#  else
            inStack =
                (objectPtr < stackObjectPtr + STACK_THRESHOLD) && 
                (objectPtr > stackObjectPtr - STACK_THRESHOLD);
#  endif
        }
#endif // USE_HEAPOBJ_LIST
        
        if ( inStack ) {
            // surely not in heap
            m_Counter.Set(eInitCounterInStack);
        }
        else if ( main_counter == eMagicCounterNew ) {
            // allocated in heap
            m_Counter.Set(eInitCounterInHeap);
        }
        else {
            // allocated in memory pool
            m_Counter.Set(eInitCounterInPool);
        }
    }
#endif
}


CObject::CObject(void)
{
    InitCounter();
}


CObject::CObject(const CObject& /*src*/)
{
    InitCounter();
}


CObject::~CObject(void)
{
    TCount count = m_Counter.Get();
    if ( ObjectStateUnreferenced(count) ) {
        // reference counter is zero -> ok
    }
    else if ( ObjectStateValid(count) ) {
        _ASSERT(ObjectStateReferenced(count));
        // referenced object
        ERR_POST_X(1, ObjFatal << "CObject::~CObject: "
                      "Referenced CObject may not be deleted"<<StackTrace);
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        // deleted object
        ERR_POST_X(2, ObjFatal << "CObject::~CObject: "
                      "CObject is already deleted"<<StackTrace);
    }
    else {
        // bad object
        ERR_POST_X(3, ObjFatal << "CObject::~CObject: "
                      "CObject is corrupted"<<StackTrace);
    }
    // mark object as deleted
    TCount final_magic;
    if ( ObjectStateIsAllocatedInPool(count) ) {
        final_magic = eMagicCounterPoolDeleted;
    }
    else {
        final_magic = eMagicCounterDeleted;
    }
    m_Counter.Set(final_magic);
}


void CObject::CheckReferenceOverflow(TCount count) const
{
    if ( ObjectStateValid(count) ) {
        // counter overflow
        NCBI_THROW(CObjectException, eRefOverflow,
                   "CObject::CheckReferenceOverflow: "
                   "CObject's reference counter overflow");
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eDeleted,
                   "CObject::CheckReferenceOverflow: "
                   "CObject is already deleted");
    }
    else {
        // bad object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::CheckReferenceOverflow: "
                   "CObject is corrupted");
    }
}


void CObject::DeleteThis(void)
{
    TCount count = m_Counter.Get();
    // Counter could be changed by some other thread,
    // we should take care of that.
    if ( (count & eInitCounterInHeap) == TCount(eInitCounterInHeap) ) {
        delete this;
    }
    else {
        _ASSERT((count & eInitCounterInPool) == TCount(eInitCounterInPool));
        CObjectMemoryPool::Delete(this);
    }
}


void CObject::RemoveLastReference(TCount count) const
{
    if ( ObjectStateCanBeDeleted(count) ) {
        // last reference to heap object -> delete it
        if ( ObjectStateUnreferenced(count) ) {
            const_cast<CObject*>(this)->DeleteThis();
            return;
        }
    }
    else {
        if ( ObjectStateValid(count) ) {
            // last reference to non heap object -> do nothing
            return;
        }
    }

    // Error here
    // restore original value
    count = m_Counter.Add(eCounterStep);
    // bad object
    if ( ObjectStateValid(count) ) {
        ERR_POST_X(4, ObjFatal << "CObject::RemoveLastReference: "
                      "CObject was referenced again"<<StackTrace);
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        ERR_POST_X(5, ObjFatal << "CObject::RemoveLastReference: "
                      "CObject is already deleted"<<StackTrace);
    }
    else {
        ERR_POST_X(6, ObjFatal << "CObject::RemoveLastReference: "
                      "CObject is corrupted"<<StackTrace);
    }
}


void CObject::ReleaseReference(void) const
{
    TCount count = m_Counter.Add(-eCounterStep);
    if ( ObjectStateValid(count) ) {
        return;
    }
    m_Counter.Add(eCounterStep); // undo

    // error
    if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::ReleaseReference: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::ReleaseReference: "
                   "CObject is corrupted");
    }
}


void CObject::DoNotDeleteThisObject(void)
{
    TCount count;
#if USE_TLS_PTR
    count = m_Counter.Get();
    if ( ObjectStateValid(count) ) {
        if ( ObjectStateCanBeDeleted(count) ) {
            NCBI_THROW(CObjectException, eHeapState,
                       "CObject::DoNotDeleteThisObject: "
                       "CObject is allocated in heap");
        }
        // no-op
        return;
    }
#else
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        count = m_Counter.Get();
        if ( ObjectStateValid(count) ) {
            // valid and unreferenced
            // reset all 'in heap' flags -> make it non-heap without signature
            m_Counter.Add(-int(count & eStateBitsInHeapMask));
            return;
        }
    }}
#endif

    if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoNotDeleteThisObject: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoNotDeleteThisObject: "
                   "CObject is corrupted");
    }
}


void CObject::DoDeleteThisObject(void)
{
#ifndef USE_SINGLE_ALLOC
    TCount count;
#if USE_TLS_PTR
    count = m_Counter.Get();
    if ( ObjectStateValid(count) ) {
        if ( !ObjectStateCanBeDeleted(count) ) {
            NCBI_THROW(CObjectException, eHeapState,
                       "CObject::DoDeleteThisObject: "
                       "CObject is not allocated in heap");
        }
        // no-op
        return;
    }
#else
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        count = m_Counter.Get();
        // DoDeleteThisObject is not allowed for stack objects
        enum {
            eCheckBits = eStateBitsValid | eStateBitsHeapSignature
        };

        if ( (count & eCheckBits) == TCount(eCheckBits) ) {
            if ( !(count & eStateBitsInHeap) ) {
                // set 'in heap' flag
                m_Counter.Add(eStateBitsInHeap);
            }
            return;
        }
    }}
#endif
    if ( ObjectStateValid(count) ) {
        ERR_POST_X(7, ObjFatal << "CObject::DoDeleteThisObject: "
                      "object was created without heap signature"<<StackTrace);
    }
    else if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoDeleteThisObject: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoDeleteThisObject: "
                   "CObject is corrupted");
    }
#endif
}


NCBI_PARAM_DECL(bool, NCBI, ABORT_ON_COBJECT_THROW);
NCBI_PARAM_DEF_EX(bool, NCBI, ABORT_ON_COBJECT_THROW, false,
                  eParam_NoThread, NCBI_ABORT_ON_COBJECT_THROW);

void CObjectException::x_InitErrCode(CException::EErrCode err_code)
{
    CCoreException::x_InitErrCode(err_code);
    static const bool sx_abort_on_throw =
        NCBI_PARAM_TYPE(NCBI, ABORT_ON_COBJECT_THROW)::GetDefault();
    if ( sx_abort_on_throw ) {
        Abort();
    }
}

void CObject::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CObject");
    ddc.Log("address",  dynamic_cast<const CDebugDumpable*>(this), 0);
//    ddc.Log("memory starts at", dynamic_cast<const void*>(this));
//    ddc.Log("onHeap", CanBeDeleted());
}


NCBI_PARAM_DECL(bool, NCBI, ABORT_ON_NULL);
NCBI_PARAM_DEF_EX(bool, NCBI, ABORT_ON_NULL, false,
                  eParam_NoThread, NCBI_ABORT_ON_NULL);
static const bool sx_abort_on_null =
    NCBI_PARAM_TYPE(NCBI, ABORT_ON_NULL)::GetDefault();

void CObject::ThrowNullPointerException(void)
{
#ifdef _DEBUG
    ERR_POST(Error << "NULL pointer exception: " << CStackTrace());
#endif
    if ( sx_abort_on_null ) {
        Abort();
    }
    NCBI_EXCEPTION_VAR(ex,
        CCoreException, eNullPtr, "Attempt to access NULL pointer.");
    ex.SetSeverity(eDiag_Critical);
    NCBI_EXCEPTION_THROW(ex);
}


void CObject::ThrowNullPointerException(const type_info& type)
{
#ifdef _DEBUG
    ERR_POST(Error << "NULL pointer exception: " << CStackTrace());
#endif
    if ( sx_abort_on_null ) {
        Abort();
    }
    NCBI_EXCEPTION_VAR(ex,
                       CCoreException, eNullPtr,
                       string("Attempt to access NULL pointer: ")+type.name());
    ex.SetSeverity(eDiag_Critical);
    NCBI_EXCEPTION_THROW(ex);
}


#ifndef NCBI_OBJECT_LOCKER_INLINE

static const type_info* sx_MonitorType = 0;

class CLocksMonitor
{
public:
    ~CLocksMonitor(void)
    {
        DumpLocks(true);
    }

    struct SLocks {
        SLocks(void) : m_Object(0) {}
        typedef multimap<const CObjectCounterLocker*, AutoPtr<CStackTrace> > TLocks;
        typedef multimap<const CObjectCounterLocker*, AutoPtr<CStackTrace> > TUnlocks;

        void Dump(void) const
        {
            unsigned total_locks = 0;
            ITERATE ( TLocks, it, m_Locks ) {
                ++total_locks;
                LOG_POST("Locked<"<<sx_MonitorType->name()<<">"
                         "("<<it->first<<","<<m_Object<<")"
                         " @ " << *it->second);
            }
            unsigned total_unlocks = 0;
            ITERATE ( TUnlocks, it, m_Unlocks ) {
                ++total_unlocks;
                LOG_POST("Unlocked<"<<sx_MonitorType->name()<<">"
                         "("<<it->first<<","<<m_Object<<")"
                         " @ " << *it->second);
            }
            if ( total_locks ) {
                LOG_POST("Total locks for "<<m_Object<<": "<<total_locks);
            }
            if ( total_unlocks ) {
                LOG_POST("Total unlocks for "<<m_Object<<": "<<total_unlocks);
            }
        }
        int LockCount(void) const
        {
            return int(m_Locks.size() - m_Unlocks.size());
        }
        void Locked(const CObjectCounterLocker* locker, const CObject* object)
        {
            _ASSERT(LockCount() >= 0);
            if ( !m_Object ) {
                m_Object = object;
            }
            m_Locks.insert(TLocks::value_type(locker, new CStackTrace()));
        }
        bool Unlocked(const CObjectCounterLocker* locker)
        {
            _ASSERT(LockCount() > 0);
            TLocks::iterator lock = m_Locks.lower_bound(locker);
            if ( lock != m_Locks.end() ) {
                m_Locks.erase(lock);
            }
            else {
                m_Unlocks.insert(TUnlocks::value_type(locker, new CStackTrace()));
            }
            if ( LockCount() <= 0 ) {
                m_Locks.clear();
                m_Unlocks.clear();
                return true;
            }
            return false;
        }

        const CObject* m_Object;
        TLocks m_Locks;
        TUnlocks m_Unlocks;
    };

    void DumpLocks(bool clear = false)
    {
        CFastMutexGuard guard(m_Mutex);
        ITERATE ( TLocks, it, m_Locks ) {
            it->second.Dump();
        }
        if ( clear ) {
            m_Locks.clear();
        }
    }
    void Locked(const CObjectCounterLocker* locker, const CObject* object)
    {
        CFastMutexGuard guard(m_Mutex);
        m_Locks[object].Locked(locker, object);
    }
    void Unlocked(const CObjectCounterLocker* locker, const CObject* object)
    {
        CFastMutexGuard guard(m_Mutex);
        if ( m_Locks[object].Unlocked(locker) ) {
            m_Locks.erase(object);
        }
    }
private:
    typedef map<const CObject*, SLocks> TLocks;
    CFastMutex m_Mutex;
    TLocks m_Locks;
};

static CSafeStatic<CLocksMonitor> sx_LocksMonitor;

inline bool MonitoredType(const CObject* object)
{
    return sx_MonitorType && *sx_MonitorType == typeid(*object);
}

void CObjectCounterLocker::Lock(const CObject* object) const
{
    object->AddReference();
    if ( MonitoredType(object) ) {
        sx_LocksMonitor.Get().Locked(this, object);
    }
}


void CObjectCounterLocker::Relock(const CObject* object) const
{
    Lock(object);
}


void CObjectCounterLocker::Unlock(const CObject* object) const
{
    if ( MonitoredType(object) ) {
        sx_LocksMonitor.Get().Unlocked(this, object);
    }
    object->RemoveReference();
}


void CObjectCounterLocker::UnlockRelease(const CObject* object) const
{
    if ( MonitoredType(object) ) {
        LOG_POST("UnlockRelease<"<<typeid(*object).name()<<">"
                 "("<<this<<", "<<object<<")"
                 " @ " << StackTrace);
        sx_LocksMonitor.Get().Unlocked(this, object);
    }
    object->ReleaseReference();
}


void CObjectCounterLocker::TransferLock(const CObject* object,
                                        const CObjectCounterLocker& old_locker) const
{
    if ( MonitoredType(object) ) {
        sx_LocksMonitor.Get().Locked(this, object);
        sx_LocksMonitor.Get().Unlocked(&old_locker, object);
    }
}


void CObjectCounterLocker::MonitorObjectType(const type_info& type)
{
    StopMonitoring();
    sx_MonitorType = &type;
}


void CObjectCounterLocker::StopMonitoring(void)
{
    if ( sx_MonitorType ) {
        ReportLockedObjects(true);
        sx_MonitorType = 0;
    }
}


void CObjectCounterLocker::ReportLockedObjects(bool clear)
{
    sx_LocksMonitor.Get().DumpLocks(clear);
}
#else
void CObjectCounterLocker::MonitorObjectType(const type_info& )
{
}


void CObjectCounterLocker::StopMonitoring(void)
{
}


void CObjectCounterLocker::ReportLockedObjects(bool )
{
}
#endif


void CObjectCounterLocker::ReportIncompatibleType(const type_info& type)
{
#ifdef _DEBUG
    ERR_POST_X(8, Fatal <<
                  "Type " << type.name() << " must be derived from CObject" <<
                  StackTrace);
#else
    NCBI_THROW(CCoreException, eInvalidArg,
               string("Type ")+type.name()+" must be derived from CObject");
#endif
}

const char* CObjectException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eRefDelete:    return "eRefDelete";
    case eDeleted:      return "eDeleted";
    case eCorrupted:    return "eCorrupted";
    case eRefOverflow:  return "eRefOverflow";
    case eNoRef:        return "eNoRef";
    case eRefUnref:     return "eRefUnref";
    case eHeapState:    return "eHeapState";
    default:    return CException::GetErrCodeString();
    }
}


DEFINE_STATIC_FAST_MUTEX(s_WeakRefMutex);


CWeakObject::CWeakObject(void)
    : m_SelfPtrProxy( new CPtrToObjectProxy(this) )
{
}


CWeakObject::~CWeakObject(void)
{
    m_SelfPtrProxy->Clear();
}


inline
bool CWeakObject::x_AddWeakReference(CObject* obj)
{
    CObject::TCount newCount = obj->m_Counter.Add(CObject::eCounterStep);

    if ( CObject::ObjectStateReferencedOnlyOnce(newCount) ) {
        obj->m_Counter.Add(-CObject::eCounterStep);
        return false;
    }
    return true;
}

void
CWeakObject::CleanWeakRefs(void) const
{
    m_SelfPtrProxy->Clear();
    m_SelfPtrProxy.Reset(new CPtrToObjectProxy(const_cast<CWeakObject*>(this)));
}


CObjectEx::~CObjectEx(void)
{
}


CPtrToObjectProxy::CPtrToObjectProxy(CWeakObject* ptr)
    : m_Ptr(NULL), m_WeakPtr(ptr)
{
}


CPtrToObjectProxy::~CPtrToObjectProxy(void)
{
}


void CPtrToObjectProxy::Clear(void)
{
    CFastMutexGuard guard(s_WeakRefMutex);

    m_Ptr     = NULL;
    m_WeakPtr = NULL;
}


CObject* CPtrToObjectProxy::GetLockedObject(void)
{
    if ( !m_WeakPtr )
        return NULL;

    CFastMutexGuard guard(s_WeakRefMutex);

    // m_Ptr is set to NULL in CWeakObject destructor and always under mutex,
    // and always together with m_WeakPtr.
    // So if m_WeakPtr here is not NULL then it will be possible to execute
    // x_AddWeakReference() without interfering with destructor.
    if (m_WeakPtr  &&  !m_WeakPtr->x_AddWeakReference(m_Ptr)) {
        return NULL;
    }

    return m_Ptr;
}


void CPtrToObjectProxy::ReportIncompatibleType(const type_info& type)
{
#ifdef _DEBUG
    ERR_POST_X(8, Fatal <<
               "Type " << type.name() << " must be derived from CWeakObject"<<
               StackTrace);
#else
    NCBI_THROW(CCoreException, eInvalidArg, string("Type ") +
               type.name() + " must be derived from CWeakObject");
#endif
}



END_NCBI_SCOPE

#ifdef USE_DEBUG_NEW

static bool s_EnvFlag(const char* env_var_name)
{
    const char* value = getenv(env_var_name);
    return value  &&  (*value == 'Y'  ||  *value == 'y' || *value == '1');
}


struct SAllocHeader
{
    unsigned magic;
    unsigned seq_number;
    size_t   size;
    void*    ptr;
};


struct SAllocFooter
{
    void*    ptr;
    size_t   size;
    unsigned seq_number;
    unsigned magic;
};

static const size_t kAllocSizeBefore = 32;
static const size_t kAllocSizeAfter = 32;

static const char kAllocFillBeforeArray = 0xba;
static const char kAllocFillBeforeOne   = 0xbb;
static const char kAllocFillInside      = 0xdd;
static const char kAllocFillAfter       = 0xaa;
static const char kAllocFillFree        = 0xee;

static const unsigned kAllocMagicHeader = 0x8b9b0b0b;
static const unsigned kAllocMagicFooter = 0x9e8e0e0e;
static const unsigned kFreedMagicHeader = 0x8b0bdead;
static const unsigned kFreedMagicFooter = 0x9e0edead;

static std::bad_alloc bad_alloc_instance;

DEFINE_STATIC_FAST_MUTEX(s_alloc_mutex);
static NCBI_NS_NCBI::CAtomicCounter seq_number;
static const size_t kLogSize = 64 * 1024;
struct SAllocLog {
    unsigned seq_number;
    enum EType {
        eEmpty = 0,
        eInit,
        eNew,
        eNewArr,
        eDelete,
        eDeleteArr
    };
    char   type;
    char   completed;
    size_t size;
    void*  ptr;
};
static SAllocLog alloc_log[kLogSize];


static inline SAllocHeader* get_header(void* ptr)
{
    return (SAllocHeader*) ((char*) ptr-kAllocSizeBefore);
}


static inline void* get_guard_before(SAllocHeader* header)
{
    return (char*) header + sizeof(SAllocHeader);
}


static inline size_t get_guard_before_size()
{
    return kAllocSizeBefore - sizeof(SAllocHeader);
}


static inline size_t get_extra_size(size_t size)
{
    return (-size) & 7;
}


static inline size_t get_guard_after_size(size_t size)
{
    return kAllocSizeAfter - sizeof(SAllocFooter) + get_extra_size(size);
}


static inline void* get_ptr(SAllocHeader* header)
{
    return (char*) header + kAllocSizeBefore;
}


static inline void* get_guard_after(SAllocFooter* footer, size_t size)
{
    return (char*)footer-get_guard_after_size(size);
}


static inline SAllocFooter* get_footer(SAllocHeader* header, size_t size)
{
    return (SAllocFooter*)((char*)get_ptr(header)+size+get_guard_after_size(size));
}


static inline size_t get_total_size(size_t size)
{
    return size+get_extra_size(size) + (kAllocSizeBefore+kAllocSizeAfter);
}


static inline size_t get_all_guards_size(size_t size)
{
    return get_total_size(size) - (sizeof(SAllocHeader)+sizeof(SAllocFooter));
}


SAllocLog& start_log(unsigned number, SAllocLog::EType type)
{
    SAllocLog& slot = alloc_log[number % kLogSize];
    slot.type = SAllocLog::eInit;
    slot.completed = false;
    slot.seq_number = number;
    slot.ptr = 0;
    slot.size = 0;
    slot.type = type;
    return slot;
}


void memchk(const void* ptr, char byte, size_t size)
{
    for ( const char* p = (const char*)ptr; size; ++p, --size ) {
        if ( *p != byte ) {
            std::abort();
        }
    }
}


void* s_alloc_mem(size_t size, bool array) throw()
{
    unsigned number = seq_number.Add(1);
    SAllocLog& log =
        start_log(number, array? SAllocLog::eNewArr: SAllocLog::eNew);
    log.size = size;

    SAllocHeader* header;
    {{
        NCBI_NS_NCBI::CFastMutexGuard guard(s_alloc_mutex);
        header = (SAllocHeader*)std::malloc(get_total_size(size));
    }}
    if ( !header ) {
        log.completed = true;
        return 0;
    }
    SAllocFooter* footer = get_footer(header, size);

    header->magic = kAllocMagicHeader;
    footer->magic = kAllocMagicFooter;
    header->seq_number = footer->seq_number = number;
    header->size = footer->size = size;
    header->ptr = footer->ptr = log.ptr = get_ptr(header);

    std::memset(get_guard_before(header),
                array? kAllocFillBeforeArray: kAllocFillBeforeOne,
                get_guard_before_size());
    std::memset(get_guard_after(footer, size),
                kAllocFillAfter,
                get_guard_after_size(size));
    std::memset(get_ptr(header), kAllocFillInside, size);

    log.completed = true;
    return get_ptr(header);
}


void s_free_mem(void* ptr, bool array)
{
    unsigned number = seq_number.Add(1);
    SAllocLog& log =
        start_log(number, array ? SAllocLog::eDeleteArr: SAllocLog::eDelete);
    if ( ptr ) {
        log.ptr = ptr;
        
        SAllocHeader* header = get_header(ptr);
        if ( header->magic != kAllocMagicHeader ||
             header->seq_number >= number ||
             header->ptr != get_ptr(header) ) {
            abort();
        }
        size_t size = log.size = header->size;
        SAllocFooter* footer = get_footer(header, size);
        if ( footer->magic      != kAllocMagicFooter ||
             footer->seq_number != header->seq_number ||
             footer->ptr        != get_ptr(header) ||
             footer->size       != size ) {
            abort();
        }
        
        memchk(get_guard_before(header),
               array? kAllocFillBeforeArray: kAllocFillBeforeOne,
               get_guard_before_size());
        memchk(get_guard_after(footer, size),
               kAllocFillAfter,
               get_guard_after_size(size));

        header->magic = kFreedMagicHeader;
        footer->magic = kFreedMagicFooter;
        footer->seq_number = number;
        static bool no_clear = s_EnvFlag("DEBUG_NEW_NO_FILL_ON_DELETE");
        if ( !no_clear ) {
            std::memset(get_guard_before(header),
                        kAllocFillFree,
                        get_all_guards_size(size));
        }
        static bool no_free = s_EnvFlag("DEBUG_NEW_NO_FREE_ON_DELETE");
        if ( !no_free ) {
            NCBI_NS_NCBI::CFastMutexGuard guard(s_alloc_mutex);
            std::free(header);
        }
    }
    log.completed = true;
}


void* operator new(size_t size) throw(std::bad_alloc)
{
    void* ret = s_alloc_mem(size, false);
    if ( !ret )
        throw bad_alloc_instance;
    return ret;
}


void* operator new(size_t size, const std::nothrow_t&) throw()
{
    return s_alloc_mem(size, false);
}


void* operator new[](size_t size) throw(std::bad_alloc)
{
    void* ret = s_alloc_mem(size, true);
    if ( !ret )
        throw bad_alloc_instance;
    return ret;
}


void* operator new[](size_t size, const std::nothrow_t&) throw()
{
    return s_alloc_mem(size, true);
}


void operator delete(void* ptr) throw()
{
    s_free_mem(ptr, false);
}


void  operator delete(void* ptr, const std::nothrow_t&) throw()
{
    s_free_mem(ptr, false);
}


void  operator delete[](void* ptr) throw()
{
    s_free_mem(ptr, true);
}


void  operator delete[](void* ptr, const std::nothrow_t&) throw()
{
    s_free_mem(ptr, true);
}

#endif
