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
* Revision 1.11  1999/03/15 16:08:09  vakatov
* Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
*
* Revision 1.10  1999/01/07 16:15:09  vakatov
* Explicitely specify "NCBI_NS_NCBI::" in the preprocessor macros
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

#  define _FILE_LINE \
"{" << __FILE__ << ":" << __LINE__ << "} "

#  define _TRACE(message)  do { \
    NCBI_NS_NCBI::CNcbiDiag _diag_(NCBI_NS_NCBI::eDiag_Trace); \
    _diag_ << _FILE_LINE << message; \
} while(0)

#  define _TROUBLE  do { \
    NCBI_NS_NCBI::CNcbiDiag _diag_(NCBI_NS_NCBI::eDiag_Fatal); \
    _diag_ << _FILE_LINE << "Trouble!"; \
} while(0)
#  define _ASSERT(expr)  do { \
    if ( !(expr) ) \
        { \
              NCBI_NS_NCBI::CNcbiDiag _diag_(NCBI_NS_NCBI::eDiag_Fatal); \
              _diag_ << _FILE_LINE << "Assertion failed: " << #expr; \
        } \
} while(0)

#  define _VERIFY(expr) _ASSERT(expr)

#else  /* _DEBUG */

#  define _TRACE(message)  ((void)0)
#  define _TROUBLE
#  define _ASSERT(expr) ((void)0)
#  define _VERIFY(expr) ((void)(expr))

#endif  /* else!_DEBUG */


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIDBG__HPP */
