#ifndef CORELIB___NCBISTR__HPP
#define CORELIB___NCBISTR__HPP

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
 *
 */

/// @file ncbistr.hpp
/// The NCBI C++ standard methods for dealing with std::string


#include <corelib/tempstr.hpp>
#ifdef NCBI_OS_OSF1
#  include <strings.h>
#endif
#include <time.h>
#include <stdarg.h>
#include <vector>



BEGIN_NCBI_SCOPE

/** @addtogroup String
 *
 * @{
 */

/// Empty "C" string (points to a '\0').
NCBI_XNCBI_EXPORT extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


/// Empty "C++" string.
#if defined(NCBI_OS_MSWIN) || ( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
class CNcbiEmptyString
{
public:
    /// Get string.
    static const string& Get(void)
    {
        static string empty_str;
        return empty_str;
    }
};
#else
class NCBI_XNCBI_EXPORT CNcbiEmptyString
{
public:
    /// Get string.
    static const string& Get(void);
private:
    /// Helper method to initialize private data member and return
    /// null string.
    static const string& FirstGet(void);
    static const string* m_Str;     ///< Null string pointer.
};
#endif // NCBI_OS_MSWIN....


#define NcbiEmptyString NCBI_NS_NCBI::CNcbiEmptyString::Get()
#define kEmptyStr NcbiEmptyString


// SIZE_TYPE and NPOS

/// Define size type.
typedef NCBI_NS_STD::string::size_type SIZE_TYPE;

/// Define NPOS constant as the special value "std::string::npos" which is
/// returned when a substring search fails, or to indicate an unspecified
/// string position.
static const SIZE_TYPE NPOS = NCBI_NS_STD::string::npos;



/////////////////////////////////////////////////////////////////////////////
///
/// NStr --
///
/// Encapuslates class-wide string processing functions.

class NCBI_XNCBI_EXPORT NStr
{
public:
    /// Convert string to numeric value.
    ///
    /// @param str
    ///   String containing digits.
    /// @return
    ///   - Convert "str" to a (non-negative) "int" value and return
    ///     this value.
    ///   - -1 if "str" contains any symbols other than [0-9], or
    ///     if it represents a number that does not fit into "int".
    static int StringToNumeric(const string& str);


    /// Number to string conversion flags.
    ///
    /// NOTE: 
    ///   If specified base in the *ToString() methods is not default 10,
    ///   that some flags like fWithSign and fWithCommas will be ignored.
    enum ENumToStringFlags {
        fWithSign        = (1 <<  9), ///< Prefix the output value with a sign
        fWithCommas      = (1 << 10), ///< Use commas as thousands separator
        fDoubleFixed     = (1 << 11), ///< Use n.nnnn format for double
        fDoubleScientific= (1 << 12), ///< Use scientific format for double
        fDoubleGeneral   = fDoubleFixed | fDoubleScientific
    };
    typedef int TNumToStringFlags;    ///< Bitwise OR of "ENumToStringFlags"

    /// String to number conversion flags.
    enum EStringToNumFlags {
        fConvErr_NoThrow      = (1 <<  9),   ///< Return "natural null"
        // value on error, instead of throwing (by default) an exception
        
        fMandatorySign        = (1 << 10),   ///< See 'fWithSign'
        fAllowCommas          = (1 << 11),   ///< See 'fWithCommas'
        fAllowLeadingSpaces   = (1 << 12),   ///< Can have leading spaces
        fAllowLeadingSymbols  = (1 << 13) | fAllowLeadingSpaces,
                                             ///< Can have leading non-nums
        fAllowTrailingSpaces  = (1 << 14),   ///< Can have trailing spaces
        fAllowTrailingSymbols = (1 << 15) | fAllowTrailingSpaces,
                                             ///< Can have leading non-nums
        fAllStringToNumFlags  = 0x7F00
    };
    typedef int TStringToNumFlags;   ///< Binary OR of "EStringToNumFlags"

    /// Convert string to int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static int StringToInt(const CTempString& str,
                           TStringToNumFlags  flags = 0,
                           int                base  = 10);

    /// Convert string to unsigned int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "unsigned int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static unsigned int StringToUInt(const CTempString& str,
                                     TStringToNumFlags  flags = 0,
                                     int                base  = 10);

    /// Convert string to long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static long StringToLong(const CTempString& str,
                             TStringToNumFlags  flags = 0,
                             int                base  = 10);

    /// Convert string to unsigned long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @return
    ///   - Convert "str" to "unsigned long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static unsigned long StringToULong(const CTempString& str,
                                       TStringToNumFlags  flags = 0,
                                       int                base  = 10);

    /// Convert string to double.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    ///   Do not support fAllowCommas flag.
    /// @return
    ///   - Convert "str" to "double" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static double StringToDouble(const CTempString& str,
                                 TStringToNumFlags  flags = 0);

    /// Convert string to Int8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "Int8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Int8 StringToInt8(const CTempString& str,
                             TStringToNumFlags  flags = 0,
                             int                base  = 10);

    /// Convert string to Uint8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "UInt8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Uint8 StringToUInt8(const CTempString& str,
                               TStringToNumFlags  flags = 0,
                               int                base  = 10);

    /// Convert string to number of bytes. 
    ///
    /// String can contain "software" qualifiers: MB(megabyte), KB (kilobyte)..
    /// Example: 100MB, 1024KB
    /// Note the qualifiers are power-of-2 based, aka kibi-, mebi- etc, so that
    /// 1KB = 1024B (not 1000B), 1MB = 1024KB = 1048576B, etc.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Numeric base of the number (before the qualifier).
    ///   Default is 10. Allowed values are 0, 2..20.
    /// @return
    ///   - Convert "str" to "Uint8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Uint8 StringToUInt8_DataSize(const CTempString& str,
                                        TStringToNumFlags  flags = 0,
                                        int                base  = 10);

    /// Convert string to pointer.
    ///
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Pointer value corresponding to its string representation.
    static const void* StringToPtr(const string& str);

    /// Convert character to integer.
    ///
    /// @param ch
    ///   Character to be converted.
    /// @return
    ///   Integer (0..15) corresponding to the "ch" as a hex digit.
    ///   Return -1 on error.
    static int HexChar(char ch);

    /// Convert Int to String.
    ///
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string IntToString(long value, TNumToStringFlags flags = 0,
                              int  base = 10);

    /// Convert Int to String.
    ///
    /// @param out_str
    ///   Output string variable.
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void IntToString(string& out_str, long value, 
                            TNumToStringFlags flags = 0,
                            int               base  = 10);

    /// Convert UInt to string.
    ///
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string UIntToString(unsigned long value,
                               TNumToStringFlags flags = 0,
                               int               base  = 10);

    /// Convert UInt to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void UIntToString(string& out_str, unsigned long value,
                             TNumToStringFlags flags = 0,
                             int               base  = 10);

    /// Convert Int8 to string.
    ///
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string Int8ToString(Int8 value,
                               TNumToStringFlags flags = 0,
                               int               base  = 10);

    /// Convert Int8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void Int8ToString(string& out_str, Int8 value,
                             TNumToStringFlags flags = 0,
                             int               base  = 10);

    /// Convert UInt8 to string.
    ///
    /// @param value
    ///   Integer value (UInt8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string UInt8ToString(Uint8 value,
                                TNumToStringFlags flags = 0,
                                int               base  = 10);

    /// Convert UInt8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (UInt8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void UInt8ToString(string& out_str, Uint8 value,
                              TNumToStringFlags flags = 0,
                              int               base  = 10);

    /// Convert double to string.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    //    If it is negative, that double will be converted to number in
    ///   scientific notation.
    /// @param flags
    ///   How to convert value to string.
    ///   If double format flags are not specified, that next output format
    ///   will be used by default:
    ///     - fDoubleFixed,   if 'precision' >= 0.
    ///     - fDoubleGeneral, if 'precision' < 0.
    /// @return
    ///   Converted string value.
    static string DoubleToString(double value, int precision = -1,
                                 TNumToStringFlags flags = 0);

    /// Convert double to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    //    If it is negative, that double will be converted to number in
    ///   scientific notation.
    /// @param flags
    ///   How to convert value to string.
    ///   If double format flags are not specified, that next output format
    ///   will be used by default:
    ///     - fDoubleFixed,   if 'precision' >= 0.
    ///     - fDoubleGeneral, if 'precision' < 0.
    static void DoubleToString(string& out_str, double value,
                               int precision = -1,
                               TNumToStringFlags flags = 0);

    /// Convert double to string with specified precision and place the result
    /// in the specified buffer.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    /// @param buf
    ///   Put result of the conversion into this buffer.
    /// @param buf_size
    ///   Size of buffer, "buf".
    /// @param flags
    ///   How to convert value to string.
    ///   Default output format is fDoubleFixed.
    /// @return
    ///   The number of bytes stored in "buf", not counting the
    ///   terminating '\0'.
    static SIZE_TYPE DoubleToString(double value, unsigned int precision,
                                    char* buf, SIZE_TYPE buf_size,
                                    TNumToStringFlags flags = 0);

    /// Convert pointer to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param str
    ///   Pointer to be converted.
    static void PtrToString(string& out_str, const void* ptr);

    /// Convert pointer to string.
    ///
    /// @param str
    ///   Pointer to be converted.
    /// @return
    ///   String value representing the pointer.
    static string PtrToString(const void* ptr);

    /// Convert bool to string.
    ///
    /// @param value
    ///   Boolean value to be converted.
    /// @return
    ///   One of: 'true, 'false'
    static const string BoolToString(bool value);

    /// Convert string to bool.
    ///
    /// @param str
    ///   Boolean string value to be converted.  Can recognize
    ///   case-insensitive version as one of:  'true, 't', 'yes', 'y'
    ///   for TRUE; and  'false', 'f', 'no', 'n' for FALSE.
    /// @return
    ///   TRUE or FALSE.
    static bool StringToBool(const string& str);


    /// Handle an arbitrary printf-style format string.
    ///
    /// This method exists only to support third-party code that insists on
    /// representing messages in this format; please stick to type-checked
    /// means of formatting such as the above ToString methods and I/O
    /// streams whenever possible.
    static string FormatVarargs(const char* format, va_list args);


    /// Which type of string comparison.
    enum ECase {
        eCase,      ///< Case sensitive compare
        eNocase     ///< Case insensitive compare
    };

    // ATTENTION.  Be aware that:
    //
    // 1) "Compare***(..., SIZE_TYPE pos, SIZE_TYPE n, ...)" functions
    //    follow the ANSI C++ comparison rules a la "basic_string::compare()":
    //       str[pos:pos+n) == pattern   --> return 0
    //       str[pos:pos+n) <  pattern   --> return negative value
    //       str[pos:pos+n) >  pattern   --> return positive value
    //
    // 2) "strn[case]cmp()" functions follow the ANSI C comparison rules:
    //       str[0:n) == pattern[0:n)   --> return 0
    //       str[0:n) <  pattern[0:n)   --> return negative value
    //       str[0:n) >  pattern[0:n)   --> return positive value


    /// Case-sensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded CompareCase() with differences in argument
    ///   types: char* vs. string&
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);

    /// Case-sensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded CompareCase() with differences in argument
    ///   types: char* vs. string&
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);

    /// Case-sensitive compare of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with same argument types.
    static int CompareCase(const char* s1, const char* s2);

    /// Case-sensitive compare of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with same argument types.
    static int CompareCase(const string& s1, const string& s2);

    /// Case-insensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern (case-insensitive compare).   
    ///   - Negative integer, if str[pos:pos+n) <  pattern (case-insensitive
    ///     compare).
    ///   - Positive integer, if str[pos:pos+n) >  pattern (case-insensitive
    ///     compare).
    /// @sa
    ///   Other forms of overloaded CompareNocase() with differences in
    ///   argument types: char* vs. string&
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);

    /// Case-insensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern (case-insensitive compare).   
    ///   - Negative integer, if str[pos:pos+n) <  pattern (case-insensitive
    ///     compare).
    ///   - Positive integer, if str[pos:pos+n) >  pattern (case-insensitive
    ///     compare).
    /// @sa
    ///   Other forms of overloaded CompareNocase() with differences in
    ///   argument types: char* vs. string&
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);

    /// Case-insensitive compare of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2 (case-insensitive compare).      
    ///   - Negative integer, if s1 < s2 (case-insensitive compare).      
    ///   - Positive integer, if s1 > s2 (case-insensitive compare).    
    /// @sa
    ///   CompareCase(), Compare() versions with same argument types.
    static int CompareNocase(const char* s1, const char* s2);

    /// Case-insensitive compare of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2 (case-insensitive compare).      
    ///   - Negative integer, if s1 < s2 (case-insensitive compare).      
    ///   - Positive integer, if s1 > s2 (case-insensitive compare).    
    /// @sa
    ///   CompareCase(), Compare() versions with same argument types.
    static int CompareNocase(const string& s1, const string& s2);

    /// Compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Compare() with differences in argument
    ///   types: char* vs. string&
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);

    /// Compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Compare() with differences in argument
    ///   types: char* vs. string&
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);

    /// Compare two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const char* s1, const char* s2,
                       ECase use_case = eCase);

    /// Compare two strings -- string&, char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const string& s1, const char* s2,
                       ECase use_case = eCase);

    /// Compare two strings -- char*, string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const char* s1, const string& s2,
                       ECase use_case = eCase);

    /// Compare two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const string& s1, const string& s2,
                       ECase use_case = eCase);

    /// Case-sensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise
    /// @sa
    ///   Other forms of overloaded EqualCase() with differences in argument
    ///   types: char* vs. string&
    static bool EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);

    /// Case-sensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise
    /// @sa
    ///   Other forms of overloaded EqualCase() with differences in argument
    ///   types: char* vs. string&
    static bool EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);

    /// Case-sensitive equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2
    ///   - false, otherwise
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualCase(const char* s1, const char* s2);

    /// Case-sensitive equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2
    ///   - false, otherwise
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualCase(const string& s1, const string& s2);

    /// Case-insensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern (case-insensitive compare).
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded EqualNocase() with differences in
    ///   argument types: char* vs. string&
    static bool EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);

    /// Case-insensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern (case-insensitive compare).
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded EqualNocase() with differences in
    ///   argument types: char* vs. string&
    static bool EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);

    /// Case-insensitive equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2 (case-insensitive compare).      
    ///   - false, otherwise.
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualNocase(const char* s1, const char* s2);

    /// Case-insensitive equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2 (case-insensitive compare).      
    ///   - false, otherwise.
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualNocase(const string& s1, const string& s2);

    /// Test for equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded Equal() with differences in argument
    ///   types: char* vs. string&
    static bool Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);

    /// Test for equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Equal() with differences in argument
    ///   types: char* vs. string&
    static bool Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);

    /// Test for equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const char* s1, const char* s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- string&, char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const string& s1, const char* s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- char*, string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const char* s1, const string& s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const string& s1, const string& s2,
                       ECase use_case = eCase);

    // NOTE.  On some platforms, "strn[case]cmp()" can work faster than their
    //        "Compare***()" counterparts.

    /// String compare.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strncmp(), strcasecmp(), strncasecmp()
    static int strcmp(const char* s1, const char* s2);

    /// String compare upto specified number of characters.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param n
    ///   Number of characters in string 
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strcasecmp(), strncasecmp()
    static int strncmp(const char* s1, const char* s2, size_t n);

    /// Case-insensitive string compare.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strncmp(), strncasecmp()
    static int strcasecmp(const char* s1, const char* s2);

    /// Case-insensitive string compare upto specfied number of characters.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strcasecmp(), strcasecmp()
    static int strncasecmp(const char* s1, const char* s2, size_t n);

    /// Wrapper for the function strftime() that corrects handling %D and %T
    /// time formats on MS Windows.
    static size_t strftime (char* s, size_t maxsize, const char* format,
                            const struct tm* timeptr);

    /// Match "str" against the "mask".
    ///
    /// This function do not use regular expressions.
    /// @param str
    ///   String to match.
    /// @param mask
    ///   Mask used to match string "str". And can contains next
    ///   wildcard characters:
    ///     ? - matches to any one symbol in the string.
    ///     * - matches to any number of symbols in the string. 
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const char *str, const char *mask,
                            ECase use_case = eCase);

    /// Match "str" against the "mask".
    ///
    /// This function do not use regular expressions.
    /// @param str
    ///   String to match.
    /// @param mask
    ///   Mask used to match string "str". And can contains next
    ///   wildcard characters:
    ///     ? - matches to any one symbol in the string.
    ///     * - matches to any number of symbols in the string. 
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const string& str, const string& mask,
                            ECase use_case = eCase);

    // The following 4 methods change the passed string, then return it

    /// Convert string to lower case -- string& version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Lower cased string.
    static string& ToLower(string& str);

    /// Convert string to lower case -- char* version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Lower cased string.
    static char* ToLower(char*   str);

    /// Convert string to upper case -- string& version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Upper cased string.
    static string& ToUpper(string& str);

    /// Convert string to upper case -- char* version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Upper cased string.
    static char* ToUpper(char*   str);

private:
    /// Privatized ToLower() with const char* parameter to prevent passing of 
    /// constant strings.
    static void/*dummy*/ ToLower(const char* /*dummy*/);

    /// Privatized ToUpper() with const char* parameter to prevent passing of 
    /// constant strings.
    static void/*dummy*/ ToUpper(const char* /*dummy*/);

public:
    /// Check if a string starts with a specified prefix value.
    ///
    /// @param str
    ///   String to check.
    /// @param start
    ///   Prefix value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool StartsWith(const string& str, const string& start,
                           ECase use_case = eCase);

    /// Check if a string starts with a specified character value.
    ///
    /// @param str
    ///   String to check.
    /// @param start
    ///   Character value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool StartsWith(const string& str, char start,
                           ECase use_case = eCase);

    /// Check if a string ends with a specified suffix value.
    ///
    /// @param str
    ///   String to check.
    /// @param end
    ///   Suffix value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool EndsWith(const string& str, const string& end,
                         ECase use_case = eCase);

    /// Check if a string ends with a specified character value.
    ///
    /// @param str
    ///   String to check.
    /// @param end
    ///   Character value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool EndsWith(const string& str, char end,
                         ECase use_case = eCase);

    /// Check if a string is blank (has no text).
    ///
    /// @param str
    ///   String to check.
    /// @param pos
    ///   starting position (default 0)
    static bool IsBlank(const string& str, SIZE_TYPE pos = 0);

    /// Whether it is the first or last occurrence.
    enum EOccurrence {
        eFirst,             ///< First occurrence
        eLast               ///< Last occurrence
    };

    /// Find the pattern in the specfied range of a string.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while searching for the pattern.
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE Find(const string& str, const string& pattern,
                          SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                          EOccurrence which = eFirst,
                          ECase use_case = eCase);

    /// Find the pattern in the specfied range of a string using a case
    /// sensitive search.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE FindCase  (const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);

    /// Find the pattern in the specfied range of a string using a case
    /// insensitive search.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE FindNoCase(const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);

    /// Which end to truncate a string.
    enum ETrunc {
        eTrunc_Begin,  ///< Truncate leading spaces only
        eTrunc_End,    ///< Truncate trailing spaces only
        eTrunc_Both    ///< Truncate spaces at both begin and end of string
    };

    /// Truncate spaces in a string.
    ///
    /// @param str
    ///   String to truncate spaces from.
    /// @param where
    ///   Which end of the string to truncate space from. Default is to
    ///   truncate space from both ends (eTrunc_Both).
    static string TruncateSpaces(const string& str, ETrunc where=eTrunc_Both);
    static CTempString TruncateSpaces(const CTempString& str,
                                      ETrunc where=eTrunc_Both);
    static CTempString TruncateSpaces(const char* str,
                                      ETrunc where=eTrunc_Both);

    /// Truncate spaces in a string (in-place)
    ///
    /// @param str
    ///   String to truncate spaces from.
    /// @param where
    ///   Which end of the string to truncate space from. Default is to
    ///   truncate space from both ends (eTrunc_Both).
    static void TruncateSpacesInPlace(string& str, ETrunc where=eTrunc_Both);
    
    /// Replace occurrences of a substring within a string.
    ///
    /// @param src
    ///   Source string from which specified substring occurrences are
    ///   replaced.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param dst
    ///   Result of replacing the "search" string with "replace" in "src".
    ///   This value is also returned by the function.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   Result of replacing the "search" string with "replace" in "src". This
    ///   value is placed in "dst" as well.
    /// @sa
    ///   Version of Replace() that returns a new string.
    static string& Replace(const string& src,
                           const string& search,
                           const string& replace,
                           string&       dst,
                           SIZE_TYPE     start_pos = 0,
                           SIZE_TYPE     max_replace = 0);

    /// Replace occurrences of a substring within a string and returns the
    /// result as a new string.
    ///
    /// @param src
    ///   Source string from which specified substring occurrences are
    ///   replaced.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   A new string containing the result of replacing the "search" string
    ///   with "replace" in "src"
    /// @sa
    ///   Version of Replace() that has a destination parameter to accept
    ///   result.
    static string Replace(const string& src,
                          const string& search,
                          const string& replace,
                          SIZE_TYPE     start_pos = 0,
                          SIZE_TYPE     max_replace = 0);

    /// Replace occurrences of a substring within a string.
    ///
    /// On some platforms this function is much faster than Replace()
    /// if sizes of "search" and "replace" strings are equal.
    /// Otherwise, the performance is mainly the same.
    /// @param src
    ///   String whre specified substring occurrences are replaced.
    ///   This value is also returned by the function.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   Result of replacing the "search" string with "replace" in "src".
    /// @sa
    ///   Replace
    static string& ReplaceInPlace(string& src,
                                  const string& search,
                                  const string& replace,
                                  SIZE_TYPE     start_pos = 0,
                                  SIZE_TYPE     max_replace = 0);

    /// Whether to merge adjacent delimiters in Split and Tokenize.
    enum EMergeDelims {
        eNoMergeDelims,     ///< No merging of delimiters -- default for
                            ///< Tokenize()
        eMergeDelims        ///< Merge the delimiters -- default for Split()
    };


    /// Split a string using specified delimiters.
    ///
    /// @param str
    ///   String to be split.
    /// @param delim
    ///   Delimiters used to split string "str".
    /// @param arr
    ///   The split tokens are added to the list "arr" and also returned
    ///   by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eMergeDelims means that delimiters that immediately follow each other
    ///   are treated as one delimiter.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Tokenize()
    static list<string>& Split(const string& str,
                               const string& delim,
                               list<string>& arr,
                               EMergeDelims  merge = eMergeDelims,
                               vector<SIZE_TYPE>* token_pos = NULL);

    /// Tokenize a string using the specified set of char delimiters.
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Set of char delimiters used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param arr
    ///   The tokens defined in "str" by using symbols from "delim" are added
    ///   to the list "arr" and also returned by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eNoMergeDelims means that delimiters that immediately follow each
    ///    other are treated as separate delimiters.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Split, TokenizePattern, TokenizeInTwo
    static vector<string>& Tokenize(const string&      str,
                                    const string&      delim,
                                    vector<string>&    arr,
                                    EMergeDelims       merge = eNoMergeDelims,
                                    vector<SIZE_TYPE>* token_pos = NULL);

    /// Tokenize a string using the specified delimiter (string).
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Delimiter used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param arr
    ///   The tokens defined in "str" by using delimeter "delim" are added
    ///   to the list "arr" and also returned by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eNoMergeDelims means that delimiters that immediately follow each
    ///   other are treated as separate delimiters.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Split, Tokenize
    static
    vector<string>& TokenizePattern(const string&      str,
                                    const string&      delim,
                                    vector<string>&    arr,
                                    EMergeDelims       merge = eNoMergeDelims,
                                    vector<SIZE_TYPE>* token_pos = NULL);

    /// Split a string into two pieces using the specified delimiters
    ///
    /// @param str 
    ///   String to be split.
    /// @param delim
    ///   Delimiters used to split string "str".
    /// @param str1
    ///   The sub-string of "str" before the first character of "delim".
    ///   It will not contain any characters in "delim".
    ///   Will be empty if "str" begin with a "delim" character.
    /// @param str2
    ///   The sub-string of "str" after the first character of "delim" found.
    ///   May contain "delim" characters.
    ///   Will be empty if "str" had no "delim" characters or ended
    ///   with the first "delim" charcter.
    /// @return
    ///   true if a symbol from "delim" was found in "str", false if not.
    ///   This lets you distinguish when there were no delimiters and when
    ///   the very last character was the first delimiter.
    /// @sa
    ///   Split, Tokenoze, TokenizePattern
    static bool SplitInTwo(const string& str, 
                           const string& delim,
                           string&       str1,
                           string&       str2);
                         

    /// Join strings using the specified delimiter.
    ///
    /// @param arr
    ///   Array of strings to be joined.
    /// @param delim
    ///   Delimiter used to join the string.
    /// @return 
    ///   The strings in "arr" are joined into a single string, separated
    ///   with "delim".
    static string Join(const list<string>& arr,   const string& delim);
    static string Join(const vector<string>& arr, const string& delim);

    /// How to display printable strings.
    ///
    /// Assists in making a printable version of "str".
    enum EPrintableMode {
        fNewLine_Quote    = 0,   ///< Display "\n" instead of actual linebreak
        eNewLine_Quote    = fNewLine_Quote,
        fNewLine_Passthru = 1,   ///< Break the line at every "\n" occurrence
        eNewLine_Passthru = fNewLine_Passthru,
        fPrintable_Full   = 2    ///< Show all octal digits at all times
    };
    typedef int TPrintableMode;  ///< Bitwise OR of EPrintableMode flags

    /// Get a printable version of the specified string. 
    ///
    /// All non-printable characters will be represented as "\r", "\n", "\v",
    /// "\t", "\"", "\\", etc, or "\ooo" where 'ooo' is the octal code of the
    /// character.  The resultant string is a well-formed C string literal,
    /// which, without alterations, can be compiled by a C/C++ compiler.
    /// In many instances, octal representations of non-printable characters
    /// can be reduced to take less than all 3 digits, if there is no
    /// ambiguity in the interpretation.  fPrintable_Full cancels the
    /// reduction, and forces to produce full 3-digit octal codes throughout.
    ///
    /// @param str
    ///   The string whose printable version is wanted.
    /// @param mode
    ///   How to display the string.  The default setting of fNewLine_Quote
    ///   displays the new lines as "\n", and uses the octal code reduction.
    ///   When set to fNewLine_Passthru, line breaks are actually
    ///   produced on output but preceded with trailing backslashes.
    /// @return
    ///   Return a printable version of "str".
    /// @sa
    ///   ParseEscapes
    static string PrintableString(const string&  str,
                                  TPrintableMode mode = eNewLine_Quote);

    /// Parse C-style escape sequences in the specified string, including
    /// all those produced by PrintableString.
    static string ParseEscapes(const string& str);

    /// How to wrap the words in a string to a new line.
    enum EWrapFlags {
        fWrap_Hyphenate  = 0x1, ///< Add a hyphen when breaking words?
        fWrap_HTMLPre    = 0x2, ///< Wrap as preformatted HTML?
        fWrap_FlatFile   = 0x4  ///< Wrap for flat file use.
    };
    typedef int TWrapFlags;     ///< Binary OR of "EWrapFlags"

    /// Encode a string for C/C++.
    ///
    /// Synonym for PrintableString().
    /// @sa PrintableString
    static string CEncode(const string& str);

    /// Encode a string for JavaScript.
    ///
    /// Like to CEncode(), but process some symbols in different way.
    /// @sa PrintableString, CEncode
    static string JavaScriptEncode(const string& str);

    /// Encode a string for XML.
    ///
    /// Replace relevant characters by predefined entities.
    static string XmlEncode(const string& str);

    /// URL-encode flags
    enum EUrlEncode {
        eUrlEnc_SkipMarkChars,    ///< Do not convert chars like '!', '(' etc.
        eUrlEnc_ProcessMarkChars, ///< Convert all non-alphanum chars,
                                  ///< spaces are converted to '+'
        eUrlEnc_PercentOnly,      ///< Convert all non-alphanum chars including
                                  ///< space and '%' to %## format
        eUrlEnc_Path              ///< Same as ProcessMarkChars but preserves
                                  ///< valid path characters ('/', '.')
    };
    /// URL decode flags
    enum EUrlDecode {
        eUrlDec_All,              ///< Decode '+' to space
        eUrlDec_Percent           ///< Decode only %XX
    };
    /// URL-encode string
    static string URLEncode(const string& str,
                            EUrlEncode flag = eUrlEnc_SkipMarkChars);
    /// URL-decode string
    static string URLDecode(const string& str,
                            EUrlDecode flag = eUrlDec_All);
    /// URL-decode string to itself
    static void URLDecodeInPlace(string& str,
                                 EUrlDecode flag = eUrlDec_All);

    /// Wrap the specified string into lines of a specified width -- prefix,
    /// prefix1 default version.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0(default), do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags = 0,
                              const string* prefix = 0,
                              const string* prefix1 = 0);

    /// Wrap the specified string into lines of a specified width -- prefix1
    /// default version.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the wrapped
    ///   lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string* prefix1 = 0);

    /// Wrap the specified string into lines of a specified width.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the wrapped
    ///   lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0, do not add a prefix string to the first
    ///   line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string& prefix1);


    /// Wrap the list using the specified criteria -- default prefix, 
    /// prefix1 version.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0(default), do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrapped list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags = 0,
                                  const string* prefix = 0,
                                  const string* prefix1 = 0);

    /// Wrap the list using the specified criteria -- default prefix1 version.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrappe list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string* prefix1 = 0);
        
    /// Wrap the list using the specified criteria.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0, do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrapped list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string& prefix1);
}; // class NStr



/////////////////////////////////////////////////////////////////////////////
///
/// CStringUTF8 --
///
///   An UTF-8 string.
///   Stores character data in UTF-8 encoding form.
///   Being initialized, converts source characters into UTF-8.
///   Can convert data back into a particular encoding form (non-UTF8)
///   Supported encodings:
///      ISO 8859-1 (Latin1)
///      Microsoft Windows code page 1252
///      UCS-2/UTF-16 (no surrogates)

enum EEncoding {
    eEncoding_Unknown,
    eEncoding_UTF8,
    eEncoding_Ascii,
    eEncoding_ISO8859_1,
    eEncoding_Windows_1252
};
typedef Uint4 TUnicodeSymbol;

class NCBI_XNCBI_EXPORT CStringUTF8 : public string
{
public:
    enum EValidate {
        eNoValidate,
        eValidate
    };

    /// Default constructor.
    CStringUTF8(void)
    {
    }

    /// Destructor.
    ~CStringUTF8(void)
    {
    }

    /// Copy constructor.
    ///
    /// @param src
    ///   Source UTF-8 string
    /// @param validate
    ///   Verify that the source character encoding is really UTF-8
    CStringUTF8(const CStringUTF8& src, EValidate validate = eNoValidate)
        : string(src)
    {
        if (validate == eValidate) {
            x_Validate();
        }
    }

    /// Constructor from a C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8(const string& src,
                EEncoding encoding = eEncoding_ISO8859_1,
                EValidate validate = eNoValidate)
        : string()
    {
        x_Append(src.c_str(), encoding, validate);
    }

    /// Constructor from a C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8(const char* src,
                EEncoding encoding = eEncoding_ISO8859_1,
                EValidate validate = eNoValidate)
        : string()
    {
        x_Append(src, encoding, validate);
    }

#if defined(HAVE_WSTRING)
    /// Constructor from a wstring (UTF-16).
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8(const wstring& src)
        : string()
    {
        x_Append( src.c_str());
    }

    /// Constructor from a whcar_t* (UTF-16).
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8(const wchar_t* src)
        : string()
    {
        x_Append(src);
    }
#endif // HAVE_WSTRING

    /// Assign to UTF8 string
    CStringUTF8& operator= (const CStringUTF8& src)
    {
        string::operator= (src);
        return *this;
    }

    /// Assign to C++ string in Latin1 encoding.
    CStringUTF8& operator= (const string& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assign to C string in Latin1 encoding.
    CStringUTF8& operator= (const char* src)
    {
        erase();
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Assign to C++ wstring in UTF16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator= (const wstring& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assign to wide-char C string in UTF16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator= (const wchar_t* src)
    {
        erase();
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING

    /// Append a string in UTF8 encoding
    CStringUTF8& operator+= (const CStringUTF8& src)
    {
        string::operator+= (src);
        return *this;
    }

    /// Append a C++ string in Latin1 encoding
    CStringUTF8& operator+= (const string& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append a C string in Latin1 encoding
    CStringUTF8& operator+= (const char* src)
    {
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Append a C++ wstring in UTF-16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator+= (const wstring& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append a wide-char C string in UTF-16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator+= (const wchar_t* src)
    {
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING
    
    /// Assign to C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Assign(const string& src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        erase();
        x_Append(src.c_str(), encoding, validate);
        return *this;
    }

    /// Assign to C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Assign(const char* src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        erase();
        x_Append(src, encoding, validate);
        return *this;
    }

    /// Assign to a single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    CStringUTF8& Assign(char ch,
                        EEncoding encoding)
    {
        erase();
        x_AppendChar( CharToSymbol( ch, encoding ) );
        return *this;
    }

    /// Append a C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Append(const string& src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        x_Append(src.c_str(), encoding, validate);
        return *this;
    }

    /// Append a C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Append(const char* src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        x_Append(src, encoding, validate);
        return *this;
    }

    /// Append single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    CStringUTF8& Append(char ch,
                        EEncoding encoding)
    {
        x_AppendChar( CharToSymbol( ch, encoding ) );
        return *this;
    }

    /// Append single Unicode code point
    ///
    /// @param ch
    ///   Unicode code point
    CStringUTF8& Append(TUnicodeSymbol ch)
    {
        x_AppendChar(ch);
        return *this;
    }

    /// Get the number of symbols (code points) in the string
    ///
    /// @return
    ///   Number of symbols (code points)
    SIZE_TYPE GetSymbolCount(void) const;
    
    /// Get the number of valid UTF-8 symbols (code points) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid symbols (no exception thrown)
    static SIZE_TYPE GetValidSymbolCount(const char* src, SIZE_TYPE buf_size);
    
    /// Get the number of valid UTF-8 bytes (code units) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid bytes (no exception thrown)
    static SIZE_TYPE GetValidBytesCount(const char* src, SIZE_TYPE buf_size);

    /// Check that the character encoding of the string is valid UTF-8
    ///
    /// @return
    ///   Result of the check
    bool IsValid(void) const
    {
        return MatchEncoding(c_str(), eEncoding_UTF8);
    }
    /// Convert to ISO 8859-1 (Latin1) character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    string AsLatin1(void) const
    {
        return AsSingleByteString(eEncoding_ISO8859_1);
    }
    
    /// Convert the string to a single-byte character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    /// @param encoding
    ///   Desired encoding
    /// @return
    ///   C++ string
    string AsSingleByteString(EEncoding encoding) const;

#if defined(HAVE_WSTRING)
    /// Convert to Unicode (UTF-16 with no surrogates).
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    /// Defined only if wstring is supported by the compiler.
    wstring AsUnicode(void) const;
#endif // HAVE_WSTRING

    /// Guess the encoding of the C string
    ///
    /// It can distinguish between UTF-8, Latin1, and Win1252 only
    /// @param src
    ///   Source zero-terminated character buffer
    /// @return
    ///   Encoding
    static EEncoding GuessEncoding( const char* src);

    /// Guess the encoding of the C++ string
    ///
    /// It can distinguish between UTF-8, Latin1, and Win1252 only
    /// @param src
    ///   Source string
    /// @return
    ///   Encoding
    static EEncoding GuessEncoding( const string& src)
    {
        return GuessEncoding( src.c_str());
    }

    /// Check the encoding of the C string
    ///
    /// Check that the encoding of the source is the same, or
    /// is compatible with the specified one
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding form to check against
    /// @return
    ///   Boolean result: encoding is same or compatible
    static bool MatchEncoding( const char* src, EEncoding encoding);

    /// Check the encoding of the C++ string
    ///
    /// Check that the encoding of the source is the same, or
    /// is compatible with the specified one
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding form to check against
    /// @return
    ///   Boolean result: encoding is same or compatible
    static bool MatchEncoding( const string& src, EEncoding encoding)
    {
        return MatchEncoding( src.c_str(), encoding);
    }
    
    /// Convert encoded character into UTF16
    ///
    /// @param ch
    ///   Encoded character
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Code point
    static TUnicodeSymbol CharToSymbol(char ch, EEncoding encoding);
    
    /// Convert Unicode code point into encoded character
    ///
    /// @param ch
    ///   Code point
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Encoded character
    static char SymbolToChar(TUnicodeSymbol sym, EEncoding encoding);

    /// Convert sequence of UTF8 code units into Unicode code point
    ///
    /// @param src
    ///   UTF8 zero-terminated buffer
    /// @return
    ///   Unicode code point
    static TUnicodeSymbol Decode(const char*& src);

    /// Convert first character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param more
    ///   if the character is valid, - how many more characters to expect
    /// @return
    ///   non-zero, if the character is valid
    static TUnicodeSymbol  DecodeFirst(char ch, SIZE_TYPE& more);

    /// Convert next character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param ch16
    ///   Unicode character
    /// @return
    ///   non-zero, if the character is valid
    static TUnicodeSymbol  DecodeNext(TUnicodeSymbol chU, char ch);

private:
    /// Function AsAscii is deprecated - use AsLatin1() instead
    string AsAscii(void) const
    {
        return AsLatin1();
    }

    void   x_Validate(void) const;
    /// Convert Unicode code point into UTF8 and append
    void   x_AppendChar(TUnicodeSymbol ch);
    /// Convert coded character sequence into UTF8 and append
    void   x_Append(const char* src,
                    EEncoding encoding = eEncoding_ISO8859_1,
                    EValidate validate = eNoValidate);
#if defined(HAVE_WSTRING)
    /// Convert Unicode character sequence into UTF8 and append
    void x_Append(const wchar_t* src);
#endif // HAVE_WSTRING
    /// Check how many bytes is needed to represent the code point in UTF8
    static SIZE_TYPE x_BytesNeeded(TUnicodeSymbol ch);
    /// Check if the character is valid first code unit of UTF8
    static bool   x_EvalFirst(char ch, SIZE_TYPE& more);
    /// Check if the character is valid non-first code unit of UTF8
    static bool   x_EvalNext(char ch);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CParseTemplException --
///
/// Define template class for parsing exception. This class is used to define
/// exceptions for complex parsing tasks and includes an additional m_Pos
/// data member. The constructor requires that an additional postional
/// parameter be supplied along with the description message.

template <class TBase>
class CParseTemplException : EXCEPTION_VIRTUAL_BASE public TBase
{
public:
    /// Error types that for exception class.
    enum EErrCode {
        eErr        ///< Generic error 
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErr: return "eErr";
        default:   return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    ///
    /// Report "pos" along with "what".
    CParseTemplException(const CDiagCompileInfo &info,
        const CException* prev_exception,
        EErrCode err_code,const string& message,
        string::size_type pos, EDiagSev severity = eDiag_Error)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message), m_Pos(pos)
    {
        this->x_Init(info,
                     string("{") + NStr::UIntToString((unsigned long)m_Pos) +
                     "} " + message,
                     prev_exception,
                     severity);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Constructor.
    CParseTemplException(const CParseTemplException<TBase>& other)
        : TBase(other)
    {
        m_Pos = other.m_Pos;
        x_Assign(other);
    }

    /// Destructor.
    virtual ~CParseTemplException(void) throw() {}

    /// Report error position.
    virtual void ReportExtra(ostream& out) const
    {
        out << "m_Pos = " << (unsigned long)m_Pos;
    }

    // Attributes.

    /// Get exception class type.
    virtual const char* GetType(void) const { return "CParseTemplException"; }

    /// Get error code.
    EErrCode GetErrCode(void) const
    {
        return typeid(*this) == typeid(CParseTemplException<TBase>) ?
            (typename CParseTemplException<TBase>::EErrCode)
                this->x_GetErrCode() :
            (typename CParseTemplException<TBase>::EErrCode)
                CException::eInvalid;
    }

    /// Get error position.
    string::size_type GetPos(void) const throw() { return m_Pos; }

protected:
    /// Constructor.
    CParseTemplException(void)
    {
        m_Pos = 0;
    }

    /// Helper clone method.
    virtual const CException* x_Clone(void) const
    {
        return new CParseTemplException<TBase>(*this);
    }

private:
    string::size_type m_Pos;    ///< Error position
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStringException --
///
/// Define exceptions generated by string classes.
///
/// CStringException inherits its basic functionality from
/// CParseTemplException<CCoreException> and defines additional error codes
/// for string parsing.

class NCBI_XNCBI_EXPORT CStringException : public CParseTemplException<CCoreException>
{
public:
    /// Error types that string classes can generate.
    enum EErrCode {
        eConvert,       ///< Failure to convert string
        eBadArgs,       ///< Bad arguments to string methods 
        eFormat         ///< Wrong format for any input to string methods
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT2(CStringException,
        CParseTemplException<CCoreException>, std::string::size_type);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CStringPairsParser --
///
/// Base class for parsing a string to a set of name-value pairs.


/// Decoder interface. Names and values can be decoded with different rules.
class IStringDecoder
{
public:
    /// Type of string to be decoded
    enum EStringType {
        eName,
        eValue
    };
    /// Decode the string. Must throw CStringException if the source string
    /// is not valid.
    virtual string Decode(const string& src, EStringType stype) const = 0;
    virtual ~IStringDecoder(void) {}
};


class NCBI_XNCBI_EXPORT CStringPairsParser
{
public:
    /// Create parser using default values.
    CStringPairsParser(void);
    /// Create parser with the specified parameters.
    ///
    /// @param arg_sep
    ///   Separator between name+value pairs
    /// @param val_sep
    ///   Separator between name and value
    /// @param decoder
    ///   String decoder (Url, Xml etc.)
    /// @param own
    ///   Decoder ownership flag
    CStringPairsParser(const string& arg_sep,
                       const string& val_sep,
                       IStringDecoder* decoder = NULL,
                       EOwnership own = eTakeOwnership);
    /// Create parser with the specified decoder and default separators.
    ///
    /// @param decoder
    ///   String decoder (Url, Xml etc.)
    /// @param own
    ///   Decoder ownership flag
    CStringPairsParser(IStringDecoder* decoder,
                       EOwnership own = eTakeOwnership);
    virtual ~CStringPairsParser(void);

    /// Set string decoder.
    ///
    /// @param decoder
    ///   String decoder (Url, Xml etc.)
    /// @param own
    ///   Decoder ownership flag
    void SetDecoder(IStringDecoder* decoder, EOwnership own = eTakeOwnership)
        { m_Decoder.reset(decoder, own); }
    /// Get decoder or NULL. Does not affect decoder ownership.
    IStringDecoder* GetDecoder(void) { return m_Decoder.get(); }

    /// Parse the string.
    ///
    /// @param str
    ///   String to parse. The parser assumes the string is formatted like
    ///   "name1<valsep>value1<argsep>name2<valsep>value2...". Each name and
    ///   value is passed to the decoder (if not NULL) before storing the pair.
    /// @param merge_argsep
    ///   Flag for merging separators between pairs. By default the separators
    ///   are merged to prevent pairs where both name and value are empty.
    void Parse(const CTempString& str,
               NStr::EMergeDelims merge_argsep = NStr::eMergeDelims);

    // Do not use map or multimap to preserve order of arguments.
    typedef pair<string, string> TStrPair;
    typedef vector<TStrPair>     TStrPairs;

    /// Read data
    const TStrPairs& GetPairs(void) const { return m_Data; }
    /// Get non-const data
    TStrPairs& GetPairs(void) { return m_Data; }

private:
    string                  m_ArgSep;   // Separator between name+value pairs ("&")
    string                  m_ValSep;   // Separator between name and value ("=")
    AutoPtr<IStringDecoder> m_Decoder;  // String decoder (Url, Xml etc.)
    TStrPairs               m_Data;     // Parsed data
};


/// URL-decoder for string pairs parser
class NCBI_XNCBI_EXPORT CStringDecoder_Url : public IStringDecoder
{
public:
    CStringDecoder_Url(NStr::EUrlDecode flag = NStr::eUrlDec_All);

    virtual string Decode(const string& src, EStringType stype) const;

private:
    NStr::EUrlDecode m_Flag;
};


/////////////////////////////////////////////////////////////////////////////
//  Predicates
//



/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-sensitive string comparison methods.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.

template <typename T>
struct PCase_Generic
{
    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool operator()(const T& s1, const T& s2) const;
};

typedef PCase_Generic<string>       PCase;
typedef PCase_Generic<const char *> PCase_CStr;



/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase_Conditional_Generic

template <typename T>
struct PNocase_Generic
{
    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const T& s1, const T& s2) const;
};

typedef PNocase_Generic<string>       PNocase;
typedef PNocase_Generic<const char *> PNocase_CStr;


/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
/// Case sensitivity can be turned on and off at runtime.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase_Generic

template <typename T>
class PNocase_Conditional_Generic
{
public:
    /// Construction
    PNocase_Conditional_Generic(NStr::ECase case_sens = NStr::eCase);

    /// Get comparison type
    NStr::ECase GetCase() const { return m_CaseSensitive; }

    /// Set comparison type
    void SetCase(NStr::ECase case_sens) { m_CaseSensitive = case_sens; }

    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const T& s1, const T& s2) const;
private:
    NStr::ECase m_CaseSensitive; ///< case sensitive when TRUE
};

typedef PNocase_Conditional_Generic<string>       PNocase_Conditional;
typedef PNocase_Conditional_Generic<const char *> PNocase_Conditional_CStr;


/////////////////////////////////////////////////////////////////////////////
///
/// PQuickStringLess implements an ordering of strings,
/// that is more efficient than usual lexicographical order.
/// It can be used in cases when no specific order is required,
/// e.g. only simple key lookup is needed.
/// Current implementation first compares lengths of strings,
/// and will compare string data only when lengths are the same.
///
struct PQuickStringLess
{
    bool operator()(const CTempString& s1, const CTempString& s2) const {
        size_t len1 = s1.size(), len2 = s2.size();
        return len1 < len2 ||
            len1 == len2 && memcmp(s1.data(), s2.data(), len1) < 0;
    }
};


/////////////////////////////////////////////////////////////////////////////
//  Algorithms
//


/// Check equivalence of arguments using predicate.
template<class Arg1, class Arg2, class Pred>
inline
bool AStrEquiv(const Arg1& x, const Arg2& y, Pred pr)
{
    return pr.Equals(x, y);
}


/* @} */



/////////////////////////////////////////////////////////////////////////////
//
//  IMPLEMENTATION of INLINE functions
//
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CNcbiEmptyString::
//
#if !defined(NCBI_OS_MSWIN) && !( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
inline
const string& CNcbiEmptyString::Get(void)
{
    const string* str = m_Str;
    return str ? *str: FirstGet();
}
#endif



/////////////////////////////////////////////////////////////////////////////
//  NStr::
//

inline
string NStr::IntToString(long value,
                         TNumToStringFlags flags, int base)
{
    string ret;
    IntToString(ret, value, flags, base);
    return ret;
}

inline
string NStr::UIntToString(unsigned long value,
                          TNumToStringFlags flags, int base)
{
    string ret;
    UIntToString(ret, value, flags, base);
    return ret;
}

inline
int NStr::HexChar(char ch)
{
    unsigned int rc = ch - '0';
    if (rc <= 9) {
        return rc;
    } else {
        rc = (ch | ' ') - 'a';
        return rc <= 5 ? int(rc + 10) : -1;
    }
}

inline
bool NStr::MatchesMask(const string& str, const string& mask, ECase use_case)
{
    return MatchesMask(str.c_str(), mask.c_str(), use_case);
}

inline
int NStr::strcmp(const char* s1, const char* s2)
{
    return ::strcmp(s1, s2);
}

inline
int NStr::strncmp(const char* s1, const char* s2, size_t n)
{
    return ::strncmp(s1, s2, n);
}

inline
int NStr::strcasecmp(const char* s1, const char* s2)
{
#if defined(HAVE_STRICMP)
#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
    return ::_stricmp(s1, s2);
#else
    return ::stricmp(s1, s2);
#endif

#elif defined(HAVE_STRCASECMP_LC)
    return ::strcasecmp(s1, s2);

#else
    int diff = 0;
    for ( ;; ++s1, ++s2) {
        char c1 = *s1;
        // calculate difference
        diff = tolower((unsigned char) c1) - tolower((unsigned char)(*s2));
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
    }
    return diff;
#endif
}

inline
int NStr::strncasecmp(const char* s1, const char* s2, size_t n)
{
#if defined(HAVE_STRICMP)
#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
    return ::_strnicmp(s1, s2, n);
#else
    return ::strnicmp(s1, s2, n);
#endif

#elif defined(HAVE_STRCASECMP_LC)
    return ::strncasecmp(s1, s2, n);

#else
    int diff = 0;
    for ( ; ; ++s1, ++s2, --n) {
        if (n == 0)
            return 0;
        char c1 = *s1;
        // calculate difference
        diff = tolower((unsigned char) c1) - tolower((unsigned char)(*s2));
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
    }
    return diff;
#endif
}

inline
size_t NStr::strftime(char* s, size_t maxsize, const char* format,
                      const struct tm* timeptr)
{
#if defined(NCBI_COMPILER_MSVC)
    string x_format = Replace(format, "%T", "%H:%M:%S");
    ReplaceInPlace(x_format,          "%D", "%m/%d/%y");
    return ::strftime(s, maxsize, x_format.c_str(), timeptr);
#else
    return ::strftime(s, maxsize, format, timeptr);
#endif
}


inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case)
{
    return use_case == eCase ?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case)
{
    return use_case == eCase ?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::CompareCase(const char* s1, const char* s2)
{
    return NStr::strcmp(s1, s2);
}

inline
int NStr::CompareNocase(const char* s1, const char* s2)
{
    return NStr::strcasecmp(s1, s2);
}

inline
int NStr::Compare(const char* s1, const char* s2, ECase use_case)
{
    return use_case == eCase ? CompareCase(s1, s2): CompareNocase(s1, s2);
}

inline
int NStr::Compare(const string& s1, const char* s2, ECase use_case)
{
    return Compare(s1.c_str(), s2, use_case);
}

inline
int NStr::Compare(const char* s1, const string& s2, ECase use_case)
{
    return Compare(s1, s2.c_str(), use_case);
}

inline
int NStr::Compare(const string& s1, const string& s2, ECase use_case)
{
    return Compare(s1.c_str(), s2.c_str(), use_case);
}

inline
int NStr::CompareCase(const string& s1, const string& s2)
{
    return CompareCase(s1.c_str(), s2.c_str());
}

inline
int NStr::CompareNocase(const string& s1, const string& s2)
{
    return CompareNocase(s1.c_str(), s2.c_str());
}

inline
bool NStr::Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case)
{
    return use_case == eCase ?
        EqualCase(str, pos, n, pattern) : EqualNocase(str, pos, n, pattern);
}

inline
bool NStr::Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case)
{
    return use_case == eCase ?
        EqualCase(str, pos, n, pattern) : EqualNocase(str, pos, n, pattern);
}

inline
bool NStr::EqualCase(const char* s1, const char* s2)
{
    return NStr::strcmp(s1, s2) == 0;
}

inline
bool NStr::EqualNocase(const char* s1, const char* s2)
{
    return NStr::strcasecmp(s1, s2) == 0;
}

inline
bool NStr::EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                     const char* pattern)
{
    return NStr::CompareCase(str, pos, n, pattern) == 0;
}

inline
bool NStr::EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                     const string& pattern)
{
    return NStr::CompareCase(str, pos, n, pattern) == 0;
}

inline
bool NStr::Equal(const char* s1, const char* s2, ECase use_case)
{
    return (use_case == eCase ? EqualCase(s1, s2) : EqualNocase(s1, s2));
}

inline
bool NStr::Equal(const string& s1, const char* s2, ECase use_case)
{
    return Equal(s1.c_str(), s2, use_case);
}

inline
bool NStr::Equal(const char* s1, const string& s2, ECase use_case)
{
    return Equal(s1, s2.c_str(), use_case);
}

inline
bool NStr::Equal(const string& s1, const string& s2, ECase use_case)
{
    return Equal(s1.c_str(), s2.c_str(), use_case);
}

inline
bool NStr::EqualCase(const string& s1, const string& s2)
{
    // return EqualCase(s1.c_str(), s2.c_str());
    return s1 == s2;
}

inline
bool NStr::EqualNocase(const string& s1, const string& s2)
{
    return EqualNocase(s1.c_str(), s2.c_str());
}

inline
bool NStr::EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern)
{
    return CompareNocase(str, pos, n, pattern) == 0;
}

inline
bool NStr::EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern)
{
    return CompareNocase(str, pos, n, pattern) == 0;
}

inline
bool NStr::StartsWith(const string& str, const string& start, ECase use_case)
{
    return str.size() >= start.size()  &&
        Compare(str, 0, start.size(), start, use_case) == 0;
}

inline
bool NStr::StartsWith(const string& str, char start, ECase use_case)
{
    return !str.empty()  &&
        ((use_case == eCase) ? (str[0] == start) :
         (toupper((unsigned char) str[0]) == start  ||
          tolower((unsigned char) str[0])));
}

inline
bool NStr::EndsWith(const string& str, const string& end, ECase use_case)
{
    return str.size() >= end.size()  &&
        Compare(str, str.size() - end.size(), end.size(), end, use_case) == 0;
}

inline
bool NStr::EndsWith(const string& str, char end, ECase use_case)
{
    if (!str.empty()) {
        char last = str[str.length() - 1];
        return (use_case == eCase) ? (last == end) :
               (toupper((unsigned char) last) == end  ||
                tolower((unsigned char) last) == end);
    }
    return false;
}

inline
SIZE_TYPE NStr::Find(const string& str, const string& pattern,
                     SIZE_TYPE start, SIZE_TYPE end, EOccurrence where,
                     ECase use_case)
{
    return use_case == eCase ? FindCase(str, pattern, start, end, where)
        : FindNoCase(str, pattern, start, end, where);
}

inline
SIZE_TYPE NStr::FindCase(const string& str, const string& pattern,
                         SIZE_TYPE start, SIZE_TYPE end, EOccurrence where)
{
    if (where == eFirst) {
        SIZE_TYPE result = str.find(pattern, start);
        return (result == NPOS  ||  result > end) ? NPOS : result;
    } else {
        SIZE_TYPE result = str.rfind(pattern, end);
        return (result == NPOS  ||  result < start) ? NPOS : result;
    }
}


inline
string NStr::CEncode(const string& str)
{
    return PrintableString(str);
}


inline
list<string>& NStr::Wrap(const string& str, SIZE_TYPE width, list<string>& arr,
                         NStr::TWrapFlags flags, const string& prefix,
                         const string* prefix1)
{
    return Wrap(str, width, arr, flags, &prefix, prefix1);
}


inline
list<string>& NStr::Wrap(const string& str, SIZE_TYPE width, list<string>& arr,
                         NStr::TWrapFlags flags, const string& prefix,
                         const string& prefix1)
{
    return Wrap(str, width, arr, flags, &prefix, &prefix1);
}


inline
list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string& prefix,
                             const string* prefix1)
{
    return WrapList(l, width, delim, arr, flags, &prefix, prefix1);
}


inline
list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string& prefix,
                             const string& prefix1)
{
    return WrapList(l, width, delim, arr, flags, &prefix, &prefix1);
}




/////////////////////////////////////////////////////////////////////////////
//  PCase_Generic::
//

template <typename T>
inline
int PCase_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, NStr::eCase);
}

template <typename T>
inline
bool PCase_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PCase_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PCase_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}



////////////////////////////////////////////////////////////////////////////
//  PNocase_Generic<T>::
//


template <typename T>
inline
int PNocase_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, NStr::eNocase);
}

template <typename T>
inline
bool PNocase_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PNocase_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PNocase_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}

////////////////////////////////////////////////////////////////////////////
//  PNocase_Conditional_Generic<T>::
//

template <typename T>
inline
PNocase_Conditional_Generic<T>::PNocase_Conditional_Generic(NStr::ECase cs)
    : m_CaseSensitive(cs)
{}

template <typename T>
inline
int PNocase_Conditional_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, m_CaseSensitive);
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}



END_NCBI_SCOPE

#endif  /* CORELIB___NCBISTR__HPP */
