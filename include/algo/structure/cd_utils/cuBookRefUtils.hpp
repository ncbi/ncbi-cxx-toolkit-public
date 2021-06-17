/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Various utility functions for working with CCdd_book_ref objects
 *
 * ===========================================================================
 */

#ifndef CU_BOOK_REF_HPP
#define CU_BOOK_REF_HPP

#include <string>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <objects/cdd/Cdd_book_ref.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//  Return true if 'bookname' starts with "NBK" (case-sensitive), as
//  per the Bookshelf group's Portal-based URL scheme.
NCBI_CDUTILS_EXPORT
bool IsPortalDerivedBookRef(const CCdd_book_ref& bookRef);

//  Based on the content of the bookRef, return the parameter string appropriate
//  to br.fcgi or Portal style Bookshelf URLs.  The latter are characterized by
//  book names starting with 'NBK'.  This method no longer supports the bv.fcgi 
//  URL format, which has been retired by the Bookshelf group.
NCBI_CDUTILS_EXPORT
string CCddBookRefToString(const CCdd_book_ref& bookRef);

//  Returns format for old-style bv.fcgi URL parameters (the 'rid').
//  bv.fcgi style URLs are no longer supported by Entrez Books, but
//  the 'rid' can still be used in calls to the 'bookref.fcgi' application.
NCBI_CDUTILS_EXPORT
string CCddBookRefToBvString(const CCdd_book_ref& bookRef);

//  Convert a Bookshelf br.fcgi URL into an ASN.1 object.
//  
//  section/chapter:
//  book=<bookname>&part=<address>[#<subaddress>]
//
//  table/figure:
//  book=<bookname>&part=<address>&rendertype=<some_enum2strValue>&id=<subaddress>
//
//  For XML validation reasons, <address> and <subaddress> have a prepended 'A'
//  if the string would have otherwise been numeric.  However, for parsing br.fcgi 
//  URLs it is fine to leave the string intact w/o stripping off the initial 'A'
//  (i.e., all br.fcgi derived book references will use Celementid and Csubelementid
//   exclusively).
NCBI_CDUTILS_EXPORT
bool BrBookURLToCCddBookRef(const string& brBookUrl, CRef< CCdd_book_ref>& bookRef);

//  Returns a string formatted for use as Bookshelf's br.fcgi URL parameters,
//  or an empty string if there was a problem interpreting 'bookRef'.
NCBI_CDUTILS_EXPORT
string CCddBookRefToBrString(const CCdd_book_ref& bookRef);

//  Convert a Bookshelf Portal URL into an ASN.1 object.
//  
//  section/chapter:
//  books/<bookname>/#<elementid>
//
//  table/figure/box/glossary item:
//  books/<bookname>/<elementtype>/<elementid>/
//      -- OR --
//  books/<bookname>/<elementtype>/<elementid>/?report=objectonly
//      -- OR --
//  books/<bookname>/?rendertype=<elementtype>&id=<elementid>
//
//  The 'bookname' is the prefix 'NBK' plus an 'elementid" - typically a string
//  starting with 'A' if the remainder of the elementid is numeric.  However, in some
//  cases the elementid may have a different format for certain element types.
//  For 'chapter' legacy book refs, or pages refering to an entire page vs. a specific
//  location in a bookshelf document, the elementid may be undefined.
//  Redirection by Entrez can generate the alternate 'rendertype' URL format
//  for figures, tables, boxes, and glossary items (the latter being a 'def-item').
//  All derived book references will exclusively use the Celementid CCdd_book_ref field; 
//  the Csubelementid field no longer appears necessary in this URL scheme.
NCBI_CDUTILS_EXPORT
bool PortalBookURLToCCddBookRef(const string& portalBookUrl, CRef< CCdd_book_ref>& bookRef);

//  Returns a string formatted for use as Bookshelf's Portal-style URL parameters.
NCBI_CDUTILS_EXPORT
string CCddBookRefToPortalString(const CCdd_book_ref& bookRef);

//  Construct the 'term' parameter for an esearch.fcgi eUtils call to the books 
//  database for the given book reference object.
NCBI_CDUTILS_EXPORT
string CCddBookRefToEsearchTerm(const CCdd_book_ref& bookRef);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_BOOK_REF_HPP
