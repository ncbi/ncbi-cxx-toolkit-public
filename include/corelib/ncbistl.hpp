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
* Revision 1.4  1998/11/05 00:31:02  lewisg
* added Modena string lib defines and ms vc warning disable
*
* Revision 1.3  1998/11/03 20:46:30  vakatov
* HAVE_NAMESPACES(not just HAVE_NAMESPACE)
* Removed extra semicolumn from NCBI_USING_STL
*
* Revision 1.2  1998/10/30 20:08:32  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.1  1998/10/21 19:22:51  vakatov
* Initial revision
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
#if _MSC_VER <= 1200
#pragma warning(disable: 4786)  
#endif

// Check if this compiler supports namespaces at all... (see in <ncbiconf.h>)
#if !defined(NCBI_NO_NAMESPACES)  &&  !defined(HAVE_NAMESPACE)
#  define NCBI_NO_NAMESPACES
#endif

// Using the STL namespace
#if defined(NCBI_NO_NAMESPACES)
#  define NCBI_USING_STL
#else
#  define NCBI_USING_STL using namespace std
#endif

#endif /* NCBISTL__HPP */
