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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 *
 */

/// @file ncbistre.hpp
/// NCBI C++ stream class wrappers for triggering between "new" and
/// "old" C++ stream libraries.


#include <corelib/ncbitype.h>
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
#  if defined(NCBI_COMPILER_ICC)  &&  defined(__GNUC__)  &&  !defined(__INTEL_CXXLIB_ICC)
#    define _BACKWARD_BACKWARD_WARNING_H 1
#    include <backward/strstream>
#  else
#    include <strstream>
#  endif
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

// Obsolete
#define SEEKOFF         PUBSEEKOFF

#include <string>
#include <stddef.h>


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


#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
// this is helper method for fstream classes only
// do not use it elsewhere
NCBI_XNCBI_EXPORT
wstring ncbi_Utf8ToWstring(const char *utf8);

class CNcbiIfstream : public IO_PREFIX::ifstream
{
public:
    CNcbiIfstream( ) {
    }
    explicit CNcbiIfstream(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::ifstream(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot) {
    }
    explicit CNcbiIfstream(
        const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::ifstream(_Filename,_Mode,_Prot) {
    }
 
    void open(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in,
        int _Prot = (int)IOS_BASE::_Openprot) {
        IO_PREFIX::ifstream::open(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot);
    }
    void open(const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in,
        int _Prot = (int)ios_base::_Openprot) {
        IO_PREFIX::ifstream::open(_Filename,_Mode,_Prot);
    }
};
#elif defined(NCBI_COMPILER_MSVC)
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

#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
class CNcbiOfstream : public IO_PREFIX::ofstream
{
public:
    CNcbiOfstream( ) {
    }
    explicit CNcbiOfstream(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::ofstream(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot) {
    }
    explicit CNcbiOfstream(
        const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::ofstream(_Filename,_Mode,_Prot) {
    }
 
    void open(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot) {
        IO_PREFIX::ofstream::open(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot);
    }
    void open(const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot) {
        IO_PREFIX::ofstream::open(_Filename,_Mode,_Prot);
    }
};
#elif defined(NCBI_COMPILER_MSVC)
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

#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
class CNcbiFstream : public IO_PREFIX::fstream
{
public:
    CNcbiFstream( ) {
    }
    explicit CNcbiFstream(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in | IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::fstream(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot) {
    }
    explicit CNcbiFstream(
        const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in | IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot
    ) : IO_PREFIX::fstream(_Filename,_Mode,_Prot) {
    }
 
    void open(
        const char *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in | IOS_BASE::out,
        int _Prot = (int)IOS_BASE::_Openprot) {
        IO_PREFIX::fstream::open(
            ncbi_Utf8ToWstring(_Filename).c_str(), _Mode, _Prot);
    }
    void open(const wchar_t *_Filename,
        IOS_BASE::openmode _Mode = IOS_BASE::in | IOS_BASE::out,
        int _Prot = (int)ios_base::_Openprot) {
        IO_PREFIX::fstream::open(_Filename,_Mode,_Prot);
    }
};
#elif defined(NCBI_COMPILER_MSVC)
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

// I/O manipulators (the list may be incomplete)
#define NcbiEndl                 IO_PREFIX::endl
#define NcbiEnds                 IO_PREFIX::ends
#define NcbiFlush                IO_PREFIX::flush

#define NcbiDec                  IO_PREFIX::dec
#define NcbiHex                  IO_PREFIX::hex
#define NcbiOct                  IO_PREFIX::oct
#define NcbiWs                   IO_PREFIX::ws

#define NcbiFixed                IO_PREFIX::fixed
#define NcbiScientific           IO_PREFIX::scientific

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


/// Platform-specific EndOfLine
NCBI_XNCBI_EXPORT
extern const char* Endl(void);

/// Read from "is" to "str" up to the delimiter symbol "delim" (or EOF)
NCBI_XNCBI_EXPORT
extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim,
                                 string::size_type* count = NULL);

/// Read from "is" to "str" up to any symbol contained within "delims" (or EOF)
NCBI_XNCBI_EXPORT
extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str,
                                 const string& delims,
                                 string::size_type* count = NULL);

/// Read from "is" to "str" the next line 
/// (taking into account platform specifics of End-of-Line)
NCBI_XNCBI_EXPORT
extern CNcbiIstream& NcbiGetlineEOL(CNcbiIstream& is, string& str,
                                    string::size_type* count = NULL);


/// Copy entire contents of stream "is" into "os".
/// @return
/// "true" if the operation was successful, "is" was read entirely,
/// and all of its contents had been written to "os";
/// "false" if either extraction from "is" or insertion into "os" have failed.
///
/// Note that upon successful completion, is.eof() may not always be true.
/// The call may throw exceptions only if they are enabled on the
/// respective stream(s).
///
/// Note that the call is an extension to the standard
/// ostream& ostream::operator<<(streambuf*),
/// which severely lacks error checking (esp. for partial write failures).
///
/// NOTE that the call (as well as the mentioned STL counterpart) provides
/// only a mechanism of delivering data to the destination "os" stream(buf);
/// and the successful return result does not generally guarantee that the
/// data have yet reached the physical destination.  Other "os"-specific API
/// must be performed to assure the data integrity at the receiving device;
/// such as checking for errors after doing a "close()" on an ofstream "os".
/// E.g. data uploading into the Toolkit FTP stream must be finalized with a
/// read for the byte count delivered;  otherwise, it may not work correctly.
/// @sa
///   CConn_IOStream
NCBI_XNCBI_EXPORT
extern bool NcbiStreamCopy(CNcbiOstream& os, CNcbiIstream& is);


/// Same as NcbiStreamCopy() but throws an CCoreException when copy fails.
/// @sa
///   NcbiStreamCopy, CCoreException
NCBI_XNCBI_EXPORT
extern void NcbiStreamCopyThrow(CNcbiOstream& os, CNcbiIstream& is);


/// Input the entire contents of an istream into a string (NULL causes drain).
///
/// @param pos
///   Where in "*str" to begin saving data (ignored when "str" == NULL).
/// @return
///   True if copy was successful (i.e. "is" had reached EOF), false otherwise.
/// @note
///   If "str" != NULL, then "str->size() >= pos" upon return.
/// @sa
///   NcbiStreamCopy
NCBI_XNCBI_EXPORT
extern bool NcbiStreamToString(string* str, CNcbiIstream& is, size_t pos = 0);


/// Compare stream contents in binary form.
///
/// @param is1
///   First stream to compare.
/// @param is2
///   Second stream to compare.
/// @return
///   TRUE if streams content is equal; FALSE otherwise.
NCBI_XNCBI_EXPORT
extern bool NcbiStreamCompare(CNcbiIstream& is1, CNcbiIstream& is2);

/// Mode to compare streams in text form.
enum ECompareTextMode {
    /// Skip end-of-line characters ('\r' and '\n')
    eCompareText_IgnoreEol,
    ///< Skip white spaces (in terms of isspace(), including end-of-line)
    eCompareText_IgnoreWhiteSpace
};

/// Compare stream contents in text form.
///
/// @param is1
///   First stream to compare.
/// @param is2
///   Second stream to compare.
/// @param mode
///   Type of white space characters to ignore.
/// @param buf_size
///   Size of buffer to read stream.
///   Zero value means using default buffer size.
/// @return
///   TRUE if streams content is equal; FALSE otherwise.
NCBI_XNCBI_EXPORT
extern bool NcbiStreamCompareText(CNcbiIstream& is1, CNcbiIstream& is2,
                                  ECompareTextMode mode, size_t buf_size = 0);

/// Compare stream content with string in text form.
///
/// @param is
///   Stream to compare.
/// @param str
///   String to compare.
/// @param mode
///   Type of white space characters to ignore.
/// @param buf_size
///   Size of buffer to read stream.
///   Zero value means using default buffer size.
/// @return
///   TRUE if stream and string content is equal; FALSE otherwise.
NCBI_XNCBI_EXPORT
extern bool NcbiStreamCompareText(CNcbiIstream& is, const string& str,
                                  ECompareTextMode mode, size_t buf_size = 0);


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


#ifdef NCBI_COMPILER_MIPSPRO
/// Special workaround for MIPSPro 1-byte look-ahead issues
class CMIPSPRO_ReadsomeTolerantStreambuf : public CNcbiStreambuf
{
public:
    /// NB: Do not use these two ugly, weird, ad-hoc methods, ever!!!
    void MIPSPRO_ReadsomeBegin(void)
    {
        if (!m_MIPSPRO_ReadsomeGptrSetLevel++)
            m_MIPSPRO_ReadsomeGptr = gptr();
    }
    void MIPSPRO_ReadsomeEnd  (void)
    {
        --m_MIPSPRO_ReadsomeGptrSetLevel;
    }
protected:
    CMIPSPRO_ReadsomeTolerantStreambuf() : m_MIPSPRO_ReadsomeGptrSetLevel(0) {}
    
    const CT_CHAR_TYPE* m_MIPSPRO_ReadsomeGptr;
    unsigned int        m_MIPSPRO_ReadsomeGptrSetLevel;
};
#endif // NCBI_COMPILER_MIPSPRO


/// Convert stream position to 64-bit int
///
/// On most systems stream position is a structure,
/// this function converts it to plain numeric value.
///
/// @sa NcbiInt8ToStreampos
///
inline
Int8 NcbiStreamposToInt8(CT_POS_TYPE stream_pos)
{
    return (CT_OFF_TYPE)(stream_pos - (CT_POS_TYPE)((CT_OFF_TYPE)0));
}


/// Convert plain numeric stream position (offset) into
/// stream position usable with STL stream library.
///
/// @sa NcbiStreamposToInt8
inline
CT_POS_TYPE NcbiInt8ToStreampos(Int8 pos)
{
    return (CT_POS_TYPE)((CT_OFF_TYPE) 0) + (CT_OFF_TYPE)(pos);
}


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
// Note: there is no requirement to put '\0' char at the end of buffer;
//       there is no need to explicitly "unfreeze" the "out" stream.

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


/// Utility class for automatic conversion of strings to all uppercase letters.
/// Sample usage:
///    out << "Original:  \"" << str         << '"' << endl;
///    out << "Uppercase: \"" << Upcase(str) << '"' << endl;

class NCBI_XNCBI_EXPORT CUpcaseStringConverter
{
public:
    explicit CUpcaseStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CUpcaseCharPtrConverter
{
public:
    explicit CUpcaseCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};


/// Utility class for automatic conversion of strings to all lowercase letters.
/// Sample usage:
///    out << "Original:  \"" << str         << '"' << endl;
///    out << "Lowercase: \"" << Locase(str) << '"' << endl;

class NCBI_XNCBI_EXPORT CLocaseStringConverter
{
public:
    explicit CLocaseStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CLocaseCharPtrConverter
{
public:
    explicit CLocaseCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};


/// Utility class for automatic conversion of strings (that may contain
/// non-graphical characters) to a safe "printable" form.
/// The safe printable form utilizes '\'-quoted special sequences, as well
/// as either contracted or full 3-digit octal representation of non-printable
/// characters, always making sure there is no ambiguity in the string
/// interpretation (so that if the printed form is used back in a C program,
/// it will be equivalent to the original string).
/// Sample usage:
///   out << "Printable: \"" << Printable(str) << '"' << endl;

class NCBI_XNCBI_EXPORT CPrintableStringConverter
{
public:
    explicit CPrintableStringConverter(const string& s) : m_String(s) { }
    const string& m_String;
};

class NCBI_XNCBI_EXPORT CPrintableCharPtrConverter
{
public:
    explicit CPrintableCharPtrConverter(const char* s) : m_String(s) { }
    const char* m_String;
};


/* @} */


/// Convert one single character to a "printable" form.
/// A "printable" form is one of well-known C-style backslash-quoted sequences
/// ('\0', '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\', '\'', '\"'),
/// or '\xXX' for other non-printable characters (per isprint()), or a
/// graphical representation of 'c' as an ASCII character.
/// @note  *DO NOT USE* to convert strings!  Because of the '\xXX' notation,
/// such conversions can result in ambiguity (e.g. "\xAAA" is a valid
/// _single_-character string per the standard).
NCBI_DEPRECATED
NCBI_XNCBI_EXPORT extern string Printable(char c);


inline
char Upcase(char c)
{
    return static_cast<char>(toupper((unsigned char) c));
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
    return static_cast<char>(tolower((unsigned char) c));
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


/////////////////////////////////////////////////////////////////////////////
///
/// Helper functions to read plain-text data streams.
/// It understands Byte Order Mark (BOM) and converts the input if needed.
///
/// See clause 13.6 in
///   http://www.unicode.org/unicode/uni2book/ch13.pdf
/// and also
///   http://unicode.org/faq/utf_bom.html#BOM
///
/// @sa ReadIntoUtf8, GetTextEncodingForm
enum EEncodingForm {
    /// Stream has no BOM.
    eEncodingForm_Unknown,
    /// Stream has no BOM.
    eEncodingForm_ISO8859_1,
    /// Stream has no BOM.
    eEncodingForm_Windows_1252,
    /// Stream has UTF8 BOM.
    eEncodingForm_Utf8,
    /// Stream has UTF16 BOM. Byte order is native for this OS
    eEncodingForm_Utf16Native,
    /// Stream has UTF16 BOM. Byte order is nonnative for this OS
    eEncodingForm_Utf16Foreign
};


/// How to read the text if the encoding form is not known (i.e. passed
/// "eEncodingForm_Unknown" and the stream does not have BOM too)
///
/// @sa ReadIntoUtf8
enum EReadUnknownNoBOM {
    /// Read the text "as is" (raw octal data). The read data can then
    /// be accessed using the regular std::string API (rather than the
    /// CStringUTF8 one).
    eNoBOM_RawRead,

    /// Try to guess the text's encoding form.
    ///
    /// @note
    ///   In this case the encoding is a guesswork, which is not necessarily
    ///   correct. If the guess is wrong then the data may be distorted on
    ///   read. Use CStringUTF8::IsValid() to verify that guess. If it
    ///   does not verify, then the read data can be accessed using the
    ///   regular std::string API (rather than the CStringUTF8 one).
    eNoBOM_GuessEncoding
};


/// Read all input data from stream and try convert it into UTF8 string.
///
/// @param input
///   Input text stream
/// @param result
///   UTF8 string (but it can be a raw octet string if the encoding is unknown)
/// @param what_if_no_bom
///   What to do if the 'encoding_form' is passed "eEncodingForm_Unknown" and
///   the BOM is not detected in the stream
/// @return
///   The encoding as detected based on the BOM
///   ("eEncodingForm_Unknown" if there was no BOM).
NCBI_XNCBI_EXPORT
EEncodingForm ReadIntoUtf8(
    CNcbiIstream&     input,
    CStringUTF8*      result,
    EEncodingForm     encoding_form  = eEncodingForm_Unknown,
    EReadUnknownNoBOM what_if_no_bom = eNoBOM_GuessEncoding
);


/// Whether to discard BOM or to keep it in the input stream
///
/// @sa GetTextEncodingForm
enum EBOMDiscard {
    eBOM_Discard,  ///< Discard the read BOM bytes
    eBOM_Keep      ///< Push the read BOM bytes back into the input stream
};


/// Detect if the stream has BOM.
///
/// @param input
///   Input stream
/// @param discard_bom
///   Whether to discard the read BOM bytes or to push them back to the stream
///
/// NOTE:  If the function needs to push back more than one char then it uses
///        CStreamUtils::Pushback().
/// @sa CStreamUtils::Pushback()
NCBI_XNCBI_EXPORT
EEncodingForm GetTextEncodingForm(CNcbiIstream& input,
                                  EBOMDiscard   discard_bom);


#include <corelib/ncbi_base64.h>


END_NCBI_SCOPE


// Provide formatted I/O of standard C++ "string" by "old-fashioned" IOSTREAMs
// NOTE:  these must have been inside the _NCBI_SCOPE and without the
//        "ncbi::" and "std::" prefixes, but there is some bug in SunPro 5.0...
#if defined(NCBI_USE_OLD_IOSTREAM)
extern NCBI_NS_NCBI::CNcbiOstream& operator<<(NCBI_NS_NCBI::CNcbiOstream& os,
                                              const NCBI_NS_STD::string& str);
extern NCBI_NS_NCBI::CNcbiIstream& operator>>(NCBI_NS_NCBI::CNcbiIstream& is,
                                              NCBI_NS_STD::string& str);
#endif // NCBI_USE_OLD_IOSTREAM


#endif /* NCBISTRE__HPP */
