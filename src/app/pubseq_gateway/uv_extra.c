

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <uv.h>


#include <objtools/pubseq_gateway/impl/rpc/uv_extra.h>

#define CONTAINER_OF(ptr, type, member) ({                                                      \
    const typeof(((type*)(0))->member) *__mptr = ((const typeof(((type*)(0))->member) *)(ptr)); \
    (type*)((char*)(__mptr) - offsetof(type, member));                                          \
})

struct import_worker_t {
    unsigned int id;
    struct uv_export_t* exp;
    uv_sem_t sem;
    uv_pipe_t ipc;
    uv_connect_t connect_req;
    char static_ipc_buf[256];
    uv_stream_t *handle;
    volatile int error;
    char ipc_closed;
    volatile char import_started;
    volatile char import_finished;
};

struct uv_export_t {
    unsigned int id_counter;
    const char* ipc_name;
    uv_stream_t *handle;
    uv_pipe_t ipc;
    uv_handle_type handle_type;
    unsigned short worker_count;
    struct import_worker_t *workers;
    volatile int error;
    volatile int error_count;
    volatile char in_waiting;
    char ipc_closed;
};

/* uv_export related procedures and types */

struct ipc_peer_t {
    uv_pipe_t peer_handle;
    uv_write_t write_req;
    struct uv_export_t* exp;
};

static void s_on_peer_close(uv_handle_t* handle) {
    struct ipc_peer_t *pc = CONTAINER_OF(handle, struct ipc_peer_t, peer_handle);
    free(pc);
}

static void s_on_ipc_write(uv_write_t* req, int status) {
    struct ipc_peer_t* pc = CONTAINER_OF(req, struct ipc_peer_t, write_req);
    if (status) {
        ++pc->exp->error_count;
        pc->exp->error = status;
    }
    uv_close((uv_handle_t*)&pc->peer_handle, s_on_peer_close);
}

static void s_on_ipc_connection(uv_stream_t* ipc, int status) {
    int e;
    char msg[] = "PING";
    struct uv_export_t* exp = CONTAINER_OF(ipc, struct uv_export_t, ipc);

    if (status) {
        ++exp->error_count;
        exp->error = status;
        return;
    }

    uv_buf_t buf = uv_buf_init(msg, sizeof(msg) - 1);

    struct ipc_peer_t* pc = calloc(1, sizeof(*pc));
    pc->exp = exp;

    assert(ipc->type == UV_NAMED_PIPE);

    e = uv_pipe_init(ipc->loop, (uv_pipe_t*)&pc->peer_handle, 1);
    if (e != 0) {
        free(pc);
        ++exp->error_count;
        exp->error = e;
        return;
    }

    do {
        e = uv_accept(ipc, (uv_stream_t*)&pc->peer_handle);
        if (e == 0)
            break;
        else if (-e != EAGAIN) {
            uv_close((uv_handle_t*)&pc->peer_handle, s_on_peer_close);
            ++exp->error_count;
            exp->error = e;
            return;
        }
    } while (1);
    
    /* send the handle */
    e = uv_write2(&pc->write_req,
                  (uv_stream_t*)&pc->peer_handle,
                  &buf, 1,
                  exp->handle, s_on_ipc_write);
    if (e != 0) {
        uv_close((uv_handle_t*)&pc->peer_handle, s_on_peer_close);
        ++exp->error_count;
        exp->error = e;
        return;
    }
}

static int uv_export_close(struct uv_export_t *exp);

int uv_export_start(uv_loop_t *loop, uv_stream_t *handle, const char* ipc_name, unsigned short count, struct uv_export_t **rv_exp) {
    int e = 0;
    struct uv_export_t *exp = calloc(1, sizeof(struct uv_export_t));
    *rv_exp = exp;
    
    exp->ipc_name = ipc_name;
    exp->handle = handle;
    exp->worker_count = count;
    exp->handle_type = handle->type;
    exp->workers = calloc(1, sizeof(struct import_worker_t) * count);
    for (unsigned int i = 0; i < count; ++i) {
        struct import_worker_t *worker = &exp->workers[i];
        worker->id = i + 1;
        worker->exp = exp;
        uv_sem_init(&worker->sem, 0);
    }

    e = uv_pipe_init(loop, &exp->ipc, 1);
    if (e) goto error;

    e = uv_pipe_bind(&exp->ipc, exp->ipc_name);
    if (e) goto error;

    e = uv_listen((uv_stream_t*)&exp->ipc, count * 2, s_on_ipc_connection);
    if (e) goto error;

    e = exp->error;
    if (e) goto error;

    return 0;
error:
    uv_export_close(exp);
    *rv_exp = NULL;
    return e;
}

static int uv_export_wait(struct uv_export_t *exp) {
    int e = 0;
    if (exp->in_waiting)
        return UV_EBUSY;

    e = exp->error;
    if (e)
        return e;

    exp->in_waiting = 1;

    for (unsigned int i = 0; i < exp->worker_count; ++i) {
        struct import_worker_t *worker = &exp->workers[i];
        uv_sem_post(&worker->sem);
    }

    /* This loop will finish once all workers have connected
     * The listen pipe is closed by the last worker */
    while (1) {
        int e = uv_run(exp->ipc.loop, UV_RUN_NOWAIT);
        if (e == 0)
            break;
        unsigned int finished_count = 0;
        for (unsigned int i = 0; i < exp->worker_count; ++i) {
            struct import_worker_t *worker = &exp->workers[i];
            if (worker->import_finished || worker->error)
                finished_count++;
        }
        if (finished_count == exp->worker_count) {
            uv_close((uv_handle_t*)&exp->ipc, NULL);
            uv_run(exp->ipc.loop, UV_RUN_NOWAIT);
            exp->ipc_closed = 1;
            break;
        }
    }

    e = exp->error;
    if (!e) {
        for (unsigned int i = 0; i < exp->worker_count; ++i) {
            struct import_worker_t *worker = &exp->workers[i];
            if (worker->error) {
                e = worker->error;
                break;
            }
        }
    }

    exp->in_waiting = 0;
    return e;
}

static int uv_export_close(struct uv_export_t *exp) {
    if (exp->in_waiting)
        return UV_EBUSY;

    for (unsigned int i = 0; i < exp->worker_count; ++i) {
        struct import_worker_t *worker = &exp->workers[i];
        if (worker->import_started)
            uv_sem_wait(&worker->sem);
    }
    if (!exp->ipc_closed) {
        exp->ipc_closed = 1;
        uv_close((uv_handle_t*)&exp->ipc, NULL);
    }
    if (exp->workers) {
        for (unsigned int i = 0; i < exp->worker_count; ++i) {
            struct import_worker_t *worker = &exp->workers[i];
            uv_sem_destroy(&worker->sem);
        }
        free(exp->workers);
    }
    if (exp->ipc_name)
        unlink(exp->ipc_name);
    free(exp);
    return 0;
}

int uv_export_finish(struct uv_export_t *exp) {
    int rc = uv_export_wait(exp);
    if (rc) {
        uv_export_close(exp);
        return rc;
    }
    rc = uv_export_close(exp);
    return rc;
}


/* uv_import related procedures and types */

void s_on_ipc_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    struct import_worker_t *worker = CONTAINER_OF(handle, struct import_worker_t, ipc);
    buf->base = worker->static_ipc_buf;
    buf->len = sizeof(worker->static_ipc_buf);
}

void s_on_ipc_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    int e;
    assert(handle->type == UV_NAMED_PIPE);
    struct import_worker_t *worker = CONTAINER_OF(handle, struct import_worker_t, ipc);
    uv_pipe_t *handle_pipe = (uv_pipe_t*)handle;
    assert(handle_pipe == &worker->ipc);

    int pending_count = uv_pipe_pending_count(handle_pipe);
    if (pending_count != 1) {
        worker->error = 1;
        return;
    }

    uv_handle_type type = uv_pipe_pending_type(handle_pipe);
    if (type != worker->exp->handle_type) {
        worker->error = 2;
        return;
    }

    switch (type) {
        case UV_TCP:
            e = uv_tcp_init(handle->loop, (uv_tcp_t*)worker->handle);
            break;
        case UV_UDP:
            e = uv_udp_init(handle->loop, (uv_udp_t*)worker->handle);
            break;
        case UV_NAMED_PIPE:
            e = uv_pipe_init(handle->loop, (uv_pipe_t*)worker->handle, 1);
            break;
    default:
        worker->error = 3;
        return;
    }
    
    if (e != 0) {
        uv_close((uv_handle_t*)(&worker->handle), NULL);
        worker->error = e;
        return;
    }

    e = uv_accept(handle, (uv_stream_t*)worker->handle);
    if (e) {
        uv_close((uv_handle_t*)&worker->handle, NULL);
        worker->error = e;
        return;
    }

    /* closing the pipe will allow us to exit our loop */
    if (!worker->ipc_closed) {
        worker->ipc_closed = 1;
        uv_close((uv_handle_t*)&worker->ipc, NULL);
    }
}

void s_on_ipc_connected(uv_connect_t* req, int status) {
    int e;
    struct import_worker_t *worker = CONTAINER_OF(req, struct import_worker_t, connect_req);

    if (status != 0) {
        worker->error = status;
        return;
    }

    e = uv_read_start((uv_stream_t*)&worker->ipc, s_on_ipc_alloc, s_on_ipc_read);
    if (e) {
        worker->error = status;
        return;
    }
}

int uv_import(uv_loop_t *loop, uv_stream_t *handle, struct uv_export_t *exp) {
    int e;
    if (!exp) {
        return 1;
    }

    unsigned int id = __sync_fetch_and_add(&exp->id_counter, 1);
    if (id < 0 || id >= exp->worker_count) {
        return 1;
    }

    struct import_worker_t *worker = &exp->workers[id];

    worker->handle = handle;
    worker->import_started = 1;
    uv_sem_wait(&worker->sem);

    if (!exp->in_waiting) {
        worker->error = 6;
        e = 1;
        goto done;
    }

    e = uv_pipe_init(loop, &worker->ipc, 1);
    if (e) {
        worker->error = e;        
        goto done;
    }

    uv_pipe_connect(&worker->connect_req,
                    &worker->ipc,
                    worker->exp->ipc_name,
                    s_on_ipc_connected);

    if (!worker->error) {
        e = uv_run(loop, UV_RUN_DEFAULT);
        if (worker->error)
            e = worker->error;
    }

    if (!worker->ipc_closed) {
        worker->ipc_closed = 1;
        uv_close((uv_handle_t*)&worker->ipc, NULL);
    }
    uv_run(loop, UV_RUN_DEFAULT); /* close handles */

    if (!e)
        worker->import_finished = 1;

done:
    uv_sem_post(&worker->sem);
    return e;
}
