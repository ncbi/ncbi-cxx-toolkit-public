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
* Author: Vsevolod Sandomirskiy, Denis Vakatov
*
* File Description:
*   NCBI OS-independent C++ exceptions
*   To be used to catch UNIX sync signals or Win32 SEH exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/11/13 00:17:13  vakatov
* [UNIX] Added handler for the unexpected exceptions
*
* Revision 1.2  1998/11/10 17:58:42  vakatov
* [UNIX] Removed extra #define's (POSIX... and EXTENTIONS...)
* Allow adding strings in CNcbiErrnoException(must have used "CC -xar"
* instead of just "ar" when building a library;  otherwise -- link error)
*
* Revision 1.1  1998/11/10 01:20:01  vakatov
* Initial revision(derived from former "ncbiexcp.cpp")
*
* ===========================================================================
*/

#include <ncbiexpt.hpp>

#if defined(NCBI_OS_UNIX)
#  include <signal.h>
#  include <siginfo.h>
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#endif

#include <errno.h>
#include <string.h>



// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


/////////////////////////////////
//  CNcbiErrnoException

CNcbiErrnoException::CNcbiErrnoException(const string& what) throw()
        : runtime_error(what + ":  " + strerror(errno)) {
}



/////////////////////////////////
// CNcbiOSException

#if defined(NCBI_OS_UNIX)

static void s_UnexpectedHandler(void) THROWS((bad_exception)) {
    throw bad_exception();
}

extern "C" void s_DefSignalHandler(int sig, siginfo_t*, void*)
    THROWS((CNcbiOSException))
{
  char msg[64];
  sprintf(msg, "Caught UNIX signal, code %d", sig);

  switch ( sig ) {
  case SIGBUS:
  case SIGSEGV:
      throw CNcbiMemException(msg);
  case SIGFPE:
      throw CNcbiFPEException(msg);
  default: 
      throw CNcbiSystemException(msg);
  }
}


void CNcbiOSException::Initialize(void)
    THROWS((runtime_error))
{
    // set DefSignalHandler() as signal handler for sync signals
    // set sigmask to allow delivery only sync signals for current thread
    //   (the signal mask of the thread is inherited)
    // signal handlers are not restored if an error occured

    // signals considered to be sync and to be caught
    // For SIGTTIN, SIGTTOU default action is to stop the process,
    // for others - to terminate

    sigset_t sigset;
    sigfillset(&sigset);
    if (sigprocmask(SIG_BLOCK, &sigset, 0) != 0)
        throw CNcbiErrnoException("CNcbiOSException:: block-all signal mask");
    sigemptyset(&sigset);

    struct sigaction sa; 
    sa.sa_sigaction = s_DefSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;  // do not block signal when it's caught

    const static int s_Sig[] = {
        SIGILL, SIGFPE, SIGABRT, SIGBUS, SIGSYS, SIGPIPE, 
        SIGTTIN, SIGTTOU, SIGXFSZ, 0
    };
    for (const int* sig = s_Sig;  *sig;  sig++) {
        if (sigaction(*sig, &sa, 0) != 0)
            throw CNcbiErrnoException("CNcbiOSException:: def. sig.-handler");
        if (sigaddset(&sigset, *sig) != 0)
            throw CNcbiErrnoException("CNcbiOSException:: ");
    }

    if (sigprocmask(SIG_UNBLOCK, &sigset, 0) != 0)
        throw CNcbiErrnoException("CNcbiOSException:: unblocking sig.-mask");

    set_unexpected(s_UnexpectedHandler);
}

#elif defined(NCBI_OS_MSWIN)

static void s_DefSEHandler(unsigned int sig, struct _EXCEPTION_POINTERS*)
	THROWS((CNcbiOSException))
{
    char msg[64];
    sprintf(msg, "Win32 exception code %X", sig);
    
    switch ( sig ) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INVALID_HANDLE:
        throw CNcbiMemException(msg);
    default:
        if (EXCEPTION_FLT_DENORMAL_OPERAND <= sig  &&
            sig <= EXCEPTION_INT_OVERFLOW) {
            throw CNcbiFPEException(msg);
        }

        throw CNcbiSystemException(msg);
    }
}


void CNcbiOSException::Initialize(void)
    THROWS((runtime_error)) {
        _set_se_translator(s_DefSEHandler);
}

#endif


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
