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
* Revision 1.10  1999/04/14 19:41:19  vakatov
* [MSVC++] Added pragma's to get rid of some warnings
*
* Revision 1.9  1998/12/03 15:47:50  vakatov
* Scope fixes for #NPOS and #SIZE_TYPE;  redesigned the std:: and ncbi::
* scope usage policy definitions(also, added #NCBI_NS_STD and #NCBI_NS_NCBI)
*
* Revision 1.8  1998/12/01 01:18:42  vakatov
* [HAVE_NAMESPACE]  Added an empty fake "ncbi" namespace -- MSVC++ 6.0
* compiler starts to recognize "std::" scope only after passing through at
* least one "namespace ncbi {}" statement(?!)
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
#endif

// get rid of some warnings in MSVC++ 6.00 and higher
#if (_MSC_VER >= 1200)
// too long identificator name in the debug info;  truncated
#  pragma warning(disable: 4786)
// too long decorated name;  truncated
#  pragma warning(disable: 4503)
// default copy constructor cannot be generated
#  pragma warning(disable: 4511)
// default assignment operator cannot be generated
#  pragma warning(disable: 4512)
// synonymous name used
#  pragma warning(disable: 4097)
#endif /* _MSC_VER >= 1200 */

// Check if this compiler supports namespaces at all... (see in <ncbiconf.h>)
#if !defined(NCBI_NO_NAMESPACES)  &&  !defined(HAVE_NAMESPACE)
#  define NCBI_NO_NAMESPACES
#endif


// Using the STL and NCBI namespaces
// To avoid the annoying(and potentially dangerous -- e.g. when dealing with a
// non-namespace-capable compiler) use of "std::" prefix in NCBI header files
// and "std::" and/or "ncbi::" prefixes in the source files
#if defined(NCBI_NO_NAMESPACES)
#  define NCBI_NS_STD
#  define NCBI_NS_NCBI
#  define NCBI_USING_NAMESPACE_STD
#  define BEGIN_NCBI_SCOPE
#  define END_NCBI_SCOPE
#  define USING_NCBI_SCOPE
#else
#  define NCBI_NS_STD  std
#  define NCBI_NS_NCBI ncbi
#  define NCBI_USING_NAMESPACE_STD using namespace NCBI_NS_STD
namespace NCBI_NS_STD  { /* the fake one */ }
namespace NCBI_NS_NCBI { /* the fake one, +"std" */ NCBI_USING_NAMESPACE_STD; }
namespace NCBI_NS_NCBI { /* the fake one */ }
#  define BEGIN_NCBI_SCOPE namespace NCBI_NS_NCBI {
#  define END_NCBI_SCOPE }
#  define USING_NCBI_SCOPE using namespace NCBI_NS_NCBI
#endif

// These two macros are used in STLport's Modena string library
#if !defined(SIZE_TYPE)
#  define SIZE_TYPE NCBI_NS_STD::string::size_type
#endif
#if !defined(NPOS)
#  define NPOS NCBI_NS_STD::string::npos
#endif

#endif /* NCBISTL__HPP */
