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

#include <ncbidiag.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


#if defined(_DEBUG)

#  define _FILE_LINE \
"{" << __FILE__ << ":" << __LINE__ << "} "

#  define _TRACE(message)  { \
    CNcbiDiag _diag_(eDiag_Trace); \
    _diag_ << _FILE_LINE << message; \
}

#  define _TROUBLE  { \
    CNcbiDiag _diag_(eDiag_Fatal); \
    _diag_ << _FILE_LINE << "Trouble!"; \
}
#  define _ASSERT(expr)  { \
    if ( !expr ) \
        { \
              CNcbiDiag _diag_(eDiag_Fatal); \
              _diag_ << _FILE_LINE << "Assertion failed: " << #expr; \
        } \
}

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
