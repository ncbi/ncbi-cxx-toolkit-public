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


#include <corelib/ncbitype.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistl.hpp>
#include <string.h>
#ifdef NCBI_OS_OSF1
#  include <strings.h>
#endif
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <string>
#include <list>
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


    /// Whether to prohibit trailing symbols (any symbol but '\0')
    /// in the StringToXxx() conversion functions below.
    enum ECheckEndPtr {
        eCheck_Need,      ///< Check is necessary
        eCheck_Skip       ///< Skip this check
    };

    /// Whether to throw an exception if there is a conversion error in
    /// the StringToXxx() function.
    enum EConvErrAction {
        eConvErr_Throw,   ///< Throw an exception on error
        eConvErr_NoThrow  ///< Return "natural null" value on error
    };

    /// Convert string to int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number that does not fit into "int".
    static int StringToInt(const string&  str,
                           int            base     = 10,
                           ECheckEndPtr   check    = eCheck_Need,
                           EConvErrAction on_error = eConvErr_Throw);


    /// Convert string to unsigned int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "unsigned int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number that does not fit into "unsigned int".
    static unsigned int StringToUInt(const string&  str,
                                     int            base     = 10,
                                     ECheckEndPtr   check    = eCheck_Need,
                                     EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number that does not fit into "long".
    static long StringToLong(const string&  str,
                             int            base     = 10,
                             ECheckEndPtr   check    = eCheck_Need,
                             EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to unsigned long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "unsigned long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number that does not fit into "unsigned long".
    static unsigned long StringToULong(const string&  str,
                                       int            base     = 10,
                                       ECheckEndPtr   check    = eCheck_Need,
                                       EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to double.
    ///
    /// @param str
    ///   String to be converted.
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "double" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number that does not fit into "double".
    static double StringToDouble(const string&  str,
                                 ECheckEndPtr   check    = eCheck_Need,
                                 EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to Int8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Radix base. Default is 10. Other values can be 2, 8, and 16.
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   Converted Int8 value.
    static Int8 StringToInt8(const string&  str,
                             int            base     = 10,
                             ECheckEndPtr   check    = eCheck_Need,
                             EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to Uint8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   Converted UInt8 value.
    static Uint8 StringToUInt8(const string&  str,
                               int            base     = 10,
                               ECheckEndPtr   check    = eCheck_Need,
                               EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to number of bytes. 
    ///
    /// String can contain "software" qulifiers: MB(megabyte), KB (kilobyte)..
    /// Example: 100MB, 1024KB
    ///
    /// @param str
    ///   String to be converted.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @param check
    ///   Whether trailing symbols (other than '\0') are permitted - default
    ///   is eCheck_Needed which means that if there are trailing symbols
    ///   after the number, an action defined by "on_error" parameter will
    ///   be performed. If the value is eCheck_Skip, the string can have
    ///   trailing symbols after the number.
    /// @param on_error
    ///   Whether to throw an exception on error, or just to return zero.
    /// @return
    ///   - Convert "str" to "Uint8" value of bytes presented by string
    ///     and return it. 
    ///   - 0 if "str" contains illegal symbols, or if it represents
    ///     a number of bytes that does not fit into "Uint8".
    static 
    Uint8 StringToUInt8_DataSize(const string&  str, 
                                 int            base     = 10,
                                 ECheckEndPtr   check    = eCheck_Need,
                                 EConvErrAction on_error = eConvErr_Throw);

    /// Convert string to pointer.
    ///
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Pointer value corresponding to its string representation.
    static const void* StringToPtr(const string& str);

    /// Convert Int to String.
    ///
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param sign
    ///   Whether converted value should be preceded by the sign (+-) character.  
    /// @return
    ///   Converted string value.
    static string IntToString(long value, bool sign = false);

    /// Convert Int to String.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param sign
    ///   Whether converted value should be preceded by the sign (+-) character.  
    static void IntToString(string& out_str, long value, bool sign = false);


    /// Convert UInt to string.
    ///
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    /// @return
    ///   Converted string value.
    static string UIntToString(unsigned long value);

    /// Convert UInt to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    static void UIntToString(string& out_str, unsigned long value);

    /// Convert Int8 to string.
    ///
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param sign
    ///   Whether converted value should be preceded by the sign (+-) character.  
    /// @return
    ///   Converted string value.
    static string Int8ToString(Int8 value, bool sign = false);


    /// Convert Int8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param sign
    ///   Whether converted value should be preceded by the sign (+-) character.  
    static void Int8ToString(string& out_str, Int8 value, bool sign = false);

    /// Convert UInt8 to string.
    ///
    /// @param value
    ///   Integer value (UInt8) to be converted.
    /// @return
    ///   Converted string value.
    static string UInt8ToString(Uint8 value);

    /// Convert UInt8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (UInt8) to be converted.
    static void UInt8ToString(string& out_str, Uint8 value);

    /// Convert double to string.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @return
    ///   Converted string value.
    static string DoubleToString(double value);

    /// Convert double to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Double value to be converted.
    static void DoubleToString(string& out_str, double value);

    /// Convert double to string with specified precision.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    /// @return
    ///   Converted string value.
    static string DoubleToString(double value, unsigned int precision);

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
    /// @return
    ///   The number of bytes stored in "buf", not counting the
    ///   terminating '\0'.
    static SIZE_TYPE DoubleToString(double value, unsigned int precision,
                                    char* buf, SIZE_TYPE buf_size);

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
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const char *str, const char *mask);

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
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const string& str, const string& mask);

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
    static bool IsBlank(const string& str);

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
                           string& dst,
                           SIZE_TYPE start_pos = 0, size_t max_replace = 0);

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
                          SIZE_TYPE start_pos = 0, size_t max_replace = 0);

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
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Tokenize()
    static list<string>& Split(const string& str,
                               const string& delim,
                               list<string>& arr,
                               EMergeDelims  merge = eMergeDelims);

    /// Tokenize a string using the specified delimiters.
    ///
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Delimiters used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param arr
    ///   The tokens defined in "str" by using symbols from "delim" are added
    ///   to the list "arr" and also returned by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eNoMergeDelims means that delimiters that immediately follow each other
    ///   are treated as separate delimiters.
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Split()
    static vector<string>& Tokenize(const string&   str,
                                    const string&   delim,
                                    vector<string>& arr,
                                    EMergeDelims    merge = eNoMergeDelims);

    /// Split a string into two pieces using the specified delimiters
    ///
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
    ///   Split()
    static bool SplitInTwo(const string& str, 
                           const string& delim,
                           string& str1,
                           string& str2);
                         

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

    /// How to display new line characters.
    ///
    /// Assists in making a printable version of "str".
    enum ENewLineMode {
        eNewLine_Quote,         ///< Display "\n" instead of actual linebreak
        eNewLine_Passthru       ///< Break the line on every "\n" occurrance
    };

    /// Get a printable version of the specified string. 
    ///
    /// The non-printable characters will be represented as "\r", "\n", "\v",
    /// "\t", "\"", "\\", or "\xDD" where DD is the character's code in
    /// hexadecimal.
    ///
    /// @param str
    ///   The string whose printable version is wanted.
    /// @param nl_mode
    ///   How to represent the new line character. The default setting of 
    ///   eNewLine_Quote displays the new line as "\n". If set to
    ///   eNewLine_Passthru, a line break is used instead.
    /// @return
    ///   Return a printable version of "str".
    /// @sa
    ///   ParseEscapes
    static string PrintableString(const string& str,
                                  ENewLineMode  nl_mode = eNewLine_Quote);

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
/// Define a UTF-8 String class.
///
/// UTF-8 stands for Unicode Transformation Format-8, and is an 8-bit
/// lossless encoding of Unicode characters.
/// @sa
///   RFC 2279

class NCBI_XNCBI_EXPORT CStringUTF8 : public string
{
public:
    /// Constructor.
    CStringUTF8(void)
    {
    }

    /// Destructor.
    ~CStringUTF8(void)
    {
    }

    /// Copy constructor.
    CStringUTF8(const CStringUTF8& src)
        : string(src)
    {
    }

    /// Constructor from a string argument.
    CStringUTF8(const string& src)
        : string()
    {
        x_Append(src.c_str());
    }

    /// Constructor from a char* argument.
    CStringUTF8(const char* src)
        : string()
    {
        x_Append(src);
    }

#if defined(HAVE_WSTRING)
    /// Constructor from a wstring argument.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8(const wstring& src)
        : string()
    {
        x_Append( src.c_str());
    }

    /// Constructor from a whcar_t* argument.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8(const wchar_t* src)
        : string()
    {
        x_Append(src);
    }
#endif // HAVE_WSTRING

    /// Assignment operator -- rhs is a CStringUTF8.
    CStringUTF8& operator= (const CStringUTF8& src)
    {
        string::operator= (src);
        return *this;
    }

    /// Assignment operator -- rhs is a string.
    CStringUTF8& operator= (const string& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assignment operator -- rhs is a char*.
    CStringUTF8& operator= (const char* src)
    {
        erase();
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Assignment operator -- rhs is a wstring.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8& operator= (const wstring& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assignment operator -- rhs is a wchar_t*.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8& operator= (const wchar_t* src)
    {
        erase();
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING

    /// Append to string operator+= -- rhs is CStringUTF8.
    CStringUTF8& operator+= (const CStringUTF8& src)
    {
        string::operator+= (src);
        return *this;
    }

    /// Append to string operator+= -- rhs is string.
    CStringUTF8& operator+= (const string& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append to string operator+= -- rhs is char*.
    CStringUTF8& operator+= (const char* src)
    {
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Append to string operator+=  -- rhs is a wstring.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8& operator+= (const wstring& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append to string operator+=  -- rhs is a wchar_t*.
    ///
    /// Defined only if HAVE_STRING is defined.
    CStringUTF8& operator+= (const wchar_t* src)
    {
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING

    /// Convert to ASCII.
    ///
    /// Can throw a StringException with error codes "eFormat" or "eConvert"
    /// if string has a wrong UTF-8 format or cannot be converted to ASCII.
    string AsAscii(void) const;

#if defined(HAVE_WSTRING)
    /// Convert to Unicode.
    ///
    /// Defined only if HAVE_STRING is defined.
    /// Can throw a StringException with error code "eFormat" if string has
    /// a wrong UTF-8 format.
    wstring AsUnicode(void) const;
#endif // HAVE_WSTRING

private:
    /// Helper method to append necessary characters for UTF-8 format.
    void x_Append(const char* src);

#if defined(HAVE_WSTRING)
    /// Helper method to append necessary characters for UTF-8 format.
    ///
    /// Defined only if HAVE_STRING is defined.
    void x_Append(const wchar_t* src);
#endif // HAVE_WSTRING
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
        string::size_type pos)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message), m_Pos(pos)
    {
        this->x_Init(info,
                     string("{") + NStr::UIntToString((unsigned long)m_Pos) + "} " + message,
                     prev_exception);
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

class CStringException : public CParseTemplException<CCoreException>
{
public:
    /// Error types that string classes can generate.
    enum EErrCode {
        eConvert,       ///< Failure to convert string
        eBadArgs,       ///< Bad arguments to string methods 
        eFormat         ///< Wrong format for any input to string methods
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eConvert:  return "eConvert";
        case eBadArgs:  return "eBadArgs";
        case eFormat:   return "eFormat";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT2(CStringException,
        CParseTemplException<CCoreException>, std::string::size_type);
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

struct PCase
{
    /// Return difference between "s1" and "s2".
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2.
    bool operator()(const string& s1, const string& s2) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase_Conditional

struct PNocase
{
    /// Return difference between "s1" and "s2".
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const string& s1, const string& s2) const;
};


/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
/// Case sensitivity can be turned on and off at runtime.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase

class PNocase_Conditional
{
public:
    /// Construction
    PNocase_Conditional(NStr::ECase case_sens = NStr::eCase);

    /// Get comparison type
    NStr::ECase GetCase() const { return m_CaseSensitive; }

    /// Set comparison type
    void SetCase(NStr::ECase case_sens) { m_CaseSensitive = case_sens; }

    /// Return difference between "s1" and "s2".
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const string& s1, const string& s2) const;
private:
    NStr::ECase m_CaseSensitive; ///< case sensitive when TRUE
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

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
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
    return ::stricmp(s1, s2);

#elif defined(HAVE_STRCASECMP_LC)
    return ::strcasecmp(s1, s2);

#else
    int diff = 0;
    for ( ;; ++s1, ++s2) {
        char c1 = *s1;
        // calculate difference
        diff = tolower(c1) - tolower(*s2);
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
    return ::strnicmp(s1, s2, n);

#elif defined(HAVE_STRCASECMP_LC)
    return ::strncasecmp(s1, s2, n);

#else
    int diff = 0;
    for ( ; ; ++s1, ++s2, --n) {
        if (n == 0)
            return 0;
        char c1 = *s1;
        // calculate difference
        diff = tolower(c1) - tolower(*s2);
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
    string x_format;
    x_format = Replace(format,   "%T", "%H:%M:%S");
    x_format = Replace(x_format, "%D", "%m/%d/%y");
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
         (toupper(str[0]) == start  ||  tolower(str[0])));
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
               (toupper(last) == end  ||  tolower(last) == end);
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
//  PCase::
//

inline
int PCase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eCase);
}

inline
bool PCase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PCase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PCase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}



////////////////////////////////////////////////////////////////////////////
//  PNocase::
//

inline
int PNocase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eNocase);
}

inline
bool PNocase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PNocase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PNocase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}

////////////////////////////////////////////////////////////////////////////
//  PNocase_Conditional::
//

inline
PNocase_Conditional::PNocase_Conditional(NStr::ECase case_sens)
    : m_CaseSensitive(case_sens)
{}

inline
int PNocase_Conditional::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, m_CaseSensitive);
}

inline
bool PNocase_Conditional::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PNocase_Conditional::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PNocase_Conditional::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.77  2005/02/23 15:34:18  gouriano
 * Type cast to get rid of compiler warnings
 *
 * Revision 1.76  2005/02/16 15:04:35  ssikorsk
 * Tweaked kEmptyStr with Linux GCC
 *
 * Revision 1.75  2005/01/05 16:54:50  ivanov
 * Added string version of NStr::MatchesMask()
 *
 * Revision 1.74  2004/12/28 21:19:20  grichenk
 * Static strings changed to char*
 *
 * Revision 1.73  2004/12/08 12:47:16  kuznets
 * +PNocase_Conditional (case sensitive/insensitive comparison for maps)
 *
 * Revision 1.72  2004/11/24 15:16:13  shomrat
 * + fWrap_FlatFile - perform flat-file specific line wrap
 *
 * Revision 1.71  2004/11/05 16:30:02  shomrat
 * Fixed implementation (inline) of Equal methods
 *
 * Revision 1.70  2004/10/15 12:00:52  ivanov
 * Renamed NStr::StringToUInt_DataSize -> NStr::StringToUInt8_DataSize.
 * Added doxygen @return statement to NStr::StringTo* comments.
 * Added additional arguments to NStr::StringTo[U]Int8 to select radix
 * (now it is not fixed with predefined values, and can be arbitrary)
 * and action on not permitted trailing symbols in the converting string.
 *
 * Revision 1.69  2004/10/13 13:05:38  ivanov
 * Some cosmetics
 *
 * Revision 1.68  2004/10/13 01:05:06  vakatov
 * NStr::strncasecmp() -- fixed bug in "hand-made" code
 *
 * Revision 1.67  2004/10/05 16:34:10  shomrat
 * in place TruncateSpaces changed to TruncateSpacesInPlace
 *
 * Revision 1.66  2004/10/05 16:12:58  shomrat
 * + in place TruncateSpaces
 *
 * Revision 1.65  2004/10/04 14:27:31  ucko
 * Treat all letters as lowercase for case-insensitive comparisons.
 *
 * Revision 1.64  2004/10/01 15:17:38  shomrat
 * Added test if string starts/ends with a specified char and test if string is blank
 *
 * Revision 1.63  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.62  2004/09/21 18:44:55  kuznets
 * SoftStringToUInt renamed StringToUInt_DataSize
 *
 * Revision 1.61  2004/09/21 18:23:32  kuznets
 * +NStr::SoftStringToUInt KB, MB converter
 *
 * Revision 1.60  2004/09/07 21:25:03  ucko
 * +<strings.h> on OSF/1, as it may be needed for str(n)casecmp's declaration.
 *
 * Revision 1.59  2004/08/19 13:02:17  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.58  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.57  2004/06/21 12:14:50  ivanov
 * Added additional parameter for all StringToXxx() function that specify
 * an action which will be performed on conversion error: to throw an
 * exception, or just to return zero.
 *
 * Revision 1.56  2004/05/26 20:46:35  ucko
 * Fix backwards logic in Equal{Case,Nocase}.
 *
 * Revision 1.55  2004/04/26 14:44:30  ucko
 * Move CParseTemplException from ncbiexpt.hpp, as it needs NStr.
 *
 * Revision 1.54  2004/03/11 22:56:51  gorelenk
 * Removed tabs.
 *
 * Revision 1.53  2004/03/11 18:48:34  gorelenk
 * Added (condionaly) Windows-specific declaration of class CNcbiEmptyString.
 *
 * Revision 1.52  2004/03/05 14:21:54  shomrat
 * added missing definitions
 *
 * Revision 1.51  2004/03/05 12:28:00  ivanov
 * Moved CDirEntry::MatchesMask() to NStr class.
 *
 * Revision 1.50  2004/03/04 20:45:21  shomrat
 * Added equality checks
 *
 * Revision 1.49  2004/03/04 13:38:39  kuznets
 * + set of ToString conversion functions taking outout string as a parameter,
 * not a return value (should give a performance advantage in some cases)
 *
 * Revision 1.48  2003/12/12 17:26:45  ucko
 * +FormatVarargs
 *
 * Revision 1.47  2003/12/01 20:45:25  ucko
 * Extend Join to handle vectors as well as lists.
 * Add ParseEscapes (inverse of PrintableString).
 *
 * Revision 1.46  2003/11/18 11:57:37  siyan
 * Changed so @addtogroup does not cross namespace boundary
 *
 * Revision 1.45  2003/11/03 14:51:55  siyan
 * Fixed a typo in StringToInt header
 *
 * Revision 1.44  2003/08/19 15:17:20  rsmith
 * Add NStr::SplitInTwo() function.
 *
 * Revision 1.43  2003/08/15 18:14:54  siyan
 * Added documentation.
 *
 * Revision 1.42  2003/05/22 20:08:00  gouriano
 * added UTF8 strings
 *
 * Revision 1.41  2003/05/14 21:51:54  ucko
 * Move FindNoCase out of line and reimplement it to avoid making
 * lowercase copies of both strings.
 *
 * Revision 1.40  2003/03/31 13:31:06  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.39  2003/03/31 13:22:02  siyan
 * Added doxygen support
 *
 * Revision 1.38  2003/02/26 16:42:27  siyan
 * Added base parameter to NStr::StringToUInt8 to support different radixes
 * such as base 10 (default), 16, 8, 2.
 *
 * Revision 1.37  2003/02/24 19:54:50  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.36  2003/02/20 18:41:49  dicuccio
 * Added NStr::StringToPtr()
 *
 * Revision 1.35  2003/01/24 16:58:54  ucko
 * Add an optional parameter to Split and Tokenize indicating whether to
 * merge adjacent delimiters; drop suggestion about joining WrapList's
 * output with ",\n" because long items may be split across lines.
 *
 * Revision 1.34  2003/01/21 23:22:06  vakatov
 * NStr::Tokenize() to return reference, and not a new "vector<>".
 *
 * Revision 1.33  2003/01/21 20:08:28  ivanov
 * Added function NStr::DoubleToString(value, precision, buf, buf_size)
 *
 * Revision 1.32  2003/01/14 21:16:12  kuznets
 * +NStr::Tokenize
 *
 * Revision 1.31  2003/01/10 22:15:56  kuznets
 * Working on String2Int8
 *
 * Revision 1.30  2003/01/10 21:59:06  kuznets
 * +NStr::String2Int8
 *
 * Revision 1.29  2003/01/10 00:08:05  vakatov
 * + Int8ToString(),  UInt8ToString()
 *
 * Revision 1.28  2003/01/06 16:43:05  ivanov
 * + DoubleToString() with 'precision'
 *
 * Revision 1.27  2002/12/20 19:40:45  ucko
 * Add NStr::Find and variants.
 *
 * Revision 1.26  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.25  2002/10/18 21:02:30  ucko
 * Drop obsolete (and misspelled!) reference to fWrap_UsePrefix1 from
 * usage description.
 *
 * Revision 1.24  2002/10/18 20:48:41  lavr
 * +ENewLine_Mode and '\n' translation in NStr::PrintableString()
 *
 * Revision 1.23  2002/10/16 19:29:17  ucko
 * +fWrap_HTMLPre
 *
 * Revision 1.22  2002/10/03 14:44:33  ucko
 * Tweak the interfaces to NStr::Wrap* to avoid publicly depending on
 * kEmptyStr, removing the need for fWrap_UsePrefix1 in the process; also
 * drop fWrap_FavorPunct, as WrapList should be a better choice for such
 * situations.
 *
 * Revision 1.21  2002/10/02 20:14:52  ucko
 * Add Join, Wrap, and WrapList functions to NStr::.
 *
 * Revision 1.20  2002/07/17 16:49:03  gouriano
 * added ncbiexpt.hpp include
 *
 * Revision 1.19  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.18  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.17  2002/06/18 16:03:49  ivanov
 * Fixed #ifdef clause in NStr::strftime()
 *
 * Revision 1.16  2002/06/18 15:19:36  ivanov
 * Added NStr::strftime() -- correct handling %D and %T time formats on MS Windows
 *
 * Revision 1.15  2002/05/02 15:38:02  ivanov
 * Fixed comments for String-to-X functions
 *
 * Revision 1.14  2002/05/02 15:25:51  ivanov
 * Added new parameter to String-to-X functions for skipping the check
 * the end of string on zero
 *
 * Revision 1.13  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.12  2002/03/01 17:54:38  kans
 * include string.h and ctype.h
 *
 * Revision 1.11  2002/02/22 22:23:20  vakatov
 * Comments changed/added for string comparison functions
 * #elseif --> #elif
 *
 * Revision 1.10  2002/02/22 19:51:57  ivanov
 * Changed NCBI_OS_WIN to HAVE_STRICMP in the NStr::strncasecmp and
 * NStr::strcasecmp functions
 *
 * Revision 1.9  2002/02/22 17:50:22  ivanov
 * Added compatible compare functions strcmp, strncmp, strcasecmp, strncasecmp.
 * Was speed-up some Compare..() functions.
 *
 * Revision 1.8  2001/08/30 17:03:59  thiessen
 * remove class name from NStr member function declaration
 *
 * Revision 1.7  2001/08/30 00:39:29  vakatov
 * + NStr::StringToNumeric()
 * Moved all of "ncbistr.inl" code to here.
 * NStr::BoolToString() to return "const string&" rather than "string".
 * Also, well-groomed the code.
 *
 * Revision 1.6  2001/05/21 21:46:17  vakatov
 * SIZE_TYPE, NPOS -- moved from <ncbistl.hpp> to <ncbistr.hpp> and
 * made non-macros (to avoid possible name clashes)
 *
 * Revision 1.5  2001/05/17 14:54:18  lavr
 * Typos corrected
 *
 * Revision 1.4  2001/04/12 21:44:34  vakatov
 * Added dummy and private NStr::ToUpper/Lower(const char*) to prohibit
 * passing of constant C strings to the "regular" NStr::ToUpper/Lower()
 * variants.
 *
 * Revision 1.3  2001/03/16 19:38:55  grichenk
 * Added NStr::Split()
 *
 * Revision 1.2  2000/12/28 16:57:41  golikov
 * add string version of case an nocase cmp
 *
 * Revision 1.1  2000/12/15 15:36:30  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBISTR__HPP */
