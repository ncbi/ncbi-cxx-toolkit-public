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
* Author:  !!! PUT YOUR NAME(s) HERE !!!
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1998/12/08 00:33:44  lewisg
* cleanup
*
* Revision 1.3  1998/12/01 23:31:14  vakatov
* Got rid of <ncbi.h> etc. ald C toolit headers
*
* Revision 1.2  1998/12/01 19:10:39  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.1  1998/11/23 23:45:21  lewisg
* *** empty log message ***
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgi.hpp>
#include <html.hpp>
#include <page.hpp>
#include <toolpages.hpp>
#include <factory.hpp>
#include <ncbiapp.hpp>
BEGIN_NCBI_SCOPE
 

extern "C" int main(int argc, char *argv[])
{
    int result;

    CCgiApplication app( argc, argv );
    try {
	app.Init(); 
	result = app.Run(); 
	app.Exit(); 
    } catch(...) {}

    return result;
}


void CCgiApplication::Init(void) {}  // probably should be defined in corelib
void CCgiApplication::Exit(void) {}

// the list of pages to be instantiated and their CGI parameters

SFactoryList < CHTMLBasicPage > PageList [] = {
    { CToolReportPage::New, "notcgi=", 0 },
    { CToolOptionPage::New, "toolname=", 0 },
    { CToolFastaPage::New, "", 0 }
};


int CCgiApplication::Run(void) 
{
    CFactory < CHTMLBasicPage > Factory;
    int i;
    CHTMLBasicPage * Page;
    // CRuntime Runtime;

    NcbiCout << "Content-TYPE: text/html\n\n";  // CgiResponse

    try {    	
	//	Runtime.m_Cgi = & m_CgiEntries;
	i = Factory.CgiFactory(m_CgiEntries, PageList);
	Page = (PageList[i].pFactory)();
	Page->SetApplication(this);
	Page->Create(PageList[i].Style);
      
	Page->Print(NcbiCout);  // serialize it

    }
    catch (...) {
	delete Page;
	throw;
    }
    delete Page;
    return 0;  
}

END_NCBI_SCOPE
