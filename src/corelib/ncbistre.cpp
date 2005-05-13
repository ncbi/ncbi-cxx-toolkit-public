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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <ctype.h>

BEGIN_NCBI_SCOPE


#if defined(NCBI_USE_OLD_IOSTREAM)  ||  defined(NCBI_OS_DARWIN)
static CNcbiIstream& s_NcbiGetline(CNcbiIstream& is, string& str,
                                   char delim, char delim2)
{
    CT_INT_TYPE ch;
    char        buf[1024];
    SIZE_TYPE   pos = 0;

    str.erase();

    IOS_BASE::fmtflags f = is.flags();
    is.unsetf(IOS_BASE::skipws);
#ifdef NO_PUBSYNC
    if ( !is.ipfx(1) ) {
        is.flags(f);
        return is;
    }
#else
    CNcbiIstream::sentry s(is);
    if ( !s ) {
        is.clear(NcbiFailbit | is.rdstate());
        is.flags(f);
        return is;
    }
#endif

    SIZE_TYPE end = str.max_size();
    SIZE_TYPE i = 0;
    for (ch = is.rdbuf()->sbumpc();  !CT_EQ_INT_TYPE(ch, CT_EOF);
         ch = is.rdbuf()->sbumpc()) {
        i++;
        if (CT_TO_CHAR_TYPE(ch) == delim  ||  CT_TO_CHAR_TYPE(ch) == delim2)
            break;
        if (i == end) {
            is.clear(NcbiFailbit | is.rdstate());      
            break;
        }

        buf[pos++] = CT_TO_CHAR_TYPE(ch);
        if (pos == sizeof(buf)) {
            str.append(buf, pos);
            pos = 0;
        }
    }
    str.append(buf, pos);
    if (ch == EOF) 
        is.clear(NcbiEofbit | is.rdstate());      
    if ( !i )
        is.clear(NcbiFailbit | is.rdstate());      

#ifdef NO_PUBSYNC
    is.isfx();
#endif

    is.flags(f);
    return is;
}
#endif  /* NCBI_USE_OLD_IOSTREAM || NCBI_OS_DARWIN */

#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION < 300
#    define NCBI_COMPILER_GCC29x
#  endif
#endif

extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim)
{
#if defined(NCBI_USE_OLD_IOSTREAM)
    return s_NcbiGetline(is, str, delim, delim);
#elif defined(NCBI_COMPILER_GCC29x)
    // The code below is normally somewhat faster than this call,
    // which typically appends one character at a time to str;
    // however, it blows up when built with some GCC versions.
    return getline(is, str, delim);
#else
    char buf[1024];
    str.erase();
    while (is.good()) {
        CT_INT_TYPE nextc = is.get();
        if (CT_EQ_INT_TYPE(nextc, CT_EOF) 
            ||  CT_EQ_INT_TYPE(nextc, CT_TO_INT_TYPE(delim))) {
            break;
        }
        is.putback(nextc);
        is.get(buf, sizeof(buf), delim);
        str.append(buf, is.gcount());
    }
    if ( str.empty()  &&  is.eof() ) {
        is.setstate(NcbiFailbit);
    }
    return is;
#endif
}


// Platform-specific EndOfLine
const char* Endl(void)
{
#if   defined(NCBI_OS_MAC)
    static const char s_Endl[] = "\r";
#elif defined(NCBI_OS_MSWIN)
    static const char s_Endl[] = "\r\n";
#else /* assume UNIX-like EOLs */
    static const char s_Endl[] = "\n";
#endif
    return s_Endl;
}


// Get the next line taking into account platform specifics of End-of-Line
CNcbiIstream& NcbiGetlineEOL(CNcbiIstream& is, string& str)
{
#if   defined(NCBI_OS_MAC)
    NcbiGetline(is, str, '\r');
#elif defined(NCBI_OS_MSWIN)
    NcbiGetline(is, str, '\n');
    if (!str.empty()  &&  str[str.length()-1] == '\r')
        str.resize(str.length() - 1);
#elif defined(NCBI_OS_DARWIN)
    s_NcbiGetline(is, str, '\r', '\n');
#else /* assume UNIX-like EOLs */
    NcbiGetline(is, str, '\n');
#endif
    // special case -- an empty line
    if (is.fail()  &&  !is.eof()  &&  !is.gcount()  &&  str.empty())
        is.clear(is.rdstate() & ~NcbiFailbit);
    return is;
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
    ITERATE ( string, c, s.m_String ) {
        out.put(char(toupper(*c)));
    }
    return out;
}

CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
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


#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
CNcbiOstream& operator<<(CNcbiOstream& out, __int64 val)
{
    return (out << NStr::Int8ToString(val));
}
#  endif
#endif


static const char s_Hex[] = "0123456789ABCDEF";

string Printable(char c)
{
    string s;
    switch ( c ) {
    case '\0':  s = "\\0";   break;
    case '\\':  s = "\\\\";  break;
    case '\n':  s = "\\n";   break;
    case '\t':  s = "\\t";   break;
    case '\r':  s = "\\r";   break;
    case '\v':  s = "\\v";   break;
    default:
        {
            if ( isprint((unsigned char) c) ) {
                s = c;
            } else {
                s = "\\x";
                s += s_Hex[(unsigned char) c / 16];
                s += s_Hex[(unsigned char) c % 16];
            }
        }
    }
    return s;
}


inline
void WritePrintable(CNcbiOstream& out, char c)
{
    switch ( c ) {
    case '\0':  out.write("\\0",  2);  break;
    case '\\':  out.write("\\\\", 2);  break;
    case '\n':  out.write("\\n",  2);  break;
    case '\t':  out.write("\\t",  2);  break;
    case '\r':  out.write("\\r",  2);  break;
    case '\v':  out.write("\\v",  2);  break;
    default:
        {
            if ( isprint((unsigned char) c) ) {
                out.put(c);
            } else {
                out.write("\\x", 2);
                out.put(s_Hex[(unsigned char) c / 16]);
                out.put(s_Hex[(unsigned char) c % 16]);
            }
        }
    }
}

CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        WritePrintable(out, *c);
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        WritePrintable(out, *c);
    }
    return out;
}

#if defined(NCBI_COMPILER_WORKSHOP)
// We have to use two #if's here because KAI C++ cannot handle #if foo == bar
#  if (NCBI_COMPILER_VERSION == 530)
// The version that ships with the compiler is buggy.
// Here's a working (and simpler!) one.
template<>
istream& istream::read(char *s, streamsize n)
{
    sentry ipfx(*this, 1);

    try {
        if (rdbuf()->sgetc() == traits_type::eof()) {
            // Workaround for bug in sgetn.  *SIGH*.
            __chcount = 0;
            setstate(eofbit);
            return *this;
        }
        __chcount = rdbuf()->sgetn(s, n);
        if (__chcount == 0) {
            setstate(eofbit);
        } else if (__chcount < n) {
            setstate(eofbit | failbit);
        } else if (!ipfx) {
            setstate(failbit);
        } 
    } catch (...) {
        setstate(failbit);
        throw;
    }

    return *this;
}
#  endif  /* NCBI_COMPILER_VERSION == 530 */
#endif  /* NCBI_COMPILER_WORKSHOP */

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
    for (ch = is.rdbuf()->sbumpc();
         ch != EOF  &&  !isspace((unsigned char) ch);
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2005/05/13 11:39:51  ivanov
 * Define CNcbiOstream& operator<< __int64 for MSVC 6 only
 *
 * Revision 1.33  2005/05/12 15:07:22  lavr
 * Use explicit (unsigned char) conversion in <ctype.h>'s macros
 *
 * Revision 1.32  2004/06/07 14:40:13  ucko
 * s_NcbiGetline: always drop the string's old contents, even if the
 * stream has run out.
 *
 * Revision 1.31  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.30  2003/09/05 16:01:37  ivanov
 * Added forgotten eof() check before set FAILBIT in the NcbiGetline()
 *
 * Revision 1.29  2003/09/05 15:56:00  ivanov
 * Fix for R2.8 -- only set FAILBIT if no symbols have been read
 *
 * Revision 1.28  2003/09/05 15:22:26  ivanov
 * Fixed NcbiGetline() to correct handle last line in the input stream
 *
 * Revision 1.27  2003/08/27 18:58:22  ucko
 * NcbiGetline: revert to std::getline for GCC 2.9x, since the custom
 * version blows up mysteriously (at least in debug builds).
 *
 * Revision 1.26  2003/08/25 21:14:58  ucko
 * [s_]NcbiGetline: take care to append characters to str in bulk rather
 * than one at a time, which can be pretty inefficient.
 *
 * Revision 1.25  2003/08/19 17:06:12  ucko
 * Actually conditionalize the Windows-specific operator<<....
 *
 * Revision 1.24  2003/08/19 15:41:55  dicuccio
 * Added conditionally compiled prototype for operator<<(ostream&, __int64)
 *
 * Revision 1.23  2003/05/18 04:28:18  vakatov
 * Fix warning about "s_NcbiGetline()" being defined but not used sometimes
 *
 * Revision 1.22  2003/03/31 21:34:01  ucko
 * s_NCBIGetline: avoid eating initial whitespace, and use portability macros.
 *
 * Revision 1.21  2003/03/27 23:10:43  vakatov
 * Identation
 *
 * Revision 1.20  2003/03/26 21:18:54  kans
 * s_NcbiGetline takes two delimiters, NcbiGetlineEOL calls this if
 * NCBI_OS_DARWIN to handle both kinds of newlines
 *
 * Revision 1.19  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.18  2002/10/17 22:10:33  vakatov
 * C&P typo fixed
 *
 * Revision 1.17  2002/10/17 22:07:27  vakatov
 * + Endl() -- platform-specific EndOfLine
 *
 * Revision 1.16  2002/08/01 18:42:17  ivanov
 * + NcbiGetlineEOL() -- moved from ncbireg and renamed
 *
 * Revision 1.15  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.14  2001/10/15 19:48:23  vakatov
 * Use two #if's instead of "#if ... && ..." as KAI cannot handle #if x == y
 *
 * Revision 1.13  2001/09/06 19:35:14  ucko
 * WorkShop 5.3's implementation of istream::read is broken; provide one
 * that works.
 *
 * Revision 1.12  2001/04/11 20:14:31  vakatov
 * Printable() -- added the forgotten "break"s.
 * Printable(), WritePrintable() -- cast "char" to "unsigned char".
 *
 * Revision 1.11  2001/03/26 20:26:59  vakatov
 * Added "Printable" symbol conversions (by A.Grichenko)
 *
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
 * Added class Locase for printing strings to ostream with automatic conversion
 *
 * Revision 1.7  2000/12/12 14:20:36  vasilche
 * Added operator bool to CArgValue.
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
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

