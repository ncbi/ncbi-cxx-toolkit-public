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
* Authors:  Eugene Vasilchenko, Denis Vakatov
*
* File Description:
*   Some helper functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.34  2000/11/07 04:06:08  vakatov
* kEmptyCStr (equiv. to NcbiEmptyCStr)
*
* Revision 1.33  2000/10/11 21:03:49  vakatov
* Cleanup to avoid 64-bit to 32-bit values truncation, etc.
* (reported by Forte6 Patch 109490-01)
*
* Revision 1.32  2000/08/03 20:21:29  golikov
* Added predicate PCase for AStrEquiv
* PNocase, PCase goes through NStr::Compare now
*
* Revision 1.31  2000/07/19 19:03:55  vakatov
* StringToBool() -- short and case-insensitive versions of "true"/"false"
* ToUpper/ToLower(string&) -- fixed
*
* Revision 1.30  2000/06/01 19:05:40  vasilche
* NStr::StringToInt now reports errors for tailing symbols in release version too
*
* Revision 1.29  2000/05/01 19:02:25  vasilche
* Force argument in NStr::StringToInt() etc to be full number.
* This check will be in DEBUG version for month.
*
* Revision 1.28  2000/04/19 18:36:04  vakatov
* Fixed for non-zero "pos" in s_Compare()
*
* Revision 1.27  2000/04/17 04:15:08  vakatov
* NStr::  extended Compare(), and allow case-insensitive string comparison
* NStr::  added ToLower() and ToUpper()
*
* Revision 1.26  2000/04/04 22:28:09  vakatov
* NStr::  added conversions for "long"
*
* Revision 1.25  2000/02/01 16:48:09  vakatov
* CNcbiEmptyString::  more dancing around the Sun "feature" (see also R1.24)
*
* Revision 1.24  2000/01/20 16:24:42  vakatov
* Kludging around the "NcbiEmptyString" to ensure its initialization when
* it is used by the constructor of a statically allocated object
* (I believe that it is actually just another Sun WorkShop compiler "feature")
*
* Revision 1.23  1999/12/28 18:55:43  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.22  1999/12/17 19:04:09  vasilche
* NcbiEmptyString made extern.
*
* Revision 1.21  1999/11/26 19:29:09  golikov
* fix
*
* Revision 1.20  1999/11/26 18:45:17  golikov
* NStr::Replace added
*
* Revision 1.19  1999/11/17 22:05:04  vakatov
* [!HAVE_STRDUP]  Emulate "strdup()" -- it's missing on some platforms
*
* Revision 1.18  1999/10/13 16:30:30  vasilche
* Fixed bug in PNocase which appears under GCC implementation of STL.
*
* Revision 1.17  1999/07/08 16:10:14  vakatov
* Fixed a warning in NStr::StringToUInt()
*
* Revision 1.16  1999/07/06 15:21:06  vakatov
* + NStr::TruncateSpaces(const string& str, ETrunc where=eTrunc_Both)
*
* Revision 1.15  1999/06/15 20:50:05  vakatov
* NStr::  +BoolToString, +StringToBool
*
* Revision 1.14  1999/05/27 15:21:40  vakatov
* Fixed all StringToXXX() functions
*
* Revision 1.13  1999/05/17 20:10:36  vasilche
* Fixed bug in NStr::StringToUInt which cause an exception.
*
* Revision 1.12  1999/05/04 00:03:13  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.11  1999/04/22 14:19:04  vasilche
* Added _TRACE_THROW() macro which can be configured to make coredump at some poin
* t fo throwing exception.
*
* Revision 1.10  1999/04/14 21:20:33  vakatov
* Dont use "snprintf()" as it is not quite portable yet
*
* Revision 1.9  1999/04/14 19:57:36  vakatov
* Use limits from <ncbitype.h> rather than from <limits>.
* [MSVC++]  fix for "snprintf()" in <ncbistd.hpp>.
*
* Revision 1.8  1999/04/09 19:51:37  sandomir
* minor changes in NStr::StringToXXX - base added
*
* Revision 1.7  1999/01/21 16:18:04  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.6  1999/01/11 22:05:50  vasilche
* Fixed CHTML_font size.
* Added CHTML_image input element.
*
* Revision 1.5  1998/12/28 17:56:39  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.4  1998/12/21 17:19:37  sandomir
* VC++ fixes in ncbistd; minor fixes in Resource
*
* Revision 1.3  1998/12/17 21:50:45  sandomir
* CNCBINode fixed in Resource; case insensitive string comparison predicate added
*
* Revision 1.2  1998/12/15 17:36:33  vasilche
* Fixed "double colon" bug in multithreaded version of headers.
*
* Revision 1.1  1998/12/15 15:43:22  vasilche
* Added utilities to convert string <> int.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <algorithm>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>


BEGIN_NCBI_SCOPE


const char *const kEmptyCStr = "";


const string* CNcbiEmptyString::m_Str = 0;
const string& CNcbiEmptyString::FirstGet(void) {
    static const string s_str = "";
    m_Str = &s_str;
    return s_str;
}


struct s_PCharDiff_Case {
    int operator()(const char& c1, const char& c2) {
        return (int) c1 - c2;
    }
};

struct s_PCharDiff_Nocase {
    int operator()(const char& c1, const char& c2) {
        return toupper(c1) - toupper(c2);
    }
};


template <class TCharDiff>
int s_Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
              const char* pattern, TCharDiff char_diff)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return *pattern ? -1 : 0;
    }
    if ( !*pattern ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    const char* s = str.data() + pos;
    while (n  &&  *pattern  &&  char_diff(*s, *pattern) == 0) {
        s++;  pattern++;  n--;
    }

    if (n == 0) {
        return *pattern ? -1 : 0; 
    }
    
    return char_diff(*s, *pattern);
}


template <class PCharDiff>
int s_Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
              const string& pattern, PCharDiff char_diff)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return pattern.empty() ? 0 : -1;
    }
    if ( pattern.empty() ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    SIZE_TYPE n_cmp = n;
    if (n_cmp > pattern.length()) {
        n_cmp = pattern.length();
    }
    const char* s = str.data() + pos;
    const char* p = pattern.data();
    while (n_cmp  &&  char_diff(*s, *p) == 0) {
        s++;  p++;  n_cmp--;
    }

    if (n_cmp == 0) {
        if (n == pattern.length())
            return 0;
        return n > pattern.length() ? 1 : -1;
    }
    
    return char_diff(*s, *p);
}

int NStr::Compare(const char* s1, const char* s2,
                  ECase use_case /* = eCase */)
{
    if (use_case == eCase) {
        while (*s1  &&  *s1 == *s2) {
            s1++;  s2++;
        }
        return (int) *s1 - *s2;
    } else {
        while (*s1  &&  toupper(*s1) == toupper(*s2)) {
            s1++;  s2++;
        }
        return toupper(*s1) - toupper(*s2);
    }
}

int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case /* = eCase */)
{
    if (use_case == eCase) {
        return s_Compare(str, pos, n, pattern, s_PCharDiff_Case());
    } else {
        return s_Compare(str, pos, n, pattern, s_PCharDiff_Nocase());
    }
}

int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case /* = eCase */)
{
    if (use_case == eCase) {
        return s_Compare(str, pos, n, pattern, s_PCharDiff_Case());
    } else {
        return s_Compare(str, pos, n, pattern, s_PCharDiff_Nocase());
    }
}



char* NStr::ToLower(char* str) {
    char* s;
    for (s = str;  *str;  str++) {
        *str = tolower(*str);
    }
    return s;
}

string& NStr::ToLower(string& str) {
    for (string::iterator it = str.begin();  it != str.end();  ++it) {
        *it = tolower(*it);
    }
    return str;
}

char* NStr::ToUpper(char* str) {
    char* s;
    for (s = str;  *str;  str++) {
        *str = toupper(*str);
    }
    return s;
}

string& NStr::ToUpper(string& str) {
    for (string::iterator it = str.begin();  it != str.end();  ++it) {
        *it = toupper(*it);
    }
    return str;
}

# define CHECK_ENDPTR() \
    if (*endptr != '\0') { \
        THROW1_TRACE(runtime_error, "no symbols should be after number"); \
    }



int NStr::StringToInt(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    long value = ::strtol(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||
        value < kMin_Int || value > kMax_Int)
        throw runtime_error("NStr::StringToInt():  cannot convert");
    CHECK_ENDPTR();
    return (int) value;
}

unsigned int NStr::StringToUInt(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    unsigned long value = ::strtoul(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||
        value > kMax_UInt)
        throw runtime_error("NStr::StringToUInt():  cannot convert");
    CHECK_ENDPTR();
    return (unsigned int) value;
}

long NStr::StringToLong(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    long value = ::strtol(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str())
        throw runtime_error("NStr::StringToLong():  cannot convert");
    CHECK_ENDPTR();
    return value;
}

unsigned long NStr::StringToULong(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    unsigned long value = ::strtoul(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str())
        throw runtime_error("NStr::StringToULong():  cannot convert");
    CHECK_ENDPTR();
    return value;
}

double NStr::StringToDouble(const string& str)
{
    errno = 0;
    char* endptr = 0;
    double value = ::strtod(str.c_str(), &endptr);
    if (errno  ||  !endptr  ||  endptr == str.c_str())
        throw runtime_error("NStr::StringToDouble():  cannot convert");
    if ( *endptr == '.' )
        endptr++;
    CHECK_ENDPTR();
    return value;
}

string NStr::IntToString(long value, bool sign /* = false */ )
{
    char buffer[64];
    ::sprintf(buffer, sign ? "%+ld": "%ld", value);
    return buffer;
}

string NStr::UIntToString(unsigned long value)
{
    char buffer[64];
    ::sprintf(buffer, "%lu", value);
    return buffer;
}

string NStr::DoubleToString(double value)
{
    char buffer[64];
    ::sprintf(buffer, "%g", value);
    return buffer;
}

string NStr::PtrToString(const void* value)
{
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    return buffer;
}

static const string s_kTrueString  = "true";
static const string s_kFalseString = "false";
static const string s_kTString     = "t";
static const string s_kFString     = "f";

string NStr::BoolToString(bool value)
{
    return value ? s_kTrueString : s_kFalseString;
}

bool NStr::StringToBool(const string& str)
{
    if (AStrEquiv(str, s_kTrueString,  PNocase())  ||
        AStrEquiv(str, s_kTString,  PNocase()) )
        return true;

    if ( AStrEquiv(str, s_kFalseString, PNocase())  ||
         AStrEquiv(str, s_kFString, PNocase()))
        return false;

    throw runtime_error("NStr::StringToBool(): cannot convert");
}


string NStr::TruncateSpaces(const string& str, ETrunc where)
{
    SIZE_TYPE beg = 0;
    if (where == eTrunc_Begin  ||  where == eTrunc_Both) {
        while (beg < str.length()  &&  isspace(str[beg]))
            beg++;
        if (beg == str.length())
            return NcbiEmptyString;
    }
    SIZE_TYPE end = str.length() - 1;
    if (where == eTrunc_End  ||  where == eTrunc_Both) {
        while ( isspace(str[end]) )
            end--;
    }
    _ASSERT( beg <= end );
    return str.substr(beg, end - beg + 1);
}

string& NStr::Replace(const string& src, const string& search, const string& replace,
                      string& dst, SIZE_TYPE start_pos, size_t max_replace)
{
    dst = src;

    if( start_pos + search.size() > src.size() ||
        search == replace)
        return dst;

    for(size_t count = 0; !(max_replace && count >= max_replace); count++) {
        start_pos = dst.find(search, start_pos);
        if(start_pos == NPOS)
            break;
        dst.replace(start_pos, search.size(), replace);
        start_pos += replace.size();
    }
    return dst;
}

string NStr::Replace(const string& src, const string& search, const string& replace,
                     SIZE_TYPE start_pos, size_t max_replace)
{
    string dst;
    return Replace(src, search, replace, dst, start_pos, max_replace);
}

// predicates

// case-INsensitive string comparison
// operator() meaning is the same as operator<
bool PNocase::operator() (const string& x, const string& y) const
{
    return NStr::Compare(x, y, NStr::eNocase) < 0;
}

// case-sensitive string comparison
// operator() meaning is the same as operator<
bool PCase::operator() (const string& x, const string& y) const
{
    return NStr::Compare(x, y, NStr::eCase) < 0;
}

#if !defined(HAVE_STRDUP)
extern char* strdup(const char* str)
{
    if ( !str )
        return 0;

    size_t size   = ::strlen(str) + 1;
    void*  result = ::malloc(size);
    return (char*) (result ? ::memcpy(result, str, size) : 0);
}
#endif


END_NCBI_SCOPE
