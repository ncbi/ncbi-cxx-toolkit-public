#ifndef NCBISTL__HPP
#define NCBISTL__HPP

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
*   The NCBI C++/STL use hints
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.8  1998/12/01 01:18:42  vakatov
* [HAVE_NAMESPACE]  Added an empty fake "ncbi" namespace -- MSVC++ 6.0
* compiler starts to recognize "std::" scope only after passing through at
* least one "namespace ncbi {}" statement(???)
*
* Revision 1.7  1998/11/13 00:13:51  vakatov
* Decide whether it NCBI_OS_UNIX or NCBI_OS_MSWIN
*
* Revision 1.6  1998/11/10 01:13:40  vakatov
* Moved "NCBI_USING_NAMESPACE_STD:" to the first(fake) definition of
* namespace "ncbi" -- no need to "using ..." it in every new "ncbi"
* scope definition...
*
* Revision 1.5  1998/11/06 22:42:39  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

/////////////////////////////////////////////////////////////////////////////
// Effective preprocessor switches:
//
//   NCBI_SGI_STL_PORT  -- use the STLport package based on SGI STL
//                         ("http://corp.metabyte.com/~fbp/stl/effort.html")
//   NCBI_NO_NAMESPACES -- assume no namespace support
//
/////////////////////////////////////////////////////////////////////////////


#include <ncbiconf.h>

// Deduce the generic platform
// (this header is not right place for it; to be moved somewhere else later...)
#if defined(HAVE_UNISTD_H)
#  define NCBI_OS_UNIX
#elif defined(HAVE_WINDOWS_H)
#  define NCBI_OS_MSWIN
#else
#  error "Unknown platform(must be one of:  UNIX, MS-Windows)!"
#endif

// Use of the STLport package("http://corp.metabyte.com/~fbp/stl/effort.html")
#if defined(NCBI_SGI_STL_PORT)
#  include <stl_config.h>
#  if !defined(NCBI_NO_NAMESPACES)  &&  (!defined(__STL_NAMESPACES)  ||  defined(__STL_NO_NAMESPACES))
#    define NCBI_NO_NAMESPACES
#  endif
#else
// these macros are used in STLport's Modena string library
#  define SIZE_TYPE string::size_type
#  define NPOS string::npos
#endif

// get rid of long identifier warning in visual c++ 5 and 6
#if (_MSC_VER <= 1200)
#  pragma warning(disable: 4786)  
#endif

// Check if this compiler supports namespaces at all... (see in <ncbiconf.h>)
#if !defined(NCBI_NO_NAMESPACES)  &&  !defined(HAVE_NAMESPACE)
#  define NCBI_NO_NAMESPACES
#endif

// Using the STL namespace
#if defined(NCBI_NO_NAMESPACES)
#  define NCBI_USING_NAMESPACE_STD
#else
#  define NCBI_USING_NAMESPACE_STD using namespace std
#endif


// Using the NCBI namespace
// To avoid the annoying(and potentially dangerous -- e.g. when dealing with a
// non-namespace-capable compiler) use of "std::" prefix in NCBI header files
// and "std::" and/or "ncbi::" prefixes in the source files
#if defined(HAVE_NAMESPACE)
namespace std { /* the fake one */ }
namespace ncbi { /* the fake one, plus "std" */ NCBI_USING_NAMESPACE_STD; }
namespace ncbi { /* the fake one */ }
#  define BEGIN_NCBI_SCOPE namespace ncbi {
#  define END_NCBI_SCOPE }
#  define USING_NCBI_SCOPE using namespace ncbi
#else
#  define BEGIN_NCBI_SCOPE
#  define END_NCBI_SCOPE
#  define USING_NCBI_SCOPE
#endif

#endif /* NCBISTL__HPP */
