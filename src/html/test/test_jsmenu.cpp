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
* File Description:
*   Test Javascript popup menus
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/07/23 21:50:33  vakatov
* Initial revision
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>

#include <html/html.hpp>
#include <html/jsmenu.hpp>



USING_NCBI_SCOPE;


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
    m1->AddItem("White" , "document.bgColor='white'");
    m1->AddSeparator();
    m1->AddItem("Green", "document.bgColor='green'");

    m1->SetAttribute(eHTML_PM_fontColor, "black");
    m1->SetAttribute(eHTML_PM_fontColorHilite, "yellow");

    // Create another menu
    CHTMLPopupMenu* m2 = new CHTMLPopupMenu("Menu2");

    m2->AddItem("NCBI", "top.location='http://ncbi.nlm.nih.gov'");
    m2->AddItem("Netscape", "top.location='http://www.netscape.com'");
    m2->AddItem("Microsoft", "top.location='http://www.microsoft.com'");

    m2->SetAttribute(eHTML_PM_disableHide, "true");

    // Append one paragraph
    body->AppendChild(new CHTML_p("paragraph 1"));

    // Add menus to the page
    html->InitPopupMenus(*head, *body);
    html->AddPopupMenu(*m1);
    html->AddPopupMenu(*m2);

    // Append another paragraph
    body->AppendChild(new CHTML_p("paragraph 2"));

    // Add menus call
    CHTML_a* anchor1 = new CHTML_a("javascript:" + m1->ShowMenu(),
                                   "Color Menu");
    body->AppendChild(anchor1);

    body->AppendChild(new CHTML_p(""));

    CHTML_a* anchor2 = new CHTML_a("javascript:" + m2->ShowMenu(), "URL Menu");
    anchor2->SetEventHandler(eHTML_EH_MouseOver, m2->ShowMenu());
    body->AppendChild(anchor2);
    
    // Print in HTML format
    html->Print(cout);
    cout << endl << endl;

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
