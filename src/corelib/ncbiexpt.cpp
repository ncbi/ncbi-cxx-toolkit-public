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
* Author:  Denis Vakatov
*
* File Description:
*   CErrnoException
*   CParseException
*   + initialization for the "unexpected"
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2001/07/30 14:42:10  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.18  2001/05/21 21:44:00  vakatov
* SIZE_TYPE --> string::size_type
*
* Revision 1.17  2001/05/17 15:04:59  lavr
* Typos corrected
*
* Revision 1.16  2000/11/16 23:52:41  vakatov
* Porting to Mac...
*
* Revision 1.15  2000/04/04 22:30:26  vakatov
* SetThrowTraceAbort() -- auto-set basing on the application
* environment and/or registry
*
* Revision 1.14  1999/12/29 13:58:39  vasilche
* Added THROWS_NONE.
*
* Revision 1.13  1999/12/28 21:04:18  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.12  1999/11/18 20:12:43  vakatov
* DoDbgPrint() -- prototyped in both _DEBUG and NDEBUG
*
* Revision 1.11  1999/10/04 16:21:04  vasilche
* Added full set of macros THROW*_TRACE
*
* Revision 1.10  1999/09/27 16:23:24  vasilche
* Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
* so that they will be much easier for compilers to eat.
*
* Revision 1.9  1999/05/04 00:03:13  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.8  1999/04/14 19:53:29  vakatov
* + <stdio.h>
*
* Revision 1.7  1999/01/04 22:41:43  vakatov
* Do not use so-called "hardware-exceptions" as these are not supported
* (on the signal level) by UNIX
* Do not "set_unexpected()" as it works differently on UNIX and MSVC++
*
* Revision 1.6  1998/12/28 17:56:37  vakatov
* New CVS and development tree structure for the NCBI C++ projects
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
#include <errno.h>
#include <string.h>
#include <stdio.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


/////////////////////////////////
// SetThrowTraceAbort
// DoThrowTraceAbort

static bool s_DoThrowTraceAbort = false; //if to abort() in DoThrowTraceAbort()
static bool s_DTTA_Initialized  = false; //if s_DoThrowTraceAbort is init'd

extern void SetThrowTraceAbort(bool abort_on_throw_trace)
{
    s_DTTA_Initialized = true;
    s_DoThrowTraceAbort = abort_on_throw_trace;
}

extern void DoThrowTraceAbort(void)
{
    if ( !s_DTTA_Initialized ) {
        const char* str = getenv(ABORT_ON_THROW);
        if (str  &&  *str)
            s_DoThrowTraceAbort = true;
        s_DTTA_Initialized  = true;
    }

    if ( s_DoThrowTraceAbort )
        abort();
}

extern void DoDbgPrint(const char* file, int line, const char* message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const char* file, int line, const string& message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const char* file, int line,
                       const char* msg1, const char* msg2)
{
    CNcbiDiag(file, line, eDiag_Trace) << msg1 << ": " << msg2;
    DoThrowTraceAbort();
}

/////////////////////////////////
//  CErrnoException

CErrnoException::CErrnoException(const string& what) THROWS_NONE
    : runtime_error(what + ": " + ::strerror(errno)), m_Errno(errno)
{
}

CErrnoException::~CErrnoException(void) THROWS_NONE
{
}


/////////////////////////////////
//  CParseException

static string s_ComposeParse(const string& what, string::size_type pos)
{
    char s[32];
    ::sprintf(s, "%ld", (long)pos);
    string str;
    str.reserve(256);
    return str.append("{").append(s).append("} ").append(what);
}

CParseException::CParseException(const string& what, string::size_type pos)
THROWS_NONE
    : runtime_error(s_ComposeParse(what,pos)), m_Pos(pos)
{
}

CParseException::~CParseException(void) THROWS_NONE
{
}

// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
