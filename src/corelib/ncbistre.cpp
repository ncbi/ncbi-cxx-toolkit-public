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
* Revision 1.10  2000/12/24 00:03:20  vakatov
* Include ncbistd.hpp instead of ncbiutil.hpp
*
* Revision 1.9  2000/12/15 15:36:41  vasilche
* Added header corelib/ncbistr.hpp for all string utility functions.
* Optimized string utility functions.
* Added assignment operator to CRef<> and CConstRef<>.
* Add Upcase() and Locase() methods for automatic conversion.
*
* Revision 1.8  2000/12/12 14:39:50  vasilche
* Added class Locase for printing strings to ostream with automatic conversion.
*
* Revision 1.7  2000/12/12 14:20:36  vasilche
* Added operator bool to CArgValue.
* Various NStr::Compare() methods made faster.
* Added class Upcase for printing strings to ostream with automatic conversion.
*
* Revision 1.6  1999/12/28 18:55:43  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.5  1999/05/06 23:02:40  vakatov
* Use the new(template-based, std::) stream library by default
*
* Revision 1.4  1998/12/30 23:15:11  vakatov
* [NCBI_USE_NEW_IOSTREAM] NcbiGetline() -- use "smart" getline()
*
* Revision 1.3  1998/12/28 17:56:40  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <ctype.h>

BEGIN_NCBI_SCOPE


extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim)
{
#if !defined(NCBI_USE_OLD_IOSTREAM)
    return getline(is, str, delim);
#else
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
#endif /* ndef!else NCBI_USE_OLD_IOSTREAM */
}


CNcbiOstrstreamToString::operator string(void) const
{
    SIZE_TYPE length = m_Out.pcount();
    if ( length == 0 )
        return string();
    const char* str = m_Out.str();
    m_Out.freeze(false);
    return string(str, length);
}

CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseStringConverter s)
{
    iterate ( string, c, s.m_String ) {
        out.put(char(toupper(*c)));
    }
    return out;
}

CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseStringConverter s)
{
    iterate ( string, c, s.m_String ) {
        out.put(char(tolower(*c)));
    }
    return out;
}

CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(toupper(*c)));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(tolower(*c)));
    }
    return out;
}


END_NCBI_SCOPE



// See in the header why it is outside of NCBI scope (SunPro bug workaround...)

#if defined(NCBI_USE_OLD_IOSTREAM)
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
#endif  /* NCBI_USE_OLD_IOSTREAM */

