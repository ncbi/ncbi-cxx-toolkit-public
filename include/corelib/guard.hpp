#ifndef CORELIB___GUARD__HPP
#define CORELIB___GUARD__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *     CGuard<> -- implementation of RAII-based locking guard
 *
 */

#include <corelib/ncbidbg.hpp>


BEGIN_NCBI_SCOPE


class CLockException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    // Enumerated list of document management errors
    enum EErrCode {
        eLockFailed,
        eUnlockFailed
    };

    // Translate the specific error code into a string representations of
    // that error code.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eLockFailed:   return "eLockFailed";
        case eUnlockFailed: return "eUnlockFailed";
        default:            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CLockException, CException);
};


///
/// SSimpleLock is a functor to wrap calling Lock().  While this may seem
/// excessive, it permits a lot of flexibility in defining what it means
/// to acquire a lock
///
template <class Class>
struct SSimpleLock
{
    bool operator()(Class& inst)
    {
        inst.Lock();
        return true;
    }
};


///
/// SSimpleLock is a functor to wrap calling Unlock().  While this may seem
/// excessive, it permits a lot of flexibility in defining what it means
/// to release a lock
///
template <class Class>
struct SSimpleUnlock
{
    void operator()(Class& inst)
    {
        try {
            inst.Unlock();
        }
        catch (CException& e) {
            ERR_POST("Unlock(): failed to release lock: " << e.GetMsg());
            NCBI_THROW(CLockException, eUnlockFailed,
                       e.GetMsg());
        }
        catch (std::exception& e) {
            ERR_POST("Unlock(): failed to release lock: " << e.what());
            NCBI_THROW(CLockException, eUnlockFailed,
                       e.what());
        }
    }
};


///
/// class CGuard<> implements a templatized "resource acquisition is
/// initialization" (RAII) locking guard.
///
/// This guard is useful for locking resources in an exception-safe manner.  The
/// classic use of this is to lock a mutex within a C++ scope, as follows:
///
/// void SomeFunction()
/// {
///     CGuard<CMutex> GUARD(some_mutex);
///     [...perform some thread-safe operations...]
/// }
///
/// If an exception is thrown during the performance of any operations while the
/// guard is held, the guarantee of C++ stack-unwinding will force the guard's
/// destructor to release whatever resources were acquired.
///
///

template <class Class,
          class Lock = SSimpleLock<Class>,
          class Unlock = SSimpleUnlock<Class> >
class CGuard
{
public:

    CGuard(Lock lock = Lock(), Unlock unlock = Unlock())
        : m_Instance(NULL),
          m_Lock(lock),
          m_Unlock(unlock)
    {
    }

    /// This constructor locks the resource passed.
    CGuard(Class& inst, Lock lock = Lock(), Unlock unlock = Unlock())
        : m_Instance(NULL),
          m_Lock(lock),
          m_Unlock(unlock)
    {
        Guard(inst);
    }

    /// Destructor releases the resource.
    ~CGuard()
    {
        Release();
    }

    /// Manually force the resource to be released.
    void Release()
    {
        if (m_Instance) {
            m_Unlock(*m_Instance);
            m_Instance = NULL;
        }
    }

    /// Manually force the guard to protect some other resource.
    void Guard(Class& inst)
    {
        Release();
        m_Lock(inst);
        m_Instance = &inst;
    }

private:
    /// Maintain a pointer to the original resource that is being guarded
    Class* m_Instance;
    Lock m_Lock;
    Unlock m_Unlock;
};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/16 11:31:08  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // CORELIB___GUARD__HPP
