#ifndef NCBISTR__HPP
#define NCBISTR__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   The NCBI C++ standard methods for dealing with std::string
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>
#include <string>

BEGIN_NCBI_SCOPE

// Empty "C" string (points to a '\0')
extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


// Empty "C++" string
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
    
    // these 4 methods change the passed string, then return it
    static string& ToLower(string& str);
    static char*   ToLower(char*   str);
    static string& ToUpper(string& str);
    static char*   ToUpper(char*   str);

    static bool StartsWith(const string& str, const string& start,
                           ECase use_case = eCase);
    static bool EndsWith(const string& str, const string& end,
                         ECase use_case = eCase);

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
    int Compare(const string& s1, const string& s2) const;

    // returns true if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    // returns true if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    // returns true if s1 < s2
    bool operator()(const string& s1, const string& s2) const;
};

// case-INsensitive string comparison
struct PNocase
{
    // return difference
    int Compare(const string& s1, const string& s2) const;

    // returns true if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    // returns true if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    // returns true if s1 < s2 ignoring case
    bool operator()(const string& s1, const string& s2) const;
};

// algorithms

// Check equivalence of arguments using predicate
template<class Arg1, class Arg2, class Pred>
inline
bool AStrEquiv(const Arg1& x, const Arg2& y, Pred pr)
{  
    return pr.Equals(x, y);
}

#include <corelib/ncbistr.inl>

END_NCBI_SCOPE

#endif  /* NCBISTR__HPP */
