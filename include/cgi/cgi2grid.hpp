#ifndef CGI___CGI2GRID__HPP
#define CGI___CGI2GRID__HPP

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
* Author:  Maxim Didenko
*
*/

#include <stddef.h>
#include <corelib/ncbistre.hpp>
#include <cgi/ncbicgi.hpp>

#include <string>


BEGIN_NCBI_SCOPE

NCBI_XCGI_EXPORT
CNcbiOstream& CGI2GRID_ComposeHtmlPage(CNcbiOstream&    os,
                                       const CCgiRequest&  cgi_request,
                                       const string&       project_name,
                                       const string&       return_url,
                                       const string&       cgi_args);

NCBI_XCGI_EXPORT 
CNcbiOstream& CGI2GRID_ComposeHtmlPage(
                                  CNcbiOstream&                os,
                                  const CCgiRequest&              cgi_request,
                                  const string&                   project_name,
                                  const string&                   return_url,
                                  const multimap<string, string>& cgi_args);

NCBI_XCGI_EXPORT
CNcbiOstream& CGI2GRID_ComposeHtmlPage(CNcbiOstream&      os,
                                       const CCgiRequest& cgi_request,
                                       const string&      project_name,
                                       const string&      return_url,
                                       const TCgiEntries& cgi_args);

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/28 17:40:53  didenko
 * Added CGI2GRID_ComposeHtmlPage(...) fuctions
 *
 * ===========================================================================
 */

#endif  /* CGI___CGI2GRID__HPP */
