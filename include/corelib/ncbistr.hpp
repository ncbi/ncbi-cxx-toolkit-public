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
 * File Description:
 *   The NCBI C++ standard methods for dealing with std::string
 *
 */

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>
#include <string.h>
#include <ctype.h>
#include <string>
#include <list>

BEGIN_NCBI_SCOPE


/// Empty "C" string (points to a '\0')
extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


/// Empty "C++" string
class CNcbiEmptyString
{
public:
    static const string& Get(void);
private:
    static const string& FirstGet(void);
    static const string* m_Str;
};
#define NcbiEmptyString NCBI_NS_NCBI::CNcbiEmptyString::Get()
#define kEmptyStr       NcbiEmptyString


// SIZE_TYPE and NPOS

typedef NCBI_NS_STD::string::size_type SIZE_TYPE;
static const SIZE_TYPE NPOS = NCBI_NS_STD::string::npos;



/////////////////////////////////////////////////////////////////////////////
//  NStr::
//    String-processing utilities
//

class NStr
{
public:
    /// convert "str" to a (non-negative) "int" value.
    /// return "-1" if "str" contains any symbols other than [0-9], or
    /// if it represents a number that does not fit into "int".
    static int StringToNumeric(const string& str);

    /// String-to-X conversion functions (throw exception if conversion error)
    static int           StringToInt    (const string& str, int base = 10);
    static unsigned int  StringToUInt   (const string& str, int base = 10);
    static long          StringToLong   (const string& str, int base = 10);
    static unsigned long StringToULong  (const string& str, int base = 10);
    static double        StringToDouble (const string& str);

    /// X-to-String conversion functions
    static string IntToString(long value, bool sign=false);
    static string UIntToString(unsigned long value);
    static string DoubleToString(double value);
    static string PtrToString(const void* ptr);

    /// Return one of: 'true, 'false'
    static const string& BoolToString(bool value);
    /// Can recognize (case-insensitive) one of:  'true, 't', 'false', 'f'
    static bool          StringToBool(const string& str);


    /// String comparison
    enum ECase {
        eCase,
        eNocase  /// ignore character case
    };

    /// ATTENTION.  Be aware that:
    ///
    /// 1) "Compare***(..., SIZE_TYPE pos, SIZE_TYPE n, ...)" functions
    ///    follow the ANSI C++ comparison rules a la "basic_string::compare()":
    ///       str[pos:pos+n) == pattern   --> return 0
    ///       str[pos:pos+n) <  pattern   --> return negative value
    ///       str[pos:pos+n) >  pattern   --> return positive value
    ///
    /// 2) "strn[case]cmp()" functions follow the ANSI C comparison rules:
    ///       str[0:n) == pattern[0:n)   --> return 0
    ///       str[0:n) <  pattern[0:n)   --> return negative value
    ///       str[0:n) >  pattern[0:n)   --> return positive value

    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);
    static int CompareCase(const char* s1, const char* s2);

    static int CompareCase(const string& s1, const string& s2);

    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);
    static int CompareNocase(const char* s1, const char* s2);

    static int CompareNocase(const string& s1, const string& s2);

    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);
    static int Compare(const char* s1, const char* s2,
                       ECase use_case = eCase);
    static int Compare(const string& s1, const char* s2,
                       ECase use_case = eCase);
    static int Compare(const char* s1, const string& s2,
                       ECase use_case = eCase);
    static int Compare(const string& s1, const string& s2,
                       ECase use_case = eCase);

    // NOTE.  On some platforms, "strn[case]cmp()" can work faster than their
    //        "Compare***()" counterparts.
    static int strcmp      (const char* s1, const char* s2);
    static int strncmp     (const char* s1, const char* s2, size_t n);
    static int strcasecmp  (const char* s1, const char* s2);
    static int strncasecmp (const char* s1, const char* s2, size_t n);


    /// The following 4 methods change the passed string, then return it
    static string& ToLower(string& str);
    static char*   ToLower(char*   str);
    static string& ToUpper(string& str);
    static char*   ToUpper(char*   str);
    // ...and these two are just dummies to prohibit passing constant C strings
private:
    static void/*dummy*/ ToLower(const char* /*dummy*/);
    static void/*dummy*/ ToUpper(const char* /*dummy*/);
public:

    static bool StartsWith(const string& str, const string& start,
                           ECase use_case = eCase);
    static bool EndsWith(const string& str, const string& end,
                         ECase use_case = eCase);

    enum ETrunc {
        eTrunc_Begin,  /// truncate leading  spaces only
        eTrunc_End,    /// truncate trailing spaces only
        eTrunc_Both    /// truncate spaces at both begin and end of string
    };
    static string TruncateSpaces(const string& str, ETrunc where=eTrunc_Both);

    /// Starting from position "start_pos", replace no more than "max_replace"
    /// occurrences of substring "search" by string "replace".
    /// If "max_replace" is zero -- then replace all occurrences.
    /// Result will be put to string "dst", and it will be returned as well.
    static string& Replace(const string& src,
                           const string& search,
                           const string& replace,
                           string& dst,
                           SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    /// The same as the above Replace(), but return new string
    static string Replace(const string& src,
                          const string& search,
                          const string& replace,
                          SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    /// Split string "str" using symbols from "delim" as delimiters
    /// Add the resultant tokens to the list "arr". Return "arr".
    static list<string>& Split(const string& str,
                               const string& delim,
                               list<string>& arr);

    /// Make a printable version of "str". The non-printable characters will
    /// be represented as "\r", "\n", "\v", "\t", "\0", "\\", or
    /// "\xDD" where DD is the character's code in hexadecimal.
    static string PrintableString(const string& str);
}; // class NStr



/////////////////////////////////////////////////////////////////////////////
//  Predicates
//


/// Case-sensitive string comparison
struct PCase
{
    /// Return difference between "s1" and "s2"
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool operator()(const string& s1, const string& s2) const;
};


/// Case-INsensitive string comparison
struct PNocase
{
    /// Return difference between "s1" and "s2"
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2 ignoring case
    bool operator()(const string& s1, const string& s2) const;
};



/////////////////////////////////////////////////////////////////////////////
//  Algorithms
//


/// Check equivalence of arguments using predicate
template<class Arg1, class Arg2, class Pred>
inline
bool AStrEquiv(const Arg1& x, const Arg2& y, Pred pr)
{
    return pr.Equals(x, y);
}





/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CNcbiEmptyString::
//

inline
const string& CNcbiEmptyString::Get(void)
{
    const string* str = m_Str;
    return str ? *str: FirstGet();
}



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

#elif defined(HAVE_STRCASECMP)
    return ::strcasecmp(s1, s2);

#else
    int diff = 0;
    for ( ;; ++s1, ++s2) {
        char c1 = *s1;
        // calculate difference
        diff = toupper(c1) - toupper(*s2);
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

#elif defined(HAVE_STRCASECMP)
    return ::strncasecmp(s1, s2, n);

#else
    int diff = 0;
    for ( ; ; ++s1, ++s2, --n) {
        char c1 = *s1;
        // calculate difference
        diff = toupper(c1) - toupper(*s2);
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
        if (n == 0)
           return 0;
    }
    return diff;
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
bool NStr::StartsWith(const string& str, const string& start, ECase use_case)
{
    return str.size() >= start.size()  &&
        Compare(str, 0, start.size(), start, use_case) == 0;
}

inline
bool NStr::EndsWith(const string& str, const string& end, ECase use_case)
{
    return str.size() >= end.size()  &&
        Compare(str, str.size() - end.size(), end.size(), end, use_case) == 0;
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



/////////////////////////////////////////////////////////////////////////////
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


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
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
