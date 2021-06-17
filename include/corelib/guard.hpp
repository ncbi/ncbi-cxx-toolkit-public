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

#include <corelib/ncbimisc.hpp>


BEGIN_NCBI_SCOPE


///
/// SSimpleLock is a functor to wrap calling Lock().  While this may seem
/// excessive, it permits a lot of flexibility in defining what it means
/// to acquire a lock
///
template <class Resource>
struct SSimpleLock
{
    typedef Resource resource_type;
    void operator()(resource_type& resource) const
    {
        resource.Lock();
    }
};


///
/// SSimpleLock is a functor to wrap calling Unlock().  While this may seem
/// excessive, it permits a lot of flexibility in defining what it means
/// to release a lock
///
template <class Resource>
struct SSimpleUnlock
{
    typedef Resource resource_type;
    void operator()(resource_type& resource) const
    {
        resource.Unlock();
    }
};


///
/// class CGuard<> implements a templatized "resource acquisition is
/// initialization" (RAII) locking guard.
///
/// This guard is useful for locking resources in an exception-safe manner.
/// The classic use of this is to lock a mutex within a C++ scope, as follows:
///
/// void SomeFunction()
/// {
///     CGuard<CMutex> GUARD(some_mutex);
///     [...perform some thread-safe operations...]
/// }
///
/// If an exception is thrown during the performance of any operations while
/// the guard is held, the guarantee of C++ stack-unwinding will force
/// the guard's destructor to release whatever resources were acquired.
///
///

enum EEmptyGuard {
    eEmptyGuard
};


/// Base class for CGuard<> template.
class NCBI_XNCBI_EXPORT CGuard_Base
{
public:
    /// Exception reporting options.
    enum EReportExceptions {
        eReport, ///< Use ERR_POST to report exceptions.
        eSilent  ///< Silently discard exceptions.
    };

protected:
    static void ReportException(std::exception& ex);
};


template <class Resource,
          class Lock = SSimpleLock<Resource>,
          class Unlock = SSimpleUnlock<Resource>,
          CGuard_Base::EReportExceptions ReportExceptions = CGuard_Base::eReport>
class CGuard : public CGuard_Base
{
public:
    typedef Resource resource_type;
    typedef resource_type* resource_ptr;
    typedef Lock lock_type;
    typedef Unlock unlock_type;
    typedef CGuard<Resource, Lock, Unlock, ReportExceptions> TThisType;

    explicit CGuard(EEmptyGuard /*empty*/)
        {
        }
    
    /// This constructor locks the resource passed.
    explicit CGuard(resource_type& resource)
        {
            Guard(resource);
        }
    
    explicit CGuard(EEmptyGuard /*empty*/,
                    const lock_type& lock,
                    const unlock_type& unlock = unlock_type())
        : m_Data(lock, pair_base_member<unlock_type, resource_ptr>(unlock, 0))
        {
        }
    
    /// This constructor locks the resource passed.
    explicit CGuard(resource_type& resource,
                    const lock_type& lock,
                    const unlock_type& unlock = unlock_type())
        : m_Data(lock, pair_base_member<unlock_type, resource_ptr>(unlock, 0))
        {
            Guard(resource);
        }
    
    /// Destructor releases the resource.
    ~CGuard()
        {
            try {
                Release();
            } catch(std::exception& ex) {
                // catch and report or ignore std exceptions in destructor
                if (ReportExceptions == eReport) {
                    ReportException(ex);
                }
            }
        }
    
    /// Manually force the resource to be released.
    void Release()
        {
            if ( GetResource() ) {
                GetUnlock()(*GetResource());
                GetResource() = 0;
            }
        }
    
    /// Manually force the guard to protect some other resource.
    void Guard(resource_type& resource)
        {
            Release();
            GetLock()(resource);
            GetResource() = &resource;
        }
    
protected:
    resource_ptr& GetResource(void)
        {
            return m_Data.second().second();
        }
    lock_type& GetLock(void)
        {
            return m_Data.first();
        }
    unlock_type& GetUnlock(void)
        {
            return m_Data.second().first();
        }

private:
    /// Maintain a pointer to the original resource that is being guarded
    pair_base_member<lock_type,
                     pair_base_member<unlock_type,
                                      resource_ptr> >  m_Data;

private:
    // prevent copying
    CGuard(const CGuard<resource_type, lock_type, unlock_type>&);
    void operator=(const CGuard<resource_type, lock_type, unlock_type>&);
};


///
/// CNoLock is a simple no-op lock which does no real locking.
/// The class can be used as a template argument instead of normal
/// locks (e.g. CMutex).
///
class CNoLock
{
public:
    typedef CGuard<CNoLock> TReadLockGuard;
    typedef CGuard<CNoLock> TWriteLockGuard;

    void Lock  (void) const {}
    void Unlock(void) const {}
};


END_NCBI_SCOPE

#endif  // CORELIB___GUARD__HPP
