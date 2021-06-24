#ifndef PSGS_UV_LOOP_BINDER__HPP
#define PSGS_UV_LOOP_BINDER__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG main uv loop binder which lets to invoke a callback
 *                   from within a uv main loop
 *
 */

#include <uv.h>

#include <functional>
#include <mutex>
#include <list>
using namespace std;



// The class provides ability to setup a callback which will be invoked from
// the libuv event loop.
// The design idea is that there is a protected queue of the callbacks which
// should be invoked from the libuv event loop. When the users postpone their
// callbacks it essentially leads to two actions:
// - memorize the callback in the protected queue
// - send the libuv async message to wakeup libuv if it is waiting on something
// Then there is as well a prepare callback setup for libuv once at the very
// beginning of the server lifetime. The prepare callback is invoked when libuv
// is woken up by an async message. The responsibility of the prepare callback
// is to check the callback queue and invoke the stored callbacks.
//
// The drawback is that the prepare callback is called once per libuv event
// loop. However it should not affect the performance severely.
//
// The async message cannot be used alone because there is no guarantee that
// its callback will be called exactly once per uv_async_send()
// See more info here:
// https://stackoverflow.com/questions/18130724/libuv-uv-check-t-and-uv-prepare-t-usage
class CPSGS_UvLoopBinder
{
    public:
        using TProcessorCB = function<void(void *  user_data)>;

    public:
        CPSGS_UvLoopBinder(uv_loop_t *  loop);
        ~CPSGS_UvLoopBinder();

        // The provided callback will be invoked from the libuv event loop.
        // The invokation will happened once.
        // The method is thread safe.
        void PostponeInvoke(TProcessorCB  cb, void *  user_data);

    public:
        // Internal usage only.
        // The libuv C-style callback calls this method upon the prepare
        // callback
        void x_UvOnPrepare(void);

    private:
        struct SUserCallback
        {
            SUserCallback(void *  user_data, TProcessorCB  cb) :
                m_UserData(user_data), m_ProcCallback(cb)
            {}

            void *          m_UserData;
            TProcessorCB    m_ProcCallback;
        };

    private:
        uv_prepare_t            m_Prepare;
        uv_async_t              m_Async;

        mutex                   m_QueueLock;
        list<SUserCallback>     m_Callbacks;
};


#endif  // PSGS_UV_LOOP_BINDER__HPP

