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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1998/12/03 18:56:14  vakatov
* minor fixes
*
* Revision 1.1  1998/12/03 16:40:26  vakatov
* Initial revision
* Aux. function "Getline()" to read from "istream" to a "string"
* Adopted standard I/O "string" <--> "istream" for old-fashioned streams
*
* ===========================================================================
*/

#include <ncbistre.hpp>
#include <ctype.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim)
{
    int ch;
    if ( !is.ipfx(1) )
      return is;

    str.erase();

    SIZE_TYPE end = str.max_size();
    SIZE_TYPE i = 0;
    for (ch = is.rdbuf()->sbumpc();  ch != EOF;  ch = is.rdbuf()->sbumpc()) {
      i++;
      if ((char)ch == delim)
        break;
      if (i == end) {
          is.clear(NcbiFailbit | is.rdstate());      
          break;
      }
        
      str.append(1, (char)ch);
    }
    if (ch == EOF) 
      is.clear(NcbiEofbit | is.rdstate());      
    if (!i)
      is.clear(NcbiFailbit | is.rdstate());      

    is.isfx();
    return is;
}



// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE


// See in the header why it's outside the NCBI scope(SunPro bug workaround...)
#if !defined(NCBI_USE_NEW_IOSTREAM)
extern NCBI_NS_NCBI::CNcbiOstream& operator<<(NCBI_NS_NCBI::CNcbiOstream& os,
                                              const NCBI_NS_STD::string& str)
{
    return str.empty() ? os : os << str.c_str();
}


extern NCBI_NS_NCBI::CNcbiIstream& operator>>(NCBI_NS_NCBI::CNcbiIstream& is,
                                              NCBI_NS_STD::string& str)
{
    int ch;
    if ( !is.ipfx() )
        return is;

    str.erase();

    SIZE_TYPE end = str.max_size();
    if ( is.width() )
      end = (int)end < is.width() ? end : is.width(); 

    SIZE_TYPE i = 0;
    for (ch = is.rdbuf()->sbumpc();  ch != EOF  &&  !isspace(ch);
         ch = is.rdbuf()->sbumpc()) {
        str.append(1, (char)ch);
        i++;
        if (i == end)
            break;
    }
    if (ch == EOF) 
      is.clear(NcbiEofbit | is.rdstate());      
    if ( !i )
      is.clear(NcbiFailbit | is.rdstate());      

    is.width(0);
    return is;
}
#endif  /* ndef NCBI_USE_NEW_IOSTREAM */

