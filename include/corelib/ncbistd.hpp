#ifndef NCBISTD__HPP
#define NCBISTD__HPP

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
*   The NCBI C++/STL use hints
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/10/07 23:05:18  vakatov
* Do not "using namespace std;" here. -- Instead, provide NCBI_USING_STL
* macro to be included(only when neceassary) in the user modules
*
* Revision 1.2  1998/10/05 21:04:34  vakatov
* Introduced #NCBI_SGI_STL_PORT and #NCBI_NO_NAMESPACES flags
*
* Revision 1.1  1998/10/05 19:43:38  vakatov
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



// Use of the STLport package("http://corp.metabyte.com/~fbp/stl/effort.html")
#ifdef NCBI_SGI_STL_PORT
#include <stl_config.h>

#if !defined(NCBI_NO_NAMESPACES)  &&  (!defined(__STL_NAMESPACES)  ||  defined(__STL_NO_NAMESPACES))
#define NCBI_NO_NAMESPACES
#endif
#endif


// Using the STL namespace
// Provide at least quasi-namespace(based on "struct") functionality
#if defined(NCBI_NO_NAMESPACES)
#define namespace struct
#define NCBI_USING_STL
#else
#define NCBI_USING_STL using namespace std
#endif


#endif /* NCBISTD__HPP */
