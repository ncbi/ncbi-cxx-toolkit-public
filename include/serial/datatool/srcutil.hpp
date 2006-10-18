#ifndef SRCUTIL__HPP
#define SRCUTIL__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

// return valid C name
string Identifier(const string& typeName, bool capitalize = true);

// check if strstream is empty
bool Empty(const CNcbiOstrstream& code);
// write strstream content
CNcbiOstream& Write(CNcbiOstream& out, const CNcbiOstrstream& code);

// add tabulation to string
string Tabbed(const string& code, const char* tab = 0);

// write adding tabulation
CNcbiOstream& WriteTabbed(CNcbiOstream& out, const string& code,
                          const char* tab = 0);
CNcbiOstream& WriteTabbed(CNcbiOstream& out, const CNcbiOstrstream& code,
                          const char* tab = 0);

// start new line in ASN.1
CNcbiOstream& PrintASNNewLine(CNcbiOstream& out, int indent);

//#include <serial/datatool/srcutil.inl>

END_NCBI_SCOPE

#endif  /* SRCUTIL__HPP */
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2006/10/18 13:04:42  gouriano
* Moved Log to bottom
*
* Revision 1.2  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.1  2000/11/29 17:42:30  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* ===========================================================================
*/
