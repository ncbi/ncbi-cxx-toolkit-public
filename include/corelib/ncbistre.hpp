#ifndef NCBISTRE__HPP
#define NCBISTRE__HPP

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
*   NCBI C++ stream class wrappers
*   Triggering between "new" and "old" C++ stream libraries
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.6  1998/11/03 22:56:17  vakatov
* + #define I/O manipulators like "flush" --> "NcbiFlush"
*
* Revision 1.5  1998/11/03 20:48:18  vakatov
* + <iostream>
* + SEEKOFF
*
* Revision 1.4  1998/10/30 20:08:33  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.3  1998/10/28 22:43:17  vakatov
* Catch the "strstrea.h" case(MSVC++ feature)
*
* Revision 1.2  1998/10/27 20:10:02  vakatov
* Catch the case of missing iostream header(s)
*
* Revision 1.1  1998/10/27 19:49:36  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <ncbistl.hpp>

#if !defined(HAVE_IOSTREAM_H)
#  define NCBI_USE_NEW_IOSTREAM
#endif


#if defined(HAVE_IOSTREAM)  &&  defined(NCBI_USE_NEW_IOSTREAM)
#  include <iostream>
#  include <fstream>
#  include <strstream>
#  define IO_PREFIX  std
#  define IOS_BASE   std::ios_base
#  define SEEKOFF    pubseekoff

#elif defined(HAVE_IOSTREAM_H)
#  include <iostream.h>
#  include <fstream.h>
#  if defined(HAVE_STRSTREA_H)
#    include <strstrea.h>
#  else
#    include <strstream.h>
#  endif
#  define IO_PREFIX
#  define IOS_BASE   ::ios
#  define SEEKOFF    seekoff

#else
#  error "Cannot find neither <iostream> nor <iostream.h>!"
#endif

// I/O classes
typedef IO_PREFIX::streampos     CNcbiStreampos;
typedef IO_PREFIX::streamoff     CNcbiStreamoff;

typedef IO_PREFIX::ios           CNcbiIos;

typedef IO_PREFIX::streambuf     CNcbiStreambuf;
typedef IO_PREFIX::istream       CNcbiIstream;
typedef IO_PREFIX::ostream       CNcbiOstream;
typedef IO_PREFIX::iostream      CNcbiIostream;

typedef IO_PREFIX::strstreambuf  CNcbiStrstreambuf;
typedef IO_PREFIX::istrstream    CNcbiIstrstream;
typedef IO_PREFIX::ostrstream    CNcbiOstrstream;
typedef IO_PREFIX::strstream     CNcbiStrstream;

typedef IO_PREFIX::filebuf       CNcbiFilebuf;
typedef IO_PREFIX::ifstream      CNcbiIfstream;
typedef IO_PREFIX::ofstream      CNcbiOfstream;
typedef IO_PREFIX::fstream       CNcbiFstream;

// Standard I/O streams
#define NcbiCin   IO_PREFIX::cin
#define NcbiCout  IO_PREFIX::cout
#define NcbiCerr  IO_PREFIX::cerr
#define NcbiClog  IO_PREFIX::clog

// I/O manipulators
#define NcbiEndl   IO_PREFIX::endl
#define NcbiEnds   IO_PREFIX::ends
#define NcbiFlush  IO_PREFIX::flush
#define NcbiDec    IO_PREFIX::dec
#define NcbiHex    IO_PREFIX::hex
#define NcbiOct    IO_PREFIX::oct
#define NcbiWs     IO_PREFIX::ws

#endif /* NCBISTRE__HPP */
