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
 * Author:  Vladimir Ivanov, Denis Vakatov
 *
 * File Description:   Test Javascript popup menus
 * 
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <html/page.hpp>
#include <html/jsmenu.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



/////////////////////////////////
// Standart HTML test
//

static void Test_Html(void)
{
    LOG_POST("\nHTML test\n");

    // Create HTML page skeleton with HEAD and BODY
    CHTML_html* html = new CHTML_html;
    CHTML_head* head = new CHTML_head;
    CHTML_body* body = new CHTML_body;

    html->AppendChild(head); 
    html->AppendChild(body); 

    // Create one menu
    CHTMLPopupMenu* m1 = new CHTMLPopupMenu("Menu1");

    m1->AddItem("Red"  , "document.bgColor='red'");
    m1->AddItem("White" , "document.bgColor='white'");
    m1->AddSeparator();
    m1->AddItem("Green", "document.bgColor='green'");
    m1->SetAttribute(eHTML_PM_fontColor, "black");
    m1->SetAttribute(eHTML_PM_fontColorHilite, "yellow");

    // !!! We can add menu to BODY only!
    body->AppendChild(m1);

    // Create another menu
    CHTMLPopupMenu* m2 = new CHTMLPopupMenu("Menu2");

    m2->AddItem("NCBI", "top.location='http://ncbi.nlm.nih.gov'");
    m2->AddItem("Netscape", "top.location='http://www.netscape.com'");
    m2->AddItem("Microsoft", "top.location='http://www.microsoft.com'");
    m2->SetAttribute(eHTML_PM_disableHide, "true");

    body->AppendChild(m2);

    // Create menu with HTML items
    CHTMLPopupMenu* m3 = new CHTMLPopupMenu("Menu3");

    m3->AddItem("<b>Bold item</b>");
    m3->AddItem("<img src='someimage.gif'> Image item");

    CHTML_a* anchor = new CHTML_a("some link...", "Link item");
    m3->AddItem(*anchor);

    body->AppendChild(m3);

    // Add menus call
    CHTML_a* anchor1 = new CHTML_a("javascript:" + m1->ShowMenu(),
                                   "Color Menu");
    body->AppendChild(anchor1);
    body->AppendChild(new CHTML_p(""));

    CHTML_a* anchor2 = new CHTML_a("javascript:" + m2->ShowMenu(), "URL Menu");
    anchor2->SetEventHandler(eHTML_EH_MouseOver, m2->ShowMenu());
    body->AppendChild(anchor2);
    body->AppendChild(new CHTML_p(""));

    CHTML_a* anchor3 = new CHTML_a("javascript:" + m3->ShowMenu(),
                                   "HTML Menu");
    body->AppendChild(anchor3);

    // Enable using popup menus (we can skip call this function)
    //html->EnablePopupMenu();
    
    // Print in HTML format
    html->Print(cout);
    cout << endl << endl;
}


/////////////////////////////////
// Template test
//

static void Test_Template(void)
{
    LOG_POST("\nTemplate test\n");

    CHTMLPage page("JSMenu test page","template_jsmenu.html"); 
    CNCBINode* view = new CNCBINode();

    // Create one menu
    CHTMLPopupMenu* m1 = new CHTMLPopupMenu("Menu1");
    m1->AddItem("Red"  , "document.bgColor='red'");
    m1->AddItem("White" , "document.bgColor='white'");
    m1->AddSeparator();
    m1->AddItem("Green", "document.bgColor='green'");
    m1->SetAttribute(eHTML_PM_fontColor, "black");
    m1->SetAttribute(eHTML_PM_fontColorHilite, "yellow");

    view->AppendChild(m1);

    // Create another menu
    CHTMLPopupMenu* m2 = new CHTMLPopupMenu("Menu2");
    m2->AddItem("NCBI", "top.location='http://ncbi.nlm.nih.gov'");
    m2->AddItem("Netscape", "top.location='http://www.netscape.com'");
    m2->AddItem("Microsoft", "top.location='http://www.microsoft.com'");
    m2->SetAttribute(eHTML_PM_disableHide, "true");

    view->AppendChild(m2);

    // Enable using popup menus (we can skip call this function)
    // page.EnablePopupMenu();

    // Add menus call
    CHTML_a* anchor1 = new CHTML_a("javascript:" + m1->ShowMenu(),
                                   "Color Menu");
    view->AppendChild(anchor1);

    view->AppendChild(new CHTML_br());

    CHTML_a* anchor2 = new CHTML_a("javascript:" + m2->ShowMenu(), "URL Menu");
    anchor2->SetEventHandler(eHTML_EH_MouseOver, m2->ShowMenu());
    view->AppendChild(anchor2);

    page.AddTagMap("VIEW", view);

    // Enable using popup menus (we can skip call this function)
    //page.EnablePopupMenu();

    // Print test result
    page.Print(cout, CNCBINode::eHTML);
    cout << endl << endl;
}


/////////////////////////////////
// Test application
//


class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test JavaScript Menu");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("feature", "HTML-specific feature to test",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("feature", &(*new CArgAllow_Strings, "html", "template"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

}


int CTestApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    if (args["feature"].AsString() == "html") {
        Test_Html();
    }
    else if (args["feature"].AsString() == "template") {
        Test_Template();
    }
    else {
        _TROUBLE;
    }

    return 0;
}

  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

static CTestApplication theTestApplication;

int main(int argc, const char* argv[])
{
    return theTestApplication.AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2002/04/16 19:05:21  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.4  2002/02/13 20:19:44  ivanov
 * Added support of dynamic popup menus
 *
 * Revision 1.3  2001/08/15 19:42:39  ivanov
 * Added test for use method AddMenuItem( node, ...)
 *
 * Revision 1.2  2001/08/14 17:02:00  ivanov
 * Changed tests for popup menu in connection with change means init menu.
 * Added second test for use menu in HTML templates.
 *
 * Revision 1.1  2001/07/23 21:50:33  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
