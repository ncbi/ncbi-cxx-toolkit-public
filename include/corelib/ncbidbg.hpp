#ifndef NCBIDBG__HPP
#define NCBIDBG__HPP

/*  $RCSfile$  $Revision$  $Date$
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
* Revision 1.1  1998/10/23 22:35:51  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <ncbidiag.hpp>

#if defined(_DEBUG)

#  define _FILE_LINE \
"{" << __FILE__ << ":" << __LINE__ << "} "

#  define _TRACE(message)  { \
    CNcbiDiag _trace_diag;  _trace_diag << Trace; \
    _trace_diag << _FILE_LINE << message; \
}

#  define _TROUBLE  { \
    CNcbiDiag _trace_diag;  _trace_diag << Fatal; \
    _trace_diag << _FILE_LINE << "Trouble!"; \
}
#  define _ASSERT(expr)  { \
    if ( expr ) \
        { \
              CNcbiDiag _trace_diag;  _trace_diag << Fatal; \
              _trace_diag << _FILE_LINE << "Assertion failed: " << #expr; \
        } \
}

#  define _VERIFY(expr) _ASSERT(expr)

#else  /* _DEBUG */

#  define _TRACE
#  define _TROUBLE
#  define _ASSERT(expr) ((void)0)
#  define _VERIFY(expr) ((void)(expr))

#endif  /* else!_DEBUG */


#endif  /* NCBIDBG__HPP */
