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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CPrefetchToken;
class CPrefetchToken_Impl;
class CPrefetchManager;
class CPrefetchManager_Impl;
class CPrefetchThread;

struct SPrefetchTypes
{
    enum EFlags {
        fOwnNone        = 0,
        fOwnAction      = 1<<0,
        fOwnListener    = 1<<1,
        fOwnAll         = fOwnAction | fOwnListener
    };
    typedef int TFlags;

    enum EState {
        eQueued,    // placed in queue
        eStarted,   // moved from queue to processing
        eAdvanced,  // got new data while processing
        eCompleted, // finished processing successfully
        eCanceled,  // canceled by user request
        eFailed     // finished processing unsuccessfully
    };
    typedef EState EEvent;
    
    typedef int TPriority;
    typedef int TProgress;
};

class NCBI_XOBJMGR_EXPORT IPrefetchAction : public SPrefetchTypes
{
public:
    virtual ~IPrefetchAction(void);
    
    virtual bool Execute(CPrefetchToken token) = 0;
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

    DECLARE_OPERATOR_BOOL_REF(m_Impl);

    IPrefetchAction* GetAction(void) const;
    IPrefetchListener* GetListener(void) const;
    
    TPriority GetPriority(void) const;
    TPriority SetPriority(TPriority priority);

    void Wait(void) const;
    void Cancel(void) const;

    EState GetState(void) const;
    
    // in one of final states: completed, failed, canceled
    bool IsDone(void) const;

    TProgress GetProgress(void) const;
    TProgress SetProgress(TProgress progress);

protected:
    friend class CPrefetchToken_Impl;
    friend class CPrefetchManager_Impl;
    friend class CPrefetchThread;

    CPrefetchToken(CPrefetchToken_Impl* impl);

    CPrefetchToken_Impl& x_GetImpl(void) const
        {
            return m_Impl.GetNCObject();
        }

private:
    CRef<CPrefetchToken_Impl>   m_Impl;
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
                             IPrefetchListener* listener = 0,
                             TFlags flags = fOwnNone);
    CPrefetchToken AddAction(IPrefetchAction* action,
                             IPrefetchListener* listener = 0,
                             TFlags flags = fOwnNone)
        {
            return AddAction(0, action, listener, flags);
        }

protected:
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


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER__HPP
