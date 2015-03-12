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
 * File Description:   Sample simple HTML program
 *
 */

// Toolkit public headers
#include <ncbi_pch.hpp>
#include <cgi/ncbicgir.hpp>
#include <html/html.hpp>


// This turns on the NCBI namespace
USING_NCBI_SCOPE; 


static int s_Demo(void)
{
    CCgiResponse Response; // Used to write out the html
    CNodeRef     Html;     // The following are tags used in the page.
    CNodeRef     Body;
    CNodeRef     Form;

    try {
        // Write out the Content-type header
        Response.WriteHeader();

        // Create the tags
        Html = new CHTML_html();
        Body = new CHTML_body();
        Form = new CHTML_form("cgidemo", CHTML_form::eGet);

        // Stick them together
        Html->AppendChild(Body);
        Body->AppendChild(Form);
        Form->AppendChild(new CHTML_text("name"));
        Form->AppendChild(new CHTML_submit("Submit"));

        // Print out the results
        Html->Print(Response.out());
        Response.Flush();

        return 0;  
    }
    // Check to see if there were any errors
    catch (exception& exc) { 
        // Deallocate memory in case of error
        NcbiCerr << "\n" << exc.what() << NcbiEndl;
    }
    // Error
    return 1;
}


int main() 
{
    IOS_BASE::sync_with_stdio(false);
    return s_Demo();
}
