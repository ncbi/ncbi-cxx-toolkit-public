#ifndef CORELIB___NCBITHR__HPP
#define CORELIB___NCBITHR__HPP

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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 *
 */

/// @file ncbithr.hpp
/// Multi-threading -- classes, functions, and features.
///
///   TLS:
///   -   CTlsBase         -- TLS implementation (base class for CTls<>)
///   -   CTls<>           -- thread local storage template
///
///   THREAD:
///   -   CThread          -- thread wrapper class
///


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <list>


BEGIN_NCBI_SCOPE

/** @addtogroup Threads
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
/// CTlBase --
///
/// Base class for CTls<> for storing thread-specific data.

class NCBI_XNCBI_EXPORT CTlsBase : public CObject
{
    friend class CRef<CTlsBase>;
    friend class CUsedTlsBases;
    friend class CStaticTlsHelper;

public:
    typedef void (*FCleanupBase)(void* value, void* cleanup_data);

protected:
    /// Constructor.
    CTlsBase(bool auto_destroy)
        : m_AutoDestroy(auto_destroy)
    {}

    /// Destructor.
    ///
    /// Cleanup data and delete TLS key.
    ~CTlsBase(void)
    {
        if (m_AutoDestroy) {
            x_Destroy();
        }
    }

    /// Helper method to get stored thread data.
    void* x_GetValue(void) const;

    /// Helper method to set thread data.
    void x_SetValue(void* value, FCleanupBase cleanup=0, void* cleanup_data=0);

    /// Helper method to reset thread data.
    void x_Reset(void);

protected:
    /// Initialize thread data
    void x_Init(void);

    /// Destroy thread data
    void x_Destroy(void);

private:
    TTlsKey m_Key;              ///<
    bool    m_Initialized;      ///< Indicates if thread data initialized.
    bool    m_AutoDestroy;      ///< Indicates if object should be destroyed
                                ///< in destructor

    /// Internal structure to store all three pointers in the same TLS.
    struct STlsData {
        void*        m_Value;
        FCleanupBase m_CleanupFunc;
        void*        m_CleanupData;
    };

    /// Helper method to get the STlsData*
    STlsData* x_GetTlsData(void) const;
    /// Deletes STlsData* structure and managed pointer
    /// Returns true if CTlsBase must be deregistered from current thread
    bool x_DeleteTlsData(void);

public:
    static void CleanupTlsData(void *data);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTls --
///
/// Define template class for thread local storage.

template <class TValue>
class CTls : public CTlsBase
{
public:
    CTls(void) : CTlsBase(true)
    {
        DoDeleteThisObject();
        x_Init();
    }

    /// Get the pointer previously stored by SetValue().
    ///
    /// Return 0 if no value has been stored, or if Reset() was last called.
    /// @sa
    ///   SetValue()
    TValue* GetValue(void) const
    {
        return reinterpret_cast<TValue*> (x_GetValue());
    }

    /// Define cleanup function type, FCleanup.
    typedef void (*FCleanup)(TValue* value, void* cleanup_data);

    /// Set value.
    ///
    /// Cleanup previously stored value, and set the new value.
    /// The "cleanup" function and "cleanup_data" will be used to
    /// destroy the new "value" in the next call to SetValue() or Reset().
    /// Do not cleanup if the new value is equal to the old one.
    /// @param value
    ///   New value to set.
    /// @param cleanup
    ///   Cleanup function.
    ///   Do not cleanup if default of 0 is specified or if new value is the
    ///   same as old value.
    /// @param cleanup_data
    ///   One of the parameters to the cleanup function.
    /// @sa
    ///   GetValue()
    void SetValue(TValue* value, FCleanup cleanup = 0, void* cleanup_data = 0)
    {
        x_SetValue(value,
                   reinterpret_cast<FCleanupBase> (cleanup), cleanup_data);
    }

    /// Reset thread local storage.
    ///
    /// Reset thread local storage to its initial value (as it was before the
    /// first call to SetValue()). Do cleanup if the cleanup function was
    /// specified in the previous call to SetValue().
    ///
    /// Reset() will always be called automatically on the thread termination,
    /// or when the TLS is destroyed.
    void Reset(void) { x_Reset(); }

    /// Discard thread local storage.
    ///
    /// Schedule the TLS to be destroyed as soon as there are no CRef to it
    /// left.
    void Discard(void) { x_Reset(); }
};


// TLS static variable support in case there is no compiler support
#ifdef NCBI_TLS_VAR
# define DECLARE_TLS_VAR(type, var) NCBI_TLS_VAR type var
#else
/////////////////////////////////////////////////////////////////////////////
///
/// CSimpleStaticTls
///
/// Define template class for simple data (POD) in thread local storage.
/// The data type must fit in the same memory as pointer type.
/// Use compiler support if possible, and direct pthread calls otherwise.
/// The variable of this type is MT-safe to be declared statically.
/// Initial value of the variable is zero or equivalent.
template<class V> class CSimpleStaticTls {
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
    /// Getter - returns value stored in TLS.
    operator value_type() const {
        return SCaster<value_type>::FromVoidP(pthread_getspecific(x_GetKey()));
    }
    /// Setter - changes value stored in TLS.
    void operator=(const value_type& v) {
        pthread_setspecific(x_GetKey(), SCaster<value_type>::ToVoidP(v));
    }
};
# define DECLARE_TLS_VAR(type, var) CSimpleStaticTls<type> var
#endif


#define NCBI_STATIC_TLS_VIA_SAFE_STATIC_REF 1

#if NCBI_STATIC_TLS_VIA_SAFE_STATIC_REF

template <class TValue>
class CStaticTls_Callbacks
{
public:
    typedef void (*FUserCleanup)(void*  ptr);
    CStaticTls_Callbacks(FUserCleanup cleanup) : m_Cleanup(cleanup) {}

    CTls<TValue>* Create() {
        return new CTls<TValue>;
    }
    void Cleanup(CTls<TValue>& value) {
        if ( m_Cleanup ) {
            m_Cleanup(&value);
        }
    }

private:
    FUserCleanup m_Cleanup;
};

template<class TValue>
class CStaticTls : private CSafeStatic<CTls<TValue>, CStaticTls_Callbacks<TValue> >
{
private:
    typedef CSafeStatic<CTls<TValue>, CStaticTls_Callbacks<TValue> > TParent;

public:
    typedef CSafeStaticLifeSpan TLifeSpan;
    /// User cleanup function type
    typedef void (*FUserCleanup)(void*  ptr);
    /// Define cleanup function type, FCleanup.
    typedef void (*FCleanup)(TValue* value, void* cleanup_data);

    CStaticTls(FUserCleanup user_cleanup = 0,
               TLifeSpan life_span = TLifeSpan::GetDefault())
        : TParent(CStaticTls_Callbacks<TValue>(user_cleanup), life_span)
    {
    }

    TValue* GetValue(void) {
        return TParent::Get().GetValue();
    }
    void SetValue(TValue* value, FCleanup cleanup = 0, void* cleanup_data = 0){
        TParent::Get().SetValue(value, cleanup, cleanup_data);
    }

    friend class CUsedTlsBases;
};

#else // !NCBI_STATIC_TLS_VIA_SAFE_STATIC_REF
template <class TValue> class CStaticTls;

/// Helper class to control life time of CStaticTls object
class CStaticTlsHelper : public CSafeStaticPtr_Base
{
private:
    template <class TValue> friend class CStaticTls;

    CStaticTlsHelper(FUserCleanup user_cleanup,
                     TLifeSpan    life_span)
        : CSafeStaticPtr_Base(sx_SelfCleanup, user_cleanup, life_span)
    {}

    void Set(CTlsBase* object)
    {
        CMutexGuard guard(CSafeStaticPtr_Base::sm_Mutex);
        if ( m_Ptr == 0 ) {
            // Set the new object and register for cleanup
            if ( object ) {
                m_Ptr = object;
                CSafeStaticGuard::Register(this);
            }
        }
    }

    static void sx_SelfCleanup(CSafeStaticPtr_Base* safe_static,
                               CMutexGuard& guard)
    {
        CStaticTlsHelper* this_ptr = static_cast<CStaticTlsHelper*>(safe_static);
        if ( CTlsBase* ptr = static_cast<CTlsBase*>(this_ptr->m_Ptr) ) {
            FUserCleanup user_cleanup = this_ptr->m_UserCleanup;
            this_ptr->m_Ptr = 0;
            guard.Release();
            if ( user_cleanup ) {
                user_cleanup(ptr);
            }
            ptr->x_Destroy();
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStaticTls --
///
/// Define template class for thread local storage in static variable
/// (as thread local storage objects are meaningful only in static content).
/// Class can be used only as static variable type.

template <class TValue>
class CStaticTls : public CTlsBase
{
public:
    /// Life span
    typedef CSafeStaticLifeSpan TLifeSpan;
    /// User cleanup function type
    typedef void (*FUserCleanup)(void*  ptr);

    // Set user-provided cleanup function to be executed on destruction.
    // Life span allows to control destruction of objects. Objects with
    // the same life span are destroyed in the order reverse to their
    // creation order.
    CStaticTls(FUserCleanup user_cleanup = 0,
               TLifeSpan life_span = TLifeSpan::GetDefault())
        : CTlsBase(false),
          m_SafeHelper(user_cleanup, life_span)
    {}

    /// Get the pointer previously stored by SetValue().
    ///
    /// Return 0 if no value has been stored, or if Reset() was last called.
    /// @sa
    ///   SetValue()
    TValue* GetValue(void)
    {
        if (!m_SafeHelper.m_Ptr) {
            x_SafeInit();
        }
        return reinterpret_cast<TValue*> (x_GetValue());
    }

    /// Define cleanup function type, FCleanup.
    typedef void (*FCleanup)(TValue* value, void* cleanup_data);

    /// Set value.
    ///
    /// Cleanup previously stored value, and set the new value.
    /// The "cleanup" function and "cleanup_data" will be used to
    /// destroy the new "value" in the next call to SetValue() or Reset().
    /// Do not cleanup if the new value is equal to the old one.
    /// @param value
    ///   New value to set.
    /// @param cleanup
    ///   Cleanup function.
    ///   Do not cleanup if default of 0 is specified or if new value is the
    ///   same as old value.
    /// @param cleanup_data
    ///   One of the parameters to the cleanup function.
    /// @sa
    ///   GetValue()
    void SetValue(TValue* value, FCleanup cleanup = 0, void* cleanup_data = 0)
    {
        if (!m_SafeHelper.m_Ptr) {
            x_SafeInit();
        }
        x_SetValue(value,
                   reinterpret_cast<FCleanupBase> (cleanup), cleanup_data);
    }

    /// Reset thread local storage.
    ///
    /// Reset thread local storage to its initial value (as it was before the
    /// first call to SetValue()). Do cleanup if the cleanup function was
    /// specified in the previous call to SetValue().
    ///
    /// Reset() will always be called automatically on the thread termination,
    /// or when the TLS is destroyed.
    void Reset(void)
    {
        if (!m_SafeHelper.m_Ptr) {
            x_SafeInit();
        }
        x_Reset();
    }

private:
    /// Object derived from CSafeStaticPtr_Base to help manage life time
    /// of the object
    CStaticTlsHelper m_SafeHelper;

    /// Initialize the object in SafeStaticRef-ish manner
    void x_SafeInit(void);
};
#endif // NCBI_STATIC_TLS_VIA_SAFE_STATIC_REF

class NCBI_XNCBI_EXPORT CUsedTlsBases
{
public:
    CUsedTlsBases(void);
    ~CUsedTlsBases(void);

    /// The function must be called before thread termination when
    /// using native threads instead of CThread. Otherwise any data
    /// allocated by the thread and put into TLS will not be destroyed
    /// and will cause memory leaks.
    void ClearAll(void);

    void Register(CTlsBase* tls);
    void Deregister(CTlsBase* tls);

    /// Get the list of used TLS-es for the current thread
    static CUsedTlsBases& GetUsedTlsBases(void);

    /// Clear used TLS-es for the current thread
    static void ClearAllCurrentThread(void);

    /// Init TLS, call before creating thread
    static void Init(void);

private:
    typedef set<CTlsBase*> TTlsSet;
    TTlsSet m_UsedTls;

    static CStaticTls<CUsedTlsBases> sm_UsedTlsBases;

private:
    CUsedTlsBases(const CUsedTlsBases&);
    void operator=(const CUsedTlsBases&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CThread --
///
/// Thread wrapper class.
///
///  Base class for user-defined threads. Creates the new thread, then
///  calls user-provided Main() function. The thread then can be detached
///  or joined. In any case, explicit destruction of the thread is prohibited.

class NCBI_XNCBI_EXPORT CThread : public CObject
{
    friend class CRef<CThread>;
    friend class CTlsBase;

public:
    /// Constructor.
    ///
    /// Must be allocated in the heap only!.
    CThread(void);

    /// Which mode should the thread run in.
    enum ERunMode {
        fRunDefault  = 0x00,    ///< Default mode
        fRunDetached = 0x01,    ///< Run the thread detached (non-joinable)
        fRunBound    = 0x10,    ///< Run thread in a 1:1 thread:LPW mode
                                ///< - may not be supported and will be
                                ///< ignored on some platforms
        fRunUnbound  = 0x20,    ///< Run thread in a N:1 thread:LPW mode
                                ///< - may not be supported and will be
                                ///< ignored on some platforms
        fRunNice     = 0x40,    ///< Run thread with low priority (MS-Win only)
        fRunAllowST  = 0x100    ///< Allow threads to run in single thread
                                ///< builds
    };

    /// Bitwise OR'd flags for thread creation passed to Run().
    typedef int TRunMode;

    /// Run the thread.
    ///
    /// Create a new thread, initialize it, and call user-provided Main()
    /// method.
    bool Run(TRunMode flags = fRunDefault);

    /// Inform the thread that user does not need to wait for its termination.
    /// The thread object will be destroyed by Exit().
    /// If the thread has already been terminated by Exit, Detach() will
    /// also schedule the thread object for destruction.
    /// NOTE:  it is no more safe to use this thread object after Detach(),
    ///        unless there are still CRef<> based references to it!
    void Detach(void);

    /// Wait for the thread termination.
    /// The thread object will be scheduled for destruction right here,
    /// inside Join(). Only one call to Join() is allowed.
    void Join(void** exit_data = 0);

    /// Cancel current thread. If the thread is detached, then schedule
    /// the thread object for destruction.
    /// Cancellation is performed by throwing an exception of type
    /// CExitThreadException to allow destruction of all objects in
    /// thread's stack, so Exit() method shell not be called from any
    /// destructor.
    static void Exit(void* exit_data);

    /// If the thread has not been Run() yet, then schedule the thread object
    /// for destruction, and return TRUE.
    /// Otherwise, do nothing, and return FALSE.
    bool Discard(void);

    /// Get ID of current thread (for main thread it is always zero).
    typedef unsigned int TID;
    static TID GetSelf(void);

    /// Get current CThread object (or NULL, if main thread)
    static CThread* GetCurrentThread(void);

    /// Get system ID of the current thread - for internal use only.
    /// The ID is unique only while the thread is running and may be
    /// re-used by another thread later.
    static void GetSystemID(TThreadSystemID* id);

    /// Get total amount of threads
    /// This amount does not contain main thread.
    static unsigned int GetThreadsCount();

    /// Set name for the current thread.
    /// This works only on Linux, no-op on other platforms
    static void SetCurrentThreadName(const char*);

    /// Initialize main thread's TID.
    /// The function must be called from the main thread if the application
    /// is using non-toolkit threads. Otherwise getting thread id of a
    /// native thread will return zero.
    static void InitializeMainThreadId(void);

protected:
    /// Derived (user-created) class must provide a real thread function.
    virtual void* Main(void) = 0;

    /// Override this to execute finalization code.
    /// Unlike destructor, this code will be executed before
    /// thread termination and as a part of the thread.
    virtual void OnExit(void);

    /// To be called only internally!
    /// NOTE:  destructor of the derived (user-provided) class should be
    ///        declared "protected", too!
    virtual ~CThread(void);

    TThreadHandle GetThreadHandle();

private:
    TThreadHandle m_Handle;        ///< platform-dependent thread handle
    bool          m_IsRun;         ///< if Run() was called for the thread
    bool          m_IsDetached;    ///< if the thread is detached
    bool          m_IsJoined;      ///< if Join() was called for the thread
    bool          m_IsTerminated;  ///< if Exit() was called for the thread
    CRef<CThread> m_SelfRef;       ///< "this" -- to avoid premature destruction
    void*         m_ExitData;      ///< as returned by Main() or passed to Exit()

#if defined NCBI_THREAD_PID_WORKAROUND
    friend class CProcess;
    TPid          m_ThreadPID;     ///< Cache thread PID to detect forks

    static TPid sx_GetThreadPid(void);
    static void sx_SetThreadPid(TPid pid);
#endif

    static volatile unsigned int sm_ThreadsCount;  ///< Total amount of threads

    /// initalize new thread id, must be called from Wrapper().
    void x_InitializeThreadId(void);

    /// Function to use (internally) as the thread's startup function
    static TWrapperRes Wrapper(TWrapperArg arg);
    friend TWrapperRes ThreadWrapperCaller(TWrapperArg arg);

    /// Prohibit copying and assigning
    CThread(const CThread&);
    CThread& operator= (const CThread&);
};


class NCBI_XNCBI_EXPORT CThreadException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eRunError,          ///< Failed to run thread
        eControlError,      ///< Failed to control thread's state
        eOther              ///< Other thread errors
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CThreadException, CException);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CTlsBase::
//

inline
CTlsBase::STlsData* CTlsBase::x_GetTlsData(void)
const
{
    if ( !m_Initialized ) {
        return 0;
    }

    void* tls_data;

#if defined(NCBI_WIN32_THREADS)
    tls_data = TlsGetValue(m_Key);
#elif defined(NCBI_POSIX_THREADS)
    tls_data = pthread_getspecific(m_Key);
#else
    tls_data = m_Key;
#endif

    return static_cast<STlsData*> (tls_data);
}


inline
void* CTlsBase::x_GetValue(void)
const
{
    // Get TLS-stored structure
    STlsData* tls_data = x_GetTlsData();

    // If assigned, extract and return user data
    return tls_data ? tls_data->m_Value : 0;
}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

#if !NCBI_STATIC_TLS_VIA_SAFE_STATIC_REF
template <class TValue>
inline
void CStaticTls<TValue>::x_SafeInit(void)
{
    m_SafeHelper.Set(this);
}
#endif


/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

inline
TThreadHandle CThread::GetThreadHandle()
{
    return m_Handle;
}


inline
unsigned int CThread::GetThreadsCount() {
    return sm_ThreadsCount;
}


// Special value, stands for "no thread" thread ID
const CThread::TID kThreadID_None = 0xFFFFFFFF;


END_NCBI_SCOPE

#endif  /* NCBITHR__HPP */
