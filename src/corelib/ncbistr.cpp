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
* Author: 
*   Eugene Vasilchenko
*
* File Description:
*   Some helper functions
*
* ---------------------------------------------------------------------------
* $Log$
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
#include <errno.h>
#include <stdio.h>


BEGIN_NCBI_SCOPE


int NStr::StringToInt(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    long value = ::strtol(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||
        value < kMin_Int || value > kMax_Int)
        throw runtime_error("NStr::StringToInt():  cannot convert");
    return value;
}

unsigned int NStr::StringToUInt(const string& str, int base /* = 10 */ )
{
    errno = 0;
    char* endptr = 0;
    unsigned long value = ::strtoul(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||
	value > kMax_UInt)
        throw runtime_error("NStr::StringToUInt():  cannot convert");
    return value;
}

double NStr::StringToDouble(const string& str)
{
    errno = 0;
    char* endptr = 0;
    double value = ::strtod(str.c_str(), &endptr);
    if (errno  ||  !endptr  ||  endptr == str.c_str())
        throw runtime_error("NStr::StringToDouble():  cannot convert");
    return value;
}

string NStr::IntToString(int value)
{
    char buffer[64];
    ::sprintf(buffer, "%d", value);
    return buffer;
}

string NStr::IntToString(int value, bool sign)
{
    char buffer[64];
    ::sprintf(buffer, sign ? "%+d": "%d", value);
    return buffer;
}

string NStr::UIntToString(unsigned int value)
{
    char buffer[64];
    ::sprintf(buffer, "%u", value);
    return buffer;
}

string NStr::DoubleToString(double value)
{
    char buffer[64];
    ::sprintf(buffer, "%g", value);
    return buffer;
}


static const string kTrueString  = "true";
static const string kFalseString = "false";

string NStr::BoolToString(bool value)
{
    return value ? kTrueString : kFalseString;
}

bool NStr::StringToBool(const string& str)
{
    if ( AStrEquiv(str, kTrueString,  PNocase()) )
        return true;
    if ( AStrEquiv(str, kFalseString, PNocase()) )
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



// predicates

// case-insensitive string comparison
// operator() meaning is the same as operator<
bool PNocase::operator() (const string& x, const string& y) const
{
    string::const_iterator x_ptr = x.begin();
    string::const_iterator y_ptr = y.begin();
    
    while ( x_ptr != x.end() ) {
        if ( y_ptr == y.end() )
            return false;
        int diff = toupper(*x_ptr) - toupper(*y_ptr);
        if ( diff != 0 )
            return diff < 0;
        ++x_ptr; ++y_ptr;
    }
    return y_ptr != y.end();
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
