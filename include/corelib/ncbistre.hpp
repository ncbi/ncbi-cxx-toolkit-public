#ifndef NCBISTRE__HPP
#define NCBISTRE__HPP

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
*   NCBI C++ stream class wrappers
*   Triggering between "new" and "old" C++ stream libraries
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/27 19:49:36  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <ncbistl.hpp>

#if defined(HAVE_IOSTREAM)  &&  defined(NCBI_USE_NEW_IOSTREAM)
#include <fstream>
#include <strstream>
#define IO_PREFIX std
#else
#include <fstream.h>
#include <strstream.h>
#define IO_PREFIX
#endif


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


#define NcbiCin   IO_PREFIX::cin
#define NcbiCout  IO_PREFIX::cout
#define NcbiCerr  IO_PREFIX::cerr
#define NcbiClog  IO_PREFIX::clog


#endif /* NCBISTRE__HPP */
