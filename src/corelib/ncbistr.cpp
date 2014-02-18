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
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistr.hpp>
#include <corelib/tempstr.hpp>
#include <corelib/ncbistr_util.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbierror.hpp>
#include <corelib/ncbifloat.h>
#include <memory>
#include <functional>
#include <algorithm>
#include <iterator>
#include <stdio.h>
#include <locale.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Corelib_Util


BEGIN_NCBI_NAMESPACE;


// Digits (up to base 36)
static const char kDigit[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


static inline
SIZE_TYPE s_DiffPtr(const char* end, const char* start)
{
    return end ? (SIZE_TYPE)(end - start) : (SIZE_TYPE) 0;
}

const char *const kEmptyCStr = "";

#if defined(HAVE_WSTRING)
const wchar_t *const kEmptyWCStr = L"";
#endif


extern const char* const kNcbiDevelopmentVersionString;
const char* const kNcbiDevelopmentVersionString
    = "NCBI_DEVELOPMENT_VER_" NCBI_AS_STRING(NCBI_DEVELOPMENT_VER);

#ifdef NCBI_PRODUCTION_VER
extern const char* const kNcbiProductionVersionString;
const char* const kNcbiProductionVersionString
    = "NCBI_PRODUCTION_VER_" NCBI_AS_STRING(NCBI_PRODUCTION_VER);
#endif


#if !defined(NCBI_OS_MSWIN) && !( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
const string* CNcbiEmptyString::m_Str = 0;
const string& CNcbiEmptyString::FirstGet(void) {
    static const string s_Str = "";
    m_Str = &s_Str;
    return s_Str;
}
#  ifdef HAVE_WSTRING
const wstring* CNcbiEmptyWString::m_Str = 0;
const wstring& CNcbiEmptyWString::FirstGet(void) {
    static const wstring s_Str = L"";
    m_Str = &s_Str;
    return s_Str;
}
#  endif
#endif


bool NStr::IsBlank(const CTempString& str, SIZE_TYPE pos)
{
    SIZE_TYPE len = str.length();
    for (SIZE_TYPE idx = pos; idx < len; ++idx) {
        if (!isspace((unsigned char) str[idx])) {
            return false;
        }
    }
    return true;
}


int NStr::CompareCase(const CTempString& str, SIZE_TYPE pos, SIZE_TYPE n,
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



int NStr::CompareCase(const CTempString& str, SIZE_TYPE pos, SIZE_TYPE n,
                      const CTempString& pattern)
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


int NStr::CompareNocase(const CTempString& str, SIZE_TYPE pos, SIZE_TYPE n,
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


int NStr::CompareNocase(const CTempString& str, SIZE_TYPE pos, SIZE_TYPE n,
                        const CTempString& pattern)
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
bool NStr::MatchesMask(CTempString str, CTempString mask, ECase use_case)
{
    char c;
    for ( size_t str_pos = 0, mask_pos = 0; ; ) {
        // Analyze symbol in mask
        switch ( c = mask[mask_pos++] ) {
        case '\0':
            return str[str_pos] == '\0';

        case '?':
            if (str[str_pos] == '\0') {
                return false;
            }
            ++str_pos;
            break;

        case '*':
            c = mask[mask_pos];
            // Collapse multiple stars
            while ( c == '*' ) {
                c = mask[++mask_pos];
            }
            if (c == '\0') {
                return true;
            }
            // General case, use recursion
            while ( str[str_pos] ) {
                if ( MatchesMask(str.substr(str_pos),
                                 mask.substr(mask_pos),
                                 use_case) ) {
                    return true;
                }
                ++str_pos;
            }
            return false;

        default:
            // Compare nonpattern character in mask and name
            char s = str[str_pos++];
            if (use_case == NStr::eNocase) {
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


int NStr::StringToNonNegativeInt(const string& str)
{
    int& errno_ref = errno;
    if ( str.empty() ) {
        CNcbiError::SetErrno(errno_ref = EINVAL, str);
        return -1;
    }
    char ch = str[0];
    if ( !isdigit((unsigned char)ch) &&  (ch != '+') ) {
        CNcbiError::SetErrno(errno_ref = EINVAL, str);
        return -1;
    }
    char* endptr = 0;
    const char* begptr = str.c_str();
    errno_ref = 0;
    unsigned long value = strtoul(begptr, &endptr, 10);
    if ( errno_ref ) {
        CNcbiError::SetErrno(errno_ref, str);
        return -1;
    }
    else if ( !endptr  ||  endptr == begptr  ||  *endptr ) {
        CNcbiError::SetErrno(errno_ref = EINVAL, str);
        return -1;
    }
    else if ( value > (unsigned long) kMax_Int ) {
        CNcbiError::SetErrno(errno_ref = ERANGE, str);
        return -1;
    }
    errno_ref = 0;
    return (int) value;
}


/// @internal
// Access to errno is slow on some platforms, because it use TLS to store a value
// for each thread. This guard class can set an errno value in string to numeric
// conversion functions only once before exit, and when necessary.
class CS2N_Guard
{
public:
    CS2N_Guard(NStr::TStringToNumFlags flags, bool skip_if_zero) :
        m_NoThrow((flags & NStr::fConvErr_NoThrow) > 0), 
        m_SkipIfZero(skip_if_zero), 
        m_Errno(0)
    {}
    ~CS2N_Guard(void)  {
        // Does the guard is used against the code that already set an errno?
        // If the error code is not defined here, do not even try to check/set it.
        if (!m_SkipIfZero  ||  m_Errno) {
            errno = m_Errno;
        }
    }
    void Set(int errcode)    { m_Errno = errcode; }
    int  Errno(void) const   { return m_Errno;}
    // Says that we want to throw an exception, do not set errno in this case
    void Throw(void)         { m_SkipIfZero = true; m_Errno = 0; }
    bool ToThrow(void) const { return !m_NoThrow; }
    // Auxiliary function to create a message about conversion error 
    // to specified type. It doesn't have any relation to the guard itself,
    // but can help to save on the amount of code in calling macro.
    string Message(const CTempString& str, const char* to_type, const CTempString& msg);

private:
    bool m_NoThrow;    // do not throw an exception if TRUE
    bool m_SkipIfZero; // do not set errno if TRUE and m_Errno == 0
    int  m_Errno;      // errno value to set
};

string CS2N_Guard::Message(const CTempString& str, const char* to_type, const CTempString& msg)
{
    string s;
    s.reserve(str.length() + msg.length() + 50);
    s += "Cannot convert string '";
    s += str;
    s += "' to ";
    s += to_type;
    if ( !msg.empty() ) {
        s += ", ";
        s += msg;
    }
    return s;
}

/// Regular guard
#define S2N_CONVERT_GUARD(flags)   \
    CS2N_Guard err_guard(flags, false)

// This guard can be used against the code that already set an errno.
// If the error code is not defined, the guard not even try to check/set it (even to zero).
#define S2N_CONVERT_GUARD_EX(flags)   \
    CS2N_Guard err_guard(flags, true)

#define S2N_CONVERT_ERROR(to_type, msg, errcode, pos)                 \
    do {                                                              \
        err_guard.Set(errcode);                                       \
        if ( err_guard.ToThrow() ) {                                  \
            err_guard.Throw();                                        \
            NCBI_THROW2(CStringException, eConvert,                   \
                        err_guard.Message(str, #to_type, msg), pos);  \
        } else {                                                      \
            CNcbiError::SetErrno(err_guard.Errno(),                   \
                err_guard.Message(str, #to_type, msg));               \
            return 0;                                                 \
        }                                                             \
    } while (false)


#define S2N_CONVERT_ERROR_INVAL(to_type)                              \
    S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, pos)

#define S2N_CONVERT_ERROR_RADIX(to_type, msg)                         \
    S2N_CONVERT_ERROR(to_type, msg, EINVAL, pos)

#define S2N_CONVERT_ERROR_OVERFLOW(to_type)                           \
    S2N_CONVERT_ERROR(to_type, "overflow",ERANGE, pos)

#define CHECK_ENDPTR(to_type)                                         \
    if ( str[pos] ) {                                                 \
        S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, pos);           \
    }

#define CHECK_ENDPTR_SIZE(to_type)                                    \
    if ( pos < size ) {                                               \
        S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, pos);           \
    }

#define CHECK_COMMAS                                                  \
    /* Check on possible commas */                                    \
    if (flags & NStr::fAllowCommas) {                                 \
        if (ch == ',') {                                              \
            if ((numpos == pos)  ||                                   \
                ((comma >= 0)  &&  (comma != 3)) ) {                  \
                /* Not first comma, sitting on incorrect place */     \
                break;                                                \
            }                                                         \
            /* Skip it */                                             \
            comma = 0;                                                \
            pos++;                                                    \
            continue;                                                 \
        } else {                                                      \
            if (comma >= 0) {                                         \
                /* Count symbols between commas */                    \
                comma++;                                              \
            }                                                         \
        }                                                             \
    }


int NStr::StringToInt(const CTempString& str, TStringToNumFlags flags,int base)
{
    S2N_CONVERT_GUARD_EX(flags);
    Int8 value = StringToInt8(str, flags, base);
    if ( value < kMin_Int  ||  value > kMax_Int) {
        S2N_CONVERT_ERROR(int, "overflow", ERANGE, 0);
    }
    return (int) value;
}


unsigned int
NStr::StringToUInt(const CTempString& str, TStringToNumFlags flags, int base)
{
    S2N_CONVERT_GUARD_EX(flags);
    Uint8 value = StringToUInt8(str, flags, base);
    if ( value > kMax_UInt ) {
        S2N_CONVERT_ERROR(unsigned int, "overflow", ERANGE, 0);
    }
    return (unsigned int) value;
}


long NStr::StringToLong(const CTempString& str, TStringToNumFlags flags,
                        int base)
{
    S2N_CONVERT_GUARD_EX(flags);
    Int8 value = StringToInt8(str, flags, base);
    if ( value < kMin_Long  ||  value > kMax_Long) {
        S2N_CONVERT_ERROR(long, "overflow", ERANGE, 0);
    }
    return (long) value;
}


unsigned long
NStr::StringToULong(const CTempString& str, TStringToNumFlags flags, int base)
{
    S2N_CONVERT_GUARD_EX(flags);
    Uint8 value = StringToUInt8(str, flags, base);
    if ( value > kMax_ULong ) {
        S2N_CONVERT_ERROR(unsigned long, "overflow", ERANGE, 0);
    }
    return (unsigned long) value;
}


/// @internal
// Check that symbol 'ch' is good symbol for number with radix 'base'.
static inline
bool s_IsGoodCharForRadix(char ch, int base, int* value = 0)
{
    if ( base <= 10 ) {
        // shortcut for most frequent case
        int delta = ch-'0';
        if ( unsigned(delta) < unsigned(base) ) {
            if ( value ) {
                *value = delta;
            }
            return true;
        }
        return false;
    }
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

static inline
bool s_IsDecimalPoint(unsigned char ch, NStr::TStringToNumFlags  flags)
{
    if ( ch != '.' && ch != ',') {
        return false;
    }
    if (flags & NStr::fDecimalPosix) {
        return ch == '.';
    }
    else if (flags & NStr::fDecimalPosixOrLocal) {
        return ch == '.' || ch == ',';
    }
    struct lconv* conv = localeconv();
    return ch == *(conv->decimal_point);
}

static inline
void s_SkipAllowedSymbols(const CTempString& str,
                          SIZE_TYPE&         pos,
                          ESkipMode          skip_mode,
                          NStr::TStringToNumFlags  flags)
{
    if (skip_mode == eSkipAll) {
        pos = str.length();
        return;
    }

    for ( SIZE_TYPE len = str.length(); pos < len; ++pos ) {
        unsigned char ch = str[pos];
        if ( isdigit(ch)  ||  ch == '+' ||  ch == '-'  ||  s_IsDecimalPoint(ch,flags) ) {
            break;
        }
        if ( (skip_mode == eSkipSpacesOnly)  &&  !isspace(ch) ) {
            break;
        }
    }
}


// Check radix base. If it is zero, determine base using first chars
// of the string. Update 'base' value.
// Update 'ptr' to current position in the string.

static inline
bool s_CheckRadix(const CTempString& str, SIZE_TYPE& pos, int& base)
{
    if ( base == 10 || base == 8 ) {
        // shortcut for most frequent case
        return true;
    }
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
    S2N_CONVERT_GUARD(flags);

    // Current position in the string
    SIZE_TYPE pos = 0;

    // Skip allowed leading symbols
    if (flags & fAllowLeadingSymbols) {
        bool spaces = ((flags & fAllowLeadingSymbols) == fAllowLeadingSpaces);
        s_SkipAllowedSymbols(str, pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed, flags);
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
    SIZE_TYPE pos0 = pos;
    // Check radix base
    if ( !s_CheckRadix(str, pos, base) ) {
        S2N_CONVERT_ERROR_RADIX(Int8, "bad numeric base '" + 
                                NStr::IntToString(base)+ "'");
    }

    // Begin conversion
    Int8 n = 0;
    Int8 limdiv = base==10? kMax_I8 / 10: kMax_I8 / base;
    Int8 limoff = (base==10? kMax_I8 % 10: kMax_I8 % base) + (sign ? 1 : 0);

    // Number of symbols between two commas. '-1' means -- no comma yet.
    int       comma  = -1;  
    SIZE_TYPE numpos = pos;

    while (char ch = str[pos]) {
        int  delta;   // corresponding numeric value of 'ch'

        // Check on possible commas
        CHECK_COMMAS;
        // Sanity check
        if ( !s_IsGoodCharForRadix(ch, base, &delta) ) {
            break;
        }
        // Overflow check
        if ( n >= limdiv  &&  (n > limdiv  ||  delta > limoff) ) {
            S2N_CONVERT_ERROR_OVERFLOW(Int8);
        }
        n *= base;
        n += delta;
        pos++;
    }

    // Last checks
    if ( pos == pos0  || ((comma >= 0)  &&  (comma != 3)) ) {
        S2N_CONVERT_ERROR_INVAL(Int8);
    }
    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll, flags);
    }
    // Assign sign before the end pointer check
    n = sign ? -n : n;
    CHECK_ENDPTR(Int8);
    return n;
}


Uint8 NStr::StringToUInt8(const CTempString& str,
                          TStringToNumFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    S2N_CONVERT_GUARD(flags);

    const TStringToNumFlags slow_flags =
        fMandatorySign|fAllowCommas|fAllowLeadingSymbols|fAllowTrailingSymbols;

    if ( base == 10 && (flags & slow_flags) == 0 ) {
        // fast conversion

        // Current position in the string
        CTempString::const_iterator ptr = str.begin(), end = str.end();

        // Determine sign
        if ( ptr != end && *ptr == '+' ) {
            ++ptr;
        }
        if ( ptr == end ) {
            S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, ptr-str.begin());
        }

        // Begin conversion
        Uint8 n = 0;

        const Uint8 limdiv = kMax_UI8/10;
        const int   limoff = int(kMax_UI8 % 10);

        do {
            char ch = *ptr;
            int  delta = ch - '0';
            if ( unsigned(delta) >= 10 ) {
                S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, ptr-str.begin());
            }
            // Overflow check
            if ( n >= limdiv && (n > limdiv || delta > limoff) ) {
                S2N_CONVERT_ERROR(Uint8, kEmptyStr, ERANGE, ptr-str.begin());
            }
            n = n*10+delta;
        } while ( ++ptr != end );

        return n;
    }

    // Current position in the string
    SIZE_TYPE pos = 0, size = str.size();

    // Skip allowed leading symbols
    if (flags & fAllowLeadingSymbols) {
        bool spaces = ((flags & fAllowLeadingSymbols) == fAllowLeadingSpaces);
        s_SkipAllowedSymbols(str, pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed, flags);
    }
    // Determine sign
    if (str[pos] == '+') {
        pos++;
    } else {
        if (flags & fMandatorySign) {
            S2N_CONVERT_ERROR_INVAL(Uint8);
        }
    }
    SIZE_TYPE pos0 = pos;

    // Begin conversion
    Uint8 n = 0;
    // Check radix base
    if ( !s_CheckRadix(str, pos, base) ) {
        S2N_CONVERT_ERROR_RADIX(Uint8, "bad numeric base '" +
                                NStr::IntToString(base) + "'");
    }

    Uint8 limdiv = kMax_UI8 / base;
    int   limoff = int(kMax_UI8 % base);

    // Number of symbols between two commas. '-1' means -- no comma yet.
    int       comma  = -1;  
    SIZE_TYPE numpos = pos;

    while (char ch = str[pos]) {
        int delta;  // corresponding numeric value of 'ch'

        // Check on possible commas
        CHECK_COMMAS;
        // Sanity check
        if ( !s_IsGoodCharForRadix(ch, base, &delta) ) {
            break;
        }
        // Overflow check
        if ( n >= limdiv  &&  (n > limdiv  ||  delta > limoff) ) {
            S2N_CONVERT_ERROR_OVERFLOW(Uint8);
        }
        n *= base;
        n += delta;
        pos++;
    }

    // Last checks
    if ( pos == pos0  || ((comma >= 0)  &&  (comma != 3)) ) {
        S2N_CONVERT_ERROR_INVAL(Uint8);
    }
    // Skip allowed trailing symbols
    if (flags & fAllowTrailingSymbols) {
        bool spaces = ((flags & fAllowTrailingSymbols) ==
                       fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll, flags);
    }
    CHECK_ENDPTR_SIZE(Uint8);
    return n;
}


double NStr::StringToDoublePosix(const char* ptr, char** endptr)
{
    S2N_CONVERT_GUARD(NStr::fConvErr_NoThrow);

    const char* start = ptr;
    char c = *ptr++;

    // skip leading blanks
    while ( isspace((unsigned char)c) ) {
        c = *ptr++;
    }
    
    // short-cut - single digit
    if ( !*ptr && c >= '0' && c <= '9' ) {
        if (endptr) {
            *endptr = (char*)ptr;
        }
        return c-'0';
    }

    int sign = 0;
    if ( c == '-' ) {
        sign = -1;
        c = *ptr++;
    }
    else if ( c == '+' ) {
        sign = +1;
        c = *ptr++;
    }

    bool         dot = false, expn = false, anydigits = false;
    int          digits = 0, dot_position = 0;
    unsigned int first=0, second=0, first_mul=1;
    long double  second_mul = NCBI_CONST_LONGDOUBLE(1.),
                 third      = NCBI_CONST_LONGDOUBLE(0.);

    // up to exponent
    for ( ; ; c = *ptr++ ) {
        if (c >= '0' && c <= '9') {
            // digits: accumulate
            c -= '0';
            anydigits = true;
            ++digits;
            if (first == 0) {
                first = c;
                if ( first == 0 ) {
                    // omit leading zeros
                    --digits;
                    if (dot) {
                        --dot_position;
                    }
                }
            } else if (digits <= 9) {
                // first 9 digits come to 'first'
                first = first*10 + c;
            } else if (digits <= 18) {
                // next 9 digits come to 'second'
                first_mul *= 10;
                second = second*10 + c;
            } else {
                // other digits come to 'third'
                second_mul *= NCBI_CONST_LONGDOUBLE(10.);
                third = third * NCBI_CONST_LONGDOUBLE(10.) + c;
            }
        }
        else if (c == '.') {
            // dot
            // if second dot, stop
            if (dot) {
                --ptr;
                break;
            }
            dot_position = digits;
            dot = true;
        }
        else if (c == 'e' || c == 'E') {
            // if exponent, stop
            if (!anydigits) {
                --ptr;
                break;
            }
            expn = true;
            break;
        }
        else {
            --ptr;
            if (!anydigits) {
                if ( !dot && (c == 'n' || c == 'N') &&
                     NStr::strncasecmp(ptr,"nan",3)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+3);
                    }
                    return HUGE_VAL/HUGE_VAL; /* NCBI_FAKE_WARNING */
                }
                if ( (c == 'i' || c == 'I') ) {
                    if ( NStr::strncasecmp(ptr,"inf",3)==0) {
                        ptr += 3;
                        if ( NStr::strncasecmp(ptr,"inity",5)==0) {
                            ptr += 5;
                        }
                        if (endptr) {
                            *endptr = (char*)ptr;
                        }
                        return sign < 0 ? -HUGE_VAL : HUGE_VAL;
                    }
                }
            }
            break;
        }
    }
    // if no digits, stop now - error
    if (!anydigits) {
        if (endptr) {
            *endptr = (char*)start;
        }
        err_guard.Set(EINVAL);
        return 0.;
    }
    int exponent = dot ? dot_position - digits : 0;

    // read exponent
    if (expn && *ptr) {
        int expvalue = 0;
        bool expsign = false, expnegate= false;
        int expdigits= 0;
        for( ; ; ++ptr) {
            c = *ptr;
            // sign: should be no digits at this point
            if (c == '-' || c == '+') {
                // if there was sign or digits, stop
                if (expsign || expdigits) {
                    break;
                }
                expsign = true;
                expnegate = c == '-';
            }
            // digits: accumulate
            else if (c >= '0' && c <= '9') {
                ++expdigits;
                int newexpvalue = expvalue*10 + (c-'0');
                if (newexpvalue > expvalue) {
                    expvalue = newexpvalue;
                }
            }
            else {
                break;
            }
        }
        // if no digits, rollback
        if (!expdigits) {
            // rollback sign
            if (expsign) {
                --ptr;
            }
            // rollback exponent
            if (expn) {
                --ptr;
            }
        }
        else {
            exponent = expnegate ? exponent - expvalue : exponent + expvalue;
        }
    }
    long double ret;
    if ( first_mul > 1 ) {
        _ASSERT(first);
        ret = ((long double)first * first_mul + second)* second_mul + third;
    }
    else {
        _ASSERT(first_mul == 1);
        _ASSERT(second == 0);
        _ASSERT(second_mul == 1);
        _ASSERT(third == 0);
        ret = first;
    }
    // calculate exponent
    if ( first && exponent ) {
        // multiply by power of 10 only non-zero mantissa
        if (exponent > 2*DBL_MAX_10_EXP) {
            ret = HUGE_VAL;
            err_guard.Set(ERANGE);
        } else if (exponent < 2*DBL_MIN_10_EXP) {
            ret = 0.;
            err_guard.Set(ERANGE);
        } else {
            if ( exponent > 0 ) {
                static const double mul1[16] = {
                    1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7,
                    1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15
                };
                ret *= mul1[exponent&15];
                if ( exponent >>= 4 ) {
                    static const long double mul2[16] = {
                        NCBI_CONST_LONGDOUBLE(1e0),
                        NCBI_CONST_LONGDOUBLE(1e16),
                        NCBI_CONST_LONGDOUBLE(1e32),
                        NCBI_CONST_LONGDOUBLE(1e48),
                        NCBI_CONST_LONGDOUBLE(1e64),
                        NCBI_CONST_LONGDOUBLE(1e80),
                        NCBI_CONST_LONGDOUBLE(1e96),
                        NCBI_CONST_LONGDOUBLE(1e112),
                        NCBI_CONST_LONGDOUBLE(1e128),
                        NCBI_CONST_LONGDOUBLE(1e144),
                        NCBI_CONST_LONGDOUBLE(1e160),
                        NCBI_CONST_LONGDOUBLE(1e176),
                        NCBI_CONST_LONGDOUBLE(1e192),
                        NCBI_CONST_LONGDOUBLE(1e208),
                        NCBI_CONST_LONGDOUBLE(1e224),
                        NCBI_CONST_LONGDOUBLE(1e240)
                    };
                    ret *= mul2[exponent&15];
                    for ( exponent >>= 4; exponent; --exponent ) {
                        ret *= NCBI_CONST_LONGDOUBLE(1e256);
                    }
                }
                if (!finite(double(ret))) {
                    err_guard.Set(ERANGE);
                }
            }
            else {
                exponent = -exponent;
                static const long double mul1[16] = {
                    NCBI_CONST_LONGDOUBLE(1e-0),
                    NCBI_CONST_LONGDOUBLE(1e-1),
                    NCBI_CONST_LONGDOUBLE(1e-2),
                    NCBI_CONST_LONGDOUBLE(1e-3),
                    NCBI_CONST_LONGDOUBLE(1e-4),
                    NCBI_CONST_LONGDOUBLE(1e-5),
                    NCBI_CONST_LONGDOUBLE(1e-6),
                    NCBI_CONST_LONGDOUBLE(1e-7),
                    NCBI_CONST_LONGDOUBLE(1e-8),
                    NCBI_CONST_LONGDOUBLE(1e-9),
                    NCBI_CONST_LONGDOUBLE(1e-10),
                    NCBI_CONST_LONGDOUBLE(1e-11),
                    NCBI_CONST_LONGDOUBLE(1e-12),
                    NCBI_CONST_LONGDOUBLE(1e-13),
                    NCBI_CONST_LONGDOUBLE(1e-14),
                    NCBI_CONST_LONGDOUBLE(1e-15)
                };
                ret *= mul1[exponent&15];
                if ( exponent >>= 4 ) {
                    static const long double mul2[16] = {
                        NCBI_CONST_LONGDOUBLE(1e-0),
                        NCBI_CONST_LONGDOUBLE(1e-16),
                        NCBI_CONST_LONGDOUBLE(1e-32),
                        NCBI_CONST_LONGDOUBLE(1e-48),
                        NCBI_CONST_LONGDOUBLE(1e-64),
                        NCBI_CONST_LONGDOUBLE(1e-80),
                        NCBI_CONST_LONGDOUBLE(1e-96),
                        NCBI_CONST_LONGDOUBLE(1e-112),
                        NCBI_CONST_LONGDOUBLE(1e-128),
                        NCBI_CONST_LONGDOUBLE(1e-144),
                        NCBI_CONST_LONGDOUBLE(1e-160),
                        NCBI_CONST_LONGDOUBLE(1e-176),
                        NCBI_CONST_LONGDOUBLE(1e-192),
                        NCBI_CONST_LONGDOUBLE(1e-208),
                        NCBI_CONST_LONGDOUBLE(1e-224),
                        NCBI_CONST_LONGDOUBLE(1e-240)
                    };
                    ret *= mul2[exponent&15];
                    for ( exponent >>= 4; exponent; --exponent ) {
                        ret *= NCBI_CONST_LONGDOUBLE(1e-256);
                    }
                }
                if ( ret < DBL_MIN ) {
                    err_guard.Set(ERANGE);
                }
            }
        }
    }
    if ( sign < 0 ) {
        ret = -ret;
    }
    // done
    if (endptr) {
        *endptr = (char*)ptr;
    }
    return ret;
}


/// @internal
static double s_StringToDouble(const char* str, size_t size,
                               NStr::TStringToNumFlags flags)
{
    _ASSERT(flags == 0  ||  flags > 32);
    _ASSERT(str[size] == '\0');
    if ((flags & NStr::fDecimalPosix) && (flags & NStr::fDecimalPosixOrLocal)) {
        NCBI_THROW2(CStringException, eBadArgs,
                    "NStr::StringToDouble():  mutually exclusive flags specified",0);
    }
    S2N_CONVERT_GUARD_EX(flags);

    // Current position in the string
    SIZE_TYPE pos  = 0;

    // Skip allowed leading symbols
    if (flags & NStr::fAllowLeadingSymbols) {
        bool spaces = ((flags & NStr::fAllowLeadingSymbols) == 
                       NStr::fAllowLeadingSpaces);
        s_SkipAllowedSymbols(CTempString(str, size), pos,
                             spaces ? eSkipSpacesOnly : eSkipAllAllowed, flags);
    }
    // Check mandatory sign
    if (flags & NStr::fMandatorySign) {
        switch (str[pos]) {
        case '-':
        case '+':
            break;
        default:
            S2N_CONVERT_ERROR_INVAL(double);
        }
    }
    // For consistency make additional check on incorrect leading symbols.
    // Because strtod() may just skip such symbols.
    if (!(flags & NStr::fAllowLeadingSymbols)) {
        char c = str[pos];
        if ( !isdigit((unsigned int)c)  &&  !s_IsDecimalPoint(c,flags)  &&  c != '-'  &&  c != '+') {
            S2N_CONVERT_ERROR_INVAL(double);
        }
    }

    // Conversion
    int& errno_ref = errno;
    errno_ref = 0;

    char* endptr = 0;
    const char* begptr = str + pos;

    double n;
    if (flags & NStr::fDecimalPosix) {
        n = NStr::StringToDoublePosix(begptr, &endptr);
    } else {
        n = strtod(begptr, &endptr);
    }
    if (flags & NStr::fDecimalPosixOrLocal) {
        char* endptr2 = 0;
        double n2 = NStr::StringToDoublePosix(begptr, &endptr2);
        if (!endptr || (endptr2 && endptr2 > endptr)) {
            n = n2;
            endptr = endptr2;
        }
    }
    if ( !endptr  ||  endptr == begptr ) {
        S2N_CONVERT_ERROR(double, kEmptyStr, EINVAL, s_DiffPtr(endptr, begptr) + pos);
    }
    if ( errno_ref ) {
        S2N_CONVERT_ERROR(double, kEmptyStr, errno_ref, s_DiffPtr(endptr, begptr) + pos);
    }
    pos += s_DiffPtr(endptr, begptr);

    // Skip allowed trailing symbols
    if (flags & NStr::fAllowTrailingSymbols) {
        bool spaces = ((flags & NStr::fAllowTrailingSymbols) ==
                       NStr::fAllowTrailingSpaces);
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll, flags);
    }
    CHECK_ENDPTR(double);
    return n;
}


double NStr::StringToDoubleEx(const char* str, size_t size,
                              TStringToNumFlags flags)
{
    return s_StringToDouble(str, size, flags);
}


double NStr::StringToDouble(const CTempStringEx& str, TStringToNumFlags flags)
{
    size_t size = str.size();
    if ( str.HasZeroAtEnd() ) {
        // string has zero at the end already
        return s_StringToDouble(str.data(), size, flags);
    }
    char buf[256]; // small temporary buffer on stack for appending zero char
    if ( size < sizeof(buf) ) {
        memcpy(buf, str.data(), size);
        buf[size] = '\0';
        return s_StringToDouble(buf, size, flags);
    }
    else {
        // use std::string() to allocate memory for appending zero char
        return s_StringToDouble(string(str).c_str(), size, flags);
    }
}

/// @internal
static Uint8 s_DataSizeConvertQual(const CTempString&      str,
                                   SIZE_TYPE&              pos, 
                                   Uint8                   value,
                                   NStr::TStringToNumFlags flags)
{
    S2N_CONVERT_GUARD(flags);

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
    if ( base < 2  ||  base > 16 ) {
        NCBI_THROW2(CStringException, eConvert,  
                    "Bad numeric base '" + NStr::IntToString(base)+ "'", 0);
    }
    S2N_CONVERT_GUARD_EX(flags);

    // Current position in the string
    SIZE_TYPE pos = 0;

    // Find end of number representation
    {{
        // Skip allowed leading symbols
        if (flags & fAllowLeadingSymbols) {
            bool spaces = ((flags & fAllowLeadingSymbols) ==
                           fAllowLeadingSpaces);
            s_SkipAllowedSymbols(str, pos,
                           spaces ? eSkipSpacesOnly : eSkipAllAllowed, flags);
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

    SIZE_TYPE numpos = pos;
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
    Uint8 n = StringToUInt8(CTempString(str.data()+numpos, pos-numpos),
                            flags, base);
    if ( !n && errno ) {
        // If exceptions are enabled that it has been already thrown.
        // The errno is also set, so just return a zero.
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
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll, flags);
    }
    CHECK_ENDPTR(Uint8);
    return n;
}


Uint8 NStr::StringToUInt8_DataSize(const CTempString& str,
                                   TStringToNumFlags flags /* = 0 */)
{
    TStringToNumFlags allowed_flags = fConvErr_NoThrow +
                                      fMandatorySign +
                                      fAllowCommas +
                                      fAllowLeadingSymbols +
                                      fAllowTrailingSymbols +
                                      fDS_ForceBinary +
                                      fDS_ProhibitFractions +
                                      fDS_ProhibitSpaceBeforeSuffix;

    if ((flags & allowed_flags) != flags) {
        NCBI_THROW2(CStringException, eConvert, "Wrong set of flags", 0);
    }
    S2N_CONVERT_GUARD(flags);

    const char* str_ptr = str.data();
    const char* str_end = str_ptr + str.size();
    if (flags & fAllowLeadingSymbols) {
        bool allow_all = (flags & fAllowLeadingSymbols) != fAllowLeadingSpaces;
        for (; str_ptr < str_end; ++str_ptr) {
            char c = *str_ptr;
            if (isdigit(c))
                break;
            if (isspace(c))
                continue;
            if ((c == '+'  ||  c == '-')  &&  (flags & fMandatorySign)
                &&  str_ptr + 1 < str_end  &&  isdigit(*(str_ptr + 1)))
            {
                break;
            }
            if (!allow_all)
                break;
        }
    }

    if (str_ptr < str_end  &&  *str_ptr == '+') {
        ++str_ptr;
    }
    else if ((str_ptr < str_end  &&  *str_ptr == '-')
             ||  (flags & fMandatorySign))
    {
        S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, str_ptr - str.data());
    }

    const char* num_start = str_ptr;
    bool have_dot = false;
    bool allow_commas = (flags & fAllowCommas) != 0;
    bool allow_dot = (flags & fDS_ProhibitFractions) == 0;
    Uint4 digs_pre_dot = 0, digs_post_dot = 0;

    for (; str_ptr < str_end; ++str_ptr) {
        char c = *str_ptr;
        if (isdigit(c)) {
            if (have_dot)
                ++digs_post_dot;
            else
                ++digs_pre_dot;
        }
        else if (c == '.'  &&  allow_dot) {
            if (have_dot  ||  str_ptr == num_start)
                break;
            if (*(str_ptr - 1) == ',') {
                --str_ptr;
                break;
            }
            have_dot = true;
        }
        else if (c == ','  &&  allow_commas) {
            if (have_dot  ||  str_ptr == num_start)
                break;
            if (*(str_ptr - 1) == ',') {
                --str_ptr;
                break;
            }
        }
        else
            break;
    }
    if (have_dot  &&  digs_post_dot == 0)
        --str_ptr;
    else if (str_ptr > num_start  &&  *(str_ptr - 1) == ',')
        --str_ptr;

    const char* num_end = str_ptr;
    if (num_start == num_end) {
        S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, str_ptr - str.data());
    }
    if (str_ptr < str_end  &&  *str_ptr == ' '
        &&  !(flags & fDS_ProhibitSpaceBeforeSuffix))
    {
        ++str_ptr;
    }
    char suff_c = 0;
    if (str_ptr < str_end)
        suff_c = toupper(*str_ptr);

    static const char s_Suffixes[] = {'K', 'M', 'G', 'T', 'P', 'E'};
    static const char* const s_BinCoefs[] = {"1024", "1048576", "1073741824",
                                             "1099511627776",
                                             "1125899906842624",
                                             "1152921504606846976"};
    static const Uint4 s_NumSuffixes = sizeof(s_Suffixes) / sizeof(s_Suffixes[0]);

    bool binary_suff = (flags & fDS_ForceBinary) != 0;
    Uint4 suff_idx = 0;
    for (; suff_idx < s_NumSuffixes; ++suff_idx) {
        if (suff_c == s_Suffixes[suff_idx])
            break;
    }
    if (suff_idx < s_NumSuffixes) {
        ++str_ptr;
        if (str_ptr + 1 < str_end  &&  toupper(*str_ptr) == 'I'
            &&  toupper(*(str_ptr + 1)) == 'B')
        {
            str_ptr += 2;
            binary_suff = true;
        }
        else if (str_ptr < str_end  &&  toupper(*str_ptr) == 'B')
            ++str_ptr;
    }
    else if (suff_c == 'B') {
        ++str_ptr;
    }
    else if (*(str_ptr - 1) == ' ')
        --str_ptr;

    if (flags & fAllowTrailingSymbols) {
        bool allow_all = (flags & fAllowTrailingSymbols) != fAllowTrailingSpaces;
        for (; str_ptr < str_end; ++str_ptr) {
            char c = *str_ptr;
            if (isspace(c))
                continue;
            if (!allow_all)
                break;
        }
    }
    if (str_ptr != str_end) {
        S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, str_ptr - str.data());
    }

    Uint4 orig_digs = digs_pre_dot + digs_post_dot;
    AutoArray<Uint1> orig_num(orig_digs);
    str_ptr = num_start;
    for (Uint4 i = 0; str_ptr < num_end; ++str_ptr) {
        if (*str_ptr == ','  ||  *str_ptr == '.')
            continue;
        orig_num[i++] = *str_ptr - '0';
    }

    Uint1* num_to_conv = orig_num.get();
    Uint4 digs_to_conv = digs_pre_dot;
    AutoArray<Uint1> mul_num;
    if (binary_suff  &&  suff_idx < s_NumSuffixes) {
        const char* coef = s_BinCoefs[suff_idx];
        Uint4 coef_size = Uint4(strlen(coef));
        mul_num = new Uint1[orig_digs + coef_size];
        memset(mul_num.get(), 0, orig_digs + coef_size);
        for (Uint4 coef_i = 0; coef_i < coef_size; ++coef_i) {
            Uint1 coef_d = Uint1(coef[coef_i] - '0');
            Uint1 carry = 0;
            Uint4 res_idx = orig_digs + coef_i;
            for (int orig_i = orig_digs - 1; orig_i >= 0; --orig_i, --res_idx) {
                Uint1 orig_d = orig_num[orig_i];
                Uint1 res_d = coef_d * orig_d + carry + mul_num[res_idx];
                carry = 0;
                while (res_d >= 10) {
                    res_d -= 10;
                    ++carry;
                }
                mul_num[res_idx] = res_d;
            }
            _ASSERT(carry <= 9);
            for (; carry != 0; --res_idx) {
                Uint1 res_d = mul_num[res_idx] + carry;
                carry = 0;
                while (res_d >= 10) {
                    res_d -= 10;
                    ++carry;
                }
                mul_num[res_idx] = res_d;
            }
        }
        digs_to_conv = orig_digs + coef_size - digs_post_dot;
        num_to_conv = mul_num.get();
        while (digs_to_conv > 1  &&  *num_to_conv == 0) {
            --digs_to_conv;
            ++num_to_conv;
        }
    }
    else if (suff_idx < s_NumSuffixes) {
        Uint4 coef_size = (suff_idx + 1) * 3;
        if (coef_size <= digs_post_dot) {
            digs_to_conv += coef_size;
            digs_post_dot -= coef_size;
        }
        else {
            digs_to_conv += digs_post_dot;
            coef_size -= digs_post_dot;
            digs_post_dot = 0;
            mul_num = new Uint1[digs_to_conv + coef_size];
            memmove(mul_num.get(), num_to_conv, digs_to_conv);
            memset(mul_num.get() + digs_to_conv, 0, coef_size);
            num_to_conv = mul_num.get();
            digs_to_conv += coef_size;
        }
    }

    const Uint8 limdiv = kMax_UI8/10;
    const int   limoff = int(kMax_UI8 % 10);
    Uint8 n = 0;
    for (Uint4 i = 0; i < digs_to_conv; ++i) {
        Uint1 d = num_to_conv[i];
        if (n >= limdiv  &&  (n > limdiv  ||  d > limoff)) {
            S2N_CONVERT_ERROR(Uint8, kEmptyStr, ERANGE, i);
        }
        n *= 10;
        n += d;
    }
    if (digs_post_dot != 0  &&  num_to_conv[digs_to_conv] >= 5) {
        if (n == kMax_UI8) {
            S2N_CONVERT_ERROR(Uint8, kEmptyStr, ERANGE, digs_to_conv);
        }
        ++n;
    }
    return n;
}


size_t NStr::StringToSizet(const CTempString& str,
                           TStringToNumFlags flags, int base)
{
#if (SIZEOF_SIZE_T > 4)
    return StringToUInt8(str, flags, base);
#else
    return StringToUInt(str, flags, base);
#endif
}



/// @internal
static void s_SignedToString(string&                 out_str,
                             unsigned long           value,
                             long                    svalue,
                             NStr::TNumToStringFlags flags,
                             int                     base)
{
    const SIZE_TYPE kBufSize = CHAR_BIT * sizeof(value);
    char  buffer[kBufSize];
    char* pos = buffer + kBufSize;
    
    if ( base == 10 ) {
        if ( svalue < 0 ) {
            value = static_cast<unsigned long>(-svalue);
        }
        
        if ( (flags & NStr::fWithCommas) ) {
            int cnt = -1;
            do {
                if (++cnt == 3) {
                    *--pos = ',';
                    cnt = 0;
                }
                unsigned long a = '0'+value;
                value /= 10;
                *--pos = char(a - value*10);
            } while ( value );
        }
        else {
            do {
                unsigned long a = '0'+value;
                value /= 10;
                *--pos = char(a - value*10);
            } while ( value );
        }

        if (svalue < 0)
            *--pos = '-';
        else if (flags & NStr::fWithSign)
            *--pos = '+';
    }
    else if ( base == 16 ) {
        do {
            *--pos = kDigit[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = kDigit[value % base];
            value /= base;
        } while ( value );
    }

    out_str.assign(pos, buffer + kBufSize - pos);
}


void NStr::IntToString(string& out_str, int svalue,
                       TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        CNcbiError::SetErrno(errno = EINVAL);
        return;
    }
    unsigned int value = static_cast<unsigned int>(svalue);
    
    if ( base == 10  &&  svalue < 0 ) {
        value = static_cast<unsigned int>(-svalue);
    }
    s_SignedToString(out_str, value, svalue, flags, base);
    errno = 0;
}


void NStr::LongToString(string& out_str, long svalue,
                       TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        CNcbiError::SetErrno(errno = EINVAL);
        return;
    }
    unsigned long value = static_cast<unsigned long>(svalue);
    
    if ( base == 10  &&  svalue < 0 ) {
        value = static_cast<unsigned long>(-svalue);
    }
    s_SignedToString(out_str, value, svalue, flags, base);
    errno = 0;
}


void NStr::ULongToString(string&          out_str,
                        unsigned long     value,
                        TNumToStringFlags flags,
                        int               base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        CNcbiError::SetErrno(errno = EINVAL);
        return;
    }

    const SIZE_TYPE kBufSize = CHAR_BIT * sizeof(value);
    char  buffer[kBufSize];
    char* pos = buffer + kBufSize;

    if ( base == 10 ) {
        if ( (flags & fWithCommas) ) {
            int cnt = -1;
            do {
                if (++cnt == 3) {
                    *--pos = ',';
                    cnt = 0;
                }
                unsigned long a = '0'+value;
                value /= 10;
                *--pos = char(a - value*10);
            } while ( value );
        }
        else {
            do {
                unsigned long a = '0'+value;
                value /= 10;
                *--pos = char(a - value*10);
            } while ( value );
        }

        if ( (flags & fWithSign) ) {
            *--pos = '+';
        }
    }
    else if ( base == 16 ) {
        do {
            *--pos = kDigit[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = kDigit[value % base];
            value /= base;
        } while ( value );
    }

    out_str.assign(pos, buffer + kBufSize - pos);
    errno = 0;
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
    if ( base == 10 ) {
        if ( (flags & NStr::fWithCommas) ) {
            int cnt = -1;
#ifdef PRINT_INT8_CHUNK
            // while n doesn't fit in Uint4 process the number
            // by 9-digit chunks within 32-bit Uint4
            while ( value & ~Uint8(Uint4(~0)) ) {
                Uint4 chunk = Uint4(value);
                value /= PRINT_INT8_CHUNK;
                chunk -= PRINT_INT8_CHUNK*Uint4(value);
                char* end = pos - PRINT_INT8_CHUNK_SIZE - 2; // 9-digit chunk should have 2 commas
                do {
                    if (++cnt == 3) {
                        *--pos = ',';
                        cnt = 0;
                    }
                    Uint4 a = '0'+chunk;
                    chunk /= 10;
                    *--pos = char(a-10*chunk);
                } while ( pos != end );
            }
            // process all remaining digits in 32-bit number
            Uint4 chunk = Uint4(value);
            do {
                if (++cnt == 3) {
                    *--pos = ',';
                    cnt = 0;
                }
                Uint4 a = '0'+chunk;
                chunk /= 10;
                *--pos = char(a-10*chunk);
            } while ( chunk );
#else
            do {
                if (++cnt == 3) {
                    *--pos = ',';
                    cnt = 0;
                }
                Uint8 a = '0'+value;
                value /= 10;
                *--pos = char(a - 10*value);
            } while ( value );
#endif
        }
        else {
#ifdef PRINT_INT8_CHUNK
            // while n doesn't fit in Uint4 process the number
            // by 9-digit chunks within 32-bit Uint4
            while ( value & ~Uint8(Uint4(~0)) ) {
                Uint4 chunk = Uint4(value);
                value /= PRINT_INT8_CHUNK;
                chunk -= PRINT_INT8_CHUNK*Uint4(value);
                char* end = pos - PRINT_INT8_CHUNK_SIZE;
                do {
                    Uint4 a = '0'+chunk;
                    chunk /= 10;
                    *--pos = char(a-10*chunk);
                } while ( pos != end );
            }
            // process all remaining digits in 32-bit number
            Uint4 chunk = Uint4(value);
            do {
                Uint4 a = '0'+chunk;
                chunk /= 10;
                *--pos = char(a-10*chunk);
            } while ( chunk );
#else
            do {
                Uint8 a = '0'+value;
                value /= 10;
                *--pos = char(a-10*value);
            } while ( value );
#endif
        }
    }
    else if ( base == 16 ) {
        do {
            *--pos = kDigit[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = kDigit[value % base];
            value /= base;
        } while ( value );
    }
    return pos;
}


void NStr::Int8ToString(string& out_str, Int8 svalue,
                        TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        CNcbiError::SetErrno(errno = EINVAL);
        return;
    }
    Uint8 value;
    if (base == 10) {
        value = static_cast<Uint8>(svalue<0?-svalue:svalue);
    } else {
        value = static_cast<Uint8>(svalue);
    }
    const SIZE_TYPE kBufSize = CHAR_BIT * sizeof(value);
    char  buffer[kBufSize];

    char* pos = s_PrintUint8(buffer + kBufSize, value, flags, base);

    if (base == 10) {
        if (svalue < 0)
            *--pos = '-';
        else if (flags & fWithSign)
            *--pos = '+';
    }
    out_str.assign(pos, buffer + kBufSize - pos);
    errno = 0;
}


void NStr::UInt8ToString(string& out_str, Uint8 value,
                         TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        CNcbiError::SetErrno(errno = EINVAL);
        return;
    }
    const SIZE_TYPE kBufSize = CHAR_BIT  * sizeof(value);
    char  buffer[kBufSize];

    char* pos = s_PrintUint8(buffer + kBufSize, value, flags, base);

    if ( (base == 10)  &&  (flags & fWithSign) ) {
        *--pos = '+';
    }
    out_str.assign(pos, buffer + kBufSize - pos);
    errno = 0;
}


void NStr::UInt8ToString_DataSize(string& out_str,
                                  Uint8 value,
                                  TNumToStringFlags flags /* = 0 */,
                                  unsigned int max_digits /* = 3 */)
{
    TNumToStringFlags allowed_flags = fWithSign +
                                      fWithCommas + 
                                      fDS_Binary + 
                                      fDS_NoDecimalPoint +
                                      fDS_PutSpaceBeforeSuffix +
                                      fDS_ShortSuffix +
                                      fDS_PutBSuffixToo;

    if ((flags & allowed_flags) != flags) {
        NCBI_THROW2(CStringException, eConvert, "Wrong set of flags", 0);
    }

    if (max_digits < 3)
        max_digits = 3;

    static const char s_Suffixes[] = {'K', 'M', 'G', 'T', 'P', 'E'};
    static const Uint4 s_NumSuffixes = sizeof(s_Suffixes) / sizeof(s_Suffixes[0]);

    static const SIZE_TYPE kBufSize = 50;
    char  buffer[kBufSize];
    char* num_start;
    char* dot_ptr;
    char* num_end;
    Uint4 digs_pre_dot, suff_idx;

    if (!(flags &fDS_Binary)) {
        static const Uint8 s_Coefs[] = {1000, 1000000, 1000000000,
                                        NCBI_CONST_UINT8(1000000000000),
                                        NCBI_CONST_UINT8(1000000000000000),
                                        NCBI_CONST_UINT8(1000000000000000000)};
        suff_idx = 0;
        for (; suff_idx < s_NumSuffixes; ++suff_idx) {
            if (value < s_Coefs[suff_idx])
                break;
        }
        num_start = s_PrintUint8(buffer + kBufSize, value, 0, 10);
        num_start[-1] = '0';
        dot_ptr = buffer + kBufSize - 3 * suff_idx;
        digs_pre_dot = Uint4(dot_ptr - num_start);
        if (!(flags & fDS_NoDecimalPoint)) {
            num_end = min(buffer + kBufSize, dot_ptr + (max_digits - digs_pre_dot));
        }
        else {
            while (suff_idx > 0  &&  max_digits - digs_pre_dot >= 3) {
                --suff_idx;
                digs_pre_dot += 3;
                dot_ptr += 3;
            }
            num_end = dot_ptr;
        }
        char* round_dig = num_end - 1;
        if (num_end < buffer + kBufSize  &&  *num_end >= '5')
            ++(*round_dig);
        while (*round_dig == '0' + 10) {
            *round_dig = '0';
            --round_dig;
            ++(*round_dig);
        }
        if (round_dig < num_start) {
            _ASSERT(num_start - round_dig == 1);
            num_start = round_dig;
            ++digs_pre_dot;
            if (!(flags & fDS_NoDecimalPoint)) {
                if (digs_pre_dot > 3) {
                    ++suff_idx;
                    digs_pre_dot -= 3;
                    dot_ptr -= 3;
                }
                --num_end;
            }
            else {
                if (digs_pre_dot > max_digits) {
                    ++suff_idx;
                    digs_pre_dot -= 3;
                    dot_ptr -= 3;
                    num_end = dot_ptr;
                }
            }
        }
    }
    else {
        static const Uint8 s_Coefs[] = {1, 1024, 1048576, 1073741824,
                                        NCBI_CONST_UINT8(1099511627776),
                                        NCBI_CONST_UINT8(1125899906842624),
                                        NCBI_CONST_UINT8(1152921504606846976)};

        suff_idx = 1;
        for (; suff_idx < s_NumSuffixes; ++suff_idx) {
            if (value < s_Coefs[suff_idx])
                break;
        }
        bool can_try_another = true;
try_another_suffix:
        Uint8 mul_coef = s_Coefs[suff_idx - 1];
        Uint8 whole_num = value / mul_coef;
        if (max_digits == 3  &&  whole_num >= 1000) {
            ++suff_idx;
            goto try_another_suffix;
        }
        num_start = s_PrintUint8(buffer + kBufSize, whole_num, 0, 10);
        num_start[-1] = '0';
        digs_pre_dot = Uint4(buffer + kBufSize - num_start);
        if (max_digits - digs_pre_dot >= 3  &&  (flags & fDS_NoDecimalPoint)
            &&  suff_idx != 1  &&  can_try_another)
        {
            Uint4 new_suff = suff_idx - 1;
try_even_more_suffix:
            Uint8 new_num = value / s_Coefs[new_suff - 1];
            char* new_start = s_PrintUint8(buffer + kBufSize / 2, new_num, 0, 10);
            Uint4 new_digs = Uint4(buffer + kBufSize / 2 - new_start);
            if (new_digs <= max_digits) {
                if (max_digits - digs_pre_dot >= 3  &&  new_suff != 1) {
                    --new_suff;
                    goto try_even_more_suffix;
                }
                suff_idx = new_suff;
                can_try_another = false;
                goto try_another_suffix;
            }
            if (new_suff != suff_idx - 1) {
                suff_idx = new_suff + 1;
                can_try_another = false;
                goto try_another_suffix;
            }
        }
        memcpy(buffer, num_start - 1, digs_pre_dot + 1);
        num_start = buffer + 1;
        dot_ptr = num_start + digs_pre_dot;
        Uint4 cnt_more_digs = 1;
        if (!(flags & fDS_NoDecimalPoint))
            cnt_more_digs += min(max_digits - digs_pre_dot, 3 * (suff_idx - 1));
        num_end = dot_ptr;
        Uint8 left_val = value - whole_num * mul_coef;
        do {
            left_val *= 10;
            Uint1 d = Uint1(left_val / mul_coef);
            *num_end = d + '0';
            ++num_end;
            left_val -= d * mul_coef;
            --cnt_more_digs;
        }
        while (cnt_more_digs != 0);
        --num_end;

        char* round_dig = num_end - 1;
        if (*num_end >= '5')
            ++(*round_dig);
        while (*round_dig == '0' + 10) {
            *round_dig = '0';
            --round_dig;
            ++(*round_dig);
        }
        if (round_dig < num_start) {
            _ASSERT(round_dig == buffer);
            num_start = round_dig;
            ++digs_pre_dot;
            if (digs_pre_dot > max_digits) {
                ++suff_idx;
                goto try_another_suffix;
            }
            if (num_end != dot_ptr)
                --num_end;
        }
        if (!(flags & fDS_NoDecimalPoint)  &&  digs_pre_dot == 4
            &&  num_start[0] == '1'  &&  num_start[1] == '0'
            &&  num_start[2] == '2'  &&  num_start[3] == '4')
        {
            ++suff_idx;
            goto try_another_suffix;
        }

        --suff_idx;
    }

    out_str.erase();
    if (flags & fWithSign)
        out_str.append(1, '+');
    if (!(flags & fWithCommas)  ||  digs_pre_dot <= 3) {
        out_str.append(num_start, digs_pre_dot);
    }
    else {
        Uint4 digs_first = digs_pre_dot % 3;
        out_str.append(num_start, digs_first);
        char* left_ptr = num_start + digs_first;
        Uint4 digs_left = digs_pre_dot - digs_first;
        while (digs_left != 0) {
            out_str.append(1, ',');
            out_str.append(left_ptr, 3);
            left_ptr += 3;
            digs_left -= 3;
        }
    }
    if (num_end != dot_ptr) {
        out_str.append(1, '.');
        out_str.append(dot_ptr, num_end - dot_ptr);
    }

    if (suff_idx == 0) {
        if (flags & fDS_PutBSuffixToo) {
            if (flags & fDS_PutSpaceBeforeSuffix)
                out_str.append(1, ' ');
            out_str.append(1, 'B');
        }
    }
    else {
        --suff_idx;
        if (flags & fDS_PutSpaceBeforeSuffix)
            out_str.append(1, ' ');
        out_str.append(1, s_Suffixes[suff_idx]);
        if (!(flags & fDS_ShortSuffix)) {
            if (flags & fDS_Binary)
                out_str.append(1, 'i');
            out_str.append(1, 'B');
        }
    }
    errno = 0;
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


void NStr::DoubleToString(string& out_str, double value,
                          int precision, TNumToStringFlags flags)
{
    char buffer[kMaxDoubleStringSize];
    if (precision >= 0 ||
        ((flags & fDoublePosix) && (isnan(value) || !finite(value)))) {
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
        if (flags & fDoublePosix) {
            struct lconv* conv = localeconv();
            if ('.' != *(conv->decimal_point)) {
                char* pos = strchr(buffer, *(conv->decimal_point));
                if (pos) {
                    *pos = '.';
                }
            }
        }
    }
    out_str = buffer;
    errno = 0;
}



SIZE_TYPE NStr::DoubleToString(double value, unsigned int precision,
                               char* buf, SIZE_TYPE buf_size,
                               TNumToStringFlags flags)
{
    char buffer[kMaxDoubleStringSize];
    int n = 0;
    if ((flags & fDoublePosix) && (isnan(value) || !finite(value))) {
        if (isnan(value)) {
            strcpy(buffer, "NaN");
            n = 4;
        } else if (value > 0.) {
            strcpy(buffer, "INF");
            n = 4;
        } else {
            strcpy(buffer, "-INF");
            n = 5;
        }
    } else {
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
        n = ::sprintf(buffer, format, (int)precision, value);
        if (n < 0) {
            n = 0;
        }
        if (flags & fDoublePosix) {
            struct lconv* conv = localeconv();
            if ('.' != *(conv->decimal_point)) {
                char* pos = strchr(buffer, *(conv->decimal_point));
                if (pos) {
                    *pos = '.';
                }
            }
        }
    }
    SIZE_TYPE n_copy = min((SIZE_TYPE) n, buf_size);
    memcpy(buf, buffer, n_copy);
    errno = 0;
    return n_copy;
}


char* s_ncbi_append_int2str(char* buffer, unsigned int value, size_t digits, bool zeros)
{
    char* buffer_start = buffer;
    char* buffer_end = (buffer += digits-1);
    if (zeros) {
        do {
            *buffer-- = (char)(48 + (value % 10));
            value /= 10;
        } while (--digits);
    } else {
        do {
            *buffer-- = (char)(48 + (value % 10));
        } while (value /= 10);

        if (++buffer != buffer_start) {
            memmove(buffer_start, buffer, buffer_end-buffer+1);
            buffer_end -= buffer - buffer_start;
        }
    }
    return ++buffer_end;
}


#define __NLG NCBI_CONST_LONGDOUBLE

SIZE_TYPE NStr::DoubleToString_Ecvt(double val, unsigned int precision,
                                    char* buffer, SIZE_TYPE bufsize,
                                    int* dec, int* sign)
{
    //errno = 0;
    *dec = *sign = 0;
    if (precision==0) {
        return 0;
    }
    if (precision > DBL_DIG) {
        precision = DBL_DIG;
    }
    if (val == 0.) {
        *buffer='0';
        return 1;
    }
    *sign = val < 0. ? -1 : 1;
    if (*sign < 0) {
        val = -val;
    }
    bool high_precision = precision > 9;

// calculate exponent
    unsigned int exp=0;
    bool exp_positive = val >= 1.;
    unsigned int first, second=0;
    long double mult = __NLG(1.);
    long double value = val;

    if (exp_positive) {
        while (value>=__NLG(1.e256))
            {value/=__NLG(1.e256); exp+=256;}
        if (value >= __NLG(1.e16)) {
            if      (value>=__NLG(1.e240)) {value*=__NLG(1.e-240); exp+=240;}
            else if (value>=__NLG(1.e224)) {value*=__NLG(1.e-224); exp+=224;}
            else if (value>=__NLG(1.e208)) {value*=__NLG(1.e-208); exp+=208;}
            else if (value>=__NLG(1.e192)) {value*=__NLG(1.e-192); exp+=192;}
            else if (value>=__NLG(1.e176)) {value*=__NLG(1.e-176); exp+=176;}
            else if (value>=__NLG(1.e160)) {value*=__NLG(1.e-160); exp+=160;}
            else if (value>=__NLG(1.e144)) {value*=__NLG(1.e-144); exp+=144;}
            else if (value>=__NLG(1.e128)) {value*=__NLG(1.e-128); exp+=128;}
            else if (value>=__NLG(1.e112)) {value*=__NLG(1.e-112); exp+=112;}
            else if (value>=__NLG(1.e96))  {value*=__NLG(1.e-96);  exp+=96;}
            else if (value>=__NLG(1.e80))  {value*=__NLG(1.e-80);  exp+=80;}
            else if (value>=__NLG(1.e64))  {value*=__NLG(1.e-64);  exp+=64;}
            else if (value>=__NLG(1.e48))  {value*=__NLG(1.e-48);  exp+=48;}
            else if (value>=__NLG(1.e32))  {value*=__NLG(1.e-32);  exp+=32;}
            else if (value>=__NLG(1.e16))  {value*=__NLG(1.e-16);  exp+=16;}
        }
        if      (value<   __NLG(1.)) {mult=__NLG(1.e+9); exp-= 1;}
        else if (value<  __NLG(10.)) {mult=__NLG(1.e+8);         }
        else if (value< __NLG(1.e2)) {mult=__NLG(1.e+7); exp+= 1;}
        else if (value< __NLG(1.e3)) {mult=__NLG(1.e+6); exp+= 2;}
        else if (value< __NLG(1.e4)) {mult=__NLG(1.e+5); exp+= 3;}
        else if (value< __NLG(1.e5)) {mult=__NLG(1.e+4); exp+= 4;}
        else if (value< __NLG(1.e6)) {mult=__NLG(1.e+3); exp+= 5;}
        else if (value< __NLG(1.e7)) {mult=__NLG(1.e+2); exp+= 6;}
        else if (value< __NLG(1.e8)) {mult=  __NLG(10.); exp+= 7;}
        else if (value< __NLG(1.e9)) {mult=   __NLG(1.); exp+= 8;}
        else if (value<__NLG(1.e10)) {mult=  __NLG(0.1); exp+= 9;}
        else if (value<__NLG(1.e11)) {mult=__NLG(1.e-2); exp+=10;}
        else if (value<__NLG(1.e12)) {mult=__NLG(1.e-3); exp+=11;}
        else if (value<__NLG(1.e13)) {mult=__NLG(1.e-4); exp+=12;}
        else if (value<__NLG(1.e14)) {mult=__NLG(1.e-5); exp+=13;}
        else if (value<__NLG(1.e15)) {mult=__NLG(1.e-6); exp+=14;}
        else if (value<__NLG(1.e16)) {mult=__NLG(1.e-7); exp+=15;}
        else                         {mult=__NLG(1.e-8); exp+=16;}
    } else {
        while (value<=__NLG(1.e-256))
            {value*=__NLG(1.e256); exp+=256;}
        if (value <= __NLG(1.e-16)) {
            if      (value<=__NLG(1.e-240)) {value*=__NLG(1.e240); exp+=240;}
            else if (value<=__NLG(1.e-224)) {value*=__NLG(1.e224); exp+=224;}
            else if (value<=__NLG(1.e-208)) {value*=__NLG(1.e208); exp+=208;}
            else if (value<=__NLG(1.e-192)) {value*=__NLG(1.e192); exp+=192;}
            else if (value<=__NLG(1.e-176)) {value*=__NLG(1.e176); exp+=176;}
            else if (value<=__NLG(1.e-160)) {value*=__NLG(1.e160); exp+=160;}
            else if (value<=__NLG(1.e-144)) {value*=__NLG(1.e144); exp+=144;}
            else if (value<=__NLG(1.e-128)) {value*=__NLG(1.e128); exp+=128;}
            else if (value<=__NLG(1.e-112)) {value*=__NLG(1.e112); exp+=112;}
            else if (value<=__NLG(1.e-96))  {value*=__NLG(1.e96);  exp+=96;}
            else if (value<=__NLG(1.e-80))  {value*=__NLG(1.e80);  exp+=80;}
            else if (value<=__NLG(1.e-64))  {value*=__NLG(1.e64);  exp+=64;}
            else if (value<=__NLG(1.e-48))  {value*=__NLG(1.e48);  exp+=48;}
            else if (value<=__NLG(1.e-32))  {value*=__NLG(1.e32);  exp+=32;}
            else if (value<=__NLG(1.e-16))  {value*=__NLG(1.e16);  exp+=16;}
        }
        if      (value<__NLG(1.e-15)) {mult=__NLG(1.e24); exp+=16;}
        else if (value<__NLG(1.e-14)) {mult=__NLG(1.e23); exp+=15;}
        else if (value<__NLG(1.e-13)) {mult=__NLG(1.e22); exp+=14;}
        else if (value<__NLG(1.e-12)) {mult=__NLG(1.e21); exp+=13;}
        else if (value<__NLG(1.e-11)) {mult=__NLG(1.e20); exp+=12;}
        else if (value<__NLG(1.e-10)) {mult=__NLG(1.e19); exp+=11;}
        else if (value<__NLG(1.e-9))  {mult=__NLG(1.e18); exp+=10;}
        else if (value<__NLG(1.e-8))  {mult=__NLG(1.e17); exp+=9;}
        else if (value<__NLG(1.e-7))  {mult=__NLG(1.e16); exp+=8;}
        else if (value<__NLG(1.e-6))  {mult=__NLG(1.e15); exp+=7;}
        else if (value<__NLG(1.e-5))  {mult=__NLG(1.e14); exp+=6;}
        else if (value<__NLG(1.e-4))  {mult=__NLG(1.e13); exp+=5;}
        else if (value<__NLG(1.e-3))  {mult=__NLG(1.e12); exp+=4;}
        else if (value<__NLG(1.e-2))  {mult=__NLG(1.e11); exp+=3;}
        else if (value<__NLG(1.e-1))  {mult=__NLG(1.e10); exp+=2;}
        else if (value<__NLG(1.))     {mult=__NLG(1.e9);  exp+=1;}
        else                          {mult=__NLG(1.e8);         }
    }

// get all digits
    long double t1 = value * mult;
    if (t1 >= __NLG(1.e9)) {
        first = 999999999;
    } else if (t1 < __NLG(1.e8)) {
        first = 100000000;
        t1 = first;
    } else {
        first = (unsigned int)t1;
    }
    if (high_precision) {
        long double t2 = (t1-first) * __NLG(1.e8);
        if (t2 >= __NLG(1.e8)) {
            second = 99999999;
        } else {
            second = (unsigned int)t2;
        }
    }

// convert them into string
    bool use_ext_buffer = bufsize > 20;
    char tmp[32];
    char *digits = use_ext_buffer ? buffer : tmp;
    char *digits_end = s_ncbi_append_int2str(digits,first,9,false);
    if (high_precision) {
        digits_end = s_ncbi_append_int2str(digits_end,second,8,true);
    }
    size_t digits_len = digits_end - digits;
    size_t digits_got = digits_len;
    size_t digits_expected = high_precision ? 17 : 9;

// get significant digits according to requested precision
    size_t pos = precision;
    if (digits_len > precision) {
        digits_len = precision;

        // this is questionable, but in fact,
        // improves the result (on average)
#if 1
        if (high_precision) {
            if (digits[pos] == '4') {
                size_t pt = pos-1;
                while (pt != 0 && digits[--pt] == '9')
                    ;
                if (pt != 0 && (pos-pt) > precision/2)
                    digits[pos]='5';
            } else if (digits[pos] == '5') {
                size_t pt = pos;
                while (pt != 0 && digits[--pt] == '0')
                    ;
                if (pt != 0 && (pos-pt) > precision/2)
                    digits[pos]='4';
            }
        }
#endif

        if (digits[pos] >= '5') {
            do {
                if (digits[--pos] < '9') {
                    ++digits[pos++];
                    break;
                }
                digits[pos]='0';
            } while (pos > 0);
            if (pos == 0) {
    	        if (digits_expected <= digits_got) {
                    if (exp_positive) {
                       ++exp; 
                    } else {
// exp cannot be 0, by design
                        exp_positive = --exp == 0;
                    }
		        }
                *digits = '1';
                digits_len = 1;
            }
        }
    }

// truncate trailing zeros
    for (pos = digits_len; pos-- > 0 && digits[pos] == '0';)
        --digits_len;

    *dec = (int)exp;
    if (!exp_positive) {
        *dec = -*dec;
    }
    if (!use_ext_buffer) {
        if (digits_len <= bufsize) {
            strncpy(buffer,digits,digits_len);
        } else {
            NCBI_THROW2(CStringException, eConvert,
                        "Destination buffer too small", 0);
        }
    }
    return digits_len;
}
#undef __NLG


SIZE_TYPE NStr::DoubleToStringPosix(double val, unsigned int precision,
                                    char* buffer, SIZE_TYPE bufsize)
{
    if (bufsize < precision+8) {
        NCBI_THROW2(CStringException, eConvert,
                    "Destination buffer too small", 0);
    }
    int dec=0, sign=0;
    char digits[32];
    size_t digits_len = DoubleToString_Ecvt(
        val, precision, digits, sizeof(digits), &dec, &sign);
    if (digits_len == 0) {
        errno = 0;
        return 0;
    }
    if (digits_len == 1 && dec == 0 && sign >=0) {
        *buffer = digits[0];
        errno = 0;
        return 1;
    }
    bool exp_positive = dec >= 0;
    unsigned int exp= (unsigned int)(exp_positive ? dec : (-dec));

// assemble the result
    char *buffer_pos = buffer;
//    char *buffer_end = buffer + bufsize;
    char *digits_pos = digits;

    if (sign < 0) {
        *buffer_pos++ = '-';
    }
// The 'e' format is used when the exponent of the value is less than -4
//  or greater than or equal to the precision argument
    if ((exp_positive && exp >= precision) || (!exp_positive && exp > 4)) {
        *buffer_pos++ = *digits_pos++;
        --digits_len;
        if (digits_len != 0) {
            *buffer_pos++ = '.';
            strncpy(buffer_pos,digits_pos,digits_len);
            buffer_pos += digits_len;
        }
        *buffer_pos++ = 'e';
        *buffer_pos++ = exp_positive ? '+' : '-';

#if defined(NCBI_OS_MSWIN)
        bool need_zeros = true;
        size_t need_digits = 3;
#else
        bool need_zeros = exp < 10 ? true : false;
        size_t need_digits = exp < 100 ? 2 : 3;
#endif
// assuming exp < 1000
        buffer_pos = s_ncbi_append_int2str(buffer_pos, exp, need_digits,need_zeros);
    } else if (exp_positive) {
        *buffer_pos++ = *digits_pos++;
        --digits_len;
        if (digits_len > exp) {
            strncpy(buffer_pos,digits_pos,exp);
            buffer_pos += exp;
            *buffer_pos++ = '.';
            strncpy(buffer_pos,digits_pos+exp,digits_len-exp);
            buffer_pos += digits_len-exp;
        } else {
            strncpy(buffer_pos,digits_pos,digits_len);
            buffer_pos += digits_len;
            exp -= (unsigned int)digits_len;
            while (exp--) {
                *buffer_pos++ = '0';
            }
        }
    } else {
        *buffer_pos++ = '0';
        *buffer_pos++ = '.';
        for (--exp; exp--;) {
            *buffer_pos++ = '0';
        }
        strncpy(buffer_pos,digits_pos, digits_len);
        buffer_pos += digits_len;
    }
    errno = 0;
    return buffer_pos - buffer;
}


string NStr::SizetToString(size_t value, TNumToStringFlags flags, int base)
{
#if (SIZEOF_SIZE_T > 4)
    return UInt8ToString(value, flags, base);
#else
    return UIntToString(static_cast<unsigned int>(value), flags, base);
#endif
}


string NStr::PtrToString(const void* value)
{
    errno = 0;
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    return buffer;
}


void NStr::PtrToString(string& out_str, const void* value)
{
    errno = 0;
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    out_str = buffer;
}


const void* NStr::StringToPtr(const CTempStringEx& str)
{
    int& errno_ref = errno;
    errno_ref = 0;
    void *ptr = NULL;
    int res;
    if ( str.HasZeroAtEnd() ) {
        res = ::sscanf(str.data(), "%p", &ptr);
    } else {
        res = ::sscanf(string(str).c_str(), "%p", &ptr);
    }
    if (res != 1) {
        CNcbiError::SetErrno(errno_ref = EINVAL, str);
        return NULL;
    }
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


bool NStr::StringToBool(const CTempString& str)
{
    if ( AStrEquiv(str, s_kTrueString,  PNocase())  ||
         AStrEquiv(str, s_kTString,     PNocase())  ||
         AStrEquiv(str, s_kYesString,   PNocase())  ||
         AStrEquiv(str, s_kYString,     PNocase()) ) {
        errno = 0;
        return true;
    }
    if ( AStrEquiv(str, s_kFalseString, PNocase())  ||
         AStrEquiv(str, s_kFString,     PNocase())  ||
         AStrEquiv(str, s_kNoString,    PNocase())  ||
         AStrEquiv(str, s_kNString,     PNocase()) ) {
        errno = 0;
        return false;
    }
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
    SIZE_TYPE size = 1024;
    AutoArray<char> buf(size);
    buf.get()[size-1] = buf.get()[size-2] = 0;
    SIZE_TYPE n = vsnprintf(buf.get(), size, format, args);
    while (n >= size  ||  buf.get()[size-2]) {
        if (buf.get()[size-1]) {
            ERR_POST_X(1, Warning << "Buffer overrun by buggy vsnprintf");
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
        ERR_POST_X(2, Warning << "Buffer overrun by vsprintf");
    }
    return buf;

#else
#  error Please port this code to your system.
#endif
}


SIZE_TYPE NStr::FindNoCase(const CTempString& str, const CTempString& pattern,
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


const string* NStr::Find(const list <string>& lst, const CTempString& val,
                         ECase use_case)
{
   if (lst.empty()) return NULL;
   ITERATE (list<string>, st_itr, lst) {
       if (Equal(*st_itr, val, use_case)) {
           return &*st_itr;
       }
   }
   return NULL;
}

const string* NStr::Find(const vector <string>& vec, const CTempString& val,
                         ECase use_case)
{
   if (vec.empty()) return NULL;
   ITERATE (vector<string>, st_itr, vec) {
       if (Equal(*st_itr, val, use_case)) {
           return &*st_itr;
       }
   }
   return NULL;
}


SIZE_TYPE NStr::CommonOverlapSize(const CTempString& s1, const CTempString& s2)
{
    const SIZE_TYPE len1 = s1.length();
    const SIZE_TYPE len2 = s2.length();

    // Eliminate the null case
    if (len1 == 0 || len2 == 0) {
        return 0;
    }
    SIZE_TYPE len = min(len1, len2);

    // Truncate the longer string
    CTempString t1, t2;
    if (len1 > len2) {
        t1 = s1.substr(len1-len, len);
        t2 = s2;
    } else {
        t1 = s1;
        t2 = s2.substr(0, len);
    }
    // Quick check for the worst case
    if (memcmp(t1.data(), t2.data(), len) == 0) {
        return len;
    }

    // Start by looking for a single character match
    // and increase length until no match is found.
    // Performance analysis: http://neil.fraser.name/news/2010/11/04/
    SIZE_TYPE best = 0;
    SIZE_TYPE n = 1;
    for (;;) {
        // Right 'n' symbols of 't1'
        CTempString pattern(t1.data() + len - n, n);
        SIZE_TYPE pos = t2.find(pattern);
        if (pos == NPOS) {
            return best;
        }
        n += pos;
        if (pos == 0 || memcmp(pattern.data(), t2.data(), n) == 0) {
            best = n;
            n++;
        }
    }
    // Unreachable
    return best;
}


template <class TStr>
TStr s_TruncateSpaces(const TStr& str, NStr::ETrunc where,
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
        _ASSERT(beg < end);
        while (isspace((unsigned char) str[--end])) {
            if (beg == end) {
                return empty_str;
            }
        }
        _ASSERT(beg <= end  &&  !isspace((unsigned char) str[end]));
        ++end;
    }
    _ASSERT(beg < end  &&  end <= length);
    if ( beg | (end - length) ) { // if either beg != 0 or end != length
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

CTempString NStr::TruncateSpaces_Unsafe(const CTempString& str, ETrunc where)
{
    return s_TruncateSpaces(str, where, CTempString());
}

//CTempString NStr::TruncateSpaces_Unsafe(const char* str, ETrunc where)
//{
//    return s_TruncateSpaces(CTempString(str), where, CTempString());
//}

void NStr::TruncateSpacesInPlace(CTempString& str, ETrunc where)
{
    str = s_TruncateSpaces(str, where, CTempString());
}

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
        _ASSERT(beg < end);
        while (isspace((unsigned char) str.data()[--end])) {
            if (beg == end) {
                str.erase();
                return;
            }
        }
        _ASSERT(beg <= end  &&  !isspace((unsigned char) str.data()[end]));
        ++end;
    }
    _ASSERT(beg < end  &&  end <= length);

#if defined(NCBI_COMPILER_GCC)  &&  (NCBI_COMPILER_VERSION == 304)
    // work around a library bug
    str.replace(end, length, kEmptyStr);
    str.replace(0, beg, kEmptyStr);
#else
    if ( beg | (end - length) ) { // if either beg != 0 or end != length
        str.replace(0, length, str, beg, end - beg);
    }
#endif
}


string& NStr::Replace(const string& src,
                      const string& search, const string& replace,
                      string& dst, SIZE_TYPE start_pos, SIZE_TYPE max_replace)
{
    // source and destination should not be the same
    if (&src == &dst) {
        NCBI_THROW2(CStringException, eBadArgs,
                    "NStr::Replace():  source and destination are the same",0);
    }
    if ( start_pos + search.size() > src.size()  ||  search == replace ) {
        dst = src;
        return dst;
    }

    // Use different algorithms depending on size or 'search' and 'replace'
    // for better performance (and for big strings only! > 16KB).

    if (replace.size() > search.size()  &&  src.size() > 16*1024) {
        // Replacing string is longer -- worst case.
        // Try to avoid memory reallocations inside std::string.
        // Count replacing strings first
        SIZE_TYPE n = 0;
        SIZE_TYPE start_orig = start_pos;
        for (SIZE_TYPE count = 0; !(max_replace && count >= max_replace); count++){
            start_pos = src.find(search, start_pos);
            if (start_pos == NPOS)
                break;
            n++;
            start_pos += search.size();
        }
        // Reallocate memory for destination string
        dst.resize(src.size() - n*search.size() + n*replace.size());

        // Use copy() to create destination string
        start_pos = start_orig;
        string::const_iterator src_start = src.begin();
        string::const_iterator src_end   = src.begin();
        string::iterator       dst_pos   = dst.begin();

        for (SIZE_TYPE count = 0; !(max_replace && count >= max_replace); count++){
            start_pos = src.find(search, start_pos);
            if (start_pos == NPOS)
                break;
            // Copy from source string up to 'search'
            src_end = src.begin() + start_pos;
            copy(src_start, src_end, dst_pos); 
            dst_pos += (src_end - src_start);
            // Append 'replace'
            copy(replace.begin(), replace.end(), dst_pos); 
            dst_pos   += replace.size();
            start_pos += search.size();
            src_start = src.begin() + start_pos; 
        }
        // Copy source's string tail to the place
        copy(src_start, src.end(), dst_pos); 

    } else {
        // Replacing string is shorter or have the same length.
        // ReplaceInPlace() can be faster on some platform, but not much,
        // so we use regular algorithm even for equal lengths here.
        dst = src;
        for (SIZE_TYPE count = 0; !(max_replace && count >= max_replace); count++){
            start_pos = dst.find(search, start_pos);
            if (start_pos == NPOS)
                break;
            dst.replace(start_pos, search.size(), replace);
            start_pos += replace.size();
        }
    }
    return dst;
}


string NStr::Replace(const string& src,
                     const string& search, const string& replace,
                     SIZE_TYPE start_pos, SIZE_TYPE max_replace)
{
    string dst;
    Replace(src, search, replace, dst, start_pos, max_replace);
    return dst;
}


string& NStr::ReplaceInPlace(string& src,
                             const string& search, const string& replace,
                             SIZE_TYPE start_pos, SIZE_TYPE max_replace)
{
    if ( start_pos + search.size() > src.size()  ||
         search == replace )
        return src;

    bool equal_len = (search.size() == replace.size());
    for (SIZE_TYPE count = 0; !(max_replace && count >= max_replace); count++){
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


template<typename TString, typename TContainer>
TContainer& s_Split(const TString& str, const TString& delim,
                    TContainer& arr, NStr::TSplitFlags flags,
                    vector<SIZE_TYPE>* token_pos,
                    CTempString_Storage* storage = NULL)
{
    typedef CStrTokenPosAdapter<vector<SIZE_TYPE> >         TPosArray;
    typedef CStrDummyTargetReserve<TContainer, TPosArray>   TReserve;
    typedef CStrTokenize<TString, TContainer, TPosArray,
                         CStrDummyTokenCount, TReserve>     TSplitter;

    TPosArray token_pos_proxy(token_pos);
    TSplitter splitter(str, delim, flags, storage);
    splitter.Do(arr, token_pos_proxy, kEmptyStr);
    return arr;
}


list<string>& NStr::Split(const CTempString& str, const CTempString& delim,
                          list<string>& arr, TSplitFlags flags,
                          vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, flags, token_pos);
}

list<CTempStringEx>& NStr::Split(const CTempString& str,
                                 const CTempString& delim,
                                 list<CTempStringEx>& arr, TSplitFlags flags,
                                 vector<SIZE_TYPE>* token_pos,
                                 CTempString_Storage* storage)
{
    return s_Split(str, delim, arr, flags, token_pos, storage);
}

list<CTempString>& NStr::Split(const CTempString& str,
                               const CTempString& delim,
                               list<CTempString>& arr, EMergeDelims merge,
                               vector<SIZE_TYPE>* token_pos)
{
    vector<CTempStringEx> arr2;
    Tokenize(str, delim, arr2,
             (merge == eMergeDelims) ? fSplit_MergeDelims : 0, token_pos);
    ITERATE (vector<CTempStringEx>, it, arr2) {
        arr.push_back(*it);
    }
    return arr;
}


vector<string>& NStr::Tokenize(const CTempString& str, const CTempString& delim,
                               vector<string>& arr, TSplitFlags flags,
                               vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, flags, token_pos);
}

vector<CTempStringEx>& NStr::Tokenize(const CTempString& str,
                                      const CTempString& delim,
                                      vector<CTempStringEx>& arr,
                                      TSplitFlags        flags,
                                      vector<SIZE_TYPE>* token_pos,
                                      CTempString_Storage* storage)
{
    return s_Split(str, delim, arr, flags, token_pos, storage);
}

vector<CTempString>& NStr::Tokenize(const CTempString& str,
                                    const CTempString& delim,
                                    vector<CTempString>& arr,
                                    EMergeDelims merge,
                                    vector<SIZE_TYPE>* token_pos)
{
    vector<CTempStringEx> arr2;
    Tokenize(str, delim, arr2,
             (merge == eMergeDelims) ? fSplit_MergeDelims : 0, token_pos);
    arr.reserve(arr.size() + arr2.size());
    ITERATE (vector<CTempStringEx>, it, arr2) {
        arr.push_back(*it);
    }
    return arr;
}


vector<CTempString>& NStr::TokenizePattern(const CTempString& str,
                                           const CTempString& delim,
                                           vector<CTempString>& arr,
                                           EMergeDelims merge,
                                           vector<SIZE_TYPE>* token_pos)
{
    vector<CTempStringEx> arr2;
    Tokenize(str, delim, arr2,
             fSplit_ByPattern 
             | ((merge == eMergeDelims) ? fSplit_MergeDelims : 0),
             token_pos);
    arr.reserve(arr.size() + arr2.size());
    ITERATE (vector<CTempStringEx>, it, arr2) {
        arr.push_back(*it);
    }
    return arr;
}


bool NStr::SplitInTwo(const CTempString& str, 
                      const CTempString& delim,
                      string& str1, string& str2, TSplitFlags flags)
{
    CTempStringEx ts1, ts2;
    bool result = SplitInTwo(str, delim, ts1, ts2, flags);
    str1 = ts1;
    str2 = ts2;
    return result;
}

bool NStr::SplitInTwo(const CTempString& str, const CTempString& delim,
                      CTempString& str1, CTempString& str2, EMergeDelims merge)
{
    CTempStringEx tsx1, tsx2;
    bool result = SplitInTwo(str, delim, tsx1, tsx2,
                             (merge == eMergeDelims) ? fSplit_MergeDelims : 0);
    str1 = tsx1;
    str2 = tsx2;
    return result;
}

bool NStr::SplitInTwo(const CTempString& str, const CTempString& delim,
                      CTempStringEx& str1, CTempStringEx& str2,
                      TSplitFlags flags,
                      CTempString_Storage* storage)
{
    if ((flags & (fSplit_CanEscape | fSplit_CanQuote)) && !storage) {
        NCBI_THROW2(CStringException, eBadArgs,
            "NStr::SplitInTwo(): the selected flags require non-NULL storage",
            0);
    }

    typedef CStrTokenize<CTempString, int, CStrDummyTokenPos,
                         CStrDummyTokenCount,
                         CStrDummyTargetReserve<int, int> > TSplitter;

    CTempStringList part_collector(storage);
    TSplitter       splitter(str, delim, flags, storage);
    bool            found_delim;

    splitter.SkipDelims();
    if (splitter.GetPos() == 0) {
        splitter.Advance(&part_collector);
        part_collector.Join(&str1);
        part_collector.Clear();
        if (splitter.AtEnd()) {
            // check for trailing delimiter
            if ((flags & fSplit_ByPattern) != 0) {
                found_delim = NStr::EndsWith(str, delim);
            } else {
                found_delim = ( !str.empty()
                               &&  delim.find(str[str.size()-1]) != NPOS);
            }
            if (found_delim  &&  (flags & fSplit_CanEscape) != 0) {
                SIZE_TYPE dsz = ((flags & fSplit_ByPattern) == 0 ? 1
                                   : delim.size());
                if (str.size() > dsz  &&  str[str.size() - dsz - 1] == '\\') {
                    found_delim = false; // actually escaped
                }
            }
        } else {
            found_delim = true;
        }
    } else {
        found_delim = true;
        str1.clear();
    }

    // don't need further splitting, just quote and escape parsing
    splitter.SetDelim(kEmptyStr);
    splitter.Advance(&part_collector);
    part_collector.Join(&str2);

    return found_delim;
}


template <typename T>
string s_NStr_Join(const T& arr, const CTempString& delim)
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


string NStr::Join(const list<string>& arr, const CTempString& delim)
{
    return s_NStr_Join(arr, delim);
}


string NStr::Join(const list<CTempString>& arr, const CTempString& delim)
{
    return s_NStr_Join(arr, delim);
}


string NStr::Join(const vector<string>& arr, const CTempString& delim)
{
    return s_NStr_Join(arr, delim);
}


string NStr::Join(const vector<CTempString>& arr, const CTempString& delim)
{
    return s_NStr_Join(arr, delim);
}


enum ELanguage {
    eLanguage_C,
    eLanguage_Javascript
};


static string s_PrintableString(const CTempString&   str,
                                NStr::TPrintableMode mode,
                                ELanguage            lang)
{
    auto_ptr<CNcbiOstrstream> out;
    SIZE_TYPE i, j = 0;

    for (i = 0;  i < str.size();  i++) {
        bool octal = false;
        char c = str[i];
        switch (c) {
        case '\t':
            c = 't';
            break;
        case '\v':
            c = 'v';
            break;
        case '\b':
            c = 'b';
            break;
        case '\r':
            c = 'r';
            break;
        case '\f':
            c = 'f';
            break;
        case '\a':
            c = 'a';
            break;
        case '\n':
            if (!(mode & NStr::fNewLine_Passthru))
                c = 'n';
            /*FALLTHRU*/
        case '\\':
        case '\'':
        case '"':
            break;
        case '&':
            if (lang != eLanguage_Javascript)
                continue;
            break;
        default:
            if (!isascii((unsigned char) c)) {
                if (mode & NStr::fNonAscii_Quote) {
                    octal = true;
                    break;
                }
            }
            if (!isprint((unsigned char) c)) {
                octal = true;
                break;
            }
            continue;
        }
        if (!out.get()) {
            out.reset(new CNcbiOstrstream);
        }
        if (i > j) {
            out->write(str.data() + j, i - j);
        }
        out->put('\\');
        if (c == '\n') {
            out->write("n\\\n", 3);
        } else if (octal) {
            bool reduce;
            if (!(mode & NStr::fPrintable_Full)) {
                reduce = (i == str.size() - 1  ||
                          str[i + 1] < '0' || str[i + 1] > '7' ? true : false);
            } else {
                reduce = false;
            }
            unsigned char v;
            char val[3];
            int k = 0;
            v =  (unsigned char) c >> 6;
            if (v  ||  !reduce) {
                val[k++] = '0' + v;
                reduce = false;
            }
            v = ((unsigned char) c >> 3) & 7;
            if (v  ||  !reduce) {
                val[k++] = '0' + v;
            }
            v =  (unsigned char) c       & 7;
            val    [k++] = '0' + v;
            out->write(val, k);
        } else {
            out->put(c);
        }
        j = i + 1;
    }
    if (j  &&  i > j) {
        _ASSERT(out.get());
        out->write(str.data() + j, i - j);
    }
    if (out.get()) {
        // Return encoded string
        return CNcbiOstrstreamToString(*out);
    }

    // All characters are good - return (a copy of) the original string
    return str;
}

        
string NStr::PrintableString(const CTempString&   str,
                             NStr::TPrintableMode mode)
{
    return s_PrintableString(str, mode, eLanguage_C);
}


string NStr::JavaScriptEncode(const CTempString& str)
{
    return s_PrintableString(str,
                             fNewLine_Quote | fNonAscii_Passthru,
                             eLanguage_Javascript);
}


string NStr::CEncode(const CTempString& str, EQuoted quoted)
{
    switch (quoted) {
    case eNotQuoted:
        return PrintableString(str);
    case eQuoted:
        return '"' + PrintableString(str) + '"';
    }
    _TROUBLE;
    // Unreachable
    return str;
}


string NStr::CParse(const CTempString& str, EQuoted quoted)
{
    if (quoted == eNotQuoted) {
        return ParseEscapes(str);
    }
    _ASSERT(quoted == eQuoted);

    SIZE_TYPE pos;
    SIZE_TYPE len = str.length();
    const char quote_char = '"';

    if (len < 2 || str[0] != quote_char  || str[len-1] != quote_char) {
        NCBI_THROW2(CStringException, eFormat,
            "The source string must start and finish with a double quote", 0);
    }

    // Flag that next char is escaped, ignore it
    bool escaped = false; 
    // We have a quote mark, start collect string chars
    bool collect = true;
    // Position of last quote
    SIZE_TYPE last_quote = 0;

    string out;
    out.reserve(str.size());

    for (pos = 1; pos < len; ++pos) {
        unsigned char ch = str[pos];
        if (ch == quote_char  &&  !escaped) {
            // Have a substring
            CTempString sub(str.data() + last_quote + 1, pos - last_quote - 1);
            if (collect) {
                // Parse escape sequences and add it to result
                out += ParseEscapes(sub);
            } else {
                // Possible we have adjacent strings ("A""B").
                if (pos != last_quote + 1) {
                    NCBI_THROW2(CStringException, eFormat,
                        "Quoted string format error", pos);
                }
            }
            last_quote = pos;
            collect = !collect;
        } else {
            escaped = ch == '\\' ? !escaped : false;
        }
    }
    if (escaped || last_quote != len-1) {
        NCBI_THROW2(CStringException, eFormat,
            "Unterminated quoted string", str.length());
    }
    return out;
}


string NStr::XmlEncode(const CTempString& str, EXmlEncode flags)
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-predefined-ent
{
    string result;
    SIZE_TYPE i;
    for (i = 0;  i < str.size();  i++) {
        char c = str[i];
        switch ( c ) {
        case '&':
            result.append("&amp;");
            break;
        case '<':
            result.append("&lt;");
            break;
        case '>':
            result.append("&gt;");
            break;
        case '\'':
            result.append("&apos;");
            break;
        case '"':
            result.append("&quot;");
            break;
        case '-':
            if (flags == eXmlEnc_CommentSafe) {
// translate double hyphen and ending hyphen
// http://www.w3.org/TR/xml11/#sec-comments
                if (i+1 == str.size()) {
                    result.append("&#x2d;");
                    break;
                } else if (str[i+1] == '-') {
                    ++i;
                    result.append(1, c).append("&#x2d;");
                    break;
                }
            }
            result.append(1, c);
            break;
        default:
            if ((unsigned int)(c) < 0x20) {
                const char* charmap = "0123456789abcdef";
                result.append("&#x");
                Uint1 ch = c;
                unsigned hi = ch >> 4;
                unsigned lo = ch & 0xF;
                if ( hi ) {
                    result.append(1, charmap[hi]);
                }
                result.append(1, charmap[lo]).append(1, ';');
            } else {
                result.append(1, c);
            }
            break;
        }
    }
    return result;
}

string NStr::JsonEncode(const CTempString& str)
// http://www.json.org/
{
    string result;
    SIZE_TYPE i;
    for (i = 0;  i < str.size();  i++) {
        char c = str[i];
        switch ( c ) {
        case '"':
            result.append("\\\"");
            break;
        case '\\':
            result.append("\\\\");
            break;
        default:
            if ((unsigned int)c < 0x20 || (unsigned int)c >= 0x80) {
                const char* charmap = "0123456789abcdef";
                result.append("\\u00");
                Uint1 ch = c;
                unsigned hi = ch >> 4;
                unsigned lo = ch & 0xF;
                result.append(1, charmap[hi]);
                result.append(1, charmap[lo]);
            } else {
                result.append(1, c);
            }
            break;
        }
    }
    return result;
}


string NStr::ShellEncode(const string& str)
{
    // 1. Special-case of non-printable characters. We have no choice and
    //    must use BASH extensions if we want printable output. 
    //
    // Aesthetic issue: Most people are not familiar with the BASH-only
    //     quoting style. Avoid it as much as possible.

    if (find_if(str.begin(),str.end(),not1(ptr_fun(::isprint))) != str.end()) {
        return "$'" + NStr::PrintableString(str) + "'";
    }

    /////////////////////////////////////////////////////////////////////////
    // Bourne Shell quoting as IEEE-standard without special extensions.
    //
    // There are 3 basic ways to quote/escape in Bourne Shell:
    //
    // - Single-quotes. All characters (including non-printable
    //   characters newlines, backslashes), are literal. There is no escape.
    // - Double-quotes. Need to escape some metacharacters, such as literal
    //   escape (\), variable expansion ($) and command substitution (`).
    // - Escape without quotes. Use backslash.
    /////////////////////////////////////////////////////////////////////////

    // 2. Non-empty printable string without metacharacters.
    //
    // Shell special characters, according to IEEE Std 1003.1,
    // plus ! (Bourne shell exit status negation and Bash history expansion),
    // braces (Bourne enhanced expansion), space, tab, and newline.
    //
    // See http://www.opengroup.org/onlinepubs/009695399/toc.htm
    // See Bourne and Bash man pages.

    if (!str.empty()  &&
        str.find_first_of("!{} \t\r\n[|&;<>()$`\"'*?#~=%\\") == NPOS) {
        return str;
    }

    // 3. Printable string, but either empty or some shell metacharacters.
    //
    // Aesthetics preference:
    // i)   If the string includes literal single-quotes, then prefer
    //      double-quoting provided there is no need to escape embedded
    //      literal double-quotes, escapes (\), variable substitution ($),
    //      or command substitution (`).

    if (str.find('\'') != NPOS  &&
        str.find_first_of("\"\\$`") == NPOS) {
        return "\"" + str + "\"";
    }

    // Use single-quoting. The only special case for Bourne shell
    // single-quoting is a literal single-quote, which needs to
    // be pulled out of the quoted region.
    //
    // Single-quoting does not have any escape character, so close
    // the quoted string ('), then emit an escaped or quoted literal
    // single-quote (\' or "'"), and resume the quoted string (').
    //
    // Aesthetics preferences:
    // ii)  Prefer single-quoting over escape characters, especially
    //      escaped whitespace. However, this is in compromise to optimal
    //      quoting: if there are many literal single-quotes and the
    //      use of double-quotes would involve the need to escape embedded
    //      characters, then it may be more pleasing to escape the
    //      shell metacharacters, and avoid the need for single-quoting
    //      in the presence of literal single-quotes.
    // iii) If there are no literal double-quotes, then all else being equal,
    //      avoid double-quotes and prefer escaping. Double-quotes are
    //      more commonly used by enclosing formats such as ASN.1 Text
    //      and CVS, and would thus need to be escaped. If there are
    //      literal double-quotes, then having them is in the output is
    //      unavoidable, and this aesthetics rule becomes secondary to
    //      the preference for avoiding escape characters. If there are
    //      literal escape characters, then having them is unavoidable
    //      and avoidance of double-quotes is once again recommended.

    // TODO: Should simplify runs of multiple quotes, for example:
    //       '\'''\'''\'' -> '"'''"'

    bool avoid_double_quotes = (str.find('"') == NPOS  ||
                                str.find('\\') != NPOS);
    string s = "'" + NStr::Replace(str, "'",
            avoid_double_quotes ? "'\\''" : "'\"'\"'") + "'";

    // Aesthetic improvement: Remove paired single-quotes ('')
    // that aren't escaped, as these evaluate to an empty string.
    // Don't apply this simplification for the degenerate case when
    // the string is the empty string ''. (Nondegenerate strings
    // must be length greater than 2). Implement the equivalent
    // of the Perl regexp:
    //
    //     s/(?<!\\)''//g
    //
    if (s.size() > 2) {
        size_t pos = 0;
        while ( true ) {
            pos = s.find("''", pos);
            if (pos == NPOS) break;
            if (pos == 0 || s[pos-1] != '\\') {
                s.erase(pos, 2);
            } else {
                ++pos;
            }
        }
    }

    return s;
}


string NStr::ParseEscapes(const CTempString& str, EEscSeqRange mode, char user_char)
{
    string out;
    out.reserve(str.size());  // result string can only be smaller
    SIZE_TYPE pos = 0;
    bool is_error = false;

    while (pos < str.size()  ||  !is_error) {
        SIZE_TYPE pos2 = str.find('\\', pos);
        if (pos2 == NPOS) {
            //~ out += str.substr(pos);
            CTempString sub(str, pos);
            out += sub;
            break;
        }
        //~ out += str.substr(pos, pos2 - pos);
        CTempString sub(str, pos, pos2-pos);
        out += sub;
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
            {{
                pos = ++pos2;
                while (pos < str.size()
                       &&  isxdigit((unsigned char) str[pos])) {
                    pos++;
                }
                if (pos > pos2) {
                    SIZE_TYPE len = pos-pos2;
                    if ((mode == eEscSeqRange_FirstByte) && (len > 2)) {
                        // Take only 2 first hex-digits
                        len = 2;
                        pos = pos2 + 2;
                    }
                    unsigned int value =
                        StringToUInt(CTempString(str, pos2, len), 0, 16);
                    if ((mode != eEscSeqRange_Standard)  &&  (value > 255)) {
                        // eEscSeqRange_Standard -- by default
                        switch (mode) {
                        case eEscSeqRange_FirstByte:
                            // Already have right value 
                            break;
                        case eEscSeqRange_Throw:
                            NCBI_THROW2(CStringException, eFormat, 
                                "Escape sequence '" + string(CTempString(str, pos2, len)) +
                                "' is out of range [0-255]", pos2);
                            break;
                        case eEscSeqRange_Errno:
                            CNcbiError::SetErrno(errno = ERANGE,str);
                            is_error = true;
                            continue;
                        case eEscSeqRange_User:
                            value = (unsigned)user_char;
                            break;
                        default:
                            NCBI_THROW2(CStringException, eFormat, "Wrong set of flags", pos2);
                        }
                    }
                    out += static_cast<char>(value);
                } else {
                    NCBI_THROW2(CStringException, eFormat,
                                "\\x followed by no hexadecimal digits", pos);
                }
            }}
            continue;
        case '0':  case '1':  case '2':  case '3':
        case '4':  case '5':  case '6':  case '7':
            {{
                pos = pos2;
                unsigned char c = str[pos++] - '0';
                while (pos < pos2 + 3  &&  pos < str.size()
                       &&  str[pos] >= '0'  &&  str[pos] <= '7') {
                    c = (c << 3) | (str[pos++] - '0');
                }
                out += c;
            }}
            continue;
        case '\n':
            // quoted EOL means no EOL
            break;
        default:
            out += str[pos2];
            break;
        }
        pos = pos2 + 1;
    }
    if (mode == eEscSeqRange_Errno) {
        if (is_error) {
            return kEmptyStr;
        }
        errno = 0;
    }
    return out;
}


string NStr::ParseQuoted(const CTempString& str, size_t* n_read /*= NULL*/)
{
    const char* str_pos = str.data();
    char quote_char;

    if (str.empty() || ((quote_char = *str_pos) != '"' && quote_char != '\'')) {
        NCBI_THROW2(CStringException, eFormat,
            "The source string must start with a quote", 0);
    }

    const char* str_end = str_pos + str.length();
    bool escaped = false;

    while (++str_pos < str_end) {
        if (*str_pos == quote_char && !escaped) {
            size_t pos = str_pos - str.data();
            if (n_read != NULL)
                *n_read = pos + 1;
            return ParseEscapes(CTempString(str.data() + 1, pos - 1));
        } else {
            escaped = *str_pos == '\\' ? !escaped : false;
        }
    }
    NCBI_THROW2(CStringException, eFormat,
        "Unterminated quoted string", str.length());
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
// (which might not actually end with a semicolon  :-/ , but we ignore that case)
static SIZE_TYPE s_EndOfReference(const string& str, SIZE_TYPE start)
{
    _ASSERT(start < str.size()  &&  str[start] == '&');

    SIZE_TYPE pos = str.find_first_not_of
        ("#0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
         start + 1);
    if (pos != NPOS  &&  str[pos] == ';') {
        // found terminating semicolon, so it's valid, and we return that
        return pos;
    } else {
        // We consider it just a '&' by itself since it's invalid
        return start;
    }
}


static SIZE_TYPE s_VisibleHtmlWidth(const string& str)
{
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
}

list<string>& NStr::Wrap(const string& str, SIZE_TYPE width,
                         list<string>& arr, NStr::TWrapFlags flags,
                         const string* prefix, const string* prefix1)
{
    if (prefix == 0) {
        prefix = &kEmptyStr ;
    }

    const string* pfx = prefix1 ? prefix1 : prefix;
    SIZE_TYPE     pos = 0, len = str.size(), nl_pos = 0;
    
    const bool          is_html  = flags & fWrap_HTMLPre ? true : false;
    const bool          do_flat = (flags & fWrap_FlatFile) != 0;

    enum EScore { // worst to best
        eForced,
        ePunct,
        eComma,
        eSpace,
        eNewline
    };

    // To avoid copying parts of str when we need to store a 
    // substr of str, we store the substr as a pair
    // representing start (inclusive) and end (exclusive).
    typedef pair<SIZE_TYPE, SIZE_TYPE> TWrapSubstr;

    // This variable is used for HTML links that cross line boundaries.
    // Since it's aesthetically displeasing for a link to cross a boundary, we 
    // close it at the end of each line and re-open it after the next line's 
    // prefix
    // (This is needed in, e.g. AE017351)
    TWrapSubstr best_link(0, 0); // last link found before current best_pos
    TWrapSubstr latest_link(0, 0); // last link found at all

    while (pos < len) {
        bool      hyphen     = false; // "-" or empty
        SIZE_TYPE column     = is_html? s_VisibleHtmlWidth(*pfx) : pfx->size();
        SIZE_TYPE column0    = column;
        // the next line will start at best_pos
        SIZE_TYPE best_pos   = NPOS;
        EScore    best_score = eForced;

        // certain logic can be skipped if this part has no backspace,
        // which is, by far, the most common case
        bool thisPartHasBackspace = false;

        arr.push_back("");
        arr.back().reserve( width );
        arr.back() = *pfx;

        // append any still-open links from previous lines
        if( is_html && best_link.second != 0 ) {
            arr.back().append( 
                str.begin() + best_link.first,
                str.begin() + best_link.second );
        }

        SIZE_TYPE pos0 = pos;

        // we can't do this in HTML mode because we might have to deal with
        // link tags that go across lines.
        if( ! is_html ) {
            if (nl_pos <= pos) {
                nl_pos = str.find('\n', pos);
                if (nl_pos == NPOS) {
                    nl_pos = len;
                }
            }
            if (column + (nl_pos-pos) <= width ) {
                pos0 = nl_pos;
            }
        }

        for (SIZE_TYPE pos2 = pos0;  pos2 < len  &&  column <= width;
             ++pos2, ++column) {
            EScore    score     = eForced;
            SIZE_TYPE score_pos = pos2;
            const char      c         = str[pos2];

            if (c == '\n') {
                best_pos   = pos2;
                best_score = eNewline;
                best_link = latest_link;
                break;
            } else if (isspace((unsigned char) c)) {
                if ( !do_flat  &&  pos2 > 0  &&
                     isspace((unsigned char) str[pos2 - 1])) {
                    if(pos2 < len - 1  &&  str[pos2 + 1] == '\b') {
                        thisPartHasBackspace = true;
                    }
                    continue; // take the first space of a group
                }
                score = eSpace;
            } else if (is_html && c == '<') {
                // treat tags as zero-width...
                SIZE_TYPE start_of_tag = pos2;
                pos2 = s_EndOfTag(str, pos2);
                --column;
                if (pos2 == NPOS) {
                    break;
                }

                if( (pos2 - start_of_tag) >= 6 &&
                    str[start_of_tag+1] == 'a' && 
                    str[start_of_tag+2] == ' ' && 
                    str[start_of_tag+3] == 'h' &&
                    str[start_of_tag+4] == 'r' &&
                    str[start_of_tag+5] == 'e' &&
                    str[start_of_tag+6] == 'f' )
                {
                    // remember current link in case of line wrap
                    latest_link.first  = start_of_tag;
                    latest_link.second = pos2 + 1;
                }
                if( (pos2 - start_of_tag) >= 3 &&
                    str[start_of_tag+1] == '/' && 
                    str[start_of_tag+2] == 'a' && 
                    str[start_of_tag+3] == '>') 
                {
                    // link is closed
                    latest_link.first  = 0;
                    latest_link.second = 0;
                }
            } else if (is_html && c == '&') {
                // ...and references as single characters
                pos2 = s_EndOfReference(str, pos2);
                if (pos2 == NPOS) {
                    break;
                }
            } else if ( c == ','  && column < width && score_pos < len - 1 ) {
                score = eComma;
                ++score_pos;
            } else if (do_flat ? c == '-' : ispunct((unsigned char) c)) {
                // For flat files, only whitespace, hyphens and commas
                // are special.
                switch(c) {
                    case '(': case '[': case '{': case '<': case '`':
                        score = ePunct;
                        break;
                    default:
                        if( score_pos < len - 1  &&  column < width ) {
                            score = ePunct;
                            ++score_pos;
                        }
                        break;
                }
            }

            if (score >= best_score  &&  score_pos > pos0) {
                best_pos   = score_pos;
                best_score = score;
                best_link = latest_link;
            }

            while (pos2 < len - 1  &&  str[pos2 + 1] == '\b') {
                // Account for backspaces
                ++pos2;
                if (column > column0) {
                    --column;
                }
                thisPartHasBackspace = true;
            }
        }

        if ( best_score != eNewline  &&  column <= width ) {
            if( best_pos != len ) {
                // If the whole remaining text can fit, don't split it...
                best_pos = len;
                best_link = latest_link;
                // Force backspace checking, to play it safe
                thisPartHasBackspace = true;
            }
        } else if ( best_score == eForced  &&  (flags & fWrap_Hyphenate) ) {
            hyphen = true;
            --best_pos;
        }

        {{ 
            string::const_iterator begin = str.begin() + pos;
            string::const_iterator end = str.begin() + best_pos;
            if( thisPartHasBackspace ) {
                // eat backspaces and the characters (if any) that precede them

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
            }
            if (begin != end) {
                // add remaining characters
                arr.back().append(begin, end);
            }
        }}

        // if we didn't close the link on this line, we 
        // close it here
        if( is_html && best_link.second != 0 ) {
            arr.back() += "</a>";
        }

        if ( hyphen ) {
            arr.back() += '-';
        }
        pos = best_pos;
        pfx = prefix;

        if (do_flat) {
            if (best_score == eSpace) {
                while (str[pos] == ' ') {
                    ++pos;
                }
                if (str[pos] == '\n') {
                    ++pos;
                }
            }
            if (best_score == eNewline) {
                ++pos;
            }
        }
        else {
            if ( best_score == eSpace  ||  best_score == eNewline ) {
                ++pos;
            }
        }
        while (pos < len  &&  str[pos] == '\b') {
            ++pos;
        }
    }

    return arr;
}


list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags,
                             const string* prefix,
                             const string* prefix1)
{
    if (l.empty()) {
        return arr;
    }

    const string* pfx      = prefix1 ? prefix1 : prefix;
    string        s        = *pfx;
    bool          is_html  = flags & fWrap_HTMLPre ? true : false;
    SIZE_TYPE     column   = is_html? s_VisibleHtmlWidth(s)     : s.size();
    SIZE_TYPE     delwidth = is_html? s_VisibleHtmlWidth(delim) : delim.size();
    bool          at_start = true;

    ITERATE (list<string>, it, l) {
        SIZE_TYPE term_width = is_html ? s_VisibleHtmlWidth(*it) : it->size();
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
                column   = is_html ? s_VisibleHtmlWidth(s) : s.size();
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
            column   = is_html ? s_VisibleHtmlWidth(s) : s.size();
            at_start = true;
            --it;
        }
    }
    arr.push_back(s);
    return arr;
}


list<string>& NStr::Justify(const CTempString& str,
                            SIZE_TYPE          width,
                            list<string>&      par,
                            const CTempString* pfx,
                            const CTempString* pfx1)
{
    static const CTempString kNothing;
    if (!pfx)
        pfx = &kNothing;
    const CTempString* p = pfx1 ? pfx1 : pfx;

    SIZE_TYPE pos = 0;
    for (SIZE_TYPE len = p->size();  pos < str.size();  len = p->size()) {
        list<CTempString> words;
        unsigned int nw = 0;  // How many words are there in the line
        bool big = false;
        do {
            while (pos < str.size()) {
                if (!isspace((unsigned char) str[pos]))
                    break;
                ++pos;
            }
            SIZE_TYPE start = pos;
            while (pos < str.size()) {
                if ( isspace((unsigned char) str[pos]))
                    break;
                ++pos;
            }
            SIZE_TYPE wlen = pos - start;
            if (!wlen)
                break;
            if (len + wlen + nw > width) {
                if (nw) {
                    pos = start; // Will have to rescan this word again
                    break;
                }
                big = true;  // Long line with a long lonely word :-/
            }
            words.push_back(CTempString(str, start, wlen));
            len += wlen;
            ++nw;
            if (str[pos - 1] == '.'  ||
                str[pos - 1] == '!'  ||
                str[pos - 1] == '?') {
                if (len + 1 >= width)
                    break;
                words.push_back(CTempString("", 0));
                _ASSERT(!big);
                nw++;
            }
        } while (!big);
        if (!nw)
            break;
        if (words.back().empty()) {
            words.pop_back();
            _ASSERT(nw > 1);
            nw--;
        }
        SIZE_TYPE space;
        if (nw > 1) {
            if (pos < str.size()  &&  len < width  &&  !big) {
                space = (width - len) / (nw - 1);
                nw    = (width - len) % (nw - 1);
            } else {
                space = 1;
                nw    = 0;
            }
        } else
            space = 0;
        par.push_back(*p);
        unsigned int n = 0;
        ITERATE(list<CTempString>, w, words) {
            if (n)
                par.back().append(space + (n <= nw ? 1 : 0) , ' ');
            par.back().append(w->data(), w->size());
            ++n;
        }
        p = pfx;
    }
    return par;
}


#if !defined(HAVE_STRDUP)
extern char* strdup(const char* str)
{
    if ( !str ) {
        return 0;
    }
    size_t size   = strlen(str) + 1;
    void*  result = malloc(size);
    return (char*)(result ? memcpy(result, str, size) : 0);
}
#endif


static const char s_Encode[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodeMarkChars[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "%5F",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodePercentOnly[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "%5F",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodePath[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "%2B", "%2C", "%2D", ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

// RFC-2396:
// scheme        = alpha *( alpha | digit | "+" | "-" | "." )
static const char s_EncodeURIScheme[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F", 
    "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "+",   "%2C", "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "%5F",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

// RFC-2396:
// userinfo      = *( unreserved | escaped |
//                   ";" | ":" | "&" | "=" | "+" | "$" | "," )
// unreserved    = alphanum | mark
// mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
// Note: ":" is name/password separator, so it must be encoded in each of them.
static const char s_EncodeURIUserinfo[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "&",   "'",
    "(",   ")",   "*",   "+",   ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", ";",   "%3C", "=",   "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

// RFC-2396:
// host          = hostname | IPv4address
// hostname      = *( domainlabel "." ) toplabel [ "." ]
// domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
// toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
// IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
static const char s_EncodeURIHost[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "%5F",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

// RFC-2396:
// path_segments = segment *( "/" segment )
// segment       = *pchar *( ";" param )
// param         = *pchar
// pchar         = unreserved | escaped |
//                 ":" | "@" | "&" | "=" | "+" | "$" | ","
// unreserved    = alphanum | mark
// mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
static const char s_EncodeURIPath[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "&",   "'",
    "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   ";",   "%3C", "=",   "%3E", "%3F",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodeURIQueryName[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "%2A", "%2B", "%2C", "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   "%3B", "%3C", "%3D", "%3E", "?",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodeURIQueryValue[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "%2A", "%2B", "%2C", "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   "%3B", "%3C", "%3D", "%3E", "?",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

// RFC-2396:
// fragment      = *uric
// uric          = reserved | unreserved | escaped
// reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
// unreserved    = alphanum | mark
// mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
static const char s_EncodeURIFragment[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "&",   "'",
    "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   ";",   "%3C", "=",   "%3E", "?",
    "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "~",   "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const char s_EncodeCookie[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", "%2C", "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

string NStr::URLEncode(const CTempString& str, EUrlEncode flag)
{
    SIZE_TYPE len = str.length();
    if ( !len ) {
        return kEmptyStr;
    }
    const char (*encode_table)[4];
    switch (flag) {
    case eUrlEnc_SkipMarkChars:
        encode_table = s_Encode;
        break;
    case eUrlEnc_ProcessMarkChars:
        encode_table = s_EncodeMarkChars;
        break;
    case eUrlEnc_PercentOnly:
        encode_table = s_EncodePercentOnly;
        break;
    case eUrlEnc_Path:
        encode_table = s_EncodePath;
        break;
    case eUrlEnc_URIScheme:
        encode_table = s_EncodeURIScheme;
        break;
    case eUrlEnc_URIUserinfo:
        encode_table = s_EncodeURIUserinfo;
        break;
    case eUrlEnc_URIHost:
        encode_table = s_EncodeURIHost;
        break;
    case eUrlEnc_URIPath:
        encode_table = s_EncodeURIPath;
        break;
    case eUrlEnc_URIQueryName:
        encode_table = s_EncodeURIQueryName;
        break;
    case eUrlEnc_URIQueryValue:
        encode_table = s_EncodeURIQueryValue;
        break;
    case eUrlEnc_URIFragment:
        encode_table = s_EncodeURIFragment;
        break;
    case eUrlEnc_Cookie:
        encode_table = s_EncodeCookie;
        break;
    case eUrlEnc_None:
        return str;
    default:
        _TROUBLE;
        // To keep off compiler warning
        encode_table = 0;
    }

    string dst;
    SIZE_TYPE pos;
    SIZE_TYPE dst_len = len;
    const unsigned char* cstr = (const unsigned char*)str.data();
    for (pos = 0;  pos < len;  pos++) {
        if (encode_table[cstr[pos]][0] == '%')
            dst_len += 2;
    }
    dst.resize(dst_len);

    SIZE_TYPE p = 0;
    for (pos = 0;  pos < len;  pos++, p++) {
        const char* subst = encode_table[cstr[pos]];
        if (*subst != '%') {
            dst[p] = *subst;
        } else {
            dst[p] = '%';
            dst[++p] = *(++subst);
            dst[++p] = *(++subst);
        }
    }
    _ASSERT( p == dst_len );
    return dst;
}


CStringUTF8 NStr::SQLEncode(const CStringUTF8& str) {
    SIZE_TYPE     stringSize = str.size();
    CStringUTF8   result;

    result.reserve(stringSize + 6);
    result.append(1, '\'');
    for (SIZE_TYPE i = 0;  i < stringSize;  i++) {
        char  c = str[i];
        if (c ==  '\'')
            result.append(1, '\'');
        result.append(1, c);
    }
    result.append(1, '\'');

    return result;
}


static
void s_URLDecode(const CTempString& src, string& dst, NStr::EUrlDecode flag)
{
    SIZE_TYPE len = src.length();
    if ( !len ) {
        dst.erase();
        return;
    }
    if (dst.length() < src.length()) {
        dst.resize(len);
    }

    SIZE_TYPE pdst = 0;
    for (SIZE_TYPE psrc = 0;  psrc < len;  pdst++) {
        switch ( src[psrc] ) {
        case '%': {
            // Accordingly RFC 1738 the '%' character is unsafe
            // and should be always encoded, but sometimes it is
            // not really encoded...
            if (psrc + 2 > len) {
                dst[pdst] = src[psrc++];
            } else {
                int n1 = NStr::HexChar(src[psrc+1]);
                int n2 = NStr::HexChar(src[psrc+2]);
                if (n1 < 0  ||  n1 > 15  || n2 < 0  ||  n2 > 15) {
                    dst[pdst] = src[psrc++];
                } else {
                    dst[pdst] = (n1 << 4) | n2;
                    psrc += 3;
                }
            }
            break;
        }
        case '+': {
            dst[pdst] = (flag == NStr::eUrlDec_All) ? ' ' : '+';
            psrc++;
            break;
        }
        default:
            dst[pdst] = src[psrc++];
        }
    }
    if (pdst < len) {
        dst.resize(pdst);
    }
}


string NStr::URLDecode(const CTempString& str, EUrlDecode flag)
{
    string dst;
    s_URLDecode(str, dst, flag);
    return dst;
}


void NStr::URLDecodeInPlace(string& str, EUrlDecode flag)
{
    s_URLDecode(str, str, flag);
}


bool NStr::NeedsURLEncoding(const CTempString& str, EUrlEncode flag)
{
    SIZE_TYPE len = str.length();
    if ( !len ) {
        return false;
    }
    const char (*encode_table)[4];
    switch (flag) {
    case eUrlEnc_SkipMarkChars:
        encode_table = s_Encode;
        break;
    case eUrlEnc_ProcessMarkChars:
        encode_table = s_EncodeMarkChars;
        break;
    case eUrlEnc_PercentOnly:
        encode_table = s_EncodePercentOnly;
        break;
    case eUrlEnc_Path:
        encode_table = s_EncodePath;
        break;
    case eUrlEnc_Cookie:
        encode_table = s_EncodeCookie;
        break;
    case eUrlEnc_None:
        return false;
    default:
        _TROUBLE;
        // To keep off compiler warning
        encode_table = 0;
    }
    const unsigned char* cstr = (const unsigned char*)str.data();

    for (SIZE_TYPE pos = 0;  pos < len;  pos++) {
        const char* subst = encode_table[cstr[pos]];
        if (*subst != cstr[pos]) {
            return true;
        }
    }
    return false;
}


/// @internal
static
bool s_IsIPAddress(const char* str, size_t size)
{
    _ASSERT(str[size] == '\0');

    const char* c = str;

    // IPv6?
    if ( strchr(str, ':') ) {
        if (NStr::CompareNocase(str, 0, 7, "::ffff:") == 0) {
            // Mapped IPv4 address
            return size > 7  &&  s_IsIPAddress(str + 7, size - 7);
        }

        int colons = 0;
        bool have_group = false;
        const char* prev_colon = NULL;
        int digits = 0;
        // Continue until
        for (; c  &&  c - str < (int)size  &&  *c != '%'; c++) {
            if (*c == ':') {
                colons++;
                if (colons > 7) {
                    // Too many separators
                    return false;
                }
                if (prev_colon  &&  c - prev_colon  == 1) {
                    // A group of zeroes found
                    if (have_group) {
                        // Only one group is allowed
                        return false;
                    }
                    have_group = true;
                }
                prev_colon = c;
                digits = 0;
                continue;
            }
            digits++;
            if (digits > 4) {
                // Too many digits between colons
                return false;
            }
            char d = toupper(*c);
            if (d < '0'  ||  d > 'F') {
                // Invalid digit
                return false;
            }
        }
        // Check if zone index is present
        if (*c == '%') {
            // It's not clear yet what zone index may look like.
            // Ignore it.
        }
        // Make sure there was at least one colon.
        return colons > 1;
    }

    unsigned long val;
    int dots = 0;

    int& errno_ref = errno;
    for (;;) {
        char* e;
        if ( !isdigit((unsigned char)(*c)) )
            return false;
        errno_ref = 0;
        val = strtoul(c, &e, 10);
        if (c == e  ||  errno_ref)
            return false;
        c = e;
        if (*c != '.')
            break;
        if (++dots > 3)
            return false;
        if (val > 255)
            return false;
        c++;
    }

    // Make sure the whole string was checked (it is possible to have \0 chars
    // in the middle of the string).
    if ((size_t)(c - str) != size) {
        return false;
    }
    return !*c  &&  dots == 3  &&  val < 256;
}


bool NStr::IsIPAddress(const CTempStringEx& str)
{
    size_t size = str.size();
    if ( str.HasZeroAtEnd() ) {
        // string has zero at the end already
        return s_IsIPAddress(str.data(), size);
    }
    char buf[256]; // small temporary buffer on stack for appending zero char
    if ( size < sizeof(buf) ) {
        memcpy(buf, str.data(), size);
        buf[size] = '\0';
        return s_IsIPAddress(buf, size);
    }
    else {
        // use std::string() to allocate memory for appending zero char
        return s_IsIPAddress(string(str).c_str(), size);
    }
}


namespace {
    // Comparator to decide if a symbol is a delimiter
    template <typename TDelimiter>
    class PDelimiter
    {
    private:
        const TDelimiter& delimiter;

    public:
        PDelimiter(const TDelimiter& delim)
            : delimiter(delim)
        {}

        bool operator()(char tested_symbol) const;
    };


    // Template search for a field
    // @param str
    //   C or C++ string to search in.
    // @param field_no
    //   Zero-based field number.
    // @param delimiter
    //   Functor to decide if a symbol is a delimiter
    // @param merge
    //   Whether to merge or not adjacent delimiters.
    // @return
    //   Found field; or empty string if the required field is not found.
    template <typename TComparator, typename TResult>
    TResult s_GetField(const CTempString& str,
                       size_t             field_no,
                       const TComparator& delimiter,
                       NStr::EMergeDelims merge)
    {
        const char*   current_ptr   = str.data();
        const char*   end_ptr       = current_ptr + str.length();
        size_t        current_field = 0;

        // Search for the beginning of the required field
        for ( ;  current_field != field_no;  current_field++) {
            while (current_ptr < end_ptr  &&  !delimiter(*current_ptr))
                current_ptr++;

            if (merge == NStr::eMergeDelims) {
                while (current_ptr < end_ptr  &&  delimiter(*current_ptr))
                    current_ptr++;
            }
            else
                current_ptr++;

            if (current_ptr >= end_ptr)
                return TResult();
        }

        if (current_field != field_no)
            return TResult();

        // Here: current_ptr points to the first character after the delimiter.
        const char* field_start = current_ptr;
        while (current_ptr < end_ptr  &&  !delimiter(*current_ptr))
            current_ptr++;

        return TResult(field_start, current_ptr - field_start);
    }



    template <>
    bool PDelimiter<char>::operator() (char c) const
    {
        return delimiter == c;
    }

    template <>
    bool PDelimiter<CTempString>::operator() (char c) const
    {
        return delimiter.find(c) != NPOS;
    }
}


string NStr::GetField(const CTempString& str,
                      size_t             field_no,
                      const CTempString& delimiters,
                      EMergeDelims       merge)
{
    return s_GetField<PDelimiter<CTempString>, string>
        (str,
         field_no,
         PDelimiter<CTempString>(delimiters),
         merge);
}


string NStr::GetField(const CTempString& str,
                      size_t             field_no,
                      char               delimiter,
                      EMergeDelims       merge)
{
    return s_GetField<PDelimiter<char>, string>
        (str,
         field_no,
         PDelimiter<char>(delimiter),
         merge);
}


CTempString NStr::GetField_Unsafe(const CTempString& str,
                                  size_t             field_no,
                                  const CTempString& delimiters,
                                  EMergeDelims       merge)
{
    return s_GetField<PDelimiter<CTempString>, CTempString>
        (str,
         field_no,
         PDelimiter<CTempString>(delimiters),
         merge);
}


CTempString NStr::GetField_Unsafe(const CTempString& str,
                                  size_t             field_no,
                                  char               delimiter,
                                  EMergeDelims       merge)
{
    return s_GetField<PDelimiter<char>, CTempString>
        (str,
         field_no,
         PDelimiter<char>(delimiter),
         merge);
}



/////////////////////////////////////////////////////////////////////////////
//  CStringUTF8 / CUtf8

#if defined(__EXPORT_CTOR_STRINGUTF8__)

CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const CTempString& src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const char* src ) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const string& src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
}


CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const CTempString& src, EEncoding encoding,EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const char* src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    const string& src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8_DEPRECATED::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const wstring& src) {
    assign( CUtf8::AsUTF8(src));
}
#endif
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
}
#endif

CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
CStringUTF8_DEPRECATED::CStringUTF8_DEPRECATED(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
#endif // __EXPORT_CTOR_STRINGUTF8__

//#ifndef __NO_EXPORT_STRINGUTF8__
#if 0

CStringUTF8::CStringUTF8(const CTempString& src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
} // NCBI_FAKE_WARNING
CStringUTF8::CStringUTF8(const char* src ) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
} // NCBI_FAKE_WARNING
CStringUTF8::CStringUTF8(const string& src) {
    assign( CUtf8::AsUTF8(src, eEncoding_ISO8859_1, CUtf8::eNoValidate));
} // NCBI_FAKE_WARNING

string CStringUTF8::AsLatin1(const char* substitute_on_error) const
{
    return CUtf8::AsSingleByteString(*this,eEncoding_ISO8859_1,substitute_on_error);
}
wstring CStringUTF8::AsUnicode(const wchar_t* substitute_on_error) const
{
    return CUtf8::AsBasicString<wchar_t>(*this,substitute_on_error,CUtf8::eNoValidate);
}
TStringUCS2 CStringUTF8::AsUCS2(const TCharUCS2* substitute_on_error) const
{
    return CUtf8::AsBasicString<TCharUCS2>(*this,substitute_on_error,CUtf8::eNoValidate);
}

CStringUTF8 CStringUTF8::TruncateSpaces(const CTempString& str,
                                    NStr::ETrunc side)
{
    return CUtf8::TruncateSpaces(str,side);
}


CStringUTF8::CStringUTF8(
    const CTempString& src, EEncoding encoding,EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}

CStringUTF8::CStringUTF8(
    const char* src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}

CStringUTF8::CStringUTF8(
    const string& src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
}
CStringUTF8::CStringUTF8(const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8::CStringUTF8(const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8::CStringUTF8(const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
CStringUTF8::CStringUTF8(const wstring& src) {
    assign( CUtf8::AsUTF8(src));
}
#endif
CStringUTF8::CStringUTF8(const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8::CStringUTF8(const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
}
CStringUTF8::CStringUTF8(const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
}
#if defined(HAVE_WSTRING)
CStringUTF8::CStringUTF8(const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
}
#endif

CStringUTF8::CStringUTF8(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}

CStringUTF8::CStringUTF8(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}

CStringUTF8::CStringUTF8(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}

CStringUTF8::CStringUTF8(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
}
CStringUTF8& CStringUTF8::operator= (const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator= (const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator= (const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::operator= (const wstring& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::operator= (const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator= (const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator= (const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::operator= (const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::operator+= (const TStringUnicode& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator+= (const TStringUCS4& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator+= (const TStringUCS2& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::operator+= (const wstring& src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::operator+= (const TUnicodeSymbol* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator+= (const TCharUCS4* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::operator+= (const TCharUCS2* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::operator+= (const wchar_t* src) {
    append( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Assign(
    const CTempString& src, EEncoding encoding, EValidate validate) {
    assign( CUtf8::AsUTF8(src, encoding, validate == CStringUTF8::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
    return *this;
}

CStringUTF8& CStringUTF8::Assign(const TStringUnicode& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(const TStringUCS4& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(const TStringUCS2& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Assign(const wstring& src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Assign(const TUnicodeSymbol* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(const TCharUCS4* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(const TCharUCS2* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Assign(const wchar_t* src) {
    assign( CUtf8::AsUTF8(src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Assign(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
CStringUTF8& CStringUTF8::Assign(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Assign(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    assign( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Assign(char ch, EEncoding encoding) {
    assign( CUtf8::AsUTF8( CTempString(&ch,1), encoding, CUtf8::eValidate));
    return *this;
}
CStringUTF8& CStringUTF8::Append(
    const CTempString& src, EEncoding encoding, EValidate validate) {
    append( CUtf8::AsUTF8( src, encoding, validate == CStringUTF8::eValidate ? CUtf8::eValidate : CUtf8::eNoValidate));
    return *this;
}
CStringUTF8& CStringUTF8::Append(const TStringUnicode& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
CStringUTF8& CStringUTF8::Append(const TStringUCS4& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
CStringUTF8& CStringUTF8::Append(const TStringUCS2& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Append(const wstring& src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Append(const TUnicodeSymbol* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
CStringUTF8& CStringUTF8::Append(const TCharUCS4* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
CStringUTF8& CStringUTF8::Append(const TCharUCS2* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Append(const wchar_t* src) {
    append( CUtf8::AsUTF8( src));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Append(
    ECharBufferType type, const TUnicodeSymbol* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
CStringUTF8& CStringUTF8::Append(
    ECharBufferType type, const TCharUCS4* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
CStringUTF8& CStringUTF8::Append(
    ECharBufferType type, const TCharUCS2* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#if defined(HAVE_WSTRING)
CStringUTF8& CStringUTF8::Append(
    ECharBufferType type, const wchar_t* src, SIZE_TYPE char_count) {
    append( CUtf8::AsUTF8(src, type == eCharBuffer ? char_count : NPOS));
    return *this;
}
#endif
CStringUTF8& CStringUTF8::Append(char ch, EEncoding encoding) {
    append( CUtf8::AsUTF8( CTempString(&ch,1), encoding, CUtf8::eValidate));
    return *this;
}
CStringUTF8& CStringUTF8::Append(TUnicodeSymbol ch) {
    append( CUtf8::AsUTF8(&ch, 1));
    return *this;
}
SIZE_TYPE CStringUTF8::GetSymbolCount(void) const {
    return CUtf8::GetSymbolCount(*this);
}
bool CStringUTF8::IsValid(void) const {
    return CUtf8::MatchEncoding(*this, eEncoding_UTF8);
}
TUnicodeSymbol CStringUTF8::Decode(const char*& src) {
    return CUtf8::Decode(src);
}
#ifndef NCBI_COMPILER_WORKSHOP
TUnicodeSymbol CStringUTF8::Decode(string::const_iterator& src) {
    return CUtf8::Decode(src);
}
#endif


SIZE_TYPE CStringUTF8::GetSymbolCount(const CTempString& src)
{
    return CUtf8::GetSymbolCount(src);
}

SIZE_TYPE CStringUTF8::GetValidSymbolCount(const char* src, SIZE_TYPE buf_size)
{
    return CUtf8::GetValidSymbolCount(CTempString(src, buf_size));
}

SIZE_TYPE CStringUTF8::GetValidSymbolCount(const CTempString& src)
{
    return CUtf8::GetValidSymbolCount(src);
}

SIZE_TYPE CStringUTF8::GetValidBytesCount(const char* src, SIZE_TYPE buf_size)
{
    return CUtf8::GetValidBytesCount(CTempString(src,buf_size));
}

SIZE_TYPE CStringUTF8::GetValidBytesCount(const CTempString& src)
{
    return CUtf8::GetValidBytesCount(src);
}

string CStringUTF8::AsSingleByteString(EEncoding encoding,
    const char* substitute_on_error) const
{
    return CUtf8::AsSingleByteString(*this,encoding,substitute_on_error);
}

EEncoding CStringUTF8::GuessEncoding( const CTempString& src)
{
    return CUtf8::GuessEncoding(src);
}

bool CStringUTF8::MatchEncoding( const CTempString& src, EEncoding encoding)
{
    return CUtf8::MatchEncoding(src,encoding);
}

string CStringUTF8::EncodingToString(EEncoding encoding)
{
    return CUtf8::EncodingToString(encoding);
}

EEncoding CStringUTF8::StringToEncoding(const CTempString& str)
{
    return CUtf8::StringToEncoding(str);
}


TUnicodeSymbol CStringUTF8::CharToSymbol(char ch, EEncoding encoding)
{
    return CUtf8::CharToSymbol(ch,encoding);
}

char CStringUTF8::SymbolToChar(TUnicodeSymbol sym, EEncoding encoding)
{
    return CUtf8::SymbolToChar(sym,encoding);
}

TUnicodeSymbol  CStringUTF8::DecodeFirst(char ch, SIZE_TYPE& more)
{
    return CUtf8::DecodeFirst(ch,more);
}

TUnicodeSymbol  CStringUTF8::DecodeNext(TUnicodeSymbol chU, char ch)
{
    return CUtf8::DecodeNext(chU,ch);
}

bool  CStringUTF8::IsWhiteSpace(TUnicodeSymbol chU)
{
    return CUtf8::IsWhiteSpace(chU);
}

CStringUTF8& CStringUTF8::TruncateSpacesInPlace(NStr::ETrunc side)
{
    return CUtf8::TruncateSpacesInPlace(*this,side);
}

CTempString CStringUTF8::TruncateSpaces_Unsafe(const CTempString& str, NStr::ETrunc side)
{
    return CUtf8::TruncateSpaces_Unsafe(str,side);
}

void   CStringUTF8::x_Validate(void) const
{
    CUtf8::x_Validate(*this);
}

void   CStringUTF8::x_AppendChar(TUnicodeSymbol ch)
{
    CUtf8::x_AppendChar(*this, ch);
}

void   CStringUTF8::x_Append(const CTempString& src,
                EEncoding encoding, EValidate validate)
{
    CUtf8::x_Append(*this, src, encoding, (CUtf8::EValidate)validate);
}

SIZE_TYPE CStringUTF8::x_BytesNeeded(TUnicodeSymbol ch)
{
    return CUtf8::x_BytesNeeded(ch);
}

bool   CStringUTF8::x_EvalFirst(char ch, SIZE_TYPE& more)
{
    return CUtf8::x_EvalFirst(ch, more);
}

bool   CStringUTF8::x_EvalNext(char ch)
{
    return CUtf8::x_EvalNext(ch);
}
#endif // __NO_EXPORT_STRINGUTF8__

SIZE_TYPE CUtf8::x_GetValidSymbolCount(const CTempString& str,
     CTempString::const_iterator& src)
{
    SIZE_TYPE count = 0;
    src = str.begin();
    CTempString::const_iterator to = str.end();
    for (; src != to; ++src, ++count) {
        SIZE_TYPE more = 0;
        bool good = x_EvalFirst(*src, more);
        while (more-- && good) {
            good = x_EvalNext(*(++src));
        }
        if ( !good ) {
            return count;
        }
    }
    return count;
}

SIZE_TYPE CUtf8::GetSymbolCount( const CTempString& str)
{
    CTempString::const_iterator err;
    SIZE_TYPE count = x_GetValidSymbolCount(str,err);
    if (err != str.end()) {
        NCBI_THROW2(CStringException, eFormat,
                    "String is not in UTF8 format",
                    (err - str.begin()));
    }
    return count;
}

string CUtf8::AsSingleByteString( const CTempString& str,
    EEncoding encoding, const char* substitute_on_error, EValidate validate)
{
    if (validate == eValidate) {
        if ( !MatchEncoding( str,eEncoding_UTF8 ) ) {
            NCBI_THROW2(CStringException, eBadArgs,
                "Source string is not in UTF8 format", 0);
        }
    }
    if( encoding == eEncoding_UTF8) {
        return str;
    }
    string result;
    result.reserve( GetSymbolCount(str)+1 );
    CTempString::const_iterator src = str.begin();
    CTempString::const_iterator to = str.end();
    for ( ; src != to; ++src ) {
        TUnicodeSymbol sym = CUtf8::Decode( src );
        if (substitute_on_error) {
            try {
                result.append(1, SymbolToChar( sym, encoding));
            }
            catch (CStringException&) {
                result.append(substitute_on_error);
            }
        } else {
            result.append(1, SymbolToChar( sym, encoding));
        }
    }
    return result;
}

EEncoding CUtf8::GuessEncoding( const CTempString& src)
{
    SIZE_TYPE more = 0;
    CTempString::const_iterator i = src.begin();
    CTempString::const_iterator end = src.end();
    bool cp1252, iso1, ascii, utf8;
    for (cp1252 = iso1 = ascii = utf8 = true; i != end; ++i) {
        Uint1 ch = *i;
        bool skip = false;
        if (more != 0) {
            if (x_EvalNext(ch)) {
                --more;
                if (more == 0) {
                    ascii = false;
                }
                skip = true;
            } else {
                more = 0;
                utf8 = false;
            }
        }
        if (ch > 0x7F) {
            ascii = false;
// http://en.wikipedia.org/wiki/ISO/IEC_8859-1
// Note:  From the point of view of the C++ Toolkit, the ISO 8859-1
// character set includes symbols 0x00 through 0xFF except 0x80 through 0x9F.
            if (ch < 0xA0) {
                iso1 = false;
// http://en.wikipedia.org/wiki/Windows-1252
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
    } else if (utf8) {
        return eEncoding_UTF8;
    } else if (cp1252) {
        return iso1 ? eEncoding_ISO8859_1 : eEncoding_Windows_1252;
    }
    return eEncoding_Unknown;
}


bool CUtf8::MatchEncoding( const CTempString& src, EEncoding encoding)
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

string  CUtf8::EncodingToString(EEncoding encoding)
{
    switch (encoding) {
    case eEncoding_UTF8:         break;
    case eEncoding_Ascii:        return "US-ASCII";
    case eEncoding_ISO8859_1:    return "ISO-8859-1";
    case eEncoding_Windows_1252: return "windows-1252";
    default:
        NCBI_THROW2(CStringException, eBadArgs,
            "Cannot convert encoding to string", 0);
        break;
    }
    return "UTF-8";
}

// see http://www.iana.org/assignments/character-sets
EEncoding CUtf8::StringToEncoding(const CTempString& str)
{
    if (NStr::CompareNocase(str,"UTF-8")==0) {
        return eEncoding_UTF8;
    }
    if (NStr::CompareNocase(str,"windows-1252")==0) {
        return eEncoding_Windows_1252;
    }
    int i;
    const char* ascii[] = {
    "ANSI_X3.4-1968","iso-ir-6","ANSI_X3.4-1986","ISO_646.irv:1991",
    "ASCII","ISO646-US","US-ASCII","us","IBM367","cp367","csASCII", NULL};
    for (i=0; ascii[i]; ++i) {
        if (NStr::CompareNocase(str,ascii[i])==0) {
            return eEncoding_Ascii;
        }
    }
    const char* iso8859_1[] = {
    "ISO_8859-1:1987","iso-ir-100","ISO_8859-1","ISO-8859-1",
    "latin1","l1","IBM819","CP819","csISOLatin1", NULL};
    for (i=0; iso8859_1[i]; ++i) {
        if (NStr::CompareNocase(str,iso8859_1[i])==0) {
            return eEncoding_ISO8859_1;
        }
    }
    return eEncoding_Unknown;
}


// cp1252, codepoints for chars 0x80 to 0x9F
static const TUnicodeSymbol s_cp1252_table[] = {
    0x20AC, 0x003F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003F, 0x017D, 0x003F,
    0x003F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003F, 0x017E, 0x0178
};

TUnicodeSymbol CUtf8::CharToSymbol(char c, EEncoding encoding)
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


char CUtf8::SymbolToChar(TUnicodeSymbol cp, EEncoding encoding)
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

void CUtf8::x_Validate(const CTempString& str)
{
    if ( !MatchEncoding( str,eEncoding_UTF8 ) ) {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
}

CStringUTF8& CUtf8::x_AppendChar( CStringUTF8& self, TUnicodeSymbol c)
{
    Uint4 ch = c;
    if (ch < 0x80) {
        self.append(1, Uint1(ch));
    }
    else if (ch < 0x800) {
        self.append(1, Uint1( (ch >>  6)         | 0xC0));
        self.append(1, Uint1( (ch        & 0x3F) | 0x80));
    } else if (ch < 0x10000) {
        self.append(1, Uint1( (ch >> 12)         | 0xE0));
        self.append(1, Uint1(((ch >>  6) & 0x3F) | 0x80));
        self.append(1, Uint1(( ch        & 0x3F) | 0x80));
    } else {
        self.append(1, Uint1( (ch >> 18)         | 0xF0));
        self.append(1, Uint1(((ch >> 12) & 0x3F) | 0x80));
        self.append(1, Uint1(((ch >>  6) & 0x3F) | 0x80));
        self.append(1, Uint1( (ch        & 0x3F) | 0x80));
    }
    return self;
}

CStringUTF8& CUtf8::x_Append( CStringUTF8& self, const CTempString& src,
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
        self.append(src);
        return self;
    }

    SIZE_TYPE needed = 0;
    CTempString::const_iterator i;
    CTempString::const_iterator end = src.end();
    for (i = src.begin(); i != end; ++i) {
        needed += x_BytesNeeded( CharToSymbol( *i,encoding ) );
    }
    if ( !needed ) {
        return self;
    }
    self.reserve(max(self.capacity(),self.length()+needed+1));
    for (i = src.begin(); i != end; ++i) {
        x_AppendChar( self, CharToSymbol( *i, encoding ) );
    }
    return self;
}

SIZE_TYPE CUtf8::x_BytesNeeded(TUnicodeSymbol c)
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


bool CUtf8::x_EvalFirst(char ch, SIZE_TYPE& more)
{
    more = 0;
    if ((ch & 0x80) != 0) {
        if ((ch & 0xE0) == 0xC0) {
            if ((ch & 0xFE) == 0xC0) {
                // C0 and C1 are not valid UTF-8 chars
                return false;
            }
            more = 1;
        } else if ((ch & 0xF0) == 0xE0) {
            more = 2;
        } else if ((ch & 0xF8) == 0xF0) {
            if ((unsigned char)ch > (unsigned char)0xF4) {
                // F5-FF are not valid UTF-8 chars
                return false;
            }
            more = 3;
        } else {
            return false;
        }
    }
    return true;
}


bool CUtf8::x_EvalNext(char ch)
{
    return (ch & 0xC0) == 0x80;
}

TUnicodeSymbol CUtf8::DecodeFirst(char ch, SIZE_TYPE& more)
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
    } else {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
    return chRes;
}


TUnicodeSymbol CUtf8::DecodeNext(TUnicodeSymbol chU, char ch)
{
    if ((ch & 0xC0) == 0x80) {
        return (chU << 6) | (ch & 0x3F);
    } else {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
    return 0;
}

bool CUtf8::IsWhiteSpace(TUnicodeSymbol chU)
{
/*
    {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20, 0x85, 0xA0, 0x1680, 0x180E,
    0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
    0x2028, 0x2029, 0x202F, 0x205F, 0x3000 };
*/
    if (chU >= 0x85) {
        if (chU < 0x2000) {
            return chU == 0x85 || chU == 0xA0 || chU == 0x1680 || chU == 0x180E;
        } else if (chU >= 0x3000) {
            return chU == 0x3000;
        }
        return chU <=0x200A || chU == 0x2028 || chU == 0x2029 || chU == 0x202F || chU ==  0x205F;
    }
    return iswspace(chU)!=0;
}

CStringUTF8& CUtf8::TruncateSpacesInPlace( CStringUTF8& str, NStr::ETrunc side)
{
    if (!str.empty()) {
        CTempString t( TruncateSpaces_Unsafe( str,side));
        if (t.empty()) {
            str.erase();
        } else {
            str.replace(0,str.length(),t.data(),t.length());
        }
    }
    return str;
}

CTempString CUtf8::TruncateSpaces_Unsafe(
    const CTempString& str, NStr::ETrunc side)
{
    if (str.empty()) {
        return str;
    }
    CTempString::const_iterator beg = str.begin();
    CTempString::const_iterator end = str.end();
    if (side == NStr::eTrunc_Begin  ||  side == NStr::eTrunc_Both) {
        for (CTempString::const_iterator next = beg; beg != end; beg = ++next) {
            if (!IsWhiteSpace( CUtf8::Decode( next ) )) {
                break;
            }
        }
    }
    if (side == NStr::eTrunc_End  ||  side == NStr::eTrunc_Both) {
        while (end != beg) {
            while (end != beg) {
                char ch = *(--end);
                if ((ch & 0x80) == 0 || (ch & 0xC0) == 0xC0) {
                    break;
                }
            }
            CTempString::const_iterator next = end;
            if (!IsWhiteSpace( CUtf8::Decode( next ) )) {
                end = ++next;
                break;
            }
        }
    }
    CTempString res;
    if (beg != end) {
        res.assign(beg,end-beg);
    }
    return res;
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


/////////////////////////////////////////////////////////////////////////////
//  CStringPairsParser decoders and encoders


CStringDecoder_Url::CStringDecoder_Url(NStr::EUrlDecode flag)
    : m_Flag(flag)
{
}


string CStringDecoder_Url::Decode(const CTempString& src,
                                  EStringType ) const
{
    return NStr::URLDecode(src, m_Flag);
}


CStringEncoder_Url::CStringEncoder_Url(NStr::EUrlEncode flag)
    : m_Flag(flag)
{
}


string CStringEncoder_Url::Encode(const CTempString& src,
                                  EStringType ) const
{
    return NStr::URLEncode(src, m_Flag);
}


/////////////////////////////////////////////////////////////////////////////
// CEncodedString --

CEncodedString::CEncodedString(const CTempString& s,
                               NStr::EUrlEncode flag)
{
    SetString(s, flag);
}


void CEncodedString::SetString(const CTempString& s,
                               NStr::EUrlEncode flag)
{
    m_Original = s;
    if ( NStr::NeedsURLEncoding(s, flag) ) {
        if ( m_Encoded.get() ) {
            // Do not re-allocate string object
            *m_Encoded = NStr::URLEncode(s, flag);
        }
        else {
            m_Encoded.reset(new string(NStr::URLEncode(s, flag)));
        }
    }
    else {
        m_Encoded.reset();
    }
}


/////////////////////////////////////////////////////////////////////////////
//  CTempString (deprecated constructors, defined out of line to cut down
//  on spurious warnings when building with compilers that warn on
//  definition rather than merely, and arguably more sensibly, on usage).


CTempString::CTempString(const char* str, size_type pos, size_type len)
    : m_String(str+pos), m_Length(len)
{
} // NCBI_FAKE_WARNING


CTempString::CTempString(const string& str, size_type len)
    : m_String(str.data()), m_Length(min(len, str.size()))
{
} // NCBI_FAKE_WARNING



void CTempStringList::Join(string* s) const
{
    s->reserve(GetSize());
    *s = m_FirstNode.str;
    for (const SNode* node = m_FirstNode.next.get();  node != NULL;
         node = node->next.get()) {
        s->append(node->str.data(), node->str.size());
    }
}


void CTempStringList::Join(CTempStringEx* s) const
{
    if (m_FirstNode.next.get() == NULL) {
        *s = m_FirstNode.str;
    } else {
        if ( !m_Storage ) {
            NCBI_THROW2(CStringException, eBadArgs,
                "CTempStringList::Join(): non-NULL storage required", 0);
        }
        SIZE_TYPE n = GetSize();
        char* buf = m_Storage->Allocate(n + 1);
        char* p = buf;
        for (const SNode* node = &m_FirstNode;  node != NULL;
             node = node->next.get()) {
            memcpy(p, node->str.data(), node->str.size());
            p += node->str.size();
        }
        *p = '\0';
        s->assign(buf, n);
    }
}


SIZE_TYPE CTempStringList::GetSize(void) const
{
    SIZE_TYPE total = m_FirstNode.str.size();
    for (const SNode* node = m_FirstNode.next.get();  node != NULL;
         node = node->next.get()) {
        total += node->str.size();
    }
    return total;
}


bool CStrTokenizeBase::Advance(CTempStringList* part_collector)
{
    SIZE_TYPE pos = m_Pos, part_start = m_Pos, delim_pos = 0, quote_pos = 0;
    bool      found_text = false, done = (pos == NPOS);
    char      active_quote = '\0';

    if (pos >= m_Str.size()) {
        pos = NPOS;
        done = true;
    }

    // Each chunk covers the half-open interval [part_start, delim_pos).

    while ( !done
           &&  ((delim_pos = m_Str.find_first_of(m_InternalDelim, pos))
                != NPOS)) {
        SIZE_TYPE next_start = pos = delim_pos + 1;
        bool      handled    = false;
        char      c          = m_Str[delim_pos];

        if ((m_Flags & NStr::fSplit_CanEscape) != 0  &&  c == '\\') {
            // treat the following character literally
            if (++pos > m_Str.size()) {
                NCBI_THROW2(CStringException, eFormat, "Unescaped trailing \\",
                            delim_pos);
            }
            handled = true;
        } else if ((m_Flags & NStr::fSplit_CanQuote) != 0) {
            if (active_quote != '\0') {
                if (c == active_quote) {
                    if (pos < m_Str.size()  &&  m_Str[pos] == active_quote) {
                        // count a doubled quote as one literal occurrence
                        ++pos;
                    } else {
                        active_quote = '\0';
                    }
                } else {
                    continue; // not actually a boundary
                }
                handled = true;
            } else if (((m_Flags & NStr::fSplit_CanSingleQuote) != 0
                        && c == '\'')
                       ||  ((m_Flags & NStr::fSplit_CanDoubleQuote) != 0
                            && c == '"')) {
                active_quote = c;
                quote_pos    = delim_pos;
                handled = true;
            }
        }

        if ( !handled ) {
            if ((m_Flags & NStr::fSplit_ByPattern) != 0) {
                if (delim_pos + m_Delim.size() <= m_Str.size()
                    &&  (memcmp(m_Delim.data() + 1, m_Str.data() + pos,
                                m_Delim.size() - 1) == 0)) {
                    done = true;
                    next_start = pos = delim_pos + m_Delim.size();
                } else {
                    continue;
                }
            } else {
                done = true;
            }
        }

        if (delim_pos > part_start) {
            found_text = true;
            if (part_collector != NULL) {
                part_collector->Add
                    (m_Str.substr(part_start, delim_pos - part_start));
            }
        }
        part_start = next_start;
    }

    if (active_quote != '\0') {
        NCBI_THROW2(CStringException, eFormat,
                    string("Unbalanced ") + active_quote,
                    quote_pos);
    }

    if (delim_pos == NPOS) {
        found_text = true;
        if (part_collector != NULL) {
            part_collector->Add(m_Str.substr(part_start));
        }
        m_Pos = NPOS;
    } else {
        m_Pos = pos;
        SkipDelims();
    }

    return found_text  ||  (m_Flags & NStr::fSplit_MergeDelims) == 0;
}

void CStrTokenizeBase::SkipDelims(void)
{
    if ((m_Flags & NStr::fSplit_MergeDelims) != 0) {
        if ((m_Flags & NStr::fSplit_ByPattern) == 0) {
            m_Pos = m_Str.find_first_not_of(m_Delim, m_Pos);
        } else {
            while (m_Pos + m_Delim.size() <= m_Str.size()  &&  m_Pos != NPOS
                   &&  (memcmp(m_Delim.data(), m_Str.data() + m_Pos,
                               m_Delim.size()) == 0)) {
                m_Pos += m_Delim.size();
            }
        }
    }
}

void CStrTokenizeBase::x_ExtendInternalDelim()
{
    if ( !(m_Flags & (NStr::fSplit_CanEscape | NStr::fSplit_CanQuote)) ) {
        return; // Nothing to do
    }

    SIZE_TYPE n = m_InternalDelim.size();
    char* buf = m_DelimStorage.Allocate(n + 3);
    char *s = buf;
    memcpy(s, m_InternalDelim.data(), n);
    if ((m_Flags & NStr::fSplit_CanEscape) != 0) {
        s[n++] = '\\';
    }
    if ((m_Flags & NStr::fSplit_CanSingleQuote) != 0) {
        s[n++] = '\'';
    }
    if ((m_Flags & NStr::fSplit_CanDoubleQuote) != 0) {
        s[n++] = '"';
    }
    m_InternalDelim.assign(buf, n);
}


END_NCBI_NAMESPACE;
