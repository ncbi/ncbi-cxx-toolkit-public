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
*   The components that go on the pages.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  1999/01/14 21:25:19  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.16  1999/01/07 17:06:35  vasilche
* Added default query text in CQueryBox.
* Added query text width in CQueryBox.
*
* Revision 1.15  1999/01/07 16:41:56  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* Revision 1.14  1999/01/07 14:53:57  vasilche
* CButtonList displays nothing if list is empty.
*
* Revision 1.13  1999/01/06 21:35:37  vasilche
* Avoid use of Clone.
* Fixed default CHTML_text width.
*
* Revision 1.12  1999/01/05 21:47:14  vasilche
* Added 'current page' to CPageList.
* CPageList doesn't display forward/backward if empty.
*
* Revision 1.11  1999/01/04 20:06:13  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.10  1998/12/28 23:29:09  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 16:48:08  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.8  1998/12/23 21:21:03  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.7  1998/12/21 22:25:02  vasilche
* A lot of cleaning.
*
* Revision 1.6  1998/12/11 22:52:01  lewisg
* added docsum page
*
* Revision 1.5  1998/12/11 18:12:45  lewisg
* frontpage added
*
* Revision 1.4  1998/12/09 23:00:53  lewisg
* use new cgiapp class
*
* Revision 1.3  1998/12/08 00:33:42  lewisg
* cleanup
*
* Revision 1.2  1998/11/23 23:44:28  lewisg
* toolpages.cpp
*
* Revision 1.1  1998/10/29 16:13:07  lewisg
* version 2
*
* ===========================================================================
*/

#include <html/components.hpp>
#include <html/nodemap.hpp>

BEGIN_NCBI_SCOPE

CQueryBox::CQueryBox(const string& URL)
    : CParent(URL), m_TermWidth(0)
{
}

CNCBINode* CQueryBox::CloneSelf(void) const
{
    return new CQueryBox(*this);
}

void CQueryBox::CreateSubNodes()
{
    for ( map<string, string>::iterator i = m_HiddenValues.begin();
          i != m_HiddenValues.end(); ++i ) {
        AddHidden(i->first, i->second);
    }

    if ( !m_Databases.empty() ) {
        AppendPlainText("Search ");

        CHTML_select* select = new CHTML_select(m_DbName);
        AppendChild(select);
        for ( map<string, string>::iterator i = m_Databases.begin();
              i != m_Databases.end(); ++i ) {
            select->AppendOption(i->second, i->first, i->first == m_DefaultDatabase);
        }
        AppendHTMLText(" for<p>");
    }
   
    CHTML_table* table = new CHTML_table();
    table->SetCellSpacing(0)->SetCellPadding(5)->SetBgColor(m_BgColor)->SetWidth(m_Width);
    AppendChild(table);

    CHTML_text* term;
    if ( m_TermWidth )
        term = new CHTML_text(m_TermName, m_TermWidth, m_TermValue);
    else
        term = new CHTML_text(m_TermName, m_TermValue);
    table->InsertAt(0, 0, term);

    table->InsertAt(0, 0, new CHTMLText("&nbsp;"));
    table->InsertAt(0, 0, new CHTML_submit("Search"));
    table->InsertAt(0, 0, new CHTML_br);
        
    CHTML_select * selectpage = new CHTML_select(m_DispMax);
    table->InsertAt(0, 0, selectpage); 
    for ( list<string>::iterator i = m_Disp.begin(); i != m_Disp.end(); ++i )
        selectpage->AppendOption(*i, *i == m_DefaultDispMax);
        
    table->InsertTextAt(0, 0, "documents per page");
}

CNCBINode* CQueryBox::CreateComments(void)
{
    return 0;
}


// pager

CButtonList::CButtonList(void)
{
}

CNCBINode* CButtonList::CloneSelf(void) const
{
    return new CButtonList(*this);
}

void CButtonList::CreateSubNodes()
{
    if ( m_List.empty() )
        return; // nothing to display

    AppendChild(new CHTML_submit(m_Name));

    CHTML_select * Select = new CHTML_select(m_Select);
    AppendChild(Select);
    for (map<string, string>::iterator i = m_List.begin();
         i != m_List.end(); ++i )
        Select->AppendOption(i->second, i->first, i == m_List.begin());
}


CPageList::CPageList(void)
    : m_Current(-1)
{
    SetCellSpacing(2);
}

CNCBINode* CPageList::CloneSelf(void) const
{
    return new CPageList(*this);
}

void CPageList::x_AddImageString(CNCBINode* node, const string& name, int number,
                            const string& imageStart, const string& imageEnd)
{
    string s = IntToString(number);

    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_image(name, imageStart + s[i] + imageEnd, 0));
    }
}

void CPageList::x_AddInactiveImageString(CNCBINode* node, const string& name, int number,
                            const string& imageStart, const string& imageEnd)
{
    string s = IntToString(number);

    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_img(imageStart + s[i] + imageEnd));
    }
}

void CPageList::CreateSubNodes()
{
    int column = 0;
    if ( !m_Backward.empty() )
        InsertAt(0, column++, new CHTML_image(m_Backward, "/images/prev.gif", 0));

    for (map<int, string>::iterator i = m_Pages.begin();
         i != m_Pages.end(); ++i ) {
        if ( i->first == m_Current ) {
            // current link
            x_AddInactiveImageString(Cell(0, column++), i->second, i->first, "/images/black_", ".gif");
        }
        else {
            // normal link
            x_AddImageString(Cell(0, column++), i->second, i->first, "/images/", ".gif");
        }
    }

    if ( !m_Forward.empty() )
        InsertAt(0, column++, new CHTML_image(m_Forward, "/images/next.gif", 0));
}

// Pager box

CPagerBox::CPagerBox(void)
    : m_NumResults(0), m_Width(460)
    , m_TopButton(new CButtonList), m_LeftButton(new CButtonList), m_RightButton(new CButtonList)
    , m_PageList(new CPageList), m_BgColor("#c0c0c0")
{
}

void CPagerBox::CreateSubNodes(void)
{
    CHTML_table* table;
    CHTML_table* tableTop;
    CHTML_table* tableBot;

    table = new CHTML_table();
    table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)->SetWidth(m_Width)->SetAttribute("border", "0");
    AppendChild(table);

    tableTop = new CHTML_table();
    tableTop->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);
    tableBot = new CHTML_table();
    tableBot->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);

    table->InsertAt(0, 0, tableTop);
    table->InsertAt(1, 0, tableBot);
    tableTop->InsertAt(0, 0, m_TopButton);
    tableTop->InsertAt(0, 1, m_PageList);
    tableBot->InsertAt(0, 0, m_LeftButton);
    tableBot->InsertAt(0, 1, m_RightButton);
    tableBot->InsertAt(0, 2, new CHTMLText(IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
}

CNCBINode* CPagerBox::CloneSelf(void) const
{
    return new CPagerBox(*this);
}

CSmallPagerBox::CSmallPagerBox()
    : m_NumResults(0), m_Width(460), m_PageList(0)
{
}

CNCBINode* CSmallPagerBox::CloneSelf(void) const
{
    return new CSmallPagerBox(*this);
}

void CSmallPagerBox::CreateSubNodes()
{
    CHTML_table * Table;

    try {
        Table = new CHTML_table();
        Table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)->SetWidth(m_Width)->SetAttribute("border", 0);
        AppendChild(Table);
        
        Table->InsertAt(0, 0, new CPageList);
        Table->InsertAt(0, 1, new CHTMLText(IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
    }
    catch (...) {
        delete Table;
    }
}


END_NCBI_SCOPE
