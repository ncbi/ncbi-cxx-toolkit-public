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


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr_conf.hpp>
#include <corelib/ncbimtx.hpp>
#include <memory>
#include <set>
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
    friend class CThread;

public:
    typedef void (*FCleanupBase)(void* value, void* cleanup_data);

protected:
    /// Constructor.
    CTlsBase(void);

    /// Destructor.
    ///
    /// Cleanup data and delete TLS key.
    ~CTlsBase(void);

    /// Helper method to get stored thread data. 
    void* x_GetValue(void) const;

    /// Helper method to set thread data. 
    void x_SetValue(void* value, FCleanupBase cleanup=0, void* cleanup_data=0);

    /// Helper method to reset thread data. 
    void x_Reset(void);

    /// Helper method to discard thread data. 
    void x_Discard(void);

private:
    TTlsKey m_Key;              ///< 
    bool    m_Initialized;      ///< Indicates if thread data initialized.

    /// Internal structure to store all three pointers in the same TLS.
    struct STlsData {
        void*        m_Value;       
        FCleanupBase m_CleanupFunc;
        void*        m_CleanupData;
    };

    /// Helper method to get the STlsData* 
    STlsData* x_GetTlsData(void) const;
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
    void Discard(void) { x_Discard(); }
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

    /// Get system ID of the current thread - for internal use only.
    /// The ID is unique only while the thread is running and may be
    /// re-used by another thread later.
    static void GetSystemID(TThreadSystemID* id);

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
    TID           m_ID;            /// thread ID
    TThreadHandle m_Handle;        /// platform-dependent thread handle
    bool          m_IsRun;         /// if Run() was called for the thread
    bool          m_IsDetached;    /// if the thread is detached
    bool          m_IsJoined;      /// if Join() was called for the thread
    bool          m_IsTerminated;  /// if Exit() was called for the thread
    CRef<CThread> m_SelfRef;       /// "this" -- to avoid premature destruction
    void*         m_ExitData;      /// as returned by Main() or passed to Exit()

    /// Function to use (internally) as the thread's startup function
    static TWrapperRes Wrapper(TWrapperArg arg);

    /// To store "CThread" object related to the current (running) thread
    static CTls<CThread>* sm_ThreadsTls;
    /// Safe access to "sm_ThreadsTls"
    static CTls<CThread>& GetThreadsTls(void)
    {
        if ( !sm_ThreadsTls ) {
            CreateThreadsTls();
        }
        return *sm_ThreadsTls;
    }

    /// sm_ThreadsTls initialization and cleanup functions
    static void CreateThreadsTls(void);
    friend void s_CleanupThreadsTls(void* /* ptr */);

    /// Keep all TLS references to clean them up in Exit()
    typedef set< CRef<CTlsBase> > TTlsSet;
    TTlsSet m_UsedTls;
    static void AddUsedTls(CTlsBase* tls);
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
    /// Get TLS-stored structure
    STlsData* tls_data = x_GetTlsData();

    /// If assigned, extract and return user data
    return tls_data ? tls_data->m_Value : 0;
}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

inline
CThread::TID CThread::GetSelf(void)
{
    /// Get pointer to the current thread object
    CThread* thread_ptr = GetThreadsTls().GetValue();

    /// If zero, it is main thread which has no CThread object
    return thread_ptr ? thread_ptr->m_ID : 0/*main thread*/;
}


inline
TThreadHandle CThread::GetThreadHandle()
{
    return m_Handle;
}


// Special value, stands for "no thread" thread ID
const CThread::TID kThreadID_None = 0xFFFFFFFF;


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.25  2005/05/17 17:52:56  grichenk
 * Added flag to run threads with low priority (MS-Win only)
 *
 * Revision 1.24  2004/11/30 15:05:22  dicuccio
 * Added protetected CThread::GetThreadHandle().  Doxygenated comments (// -> ///)
 *
 * Revision 1.23  2003/11/18 11:57:58  siyan
 * Changed so @addtogroup does not cross namespace boundary
 *
 * Revision 1.22  2003/09/17 15:20:45  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.21  2003/09/03 14:47:09  siyan
 * Added documentation.
 *
 * Revision 1.20  2003/07/31 19:29:03  ucko
 * SwapPointers: fix for Mac OS (classic and X) and AIX.
 *
 * Revision 1.19  2003/06/27 17:27:44  ucko
 * +SwapPointers
 *
 * Revision 1.18  2003/05/08 20:50:08  grichenk
 * Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
 *
 * Revision 1.17  2003/03/31 13:30:13  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.16  2003/03/31 13:02:47  siyan
 * Added doxygen support
 *
 * Revision 1.15  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.14  2002/09/30 16:57:34  vasilche
 * Removed extra comma in enum.
 *
 * Revision 1.13  2002/09/30 16:32:28  vasilche
 * Fixed bug with self referenced CThread.
 * Added bound running flag to CThread.
 * Fixed concurrency level on POSIX threads.
 *
 * Revision 1.12  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.11  2002/09/13 15:14:43  ucko
 * Give CSemaphore::TryWait an optional timeout (defaulting to 0)
 *
 * Revision 1.10  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.9  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.8  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.7  2001/12/10 18:07:53  vakatov
 * Added class "CSemaphore" -- application-wide semaphore
 *
 * Revision 1.6  2001/05/17 14:54:33  lavr
 * Typos corrected
 *
 * Revision 1.5  2001/03/30 22:57:32  grichenk
 * + CThread::GetSystemID()
 *
 * Revision 1.4  2001/03/26 21:45:28  vakatov
 * Workaround static initialization/destruction traps:  provide better
 * timing control, and allow safe use of the objects which are
 * either not yet initialized or already destructed. (with A.Grichenko)
 *
 * Revision 1.3  2001/03/13 22:43:19  vakatov
 * Full redesign.
 * Implemented all core functionality.
 * Thoroughly tested on different platforms.
 *
 * Revision 1.2  2000/12/11 06:48:51  vakatov
 * Revamped Mutex and RW-lock APIs
 *
 * Revision 1.1  2000/12/09 00:03:26  vakatov
 * First draft:  Mutex and RW-lock API
 *
 * ===========================================================================
 */

#endif  /* NCBITHR__HPP */
