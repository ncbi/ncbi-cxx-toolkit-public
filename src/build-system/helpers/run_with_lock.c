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
* Author:  Aaron Ucko
*
* File Description:
*   Run a portion of the build under a filesystem-based lock,
*   optionally logging any resulting output (both stdout and stderr).
*
* ===========================================================================
*/


#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <assert.h>

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

#ifndef W_EXITCODE
#  define W_EXITCODE(x, y) (((x) << 8) | (y))
#endif

static const char* s_AppName;
static char        s_LockName[PATH_MAX];
static int         s_MainChildDone;
static int         s_FinishingInBackground;
static int         s_CaughtSignal;
static sig_t       s_OrigHandlers[NSIG + 1];


typedef struct {
    const char* base;
    const char* getter;
    const char* logfile;
    const char* map;
    const char* reviewer;
    int         error_status;
} SOptions;

typedef enum {
    eFS_off,
    eFS_on,
    eFS_esc,
    eFS_csi,
} EFilterState;


static
void s_ApplyMap(SOptions* options)
{
    char line[1024];
    FILE* map_file;

    assert(options->map != NULL);
    map_file = fopen(options->map, "r");
    if (map_file == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr,
                    "%s: Unable to open lock map %s: %s; leaving base as %s.\n",
                    s_AppName, options->map, strerror(errno), options->base);
        }
        return;
    }
    while (fgets(line, sizeof(line), map_file) != NULL) {
        char key[1024], value[1024];
        if (sscanf(line, " %s %s", key, value) == 2
            &&  strcmp(key, options->base + 5) == 0) {
            char *new_base = malloc(strlen(value) + 6);
            sprintf(new_base, "make_%s", value);
            fprintf(stderr, "%s: Adjusting base from %s to %s per %s.\n",
                    s_AppName, options->base, new_base, options->map);
            options->base = new_base;
            break;
        }
    }
    
    fclose(map_file);
}


static
void s_ParseOptions(SOptions* options, int* argc, const char* const** argv)
{
    options->base         = NULL;
    options->getter       = NULL;
    options->logfile      = NULL;
    options->map          = NULL;
    options->reviewer     = NULL;
    options->error_status = 1;
        
    while (*argc > 1) {
        if (strcmp((*argv)[1], "-base") == 0  &&  *argc > 2) {
            options->base = (*argv)[2];
            *argv += 2;
            *argc -= 2;
        } else if (strcmp((*argv)[1], "-getter") == 0  &&  *argc > 2) {
            options->getter = (*argv)[2];
            *argv += 2;
            *argc -= 2;
        } else if (strcmp((*argv)[1], "-log") == 0  &&  *argc > 2) {
            options->logfile = (*argv)[2];
            *argv += 2;
            *argc -= 2;
        } else if (strcmp((*argv)[1], "-map") == 0  &&  *argc > 2) {
            options->map = (*argv)[2];
            *argv += 2;
            *argc -= 2;
        } else if (strcmp((*argv)[1], "-reviewer") == 0  &&  *argc > 2) {
            options->reviewer = (*argv)[2];
            *argv += 2;
            *argc -= 2;
        } else if ((*argv)[1][0] == '-') {
            fprintf(stderr, "%s: Unsupported option %s.\n",
                    s_AppName, (*argv)[1]);
            exit(options->error_status);
        } else if (strcmp((*argv)[1], "!") == 0) {
            options->error_status = 0;
            ++*argv;
            --*argc;
        } else {
            break;
        }
    }

    if (options->base == NULL  &&  *argc > 1) {
        const char* p = strrchr((*argv)[1], '/');
        if (p != NULL) {
            options->base = p + 1;
        } else {
            options->base = (*argv)[1];
        }
    }
}


static
int s_Tee(int in, FILE* out1, FILE* out2, EFilterState* state)
{
    char    buffer[1024];
    ssize_t n;
    while ((n = read(in, buffer, sizeof(buffer))) > 0) {
        char* p = buffer;
        if (fwrite(buffer, 1, n, out1) < n) {
            fprintf(stderr, "%s: Error propagating process output: %s.\n",
                    s_AppName, strerror(errno));
        }
        if (*state == eFS_esc) {
            if (*p == '[') {
                ++p;
                --n;
                *state = eFS_csi;
            } else {
                fputc('\033', out2);
                *state = eFS_on;
            }
        } else if (*state == eFS_csi) {
            p = memchr(buffer, 'm', n);
            if (p == NULL) {
                continue;
            } else {
                ++p;
                n -= (p - buffer);
                *state = eFS_on;
            }
        }
        if (*state == eFS_on) {
            const char* start   = p;
            const char* src     = p;
            char*       dest    = p;
            while (src - p < n
                   &&  (start = memchr(src, '\033', n - (src - p))) != NULL
                   &&  (start == p + n - 1  ||  start[1] == '[')) {
                if (src > dest  &&  start > src) {
                    memmove(dest, src, start - src);
                }
                if (start == p + n - 1) {
                    *state = eFS_esc;
                    n = start - p;
                    break;
                }
                dest += start - src;
                const char* end = memchr(start + 2, 'm', n - 2 - (start - p));
                if (end == NULL) {
                    *state = eFS_csi;
                    n = start - p;
                    break;
                } else {
                    src = end + 1;
                }
            }
            memmove(dest, src, n - (src - p));
            n -= src - dest;
        }
        if (fwrite(p, 1, n, out2) < n) {
            fprintf(stderr, "%s: Error logging process output: %s.\n",
                    s_AppName, strerror(errno));
        }
    }
    if (n < 0  &&  errno != EAGAIN  &&  errno != EWOULDBLOCK) {
        if (*state == eFS_off  ||  errno != EIO) {
            fprintf(stderr, "%s: Error reading from process: %s.\n",
                    s_AppName, strerror(errno));
        }
        return 1;
    }
    return n == 0;
}


static
void s_OnSIGCHLD(int n)
{
    s_MainChildDone = 1;
}


static
int s_OpenPipeOrPty(int fd_to_mimic, const char* label, int fds[2],
                    EFilterState* state)
{
    if (isatty(fd_to_mimic)) {
        struct termios attr;
        const char*    name;
        *state = eFS_on;
        if ((fds[0] = posix_openpt(O_RDONLY | O_NOCTTY)) < 0) {
            fprintf(stderr, "%s: Error allocating pty master for %s: %s.\n",
                    s_AppName, label, strerror(errno));
            return -1;
        } else if (grantpt(fds[0]) < 0  ||  unlockpt(fds[0]) < 0
                   ||  (name = ptsname(fds[0])) == NULL
                   ||  (fds[1] = open(name, O_WRONLY)) < 0) {
            fprintf(stderr, "%s: Error opening pty slave for %s: %s.\n",
                    s_AppName, label, strerror(errno));
            close(fds[0]);
            return -1;
        }
        if (tcgetattr(fds[1], &attr) < 0) {
            fprintf(stderr,
                    "%s: Warning: unable to get attributes for %s: %s.\n",
                    s_AppName, label, strerror(errno));
        } else {
            attr.c_oflag |= ONLRET;
#ifdef ONLCR
            attr.c_oflag &= ~ONLCR;
#endif
            /* XXX -- propagate anything from fd_to_mimic? */
            if (tcsetattr(fds[1], TCSADRAIN, &attr) < 0) {
                fprintf(stderr,
                    "%s: Warning: unable to set attributes for %s: %s.\n",
                        s_AppName, label, strerror(errno));
            }
        }
    } else {
        if (pipe(fds) < 0) {
            fprintf(stderr, "%s: Error creating pipe for %s: %s.n",
                    s_AppName, label, strerror(errno));
            return -1;
        }
    }

    return 0;
}


static
int s_Run(const char* const* args, FILE* log)
{
    int   stdout_fds[2], stderr_fds[2];
    int   status = W_EXITCODE(126, 0);
    pid_t pid;
    EFilterState stdout_state = eFS_off, stderr_state = eFS_off;
    if (log != NULL) {
        if (s_OpenPipeOrPty(STDOUT_FILENO, "stdout", stdout_fds, &stdout_state)
            < 0) {
            return status;
        }
        if (s_OpenPipeOrPty(STDERR_FILENO, "stderr", stderr_fds, &stderr_state)
            < 0) {
            close(stdout_fds[0]);
            close(stdout_fds[1]);
            return status;
        }
        s_MainChildDone = 0;
        signal(SIGCHLD, s_OnSIGCHLD);
    }
    
    pid = fork();
    if (pid < 0) { /* error */
        fprintf(stderr, "%s: Fork failed: %s.\n", s_AppName, strerror(errno));
    } else if (pid == 0) { /* child */
        int n;
        if (log != NULL) {
            close(stdout_fds[0]);
            close(stderr_fds[0]);
            dup2(stdout_fds[1], STDOUT_FILENO);
            dup2(stderr_fds[1], STDERR_FILENO);
        }
        for (n = 1;  n <= NSIG;  ++n) {
            if (n != SIGKILL  &&  n != SIGSTOP) {
                signal(n, s_OrigHandlers[n]);
            }
        }
        execvp(args[0], (char* const*) args);
        fprintf(stderr, "%s: Unable to exec %s: %s.\n",
                s_AppName, args[0], strerror(errno));
        _exit(127);
    } else { /* parent */
        if (log != NULL) {
            int stdout_done = 0, stderr_done = 0;
            close(stdout_fds[1]);
            close(stderr_fds[1]);
            fcntl(stdout_fds[0], F_SETFL,
                  fcntl(stdout_fds[0], F_GETFL) | O_NONBLOCK);
            fcntl(stderr_fds[0], F_SETFL,
                  fcntl(stderr_fds[0], F_GETFL) | O_NONBLOCK);
            while ( !stdout_done  ||  !stderr_done ) {
                fd_set rfds, efds;
                unsigned int nfds = 0;
                FD_ZERO(&rfds);
                FD_ZERO(&efds);
                if ( !stderr_done ) {
                    FD_SET(stderr_fds[0], &rfds);
                    FD_SET(stderr_fds[0], &efds);
                    nfds = stderr_fds[0] + 1;
                }
                if ( !stdout_done ) {
                    FD_SET(stdout_fds[0], &rfds);
                    FD_SET(stdout_fds[0], &efds);
                    if (stdout_fds[0] >= nfds) {
                        nfds = stdout_fds[0] + 1;
                    }
                }
                if (select(nfds, &rfds, NULL, &efds, NULL) < 0
                    &&  errno != EINTR  &&  errno != EAGAIN) {
                    fprintf(stderr,
                            "%s: Error checking for output to log: %s.\n",
                            s_AppName, strerror(errno));
                    break;
                }
                if (FD_ISSET(stdout_fds[0], &rfds)
                    ||  FD_ISSET(stdout_fds[0], &efds)) {
                    stdout_done = s_Tee(stdout_fds[0], stdout, log,
                                        &stdout_state);
                }
                if (FD_ISSET(stderr_fds[0], &rfds)
                    ||  FD_ISSET(stderr_fds[0], &efds)) {
                    stderr_done = s_Tee(stderr_fds[0], stderr, log,
                                        &stderr_state);
                }
                if (s_MainChildDone  &&  ( !stdout_done  ||  !stderr_done )
                    &&  waitpid(pid, &status, WNOHANG) != 0) {
                    s_MainChildDone = 0;
                    fflush(log);
                    if (fork() > 0) {
                        s_FinishingInBackground = 1;
                        break;
                    }
                }
            }
            signal(SIGCHLD, s_OrigHandlers[SIGCHLD]);
        }
        if (log == NULL  ||  !s_FinishingInBackground ) {
            waitpid(pid, &status, 0);
        }
    }

    if (log != NULL) {
        close(stdout_fds[0]);
        close(stderr_fds[0]);
    }
    return status;
}


static
void s_CleanUp(void)
{
    const char* args[] = { "/bin/rm", "-rf", s_LockName, NULL };
    if ( !s_FinishingInBackground ) {
        s_Run(args, NULL);
    }
}


static
void s_OnSignal(int n)
{
    /* Propagate to child?  The shell version doesn't. */
    fprintf(stderr, "%s: Caught signal %d\n", s_AppName, n);
    /*
    s_CleanUp();
    _exit(n | 0x80);
    */
    s_CaughtSignal = n;
    signal(n, s_OnSignal);
}


typedef enum {
    eOldLogMissing,
    eNewLogBoring,
    eNewLogInteresting
} ELogStatus;


int main(int argc, const char* const* argv)
{
    SOptions    options;
    char        pid_str[sizeof(pid_t) * 3 + 1], new_log[PATH_MAX];
    const char* getter_args[] = { "get_lock", "BASE", pid_str, NULL };
    FILE*       log = NULL;
    int         status = 0, n;

    s_AppName = argv[0];
    s_ParseOptions(&options, &argc, &argv);

    if (options.map != NULL  &&  strncmp(options.base, "make_", 5) == 0) {
        s_ApplyMap(&options);
    }

    if (options.getter != NULL) {
        getter_args[0] = options.getter;
    }
    getter_args[1] = options.base;
    sprintf(pid_str, "%ld", (long)getpid());
    sprintf(s_LockName, "%s.lock", options.base);
    for (n = 1;  n <= NSIG;  ++n) {
        switch (n) {
            /* Blacklist some signals for various reasons. */
        case SIGQUIT: case SIGILL: case SIGABRT: case SIGFPE: case SIGSEGV:
        case SIGBUS: case SIGSYS: case SIGTRAP: case SIGXCPU: case SIGXFSZ:
            /* Don't touch anything severe enough to cause a core dump. */
        case SIGPIPE:
            /* Should react immediately here too. */
        case SIGKILL: case SIGSTOP:
            /* Uncatchable. */
        case SIGTSTP: case SIGTTIN: case SIGTTOU:
            /* Harmless temporary suspension; don't overreact. */
        case SIGCHLD: case SIGCONT: case SIGURG: case SIGWINCH:
            /* Totally harmless; don't overreact. */
            break;
        default:
            s_OrigHandlers[n] = signal(n, s_OnSignal);
        }
    }
    s_Run(getter_args, NULL);
    if (s_CaughtSignal) {
        goto cleanup;
    }

    if (options.logfile != NULL) {
        sprintf(new_log, "%s.new", options.logfile);
        log = fopen(new_log, "w");
        if (log == NULL) {
            fprintf(stderr, "%s: Couldn't open log file %s: %s.\n",
                    s_AppName, options.logfile, strerror(errno));
            return options.error_status;
        }
    }

    if ( !s_CaughtSignal ) {
        status = s_Run(argv + 1, log);
    }

    if (log != NULL  &&  !s_CaughtSignal  &&  access(new_log, F_OK) == 0) {
        ELogStatus log_status = eNewLogInteresting;
        fclose(log);
        if (access(options.logfile, F_OK) != 0) {
            log_status = eOldLogMissing;
        } else if (options.reviewer != NULL) {
            const char* reviewer_args[] = { options.reviewer, new_log, NULL };
            if (s_Run(reviewer_args, NULL) != 0) {
                log_status = eNewLogBoring;
            }
        }
        if (log_status != eNewLogBoring) {
            if (rename(new_log, options.logfile) < 0) {
                fprintf(stderr, "%s: Unable to rename log file %s: %s.\n",
                        s_AppName, new_log, strerror(errno));
            }
        }
    }

 cleanup:
    s_CleanUp();

    if (s_CaughtSignal) {
        status = s_CaughtSignal | 0x80;
    } else if (WIFSIGNALED(status)) {
        status = WTERMSIG(status) | 0x80;
    } else {
        status = WEXITSTATUS(status);
    }
    if (options.error_status == 0) {
        status = !status;
    }

    return status;
}
