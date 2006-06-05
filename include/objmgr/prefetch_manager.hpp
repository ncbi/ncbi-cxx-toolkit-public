#ifndef PREFETCH_MANAGER__HPP
#define PREFETCH_MANAGER__HPP

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
*   Prefetch manager
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/prefetch_manager_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CPrefetchToken;
class CPrefetchRequest;
class CPrefetchManager;
class CPrefetchManager_Impl;
class CPrefetchThread;

class NCBI_XOBJMGR_EXPORT IPrefetchAction : public SPrefetchTypes
{
public:
    virtual ~IPrefetchAction(void);
    
    virtual bool Execute(CPrefetchToken token) = 0;
};


class NCBI_XOBJMGR_EXPORT IPrefetchActionSource
{
public:
    virtual ~IPrefetchActionSource(void);

    virtual CIRef<IPrefetchAction> GetNextAction(void) = 0;
};


class NCBI_XOBJMGR_EXPORT IPrefetchListener : public SPrefetchTypes
{
public:
    virtual ~IPrefetchListener(void);

    virtual void PrefetchNotify(CPrefetchToken token, EEvent event) = 0;
};


class NCBI_XOBJMGR_EXPORT CPrefetchToken : public SPrefetchTypes
{
public:
    CPrefetchToken(void);
    ~CPrefetchToken(void);
    CPrefetchToken(const CPrefetchToken& token);
    CPrefetchToken& operator=(const CPrefetchToken& token);

    DECLARE_OPERATOR_BOOL_REF(m_Handle);

    IPrefetchAction* GetAction(void) const;
    IPrefetchListener* GetListener(void) const;
    void SetListener(IPrefetchListener* listener);
    
    TPriority GetPriority(void) const;
    TPriority SetPriority(TPriority priority);

    void Cancel(void) const;

    EState GetState(void) const;
    
    // in one of final states: completed, failed, canceled
    bool IsDone(void) const;

    TProgress GetProgress(void) const;
    TProgress SetProgress(TProgress progress);

protected:
    friend class CPrefetchRequest;
    friend class CPrefetchManager_Impl;
    friend class CPrefetchThread;

    CPrefetchToken(CPrefetchRequest* impl);
    CPrefetchToken(CPrefetchManager_Impl::TItemHandle handle);

    CPrefetchRequest& x_GetImpl(void) const;

private:
    CPrefetchManager_Impl::TItemHandle  m_Handle;
    CRef<CPrefetchManager_Impl>         m_Manager;
};


class NCBI_XOBJMGR_EXPORT CPrefetchManager :
    public CObject,
    public SPrefetchTypes
{
public:
    CPrefetchManager(void);
    ~CPrefetchManager(void);

    size_t GetThreadCount(void) const;
    size_t SetThreadCount(size_t count);

    CPrefetchToken AddAction(TPriority priority,
                             IPrefetchAction* action,
                             IPrefetchListener* listener = 0);
    CPrefetchToken AddAction(IPrefetchAction* action,
                             IPrefetchListener* listener = 0)
        {
            return AddAction(0, action, listener);
        }

protected:
    friend class CPrefetchToken;
    friend class CPrefetchThread;

private:
    CRef<CPrefetchManager_Impl> m_Impl;

private:
    CPrefetchManager(const CPrefetchManager&);
    void operator=(const CPrefetchManager&);
};


/// This exception is used to interrupt actions canceled by user
class NCBI_XOBJMGR_EXPORT CPrefetchCanceled : public CException
{
public:
    enum EErrCode {
        eCanceled
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CPrefetchCanceled,CException);
};


class NCBI_XOBJMGR_EXPORT CPrefetchSequence : public CObject
{
public:
    CPrefetchSequence(CPrefetchManager& manager,
                      IPrefetchActionSource* source,
                      size_t active_size = 10);
    ~CPrefetchSequence(void);
    
    /// Returns next action waiting for its result if necessary
    CPrefetchToken GetNextToken(void);

protected:
    void EnqueNextAction(void);

private:
    CRef<CPrefetchManager>       m_Manager;
    CIRef<IPrefetchActionSource> m_Source;
    CMutex                       m_Mutex;
    list<CPrefetchToken>         m_ActiveTokens;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER__HPP
