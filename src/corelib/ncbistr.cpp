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
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbistr_util.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbifloat.h>
#include <memory>
#include <algorithm>
#include <iterator>
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Corelib_Util


BEGIN_NCBI_NAMESPACE;


// Hex symbols (up to base 36)
static const char s_Hex[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


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
/// @internal
bool s_MatchesMask(const char* str, const char* mask, NStr::ECase use_case) 
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
                if (s_MatchesMask(str, mask, use_case)) {
                    return true;
                }
                ++str;
            }
            return false;

        default:
            // Compare nonpattern character in mask and name
            char s = *str++;
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


bool NStr::MatchesMask(const CTempStringEx& str, 
                       const CTempStringEx& mask, ECase use_case)
{
    const char* s_ptr = str.data();
    const char* m_ptr = mask.data();

    if ( str.HasZeroAtEnd() &&
         mask.HasZeroAtEnd() ) {
        // strings has zero at the end already
        return s_MatchesMask(s_ptr, m_ptr, use_case);
    }

    // Small temporary buffers on stack for appending zero chars
    char s_buf[256]; 
    char m_buf[256]; 

    // 'mask' usually have shorter length, check it first.
    if ( !mask.HasZeroAtEnd() ) {
        size_t size = mask.size();
        if ( size < sizeof(m_buf) ) {
            memcpy(m_buf, mask.data(), size);
            m_buf[size] = '\0';
            m_ptr = m_buf;
        } else {
            // 'mask' is long -- very rare case, can assume that 'str'
            // is long also.
            // use std::string() to allocate memory for appending zero char
            return s_MatchesMask(string(str).c_str(),
                                 string(mask).c_str(), use_case);
        }
    }
    if ( !str.HasZeroAtEnd() ) {
        size_t size = str.size();
        if ( size < sizeof(s_buf) ) {
            memcpy(s_buf, str.data(), size);
            s_buf[size] = '\0';
            s_ptr = s_buf;
        } else {
            // use std::string() to allocate memory for appending zero char
            return s_MatchesMask(string(str).c_str(), m_ptr, use_case);
        }
    }
    // Both strings are zero-terminated now
    return MatchesMask(s_ptr, m_ptr, use_case);
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
    int& errno_ref = errno;
    if ( str.empty() ) {
        errno_ref = EINVAL;
        return -1;
    }
    char ch = str[0];
    if ( !isdigit((unsigned char)ch) &&  (ch != '+') ) {
        errno_ref = EINVAL;
        return -1;
    }
    char* endptr = 0;
    const char* begptr = str.c_str();
    errno_ref = 0;
    unsigned long value = strtoul(begptr, &endptr, 10);
    if ( errno_ref ) {
        return -1;
    }
    else if ( !endptr  ||  endptr == begptr  ||  *endptr ) {
        errno_ref = EINVAL;
        return -1;
    }
    else if ( value > (unsigned long) kMax_Int ) {
        errno_ref = ERANGE;
        return -1;
    }
    return (int) value;
}


#define S2N_CONVERT_ERROR(to_type, msg, errcode, force_errno, delta)        \
        if (flags & NStr::fConvErr_NoThrow)  {                              \
            if ( force_errno || !errno )                                    \
                errno = errcode;                                            \
            /* ignore previosly converted value -- always return zero */    \
            return 0;                                                       \
        } else {                                                            \
            CTempString str_tmp(str);                                       \
            CTempString msg_tmp(msg);                                       \
            string smsg;                                                    \
            smsg.reserve(str_tmp.length() + msg_tmp.length() + 50);         \
            smsg += "Cannot convert string '";                              \
            smsg += str;                                                    \
            smsg += "' to " #to_type;                                       \
            if ( !msg_tmp.empty() ) {                                       \
                smsg += ", ";                                               \
                smsg += msg;                                                \
            }                                                               \
            NCBI_THROW2(CStringException, eConvert, smsg, delta);           \
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

#define CHECK_ENDPTR_SIZE(to_type)                                          \
    if ( pos < size ) {                                                     \
        S2N_CONVERT_ERROR(to_type, kEmptyStr, EINVAL, true, pos);           \
    }

#define CHECK_RANGE(nmin, nmax, to_type)                                    \
    if ( value < nmin  ||  value > nmax  ||  errno ) {                      \
        S2N_CONVERT_ERROR(to_type, "overflow", ERANGE, false, 0);           \
    }

#define CHECK_RANGE_U(nmax, to_type)                                        \
    if ( value > nmax  ||  errno ) {                                        \
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
    Int8 value = StringToInt8(str, flags, base);
    CHECK_RANGE(kMin_Int, kMax_Int, int);
    return (int) value;
}


unsigned int
NStr::StringToUInt(const CTempString& str, TStringToNumFlags flags, int base)
{
    Uint8 value = StringToUInt8(str, flags, base);
    CHECK_RANGE_U(kMax_UInt, unsigned int);
    return (unsigned int) value;
}


long NStr::StringToLong(const CTempString& str, TStringToNumFlags flags,
                        int base)
{
    Int8 value = StringToInt8(str, flags, base);
    CHECK_RANGE(kMin_Long, kMax_Long, long);
    return (long) value;
}


unsigned long
NStr::StringToULong(const CTempString& str, TStringToNumFlags flags, int base)
{
    Uint8 value = StringToUInt8(str, flags, base);
    CHECK_RANGE_U(kMax_ULong, long);
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

    errno = 0;
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
            S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, true, ptr-str.begin())
        }

        // Begin conversion
        Uint8 n = 0;

        const Uint8 limdiv = kMax_UI8/10;
        const int   limoff = int(kMax_UI8 % 10);

        do {
            char ch = *ptr;
            int  delta = ch - '0';
            if ( unsigned(delta) >= 10 ) {
                S2N_CONVERT_ERROR(Uint8, kEmptyStr, EINVAL, true, ptr-str.begin());
            }
            // Overflow check
            if ( n >= limdiv && (n > limdiv || delta > limoff) ) {
                S2N_CONVERT_ERROR(Uint8, kEmptyStr, ERANGE, true, ptr-str.begin());
            }
            n = n*10+delta;
        } while ( ++ptr != end );
        errno = 0;
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

    errno = 0;
    while (char ch = str[pos]) {
        int  delta;         // corresponding numeric value of 'ch'

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
// skip leading blanks
    for ( ; isspace(*ptr); ++ptr)
        ;
    const char* start = ptr;
    long double ret = NCBI_CONST_LONGDOUBLE(0.);
    bool sign= false, negate= false, dot= false, expn=false, anydigits=false;
    int digits = 0, dot_position = 0, exponent = 0;
    unsigned int first=0, second=0, first_exp=1;
    long double second_exp=NCBI_CONST_LONGDOUBLE(1.),
                third    = NCBI_CONST_LONGDOUBLE(0.);
    char c;
// up to exponent
    for( ; ; ++ptr) {
        c = *ptr;
        // sign: should be no digits at this point
        if (c == '-' || c == '+') {
            // if there was sign or digits, stop
            if (sign || digits) {
                break;
            }
            sign = true;
            negate = c == '-';
        }
        // digits: accumulate
        else if (c >= '0' && c <= '9') {
            anydigits = true;
            ++digits;
            if (first == 0 && c == '0') {
                --digits;
                if (dot) {
                    --dot_position;
                }
            } else if (digits <= 9) {
                first = first*10 + (c-'0');
            } else if (digits <= 18) {
                first_exp *= 10;
                second = second*10 + (c-'0');
            } else {
                second_exp *= NCBI_CONST_LONGDOUBLE(10.);
                third = third * NCBI_CONST_LONGDOUBLE(10.) + (c-'0');
            }
        }
        // dot
        else if (c == '.') {
            // if second dot, stop
            if (dot) {
                break;
            }
            dot_position = digits;
            dot = true;
        }
        // if exponent, stop
        else if (c == 'e' || c == 'E') {
            if (!digits) {
                break;
            }
            expn = true;
            ++ptr;
            break;
        }
        else {
            if (!digits) {
                if (!dot && NStr::strncasecmp(ptr,"nan",3)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+3);
                    }
                    return HUGE_VAL/HUGE_VAL; /* NCBI_FAKE_WARNING */
                }
                if (NStr::strncasecmp(ptr,"inf",3)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+3);
                    }
                    return negate ? -HUGE_VAL : HUGE_VAL;
                }
                if (NStr::strncasecmp(ptr,"infinity",8)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+8);
                    }
                    return negate ? -HUGE_VAL : HUGE_VAL;
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
        errno = EINVAL;
        return 0.;
    }
    exponent = dot ? dot_position - digits : 0;
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
    ret = ((long double)first * first_exp + second)* second_exp + third;
    // calculate exponent
    if ((first || second) && exponent) {
        if (exponent > 2*DBL_MAX_10_EXP) {
            ret = HUGE_VAL;
            errno = ERANGE;
        } else if (exponent < 2*DBL_MIN_10_EXP) {
            ret = 0.;
            errno = ERANGE;
        } else {
            for (; exponent < -256; exponent += 256) {
                ret /= NCBI_CONST_LONGDOUBLE(1.e256);
            }
            long double power      = NCBI_CONST_LONGDOUBLE(1.),
                        power_mult = NCBI_CONST_LONGDOUBLE(10.);
            unsigned int mask = 1;
            unsigned int uexp = exponent < 0 ? -exponent : exponent;
            int count = 1;
            for (; count < 32 && mask <= uexp; ++count, mask <<= 1) {
                if (mask & uexp) {
                    switch (mask) {
                    case   0: break;
                    case   1: power *= NCBI_CONST_LONGDOUBLE(10.);    break;
                    case   2: power *= NCBI_CONST_LONGDOUBLE(100.);   break;
                    case   4: power *= NCBI_CONST_LONGDOUBLE(1.e4);   break;
                    case   8: power *= NCBI_CONST_LONGDOUBLE(1.e8);   break;
                    case  16: power *= NCBI_CONST_LONGDOUBLE(1.e16);  break;
                    case  32: power *= NCBI_CONST_LONGDOUBLE(1.e32);  break;
                    case  64: power *= NCBI_CONST_LONGDOUBLE(1.e64);  break;
                    case 128: power *= NCBI_CONST_LONGDOUBLE(1.e128); break;
                    case 256: power *= NCBI_CONST_LONGDOUBLE(1.e256); break;
                    default:  power *= power_mult;                    break;
                    }
                }
                if (mask >= 256) {
                    if (mask == 256) {
                        power_mult = NCBI_CONST_LONGDOUBLE(1.e256);
                    }
                    power_mult = power_mult*power_mult;
                }
            }
            if (exponent < 0) {
                ret /= power;
                if (double(ret) == 0.) {
                    errno = ERANGE;
                }
            } else {
                ret *= power;
                if (!finite(double(ret))) {
                    errno = ERANGE;
                }
            }
        }
    }
    if (negate) {
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
    char* endptr = 0;
    const char* begptr = str + pos;

    int the_errno = 0;
    errno = 0;
    double n;
    if (flags & NStr::fDecimalPosix) {
        n = NStr::StringToDoublePosix(begptr, &endptr);
    } else {
        n = strtod(begptr, &endptr);
    }
    the_errno = errno;
    if (flags & NStr::fDecimalPosixOrLocal) {
        char* endptr2 = 0;
        double n2 = NStr::StringToDoublePosix(begptr, &endptr2);
        if (!endptr || (endptr2 && endptr2 > endptr)) {
            n = n2;
            endptr = endptr2;
            the_errno = errno;
        }
    }
    if (flags & NStr::fIgnoreErrno) {
        the_errno = 0;
    }
    if ( the_errno  ||  !endptr  ||  endptr == begptr ) {
        S2N_CONVERT_ERROR(double, kEmptyStr, EINVAL, false,
                          s_DiffPtr(endptr, begptr) + pos);
    }
#if 0
    if ( !s_IsDecimalPoint(*(endptr - 1), flags) && s_IsDecimalPoint(*endptr, flags) ) {
        // Only a single dot at the end of line is allowed
        if (endptr == strchr(begptr, *endptr)) {
            endptr++;
        }
    }
#endif
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
        s_SkipAllowedSymbols(str, pos, spaces ? eSkipSpacesOnly : eSkipAll, flags);
    }
    CHECK_ENDPTR(Uint8);
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
            *--pos = s_Hex[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = s_Hex[value % base];
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
        return;
    }
    unsigned int value = static_cast<unsigned int>(svalue);
    
    if ( base == 10  &&  svalue < 0 ) {
        value = static_cast<unsigned int>(-svalue);
    }
    s_SignedToString(out_str, value, svalue, flags, base);
}


void NStr::LongToString(string& out_str, long svalue,
                       TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        return;
    }
    unsigned long value = static_cast<unsigned long>(svalue);
    
    if ( base == 10  &&  svalue < 0 ) {
        value = static_cast<unsigned long>(-svalue);
    }
    s_SignedToString(out_str, value, svalue, flags, base);
}


void NStr::ULongToString(string&          out_str,
                        unsigned long     value,
                        TNumToStringFlags flags,
                        int               base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
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
            *--pos = s_Hex[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = s_Hex[value % base];
            value /= base;
        } while ( value );
    }

    out_str.assign(pos, buffer + kBufSize - pos);
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
            *--pos = s_Hex[value % 16];
            value /= 16;
        } while ( value );
    }
    else {
        do {
            *--pos = s_Hex[value % base];
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
}


void NStr::UInt8ToString(string& out_str, Uint8 value,
                         TNumToStringFlags flags, int base)
{
    _ASSERT(flags == 0  ||  flags > 32);
    if ( base < 2  ||  base > 36 ) {
        return;
    }

    const SIZE_TYPE kBufSize = CHAR_BIT  * sizeof(value);
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
}



SIZE_TYPE NStr::DoubleToString(double value, unsigned int precision,
                               char* buf, SIZE_TYPE buf_size,
                               TNumToStringFlags flags)
{
    char buffer[kMaxDoubleStringSize];
    int n = 0;
    if ((flags & fDoublePosix) && (isnan(value) || !finite(value))) {
        if (isnan(value)) {
            strcpy(buffer, "NAN");
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
    return n_copy;
}


string NStr::SizetToString(size_t value, TNumToStringFlags flags, int base)
{
#if (SIZEOF_SIZE_T > 4)
    return UInt8ToString(value, flags, base);
#else
    return UIntToString(value, flags, base);
#endif
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


const void* NStr::StringToPtr(const CTempStringEx& str)
{
    void *ptr = NULL;
    if ( str.HasZeroAtEnd() ) {
        ::sscanf(str.data(), "%p", &ptr);
    } else {
        ::sscanf(string(str).c_str(), "%p", &ptr);
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
    SIZE_TYPE size = 1024;
    AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
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

CTempString NStr::TruncateSpaces(const CTempString& str, ETrunc where)
{
    return s_TruncateSpaces(str, where, CTempString());
}

CTempString NStr::TruncateSpaces(const char* str, ETrunc where)
{
    return s_TruncateSpaces(CTempString(str), where, CTempString());
}

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
    dst = src;

    if ( start_pos + search.size() > src.size() ||
         search == replace )
        return dst;

    for (SIZE_TYPE count = 0; !(max_replace && count >= max_replace); count++){
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
                    TContainer& arr, NStr::EMergeDelims merge,
                    vector<SIZE_TYPE>* token_pos)
{
    typedef CStrTokenPosAdapter<vector<SIZE_TYPE> >         TPosArray;
    typedef CStrDummyTargetReserve<TString, TContainer, 
            TPosArray, CStrDummyTokenCount<TString > >      TReserve;
    typedef CStrTokenize<TString, TContainer, TPosArray,
                         CStrDummyTokenCount<TString>,
                         TReserve>                          TSplitter;
    TPosArray token_pos_proxy(token_pos);
    TSplitter::Do(str, delim, arr, 
                  (CStrTokenizeBase::EMergeDelims)merge, 
                  token_pos_proxy,
                  kEmptyStr);
    return arr;
}


list<string>& NStr::Split(const CTempString& str, const CTempString& delim,
                          list<string>& arr, EMergeDelims merge,
                          vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, merge, token_pos);

/*
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
*/
}


list<CTempString>& NStr::Split(const CTempString& str, const CTempString& delim,
                               list<CTempString>& arr, EMergeDelims merge,
                               vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, merge, token_pos);
}


vector<string>& NStr::Tokenize(const CTempString& str, const CTempString& delim,
                               vector<string>& arr, EMergeDelims merge,
                               vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, merge, token_pos);

/*
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
*/
}


vector<CTempString>& NStr::Tokenize(const CTempString& str,
                                    const CTempString& delim,
                                    vector<CTempString>& arr,
                                    EMergeDelims merge,
                                    vector<SIZE_TYPE>* token_pos)
{
    return s_Split(str, delim, arr, merge, token_pos);
}

vector<string>& NStr::TokenizePattern(const CTempString& str,
                                      const CTempString& pattern,
                                      vector<string>&    arr,
                                      EMergeDelims       merge,
                                      vector<SIZE_TYPE>* token_pos)
{
    vector<CTempString> tsa;
    TokenizePattern(str, pattern, tsa, merge, token_pos);
    if (arr.empty()) {
        arr.reserve(tsa.size());
    }
    copy(tsa.begin(), tsa.end(), back_inserter(arr));
    return arr;
}

vector<CTempString>& NStr::TokenizePattern(const CTempString&   str,
                                           const CTempString&   pattern,
                                           vector<CTempString>& arr,
                                           EMergeDelims         merge,
                                           vector<SIZE_TYPE>*   token_pos)
{
    // Special cases
    if (str.empty()) {
        return arr;
    } else if (pattern.empty()) {
        // Avoid using temporary objects
        //~ arr.push_back(str);
        arr.push_back(kEmptyStr);
        arr.back().assign(str.data(), str.length());
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
                    arr.back().assign(str.data(), prev_pos,
                                      str.length() - prev_pos);
                    if (token_pos)
                        token_pos->push_back(prev_pos);
                }
                break;
            }
            // Avoid using temporary objects
            // ~ arr.push_back(str.substr(prev_pos, pos - prev_pos));
            arr.push_back(kEmptyStr);
            arr.back().assign(str.data(), prev_pos, pos - prev_pos);
            if (token_pos)
                token_pos->push_back(prev_pos);
        }
        prev_pos = pos + pattern.length();
    }
    return arr;
}


bool NStr::SplitInTwo(const CTempString& str, 
                      const CTempString& delim,
                      string& str1, string& str2, EMergeDelims merge)
{
    CTempString ts1, ts2;
    bool result = SplitInTwo(str, delim, ts1, ts2, merge);
    str1 = ts1;
    str2 = ts2;
    return result;
}

bool NStr::SplitInTwo(const CTempString& str, 
                      const CTempString& delim,
                      CTempString& str1, CTempString& str2, EMergeDelims merge)
{
    SIZE_TYPE delim_pos = str.find_first_of(delim);
    if (NPOS == delim_pos) {
        // only one piece
        str1 = str;
        str2 = kEmptyStr;
        return false;
    }
    str1.assign(str.data(), 0, delim_pos);

    // Skip merged delimeters if needed
    SIZE_TYPE next_pos = (merge == eMergeDelims
                          ? str.find_first_not_of(delim, delim_pos + 1)
                          : delim_pos + 1);
    if (next_pos == NPOS) {
        str2 = kEmptyStr;
    } else {
        str2.assign(str.data(), next_pos, str.length() - next_pos);
    }
    return true;
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


static inline bool s_IsQuoted(char c, ELanguage lang)
{
    return (c == '\t'  ||   c == '\v'  ||  c == '\b'                      ||
            c == '\r'  ||   c == '\f'  ||  c == '\a'                      ||
            c == '\n'  ||   c == '\\'  ||  c == '\''                      ||
            c == '"'   ||  (c == '&'   &&  lang == eLanguage_Javascript)  ||
            !isprint((unsigned char) c) ? true : false);
}


static string s_PrintableString(const CTempString&   str,
                                NStr::TPrintableMode mode,
                                ELanguage            lang)
{
    auto_ptr<CNcbiOstrstream> out;
    SIZE_TYPE i, j = 0;

    for (i = 0;  i < str.size();  i++) {
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
            if (isprint((unsigned char) c))
                continue;
            break;
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
        } else if (!isprint((unsigned char) c)) {
            bool reduce;
            if (!(mode & NStr::fPrintable_Full)) {
                reduce = (i == str.size() - 1  ||  s_IsQuoted(str[i + 1], lang)
                          ||  str[i + 1] < '0'  ||  str[i + 1] > '7');
            } else {
                reduce = false;
            }
            unsigned char v;
            char octal[3];
            int k = 0;
            v =  (unsigned char) c >> 6;
            if (v  ||  !reduce) {
                octal[k++] = '0' + v;
                reduce = false;
            }
            v = ((unsigned char) c >> 3) & 7;
            if (v  ||  !reduce) {
                octal[k++] = '0' + v;
            }
            v =  (unsigned char) c & 7;
            octal    [k++] = '0' + v;
            out->write(octal, k);
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

    // All characters are good - return original string
    return str;
}

        
string NStr::PrintableString(const CTempString&   str,
                             NStr::TPrintableMode mode)
{
    return s_PrintableString(str, mode, eLanguage_C);
}


string NStr::JavaScriptEncode(const CTempString& str)
{
    return s_PrintableString(str, eNewLine_Quote, eLanguage_Javascript);
}

string NStr::XmlEncode(const CTempString& str)
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


string NStr::ParseEscapes(const CTempString& str)
{
    string out;
    out.reserve(str.size()); // can only be smaller
    SIZE_TYPE pos = 0;

    while (pos < str.size()) {
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
                    //~ out += static_cast<char>
                        //~     (StringToUInt(str.substr(pos2, pos - pos2), 0, 16));
                    out += static_cast<char>
                        (StringToUInt(CTempString(str, pos2, pos - pos2), 0, 16));
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
            /*quoted EOL means no EOL*/
            break;
        default:
            out += str[pos2];
            break;
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
static const char s_EncodeURIUserinfo[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "!",   "%22", "%23", "$",   "%25", "&",   "'",
    "(",   ")",   "*",   "+",   ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   ":",   ";",   "%3C", "=",   "%3E", "%3F",
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
    dst.reserve(dst_len + 1);
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
    dst[dst_len] = '\0';
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
        dst.clear();
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
        dst[pdst] = '\0';
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

    for (;;) {
        char* e;
        if ( !isdigit((unsigned char)(*c)) )
            return false;
        errno = 0;
        val = strtoul(c, &e, 10);
        if (c == e  ||  errno)
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
//  CStringUTF8_Helper
#if !defined(__NO_EXPORT_STRINGUTF8__)
#  define CStringUTF8_Helper  CStringUTF8
#endif

#if defined(__NO_EXPORT_STRINGUTF8__)
SIZE_TYPE CStringUTF8_Helper::GetSymbolCount( const CTempString& str)
#else
SIZE_TYPE CStringUTF8::GetSymbolCount( const CTempString& str)
#endif
{
    SIZE_TYPE count = 0;
    CTempString::const_iterator src = str.begin();
    CTempString::const_iterator to = str.end();
    for (; src != to; ++src, ++count) {
        SIZE_TYPE more = 0;
        bool good = x_EvalFirst(*src, more);
        while (more-- && good) {
            good = x_EvalNext(*(++src));
        }
        if ( !good ) {
            NCBI_THROW2(CStringException, eFormat,
                        "String is not in UTF8 format",
                        (src - str.begin()));
        }
    }
    return count;
}

SIZE_TYPE CStringUTF8_Helper::GetValidSymbolCount(const CTempString& src, SIZE_TYPE buf_size)
{
    SIZE_TYPE count = 0, cur_size=0;
    CTempString::const_iterator i = src.begin();
    CTempString::const_iterator end = src.end();
    for (; cur_size < buf_size && i != end; ++i, ++count, ++cur_size) {
        SIZE_TYPE more = 0;
        bool good = x_EvalFirst(*i, more);
        while (more-- && good && ++cur_size < buf_size) {
            good = x_EvalNext(*(++i));
        }
        if ( !good ) {
            return count;
        }
    }
    return count;
}


SIZE_TYPE CStringUTF8_Helper::GetValidBytesCount(const CTempString& src, SIZE_TYPE buf_size)
{
    SIZE_TYPE count = 0;
    SIZE_TYPE cur_size = 0;
    CTempString::const_iterator i = src.begin();
    CTempString::const_iterator end = src.end();

    for (; cur_size < buf_size && i != end; ++i, ++count, ++cur_size) {
        SIZE_TYPE more = 0;
        bool good = x_EvalFirst(*i, more);
        while (more-- && good && cur_size < buf_size) {
            good = x_EvalNext(*(++i));
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


#if defined(__NO_EXPORT_STRINGUTF8__)
string CStringUTF8_Helper::AsSingleByteString(
    const CStringUTF8& self,EEncoding encoding, const char* substitute_on_error)
#else
string CStringUTF8::AsSingleByteString(
    EEncoding encoding, const char* substitute_on_error) const
#endif
{
    string result;
#if defined(__NO_EXPORT_STRINGUTF8__)
    result.reserve( GetSymbolCount(self)+1 );
#else
    result.reserve( GetSymbolCount()+1 );
    const CStringUTF8& self(*this);
#endif
    CStringUTF8::const_iterator src = self.begin();
    CStringUTF8::const_iterator to = self.end();
    for ( ; src != to; ++src ) {
        TUnicodeSymbol sym = CStringUTF8::Decode( src );
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


EEncoding CStringUTF8_Helper::GuessEncoding( const CTempString& src)
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
    } else if (utf8) {
        return eEncoding_UTF8;
    } else if (cp1252) {
        return iso1 ? eEncoding_ISO8859_1 : eEncoding_Windows_1252;
    }
    return eEncoding_Unknown;
}


bool CStringUTF8_Helper::MatchEncoding( const CTempString& src, EEncoding encoding)
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


TUnicodeSymbol CStringUTF8_Helper::CharToSymbol(char c, EEncoding encoding)
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


char CStringUTF8_Helper::SymbolToChar(TUnicodeSymbol cp, EEncoding encoding)
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


#if defined(__NO_EXPORT_STRINGUTF8__)
void CStringUTF8_Helper::x_Validate(const CStringUTF8& self)
#else
void CStringUTF8::x_Validate(void) const
#endif
{
#if !defined(__NO_EXPORT_STRINGUTF8__)
    const CStringUTF8& self(*this);
#endif
    if (!self.IsValid()) {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
    }
}


#if defined(__NO_EXPORT_STRINGUTF8__)
void CStringUTF8_Helper::x_AppendChar( CStringUTF8& self, TUnicodeSymbol c)
#else
void CStringUTF8::x_AppendChar( TUnicodeSymbol c)
#endif
{
#if !defined(__NO_EXPORT_STRINGUTF8__)
    CStringUTF8& self(*this);
#endif
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
}


#if defined(__NO_EXPORT_STRINGUTF8__)
void CStringUTF8_Helper::x_Append(
    CStringUTF8& self,
    const CTempString& src, EEncoding encoding, EValidate validate)
#else
void CStringUTF8::x_Append(
    const CTempString& src, EEncoding encoding, EValidate validate)
#endif
{
#if !defined(__NO_EXPORT_STRINGUTF8__)
    CStringUTF8& self(*this);
#endif
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
        return;
    }

    SIZE_TYPE needed = 0;
    CTempString::const_iterator i;
    CTempString::const_iterator end = src.end();
    for (i = src.begin(); i != end; ++i) {
        needed += x_BytesNeeded( CharToSymbol( *i,encoding ) );
    }
    if ( !needed ) {
        return;
    }
    self.reserve(max(self.capacity(),self.length()+needed+1));
    for (i = src.begin(); i != end; ++i) {
#if defined(__NO_EXPORT_STRINGUTF8__)
        x_AppendChar( self, CharToSymbol( *i, encoding ) );
#else
        x_AppendChar(       CharToSymbol( *i, encoding ) );
#endif
    }
}

SIZE_TYPE CStringUTF8_Helper::x_BytesNeeded(TUnicodeSymbol c)
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


bool CStringUTF8_Helper::x_EvalFirst(char ch, SIZE_TYPE& more)
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


bool CStringUTF8_Helper::x_EvalNext(char ch)
{
    return (ch & 0xC0) == 0x80;
}

TUnicodeSymbol CStringUTF8_Helper::DecodeFirst(char ch, SIZE_TYPE& more)
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


TUnicodeSymbol CStringUTF8_Helper::DecodeNext(TUnicodeSymbol chU, char ch)
{
    if ((ch & 0xC0) == 0x80) {
        return (chU << 6) | (ch & 0x3F);
    } else {
        NCBI_THROW2(CStringException, eBadArgs,
            "Source string is not in UTF8 format", 0);
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



END_NCBI_NAMESPACE;
