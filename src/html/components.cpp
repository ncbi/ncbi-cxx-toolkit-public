/*  $RCSfile$  $Revision$  $Date$
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


#include <ncbistd.hpp>
#include <ncbidbg.hpp>
#include <html.hpp>
#include <components.hpp>
#include <nodemap.hpp>
BEGIN_NCBI_SCOPE



CQueryBox::CQueryBox(const string& URL)
    : CParent(URL)
{
}

/*
CQueryBox::CQueryBox(const CQueryBox* origin)
    : CParent(origin), m_Width(origin->m_Width), m_BgColor(origin->m_BgColor),
      m_DbName(origin->m_DbName), m_Databases(origin->m_Databases),
      m_HiddenValues(origin->m_HiddenValues), m_TermName(origin->m_TermName),
      m_DispMax(origin->m_DispMax), m_DefaultDispMax(origin->m_DefaultDispMax),
      m_Disp(origin->m_Disp)
{
}
*/

CNCBINode* CQueryBox::CloneSelf(void) const
{
    return new CQueryBox(*this);
}

void CQueryBox::CreateSubNodes()
{ 
    list < string >::iterator iList;
    map < string, string, less <string> >::iterator iMap;

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
            select->AppendOption(i->second, i->first, i == m_Databases.begin());
        }
        AppendHTMLText(" for<p>");
    }
   
    CHTML_table* table = new CHTML_table(m_BgColor, IntToString(m_Width), "0", "5", 1, 1);
    AppendChild(table);

    table->InsertInTable(0, 0, new CHTML_text(m_TermName, (int)(m_Width*0.075))); // todo: the width calculation
    table->InsertInTable(0, 0, new CHTMLText("&nbsp;"));
    table->InsertInTable(0, 0, new CHTML_submit("Search"));
    table->InsertInTable(0, 0, new CHTML_br);
        
    CHTML_select * selectpage = new CHTML_select(m_DispMax);
    table->InsertInTable(0, 0, selectpage); 
    for ( iList = m_Disp.begin(); iList != m_Disp.end(); iList++ )
        selectpage->AppendOption(*iList, *iList == m_DefaultDispMax);
        
    table->InsertTextInTable(0, 0, "documents per page");
}

CNCBINode* CQueryBox::CreateComments(void)
{
    return 0;
}


// pager

CButtonList::CButtonList(void)
{
}

/*
CButtonList::CButtonList(const CButtonList* origin)
    : CParent(origin), m_Select(origin->m_Select), m_List(origin->m_List)
{
}
*/

CNCBINode* CButtonList::CloneSelf(void) const
{
    return new CButtonList(*this);
}

void CButtonList::CreateSubNodes()
{
    CHTML_select * Select = new CHTML_select(m_Select);
    map < string, string >::iterator iMap;

    try {
        AppendChild(new CHTML_input("submit", m_Name));
        AppendChild(Select);
        for ( iMap = m_List.begin(); iMap != m_List.end(); iMap++ )
            Select->AppendOption(iMap->second, iMap->first, iMap == m_List.begin());
    }
    catch (...) {
        delete Select;
    }
}


CPageList::CPageList(void)
{
}

CNCBINode* CPageList::CloneSelf(void) const
{
    return new CPageList(*this);
}

void CPageList::CreateSubNodes()
{
    map < int, string >::iterator iMap;

    AppendChild(new CHTML_a(m_Backward, "&lt;&lt;"));
    for ( iMap = m_Pages.begin(); iMap != m_Pages.end(); iMap++ )
        AppendChild(new CHTML_a((*iMap).second, IntToString((*iMap).first)));
    AppendChild(new CHTML_a(m_Forward, "&gt;&gt;"));
}

// Pager box

CPagerBox::CPagerBox()
    : m_NumResults(0), m_Width(460)
    , m_TopButton(0), m_LeftButton(0), m_RightButton(0)
    , m_PageList(0)
{
    CHTML_table* table;
    CHTML_table* tableTop;
    CHTML_table* tableBot;

    table = new CHTML_table("#CCCCCC", IntToString(m_Width), "0", "0", 2, 1);
    table->SetAttribute("border", "0");
    AppendChild(table);

    tableTop = new CHTML_table("#CCCCCC", IntToString(m_Width), "0", "0", 1, 2);
    tableBot = new CHTML_table("#CCCCCC", IntToString(m_Width), "0", "0", 1, 3);

    table->InsertInTable(0, 0, tableTop);
    table->InsertInTable(1, 0, tableBot);
    tableTop->InsertInTable(0, 0, new CButtonList);
    tableTop->InsertInTable(0, 1, new CPageList);
    tableBot->InsertInTable(0, 0, new CButtonList);
    tableBot->InsertInTable(0, 1, new CButtonList);
    tableBot->InsertInTable(0, 2, new CHTMLText(IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
}

CNCBINode* CPagerBox::CloneSelf(void) const
{
    return new CPagerBox(*this);
}

void CPagerBox::CreateSubNodes(void)
{
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
        Table = new CHTML_table("#CCCCCC", IntToString(m_Width), "0", "0", 1, 2);
        Table->SetAttribute("border", "0");
        AppendChild(Table);
        
        Table->InsertInTable(0, 0, new CPageList);
        Table->InsertInTable(0, 1, new CHTMLText(IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
    }
    catch (...) {
        delete Table;
    }
}


END_NCBI_SCOPE
