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
* Revision 1.6  1998/12/28 17:56:37  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/11/26 00:03:52  vakatov
* [_DEBUG] Do not catch OS exceptions in debug mode
*
* Revision 1.4  1998/11/24 17:51:46  vakatov
* + CParseException
* Removed "Ncbi" sub-prefix from the NCBI exception class names
* [NCBI_OS_UNIX] s_DefSignalHandler() -- handle only SEGV,BUS and FPE
*
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

#include <corelib/ncbiexpt.hpp>

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
//  CErrnoException

CErrnoException::CErrnoException(const string& what)
    throw() : runtime_error(what + ": " + ::strerror(errno)) {
        m_Errno = errno;
}


/////////////////////////////////
//  CParseException

static string s_ComposeParse(const string& what, SIZE_TYPE pos)
{
    char s[32];
    ::sprintf(s, "%ld", (long)pos);
    string str;
    str.reserve(256);
    return str.append("{").append(s).append("} ").append(what);
}

CParseException::CParseException(const string& what, SIZE_TYPE pos)
    throw() : runtime_error(s_ComposeParse(what,pos)) {
        m_Pos = pos;
}




/////////////////////////////////
// COSException

#if defined(NCBI_OS_UNIX)

static void s_UnexpectedHandler(void) THROWS((bad_exception)) {
    throw bad_exception();
}

#  if !defined(_DEBUG)
extern "C" void s_DefSignalHandler(int sig, siginfo_t*, void*)
    THROWS((COSException))
{
  char msg[64];
  sprintf(msg, "Caught UNIX signal, code %d", sig);

  switch ( sig ) {
  case SIGBUS:
  case SIGSEGV:
      throw CMemException(msg);
  case SIGFPE:
      throw CFPEException(msg);
  default: 
      break;
      // throw CSystemException(msg);
  }
}
#  endif /* ndef _DEBUG */


void COSException::Initialize(void)
    THROWS((runtime_error))
{
#  if !defined(_DEBUG)
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
        throw CErrnoException("COSException:: block-all signal mask");
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
            throw CErrnoException("COSException:: def. sig.-handler");
        if (sigaddset(&sigset, *sig) != 0)
            throw CErrnoException("COSException:: ");
    }

    if (sigprocmask(SIG_UNBLOCK, &sigset, 0) != 0)
        throw CErrnoException("COSException:: unblocking sig.-mask");
#  endif /* ndef _DEBUG */

    set_unexpected(s_UnexpectedHandler);
}

#elif defined(NCBI_OS_MSWIN)

#  if !defined(_DEBUG)
static void s_DefSEHandler(unsigned int sig, struct _EXCEPTION_POINTERS*)
	THROWS((COSException))
{
    char msg[64];
    sprintf(msg, "Win32 exception code %X", sig);
    
    switch ( sig ) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INVALID_HANDLE:
        throw CMemException(msg);
    default:
        if (EXCEPTION_FLT_DENORMAL_OPERAND <= sig  &&
            sig <= EXCEPTION_INT_OVERFLOW) {
            throw CFPEException(msg);
        }

        throw CSystemException(msg);
    }
}
#  endif /* ndef _DEBUG */


void COSException::Initialize(void)
    THROWS((runtime_error)) {
#  if !defined(_DEBUG)
        _set_se_translator(s_DefSEHandler);
#  endif /* ndef _DEBUG */
}

#endif /* NCBI_OS_UNIX else NCBI_OS_MSWIN */


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
