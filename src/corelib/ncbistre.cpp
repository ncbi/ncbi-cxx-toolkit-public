/* $Id$
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
#include <corelib/stream_utils.hpp>
#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)

CNcbiIfstream::CNcbiIfstream(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
 : IO_PREFIX::ifstream(_T_XCSTRING(_Filename), _Mode, _Prot)
{
}
void CNcbiIfstream::open(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
{
    IO_PREFIX::ifstream::open(_T_XCSTRING(_Filename), _Mode, _Prot);
}


CNcbiOfstream::CNcbiOfstream(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
 : IO_PREFIX::ofstream(_T_XCSTRING(_Filename), _Mode, _Prot)
{
}
void CNcbiOfstream::open(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
{
    IO_PREFIX::ofstream::open(_T_XCSTRING(_Filename), _Mode, _Prot);
}


CNcbiFstream::CNcbiFstream(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
 : IO_PREFIX::fstream(_T_XCSTRING(_Filename), _Mode, _Prot)
{
}
void CNcbiFstream::open(
    const char *_Filename, IOS_BASE::openmode _Mode, int _Prot)
{
    IO_PREFIX::fstream::open(_T_XCSTRING(_Filename), _Mode, _Prot);
}

#endif


CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, const string& delims)
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
        is.setstate(NcbiFailbit);
        return is;
    }
#else
    CNcbiIstream::sentry s(is);
    if ( !s ) {
        is.flags(f);
        is.setstate(NcbiFailbit);
        return is;
    }
#endif
    _ASSERT( is.good() );

    SIZE_TYPE size = 0;
    SIZE_TYPE max_size = str.max_size();
    IOS_BASE::iostate iostate = NcbiGoodbit/*0*/;
    for (;;) {
        ch = is.rdbuf()->sbumpc();
        if ( CT_EQ_INT_TYPE(ch, CT_EOF) ) {
            iostate = NcbiEofbit;
            break;
        }
        SIZE_TYPE delim_pos = delims.find(CT_TO_CHAR_TYPE(ch));
        if (delim_pos != NPOS) {
            // Special case -- if two different delimiters are back to
            // back and in the same order as in delims, treat them as
            // a single delimiter (necessary for correct handling of
            // DOS/MAC-style CR/LF endings).
            ch = is.rdbuf()->sgetc();
            if (!CT_EQ_INT_TYPE(ch, CT_EOF)
                &&  delims.find(CT_TO_CHAR_TYPE(ch), delim_pos + 1) != NPOS) {
                is.rdbuf()->sbumpc();
            }
            break;
        }
        if (size == max_size) {
            CT_INT_TYPE pc = is.rdbuf()->sungetc();
            iostate = CT_EQ_INT_TYPE(pc, ch) ? NcbiFailbit : NcbiBadbit;
            break;
        }

        buf[pos++] = CT_TO_CHAR_TYPE(ch);
        if (pos == sizeof(buf)) {
            str.append(buf, pos);
            pos  = 0;
        }
        size++;
    }
    if (pos > 0)
        str.append(buf, pos);

#ifdef NO_PUBSYNC
    is.isfx();
#endif
    is.flags(f);
    if (iostate  &&  (iostate != NcbiEofbit  ||  str.empty()))
        is.clear(iostate);
    return is;
}


#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION < 300
#    define NCBI_COMPILER_GCC29x
#  endif
#endif

extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim)
{
#if defined(NCBI_USE_OLD_IOSTREAM)
    return NcbiGetline(is, str, string(1, delim));
#elif defined(NCBI_COMPILER_GCC29x)
    // The code below is normally somewhat faster than this call,
    // which typically appends only one character to "str" at a time;
    // however, it blows up when is built with some GCC versions.
    getline(is, str, delim);
#else
    str.erase();

    if ( !is.good() ) {
        is.setstate(NcbiFailbit);
        return is;
    }

    char buf[1024];
    SIZE_TYPE size = 0;
    SIZE_TYPE max_size = str.max_size();
    do {
        CT_INT_TYPE nextc = is.get();
        if (CT_EQ_INT_TYPE(nextc, CT_EOF) 
            ||  CT_EQ_INT_TYPE(nextc, CT_TO_INT_TYPE(delim))) {
            break;
        }
        if ( !is.unget() )
            break;
        if (size == max_size) {
            is.clear(NcbiFailbit);
            break;
        }
        SIZE_TYPE n = max_size - size;
        is.get(buf, n < sizeof(buf) ? n : sizeof(buf), delim);
        n = (size_t) is.gcount();
        str.append(buf, n);
        size += n;
        _ASSERT(size == str.length());
    } while ( is.good() );
#endif

    if (is.rdstate() == NcbiEofbit  &&  !str.empty())
        is.clear();
    return is;
}


// Platform-specific EndOfLine
const char* Endl(void)
{
#if defined(NCBI_OS_MSWIN)
    static const char s_Endl[] = "\r\n";
#else /* assume UNIX-like EOLs */
    static const char s_Endl[] = "\n";
#endif
    return s_Endl;
}


// Get a line taking into account platform-specific of End-Of-Line
CNcbiIstream& NcbiGetlineEOL(CNcbiIstream& is, string& str)
{
#if   defined(NCBI_OS_MSWIN)
    NcbiGetline(is, str, '\n');
    if (!str.empty()  &&  str[str.length() - 1] == '\r')
        str.resize(str.length() - 1);
#elif defined(NCBI_OS_DARWIN)
    NcbiGetline(is, str, "\r\n");
#else /* assume UNIX-like EOLs */
    NcbiGetline(is, str, '\n');
#endif //NCBI_OS_...
    return is;
}


bool NcbiStreamCopy(CNcbiOstream& os, CNcbiIstream& is)
{
    if (!is.good())
        return false;
#ifndef NCBI_COMPILER_GCC
    if (CT_EQ_INT_TYPE(is.peek(), CT_EOF))
        return true; // NB: Workshop doesn't set eofbit (C++ Std 27.6.1.3.27)
    os << is.rdbuf();
#elif   NCBI_COMPILER_VERSION <= 330
    // GCC stdlib++ version <= 3.3.0 has a bug in implementation of streamcopy,
    // which wrongly assumes that showmanyc() (which is called when no read
    // position is available for in_avail()) returns the number of bytes that
    // have been placed in the buffer, so it tries to read right off gptr()
    // that many bytes, causing bound conditions (ending up with SEGV).
    // Note that this implementation here does not agree with the standard
    // in that if insertion was not successful, the corresponding extraction
    // would not be effected as have occurred.  Hopefully that's okay because
    // GCC 3.3.0 is a very distant memory now.
    do {
        char buf[4096];
        is.read(buf, sizeof(buf));
        streamsize count = is.gcount();
        if (!count)
            break;
        if (!os.write(buf, count))
            break;
    } while (is.good());
#else
    if (CT_EQ_INT_TYPE(is.peek(), CT_EOF)) {
#  ifdef __GLIBCXX__
        return is.eof() ? true : false; // NB: GLIBC++ sets eofbit
#  else
        return true; // The C++ Std does not say about eofbit (27.6.1.3.27)
#  endif
    }
    os << is.rdbuf();
#endif
    if (!os.good())
        return false;
    os.flush();
    if (!os.good())
        return false;
    if (!CT_EQ_INT_TYPE(is.peek(), CT_EOF)) {
        os.clear(NcbiFailbit);
        return false;
    }
    return true;
}


CNcbiOstrstreamToString::operator string(void) const
{
    SIZE_TYPE length = (size_t)m_Out.pcount();
    if ( length == 0 )
        return string();
    const char* str = m_Out.str();
    m_Out.freeze(false);
    return string(str, length);
}


CNcbiOstream& operator<<(CNcbiOstream& out, const CTempString& str)
{
    return out.write(str.data(), str.length());
}


CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        out.put(char(toupper((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        out.put(char(tolower((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(toupper((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(tolower((unsigned char)(*c))));
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


static inline
bool s_IsQuoted(char c)
{
    return (c == '\t'  ||   c == '\v'  ||  c == '\b'  ||
            c == '\r'  ||   c == '\f'  ||  c == '\a'  ||
            c == '\n'  ||   c == '\\'  ||  c == '\''  ||
            c == '"'   ||  !isprint((unsigned char) c) ? true : false);
}


static inline
void s_WritePrintable(CNcbiOstream& out, char c, char n)
{
    switch ( c ) {
    case '\t':  out.write("\\t",  2);  return;
    case '\v':  out.write("\\v",  2);  return;
    case '\b':  out.write("\\b",  2);  return;
    case '\r':  out.write("\\r",  2);  return;
    case '\f':  out.write("\\f",  2);  return;
    case '\a':  out.write("\\a",  2);  return;
    case '\n':  out.write("\\n",  2);  return;
    case '\\':  out.write("\\\\", 2);  return;
    case '\'':  out.write("\\'",  2);  return;
    case '"':   out.write("\\\"", 2);  return;
    default:
        if ( isprint((unsigned char) c) ) {
            out.put(c);
            return;
        }
        break;
    }

    bool full = !s_IsQuoted(n)  &&  n >= '0'  &&  n <= '7' ? true : false;
    unsigned char v;
    char octal[4];
    int k = 1;

    *octal = '\\';
    v =  (unsigned char) c >> 6;
    if (v  ||  full) {
        octal[k++] = '0' + v;
        full = true;
    }
    v = ((unsigned char) c >> 3) & 7;
    if (v  ||  full) {
        octal[k++] = '0' + v;
    }
    v =  (unsigned char) c       & 7;
    octal    [k++] = '0' + v;
    out.write(octal, k);
}


CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableStringConverter s)
{
    size_t size = s.m_String.size();
    if (size) {
        const char* data = s.m_String.data();
        for (size_t i = 0;  i < size - 1;  ++i) {
            s_WritePrintable(out, data[i], data[i + 1]);
        }
        s_WritePrintable(out, data[size - 1], '\0');
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableCharPtrConverter s)
{
    const char* p = s.m_String;
    char        c = *p;
    while (c) {
        char n = *++p;
        s_WritePrintable(out, c, n);
        c = n;
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


EEncodingForm ReadIntoUtf8(
    CNcbiIstream&     input,
    CStringUTF8*      result,
    EEncodingForm     ef             /* = eEncodingForm_Unknown */,
    EReadUnknownNoBOM what_if_no_bom /* = eNoBOM_GuessEncoding  */
)
{
    EEncodingForm ef_bom = eEncodingForm_Unknown;
    result->erase();
    if (!input.good()) {
        return ef_bom;
    }

    const int buf_size = 4096;//2048;//256;
    char tmp[buf_size+2];
    Uint2* us = reinterpret_cast<Uint2*>(tmp);

// check for Byte Order Mark
    const int bom_max = 4;
    memset(tmp,0,bom_max);
    input.read(tmp,bom_max);
    int n = (int)input.gcount();
    {
        int bom_len=0;
        Uchar* uc = reinterpret_cast<Uchar*>(tmp);
        if (n >= 3 && uc[0] == 0xEF && uc[1] == 0xBB && uc[2] == 0xBF) {
            ef_bom = eEncodingForm_Utf8;
            uc[0] = uc[3];
            bom_len=3;
        }
        else if (n >= 2 && (us[0] == 0xFEFF || us[0] == 0xFFFE)) {
            if (us[0] == 0xFEFF) {
                ef_bom = eEncodingForm_Utf16Native;
            } else {
                ef_bom = eEncodingForm_Utf16Foreign;
            }
            us[0] = us[1];
            bom_len=2;
        }
        if (ef == eEncodingForm_Unknown || ef == ef_bom) {
            ef = ef_bom;
            n -= bom_len;
        }
        // else proceed at user's risk
    }

// keep reading
    while (n != 0  ||  (input.good()  &&  !input.eof())) {

        if (n == 0) {
            input.read(tmp, buf_size);
            n = (int)input.gcount();
            result->reserve(max(result->capacity(), result->size() + n));
        }
        tmp[n] = '\0';

        switch (ef) {
        case eEncodingForm_Utf16Foreign:
            {
                char buf[buf_size];
                swab(tmp,buf,n);
                memcpy(tmp, buf, n);
            }
            // no break here
        case eEncodingForm_Utf16Native:
            {
                Uint2* u = us;
                for (n = n/2; n--; ++u) {
                    result->Append(*u);
                }
            }
            break;
        case eEncodingForm_ISO8859_1:
            result->Append(tmp,eEncoding_ISO8859_1);
            break;
        case eEncodingForm_Windows_1252:
            result->Append(tmp,eEncoding_Windows_1252);
            break;
        case eEncodingForm_Utf8:
//            result->Append(tmp,eEncoding_UTF8);   
            result->append(tmp,n);
            break;
        default:
            if (what_if_no_bom == eNoBOM_GuessEncoding) {
                if (n == bom_max) {
                    input.read(tmp + n, buf_size - n);
                    n += (int)input.gcount();
                    result->reserve(max(result->capacity(), result->size() + n));
                }
                tmp[n] = '\0';
                EEncoding enc = CStringUTF8::GuessEncoding(tmp);
                switch (enc) {
                default:
                case eEncoding_Unknown:
                    if (CStringUTF8::GetValidBytesCount(tmp, n) != 0) {
                        ef = eEncodingForm_Utf8;
                        result->Append(tmp,enc);
                    }
                    else {
                        NCBI_THROW(CCoreException, eCore,
                                "ReadIntoUtf8: cannot guess text encoding");
                    }
                    break;
                case eEncoding_UTF8:
                    ef = eEncodingForm_Utf8;
                    // no break here
                case eEncoding_Ascii:
                case eEncoding_ISO8859_1:
                case eEncoding_Windows_1252:
                    result->Append(tmp,enc);
                    break;
                }
            } else {
//                result->Append(tmp,eEncoding_UTF8);
                result->append(tmp,n);
            }
            break;
        }
        n = 0;
    }
    return ef_bom;
}


EEncodingForm GetTextEncodingForm(CNcbiIstream& input,
                                  EBOMDiscard   discard_bom)
{
    EEncodingForm ef = eEncodingForm_Unknown;
    if (input.good()) {
        const int bom_max = 4;
        char tmp[bom_max];
        memset(tmp,0,bom_max);
        Uint2* us = reinterpret_cast<Uint2*>(tmp);
        Uchar* uc = reinterpret_cast<Uchar*>(tmp);
        input.get(tmp[0]);
        int n = (int)input.gcount();
        if (n == 1 && (uc[0] == 0xEF || uc[0] == 0xFE || uc[0] == 0xFF)) {
            input.get(tmp[1]);
            if (input.gcount()==1) {
                ++n;
                if (us[0] == 0xFEFF) {
                    ef = eEncodingForm_Utf16Native;
                } else if (us[0] == 0xFFFE) {
                    ef = eEncodingForm_Utf16Foreign;
                } else if (uc[1] == 0xBB) {
                    input.get(tmp[2]);
                    if (input.gcount()==1) {
                        ++n;
                        if (uc[2] == 0xBF) {
                            ef = eEncodingForm_Utf8;
                        }
                    }
                }
            }
        }
        if (ef == eEncodingForm_Unknown) {
            if (n > 1) {
                CStreamUtils::Pushback(input,tmp,n);
            } else if (n == 1) {
                input.unget();
            }
        } else {
            if (discard_bom == eBOM_Keep) {
                CStreamUtils::Pushback(input,tmp,n);
            }
        }
    }
    return ef;
}


#include "ncbi_base64.c"


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
