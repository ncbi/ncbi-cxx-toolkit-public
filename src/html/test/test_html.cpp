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
* Author:  Vladimir Ivanov
*
* File Description:
*   Play around with the HTML library
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/07/16 13:57:43  ivanov
* Added test for JavaScript popup menus (CHTMLPopupMenu).
* Chanded application skeleton, now it based on CNCBIApplication.
*
* Revision 1.1  2001/06/08 19:05:27  ivanov
* Initialization. Test application for html library
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>

#include <html/html.hpp>
#include <html/page.hpp>
#include <html/jsmenu.hpp>



USING_NCBI_SCOPE;


/////////////////////////////////
// SpecialChars test
//

static void s_TEST_SpecialChars(void)
{
    CHTMLPage page("Simple test page"); 

    page.AppendChild(new CHTML_nbsp(2));
    page.AppendChild(new CHTML_gt);
    page.AppendChild(new CHTML_lt);
    page.AppendChild(new CHTML_quot);
    page.AppendChild(new CHTML_amp);
    page.AppendChild(new CHTML_copy);
    page.AppendChild(new CHTML_reg);
    page.AppendChild(new CHTMLSpecialChar("#177","+/-"));

    page.Print(cout, CNCBINode::eHTML);
    cout << endl;
    page.Print(cout, CNCBINode::ePlainText);
    cout << endl;

}


/////////////////////////////////
// Tags test
//

static void s_TEST_Tags(void)
{
    CNodeRef html, head, body;

    html = new CHTML_html;
    head = new CHTML_head;
    body = new CHTML_body;

    html->AppendChild(head); 
    html->AppendChild(body); 

    head->AppendChild(
          new CHTML_meta(CHTML_meta::eName,"author","I'm"));
    head->AppendChild(
          new CHTML_meta(CHTML_meta::eHttpEquiv,"refresh","3; URL=www.ru"));
    body->AppendChild(
          new CHTML_p(new CHTML_i("paragraph")));
    body->AppendChild(
          new CHTML_img("url","alt"));
    body->AppendChild(
          new CHTMLDualNode(new CHTML_p("Example1"),"Example1\n"));
    body->AppendChild(
          new CHTMLDualNode("<p>Example2</p>","Example2\n"));
    body->AppendChild(
          new CHTML_script("text/javascript","http://localhost/sample.js"));

    CHTML_script* script = new CHTML_script("text/vbscript");
    script->AppendScript("Place VB1 script here");
    script->AppendScript("Place VB2 script here");
    body->AppendChild(script);


    html->Print(cout);
    cout << endl << endl;

    html->Print(cout,CNCBINode::ePlainText);
    cout << endl;
}


/////////////////////////////////
// JSMenu test
//

static void s_TEST_JSMenu(void)
{
    // Create HTML page skeleton with HEAD and BODY
    CHTML_html* html = new CHTML_html;
    CHTML_head* head = new CHTML_head;
    CHTML_body* body = new CHTML_body;

    // NOTE: the BODY will be added to the HTML automatically in
    // InitPopupMenus().
    html->AppendChild(head); 

    // Create one menu
    CHTMLPopupMenu* m1 = new CHTMLPopupMenu("Menu1");

    m1->AddItem("Red"  , "document.bgColor='red'");
    m1->AddItem("Blue" , "document.bgColor='blue'");
    m1->AddSeparator();
    m1->AddItem("Green", "document.bgColor='green'");

    m1->SetAttribute(eHTML_PM_fontColor, "black");
    m1->SetAttribute(eHTML_PM_fontColorHilite, "yellow");

    // Create another menu
    CHTMLPopupMenu* m2 = new CHTMLPopupMenu("Menu2");

    m2->AddItem("url 1", "top.location='http://www.microsoft.com'");
    m2->AddItem("url 2", "frame.location='http://www.netscape.com'");
    m2->AddItem("url 3", "location='/index.html'");

    m2->SetAttribute(eHTML_PM_disableHide, "true");

    // Append one paragraph
    body->AppendChild(new CHTML_p("paragraph 1"));

    // Add menus to the page
    html->InitPopupMenus(*head, *body, "menu.js");
    html->AddPopupMenu(*m1);
    html->AddPopupMenu(*m2);

    // Append another paragraph
    body->AppendChild(new CHTML_p("paragraph 2"));

    // Add menus call
    CHTML_a* anchor1 = new CHTML_a("javascript:" + m1->ShowMenu(), "Menu 1");
    body->AppendChild(anchor1);

    CHTML_a* anchor2 = new CHTML_a("javascript:" + m2->ShowMenu(), "Menu 2");
    anchor2->SetEventHandler(eHTML_EH_MouseOver, m2->ShowMenu());
    body->AppendChild(anchor2);
    
    // Print in HTML format
    html->Print(cout);
    cout << endl << endl;
}


////////////////////////////////
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
}


int CTestApplication::Run(void)
{
    cout << "---------------------------------------------" << endl;

    s_TEST_SpecialChars();

    cout << "---------------------------------------------" << endl;

    s_TEST_Tags();

    cout << "---------------------------------------------" << endl;

    s_TEST_JSMenu();

    cout << "---------------------------------------------" << endl;

    return 0;
}

  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

static CTestApplication theTestApplication;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv, 0, eDS_Default, 0);
}
