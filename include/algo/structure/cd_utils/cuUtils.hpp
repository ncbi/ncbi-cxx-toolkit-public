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
 *       Various utility functions for working with CD objects
 *
 * ===========================================================================
 */

#ifndef CU_UTILS_HPP
#define CU_UTILS_HPP

// include ncbistd.hpp, ncbiobj.hpp, ncbi_limits.h, various stl containers
#include <corelib/ncbiargs.hpp>   
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/cdd/Cdd_book_ref.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

#define i2s(i)  NStr::IntToString(i)
#define I2S(i)  NStr::IntToString(i)

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

extern const int  CDTreeColorCycle[];
extern const int  kNumColorsInCDTreeColorCycle;

NCBI_CDUTILS_EXPORT 
string GetSeqIDStr(const CSeq_id& SeqID);
NCBI_CDUTILS_EXPORT 
string GetSeqIDStr(const CRef< CSeq_id >& SeqID);
NCBI_CDUTILS_EXPORT 
string Make_SeqID_String(const CRef< CSeq_id > SeqID, bool Pad, int Len);
NCBI_CDUTILS_EXPORT 
void Make_GI_or_PDB_String_CN3D(const CRef< CSeq_id > SeqID, std::string& Str);

NCBI_CDUTILS_EXPORT 
string CddIdString(const CCdd& cdd);    //  give all Cdd_id's for this Cd
NCBI_CDUTILS_EXPORT 
string CddIdString(const CCdd_id& id);
NCBI_CDUTILS_EXPORT 
bool   SameCDAccession(const CCdd_id& id1, const CCdd_id& id2);

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

//  Returns format for Bookshelf's br.fcgi URL parameters
NCBI_CDUTILS_EXPORT
string CCddBookRefToBrString(const CCdd_book_ref& bookRef);

//  Convert a Bookshelf br.fcgi URL into an ASN.1 object.
NCBI_CDUTILS_EXPORT
bool BrBookURLToCCddBookRef(const string& brBookUrl, CRef< CCdd_book_ref>& bookRef);

//  Returns format for Bookshelf's Portal-style URL parameters
NCBI_CDUTILS_EXPORT
string CCddBookRefToPortalString(const CCdd_book_ref& bookRef);

//  Convert a Bookshelf Portal URL into an ASN.1 object.
NCBI_CDUTILS_EXPORT
bool PortalBookURLToCCddBookRef(const string& portalBookUrl, CRef< CCdd_book_ref>& bookRef);

NCBI_CDUTILS_EXPORT 
bool Prosite2Regex(const std::string& prosite, std::string* regex, std::string* errString);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_UTILS_HPP
