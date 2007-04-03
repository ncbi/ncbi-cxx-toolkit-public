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
 * File Description:   Test Javascript popup menues
 * 
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <html/page.hpp>
#include <html/jsmenu.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////
// Create popup menu
//

enum EMenuType {
    eSmithPlain,
    eSmithHtml,
    eKurdin,
    eKurdinConf,
    eKurdinSide
};


static CHTMLPopupMenu* CreatePopupMenu(EMenuType type)
{
    CHTMLPopupMenu* menu = 0;
    static char index = '0';
    index++;

    switch (type) {
    case eSmithPlain:
        // Create Smith's popup menu
        menu = new CHTMLPopupMenu(string("MenuSmith_") + index,
                                  CHTMLPopupMenu::eSmith);
        menu->AddItem("Red"  , "document.bgColor='red'");
        menu->AddItem("Yellow","document.bgColor='yellow'");
        menu->AddItem("Green", "document.bgColor='green'");
        menu->AddSeparator();
        menu->AddItem("White", "document.bgColor='white'");
        menu->SetAttribute(eHTML_PM_fontColor, "black");
        menu->SetAttribute(eHTML_PM_fontColorHilite,"white");
        break;

    case eSmithHtml:
        {
        // Create Smith's popup menu with HTML items
        menu = new CHTMLPopupMenu(string("MenuSmithHtml_") + index);
        menu->AddItem("<b>Bold item</b>");
        menu->AddItem("<img src='http://www.ncbi.nlm.nih.gov/corehtml/help.gif'> Image item");
        CNodeRef anchor;
	anchor = new CHTML_a("some link...", "Link item");
        menu->AddItem(*anchor);
        }
        break;

    case eKurdin:
        menu = new CHTMLPopupMenu(string("MenuKurdinPopup_") + index,
                                  CHTMLPopupMenu::eKurdin);
        menu->AddItem("NIH", "http://www.nih.gov");
        menu->AddItem("NLM", "http://www.nlm.nih.gov");
        menu->AddItem("NCBI","top.location='http://www.ncbi.nlm.nih.gov'");
        menu->SetAttribute(eHTML_PM_disableHide, "true");
        menu->SetAttribute(eHTML_PM_titleColor, "yellow");
        menu->SetAttribute(eHTML_PM_alignV, "top");
        break;

    case eKurdinConf:
        menu = new CHTMLPopupMenu(string("MenuKurdinConfPopup_") + index,
                                  CHTMLPopupMenu::eKurdinConf);
        menu->AddItem("NIH", "http://www.nih.gov");
        menu->AddSeparator();
        menu->AddItem("NLM", "http://www.nlm.nih.gov");
        menu->AddSeparator("text");
        menu->AddItem("NCBI","top.location='http://www.ncbi.nlm.nih.gov'");
        menu->SetAttribute(eHTML_PM_TitleColor, "yellow");
        menu->SetAttribute(eHTML_PM_ItemColor, "#000000");
        menu->SetAttributeGlobal(eHTML_PM_AlignTB, "top");
        menu->SetAttributeGlobal(eHTML_PM_AlignLR, "right");
        break;

    case eKurdinSide:
        menu = new CHTMLPopupMenu("MenuKurdinSide_" + index,
                                  CHTMLPopupMenu::eKurdinSide);
        menu->AddItem("Web sites");
        menu->AddItem("NIH", "http://www.nih.gov");
        menu->AddItem("NLM", "http://www.nlm.nih.gov");
        menu->AddItem("NCBI","http://www.ncbi.nlm.nih.gov");
        menu->AddSeparator();
        menu->AddItem("Netscape", "http://www.netscape.com");
        menu->AddItem("Microsoft","http://www.microsoft.com");
        //menu->SetAttribute(eHTML_PM_disableHide, "false");
        menu->SetAttribute(eHTML_PM_menuWidth, "120");
        menu->SetAttribute(eHTML_PM_peepOffset,"20");
        menu->SetAttribute(eHTML_PM_topOffset, "10");
        break;

    default:
        _TROUBLE;
    }
    return menu;
}



/////////////////////////////////
// Standart HTML test
//

static void Test_Html(void)
{
    LOG_POST("HTML test\n");

    // Create HTML page skeleton with HEAD and BODY
    
    CNodeRef html(new CHTML_html);
    CNodeRef head (new CHTML_head);
    CRef<CHTML_body> body(new CHTML_body);
    
    html->AppendChild(head); 
    html->AppendChild(body); 

    // Create menues

    CRef<CHTMLPopupMenu> menuSP(CreatePopupMenu(eSmithPlain));
    CRef<CHTMLPopupMenu> menuSH(CreatePopupMenu(eSmithHtml));
    CRef<CHTMLPopupMenu> menuKP(CreatePopupMenu(eKurdin));
    CRef<CHTMLPopupMenu> menuKC(CreatePopupMenu(eKurdinConf));
    CRef<CHTMLPopupMenu> menuKS(CreatePopupMenu(eKurdinSide));

    // Add menues to the page
    // !!! We can add Smith's and Kurdin's menu to the BODY only!
    body->AppendChild(menuSP);
    body->AppendChild(menuSH);
    body->AppendChild(menuKP);
    body->AppendChild(menuKC);
    // !!! We can add Kurdin's Side menu to the HEAD only!
    head->AppendChild(menuKS);

    // Add menu calls
    CRef<CHTML_a> anchorSP(new CHTML_a("#","Smith's menu (click me)"));
    anchorSP->AttachPopupMenu(menuSP, eHTML_EH_Click);
    CRef<CHTML_a> anchorSH(new CHTML_a("#","Smith's menu with HTML (move mouse over me)"));
    anchorSH->AttachPopupMenu(menuSH, eHTML_EH_MouseOver);
    CRef<CHTML_a> anchorKP(new CHTML_a("#","Kurdin's popup menu (move mouse over me)"));
    anchorKP->AttachPopupMenu(menuKP);
    CRef<CHTML_a> anchorKPc(new CHTML_a("#","Kurdin's popup menu (click me)"));
    anchorKPc->AttachPopupMenu(menuKP, eHTML_EH_Click);
    CRef<CHTML_a> anchorKC(new CHTML_a("#","Kurdin's popup menu with configurations (move mouse over me)"));
    anchorKC->AttachPopupMenu(menuKC);
    CRef<CHTML_a> anchorKCc(new CHTML_a("#","Kurdin's popup menu with configurations (click me)"));
    anchorKCc->AttachPopupMenu(menuKC, eHTML_EH_Click);

    CRef<CHTML_blockquote> bc(new CHTML_blockquote);
    bc->AppendChild(anchorSP);
    bc->AppendChild(new CHTML_br(2));
    bc->AppendChild(anchorSH);
    bc->AppendChild(new CHTML_br(2));
    bc->AppendChild(anchorKP);
    bc->AppendChild(new CHTML_br(2));
    bc->AppendChild(anchorKPc);
    bc->AppendChild(new CHTML_br(2));
    bc->AppendChild(anchorKC);
    bc->AppendChild(new CHTML_br(2));
    bc->AppendChild(anchorKCc);

    body->AppendChild(bc);
    body->AttachPopupMenu(menuKS);


    // Enable using popup menus (we can skip call this function)
//  html->EnablePopupMenu(CHTMLPopupMenu::eSmith);
//  html->EnablePopupMenu(CHTMLPopupMenu::eKurdin);
//  html->EnablePopupMenu(CHTMLPopupMenu::eKurdinConf);
//  html->EnablePopupMenu(CHTMLPopupMenu::eKurdinSide);
    
    // Print in HTML format
    html->Print(cout);
    cout << endl;
}


/////////////////////////////////
// Template test
//

static void Test_Template(void)
{
    LOG_POST("\nTemplate test\n");

    CHTMLPage page("JSMenu test page","template_jsmenu.html"); 
    CNodeRef view_menues   (new CNCBINode());
    CNodeRef view_sidemenu (new CNCBINode());

    // Create menues
    CRef<CHTMLPopupMenu> menuSP(CreatePopupMenu(eSmithPlain));
    CRef<CHTMLPopupMenu> menuSH(CreatePopupMenu(eSmithHtml));
    CRef<CHTMLPopupMenu> menuKP(CreatePopupMenu(eKurdin));
    CRef<CHTMLPopupMenu> menuKS(CreatePopupMenu(eKurdinSide));

    // Add menues to the page
    // !!! We can add Smith's and Kurdin's menu to the BODY only!
    view_menues->AppendChild(menuSP);
    view_menues->AppendChild(menuSH);
    view_menues->AppendChild(menuKP);
    // !!! We can add Kurdin's side menu to the HEAD only!
    view_sidemenu->AppendChild(menuKS);

    // Add menu calls
    CRef<CHTML_a> anchorSP(new CHTML_a("#","Smith's menu (click me)"));
    anchorSP->AttachPopupMenu(menuSP, eHTML_EH_Click);
    CRef<CHTML_a> anchorSH(new CHTML_a("#","Smith's menu with HTML (move mouse over me)"));
    anchorSH->AttachPopupMenu(menuSH, eHTML_EH_MouseOver);
    CRef<CHTML_a> anchorKP(new CHTML_a("#","Kurdin's popup menu (move mouse over me)"));
    anchorKP->AttachPopupMenu(menuKP);
    CRef<CHTML_a> anchorKPC(new CHTML_a("#","Kurdin's popup menu (click me)"));
    anchorKPC->AttachPopupMenu(menuKP, eHTML_EH_Click);

    view_menues->AppendChild(anchorSP);
    view_menues->AppendChild(new CHTML_p(new CHTML_br(2)));
    view_menues->AppendChild(anchorSH);
    view_menues->AppendChild(new CHTML_p(new CHTML_br(2)));
    view_menues->AppendChild(anchorKP);
    view_menues->AppendChild(new CHTML_p(new CHTML_br(2)));
    view_menues->AppendChild(anchorKPC);

    view_sidemenu->AppendChild(menuKS);

    page.AddTagMap("VIEW_MENUES",   view_menues);
    page.AddTagMap("VIEW_SIDEMENU", view_sidemenu);

    // Enable using popup menues (we can skip call this function)
    page.EnablePopupMenu(CHTMLPopupMenu::eSmith);
    page.EnablePopupMenu(CHTMLPopupMenu::eKurdin);
    page.EnablePopupMenu(CHTMLPopupMenu::eKurdinSide);

    // Print test result
    page.Print(cout, CNCBINode::eHTML);
    cout << endl;
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
