#ifndef NCBISTD__HPP
#define NCBISTD__HPP

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
* File Description:
*   The NCBI C++ standard #include's and #defin'itions
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.45  2000/12/12 14:20:14  vasilche
* Added operator bool to CArgValue.
* Added standard typedef element_type to CRef<> and CConstRef<>.
* Macro iterate() now calls method end() only once and uses temporary variable.
* Various NStr::Compare() methods made faster.
* Added class Upcase for printing strings to ostream with automatic conversion.
*
* Revision 1.44  2000/12/11 20:42:48  vakatov
* + NStr::PrintableString()
*
* Revision 1.43  2000/11/07 04:07:19  vakatov
* kEmptyCStr, kEmptyStr (equiv. to NcbiEmptyCStr,NcbiEmptyString)
*
* Revision 1.42  2000/10/05 20:01:10  vakatov
* auto_ptr -- no "const" in constructor and assignment
*
* Revision 1.41  2000/08/03 20:21:10  golikov
* Added predicate PCase for AStrEquiv
* PNocase, PCase goes through NStr::Compare now
*
* Revision 1.40  2000/07/19 19:03:52  vakatov
* StringToBool() -- short and case-insensitive versions of "true"/"false"
* ToUpper/ToLower(string&) -- fixed
*
* Revision 1.39  2000/04/17 19:30:12  vakatov
* Allowed case-insensitive comparison for StartsWith() and EndsWith()
*
* Revision 1.38  2000/04/17 04:14:20  vakatov
* NStr::  extended Compare(), and allow case-insensitive string comparison
* NStr::  added ToLower() and ToUpper()
*
* Revision 1.37  2000/04/04 22:28:06  vakatov
* NStr::  added conversions for "long"
*
* Revision 1.36  2000/02/17 20:00:24  vasilche
* Added EResetVariant enum for serialization package.
*
* Revision 1.35  2000/01/20 16:24:20  vakatov
* Kludging around the "NcbiEmptyString" to ensure its initialization when
* it is used by the constructor of a statically allocated object
* (I believe that it is actually just another Sun WorkShop compiler "feature")
*
* Revision 1.34  1999/12/28 19:04:22  vakatov
* #HAVE_NO_MINMAX_TEMPLATE
*
* Revision 1.33  1999/12/28 18:55:25  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.32  1999/12/17 19:04:06  vasilche
* NcbiEmptyString made extern.
*
* Revision 1.31  1999/12/03 21:36:45  vasilche
* Added forward decaration of CEnumeratedTypeValues
*
* Revision 1.30  1999/11/26 18:45:08  golikov
* NStr::Replace added
*
* Revision 1.29  1999/11/17 22:05:02  vakatov
* [!HAVE_STRDUP]  Emulate "strdup()" -- it's missing on some platforms
*
* Revision 1.28  1999/10/26 18:10:24  vakatov
* [auto_ptr] -- simpler and more standard
*
* Revision 1.27  1999/09/29 22:22:37  vakatov
* [auto_ptr] Use "mutable" rather than "static_cast" on m_Owns; fixed a
*            double "delete" bug in reset().
*
* Revision 1.26  1999/09/14 18:49:40  vasilche
* Added forward declaration of CTypeInfo class.
*
* Revision 1.25  1999/07/08 14:44:52  vakatov
* Tiny fix in EndsWith()
*
* Revision 1.24  1999/07/06 15:21:04  vakatov
* + NStr::TruncateSpaces(const string& str, ETrunc where=eTrunc_Both)
*
* Revision 1.23  1999/06/21 15:59:40  vakatov
* [auto_ptr] -- closer to standard:  added an ownership and
* initialization/assignment with "auto_ptr<>&", made "release()" be "const"
*
* Revision 1.22  1999/06/15 20:50:03  vakatov
* NStr::  +BoolToString, +StringToBool
*
* Revision 1.21  1999/05/28 20:12:29  vakatov
* [HAVE_NO_AUTO_PTR]  Prohibit "operator=" in the home-made "auto_ptr::"
*
* Revision 1.20  1999/04/16 17:45:31  vakatov
* [MSVC++] Replace the <windef.h>'s min/max macros by the hand-made templates.
*
* Revision 1.19  1999/04/15 21:56:47  vakatov
* Introduced NcbiMin/NcbiMax to workaround some portability issues with
* the standard "min/max"
*
* Revision 1.18  1999/04/14 21:20:31  vakatov
* Dont use "snprintf()" as it is not quite portable yet
*
* Revision 1.17  1999/04/14 19:46:01  vakatov
* Fixed for the features:
*    { NCBI_OBSOLETE_STR_COMPARE, HAVE_NO_AUTO_PTR, HAVE_NO_SNPRINTF }
*
* Revision 1.16  1999/04/09 19:51:36  sandomir
* minor changes in NStr::StringToXXX - base added
*
* Revision 1.15  1999/01/21 16:18:04  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.14  1999/01/11 22:05:45  vasilche
* Fixed CHTML_font size.
* Added CHTML_image input element.
*
* Revision 1.13  1998/12/28 17:56:29  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.12  1998/12/21 17:19:36  sandomir
* VC++ fixes in ncbistd; minor fixes in Resource
*
* Revision 1.11  1998/12/17 21:50:43  sandomir
* CNCBINode fixed in Resource; case insensitive string comparison predicate added
*
* Revision 1.10  1998/12/15 17:38:16  vasilche
* Added conversion functions string <> int.
*
* Revision 1.9  1998/12/04 23:36:30  vakatov
* + NcbiEmptyCStr and NcbiEmptyString (const)
*
* Revision 1.8  1998/11/06 22:42:38  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbiexpt.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// Empty "C" string (points to a '\0')
extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


// Empty "C++" string
class CNcbiEmptyString {
public:
    const string& Get(void) {
        return m_Str ? *m_Str : FirstGet();
    }
private:
    static const string& FirstGet(void);
    static const string* m_Str;
};
#define NcbiEmptyString NCBI_NS_NCBI::CNcbiEmptyString().Get()
#define kEmptyStr       NcbiEmptyString



// String-processing utilities
struct NStr {
    // conversion functions (throw an exception on the conversion error)
    static int StringToInt(const string& str, int base = 10);
    static unsigned int StringToUInt(const string& str, int base = 10);
    static long StringToLong(const string& str, int base = 10);
    static unsigned long StringToULong(const string& str, int base = 10);
    static double StringToDouble(const string& str);
    static string IntToString(long value, bool sign=false);
    static string UIntToString(unsigned long value);
    static string DoubleToString(double value);
    static string PtrToString(const void* ptr);

    // 'true, 'false'
    static string BoolToString(bool value);
    // 'true, 't', 'false', 'f'  (case-insensitive)
    static bool   StringToBool(const string& str);

    /*  str[pos:pos+n) == pattern  --> return 0
     *  str[pos:pos+n) <  pattern  --> return negative value
     *  str[pos:pos+n) >  pattern  --> return positive value
     */
    enum ECase {
        eCase,
        eNocase  /* ignore character case */
    };
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);
    static int CompareCase(const char* s1, const char* s2);
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);
    static int CompareNocase(const char* s1, const char* s2);

    static inline int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                              const char* pattern, ECase use_case = eCase)
        {
            if ( use_case == eCase )
                return CompareCase(str, pos, n, pattern);
            else
                return CompareNocase(str, pos, n, pattern);
        }
    static inline int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                              const string& pattern, ECase use_case = eCase)
        {
            if ( use_case == eCase )
                return CompareCase(str, pos, n, pattern);
            else
                return CompareNocase(str, pos, n, pattern);
        }
    static inline int Compare(const char* s1, const char* s2,
                              ECase use_case = eCase)
        {
            if ( use_case == eCase )
                return CompareCase(s1, s2);
            else
                return CompareNocase(s1, s2);
        }
    static inline int Compare(const string& s1, const char* s2,
                              ECase use_case = eCase)
        {
            return Compare(s1, 0, NPOS, s2, use_case);
        }
    static inline int Compare(const char* s1, const string& s2,
                              ECase use_case = eCase)
        {
            return Compare(s2, 0, NPOS, s1, use_case);
        }
    static inline int Compare(const string& s1, const string& s2,
                              ECase use_case = eCase)
        {
            return Compare(s1, 0, NPOS, s2, use_case);
        }
    
    // these 4 methods change the passed string, then return it
    static string& ToLower(string& str);
    static char*   ToLower(char*   str);
    static string& ToUpper(string& str);
    static char*   ToUpper(char*   str);

    static inline bool StartsWith(const string& str, const string& start,
                                  ECase use_case = eCase) {
        return str.size() >= start.size()  &&
            Compare(str, 0, start.size(), start, use_case) == 0;
    }
    
    static inline bool EndsWith(const string& str, const string& end,
                                  ECase use_case = eCase) {
        return str.size() >= end.size()  &&
            Compare(str, str.size() - end.size(), end.size(),
                    end, use_case) == 0;
    }

    enum ETrunc {
        eTrunc_Begin,  // truncate leading  spaces only
        eTrunc_End,    // truncate trailing spaces only
        eTrunc_Both    // truncate spaces at both begin and end of string
    };
    static string TruncateSpaces(const string& str, ETrunc where=eTrunc_Both);

    // starting from position "start_pos", replace no more than "max_replace"
    // occurencies of substring "search" by string "replace"
    // if "max_replace" is zero -- then replace all occurences.
    static string& Replace(const string& src,
                           const string& search,
                           const string& replace,
                           string& dst,
                           SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    // the same as the above Replace(), but return new string
    static string Replace(const string& src,
                          const string& search,
                          const string& replace,
                          SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    // Make a printable version of "str". The non-printable characters will
    // be represented as "\r", "\n", "\v", "\t", "\0", "\\", or
    // "\xDD" where DD is the character's code in hexadecimal.
    static string PrintableString(const string& str);
}; // struct NStr


// predicates

// case-sensitive string comparison
struct PCase
{
    // return difference
    int Compare(const string& s1, const string& s2) const
        {
            return NStr::Compare(s1, s2, NStr::eCase);
        }

    // returns true if s1 < s2
    bool Less(const string& s1, const string& s2) const
        {
            return Compare(s1, s2) < 0;
        }

    // returns true if s1 == s2
    bool Equals(const string& s1, const string& s2) const
        {
            return Compare(s1, s2) == 0;
        }

    // returns true if s1 < s2
    bool operator()(const string& s1, const string& s2) const
        {
            return Less(s1, s2);
        }
};

// case-INsensitive string comparison
struct PNocase
{
    // return difference
    int Compare(const string& s1, const string& s2) const
        {
            return NStr::Compare(s1, s2, NStr::eNocase);
        }

    // returns true if s1 < s2
    bool Less(const string& s1, const string& s2) const
        {
            return Compare(s1, s2) < 0;
        }

    // returns true if s1 == s2
    bool Equals(const string& s1, const string& s2) const
        {
            return Compare(s1, s2) == 0;
        }

    // returns true if s1 < s2 ignoring case
    bool operator()(const string& s1, const string& s2) const
        {
            return Less(s1, s2);
        }
};

// algorithms

template<class Pred>
inline
bool AStrEquiv( const string& x, const string& y, Pred pr)
{  
    return pr.Equals(x, y);
}

class CTypeInfo;
class CEnumeratedTypeValues;

enum EResetVariant {
    eDoResetVariant,
    eDoNotResetVariant
};

// auto_ptr

#if defined(HAVE_NO_AUTO_PTR)
template <class X> class auto_ptr {
private:
    X* m_Ptr;

public:
    explicit auto_ptr(X* p = 0) : m_Ptr(p) {}
    auto_ptr(auto_ptr<X>& a) : m_Ptr(a.release()) {}
    auto_ptr<X>& operator=(auto_ptr<X>& a) {
        if (this != &a) {
            if (m_Ptr  &&  m_Ptr != a.m_Ptr) {
                delete m_Ptr;
            }
            m_Ptr = a.release();
        }
        return *this;
    }
    ~auto_ptr(void) {
        if ( m_Ptr )
            delete m_Ptr;
    }

    X&  operator*(void)         const { return *m_Ptr; }
    X*  operator->(void)        const { return m_Ptr; }
    int operator==(const X* p)  const { return (m_Ptr == p); }
    X*  get(void)               const { return m_Ptr; }

    X* release(void) {
        X* x_Ptr = m_Ptr;  m_Ptr = 0;  return x_Ptr;
    }

    void reset(X* p = 0) {
        if (m_Ptr != p) {
            delete m_Ptr;
            m_Ptr = p;
        }
    }
};
#endif /* HAVE_NO_AUTO_PTR */


// "min" and "max" templates
#if defined(HAVE_NO_MINMAX_TEMPLATE)
#  define NOMINMAX
#  ifdef min
#    undef min
#  endif
#  ifdef max
#    undef max
#  endif
template <class T>
inline
const T& min(const T& a, const T& b) {
    return b < a ? b : a;
}
template <class T>
inline
const T& max(const T& a, const T& b) {
    return  a < b ? b : a;
}
#endif /* HAVE_NO_MINMAX_TEMPLATE */

// strdup()
#if !defined(HAVE_STRDUP)
extern char* strdup(const char* str);
#endif

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif /* NCBISTD__HPP */
