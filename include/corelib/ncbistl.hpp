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
* Revision 1.13  1999/10/12 21:46:31  vakatov
* Assume all supported compilers are namespace-capable and have "std::"
*
* Revision 1.12  1999/05/13 16:43:29  vakatov
* Added Mac(#NCBI_OS_MAC) to the list of supported platforms
* Also, use #NCBI_OS to specify a string representation of the
* current platform("UNIX", "WINDOWS", or "MAC")
*
* Revision 1.11  1999/05/10 14:26:07  vakatov
* Fixes to compile and link with the "egcs" C++ compiler under Linux
*
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
* ==========================================================================
*/

#include <ncbiconf.h>

// Get rid of some warnings in MSVC++ 6.00
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


// Using STD and NCBI namespaces
#define NCBI_NS_STD  std
#define NCBI_USING_NAMESPACE_STD using namespace NCBI_NS_STD

#define NCBI_NS_NCBI ncbi
#define BEGIN_NCBI_SCOPE namespace NCBI_NS_NCBI {
#define END_NCBI_SCOPE }
#define USING_NCBI_SCOPE using namespace NCBI_NS_NCBI


// Magic spells ;-) needed for some weird compilers... very empiric
namespace NCBI_NS_STD  { /* the fake one */ }
namespace NCBI_NS_NCBI { /* the fake one, +"std" */ NCBI_USING_NAMESPACE_STD; }
namespace NCBI_NS_NCBI { /* the fake one */ }


// SIZE_TYPE and NPOS 
#if !defined(SIZE_TYPE)
#  define SIZE_TYPE NCBI_NS_STD::string::size_type
#endif

#if !defined(NPOS)
#  define NPOS NCBI_NS_STD::string::npos
#endif

#endif /* NCBISTL__HPP */
