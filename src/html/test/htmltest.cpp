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
* Revision 1.2  1998/12/11 22:43:35  lewisg
* added docsum page
*
* Revision 1.1  1998/12/11 18:11:17  lewisg
* frontpage added
*

*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgi.hpp>
#include <html.hpp>
#include <page.hpp>
#include <factory.hpp>
#include <cgiapp.hpp>
#include <components.hpp>
#include <querypages.hpp>
BEGIN_NCBI_SCOPE

class CMyApp : public CSimpleCgiApp
{
public:
    CMyApp(int argc = 0, char** argv = 0)
        : CSimpleCgiApp(argc, argv) {}
    virtual int Run(void);
};


extern "C" int main(int argc, char *argv[])
{
    int result;

    CMyApp app( argc, argv );
    try {
	app.Init(); 
	result = app.Run();
	app.Exit(); 
    } catch(...) {}

    return result;
}

SFactoryList < CHTMLBasicPage > PageList [] = {
    { CPmDocSumPage::New, "docsum", 0 },
    { CPmFrontPage::New, "", 0 }
};


int CMyApp::Run(void) 
{
    CFactory < CHTMLBasicPage > Factory;
    int i;
    CHTMLBasicPage * Page;

    NcbiCout << "Content-TYPE: text/html\n\n";  // CgiResponse

    try {    	
	i = Factory.CgiFactory( (TCgiEntries&)(GetRequest()->GetEntries()), PageList);
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
