#ifndef NCBIEXPT__HPP
#define NCBIEXPT__HPP

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
*   NCBI C++ exception handling
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/11/06 22:42:38  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <stdexcept>
#include <ncbidiag.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// The use of C+++ exception specification mechanism, like:
//   "f() throw();"       <==  "f() THROWS_NONE;"
//   "g() throw(e1,e2);"  <==  "f() THROWS((e1,e2));"
#if defined(NCBI_USE_THROW_SPEC)
#define THROWS_NONE throw()
#define THROWS(x) throw x
#else
#define THROWS_NONE
#define THROWS(x)
#endif


// Standard handling of "exception"-derived exceptions
#define STD_CATCH(message) \
catch (exception& e) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Exception: " << e.what(); \
}

// Standard handling of "exception"-derived and all other exceptions
#define STD_CATCH_ALL(message) \
STD_CATCH(message) \
    catch (...) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Unknown exception"; \
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIEXPT__HPP */
