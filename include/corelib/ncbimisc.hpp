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

#include <ncbitype.h>
#include <ncbistl.hpp>
#include <ncbidbg.hpp>
#include <ncbiexpt.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

const char   NcbiEmptyCStr[] = "";
const string NcbiEmptyString;

// conversion functions
extern int StringToInt(const string& str);
extern unsigned int StringToUInt(const string& str);
extern double StringToDouble(const string& str);
extern string IntToString(int value);
extern string UIntToString(unsigned int value);
extern string DoubleToString(double value);

// predicates

// case-insensitive string comparison
struct PNocase
{
  bool operator() ( const string&, const string& ) const;
};

// algorithms

template<class Pred>
bool AStrEquiv( const string& x, const string& y, Pred pr )
{  
  return !( pr( x, y ) || pr( y, x ) );
}

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif /* NCBISTD__HPP */
