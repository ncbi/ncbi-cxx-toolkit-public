#ifndef NCBIDBG__HPP
#define NCBIDBG__HPP

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
*   NCBI C++ auxiliary debug macros
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.23  2001/07/30 14:40:47  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.22  2001/05/17 14:50:41  lavr
* Typos corrected
*
* Revision 1.21  2000/06/23 15:12:57  thiessen
* minor change to _ASSERT macro to avoid frequent Mac warnings about semicolons
*
* Revision 1.20  2000/05/24 20:01:07  vasilche
* Added _DEBUG_ARG macro for arguments used only in debug mode.
*
* Revision 1.19  2000/03/10 14:17:40  vasilche
* Added missing namespace specifier to macro.
*
* Revision 1.18  1999/12/27 19:39:12  vakatov
* [_DEBUG]  Forcibly flush ("<< Endm") the diag. stream
*
* Revision 1.17  1999/09/27 16:23:19  vasilche
* Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
* so that they will be much easier for compilers to eat.
*
* Revision 1.16  1999/09/23 21:15:48  vasilche
* Added namespace modifiers.
*
* Revision 1.15  1999/05/04 00:03:05  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.14  1999/05/03 20:32:25  vakatov
* Use the (newly introduced) macro from <corelib/ncbidbg.h>:
*   RETHROW_TRACE,
*   THROW0_TRACE(exception_class),
*   THROW1_TRACE(exception_class, exception_arg),
*   THROW_TRACE(exception_class, exception_args)
* instead of the former (now obsolete) macro _TRACE_THROW.
*
* Revision 1.13  1999/04/30 19:20:56  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.12  1999/04/22 14:18:18  vasilche
* Added _TRACE_THROW() macro which can be configured to make coredump at some point fo throwing exception.
*
* Revision 1.11  1999/03/15 16:08:09  vakatov
* Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
*
* Revision 1.10  1999/01/07 16:15:09  vakatov
* Explicitly specify "NCBI_NS_NCBI::" in the preprocessor macros
*
* Revision 1.9  1998/12/28 17:56:26  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.8  1998/11/19 22:17:41  vakatov
* Forgot "()" in the _ASSERT macro
*
* Revision 1.7  1998/11/18 21:00:24  vakatov
* [_DEBUG]  Fixed a typo in _ASSERT macro
*
* Revision 1.6  1998/11/06 22:42:37  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.5  1998/11/04 23:46:35  vakatov
* Fixed the "ncbidbg/diag" header circular dependencies
*
* ==========================================================================
*/

#include <corelib/ncbidiag.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


#if defined(_DEBUG)

#  define _TRACE(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__, NCBI_NS_NCBI::eDiag_Trace) \
      << message << NCBI_NS_NCBI::Endm )

#  define _TROUBLE \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__, NCBI_NS_NCBI::eDiag_Fatal) \
      << "Trouble!" << NCBI_NS_NCBI::Endm )

#  define _ASSERT(expr) \
    if (!( expr )) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__, NCBI_NS_NCBI::eDiag_Fatal) \
      << "Assertion failed: (" #expr ")" << NCBI_NS_NCBI::Endm )

#  define _VERIFY(expr) _ASSERT(expr)

#  define _DEBUG_ARG(arg) arg

#else  /* _DEBUG */

#  define _TRACE(message)  ((void)0)
#  define _TROUBLE
#  define _ASSERT(expr) ((void)0)
#  define _VERIFY(expr) ((void)(expr))
#  define _DEBUG_ARG(arg)

#endif  /* else!_DEBUG */


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIDBG__HPP */
