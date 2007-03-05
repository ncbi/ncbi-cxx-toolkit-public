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
 * Author:  Lewis Geer
 *
 * File Description:
 *   sample cgi program
 *
 */

// toolkit public headers
#include <ncbi_pch.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;  // this turns on the ncbi namespace


int main(int argc, char *argv[]) 
{
    CCgiRequest Request(argc, argv);
    CCgiResponse Response;

    // write out the Content-type header
    Response.WriteHeader();

    // Get the multimap.  see http://www.sgi.com/Technology/STL/Multimap.html
    // on how to manipulate multimaps
    TCgiEntries Entries = Request.GetEntries();

    // this program expects queries of the form cgidemo?name=Fred
    // the following line extracts the "Fred"
    string Name;
    TCgiEntries::const_iterator iName = Entries.find("name");
    if (iName == Entries.end()) Name = "World"; 
    else Name = iName->second;

    // print out the results
    Response.out() << "<html><body>Hello, " << Name;
    Response.out() << "</body></html>" << endl;
    Response.Flush();

    return 0;  
}
