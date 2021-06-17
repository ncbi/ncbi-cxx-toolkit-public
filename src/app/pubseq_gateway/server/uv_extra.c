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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <uv.h>

#include "uv_extra.h"

#define CONTAINER_OF(ptr, type, member) ({                                                      \
    const typeof(((type*)(0))->member) *__mptr = ((const typeof(((type*)(0))->member) *)(ptr)); \
    (type*)((char*)(__mptr) - offsetof(type, member));                                          \
})


struct import_worker_t
{
    unsigned int                m_Id;
    struct uv_export_t *        m_Exp;
    uv_sem_t                    m_Sem;
    uv_pipe_t                   m_Ipc;
    uv_connect_t                m_ConnectReq;
    char                        m_StaticIpcBuf[256];
    uv_stream_t *               m_Handle;
    volatile int                m_Error;
    char                        m_IpcClosed;
    volatile char               m_ImportStarted;
    volatile char               m_ImportFinished;
};


struct uv_export_t
{
    unsigned int                m_IdCounter;
    const char *                m_IpcName;
    uv_stream_t *               m_Handle;
    uv_pipe_t                   m_Ipc;
    uv_handle_type              m_HandleType;
    unsigned short              m_WorkerCount;
    struct import_worker_t *    m_Workers;
    volatile int                m_Error;
    volatile int                m_ErrorCount;
    volatile char               m_InWaiting;
    char                        m_IpcClosed;
};

/* uv_export related procedures and types */

struct ipc_peer_t
{
    uv_pipe_t                   m_PeerHandle;
    uv_write_t                  m_WriteReq;
    struct uv_export_t *        m_Exp;
};


static void s_on_peer_close(uv_handle_t *  handle)
{
    struct ipc_peer_t *     pc = CONTAINER_OF(handle, struct ipc_peer_t,
                                              m_PeerHandle);
    free(pc);
}


static void s_on_ipc_write(uv_write_t *  req, int  status)
{
    struct ipc_peer_t *     pc = CONTAINER_OF(req, struct ipc_peer_t,
                                              m_WriteReq);
    if (status) {
        ++pc->m_Exp->m_ErrorCount;
        pc->m_Exp->m_Error = status;
    }
    uv_close((uv_handle_t*)&pc->m_PeerHandle, s_on_peer_close);
}


static void s_on_ipc_connection(uv_stream_t *  ipc, int  status)
{
    char                    msg[] = "PING";
    struct uv_export_t *    exp = CONTAINER_OF(ipc, struct uv_export_t, m_Ipc);

    if (status) {
        ++exp->m_ErrorCount;
        exp->m_Error = status;
        return;
    }

    uv_buf_t buf = uv_buf_init(msg, sizeof(msg) - 1);

    struct ipc_peer_t *     pc = calloc(1, sizeof(*pc));
    pc->m_Exp = exp;

    assert(ipc->type == UV_NAMED_PIPE);

    // Here the pipe must be intialized with ipc == 1 because the further
    // call of uv_write2(...) sends a handle and thus must use ipc == 1
    int     e = uv_pipe_init(ipc->loop, (uv_pipe_t*)&pc->m_PeerHandle, 1);
    if (e != 0) {
        free(pc);
        ++exp->m_ErrorCount;
        exp->m_Error = e;
        return;
    }

    do {
        e = uv_accept(ipc, (uv_stream_t*)&pc->m_PeerHandle);
        if (e == 0)
            break;
        else if (-e != EAGAIN) {
            uv_close((uv_handle_t*)&pc->m_PeerHandle, s_on_peer_close);
            ++exp->m_ErrorCount;
            exp->m_Error = e;
            return;
        }
    } while (1);

    /* send the handle */
    e = uv_write2(&pc->m_WriteReq,
                  (uv_stream_t*)&pc->m_PeerHandle,
                  &buf, 1,
                  exp->m_Handle, s_on_ipc_write);
    if (e != 0) {
        uv_close((uv_handle_t*)&pc->m_PeerHandle, s_on_peer_close);
        ++exp->m_ErrorCount;
        exp->m_Error = e;
        return;
    }
}



int uv_export_start(uv_loop_t *  loop, uv_stream_t *  handle,
                    const char *  ipc_name, unsigned short  count,
                    struct uv_export_t **  rv_exp)
{
    struct uv_export_t *    exp = calloc(1, sizeof(struct uv_export_t));

    *rv_exp = exp;

    exp->m_IpcName = ipc_name;
    exp->m_Handle = handle;
    exp->m_WorkerCount = count;
    exp->m_HandleType = handle->type;
    exp->m_Workers = calloc(1, sizeof(struct import_worker_t) * count);
    for (unsigned int i = 0; i < count; ++i) {
        struct import_worker_t *    worker = &exp->m_Workers[i];
        worker->m_Id = i + 1;
        worker->m_Exp = exp;
        uv_sem_init(&worker->m_Sem, 0);
    }

    // Here the pipe must be initialized with ipc == 0 because otherwise
    // listening on it is impossible
    int     e = uv_pipe_init(loop, &exp->m_Ipc, 0);
    if (e)
        goto error;

    e = uv_pipe_bind(&exp->m_Ipc, exp->m_IpcName);
    if (e)
        goto error;

    e = uv_listen((uv_stream_t*)&exp->m_Ipc, count * 2, s_on_ipc_connection);
    if (e)
        goto error;

    e = exp->m_Error;
    if (e)
        goto error;

    return 0;

error:
    uv_export_close(exp);
    *rv_exp = NULL;
    return e;
}


static int uv_export_wait(struct uv_export_t *  exp)
{
    if (exp->m_InWaiting)
        return UV_EBUSY;

    int         e = exp->m_Error;
    if (e)
        return e;

    exp->m_InWaiting = 1;

    for (unsigned int i = 0; i < exp->m_WorkerCount; ++i) {
        struct import_worker_t *    worker = &exp->m_Workers[i];
        uv_sem_post(&worker->m_Sem);
    }

    /* This loop will finish once all workers have connected
     * The listen pipe is closed by the last worker */
    while (1) {
        int     e = uv_run(exp->m_Ipc.loop, UV_RUN_NOWAIT);
        if (e == 0)
            break;

        unsigned int    finished_count = 0;
        for (unsigned int i = 0; i < exp->m_WorkerCount; ++i) {
            struct import_worker_t *    worker = &exp->m_Workers[i];
            if (worker->m_ImportFinished || worker->m_Error)
                finished_count++;
        }
        if (finished_count == exp->m_WorkerCount) {
            uv_close((uv_handle_t*)&exp->m_Ipc, NULL);
            uv_run(exp->m_Ipc.loop, UV_RUN_NOWAIT);
            exp->m_IpcClosed = 1;
            break;
        }
    }

    e = exp->m_Error;
    if (!e) {
        for (unsigned int i = 0; i < exp->m_WorkerCount; ++i) {
            struct import_worker_t *    worker = &exp->m_Workers[i];
            if (worker->m_Error) {
                e = worker->m_Error;
                break;
            }
        }
    }

    exp->m_InWaiting = 0;
    return e;
}


int uv_export_close(struct uv_export_t *  exp)
{
    if (exp->m_InWaiting)
        return UV_EBUSY;

    for (unsigned int i = 0; i < exp->m_WorkerCount; ++i) {
        struct import_worker_t *    worker = &exp->m_Workers[i];
        if (worker->m_ImportStarted)
            uv_sem_wait(&worker->m_Sem);
    }
    if (!exp->m_IpcClosed) {
        exp->m_IpcClosed = 1;
        uv_close((uv_handle_t*)&exp->m_Ipc, NULL);
    }
    if (exp->m_Workers) {
        for (unsigned int i = 0; i < exp->m_WorkerCount; ++i) {
            struct import_worker_t *    worker = &exp->m_Workers[i];
            uv_sem_destroy(&worker->m_Sem);
        }
        free(exp->m_Workers);
    }
    if (exp->m_IpcName)
        unlink(exp->m_IpcName);
    free(exp);
    return 0;
}


int uv_export_finish(struct uv_export_t *  exp)
{
    int         rc = uv_export_wait(exp);
    if (rc) {
        uv_export_close(exp);
        return rc;
    }
    rc = uv_export_close(exp);
    return rc;
}


/* uv_import related procedures and types */

void s_on_ipc_alloc(uv_handle_t *  handle, size_t  suggested_size,
                    uv_buf_t *  buf)
{
    struct import_worker_t *    worker = CONTAINER_OF(handle,
                                                      struct import_worker_t,
                                                      m_Ipc);
    buf->base = worker->m_StaticIpcBuf;
    buf->len = sizeof(worker->m_StaticIpcBuf);
}


void s_on_ipc_read(uv_stream_t *  handle, ssize_t  nread,
                   const uv_buf_t *  buf)
{
    assert(handle->type == UV_NAMED_PIPE);

    struct import_worker_t *    worker = CONTAINER_OF(handle,
                                                      struct import_worker_t,
                                                      m_Ipc);
    uv_pipe_t *         handle_pipe = (uv_pipe_t*)handle;
    assert(handle_pipe == &worker->m_Ipc);

    int                 pending_count = uv_pipe_pending_count(handle_pipe);
    if (pending_count != 1) {
        worker->m_Error = 1;
        return;
    }

    uv_handle_type type = uv_pipe_pending_type(handle_pipe);
    if (type != worker->m_Exp->m_HandleType) {
        worker->m_Error = 2;
        return;
    }

    int     e;
    switch (type) {
        case UV_TCP:
            e = uv_tcp_init(handle->loop, (uv_tcp_t*)worker->m_Handle);
            break;
        case UV_UDP:
            e = uv_udp_init(handle->loop, (uv_udp_t*)worker->m_Handle);
            break;
        case UV_NAMED_PIPE:
            e = uv_pipe_init(handle->loop, (uv_pipe_t*)worker->m_Handle, 1);
            break;
    default:
        worker->m_Error = 3;
        return;
    }

    if (e != 0) {
        uv_close((uv_handle_t*)(&worker->m_Handle), NULL);
        worker->m_Error = e;
        return;
    }

    e = uv_accept(handle, (uv_stream_t*)worker->m_Handle);
    if (e) {
        uv_close((uv_handle_t*)&worker->m_Handle, NULL);
        worker->m_Error = e;
        return;
    }

    /* closing the pipe will allow us to exit our loop */
    if (!worker->m_IpcClosed) {
        worker->m_IpcClosed = 1;
        uv_close((uv_handle_t*)&worker->m_Ipc, NULL);
    }
}


void s_on_ipc_connected(uv_connect_t *  req, int  status)
{
    struct import_worker_t *    worker = CONTAINER_OF(req,
                                                      struct import_worker_t,
                                                      m_ConnectReq);

    if (status != 0) {
        worker->m_Error = status;
        return;
    }

    int     e = uv_read_start((uv_stream_t*)&worker->m_Ipc, s_on_ipc_alloc,
                              s_on_ipc_read);
    if (e) {
        worker->m_Error = status;
        return;
    }
}


int uv_import(uv_loop_t *  loop, uv_stream_t *  handle,
              struct uv_export_t *  exp)
{
    if (!exp) {
        return 1;
    }

    unsigned int    id = __sync_fetch_and_add(&exp->m_IdCounter, 1);
    if (id < 0 || id >= exp->m_WorkerCount) {
        return 1;
    }

    struct import_worker_t *    worker = &exp->m_Workers[id];

    worker->m_Handle = handle;
    worker->m_ImportStarted = 1;
    uv_sem_wait(&worker->m_Sem);

    int         e;
    if (!exp->m_InWaiting) {
        worker->m_Error = 6;
        e = 1;
        goto done;
    }

    e = uv_pipe_init(loop, &worker->m_Ipc, 1);
    if (e) {
        worker->m_Error = e;
        goto done;
    }

    uv_pipe_connect(&worker->m_ConnectReq,
                    &worker->m_Ipc,
                    worker->m_Exp->m_IpcName,
                    s_on_ipc_connected);

    if (!worker->m_Error) {
        e = uv_run(loop, UV_RUN_DEFAULT);
        if (worker->m_Error)
            e = worker->m_Error;
    }

    if (!worker->m_IpcClosed) {
        worker->m_IpcClosed = 1;
        uv_close((uv_handle_t*)&worker->m_Ipc, NULL);
    }
    uv_run(loop, UV_RUN_DEFAULT); /* close handles */

    if (!e)
        worker->m_ImportFinished = 1;

done:
    uv_sem_post(&worker->m_Sem);
    return e;
}
