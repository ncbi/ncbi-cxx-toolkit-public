#ifndef CORELIB___NCBISTRE__HPP
#define CORELIB___NCBISTRE__HPP

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
 *
 */

/// @file ncbistre.hpp
/// NCBI C++ stream class wrappers for triggering between "new" and
/// "old" C++ stream libraries.


#include <corelib/ncbistl.hpp>


/// Determine which iostream library to use, include appropriate
/// headers, and #define specific preprocessor variables.
/// The default is the new(template-based, std::) one.

#if !defined(HAVE_IOSTREAM)  &&  !defined(NCBI_USE_OLD_IOSTREAM)
#  define NCBI_USE_OLD_IOSTREAM
#endif

#if defined(HAVE_IOSTREAM_H)  &&  defined(NCBI_USE_OLD_IOSTREAM)
#  include <iostream.h>
#  include <fstream.h>
#  if defined(HAVE_STRSTREA_H)
#    include <strstrea.h>
#  else
#    include <strstream.h>
#  endif
#  include <iomanip.h>
#  define IO_PREFIX
#  define IOS_BASE      ::ios
#  define IOS_PREFIX    ::ios
#  define PUBSYNC       sync
#  define PUBSEEKPOS    seekpos
#  define PUBSEEKOFF    seekoff

#elif defined(HAVE_IOSTREAM)
#  if defined(NCBI_USE_OLD_IOSTREAM)
#    undef NCBI_USE_OLD_IOSTREAM
#  endif
#  if defined(NCBI_COMPILER_GCC)
#    if NCBI_COMPILER_VERSION < 300
#      define NO_PUBSYNC
#    elif NCBI_COMPILER_VERSION >= 310
// Don't bug us about including <strstream>.
#      define _CPP_BACKWARD_BACKWARD_WARNING_H 1
#      define _BACKWARD_BACKWARD_WARNING_H 1
#    endif
#  endif
#  include <iostream>
#  include <fstream>
#  include <strstream>
#  include <iomanip>
#  if defined(HAVE_NO_STD)
#    define IO_PREFIX
#  else
#    define IO_PREFIX   NCBI_NS_STD
#  endif
#  if defined HAVE_NO_IOS_BASE
#    define IOS_BASE    IO_PREFIX::ios
#  else
#    define IOS_BASE    IO_PREFIX::ios_base
#  endif
#  define IOS_PREFIX    IO_PREFIX::ios

#  ifdef NO_PUBSYNC
#    define PUBSYNC     sync
#    define PUBSEEKOFF  seekoff
#    define PUBSEEKPOS  seekpos
#  else
#    define PUBSYNC     pubsync
#    define PUBSEEKOFF  pubseekoff
#    define PUBSEEKPOS  pubseekpos
#  endif

#else
#  error "Neither <iostream> nor <iostream.h> can be found!"
#endif

// obsolete
#define SEEKOFF         PUBSEEKOFF


#include <string>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

/** @addtogroup Stream
 *
 * @{
 */

// I/O classes

/// Portable alias for streampos.
typedef IO_PREFIX::streampos     CNcbiStreampos;

/// Portable alias for streamoff.
typedef IO_PREFIX::streamoff     CNcbiStreamoff;

/// Portable alias for ios.
typedef IO_PREFIX::ios           CNcbiIos;

/// Portable alias for streambuf.
typedef IO_PREFIX::streambuf     CNcbiStreambuf;

/// Portable alias for istream.
typedef IO_PREFIX::istream       CNcbiIstream;

/// Portable alias for ostream.
typedef IO_PREFIX::ostream       CNcbiOstream;

/// Portable alias for iostream.
typedef IO_PREFIX::iostream      CNcbiIostream;

/// Portable alias for strstreambuf.
typedef IO_PREFIX::strstreambuf  CNcbiStrstreambuf;

/// Portable alias for istrstream.
typedef IO_PREFIX::istrstream    CNcbiIstrstream;

/// Portable alias for ostrstream.
typedef IO_PREFIX::ostrstream    CNcbiOstrstream;

/// Portable alias for strstream.
typedef IO_PREFIX::strstream     CNcbiStrstream;

/// Portable alias for filebuf.
typedef IO_PREFIX::filebuf       CNcbiFilebuf;

#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
class CNcbiIfstream : public IO_PREFIX::ifstream
{
public:
    CNcbiIfstream() : m_Fp(0)
    {
    }

    explicit CNcbiIfstream(const char* s,
                           IOS_BASE::openmode mode = IOS_BASE::in)
    {
        fastopen(s, mode);
    }

    void fastopen(const char* s, IOS_BASE::openmode mode = IOS_BASE::in)
    {
        if (is_open()  ||  !(m_Fp = __Fiopen(s, mode | in)))
            setstate(failbit);
        else
            (void) new (rdbuf()) basic_filebuf<char, char_traits<char> >(m_Fp);
    }

    virtual ~CNcbiIfstream(void)
    {
        if (m_Fp)
            fclose(m_Fp);
    }
private:
    FILE* m_Fp;
};
#  else
/// Portable alias for ifstream.
typedef IO_PREFIX::ifstream      CNcbiIfstream;
#  endif
#else
/// Portable alias for ifstream.
typedef IO_PREFIX::ifstream      CNcbiIfstream;
#endif

#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
class CNcbiOfstream : public IO_PREFIX::ofstream
{
public:
    CNcbiOfstream() : m_Fp(0)
    {
    }

    explicit CNcbiOfstream(const char* s,
                           IOS_BASE::openmode mode = IOS_BASE::out)
    {
        fastopen(s, mode);
    }

    void fastopen(const char* s, IOS_BASE::openmode mode = IOS_BASE::out)
    {
        if (is_open()  ||  !(m_Fp = __Fiopen(s, mode | out)))
            setstate(failbit);
        else
            (void) new (rdbuf()) basic_filebuf<char, char_traits<char> >(m_Fp);
    }

    virtual ~CNcbiOfstream(void)
    {
        if (m_Fp)
            fclose(m_Fp);
    }
private:
    FILE* m_Fp;
};
#  else
/// Portable alias for ofstream.
typedef IO_PREFIX::ofstream      CNcbiOfstream;
#  endif
#else
/// Portable alias for ofstream.
typedef IO_PREFIX::ofstream      CNcbiOfstream;
#endif

#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
class CNcbiFstream : public IO_PREFIX::fstream
{
public:
    CNcbiFstream() : m_Fp(0)
    {
    }

    explicit CNcbiFstream(const char* s,
                          IOS_BASE::openmode
                          mode = IOS_BASE::in | IOS_BASE::out)
    {
        fastopen(s, mode);
    }

    void fastopen(const char* s, IOS_BASE::openmode
                  mode = IOS_BASE::in | IOS_BASE::out)
    {
        if (is_open()  ||  !(m_Fp = __Fiopen(s, mode)))
            setstate(failbit);
        else
            (void) new (rdbuf()) basic_filebuf<char, char_traits<char> >(m_Fp);
    }

    virtual ~CNcbiFstream(void)
    {
        if (m_Fp)
            fclose(m_Fp);
    }
private:
    FILE* m_Fp;
};
#  else
/// Portable alias for fstream.
typedef IO_PREFIX::fstream       CNcbiFstream;
#  endif
#else
/// Portable alias for fstream.
typedef IO_PREFIX::fstream       CNcbiFstream;
#endif

// Standard I/O streams
#define NcbiCin                  IO_PREFIX::cin
#define NcbiCout                 IO_PREFIX::cout
#define NcbiCerr                 IO_PREFIX::cerr
#define NcbiClog                 IO_PREFIX::clog

// I/O manipulators
#define NcbiEndl                 IO_PREFIX::endl
#define NcbiEnds                 IO_PREFIX::ends
#define NcbiFlush                IO_PREFIX::flush
#define NcbiDec                  IO_PREFIX::dec
#define NcbiHex                  IO_PREFIX::hex
#define NcbiOct                  IO_PREFIX::oct
#define NcbiWs                   IO_PREFIX::ws

#define NcbiSetbase              IO_PREFIX::setbase
#define NcbiResetiosflags        IO_PREFIX::resetiosflags
#define NcbiSetiosflags          IO_PREFIX::setiosflags
#define NcbiSetfill              IO_PREFIX::setfill
#define NcbiSetprecision         IO_PREFIX::setprecision
#define NcbiSetw                 IO_PREFIX::setw

// I/O state
#define NcbiGoodbit              IOS_PREFIX::goodbit
#define NcbiEofbit               IOS_PREFIX::eofbit
#define NcbiFailbit              IOS_PREFIX::failbit
#define NcbiBadbit               IOS_PREFIX::badbit
#define NcbiHardfail             IOS_PREFIX::hardfail



// Platform-specific EndOfLine
NCBI_XNCBI_EXPORT
extern const char* Endl(void);

// Read from "is" to "str" up to the delimiter symbol "delim"(or EOF)
NCBI_XNCBI_EXPORT
extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim);

// Read from "is" to "str" the next line 
// (taking into account platform specifics of End-of-Line)
NCBI_XNCBI_EXPORT
extern CNcbiIstream& NcbiGetlineEOL(CNcbiIstream& is, string& str);


// "char_traits" may not be defined(e.g. EGCS egcs-2.91.66)
#if defined(HAVE_NO_CHAR_TRAITS)
#  define CT_INT_TYPE      int
#  define CT_CHAR_TYPE     char
#  define CT_POS_TYPE      CNcbiStreampos
#  define CT_OFF_TYPE      CNcbiStreamoff
#  define CT_EOF           EOF
inline CT_INT_TYPE  ct_not_eof(CT_INT_TYPE i) {
    return i == CT_EOF ? 0 : i;
}
#  define CT_NOT_EOF       ct_not_eof
inline CT_INT_TYPE  ct_to_int_type(CT_CHAR_TYPE c) {
    return (unsigned char)c;
}
#  define CT_TO_INT_TYPE   ct_to_int_type
inline CT_CHAR_TYPE ct_to_char_type(CT_INT_TYPE i) {
    return (unsigned char)i;
}
#  define CT_TO_CHAR_TYPE  ct_to_char_type
inline bool ct_eq_int_type(CT_INT_TYPE i1, CT_INT_TYPE i2) {
    return i1 == i2;
}
#  define CT_EQ_INT_TYPE   ct_eq_int_type
#else  /* HAVE_NO_CHAR_TRAITS */
#  define CT_INT_TYPE      NCBI_NS_STD::char_traits<char>::int_type
#  define CT_CHAR_TYPE     NCBI_NS_STD::char_traits<char>::char_type
#  define CT_POS_TYPE      NCBI_NS_STD::char_traits<char>::pos_type
#  define CT_OFF_TYPE      NCBI_NS_STD::char_traits<char>::off_type
#  define CT_EOF           NCBI_NS_STD::char_traits<char>::eof()
#  define CT_NOT_EOF       NCBI_NS_STD::char_traits<char>::not_eof
#  define CT_TO_INT_TYPE   NCBI_NS_STD::char_traits<char>::to_int_type
#  define CT_TO_CHAR_TYPE  NCBI_NS_STD::char_traits<char>::to_char_type
#  define CT_EQ_INT_TYPE   NCBI_NS_STD::char_traits<char>::eq_int_type
#endif /* HAVE_NO_CHAR_TRAITS */

// CNcbiOstrstreamToString class helps to convert CNcbiOstream buffer to string
// Sample usage:
/*
string GetString(void)
{
    CNcbiOstrstream buffer;
    buffer << "some text";
    return CNcbiOstrstreamToString(buffer);
}
*/
// Note: there is no requirements to put '\0' char at the end of buffer

class NCBI_XNCBI_EXPORT CNcbiOstrstreamToString
{
    CNcbiOstrstreamToString(const CNcbiOstrstreamToString&);
    CNcbiOstrstreamToString& operator= (const CNcbiOstrstreamToString&);
public:
    CNcbiOstrstreamToString(CNcbiOstrstream& out)
        : m_Out(out)
        {
        }
    operator string(void) const;
private:
    CNcbiOstrstream& m_Out;
};


// utility class for automatic conversion of strings to uppercase letters
// sample usage:
//    out << "Original:  \"" << str << "\"\n";
//    out << "Uppercase: \"" << Upcase(str) << "\"\n";
// utility class for automatic conversion of strings to lowercase letters
// sample usage:
//    out << "Original:  \"" << str << "\"\n";
//    out << "Lowercase: \"" << Locase(str) << "\"\n";

class NCBI_XNCBI_EXPORT CUpcaseStringConverter
{
public:
    CUpcaseStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CUpcaseCharPtrConverter
{
public:
    CUpcaseCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};

class NCBI_XNCBI_EXPORT CLocaseStringConverter
{
public:
    CLocaseStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CLocaseCharPtrConverter
{
public:
    CLocaseCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};

class NCBI_XNCBI_EXPORT CPrintableStringConverter
{
public:
    CPrintableStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CPrintableCharPtrConverter
{
public:
    CPrintableCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};


/* @} */


inline
char Upcase(char c)
{
    return static_cast<char>(toupper(c));
}

inline
CUpcaseStringConverter Upcase(const string& s)
{
    return CUpcaseStringConverter(s);
}

inline
CUpcaseCharPtrConverter Upcase(const char* s)
{
    return CUpcaseCharPtrConverter(s);
}

inline
char Locase(char c)
{
    return static_cast<char>(tolower(c));
}

inline
CLocaseStringConverter Locase(const string& s)
{
    return CLocaseStringConverter(s);
}

inline
CLocaseCharPtrConverter Locase(const char* s)
{
    return CLocaseCharPtrConverter(s);
}

NCBI_XNCBI_EXPORT
extern string Printable(char c);

inline
CPrintableStringConverter Printable(const string& s)
{
    return CPrintableStringConverter(s);
}

inline
CPrintableCharPtrConverter Printable(const char* s)
{
    return CPrintableCharPtrConverter(s);
}

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseStringConverter s);

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseCharPtrConverter s);

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseStringConverter s);

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseCharPtrConverter s);

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableStringConverter s);

NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableCharPtrConverter s);

#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, __int64 val);
#  endif
#endif


END_NCBI_SCOPE


// Provide formatted I/O of standard C++ "string" by "old-fashioned" IOSTREAMs
// NOTE:  these must have been inside the _NCBI_SCOPE and without the
//        "ncbi::" and "std::" prefixes, but there is some bug in SunPro 5.0...
#if defined(NCBI_USE_OLD_IOSTREAM)
extern NCBI_NS_NCBI::CNcbiOstream& operator<<(NCBI_NS_NCBI::CNcbiOstream& os,
                                              const NCBI_NS_STD::string& str);
extern NCBI_NS_NCBI::CNcbiIstream& operator>>(NCBI_NS_NCBI::CNcbiIstream& is,
                                              NCBI_NS_STD::string& str);
#endif


/*
 * ===========================================================================
 * $Log$
 * Revision 1.42  2005/05/13 11:39:32  ivanov
 * Define CNcbiOstream& operator<< __int64 for MSVC 6 only
 *
 * Revision 1.41  2004/08/17 14:34:51  dicuccio
 * Export Printable()
 *
 * Revision 1.40  2004/04/26 14:45:08  ucko
 * Continue to suppress warnings about strstream in GCC 3.4.0, which uses
 * a slightly different include guard.
 *
 * Revision 1.39  2003/11/26 15:21:08  lavr
 * Do ad-hoc fstream fix conditionally for MSVC 6 only
 *
 * Revision 1.38  2003/11/18 11:57:46  siyan
 * Changed so @addtogroup does not cross namespace boundary
 *
 * Revision 1.37  2003/11/05 13:41:14  lavr
 * Enforce direction in CNcbiIfstream and CNcbiOfstream for MSVC
 *
 * Revision 1.36  2003/10/20 15:27:17  lavr
 * Implementations of CNcbiFstreams that forcedly turn on stream
 * buffering in MSVC (caution: very kludgy and ad hoc solution)
 *
 * Revision 1.35  2003/08/24 14:32:43  siyan
 * Added CORELIB___ prefix to the inclusion macros.
 *
 * Revision 1.34  2003/08/24 02:18:40  siyan
 * Documentation changes.
 *
 * Revision 1.33  2003/08/19 15:41:30  dicuccio
 * Added conditionally compiled prototype for operator<<(ostream&, __int64)
 *
 * Revision 1.32  2003/04/01 19:18:29  siyan
 * Added doxygen support
 *
 * Revision 1.31  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.30  2002/10/17 22:06:37  vakatov
 * + Endl() -- platform-specific EndOfLine
 *
 * Revision 1.29  2002/08/16 17:53:54  lavr
 * PUBSYNC, PUBSEEK* macros; SEEKOFF obsoleted; some formatting done
 *
 * Revision 1.28  2002/08/01 18:42:48  ivanov
 * + NcbiGetlineEOL() -- moved from ncbireg and renamed
 *
 * Revision 1.27  2002/05/22 17:58:20  ucko
 * Suppress GCC 3.1 warnings about <strstream>.
 *
 * Revision 1.26  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.25  2001/08/15 20:53:00  juran
 * Heed warnings.
 *
 * Revision 1.24  2001/05/17 14:54:27  lavr
 * Typos corrected
 *
 * Revision 1.23  2001/03/26 20:26:57  vakatov
 * Added "Printable" symbol conversions (by A.Grichenko)
 *
 * Revision 1.22  2001/03/26 18:35:38  lavr
 * CT_* macros aligned in a pretty way
 *
 * Revision 1.21  2001/03/24 00:32:59  lavr
 * Macros added: CT_TO_INT_TYPE, CT_TO_CHAR_TYPE, CT_EQ_INT_TYPE
 *
 * Revision 1.20  2000/12/15 15:36:30  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * Revision 1.19  2000/12/12 14:39:46  vasilche
 * Added class Locase for printing strings to ostream with automatic conversion
 *
 * Revision 1.18  2000/12/12 14:20:14  vasilche
 * Added operator bool to CArgValue.
 * Added standard typedef element_type to CRef<> and CConstRef<>.
 * Macro iterate() now calls method end() only once and uses temporary variable
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
 *
 * Revision 1.17  1999/12/28 18:55:25  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.16  1999/11/09 20:57:03  vasilche
 * Fixed exception with conversion empty strstream to string.
 *
 * Revision 1.15  1999/10/21 15:44:18  vasilche
 * Added helper class CNcbiOstreamToString to convert CNcbiOstrstream buffer
 * to string.
 *
 * Revision 1.14  1999/06/08 21:34:35  vakatov
 * #HAVE_NO_CHAR_TRAITS::  handle the case of missing "std::char_traits::"
 *
 * Revision 1.13  1999/05/10 14:26:07  vakatov
 * Fixes to compile and link with the "egcs" C++ compiler under Linux
 *
 * Revision 1.12  1999/05/06 23:02:38  vakatov
 * Use the new(template-based, std::) stream library by default
 *
 * Revision 1.11  1998/12/28 17:56:29  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.10  1998/12/03 18:56:12  vakatov
 * minor fixes
 *
 * Revision 1.9  1998/12/03 16:38:32  vakatov
 * Added aux. function "Getline()" to read from "istream" to a "string"
 * Adopted standard I/O "string" <--> "istream" for old-fashioned streams
 *
 * Revision 1.8  1998/11/26 00:27:05  vakatov
 * Added <iomanip[.h]> and relevant #define's NcbiXXX
 *
 * Revision 1.7  1998/11/06 22:42:39  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * ==========================================================================
 */

#endif /* NCBISTRE__HPP */
