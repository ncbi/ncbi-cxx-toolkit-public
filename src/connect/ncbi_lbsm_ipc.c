/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Implementation of the LBSM client-server data exchange API
 *   with the use of SYSV IPC
 *
 *   UNIX only !!!
 *
 */

#include "ncbi_lbsm_ipc.h"
#include "ncbi_once.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM


int g_LBSM_NOSYSVIPC = 0;

static int         s_Muxid        =  -1;
static int         s_Shmid[2]     = {-1, -1};
static void*       s_Shmem[2]     = { 0,  0};
static TNCBI_Size  s_ShmemSize[2] = { 0,  0};
static const int   k_ShmemKey [2] = {LBSM_SHMEM_KEY_1, LBSM_SHMEM_KEY_2};
static int/*bool*/ s_SemUndo  [4] = {0,0,0,0}; /* for semnums 1, 2, 3, and 4 */


/* We have 5 semaphores in the array for the shared memory access control:
 * semnum
 *    0      0 when the daemon is not running, 1 when it is;
 *    1      0 when the 1st shmem segment is not pre-locked for writing, 1 when
 *               it is;  however, update happens only when semnum(2) reaches 0;
 *    2      number of current shared memory accessors, i.e. those passed via
 *               semnum(1) (and perhaps semnum(2));
 *    3      same as 1 and 2 but for the 2nd copy of the shmem segment.
 *    4
 * All readers are blocked by the semnum[1] > 0.
 * Writers always wait for the semnum[2] == 0 before proceeding.
 * [1] means access to the semnum 1 or 3 (1st sem in the block);
 * [2] means access to the semnum 2 or 4 (2nd sem in the block).
 */


/* Non-blocking decrement of a specified [sem] */
static int/*syscall*/ s_Shmem_Unlock(int which, int sem)
{
    unsigned short semnum = (unsigned short)((which << 1) + sem);
    struct sembuf unlock;

    unlock.sem_num =  semnum;
    unlock.sem_op  = -1;
    unlock.sem_flg =  IPC_NOWAIT | (s_SemUndo[semnum - 1] ? SEM_UNDO : 0);
    return semop(s_Muxid, &unlock, 1) != 0  &&  errno != EAGAIN ? -1 : 0;
}


/* For 'which' block: check specified [sem] first, then continue with [2]++ */
static int/*syscall*/ s_Shmem_Lock(int which, int sem, unsigned int wait)
{
    unsigned short semnum = (unsigned short)((which << 1) + sem);
    unsigned short accsem = (unsigned short)((which << 1) + 2);
    int/*bool*/ undo = 1/*true*/;
    struct sembuf lock[2];
    int error = 0;

    for (;;) {
        lock[0].sem_num = semnum;
        lock[0].sem_op  = 0; /* precondition:  [sem] == 0 */
        lock[0].sem_flg = wait ? 0 : IPC_NOWAIT;
        lock[1].sem_num = accsem;
        lock[1].sem_op  = 1; /* postcondition: [2]++      */
        lock[1].sem_flg = undo ? SEM_UNDO : 0;

        if (semop(s_Muxid, lock, sizeof(lock)/sizeof(lock[0])) == 0) {
            s_SemUndo[accsem - 1] = undo;
            return 0;
        }
        if (error)
            break;
        error = errno;
        if (error == ENOSPC) {
            CORE_LOGF_X(7, eLOG_Warning,
                        ("LBSM %c-locking[%d] w/o undo",
                         "RW"[sem > 1], which + 1));
            undo = 0/*false*/;
            continue;
        }
        if (error == EINTR)
            continue;
        if (!wait  ||  error != ENOMEM)
            break;
#ifdef LBSM_DEBUG
        CORE_LOGF(eLOG_Trace,
                  ("LBSM %c-locking[%d] wait(ENOMEM)",
                   "RW"[sem > 1], which + 1));
#endif /*LBSM_DEBUG*/
        CORE_Msdelay(1000);
    }
    return -1;
}


static int/*bool*/ s_Shmem_RUnlock(int which)
{
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM R-lock[%d] release", which + 1));
#endif /*LBSM_DEBUG*/
    /* Decrement number of accessors, [2] */
    return s_Shmem_Unlock(which, 2) == 0;
}


/* Return locked block number {0, 1}, or -1 on error */
static int/*tri-state*/ s_Shmem_RLock(int which)
{
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM R-lock acquire (%d)", which + 1));
#endif /*LBSM_DEBUG*/
    /* Try block 1 then block 0: check [1] first, then continue with [2]++ */
    if (s_Shmem_Lock(which, 1, !which/*1:no-wait,0:wait*/) == 0)
        return which/*lock acquired successfully*/;
    return !which  ||  errno == EINVAL ? -1 : s_Shmem_Lock(0, 1, 1/*wait*/);
}


/* Return bitmask of sems _failed_ to unlock */
static int s_Shmem_WUnlock(int which)
{
#ifndef NCBI_OS_BSD
    /* More robust, it seems? */
    static const union semun arg = { 0 };
    int semnum  = (which << 1) | 1;
    /* Clear the number of accessors [2] */
    int retval  = semctl(s_Muxid, ++semnum, SETVAL, arg) < 0 ? 2 : 0;
    /* Open read semaphore [1] by resetting it to 0 */
        retval |= semctl(s_Muxid, --semnum, SETVAL, arg) < 0 ? 1 : 0;
#else
    /* FreeBSD bug 173724 prevents from using SETVAL safely */
    /* Decrement number of accessors [2]; should become 0 after that */
    int retval  = s_Shmem_Unlock(which, 2) == 0 ? 0 : 2;
    /* Open read semaphore [1] */
        retval |= s_Shmem_Unlock(which, 1) == 0 ? 0 : 1;
#endif /*!NCBI_OS_BSD*/
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM W-lock[%d] release (%d)", which + 1, retval));
#endif /*LBSM_DEBUG*/
    return retval;
}


static int/*tri-state-bool,inverted*/ s_Shmem_TryWLock(int which)
{
    unsigned short semnum = (unsigned short)((which << 1) | 1);
    int/*bool*/ undo = 1/*true*/;
    struct sembuf rwlock[2];
    int error = 0;

    for (;;) {
        rwlock[0].sem_num = semnum;
        rwlock[0].sem_op  = 0; /* precondition:  [1] == 0 */
        rwlock[0].sem_flg = 0; /* NB: waited!             */
        rwlock[1].sem_num = semnum;
        rwlock[1].sem_op  = 1; /* postcondition: [1] == 1 */
        rwlock[1].sem_flg = undo ? SEM_UNDO : 0;

        if (semop(s_Muxid, rwlock, sizeof(rwlock)/sizeof(rwlock[0])) == 0) {
            /* No new read accesses are allowed after this point */
            s_SemUndo[semnum - 1] = undo;
            /* Check [2] first (readers gone), then continue with it ([2]++) */
            return s_Shmem_Lock(which, 2, 1/*wait*/)
                ? 1/*partially locked*/ : 0/*success*/;
        }
        if (error)
            break;
        error = errno;
        if (error == ENOSPC) {
            CORE_LOGF_X(8, eLOG_Warning,
                        ("LBSM PreW-locking[%d] w/o undo", which + 1));
            undo = 0/*false*/;
            continue;
        }
        if (error == EINTR)
            continue;
        if (error != ENOMEM)
            break;
#ifdef LBSM_DEBUG
        CORE_LOGF(eLOG_Trace,
                  ("LBSM PreW-lock[%d] wait(ENOMEM)", which + 1));
#endif/*LBSM_DEBUG*/
        CORE_Msdelay(1000);
    }
    return -1/*not locked*/;
}


/* Return 0 if successful;  return a negative value on a severe failure (e.g.
 * a removed IPC id), return a positive value on a lock failure (no lock
 * acquired).  If "wait" passed non-zero, do not attempt to assassinate a
 * contender (still, try to identify and print its PID anyways).  Otherwise,
 * having killed the contender, try to reset and then to re-acquire the lock.
 */
static int/*tri-state-bool,inverted*/s_Shmem_WLock(int which, int/*bool*/ wait)
{
    static union semun dummy;
    int locked;
    int error;
    int pid;
    int rv;

#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace,
              ("LBSM W-lock[%d] acquire%s", which + 1, wait ? " w/wait" : ""));
#endif /*LBSM_DEBUG*/

    if ((locked = s_Shmem_TryWLock(which)) == 0)
        return 0/*success*/;

    if (locked < 0) {
        /* even [1] was not successfully acquired, so we have either
         *   a/ IPC id removed, or
         *   b/ a hanging writer, or
         *   c/ a hanging old reader (which doesn't change [2], and only uses
         *      block 0 -- there was no block 1 at that time).
         * In the latter two cases, we can try to obtain PID of the offender.
         */
        if (errno == EIDRM)
            return -1/*bad*/;
        if ((pid = semctl(s_Muxid, (which << 1) | 1, GETPID, dummy)) > 0) {
            int self   = (pid_t) pid == getpid();
            int other  = !self  &&  (kill(pid, 0) == 0  ||  errno == EPERM);
            int killed = 0/*false*/;
            if (!wait) {
                if (other  &&  kill(pid, SIGTERM) == 0) {
                    CORE_LOGF_X(17, eLOG_Warning,
                                ("Terminating PID %lu", (long) pid));
                    CORE_Msdelay(1000); /*let'em do SIGTERM + exit gracefully*/
                    kill(pid, SIGKILL);
                    killed = 1/*true*/;
                } else if (!self) {
                    CORE_LOGF_ERRNO_X(18, eLOG_Warning,
                                      other ? errno : 0,
                                      ("Unable to kill PID %lu", (long) pid));
                }
            }
            CORE_LOGF_X(19, eLOG_Warning,
                        ("LBSM lock[%d] %s revoked from PID %lu (%s)",
                         which + 1, killed || !wait ? "is being" : "has to be",
                         (long) pid, self  ? "self" :
                         other ? (killed ? "killed" : "hanging") : "zombie?"));
        } else if (pid < 0) {
            return errno == EFBIG  ||  semget(LBSM_MUTEX_KEY, 0, 0) != s_Muxid
                ? -1/*bad: most likely invalid IPC id*/
                :  1/*failure*/;
        }
        locked = 0/*false*/;
    } else
        pid = 0;

    rv = 1/*failure*/;
    if (!pid) {
        char num[32];
        int  val = 0;
        if (locked
            &&  (val = semctl(s_Muxid, (which << 1) + 2, GETVAL, dummy)) > 1) {
            sprintf(num, "%d", val);
        } else
            strcpy(num, "a");
        if (val != -1) {
            CORE_LOGF_X(20, eLOG_Warning,
                        ("LBSM shmem[%d] has %s stuck %s%s", which + 1, num,
                         !locked ? "process" : val > 1 ? "readers" : "reader",
                         wait ? "" : ", revoking lock"));
        } else
            rv = -1/*bad*/;
    }

    if (locked) {
        /* [1] had been free (now locked) but [2] was taken by someone else:
         * we have a hanging reader, yet no additional info can be obtained.
         */
        if (rv < 0)
            wait = 1/*this makes us undo [1] and fail later*/;
        if (!wait) {
            union semun arg;
            arg.val = 1;
            /* adjust [2] to reflect the number of accessors: 1 */
            if (semctl(s_Muxid, (which << 1) + 2, SETVAL, arg) < 0) {
                error = errno;
                CORE_LOGF_ERRNO_X(9, eLOG_Error, error,
                                  ("LBSM access count[%d] failed to adjust",
                                   which + 1));
                wait = 1/*-"-*/;
                rv = -1/*bad*/;
                errno = error;
            }
        }
        if (wait) {
            error = errno;
            (void) s_Shmem_Unlock(which, 1);
            errno = error;
        }
    }

    if (wait)
        return rv/*failure*/;

    if (locked)
        return 0/*success*/;

    if ((wait = s_Shmem_WUnlock(which)) != 0) {
        error = errno;
        CORE_LOGF_ERRNO_X(23, eLOG_Error, error,
                          ("LBSM lock[%d] failed to reset, code %d",
                           which + 1, wait));
        errno = error;
        return -1/*bad*/;
    }

#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace,
              ("LBSM W-lock[%d] re-acquire", which + 1));
#endif /*LBSM_DEBUG*/

    if ((locked = s_Shmem_TryWLock(which)) == 0)
        return 0/*success*/;

    if (!wait) {
        error = errno;
        CORE_LOGF_ERRNO_X(24, eLOG_Critical, error,
                          ("LBSM lock[%d] cannot be re-acquired, code %d",
                           which + 1, locked));
        errno = error;
    }
    return 1/*failure*/;
}


static HEAP s_Shmem_Attach(int which)
{
    void* shmem = 0; /*dummy init for compiler not to complain*/
    int   shmid;
    HEAP  heap;

#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM shmem[%d] attaching", which + 1));
#endif /*LBSM_DEBUG*/
    if ((shmid = shmget(k_ShmemKey[which], 0, 0)) != -1  &&
        (shmid == s_Shmid[which]  ||
         ((shmem = shmat(shmid, 0, SHM_RDONLY))  &&  shmem != (void*)(-1L)))) {
        if (shmid != s_Shmid[which]) {
            struct shmid_ds shm_ds;
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace, ("LBSM shmem[%d] attached", which + 1));
#endif /*LBSM_DEBUG*/
            s_Shmid[which] = shmid;
            if (s_Shmem[which])
                shmdt(s_Shmem[which]);
            s_Shmem[which] = shmem;
            s_ShmemSize[which]
                = (TNCBI_Size)(shmctl(shmid, IPC_STAT, &shm_ds) != -1
                               ? shm_ds.shm_segsz : 0);
        }
        heap = s_ShmemSize[which]
            ? HEAP_AttachFast(s_Shmem[which], s_ShmemSize[which], which + 1)
            : HEAP_Attach    (s_Shmem[which], 0,                  which + 1);
    } else
        heap = 0;

    return heap;
}


HEAP LBSM_Shmem_Attach(int/*bool*/ fallback)
{
    int  which;
    HEAP heap;

#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM ATTACHING%s", fallback ? " (fallback)" : ""));
#endif /*LBSM_DEBUG*/
    if ((which = s_Shmem_RLock(!fallback)) < 0) {
        CORE_LOG_ERRNO_X(10, eLOG_Warning, errno,
                         "LBSM shmem failed to lock for attachment");
        return 0;
    }
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM R-lock[%d] acquired", which + 1));
#endif /*LBSM_DEBUG*/
    if (!(heap = s_Shmem_Attach(which))) {
        int error = errno;
        s_Shmem_RUnlock(which);
        CORE_LOGF_ERRNO_X(11, eLOG_Error, error,
                          ("LBSM shmem[%d] %s", which + 1, s_Shmem[which]
                           ? "access failed" : "failed to attach"));
    }
#ifdef LBSM_DEBUG
    else {
        CORE_LOGF(eLOG_Trace,
                  ("LBSM heap[%p, %p, %d] attached",
                   heap, HEAP_Base(heap), which + 1));
        assert(HEAP_Serial(heap) == which + 1);
    }
#endif /*LBSM_DEBUG*/
    if (s_Shmem[which = !which]) {
#ifdef LBSM_DEBUG
        CORE_LOGF(eLOG_Trace, ("LBSM shmem[%d] detached", which + 1));
#endif /*LBSM_DEBUG*/
        shmdt(s_Shmem[which]);
        s_Shmem[which] =  0;
        s_Shmid[which] = -1;
    } else
        assert(s_Shmid[which] < 0);
    s_ShmemSize[which] =  0;
    return heap;
}


void LBSM_Shmem_Detach(HEAP heap)
{
    int which;

    assert(heap);
    which = HEAP_Serial(heap);
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM shmem[%d] detaching", which));
#endif /*LBSM_DEBUG*/
    if (which != 1  &&  which != 2) {
        CORE_LOGF_X(12, eLOG_Critical,
                    ("LBSM shmem[%d?] cannot detach", which));
    } else
        s_Shmem_RUnlock(which - 1);
    HEAP_Detach(heap);
#ifdef LBSM_DEBUG
    CORE_LOG(eLOG_Trace, "LBSM DETACHED");
#endif /*LBSM_DEBUG*/
}


#ifdef __cplusplus
extern "C" {
    static void* s_LBSM_ResizeHeap(void*, TNCBI_Size, void*);
}
#endif

/*ARGSUSED*/
static void* s_LBSM_ResizeHeap(void* mem, TNCBI_Size newsize, void* unused)
{
#ifdef LBSM_DEBUG
    CORE_LOGF(eLOG_Trace, ("LBSM Heap resize(%p, %u)", mem, newsize));
#endif /*LBSM_DEBUG*/
    if (mem  &&  newsize)
        return realloc(mem, newsize);
    if (newsize) /* mem     == 0 */
        return malloc(newsize);
    if (mem)     /* newsize == 0 */
        free(mem);
    return 0;
}


HEAP LBSM_Shmem_Create(TNCBI_Size pagesize)
{
    if (!g_LBSM_NOSYSVIPC) {
        int/*bool*/ one = 0/*false*/, two = 0/*false*/;
        size_t i;

        for (i = 0;  i < 2;  ++i)
            s_Shmid[i] = shmget(k_ShmemKey[i], 0, 0);
        if ((one = (s_Shmid[0] != -1)) | (two = (s_Shmid[1] != -1))) {
            CORE_LOGF_X(13, eLOG_Warning,
                        ("Re-creating existing LBSM shmem segment%s %s%s%s",
                         one ^ two ? ""    : "s",
                         one       ? "[1]" : "",
                         one ^ two ? ""    : " and ",
                         two       ? "[2]" : ""));
            if (!LBSM_Shmem_Destroy(0))
                return 0;
        }
    }
    if (!pagesize  &&  !(pagesize = (TNCBI_Size) CORE_GetVMPageSize()))
        pagesize = LBSM_DEFAULT_PAGE_SIZE;
    else if (pagesize < LBSM_DEFAULT_PAGE_SIZE)
        pagesize = LBSM_DEFAULT_PAGE_SIZE;
    return HEAP_Create(0, 0, pagesize, s_LBSM_ResizeHeap, 0);
}


static int/*bool*/ s_Shmem_Destroy(int which, pid_t own_pid)
{
    struct shmid_ds shm_ds;
    int error, /*bool*/ rv;

    if (s_Shmid[which] < 0) {
        assert(!s_Shmem[which]  &&  !s_ShmemSize[which]);
        return -1/*NOP*/;
    }
    rv = 1/*true*/;
    if (own_pid) {
        if (shmctl(s_Shmid[which], IPC_STAT, &shm_ds) == -1)
            memset(&shm_ds, 0, sizeof(shm_ds));
        if (own_pid != (pid_t)(-1)  &&  own_pid != shm_ds.shm_cpid) {
            error = errno;
            if (shm_ds.shm_cpid) {
                CORE_LOGF_X(15, eLOG_Error,
                            ("LBSM shmem[%d] not an owner (%lu) to remove",
                             which + 1, (long) shm_ds.shm_cpid));
            } else {
                CORE_LOGF_ERRNO_X(25, eLOG_Error, error,
                                  ("LBSM shmem[%d] unable to stat",which + 1));
            }
            rv = 0/*false*/;
            errno = error;
            own_pid = 0;
        }
    } else
        own_pid = 1;
    if (s_Shmem[which]) {
        if (shmdt(s_Shmem[which]) < 0) {
            error = errno;
            CORE_LOGF_ERRNO_X(14, eLOG_Error, error,
                              ("LBSM shmem[%d] unable to detach", which + 1));
            errno = error;
        }
        s_Shmem[which] = 0;
    }
    if (own_pid  &&  shmctl(s_Shmid[which], IPC_RMID, 0) == -1) {
        error = errno;
        if (error != EINVAL  ||  own_pid != (pid_t)(-1)
#ifdef SHM_DEST
            ||  !(shm_ds.shm_perm.mode & SHM_DEST)
#endif /*SHM_DEST*/
            ) {
            CORE_LOGF_ERRNO_X(16, eLOG_Error, error,
                              ("LBSM shmem[%d] unable to remove", which + 1));
            rv = 0/*false*/;
            errno = error;
        }
    }
    s_Shmid[which]     = -1;
    s_ShmemSize[which] =  0;
    return rv;
}


int/*bool*/ LBSM_Shmem_Destroy(HEAP heap)
{
    int/*bool*/ rv = 1/*true*/;
    if (!g_LBSM_NOSYSVIPC) {
        /* Careful with a live heap detach! */
        pid_t self = heap ? getpid() : 0;
        int i;
        for (i = 0;  i < 2;  ++i) {
            if (!s_Shmem_Destroy(i, self))
                rv = 0/*false*/;
        }
    }
    HEAP_Destroy(heap);
    return rv;
}


int LBSM_Shmem_Update(HEAP heap, int/*bool*/ wait)
{
    size_t heapsize = HEAP_Size(heap);
    void*  heapbase = HEAP_Base(heap);
    int    updated = 0, i;

    assert(heapbase  &&  heapsize);
    if (g_LBSM_NOSYSVIPC)
        return 3;
    for (i = 0;  i < 2;  ++i) {
        pid_t pid;
        int   temp = wait  &&  s_ShmemSize[i]  &&  (!i  ||  updated);
        int   code = s_Shmem_WLock(i, temp);

        if (unlikely(code)) {
            if (code < 0)
                return code;
            continue;
        }

        pid = 0;
        code = 1 << i; /* NB: code == i + 1 for i in [0..1] */
        /* Update shmem here: strict checks for the first time */
        if (s_ShmemSize[i] != heapsize
            ||  s_Shmid[i] != shmget(k_ShmemKey[i], 0, 0)) {
            int   shmid;
            void* shmem;
            if (!s_ShmemSize[i])
                pid = getpid();
            else if (s_ShmemSize[i] == heapsize) {
                pid = (pid_t)(-1);
                CORE_LOGF_X(2, eLOG_Warning,
                            ("LBSM shmem[%d] tainted, re-creating", i + 1));
            }
            if (!s_Shmem_Destroy(i, pid)
                ||  (shmid = shmget(k_ShmemKey[i], heapsize,
                                    LBSM_SHM_PROT | IPC_CREAT | IPC_EXCL))== -1
                ||  !(shmem = shmat(shmid, 0, 0))  ||  shmem == (void*)(-1L)) {
                CORE_LOGF_ERRNO_X(22, pid ? eLOG_Error : eLOG_Warning, errno,
                                  ("LBSM shmem[%d] unable to re-create", i+1));
                assert(s_Shmid[i] < 0  &&  !s_Shmem[i]  &&  !s_ShmemSize[i]);
                code = 0;
            } else {
                s_Shmid[i] = shmid;
                s_Shmem[i] = shmem;
                s_ShmemSize[i] = (TNCBI_Size) heapsize;
            }
        }
        if (code) {
#ifdef LBSM_DEBUG
            CORE_LOGF(eLOG_Trace, ("Updating [%d]", i + 1));
#endif /*LBSM_DEBUG*/
            memcpy(s_Shmem[i], heapbase, heapsize);
            updated |= code;
        }
        if ((temp = s_Shmem_WUnlock(i)) != 0) {
            CORE_LOGF_ERRNO_X(21, eLOG_Warning, errno,
                              ("LBSM shmem[%d] failed to unlock, code %d",
                               i + 1, temp));
        }
        if (!code  &&  pid  &&  pid != (pid_t)(-1))
            return 0/*initial update failed*/;
    }
    return updated;
}


int LBSM_LBSMD(int/*bool*/ check_n_lock)
{
    struct sembuf lock[2];
    int id;

    if (g_LBSM_NOSYSVIPC)
        return check_n_lock ? 0 : -1;

#if defined(NCBI_OS_CYGWIN)  &&  defined(SIGSYS)
    {{
        static void* /*bool*/ s_SigSysInit = 0/*false*/;
        if (CORE_Once(&s_SigSysInit))
            signal(SIGSYS, SIG_IGN); /*ENOSYS won't kill*/
    }}
#endif /*NCBI_OS_CYGWIN && SIGSYS*/

    if ((id = semget(LBSM_MUTEX_KEY, check_n_lock ? 5 : 0,
                     check_n_lock ? (IPC_CREAT | LBSM_SEM_PROT) : 0)) == -1) {
        return -1;
    }

    s_Muxid = id;
    /* Check & lock daemon presence: done atomically */
    lock[0].sem_num = 0;
    lock[0].sem_op  = 0;
    lock[0].sem_flg = IPC_NOWAIT;
    lock[1].sem_num = 0;
    lock[1].sem_op  = 1;
    lock[1].sem_flg = SEM_UNDO;
    if (semop(id, lock,
              sizeof(lock)/sizeof(lock[0]) - (check_n_lock ? 0 : 1)) != 0) {
        return  1;
    }
    return 0;
}


/* Daemon use: undaemon > 0;  Client use: undaemon == 0 => return LBSMD PID;
 *                                        undaemon <  0 => faster return    */
pid_t LBSM_UnLBSMD(int/*bool*/ undaemon)
{
    pid_t pid = 0;

    if (!g_LBSM_NOSYSVIPC  &&  s_Muxid >= 0) {
        static union semun dummy;
        if (undaemon <= 0) {
            int which;
            if (undaemon == 0) {
                which = 1;
                while ((which = s_Shmem_RLock(which)) >= 0) {
                    int shmid = shmget(k_ShmemKey[which], 0, 0);
                    struct shmid_ds shm_ds;
                    if (shmid != -1 && shmctl(shmid, IPC_STAT, &shm_ds) != -1)
                        pid = shm_ds.shm_cpid;
                    s_Shmem_RUnlock(which);
                    if ((int) pid < 0)
                        pid = 0;
                    if (pid  ||  !which)
                        break;
                    which = 0;
                }
                if (!pid) {
                    int retval = semctl(s_Muxid, 0, GETPID, dummy);
                    if (retval >= 0)
                        pid = (pid_t) retval;
                }
            }
            CORE_LOCK_WRITE;
            for (which = 0;  which < 2;  ++which) {
                if (s_Shmem[which]) {
#ifdef LBSM_DEBUG
                    CORE_LOGF(eLOG_Trace, ("LBSM shmem[%d] detached",which+1));
#endif /*LBSM_DEBUG*/
                    shmdt(s_Shmem[which]);
                    s_Shmem[which] =  0;
                    s_Shmid[which] = -1;
                } else
                    assert(s_Shmid[which] < 0);
                s_ShmemSize[which] =  0;
            }
            CORE_UNLOCK;
        } else {
            semctl(s_Muxid, 0, IPC_RMID, dummy);
            s_Muxid = -1;
        }
    }

    assert((int) pid >= 0);
    return pid;
}
