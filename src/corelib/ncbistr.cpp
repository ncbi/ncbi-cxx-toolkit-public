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
 *   Government have not placed any restriction on its use or reproduction.
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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/tempstr.hpp>
#include <corelib/ncbi_limits.hpp>
#include <memory>
#include <algorithm>
#include <errno.h>
#include <stdio.h>


BEGIN_NCBI_SCOPE

// Hex symbols
static const char s_Hex[] = "0123456789ABCDEF";


inline
std::string::size_type s_DiffPtr(const char* end, const char* start)
{
    return end ? (size_t)(end - start) : (size_t) 0;
}

const char *const kEmptyCStr = "";


#if !defined(NCBI_OS_MSWIN) && !( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
const string* CNcbiEmptyString::m_Str = 0;
const string& CNcbiEmptyString::FirstGet(void) {
    static const string s_Str = "";
    m_Str = &s_Str;
    return s_Str;
}
#endif

bool NStr::IsBlank(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE len = str.length();
    for (SIZE_TYPE idx = pos; idx < len; ++idx) {
        if (!isspace((unsigned char) str[idx])) {
            return false;
        }
    }
    return true;
}


int NStr::CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                      const char* pattern)
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
    while (n  &&  *pattern  &&  *s == *pattern) {
        s++;  pattern++;  n--;
    }

    if (n == 0) {
        return *pattern ? -1 : 0;
    }

    return *s - *pattern;
}


int NStr::CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                        const char* pattern)
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
    while (n  &&  *pattern  &&
           tolower((unsigned char)(*s)) == 
           tolower((unsigned char)(*pattern))) {
        s++;  pattern++;  n--;
    }

    if (n == 0) {
        return *pattern ? -1 : 0;
    }

    return tolower((unsigned char)(*s)) - tolower((unsigned char)(*pattern));
}


int NStr::CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                      const string& pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return pattern.empty() ? 0 : -1;
    }
    if (pattern.empty()) {
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
    while (n_cmp  &&  *s == *p) {
        s++;  p++;  n_cmp--;
    }

    if (n_cmp == 0) {
        if (n == pattern.length())
            return 0;
        return n > pattern.length() ? 1 : -1;
    }

    return *s - *p;
}


int NStr::CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                        const string& pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return pattern.empty() ? 0 : -1;
    }
    if (pattern.empty()) {
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
    while (n_cmp  &&  
           tolower((unsigned char)(*s)) == tolower((unsigned char)(*p))) {
        s++;  p++;  n_cmp--;
    }

    if (n_cmp == 0) {
        if (n == pattern.length())
            return 0;
        return n > pattern.length() ? 1 : -1;
    }

    return tolower((unsigned char)(*s)) - tolower((unsigned char)(*p));
}


// NOTE: This code is used also in the CDirEntry::MatchesMask.

bool NStr::MatchesMask(const char* str, const char* mask, ECase use_case) 
{
    char c;
    bool infinite = true;

    while (infinite) {
        // Analyze symbol in mask
        switch ( c = *mask++ ) {
        
        case '\0':
            return *str == '\0';

        case '?':
            if (*str == '\0') {
                return false;
            }
            ++str;
            break;

        case '*':
            c = *mask;
            // Collapse multiple stars
            while ( c == '*' ) {
                c = *++mask;
            }
            if (c == '\0') {
                return true;
            }
            // General case, use recursion
            while ( *str ) {
                if (MatchesMask(str, mask, use_case)) {
                    return true;
                }
                ++str;
            }
            return false;

        default:
            // Compare nonpattern character in mask and name
            char s = *str++;
            if (use_case == eNocase) {
                c = tolower((unsigned char) c);
                s = tolower((unsigned char) s);
            }
            if (c != s) {
                return false;
            }
            break;
        }
    }
    return false;
}


char* NStr::ToLower(char* str)
{
    char* s;
    for (s = str;  *str;  str++) {
        *str = tolower((unsigned char)(*str));
    }
    return s;
}


string& NStr::ToLower(string& str)
{
    NON_CONST_ITERATE (string, it, str) {
        *it = tolower((unsigned char)(*it));
    }
    return str;
}


char* NStr::ToUpper(char* str)
{
    char* s;
    for (s = str;  *str;  str++) {
        *str = toupper((unsigned char)(*str));
    }
    return s;
}


string& NStr::ToUpper(string& str)
{
    NON_CONST_ITERATE (string, it, str) {
        *it = toupper((unsigned char)(*it));
    }
    return str;
}


int NStr::StringToNumeric(const string& str)
{
    if ( str.empty()  ||
         (!isdigit((unsigned char)(*str.begin())) &  (*str.begin() != '+')) ) {
        errno = EINVAL;
        return -1;
    }
    char* endptr = 0;
    const char* begptr = str.c_str();
    errno = 0;
    unsigned long value = strtoul(begptr, &endptr, 10);
    if ( errno  ||  !endptr  ||  endptr == begptr  ||
        value > (unsigned long) kMax_Int  ||  *endptr ) {
        if ( !errno ) {
            errno = !endptr || endptr == begptr || *endptr ? EINVAL : ERANGE;
        }
        return -1;
    }
    return (int) value;
}


#define S2N_CONVERT_ERROR(to_type, msg, errcode, force_errno, delta)        \
        if (flags & NStr::fConvErr_NoThrow)  {                              \
            if ( force_errno )                                              \
                errno = 0;                                                  \
            if ( !errno )                                                   \
                errno = errcode;                                            \
            /* ignore previosly converted value -- always return zero */    \
            return 0;                                                       \
        } else {                                                            \
            CTempString str_tmp(str);                                       \
            CTempString msg_tmp(msg);                                       \
            string s;                                                       \
            s.reserve(str_tmp.length() + msg_tmp.length() + 50);            \
            s += "Cannot convert string '";                                 \
            s += str;                                                       \
            s += "' to " #to_type;                                          \
            if ( !msg_tmp.empty() ) {                                       \
                s += ", ";                                                  \
                s += msg;                                                   \
            }                                                               \
            NCBI_THROW2(CStringException, eConvert, s, delta);              \
        }                                                                   \

#define S2N_CONVERT_ERROR_INVAL(to_type)                                    \
    S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, true, pos)

#define S2N_CONVERT_ERROR_RADIX(to_type, msg)                               \
    S2N_CONVERT_ERROR(to_type, msg, EINVAL, true, pos)

#define S2N_CONVERT_ERROR_OVERFLOW(to_type)                                 \
    S2N_CONVERT_ERROR(to_type, "overflow",ERANGE, true, pos)

#define CHECK_ENDPTR(to_type)                                               \
    if ( str[pos] ) {                                                       \
        S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, true, pos);           \
    }

#define CHECK_RANGE(nmin, nmax, to_type)                                    \
    if ( errno  ||  value < nmin  ||  value > nmax ) {                      \
        S2N_CONVERT_ERROR(to_type, "overflow", ERANGE, false, 0);           \
    }

#define CHECK_RANGE_U(nmax, to_type)                                        \
    if ( errno  ||  value > nmax ) {                                        \
        S2N_CONVERT_ERROR(to_type, "overflow", ERANGE, false, 0);           \
    }

#define CHECK_COMMAS                                                        \
    /* Check on possible commas */                                          \
    if (flags & NStr::fAllowCommas) {                                       \
        if (ch == ',') {                                                    \
            if ((numpos == pos)  ||                                         \
                ((comma >= 0)  &&  (comma != 3)) ) {                        \
                /* Not first comma, sitting on incorrect place */           \
                break;                                                      \
            }                                                               \
            /* Skip it */                                                   \
            comma = 0;                                                      \
            pos++;                                                          \
            continue;                                                       \
        } else {                                                            \
            if (comma >= 0) {                                               \
                /* Count symbols between commas */                          \
                comma++;                                                    \
            }                                                               \
        }                                                                   \
    }


int NStr::StringToInt(const CTempString& str, TStringToNumFlags flags,int base)
{
    errno = 0;
    Int8 value = StringToInt8(str, flags, base);
    CHECK_RANGE(kMin_Int, kMax_Int, int);
    return (int) value;
}


unsigned int
NStr::StringToUInt(const CTempString& str, TStringToNumFlags flags, int base)
{
    errno = 0;
    Uint8 value = StringToUInt8(str, flags, base);
    CHECK_RANGE_U(kMax_UInt, unsigned int);
    return (unsigned int) value;
}


long NStr::StringToLong(const CTempString& str, TStringToNumFlags flags,
                        int base)
{
    errno = 0;
    Int8 value = StringToInt8(str, flags, base);
    CHECK_RANGE(kMin_Long, kMax_Long, long);
    return (long) value;
}


unsigned long
NStr::StringToULong(const CTempString& str, TStringToNumFlags flags, int base)
{
    errno = 0;
    Uint8 value = StringToUInt8(str, flags, base);
    CHECK_RANGE_U(kMax_ULong, long);
    return (unsigned long) value;
}


/// @internal
// Check that symbol 'ch' is good symbol for number with radix 'base'.
bool s_IsGoodCharForRadix(char ch, int base, int* value = 0)
{
    if (!isalnum((unsigned char) ch)) {
        return false;
    }
    // Corresponding numeric value of *endptr
    int delta;
    if (isdigit((unsigned char) ch)) {
        delta = ch - '0';
    } else {
        ch = tolower((unsigned char) ch);
        delta = ch - 'a' + 10;
    }
    if ( value ) {
        *value = delta;
    }
    return delta < base;
 }


// Skip all allowed chars (all except used for digit composition). 
// Update 'ptr' to current position in the string.
enum ESkipMode {
    eSkipAll,           // all symbols
    eSkipAllAllowed,    // all symbols, except digit/+/-/.
    eSkipSpacesOnly     // spaces only 
};

void s_SkipAllowedSymbols(const CTempString& str, size_t& pos,
                          ESkipMode skip_mode)
{
    if (skip_mode == eSkipAll) {
        pos = str.length();
        return;
    }
    unsigned char ch = str[pos];
    while ( ch ) {
        if ( isdigit(ch)  ||  ch == '+' ||  ch == '-'  ||  ch == '.' ) {
            break;
        }
        if ( (skip_mode == eSkipSpacesOnly)  &&  !isspace(ch) ) {
            break;
        }
        ch = str[++pos];
    }
}


// Check radix base. If it is zero, determine base using first chars
// of the string. Update 'base' value.
// Update 'ptr' to current position in the string.

bool s_CheckRadix(const CTempString& str, size_t& pos, int& base)
{
    // Check base
    if ( base < 0  ||  base == 1  ||  base > 36 ) {
        return false;
    }
    // Try to determine base using first chars of the string
    unsigned char ch   = str[pos];
    unsigned char next = str[pos+1];
    if ( base == 0 ) {
        if ( ch != '0' ) {
            base = 10;
        } else if (next == 'x' || next == 'X') {
            base = 16;
        } else {
            base = 8;
        }
    }
    // Remove leading '0x' for hex numbers
    if ( base == 16 ) {
        if (ch == '0'  &&  (next == 'x' || next == 'X')) {
            pos += 2;
        }
    }
    return true;
}


Int8 NStr::StringToInt8(const CTempString& str, TStringToNumFlags flags,
                        int base)
{
    _ASSERT(flags == 0  ||  flags > 32);

    // Current position in the string
    size_t pos = 0;

    // Skip allowed leading symbols
    if (flags & fAllowLeadingSymbols) {
        bool spaces = ((flags & fAllowLeadingSymbols) == fAllowLeadingSpaces);
        s_SkipAllowedSymbols(str, pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed);
    }
    // Determine sign
    bool sign = false;
    switch (str[pos]) {
    case '-':
        sign = true;
        /*FALLTHRU*/
    case '+':
        pos++;
        break;
    default:
        if (flags & fMandatorySign) {
            S2N_CONVERT_ERROR_INVAL(Int8);
        }
        break;
    }
    // Check radix base
    if ( !s_CheckRadix(str, pos, base) ) {
        S2N_CONVERT_ERROR_RADIX(Int8, "bad numeric base '" + 
                                NStr::IntToString(base)+ "'");
    }

    // Begin conversion
    Int8 n = 0;
    Int8 limdiv = kMax_I8 / base;
    Int8 limoff = kMax_I8 % base + (sign ? 1 : 0);

    // Number of symbols between two commas. '-1' means -- no comma yet.
    int    comma  = -1;  
    size_t numpos = pos;

    errno = 0;
    while (str[pos]) {
        char ch = str[pos];
        int  delta;   // corresponding numeric value of 'ch'

        // Check on possible commas
        CHECK_COMMAS;
        // Sanity check
        if ( !s_IsGoodCharForRadix(ch, base, &delta) ) {
            break;
        }
        // Overflow check
        if ( n > limdiv  ||  (n == limdiv  &&  delta > limoff) ) {
            S2N_CONVERT_ERROR_OVERFLOW(Int8);
        }
        n *= base;
        n += delta;
        pos++;
    }

    // Last checks
    if ( !pos  || ((comma >= 0)  &&  (comma != 3)) ) {
        S2N_CONVERT_ERROR_INVAL(Int8);
    }
    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll);
    }
    // Assign sign before the end pointer check
    n = sign ? -n : n;
    CHECK_ENDPTR(Int8);
    return n;
}


Uint8
NStr::StringToUInt8(const CTempString& str, TStringToNumFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);

    // Current position in the string
    size_t pos = 0;

    // Skip allowed leading symbols
    if (flags & fAllowLeadingSymbols) {
        bool spaces = ((flags & fAllowLeadingSymbols) == fAllowLeadingSpaces);
        s_SkipAllowedSymbols(str, pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed);
    }
    // Determine sign
    if (str[pos] == '+') {
        pos++;
    } else {
        if (flags & fMandatorySign) {
            S2N_CONVERT_ERROR_INVAL(Uint8);
        }
    }
    // Check radix base
    if ( !s_CheckRadix(str, pos, base) ) {
        S2N_CONVERT_ERROR_RADIX(Uint8, "bad numeric base '" +
                                NStr::IntToString(base) + "'");
    }

    // Begin conversion
    Uint8 n = 0;
    Uint8 limdiv = kMax_UI8 / base;
    int   limoff = int(kMax_UI8 % base);

    // Number of symbols between two commas. '-1' means -- no comma yet.
    int    comma  = -1;  
    size_t numpos = pos;

    errno = 0;
    while (str[pos]) {
        char ch = str[pos];
        int  delta;         // corresponding numeric value of 'ch'

        // Check on possible commas
        CHECK_COMMAS;
        // Sanity check
        if ( !s_IsGoodCharForRadix(ch, base, &delta) ) {
            break;
        }
        // Overflow check
        if (n > limdiv  ||  (n == limdiv  &&  delta > limoff)) {
            S2N_CONVERT_ERROR_OVERFLOW(Uint8);
        }
        n *= base;
        n += delta;
        pos++;
    }

    // Last checks
    if ( !pos  || ((comma >= 0)  &&  (comma != 3)) ) {
        S2N_CONVERT_ERROR_INVAL(Uint8);
    }
    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll);
    }
    CHECK_ENDPTR(Uint8);
    return n;
}


double
NStr::StringToDouble(const CTempString& str, TStringToNumFlags flags)
{
    _ASSERT(flags == 0  ||  flags > 32);

    // Current position in the string
    size_t pos  = 0;

    // Skip allowed leading symbols
    if (flags & fAllowLeadingSymbols) {
        bool spaces = ((flags & fAllowLeadingSymbols) == fAllowLeadingSpaces);
        s_SkipAllowedSymbols(str, pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed);
    }
    // Check mandatory sign
    if (flags & fMandatorySign) {
        switch (str[pos]) {
        case '-':
        case '+':
            break;
        default:
            S2N_CONVERT_ERROR_INVAL(double);
            break;
        }
    }
    // For consistency make additional check on incorrect leading symbols.
    // Because strtod() may just skip such symbols.
    if (!(flags & fAllowLeadingSymbols)) {
        char c = str[pos];
        if ( !isdigit((unsigned int)c)  &&  c != '.'  &&  c != '-'  &&  c != '+') {
            S2N_CONVERT_ERROR_INVAL(double);
        }
    }

    // Conversion
    string s;
    str.Copy(s, 0, str.size());
    char* endptr = 0;
    const char* begptr = s.c_str() + pos;

    errno = 0;
    double n = strtod(begptr, &endptr);
    if ( errno  ||  !endptr  ||  endptr == begptr ) {
        S2N_CONVERT_ERROR(double, kEmptyStr, EINVAL, false,
                          s_DiffPtr(endptr, begptr) + pos);
    }
    if ( *(endptr - 1) != '.'  &&  *endptr == '.' ) {
        // Only a single dot at the end of line is allowed
        if (endptr == strchr(begptr, '.')) {
            endptr++;
        }
    }
    pos += s_DiffPtr(endptr, begptr);

    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll);
    }
    CHECK_ENDPTR(double);
    return n;
}


/// @internal
static Uint8 s_DataSizeConvertQual(const CTempString&      str,
                                   size_t&                 pos, 
                                   Uint8                   value,
                                   NStr::TStringToNumFlags flags)
{
    unsigned char ch = str[pos];
    if ( !ch ) {
        return value;
    }

    ch = toupper(ch);
    Uint8 v   = value;
    bool  err = false;

    switch(ch) {
    case 'K':
        pos++;
        if ((kMax_UI8 / 1024) < v) {
            err = true;
        }
        v *= 1024;
        break;
    case 'M':
        pos++;
        if ((kMax_UI8 / 1024 / 1024) < v) {
            err = true;
        }
        v *= 1024 * 1024;
        break;
    case 'G':
        pos++;
        if ((kMax_UI8 / 1024 / 1024 / 1024) < v) {
            err = true;
        }
        v *= 1024 * 1024 * 1024;
        break;
    default:
        // error -- the "qual" points to the last unprocessed symbol
        S2N_CONVERT_ERROR_INVAL(Uint8);
    }
    if ( err ) {
        S2N_CONVERT_ERROR_OVERFLOW(DataSize);
    }

    ch = str[pos];
    if ( ch  &&  toupper(ch) == 'B' ) {
        pos++;
    }
    return v;
}


Uint8 NStr::StringToUInt8_DataSize(const CTempString& str, 
                                   TStringToNumFlags  flags, 
                                   int                base)
{
    // We have a limited base range here
    _ASSERT(flags == 0  ||  flags > 20);

    // Current position in the string
    size_t pos = 0;

    // Find end of number representation
    {{
        // Skip allowed leading symbols
        if (flags & fAllowLeadingSymbols) {
            bool spaces = ((flags & fAllowLeadingSymbols) ==
                           fAllowLeadingSpaces);
            s_SkipAllowedSymbols(str, pos,
                           spaces ? eSkipSpacesOnly : eSkipAllAllowed);
        }
        // Determine sign
        if (str[pos] == '+') {
            pos++;
            // strip fMandatorySign flag
            flags &= ~fMandatorySign;
        } else {
            if (flags & fMandatorySign) {
                S2N_CONVERT_ERROR_INVAL(Uint8);
            }
        }
        // Check radix base
        if ( !s_CheckRadix(str, pos, base) ) {
            S2N_CONVERT_ERROR_RADIX(Uint8, "bad numeric base '" +
                                    NStr::IntToString(base) + "'");
        }
    }}

    size_t numpos = pos;
    char ch = str[pos];
    while (ch) {
        if ( !s_IsGoodCharForRadix(ch, base)  &&
             ((ch != ',')  ||  !(flags & fAllowCommas)) ) {
            break;
        }
        ch = str[++pos];
    }
    // If string is empty, just use whole remaining string for conversion
    // (for correct error reporting)
    if (pos-numpos == 0) {
        pos = str.length();
    }

    // Convert to number
    Uint8 n = StringToUInt8(CTempString(str.data(), numpos, pos-numpos),
                            flags, base);
    if ( errno ) {
        // If exceptions enabled by flags that it has been already thrown.
        // errno is also set, so return a zero.
        return 0;
    }
    // Check trailer (KB, MB, ...)
    if ( ch ) {
        n = s_DataSizeConvertQual(str, pos, n, flags);
    }
    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll);
    }
    CHECK_ENDPTR(Uint8);
    return n;
}


void NStr::IntToString(string& out_str, long svalue,
                       TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 1  ||  base > 36 ) {
        return;
    }

    unsigned long value;
    if (base == 10) {
        value = static_cast<unsigned long>(svalue<0?-svalue:svalue);
    } else {
        value = static_cast<unsigned long>(svalue);
    }
    
    const size_t kBufSize = (sizeof(value) * CHAR_BIT);
    char  buffer[kBufSize];
    char* pos = buffer + kBufSize;

    if ( (base == 10) && (flags & fWithCommas) ) {
        int cnt = -1;
        do {
            if (++cnt == 3) {
                *--pos = ',';
                cnt = 0;
            }
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
    }
    else {
        do {
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
    }

    if (base == 10) {
        if (svalue < 0)
            *--pos = '-';
        else if (flags & fWithSign)
            *--pos = '+';
    }

    out_str.assign(pos, buffer + kBufSize - pos);
}


void NStr::UIntToString(string&           out_str,
                        unsigned long     value,
                        TNumToStringFlags flags,
                        int               base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 1  ||  base > 36 ) {
        return;
    }

    const size_t kBufSize = (sizeof(value) * CHAR_BIT);
    char  buffer[kBufSize];
    char* pos = buffer + kBufSize;

    if ( (base == 10) && (flags & fWithCommas) ) {
        int cnt = -1;
        do {
            if (++cnt == 3) {
                *--pos = ',';
                cnt = 0;
            }
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
    }
    else {
        do {
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
    }
    
    if ( (base == 10)  &&  (flags & fWithSign) ) {
        *--pos = '+';
    }
    out_str.assign(pos, buffer + kBufSize - pos);
}


string NStr::Int8ToString(Int8 value, TNumToStringFlags flags, int base)
{
    string ret;
    NStr::Int8ToString(ret, value, flags, base);
    return ret;
}


// On some platforms division of Int8 is very slow,
// so will try to optimize it working with chunks.
// Works only for radix base == 10.

#define PRINT_INT8_CHUNK 1000000000
#define PRINT_INT8_CHUNK_SIZE 9

/// @internal
static char* s_PrintUint8(char*                   pos,
                          Uint8                   value,
                          NStr::TNumToStringFlags flags,
                          int                     base)
{
    if ( (base == 10) && (flags & NStr::fWithCommas) ) {
        int cnt = -1;
#ifdef PRINT_INT8_CHUNK
        for ( ;; ) {
            Uint4 chunk = Uint4(value % PRINT_INT8_CHUNK);
            value /= PRINT_INT8_CHUNK;
            if ( value ) {
                for ( int i = 0; i < PRINT_INT8_CHUNK_SIZE; ++i ) {
                    if (++cnt == 3) {
                        *--pos = ',';
                        cnt = 0;
                    }
                    *--pos = s_Hex[chunk % base];
                    chunk /= base;
                }
            }
            else {
                do {
                    if (++cnt == 3) {
                        *--pos = ',';
                        cnt = 0;
                    }
                    *--pos = s_Hex[chunk % base];
                    chunk /= base;
                } while ( chunk );
                break;
            }
        }
#else
        do {
            if (++cnt == 3) {
                *--pos = ',';
                cnt = 0;
            }
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
#endif
    }
    else {
        if ( PRINT_INT8_CHUNK  &&  (base == 10) ) {
            for ( ;; ) {
                Uint4 chunk = Uint4(value % PRINT_INT8_CHUNK);
                value /= PRINT_INT8_CHUNK;
                if ( value ) {
                    for ( int i = 0; i < PRINT_INT8_CHUNK_SIZE; ++i ) {
                        *--pos = s_Hex[chunk % base];
                        chunk /= base;
                    }
                }
                else {
                    do {
                        *--pos = s_Hex[chunk % base];
                        chunk /= base;
                    } while ( chunk );
                    break;
                }
            }
        } else {
            do {
                *--pos = s_Hex[value % base];
                value /= base;
            } while ( value );
        }
    }
    return pos;
}


void NStr::Int8ToString(string& out_str, Int8 svalue,
                        TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 1  ||  base > 36 ) {
        return;
    }

    Uint8 value;
    if (base == 10) {
        value = static_cast<Uint8>(svalue<0?-svalue:svalue);
    } else {
        value = static_cast<Uint8>(svalue);
    }

    const size_t kBufSize = (sizeof(value) * CHAR_BIT);
    char  buffer[kBufSize];

    char* pos = s_PrintUint8(buffer + kBufSize, value, flags, base);

    if (base == 10) {
        if (svalue < 0)
            *--pos = '-';
        else if (flags & fWithSign)
            *--pos = '+';
    }
    out_str.assign(pos, buffer + kBufSize - pos);
}


string NStr::UInt8ToString(Uint8 value, TNumToStringFlags flags, int base)
{
    string ret;
    NStr::UInt8ToString(ret, value, flags, base);
    return ret;
}


void NStr::UInt8ToString(string& out_str, Uint8 value,
                         TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 1  ||  base > 36 ) {
        return;
    }

    const size_t kBufSize = (sizeof(value) * CHAR_BIT);
    char  buffer[kBufSize];

    char* pos = s_PrintUint8(buffer + kBufSize, value, flags, base);

    if ( (base == 10)  &&  (flags & fWithSign) ) {
        *--pos = '+';
    }
    out_str.assign(pos, buffer + kBufSize - pos);
}


// A maximal double precision used in the double to string conversion
#if defined(NCBI_OS_MSWIN)
    const int kMaxDoublePrecision = 200;
#else
    const int kMaxDoublePrecision = 308;
#endif
// A maximal size of a double value in a string form.
// Exponent size + sign + dot + ending '\0' + max.precision
const int kMaxDoubleStringSize = 308 + 3 + kMaxDoublePrecision;


string NStr::DoubleToString(double value, int precision,
                            TNumToStringFlags flags)
{
    string str;
    DoubleToString(str, value, precision, flags);
    return str;
}


void NStr::DoubleToString(string& out_str, double value,
                          int precision, TNumToStringFlags flags)
{
    char buffer[kMaxDoubleStringSize];
    if (precision >= 0) {
        SIZE_TYPE n = DoubleToString(value, precision, buffer,
                                     kMaxDoubleStringSize, flags);
        buffer[n] = '\0';
    } else {
        const char* format;
        switch (flags & fDoubleGeneral) {
            case fDoubleFixed:
                format = "%f";
                break;
            case fDoubleScientific:
                format = "%e";
                break;
            case fDoubleGeneral: // default
            default: 
                format = "%g";
                break;
        }
        ::sprintf(buffer, format, value);
    }
    out_str = buffer;
}



SIZE_TYPE NStr::DoubleToString(double value, unsigned int precision,
                               char* buf, SIZE_TYPE buf_size,
                               TNumToStringFlags flags)
{
    char buffer[kMaxDoubleStringSize];
    if (precision > (unsigned int)kMaxDoublePrecision) {
        precision = (unsigned int)kMaxDoublePrecision;
    }
    const char* format;
    switch (flags & fDoubleGeneral) {
        case fDoubleScientific:
            format = "%.*e";
            break;
        case fDoubleGeneral:
            format = "%.*g";
            break;
        case fDoubleFixed: // default
        default:
            format = "%.*f";
            break;
    }
    int n = ::sprintf(buffer, format, (int)precision, value);
    SIZE_TYPE n_copy = min((SIZE_TYPE) n, buf_size);
    memcpy(buf, buffer, n_copy);
    return n_copy;
}


string NStr::PtrToString(const void* value)
{
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    return buffer;
}


void NStr::PtrToString(string& out_str, const void* value)
{
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    out_str = buffer;
}


const void* NStr::StringToPtr(const string& str)
{
    void *ptr = NULL;
    ::sscanf(str.c_str(), "%p", &ptr);
    return ptr;
}


static const char* s_kTrueString  = "true";
static const char* s_kFalseString = "false";
static const char* s_kTString     = "t";
static const char* s_kFString     = "f";
static const char* s_kYesString   = "yes";
static const char* s_kNoString    = "no";
static const char* s_kYString     = "y";
static const char* s_kNString     = "n";


const string NStr::BoolToString(bool value)
{
    return value ? s_kTrueString : s_kFalseString;
}


bool NStr::StringToBool(const string& str)
{
    if ( AStrEquiv(str, s_kTrueString,  PNocase())  ||
         AStrEquiv(str, s_kTString,     PNocase())  ||
         AStrEquiv(str, s_kYesString,   PNocase())  ||
         AStrEquiv(str, s_kYString,     PNocase()) )
        return true;

    if ( AStrEquiv(str, s_kFalseString, PNocase())  ||
         AStrEquiv(str, s_kFString,     PNocase())  ||
         AStrEquiv(str, s_kNoString,    PNocase())  ||
         AStrEquiv(str, s_kNString,     PNocase()) )
        return false;

    NCBI_THROW2(CStringException, eConvert,
                "String cannot be converted to bool", 0);
}


string NStr::FormatVarargs(const char* format, va_list args)
{
#ifdef HAVE_VASPRINTF
    char* s;
    int n = vasprintf(&s, format, args);
    if (n >= 0) {
        string str(s, n);
        free(s);
        return str;
    } else {
        return kEmptyStr;
    }

#elif defined(NCBI_COMPILER_GCC) && defined(NO_PUBSYNC)
    CNcbiOstrstream oss;
    oss.vform(format, args);
    return CNcbiOstrstreamToString(oss);

#elif defined(HAVE_VSNPRINTF)
    // deal with implementation quirks
    size_t size = 1024;
    AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
    buf.get()[size-1] = buf.get()[size-2] = 0;
    size_t n = vsnprintf(buf.get(), size, format, args);
    while (n >= size  ||  buf.get()[size-2]) {
        if (buf.get()[size-1]) {
            ERR_POST(Warning << "Buffer overrun by buggy vsnprintf");
        }
        size = max(size << 1, n);
        buf.reset(new char[size]);
        buf.get()[size-1] = buf.get()[size-2] = 0;
        n = vsnprintf(buf.get(), size, format, args);
    }
    return (n > 0) ? string(buf.get(), n) : kEmptyStr;

#elif defined(HAVE_VPRINTF)
    char buf[1024];
    buf[sizeof(buf) - 1] = 0;
    vsprintf(buf, format, args);
    if (buf[sizeof(buf) - 1]) {
        ERR_POST(Warning << "Buffer overrun by vsprintf");
    }
    return buf;

#else
#  error Please port this code to your system.
#endif
}


SIZE_TYPE NStr::FindNoCase(const string& str, const string& pattern,
                           SIZE_TYPE start, SIZE_TYPE end, EOccurrence where)
{
    string    pat(pattern, 0, 1);
    SIZE_TYPE l = pattern.size();
    if (isupper((unsigned char) pat[0])) {
        pat += (char) tolower((unsigned char) pat[0]);
    } else if (islower((unsigned char) pat[0])) {
        pat += (char) toupper((unsigned char) pat[0]);
    }
    if (where == eFirst) {
        SIZE_TYPE pos = str.find_first_of(pat, start);
        while (pos != NPOS  &&  pos <= end
               &&  CompareNocase(str, pos, l, pattern) != 0) {
            pos = str.find_first_of(pat, pos + 1);
        }
        return pos > end ? NPOS : pos;
    } else { // eLast
        SIZE_TYPE pos = str.find_last_of(pat, end);
        while (pos != NPOS  &&  pos >= start
               &&  CompareNocase(str, pos, l, pattern) != 0) {
            if (pos == 0) {
                return NPOS;
            }
            pos = str.find_last_of(pat, pos - 1);
        }
        return pos < start ? NPOS : pos;
    }
}


template <class TStr>
static TStr s_TruncateSpaces(const TStr& str, NStr::ETrunc where,
                             const TStr& empty_str)
{
    SIZE_TYPE length = str.length();
    if (length == 0) {
        return empty_str;
    }
    SIZE_TYPE beg = 0;
    if (where == NStr::eTrunc_Begin  ||  where == NStr::eTrunc_Both) {
        _ASSERT(beg < length);
        while ( isspace((unsigned char) str[beg]) ) {
            if (++beg == length) {
                return empty_str;
            }
        }
    }
    SIZE_TYPE end = length;
    if ( where == NStr::eTrunc_End  ||  where == NStr::eTrunc_Both ) {
        _ASSERT(end > beg);
        for (--end;  end > beg  &&  isspace((unsigned char)str[end]);  --end) {
        }
        _ASSERT(end >= beg && !isspace((unsigned char) str[end]));
        ++end;
    }
    _ASSERT(beg <= end);
    if (beg == end) {
        return empty_str;
    }
    else if ( beg  ||  (end - length) ) {
        // if either beg != 0 or end != length
        return str.substr(beg, end - beg);
    }
    else {
        return str;
    }
}


string NStr::TruncateSpaces(const string& str, ETrunc where)
{
    return s_TruncateSpaces(str, where, kEmptyStr);
}

/**
CTempString NStr::TruncateSpaces(const CTempString& str, ETrunc where)
{
    return s_TruncateSpaces(str, where, CTempString());
}
**/


void NStr::TruncateSpacesInPlace(string& str, ETrunc where)
{
    SIZE_TYPE length = str.length();
    if (length == 0) {
        return;
    }
    SIZE_TYPE beg = 0;
    if ( where == eTrunc_Begin  ||  where == eTrunc_Both ) {
        // It's better to use str.data()[] to check string characters
        // to avoid implicit modification of the string by non-const operator[]
        _ASSERT(beg < length);
        while ( isspace((unsigned char) str.data()[beg]) ) {
            if (++beg == length) {
                str.erase();
                return;
            }
        }
    }

    SIZE_TYPE end = length;
    if ( where == eTrunc_End  ||  where == eTrunc_Both ) {
        // It's better to use str.data()[] to check string characters
        // to avoid implicit modification of the string by non-const operator[]
        _ASSERT(end > beg);
        while (isspace((unsigned char) str.data()[--end])) {
            if (end == beg) {
                str.erase();
                return;
            }
        }
        _ASSERT(end >= beg  &&  !isspace((unsigned char) str.data()[end]));
        ++end;
    }
    _ASSERT(beg < end);

#if defined(NCBI_COMPILER_GCC)  &&  (NCBI_COMPILER_VERSION == 304)
    // work around a library bug
    str.replace(end, length, kEmptyStr);
    str.replace(0, beg, kEmptyStr);
#else
    if ( (beg - 0) | (end - length) ) { // if either beg != 0 or end != length
        str.replace(0, length, str, beg, end - beg);
    }
#endif
}


string& NStr::Replace(const string& src,
                      const string& search, const string& replace,
                      string& dst, SIZE_TYPE start_pos, size_t max_replace)
{
    // source and destination should not be the same
    if (&src == &dst) {
        NCBI_THROW2(CStringException, eBadArgs,
                    "NStr::Replace():  source and destination are the same",0);
    }

    dst = src;

    if ( start_pos + search.size() > src.size() ||
         search == replace )
        return dst;

    for (size_t count = 0; !(max_replace && count >= max_replace); count++) {
        start_pos = dst.find(search, start_pos);
        if (start_pos == NPOS)
            break;
        dst.replace(start_pos, search.size(), replace);
        start_pos += replace.size();
    }
    return dst;
}


string NStr::Replace(const string& src,
                     const string& search, const string& replace,
                     SIZE_TYPE start_pos, size_t max_replace)
{
    string dst;
    Replace(src, search, replace, dst, start_pos, max_replace);
    return dst;
}


string& NStr::ReplaceInPlace(string& src,
                             const string& search, const string& replace,
                             SIZE_TYPE start_pos, size_t max_replace)
{
    if ( start_pos + search.size() > src.size()  ||
         search == replace )
        return src;

    bool equal_len = (search.size() == replace.size());
    for (size_t count = 0; !(max_replace && count >= max_replace); count++) {
        start_pos = src.find(search, start_pos);
        if (start_pos == NPOS)
            break;
        // On some platforms string's replace() implementation
        // is not optimal if size of search and replace strings are equal
        if ( equal_len ) {
            copy(replace.begin(), replace.end(), src.begin() + start_pos); 
        } else {
            src.replace(start_pos, search.size(), replace);
        }
        start_pos += replace.size();
    }
    return src;
}


list<string>& NStr::Split(const string& str, const string& delim,
                          list<string>& arr, EMergeDelims merge,
                          vector<SIZE_TYPE>* token_pos)
{
    // Special cases
    if (str.empty()) {
        return arr;
    } else if (delim.empty()) {
        arr.push_back(str);
        if (token_pos)
            token_pos->push_back(0);
        return arr;
    }

    for (SIZE_TYPE pos = 0; ; ) {
        SIZE_TYPE prev_pos = (merge == eMergeDelims
                              ? str.find_first_not_of(delim, pos)
                              : pos);
        if (prev_pos == NPOS) {
            break;
        }
        pos = str.find_first_of(delim, prev_pos);
        if (pos == NPOS) {
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str, prev_pos, str.length() - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
            break;
        } else {
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos, pos - prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str, prev_pos, pos - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
            ++pos;
        }
    }
    return arr;
}


vector<string>& NStr::Tokenize(const string& str, const string& delim,
                               vector<string>& arr, EMergeDelims merge,
                               vector<SIZE_TYPE>* token_pos)
{
    // Special cases
    if (str.empty()) {
        return arr;
    } else if (delim.empty()) {
        arr.push_back(str);
        if (token_pos)
            token_pos->push_back(0);
        return arr;
    }

    SIZE_TYPE pos, prev_pos;

    // Reserve vector size only for empty vectors.
    // For vectors which already have items this usualy works slower.
    if ( !arr.size() ) {
        // Count number of tokens to determine the array size
        size_t tokens = 0;
        
        for (pos = 0;;) {
            prev_pos = (merge == NStr::eMergeDelims ? 
                            str.find_first_not_of(delim, pos) : pos);
            if (prev_pos == NPOS) {
                break;
            } 
            pos = str.find_first_of(delim, prev_pos);
            ++tokens;
            if (pos == NPOS) {
                break;
            }
            ++pos;
        }
        arr.reserve(tokens);
        if (token_pos)
            token_pos->reserve(tokens);

    }

    // Tokenization
    for (pos = 0;;) {
        prev_pos = (merge == eMergeDelims ? 
                        str.find_first_not_of(delim, pos) : pos);
        if (prev_pos == NPOS) {
            break;
        }
        pos = str.find_first_of(delim, prev_pos);
        if (pos == NPOS) {
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str, prev_pos, str.length() - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
            break;
        } else {
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos, pos - prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str, prev_pos, pos - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
            ++pos;
        }
    }
    return arr;
}


vector<string>& NStr::TokenizePattern(const string& str,
                                      const string& pattern,
                                      vector<string>& arr, EMergeDelims merge,
                                      vector<SIZE_TYPE>* token_pos)
{
    // Special cases
    if (str.empty()) {
        return arr;
    } else if (pattern.empty()) {
        arr.push_back(str);
        if (token_pos)
            token_pos->push_back(0);
        return arr;
    }

    SIZE_TYPE pos, prev_pos;

    // Reserve vector size only for empty vectors.
    // For vectors which already have items this usualy works slower.
    if ( !arr.size() ) {
        // Count number of tokens to determine the array size
        size_t tokens = 0;
        for (pos = 0, prev_pos = 0; ; ) {
            pos = str.find(pattern, prev_pos);
            if ( merge != eMergeDelims  ||  pos > prev_pos ) {
                if (pos == NPOS) {
                    if (merge != eMergeDelims  ||  
                        prev_pos < str.length() ) {
                        ++tokens;
                    }
                    break;
                }
                ++tokens;
            }
            prev_pos = pos + pattern.length();
        }
        arr.reserve(tokens);
        if (token_pos)
            token_pos->reserve(tokens);
    }

    // Tokenization
    for (pos = 0, prev_pos = 0; ; ) {
        pos = str.find(pattern, prev_pos);
        if ( merge != eMergeDelims  ||  pos > prev_pos ) {
            if (pos == NPOS) {
                if (merge != eMergeDelims  ||  
                    prev_pos < str.length() ) {
                    // Avoid using temporary objects
                    // ~ arr.push_back(str.substr(prev_pos));
                    arr.push_back(kEmptyStr);
                    arr.back().assign(str, prev_pos,
                                      str.length() - prev_pos);
                    if (token_pos)
                        token_pos->push_back(prev_pos);
                }
                break;
            }
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos, pos - prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str, prev_pos, pos - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
        }
        prev_pos = pos + pattern.length();
    }
    return arr;
}


bool NStr::SplitInTwo(const string& str, const string& delim,
                      string& str1, string& str2)
{
    SIZE_TYPE delim_pos = str.find_first_of(delim);
    if (NPOS == delim_pos) {   // only one piece.
        str1 = str;
        str2 = kEmptyStr;
        return false;
    }
    str1.assign(str, 0, delim_pos);
    // skip only one delimiter character.
    str2.assign(str, delim_pos + 1, str.length() - delim_pos - 1);
    
    return true;
}


template <typename T>
string s_NStr_Join(const T& arr, const string& delim)
{
    if (arr.empty()) {
        return kEmptyStr;
    }

    string result = arr.front();
    typename T::const_iterator it = arr.begin();
    SIZE_TYPE needed = result.size();

    while (++it != arr.end()) {
        needed += delim.size() + it->size();
    }
    result.reserve(needed);
    it = arr.begin();
    while (++it != arr.end()) {
        result += delim;
        result += *it;
    }
    return result;
}


string NStr::Join(const list<string>& arr, const string& delim)
{
    return s_NStr_Join(arr, delim);
}


string NStr::Join(const vector<string>& arr, const string& delim)
{
    return s_NStr_Join(arr, delim);
}


string NStr::PrintableString(const string&      str,
                             NStr::ENewLineMode nl_mode)
{
    ITERATE ( string, it, str ) {
        if ( !isprint((unsigned char)(*it)) || *it == '"' || *it == '\\' ) {
            // Bad character - convert via CNcbiOstrstream
            CNcbiOstrstream out;
            // Write first good characters in one chunk
            out.write(str.data(), it-str.begin());
            // Convert all other characters one by one
            do {
                if (isprint((unsigned char)(*it))) {
                    // Escape '"' and '\\' anyway
                    if ( *it == '"' || *it == '\\' )
                        out.put('\\');
                    out.put(*it);
                }
                else if (*it == '\n') {
                    // New line needs special processing
                    if (nl_mode == eNewLine_Quote) {
                        out.write("\\n", 2);
                    }
                    else {
                        out.put('\n');
                    }
                } else {
                    // All other non-printable characters need to be escaped
                    out.put('\\');
                    if (*it == '\a') {        // bell (alert)
                        out.put('a');
                    } else if (*it == '\b') { // backspace
                        out.put('b');
                    } else if (*it == '\f') { // form feed
                        out.put('f');
                    } else if (*it == '\r') { // carriage return
                        out.put('r');
                    } else if (*it == '\t') {     // horizontal tab
                        out.put('t');
                    } else if (*it == '\v') { // vertical tab
                        out.put('v');
                    } else {
                        // Hex string for non-standard codes
                        out.put('x');
                        out.put(s_Hex[(unsigned char) *it >> 4]);
                        out.put(s_Hex[(unsigned char) *it & 15]);
                    }
                }
            } while (++it < it_end); // it_end is from ITERATE macro

            // Return encoded string
            return CNcbiOstrstreamToString(out);
        }
    }
    // All characters are good - return original string
    return str;
}


string NStr::JavaScriptEncode(const string& str)
{
    ITERATE ( string, it, str ) {
        if ( !isprint((unsigned char)(*it)) || *it == '\'' || *it == '\\' ) {
            // Bad character - convert via CNcbiOstrstream
            CNcbiOstrstream out;
            // Write first good characters in one chunk
            out.write(str.data(), it-str.begin());
            // Convert all other characters one by one
            do {
               if (isprint((unsigned char)(*it))) {
                    // Escape \', \" and \\ anyway.
                    if ( *it == '\'' || *it == '"' || *it == '\\' )
                        out.put('\\');
                    out.put(*it);
                } else {
                    // All other non-printable characters need to be escaped
                    out.put('\\');
                    if (*it == '\b')        { // backspace
                        out.put('b');
                    } else if (*it == '\f') { // form feed
                        out.put('f');
                    } else if (*it == '\n') { // new line
                        out.put('n');
                    } else if (*it == '\r') { // carriage return
                        out.put('r');
                    } else if (*it == '\t') { // horizontal tab
                        out.put('t');
                    } else if (*it == '\v') { // vertical tab
                        out.put('v');
                    // Note, that not all browsers support all JS symbols
                    // (for example - \e escape), so they will be encoded
                    // to hex format.
                    } else {
                        // Hex string for non-standard codes.
                        out.put('x');
                        out.put(s_Hex[(unsigned char) *it >> 4]);
                        out.put(s_Hex[(unsigned char) *it & 15]);
                    }
                }
            } while (++it < it_end); // it_end is from ITERATE macro

            // Return encoded string
            return CNcbiOstrstreamToString(out);
        }
    }
    // All characters are good - return original string
    return str;
}


string NStr::ParseEscapes(const string& str)
{
    string out;
    out.reserve(str.size()); // can only be smaller
    SIZE_TYPE pos = 0;

    while (pos < str.size()) {
        SIZE_TYPE pos2 = str.find('\\', pos);
        if (pos2 == NPOS) {
            out += str.substr(pos);
            break;
        }
        out += str.substr(pos, pos2 - pos);
        if (++pos2 == str.size()) {
            NCBI_THROW2(CStringException, eFormat,
                        "Unterminated escape sequence", pos2);
        }
        switch (str[pos2]) {
        case 'a':  out += '\a';  break;
        case 'b':  out += '\b';  break;
        case 'f':  out += '\f';  break;
        case 'n':  out += '\n';  break;
        case 'r':  out += '\r';  break;
        case 't':  out += '\t';  break;
        case 'v':  out += '\v';  break;
        case 'x':
        {
            pos = pos2 + 1;
            while (pos2 <= pos  &&  pos2 + 1 < str.size()
                   &&  isxdigit((unsigned char) str[pos2 + 1])) {
                ++pos2;
            }
            if (pos2 >= pos) {
                out += static_cast<char>
                    (StringToUInt(str.substr(pos, pos2 - pos + 1), 0, 16));
            } else {
                NCBI_THROW2(CStringException, eFormat,
                            "\\x used with no following digits", pos);
            }
            break;
        }
        case '0':  case '1':  case '2':  case '3':
        case '4':  case '5':  case '6':  case '7':
        {
            pos = pos2;
            unsigned char c = str[pos2] - '0';
            while (pos2 < pos + 3  &&  pos2 + 1 < str.size()
                   &&  str[pos2 + 1] >= '0'  &&  str[pos2 + 1] <= '7') {
                c = (c << 3) | (str[++pos2] - '0');
            }
            out += c;
        }
        default:
            out += str[pos2];
        }
        pos = pos2 + 1;
    }
    return out;
}


// Determines the end of an HTML <...> tag, accounting for attributes
// and comments (the latter allowed only within <!...>).
static SIZE_TYPE s_EndOfTag(const string& str, SIZE_TYPE start)
{
    _ASSERT(start < str.size()  &&  str[start] == '<');
    bool comments_ok = (start + 1 < str.size()  &&  str[start + 1] == '!');
    for (SIZE_TYPE pos = start + 1;  pos < str.size();  ++pos) {
        switch (str[pos]) {
        case '>': // found the end
            return pos;

        case '\"': // start of "string"; advance to end
            pos = str.find('\"', pos + 1);
            if (pos == NPOS) {
                NCBI_THROW2(CStringException, eFormat,
                            "Unclosed string in HTML tag", start);
                // return pos;
            }
            break;

        case '-': // possible start of -- comment --; advance to end
            if (comments_ok  &&  pos + 1 < str.size()
                &&  str[pos + 1] == '-') {
                pos = str.find("--", pos + 2);
                if (pos == NPOS) {
                    NCBI_THROW2(CStringException, eFormat,
                                "Unclosed comment in HTML tag", start);
                    // return pos;
                } else {
                    ++pos;
                }
            }
        }
    }
    NCBI_THROW2(CStringException, eFormat, "Unclosed HTML tag", start);
    // return NPOS;
}


// Determines the end of an HTML &foo; character/entity reference
// (which might not actually end with a semicolon :-/)
static SIZE_TYPE s_EndOfReference(const string& str, SIZE_TYPE start)
{
    _ASSERT(start < str.size()  &&  str[start] == '&');
#ifdef NCBI_STRICT_HTML_REFS
    return str.find(';', start + 1);
#else
    SIZE_TYPE pos = str.find_first_not_of
        ("#0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
         start + 1);
    if (pos == NPOS  ||  str[pos] == ';') {
        return pos;
    } else {
        return pos - 1;
    }
#endif
}


static SIZE_TYPE s_VisibleWidth(const string& str, bool is_html)
{
    if (is_html) {
        SIZE_TYPE width = 0, pos = 0;
        for (;;) {
            SIZE_TYPE pos2 = str.find_first_of("<&", pos);
            if (pos2 == NPOS) {
                width += str.size() - pos;
                break;
            } else {
                width += pos2 - pos;
                if (str[pos2] == '&') {
                    ++width;
                    pos = s_EndOfReference(str, pos);
                } else {
                    pos = s_EndOfTag(str, pos);
                }
                if (pos == NPOS) {
                    break;
                } else {
                    ++pos;
                }
            }
        }
        return width;
    } else {
        return str.size();
    }
}


list<string>& NStr::Wrap(const string& str, SIZE_TYPE width,
                         list<string>& arr, NStr::TWrapFlags flags,
                         const string* prefix, const string* prefix1)
{
    if (prefix == 0) {
        prefix = &kEmptyStr;
    }

    const string* pfx = prefix1 ? prefix1 : prefix;
    SIZE_TYPE     pos = 0, len = str.size();
    
    bool          is_html  = flags & fWrap_HTMLPre ? true : false;
    bool          do_flat = (flags & fWrap_FlatFile) != 0;

    enum EScore { // worst to best
        eForced,
        ePunct,
        eComma,
        eSpace,
        eNewline
    };

    while (pos < len) {
        bool      hyphen     = false; // "-" or empty
        SIZE_TYPE column     = s_VisibleWidth(*pfx, is_html);
        SIZE_TYPE column0    = column;
        // the next line will start at best_pos
        SIZE_TYPE best_pos   = NPOS;
        EScore    best_score = eForced;
        SIZE_TYPE pos0       = pos;
        SIZE_TYPE nl_pos     = str.find('\n', pos);
        if (nl_pos == NPOS) {
            nl_pos = len;
        }
        if (column + (nl_pos-pos) <= width) {
            pos0 = nl_pos;
        }
        for (SIZE_TYPE pos2 = pos0;  pos2 < len  &&  column <= width;
             ++pos2, ++column) {
            EScore    score     = eForced;
            SIZE_TYPE score_pos = pos2;
            char      c         = str[pos2];

            if (c == '\n') {
                best_pos   = pos2;
                best_score = eNewline;
                break;
            } else if (isspace((unsigned char) c)) {
                if ( !do_flat  &&  pos2 > 0  &&
                     isspace((unsigned char) str[pos2 - 1])) {
                    continue; // take the first space of a group
                }
                score = eSpace;
            } else if (is_html  &&  c == '<') {
                // treat tags as zero-width...
                pos2 = s_EndOfTag(str, pos2);
                --column;
            } else if (is_html  &&  c == '&') {
                // ...and references as single characters
                pos2 = s_EndOfReference(str, pos2);
            } else if (c == ','  &&  score_pos < len - 1  &&  column < width) {
                score = eComma;
                ++score_pos;
            } else if (do_flat ? c == '-' : ispunct((unsigned char) c)) {
                // For flat files, only whitespace, hyphens and commas
                // are special.
                if (c == '('  ||  c == '['  ||  c == '{'  ||  c == '<'
                    ||  c == '`') { // opening element
                    score = ePunct;
                } else if (score_pos < len - 1  &&  column < width) {
                    // Prefer breaking *after* most types of punctuation.
                    score = ePunct;
                    ++score_pos;
                }
            }

            if (pos2 == NPOS) {
                break;
            }

            if (score >= best_score) {
                best_pos   = score_pos;
                best_score = score;
            }

            while (pos2 < len - 1  &&  str[pos2 + 1] == '\b') {
                // Account for backspaces
                ++pos2;
                if (column > column0) {
                    --column;
                }
            }
        }

        if ( best_score != eNewline  &&  column <= width ) {
            // If the whole remaining text can fit, don't split it...
            best_pos = len;
        } else if ( best_score == eForced  &&  (flags & fWrap_Hyphenate) ) {
            hyphen = true;
            --best_pos;
        }
        arr.push_back(*pfx);
        {{ // eat backspaces and the characters (if any) that precede them
            string::const_iterator begin = str.begin() + pos;
            string::const_iterator end = str.begin() + best_pos;
            string::const_iterator bs; // position of next backspace
            while ((bs = find(begin, end, '\b')) != end) {
                if (bs != begin) {
                    // add all except the last one
                    arr.back().append(begin, bs - 1);
                }
                else {
                    // The backspace is at the beginning of next substring,
                    // so we should remove previously added symbol if any.
                    SIZE_TYPE size = arr.back().size();
                    if (size > pfx->size()) { // current size > prefix size
                        arr.back().resize(size - 1);
                    }
                }
                // skip over backspace
                begin = bs + 1;
            }
            if (begin != end) {
                // add remaining characters
                arr.back().append(begin, end);
            }
        }}
        if ( hyphen ) {
            arr.back() += '-';
        }
        pos = best_pos;
        pfx = prefix;

        if ( best_score == eSpace  ||  best_score == eNewline ) {
            ++pos;
        }
        while (pos < len  &&  str[pos] == '\b') {
            ++pos;
        }
    }

    return arr;
}


list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string* prefix,
                             const string* prefix1)
{
    if (l.empty()) {
        return arr;
    }

    const string* pfx      = prefix1 ? prefix1 : prefix;
    string        s        = *pfx;
    bool          is_html  = flags & fWrap_HTMLPre ? true : false;
    SIZE_TYPE     column   = s_VisibleWidth(s,     is_html);
    SIZE_TYPE     delwidth = s_VisibleWidth(delim, is_html);
    bool          at_start = true;

    ITERATE (list<string>, it, l) {
        SIZE_TYPE term_width = s_VisibleWidth(*it, is_html);
        if ( at_start ) {
            if (column + term_width <= width) {
                s += *it;
                column += term_width;
                at_start = false;
            } else {
                // Can't fit, even on its own line; break separately.
                Wrap(*it, width, arr, flags, prefix, pfx);
                pfx      = prefix;
                s        = *prefix;
                column   = s_VisibleWidth(s, is_html);
                at_start = true;
            }
        } else if (column + delwidth + term_width <= width) {
            s += delim;
            s += *it;
            column += delwidth + term_width;
            at_start = false;
        } else {
            // Can't fit on this line; break here and try again.
            arr.push_back(s);
            pfx      = prefix;
            s        = *prefix;
            column   = s_VisibleWidth(s, is_html);
            at_start = true;
            --it;
        }
    }

    arr.push_back(s);
    return arr;
}


#if !defined(HAVE_STRDUP)
extern char* strdup(const char* str)
{
    if ( !str ) {
        return 0;
    }
    size_t size   = strlen(str) + 1;
    void*  result = malloc(size);
    return (char*) (result ? memcpy(result, str, size) : 0);
}
#endif

/////////////////////////////////////////////////////////////////////////////
//  CStringUTF8

SIZE_TYPE CStringUTF8::GetSymbolCount(void) const
{
    SIZE_TYPE count = 0;
    for (const char* src = c_str(); *src; ++src, ++count) {
        size_t more=0;
        bool good = x_EvalFirst(*src, more);
        while (more-- && good) {
            good = x_EvalNext(*(++src));
        }
        if ( !good ) {
            NCBI_THROW2(CStringException, eFormat,
                        "String is not in UTF8 format",
                        s_DiffPtr(src,c_str()));
        }
    }
    return count;
}

SIZE_TYPE CStringUTF8::GetValidSymbolCount(const char* src, SIZE_TYPE buf_size)
{
    SIZE_TYPE count = 0, cur_size=0;
    for (; *src && cur_size < buf_size; ++src, ++count, ++cur_size) {
        size_t more=0;
        bool good = x_EvalFirst(*src, more);
        while (more-- && good && ++cur_size < buf_size) {
            good = x_EvalNext(*(++src));
        }
        if ( !good ) {
            return count;
        }
    }
    return count;
}

SIZE_TYPE CStringUTF8::GetValidBytesCount(const char* src, SIZE_TYPE buf_size)
{
    SIZE_TYPE count = 0, cur_size=0;
    for (; *src && cur_size < buf_size; ++src, ++count, ++cur_size) {
        size_t more=0;
        bool good = x_EvalFirst(*src, more);
        while (more-- && good && cur_size < buf_size) {
            good = x_EvalNext(*(++src));
            if (good) {
                ++cur_size;
            }
        }
        if ( !good ) {
            return cur_size;
        }
    }
    return cur_size;
}

string CStringUTF8::AsSingleByteString(EEncoding encoding) const
{
    string result;
    result.reserve( GetSymbolCount()+1 );
    for ( const char* src = c_str(); *src; ++src ) {
        result.append(1, SymbolToChar( Decode( src ), encoding));
    }
    return result;
}

#if defined(HAVE_WSTRING)
wstring CStringUTF8::AsUnicode(void) const
{
    TUnicodeSymbol maxw = (TUnicodeSymbol)numeric_limits<wchar_t>::max();
    wstring result;
    result.reserve( GetSymbolCount()+1 );
    for (const char* src = c_str(); *src; ++src) {
        TUnicodeSymbol ch = Decode(src);
        if (ch > maxw) {
            NCBI_THROW2(CStringException, eConvert,
                        "Failed to convert symbol to wide character",
                        s_DiffPtr(src, c_str()));
        }
        result.append(1, ch);
    }
    return result;
}
#endif // HAVE_WSTRING

EEncoding CStringUTF8::GuessEncoding( const char* src)
{
    size_t more=0;
    bool cp1252, iso1, ascii, utf8;
    for (cp1252 = iso1 = ascii = utf8 = true; *src; ++src) {
        Uint1 ch = *src;
        bool skip = false;
        if (more != 0) {
            if (x_EvalNext(ch)) {
                --more;
                if (more == 0) {
                    ascii = cp1252 = iso1 = false;
                }
                skip = true;
            } else {
                more = 0;
                utf8 = false;
            }
        }
        if (ch > 0x7F) {
            ascii = false;
            if (ch < 0xA0) {
                iso1 = false;
                if (ch == 0x81 || ch == 0x8D || ch == 0x8F ||
                    ch == 0x90 || ch == 0x9D) {
                    cp1252 = false;
                }
            }
            if (!skip && utf8 && !x_EvalFirst(ch, more)) {
                utf8 = false;
            }
        }
    }
    if (more != 0) {
        utf8 = false;
    }
    if (ascii) {
        return eEncoding_Ascii;
    } else if (cp1252) {
        return iso1 ? eEncoding_ISO8859_1 : eEncoding_Windows_1252;
    } else if (utf8) {
        return eEncoding_UTF8;
    }
    return eEncoding_Unknown;
 }

bool CStringUTF8::MatchEncoding( const char* src, EEncoding encoding)
{
    bool matches = false;
    EEncoding enc_src = GuessEncoding(src);
    switch ( enc_src ) {
    default:
    case eEncoding_Unknown:
        matches = false;
        break;
    case eEncoding_Ascii:
        matches = true;
        break;
    case eEncoding_UTF8:
    case eEncoding_Windows_1252:
        matches = (encoding == enc_src);
        break;
    case eEncoding_ISO8859_1:
        matches = (encoding == enc_src || encoding == eEncoding_Windows_1252);
        break;
    }
    return matches;
}

// cp1252, codepoints for chars 0x80 to 0x9F
static const TUnicodeSymbol s_cp1252_table[] = {
    0x20AC, 0x003F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003F, 0x017D, 0x003F,
    0x003F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003F, 0x017E, 0x0178
};

TUnicodeSymbol CStringUTF8::CharToSymbol(char c, EEncoding encoding)
{
    Uint1 ch = c;
    switch (encoding)
    {
    case eEncoding_Unknown:
    case eEncoding_UTF8:
        NCBI_THROW2(CStringException, eBadArgs,
                    "Unacceptable character encoding", 0);
    case eEncoding_Ascii:
    case eEncoding_ISO8859_1:
        break;
    case eEncoding_Windows_1252:
        if (ch > 0x7F && ch < 0xA0) {
            return s_cp1252_table[ ch - 0x80 ];
        }
        break;
    default:
        NCBI_THROW2(CStringException, eBadArgs,
                    "Unsupported character encoding", 0);
    }
    return (TUnicodeSymbol)ch;
}

char CStringUTF8::SymbolToChar(TUnicodeSymbol cp, EEncoding encoding)
{
    if( encoding == eEncoding_UTF8 || encoding == eEncoding_Unknown) {
        NCBI_THROW2(CStringException, eBadArgs,
                    "Unacceptable character encoding", 0);
    }
    if ( cp <= 0xFF) {
        return (char)cp;
    }
    if ( encoding == eEncoding_Windows_1252 ) {
        for (Uint1 ch = 0x80; ch <= 0x9F; ++ch) {
            if (s_cp1252_table[ ch - 0x80 ] == cp) {
                return (char)ch;
            }
        }
    }
    if (cp > 0xFF) {
        NCBI_THROW2(CStringException, eConvert,
                    "Failed to convert symbol to requested encoding", 0);
    }
    return (char)cp;
}

void CStringUTF8::x_Validate(void) const
{
    if (!IsValid()) {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
}

void CStringUTF8::x_AppendChar(TUnicodeSymbol c)
{
    Uint4 ch = c;
    if (ch < 0x80) {
        append(1, Uint1(ch));
    }
    else if (ch < 0x800) {
        append(1, Uint1( (ch >>  6)         | 0xC0));
        append(1, Uint1( (ch        & 0x3F) | 0x80));
    } else if (ch < 0x10000) {
        append(1, Uint1( (ch >> 12)         | 0xE0));
        append(1, Uint1(((ch >>  6) & 0x3F) | 0x80));
        append(1, Uint1(( ch        & 0x3F) | 0x80));
    } else {
        append(1, Uint1( (ch >> 18)         | 0xF0));
        append(1, Uint1(((ch >> 12) & 0x3F) | 0x80));
        append(1, Uint1(((ch >>  6) & 0x3F) | 0x80));
        append(1, Uint1( (ch        & 0x3F) | 0x80));
    }
}

void CStringUTF8::x_Append(const char* src,
                           EEncoding encoding, EValidate validate)
{
    if (encoding == eEncoding_Unknown) {
        encoding = GuessEncoding(src);
        if (encoding == eEncoding_Unknown) {
            NCBI_THROW2(CStringException, eBadArgs,
                "Unable to guess the source string encoding", 0);
        }
    } else if (validate == eValidate) {
        if ( !MatchEncoding( src,encoding ) ) {
            NCBI_THROW2(CStringException, eBadArgs,
                "Source string does not match the declared encoding", 0);
        }
    }
    if (encoding == eEncoding_UTF8 || encoding == eEncoding_Ascii) {
        append(src);
        return;
    }

    const char* srcBuf;
    size_t needed = 0;
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        needed += x_BytesNeeded( CharToSymbol( *srcBuf,encoding ) );
    }
    if ( !needed ) {
        return;
    }
    reserve(length()+needed+1);
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        x_AppendChar( CharToSymbol( *srcBuf, encoding ) );
    }
}


#if defined(HAVE_WSTRING)
void CStringUTF8::x_Append(const wchar_t* src)
{
    const wchar_t* srcBuf;
    size_t needed = 0;

    for (srcBuf = src; *srcBuf; ++srcBuf) {
        needed += x_BytesNeeded( *srcBuf );
    }
    if ( !needed ) {
        return;
    }
    reserve(length()+needed+1);
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        x_AppendChar( *srcBuf );
    }
}
#endif // HAVE_WSTRING

size_t CStringUTF8::x_BytesNeeded(TUnicodeSymbol c)
{
    Uint4 ch = c;
    if (ch < 0x80) {
        return 1;
    } else if (ch < 0x800) {
        return 2;
    } else if (ch < 0x10000) {
        return 3;
    }
    return 4;
}

bool CStringUTF8::x_EvalFirst(char ch, size_t& more)
{
    more = 0;
    if ((ch & 0x80) != 0) {
        if ((ch & 0xE0) == 0xC0) {
            more = 1;
        } else if ((ch & 0xF0) == 0xE0) {
            more = 2;
        } else if ((ch & 0xF8) == 0xF0) {
            more = 3;
        } else {
            return false;
        }
    }
    return true;
}

bool CStringUTF8::x_EvalNext(char ch)
{
    return (ch & 0xC0) == 0x80;
}

TUnicodeSymbol CStringUTF8::Decode(const char*& src)
{
    TUnicodeSymbol chRes;
    size_t more;
    Uint1 ch = *src;
    if ((ch & 0x80) == 0) {
        chRes = ch;
        more = 0;
    } else if ((ch & 0xE0) == 0xC0) {
        chRes = (ch & 0x1F);
        more = 1;
    } else if ((ch & 0xF0) == 0xE0) {
        chRes = (ch & 0x0F);
        more = 2;
    } else if ((ch & 0xF8) == 0xF0) {
        chRes = (ch & 0x07);
        more = 3;
    } else {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
    while (more--) {
        ch = *(++src);
        if ((ch & 0xC0) != 0x80) {
            NCBI_THROW2(CStringException, eBadArgs,
                "Source string is not in UTF8 format", 0);
        }
        chRes = (chRes << 6) | (ch & 0x3F);
    }
    return chRes;
}

TUnicodeSymbol CStringUTF8::DecodeFirst(char ch, size_t& more)
{
    TUnicodeSymbol chRes = 0;
    more = 0;
    if ((ch & 0x80) == 0) {
        chRes = ch;
    } else if ((ch & 0xE0) == 0xC0) {
        chRes = (ch & 0x1F);
        more = 1;
    } else if ((ch & 0xF0) == 0xE0) {
        chRes = (ch & 0x0F);
        more = 2;
    } else if ((ch & 0xF8) == 0xF0) {
        chRes = (ch & 0x07);
        more = 3;
    }
    return chRes;
}

TUnicodeSymbol CStringUTF8::DecodeNext(TUnicodeSymbol chU, char ch)
{
    if ((ch & 0xC0) == 0x80) {
        return (chU << 6) | (ch & 0x3F);
    }
    return 0;
}

const char* CStringException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eConvert:  return "eConvert";
    case eBadArgs:  return "eBadArgs";
    case eFormat:   return "eFormat";
    default:    return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.191  2006/12/22 17:49:12  dicuccio
 * Fix typo in logic in s_TruncateSpaces()
 *
 * Revision 1.190  2006/12/22 13:01:52  dicuccio
 * Temporarily remove new TruncateSpaces() prototype
 *
 * Revision 1.189  2006/12/22 12:43:22  dicuccio
 * Split CTempString into its own header.  Added NStr::TruncateSpaces() variant to
 * handle CTempString.
 *
 * Revision 1.188  2006/12/20 13:22:59  dicuccio
 * Further updates to CTempString:
 * - Code rearrangements and patches from Eugene Vasilchenko: Unified
 *   initialization; reordered functions for better inlining.
 * - Implemented find()
 *
 * Revision 1.187  2006/12/18 13:01:26  dicuccio
 * Make CTempString more congruent with std::string
 *
 * Revision 1.186  2006/11/29 13:56:29  gouriano
 * Moved GetErrorCodeString method into cpp
 *
 * Revision 1.185  2006/10/24 18:44:20  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.184  2006/09/19 14:27:56  gouriano
 * Corrected exception thrown in CStringUTF8::AsUnicode
 *
 * Revision 1.183  2006/09/18 20:49:05  ucko
 * +<corelib/ncbi_limits.hpp> for numeric_limits<>.
 * CStringUTF8::AsUnicode: correct exception text, as wide characters may
 * be UCS-4 rather than UTF-16 (and the code doesn't handle characters
 * that would need surrogates anyway).
 *
 * Revision 1.182  2006/09/18 15:09:36  gouriano
 * Check numeric limits when converting UTF8 string to UNICODE
 *
 * Revision 1.181  2006/09/11 13:07:53  gouriano
 * Added buffer validation into CStringUTF8
 *
 * Revision 1.180  2006/08/23 13:32:57  ivanov
 * + NStr::ReplaceInPlace()
 *
 * Revision 1.179  2006/07/10 19:26:23  ivanov
 * Tune S2N_CONVERT_ERROR macro for a better performance
 *
 * Revision 1.178  2006/07/10 15:02:38  ivanov
 * Improved StringToXxx exception messages
 *
 * Revision 1.177  2006/04/19 18:38:40  ivanov
 * Added additional optional parameter to Split(), Tokenize() and
 * TokenizePattern() to get tokens' positions in source string
 *
 * Revision 1.176  2006/04/03 19:23:07  ivanov
 * NStr::TokenizePattern() -- fixed bug with processing last
 * token in the string
 *
 * Revision 1.175  2006/03/28 16:31:12  ivanov
 * Some comment spellings
 *
 * Revision 1.174  2006/02/13 19:47:35  ucko
 * Tweak CVS log to avoid accidental comment-end markers.
 *
 * Revision 1.173  2006/02/13 19:34:33  ivanov
 * Tokenize*() -- do not use reserve() for vectors, which already have
 * items, usually this works mutch slower.
 * Split* /Tokenize*() -- avoid using temporary string objects,
 * use assign() instead.
 *
 * Revision 1.172  2006/02/06 15:47:09  ivanov
 * Replaced class-based NStr::TStringToNumFlags to int-based counterparts
 *
 * Revision 1.171  2006/01/03 17:39:39  ivanov
 * NStr:StringToDouble() added support for TStringToNumFlags flags
 *
 * Revision 1.170  2005/12/28 15:39:24  ivanov
 * NStr::StringTo* - allow digits as trailing symbols right after non-digits
 *
 * Revision 1.169  2005/11/16 18:52:18  vakatov
 * NStr::[U]Int8ToString() -- remember to use the "base" argument.
 *
 * Revision 1.168  2005/11/15 16:14:11  vakatov
 * + CHECK_RANGE_U() to heed ICC warning "comparing unsigned with zero"
 *
 * Revision 1.167  2005/10/27 15:53:21  gouriano
 * Further enhancements of CStringUTF8
 *
 * Revision 1.166  2005/10/21 17:35:33  gouriano
 * Enhanced CStringUTF8
 *
 * Revision 1.165  2005/10/19 12:04:07  ivanov
 * Removed obsolete NStr::StringTo*() methods.
 * NStr::*ToString() -- added new radix base parameter instead of flags.
 *
 * Revision 1.164  2005/10/17 18:24:38  ivanov
 * Allow NStr::*ToString() convert numbers using fBinary format
 *
 * Revision 1.163  2005/10/17 13:49:11  ivanov
 * Allow NStr::*ToString() convert numbers using octal and hex formats.
 *
 * Revision 1.162  2005/10/03 14:10:37  gouriano
 * Corrected CStringUTF8 class
 *
 * Revision 1.161  2005/08/25 18:57:25  ivanov
 * Moved JavaScriptEncode() from CHTMLHelper:: to NStr::.
 * Changed \" processing.
 *
 * Revision 1.160  2005/08/05 21:26:24  vakatov
 * OBSOLETE_FLAGS() -- report only once, and with the function name
 *
 * Revision 1.159  2005/08/04 11:08:59  ivanov
 * Revamp of NStr::StringTo*() functions
 *
 * Revision 1.158  2005/06/13 18:23:58  lavr
 * #include <corelib/ncbimisc.hpp> instead of <ctype.h>
 *
 * Revision 1.157  2005/06/10 19:50:31  lavr
 * Beautify string->number trailing pointer exception message
 *
 * Revision 1.156  2005/06/06 18:59:34  lavr
 * Fix previous commit
 *
 * Revision 1.155  2005/06/06 15:29:05  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.154  2005/06/03 16:41:52  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.153  2005/05/26 20:24:25  lavr
 * Accurately assert errno in StringToXXX() when not throwing on error
 *
 * Revision 1.152  2005/05/19 19:21:02  shomrat
 * Bug fix in TruncateSpacesInPlace
 *
 * Revision 1.151  2005/05/18 15:23:28  shomrat
 * Added starting position to IsBlank
 *
 * Revision 1.150  2005/05/17 15:51:48  ivanov
 * s_DataSizeConvertQual(): throw exception only when this specified
 * by 'on_error' parameter
 *
 * Revision 1.149  2005/05/13 16:24:26  ivanov
 * NStr::StringTo[U]Int8 -- fixed limits checks
 *
 * Revision 1.148  2005/05/13 16:12:39  vasilche
 * Enabled int chunk printing.
 * Correctly pass bool sign in flags.
 *
 * Revision 1.147  2005/05/13 13:59:39  vasilche
 * Fixed conversion of int to string using int chunks.
 * Allow configurable int chunks in conversion.
 *
 * Revision 1.146  2005/05/13 12:54:22  ivanov
 * NStr::StringToDouble() -- allow only one dot in the converted string.
 * NStr::StringTo[U]Int8() -- added check on empty string.
 *
 * Revision 1.145  2005/05/13 11:27:21  ivanov
 * Fixed MSVC compilation warnings
 *
 * Revision 1.144  2005/05/12 17:00:20  vasilche
 * Use handmade code for *Int*ToString() conversions.
 * Fixed Int8ToString(kMin_I8) conversion.
 * UInt*ToString(*, TIntToStringFlags fmt) add '+' if fSign is set.
 *
 * Revision 1.143  2005/05/12 15:07:13  lavr
 * Use explicit (unsigned char) conversion in <ctype.h>'s macros
 *
 * Revision 1.142  2005/05/12 13:55:00  ucko
 * Optimize out temporary C++ string objects introduced in a recent
 * change to IntToString.
 *
 * Revision 1.141  2005/05/12 12:06:52  ivanov
 * Fixed previous commit
 *
 * Revision 1.140  2005/05/12 11:14:04  ivanov
 * Added NStr::*Int*ToString() version with flags parameter.
 *
 * Revision 1.139  2005/04/29 14:46:43  ivanov
 * Restoring changes in the NStr::Tokenize()
 *
 * Revision 1.138  2005/04/29 14:41:26  ivanov
 * + NStr::TokenizePattern(). Minor changes in the NStr::Tokenize().
 *
 * Revision 1.137  2005/04/07 16:28:00  vasilche
 * Take care of several sequential backspaces in Wrap().
 * Allow return class value optimization in Replace().
 * Optimized TruncateSpaces() and TruncateSpacesInPlace().
 *
 * Revision 1.136  2005/04/07 13:47:59  shomrat
 * Wrap optimization
 *
 * Revision 1.135  2005/03/24 16:40:36  ucko
 * Streamline Wrap a bit more.
 *
 * Revision 1.134  2005/03/16 15:28:30  ivanov
 * MatchesMask(): Added parameter for case sensitive/insensitive matching
 *
 * Revision 1.133  2005/02/16 15:04:35  ssikorsk
 * Tweaked kEmptyStr with Linux GCC
 *
 * Revision 1.132  2005/02/01 21:47:14  grichenk
 * Fixed warnings
 *
 * Revision 1.131  2005/01/05 16:55:01  ivanov
 * Added string version of NStr::MatchesMask()
 *
 * Revision 1.130  2004/12/28 21:19:20  grichenk
 * Static strings changed to char*
 *
 * Revision 1.129  2004/12/20 22:36:47  ucko
 * TruncateSpacesInPlace: when built with GCC 3.0.4, don't attempt to
 * replace() str with a portion of itself, as that can yield incorrect
 * results.
 *
 * Revision 1.128  2004/11/24 15:30:20  ucko
 * Simplify logic in Wrap slightly.
 *
 * Revision 1.127  2004/11/24 15:17:02  shomrat
 * Implemented flat-file specific line wrap
 *
 * Revision 1.126  2004/11/23 17:04:40  ucko
 * Roll back changes to Wrap in revisions 1.118 and 1.120.
 *
 * Revision 1.125  2004/10/21 18:16:24  ucko
 * NStr::Wrap: don't loop forever in HTML mode if the string ends with
 * an unterminated entity reference.
 *
 * Revision 1.124  2004/10/15 12:01:12  ivanov
 * Added 's_' to names of local static functions.
 * Renamed NStr::StringToUInt_DataSize -> NStr::StringToUInt8_DataSize.
 * Added additional arguments to NStr::StringTo[U]Int8 to select radix
 * (now it is not fixed with predefined values, and can be arbitrary)
 * and action on not permitted trailing symbols in the converting string.
 *
 * Revision 1.123  2004/10/13 13:08:57  ivanov
 * NStr::DataSizeConvertQual() -- changed return type to Uint8.
 * NStr::StringToUInt_DataSize() -- added check on overflow.
 *
 * Revision 1.122  2004/10/05 16:35:51  shomrat
 * in place TruncateSpaces changed to TruncateSpacesInPlace
 *
 * Revision 1.121  2004/10/05 16:13:19  shomrat
 * + in place TruncateSpaces
 *
 * Revision 1.120  2004/10/05 16:07:04  shomrat
 * Changed Wrap
 *
 * Revision 1.119  2004/10/04 14:27:31  ucko
 * Treat all letters as lowercase for case-insensitive comparisons.
 *
 * Revision 1.118  2004/10/01 15:18:25  shomrat
 * + IsBlank; changes to string wrap
 *
 * Revision 1.117  2004/09/22 16:01:30  ivanov
 * CHECK_ENDPTR macro -- throw exception only if specified, otherwise return 0
 *
 * Revision 1.116  2004/09/22 13:52:36  kuznets
 * Code cleanup
 *
 * Revision 1.115  2004/09/21 18:45:03  kuznets
 * SoftStringToUInt renamed StringToUInt_DataSize
 *
 * Revision 1.114  2004/09/21 18:23:59  kuznets
 * +NStr::SoftStringToUInt KB, MB converter
 *
 * Revision 1.113  2004/08/24 15:23:14  shomrat
 * comma has higher priority over other punctuaution chars
 *
 * Revision 1.112  2004/07/20 18:44:39  ucko
 * Split, Tokenize: yield no elements for empty strings.
 *
 * Revision 1.111  2004/06/30 21:58:05  vasilche
 * Fixed wrong cast in wchar to utf8 conversion.
 *
 * Revision 1.110  2004/06/21 12:14:50  ivanov
 * Added additional parameter for all StringToXxx() function that specify
 * an action which will be performed on conversion error: to throw an
 * exception, or just to return zero.
 *
 * Revision 1.109  2004/05/26 19:21:25  ucko
 * FindNoCase: avoid looping in eLastMode when there aren't any full
 * matches but the first character of the string matches the first
 * character of the pattern.
 *
 * Revision 1.108  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.107  2004/03/11 18:49:48  gorelenk
 * Removed(condionaly) implementation of class CNcbiEmptyString.
 *
 * Revision 1.106  2004/03/05 12:26:43  ivanov
 * Moved CDirEntry::MatchesMask() to NStr class.
 *
 * Revision 1.105  2004/03/04 13:38:57  kuznets
 * + set of ToString conversion functions taking outout string as a parameter,
 * not a return value (should give a performance advantage in some cases)
 *
 * Revision 1.104  2004/02/19 16:44:55  vasilche
 * WorkShop compiler doesn't support static templates.
 *
 * Revision 1.103  2004/02/18 20:54:47  shomrat
 * bug fix (pos -> pos2)
 *
 * Revision 1.102  2003/12/12 20:06:44  rsmith
 * Take out un-needed include of stdarg.h (included in ncbistr.hpp).
 *
 * Revision 1.101  2003/12/12 20:04:24  rsmith
 * make sure stdarg.h is included to define va_list.
 *
 * Revision 1.100  2003/12/12 17:26:54  ucko
 * +FormatVarargs
 *
 * Revision 1.99  2003/12/01 20:45:47  ucko
 * Extend Join to handle vectors as well as lists (common code templatized).
 * Add ParseEscapes (inverse of PrintableString).
 *
 * Revision 1.98  2003/10/31 13:15:20  lavr
 * Fix typos in the log of the previous commit :-)
 *
 * Revision 1.97  2003/10/31 12:59:46  lavr
 * Better diagnostics messages from exceptions; some other cosmetic changes
 *
 * Revision 1.96  2003/10/03 15:16:02  ucko
 * NStr::Join: preallocate as much space as we need for result.
 *
 * Revision 1.95  2003/09/17 15:18:29  vasilche
 * Reduce memory allocations in NStr::PrintableString()
 *
 * Revision 1.94  2003/08/19 15:17:20  rsmith
 * Add NStr::SplitInTwo() function.
 *
 * Revision 1.93  2003/06/16 15:19:03  ucko
 * FindNoCase: always honor both start and end (oops).
 *
 * Revision 1.92  2003/05/22 20:09:29  gouriano
 * added UTF8 strings
 *
 * Revision 1.91  2003/05/14 21:52:09  ucko
 * Move FindNoCase out of line and reimplement it to avoid making
 * lowercase copies of both strings.
 *
 * Revision 1.90  2003/03/25 22:15:40  lavr
 * NStr::PrintableString():: Print NUL char as \x00 instead of \0
 *
 * Revision 1.89  2003/03/20 13:27:52  dicuccio
 * Oops.  Removed old code wrapped in #if 0...#endif.
 *
 * Revision 1.88  2003/03/20 13:27:11  dicuccio
 * Changed NStr::StringToPtr() - now symmetric with NSrt::PtrToString (there were
 * too many special cases).
 *
 * Revision 1.87  2003/03/17 12:49:26  dicuccio
 * Fixed indexing error in NStr::PtrToString() - buffer is 0-based index, not
 * 1-based
 *
 * Revision 1.86  2003/03/11 16:57:12  ucko
 * Process backspaces in NStr::Wrap, allowing in particular the use of
 * " \b" ("-\b") to indicate mid-word break (hyphenation) points.
 *
 * Revision 1.85  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.84  2003/03/04 00:02:21  vakatov
 * NStr::PtrToString() -- use runtime check for "0x"
 * NStr::StringToPtr() -- minor polishing
 *
 * Revision 1.83  2003/02/27 15:34:01  lavr
 * Bugfix in converting string to double [spurious dots], some reformatting
 *
 * Revision 1.82  2003/02/26 21:07:52  siyan
 * Remove const for base parameter for StringToUInt8
 *
 * Revision 1.81  2003/02/26 20:34:11  siyan
 * Added/deleted whitespaces to conform to existing coding style
 *
 * Revision 1.80  2003/02/26 16:45:53  siyan
 * Reimplemented NStr::StringToUInt8 to support additional base parameters
 * that can take radix values such as 10(default), 16, 8, 2.
 * Reimplemented StringToPtr to support 64 bit addresses.
 *
 * Revision 1.79  2003/02/25 19:14:53  kuznets
 * NStr::StringToBool changed to understand YES/NO
 *
 * Revision 1.78  2003/02/25 15:43:40  dicuccio
 * Added #ifdef'd hack for MSVC's non-standard sprintf() in PtrToString(),
 * '%p' lacks a leading '0x'
 *
 * Revision 1.77  2003/02/25 14:43:53  dicuccio
 * Added handling of special NULL pointer encoding in StringToPtr()
 *
 * Revision 1.76  2003/02/24 20:25:59  gouriano
 * use template-based errno and parse exceptions
 *
 * Revision 1.75  2003/02/21 21:20:01  vakatov
 * Fixed some types, did some casts to avoid compilation warnings in 64-bit
 *
 * Revision 1.74  2003/02/20 18:41:28  dicuccio
 * Added NStr::StringToPtr()
 *
 * Revision 1.73  2003/02/11 22:11:03  ucko
 * Make NStr::WrapList a no-op if the input list is empty.
 *
 * Revision 1.72  2003/02/06 21:31:35  ucko
 * Fixed an off-by-one error in NStr::Wrap.
 *
 * Revision 1.71  2003/02/04 21:54:12  ucko
 * NStr::Wrap: when breaking on punctuation, try to position the break
 * *after* everything but opening delimiters.
 *
 * Revision 1.70  2003/01/31 03:39:11  lavr
 * Heed int->bool performance warning
 *
 * Revision 1.69  2003/01/27 20:06:59  ivanov
 * Get rid of compilation warnings in StringToUInt8() and DoubleToString()
 *
 * Revision 1.68  2003/01/24 16:59:27  ucko
 * Add an optional parameter to Split and Tokenize indicating whether to
 * merge adjacent delimiters; clean up WrapList slightly.
 *
 * Revision 1.67  2003/01/21 23:22:22  vakatov
 * NStr::Tokenize() to return reference, and not a new "vector<>".
 *
 * Revision 1.66  2003/01/21 20:08:01  ivanov
 * Added function NStr::DoubleToString(value, precision, buf, buf_size)
 *
 * Revision 1.65  2003/01/14 22:13:56  kuznets
 * Overflow check reimplemented for NStr::StringToInt
 *
 * Revision 1.64  2003/01/14 21:16:46  kuznets
 * +Nstr::Tokenize
 *
 * Revision 1.63  2003/01/13 14:47:16  kuznets
 * Implemented overflow checking for StringToInt8 function
 *
 * Revision 1.62  2003/01/10 22:17:06  kuznets
 * Implemented NStr::String2Int8
 *
 * Revision 1.61  2003/01/10 16:49:54  kuznets
 * Cosmetics
 *
 * Revision 1.60  2003/01/10 15:27:12  kuznets
 * Eliminated int -> bool performance warning
 *
 * Revision 1.59  2003/01/10 00:08:17  vakatov
 * + Int8ToString(),  UInt8ToString()
 *
 * Revision 1.58  2003/01/06 16:42:45  ivanov
 * + DoubleToString() with 'precision'
 *
 * Revision 1.57  2002/10/18 20:48:56  lavr
 * +ENewLine_Mode and '\n' translation in NStr::PrintableString()
 *
 * Revision 1.56  2002/10/17 14:41:20  ucko
 * * Make s_EndOf{Tag,Reference} actually static (oops).
 * * Pull width-determination code from WrapList into a separate function
 *   (s_VisibleWidth) and make Wrap{,List} call it for everything rather
 *   than assuming prefixes and delimiters to be plain text.
 * * Add a column variable to WrapList, as it may not equal s.size().
 *
 * Revision 1.55  2002/10/16 19:30:36  ucko
 * Add support for wrapping HTML <PRE> blocks.  (Not yet tested, but
 * behavior without fWrap_HTMLPre should stay the same.)
 *
 * Revision 1.54  2002/10/11 19:41:48  ucko
 * Clean up NStr::Wrap a bit more, doing away with the "limit" variables
 * for ease of potential extension.
 *
 * Revision 1.53  2002/10/03 14:44:35  ucko
 * Tweak the interfaces to NStr::Wrap* to avoid publicly depending on
 * kEmptyStr, removing the need for fWrap_UsePrefix1 in the process; also
 * drop fWrap_FavorPunct, as WrapList should be a better choice for such
 * situations.
 *
 * Revision 1.52  2002/10/02 20:15:09  ucko
 * Add Join, Wrap, and WrapList functions to NStr::.
 *
 * Revision 1.51  2002/09/04 15:16:57  lavr
 * Backslashed double quote (\") in PrintableString()
 *
 * Revision 1.50  2002/07/15 18:17:25  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.49  2002/07/11 14:18:28  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.48  2002/05/02 15:25:37  ivanov
 * Added new parameter to String-to-X functions for skipping the check
 * the end of string on zero
 *
 * Revision 1.47  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.46  2002/02/22 17:50:52  ivanov
 * Added compatible compare functions strcmp, strncmp, strcasecmp, strncasecmp.
 * Was speed-up some Compare..() functions.
 *
 * Revision 1.45  2001/08/30 00:36:45  vakatov
 * + NStr::StringToNumeric()
 * Also, well-groomed the code and get rid of some compilation warnings.
 *
 * Revision 1.44  2001/05/30 15:56:25  vakatov
 * NStr::CompareNocase, NStr::CompareCase -- get rid of the possible
 * compilation warning (ICC compiler:  "return statement missing").
 *
 * Revision 1.43  2001/05/17 15:04:59  lavr
 * Typos corrected
 *
 * Revision 1.42  2001/04/12 21:39:44  vakatov
 * NStr::Replace() -- check against source and dest. strings being the same
 *
 * Revision 1.41  2001/04/11 20:15:29  vakatov
 * NStr::PrintableString() -- cast "char" to "unsigned char".
 *
 * Revision 1.40  2001/03/16 19:38:35  grichenk
 * Added NStr::Split()
 *
 * Revision 1.39  2001/01/03 17:45:35  vakatov
 * + <ncbi_limits.h>
 *
 * Revision 1.38  2000/12/15 15:36:41  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * Revision 1.37  2000/12/12 14:20:36  vasilche
 * Added operator bool to CArgValue.
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
 *
 * Revision 1.36  2000/12/11 20:42:50  vakatov
 * + NStr::PrintableString()
 *
 * Revision 1.35  2000/11/16 23:52:41  vakatov
 * Porting to Mac...
 *
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
 * NStr::StringToInt now reports errors for tailing symbols in release version
 * too
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
 * Added _TRACE_THROW() macro, which can be configured to produce coredump
 * at a point of throwing an exception.
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
 * CNCBINode fixed in Resource; case insensitive string comparison predicate
 * added
 *
 * Revision 1.2  1998/12/15 17:36:33  vasilche
 * Fixed "double colon" bug in multithreaded version of headers.
 *
 * Revision 1.1  1998/12/15 15:43:22  vasilche
 * Added utilities to convert string <> int.
 * ===========================================================================
 */
