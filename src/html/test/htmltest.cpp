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
* Revision 1.7  1998/12/28 16:48:12  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.6  1998/12/24 16:15:43  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.5  1998/12/23 21:21:06  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.4  1998/12/23 14:28:12  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.3  1998/12/21 22:25:06  vasilche
* A lot of cleaning.
*
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
        : CSimpleCgiApp(argc, argv)
        {
        }

    virtual int Run(void);
};

extern "C" int main(int argc, char *argv[])
{
    int result;

    CMyApp app( argc, argv );

    app.Init(); 
    result = app.Run();
    app.Exit(); 

    return result;
}

SFactoryList < CHTMLBasicPage > PageList [] = {
    { CPmDocSumPage::New, "docsum", 0 },
    { CPmFrontPage::New, "", 0 }
};


int CMyApp::Run(void) 
{
    CFactory < CHTMLBasicPage > Factory;

    NcbiCout << "Content-TYPE: text/html\n\n";  // CgiResponse

    try {
        int i = Factory.CgiFactory( (TCgiEntries&)(GetRequest()->GetEntries()), PageList);
        CHTMLBasicPage * Page = (PageList[i].pFactory)();
        Page->SetApplication(this);
        Page->SetStyle(PageList[i].Style);
/*        Page->AppendChild(new CHTML_p(new CHTML_b("bold paragraph")));
        CHTML_form* form;
        Page->AppendChild(form = new CHTML_form("FORM"));
        form->AppendChild(new CHTML_textarea("area", 10, 10));
        form->AppendChild(new CHTML_textarea("area1", 10, 10, "area1"));
        form->AppendChild(new CHTML_select("Select")->AppendOption("One", "ONE")->AppendOption("Two")->AppendOption("THREE", "Three"));
        Page->AppendChild(new CHTML_ol("A")->AppendItem("item 1")->AppendItem("item 2")->AppendItem("Item 3")->AppendItem("Fourth item"));
        Page->AppendChild(new CHTMLComment("this is comment"));*/
        Page->Print(NcbiCout);  // serialize it
        delete Page;
    }
    catch (exception exc) {
        NcbiCerr << "\n" << exc.what() << NcbiEndl;
    }

    return 0;  
}


END_NCBI_SCOPE
