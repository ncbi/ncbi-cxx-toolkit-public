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
#include <html.hpp>
#include <components.hpp>
BEGIN_NCBI_SCOPE



CQueryBox::CQueryBox(): m_Comments(0) {}


CHTMLNode * CQueryBox::CreateComments(void)
{
    return new CHTMLNode; // new CHTMLNode;
}


void CQueryBox::InitMembers(int style)
{  
    m_Width = 400;
    m_BgColor = "#CCCCCFF";
    m_DispMax = "dispmax";
    m_DefaultDispMax = "20";
    m_Disp.push_back("10");
    m_Disp.push_back("20");
    m_Disp.push_back("50");
    m_Disp.push_back("100");
    m_Disp.push_back("200");
    m_Disp.push_back("1000");
    m_Disp.push_back("2000");
    m_Disp.push_back("5000");
    m_DbName = "db";
    m_Databases["m"] = "Medline";
    m_Databases["n"] = "GenBank DNA Sequences";
    m_Databases["p"] = "GenBank Protein Sequences";
    m_Databases["t"] = "Biomolecule 3D Structures";
    m_Databases["c"] = "Complete Genomes";
    m_HiddenValues["form"] = "4";
    m_TermName = "term";
    m_URL = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query";
}


void CQueryBox::InitSubPages(int style)
{  
    if(!(style & kNoCOMMENTS))
	if (!m_Comments) m_Comments = CreateComments();
}


void CQueryBox::Finish(int style)
{ 

    list < string >::iterator iList;
    map < string, string, less <string> >::iterator iMap;
    CHTML_form * form;

    try {
	form = new CHTML_form(m_URL, "GET");

	for(iMap = m_HiddenValues.begin(); iMap != m_HiddenValues.end(); iMap++)
	    form->AppendChild(new CHTML_input("hidden", (*iMap).first, (*iMap).second));
	if(!(kNoLIST & style)) {
	    form->AppendText("Search ");

	    CHTML_select * select = new CHTML_select(m_DbName);
	    form->AppendChild(select);
	    for(iMap = m_Databases.begin(); iMap != m_Databases.end(); iMap++)
		select->AppendOption((*iMap).second, iMap == m_Databases.begin(), (*iMap).first);
	 
	    form->AppendText(" for<p>");
	}
   

	CHTML_table * table = new CHTML_table(m_BgColor, CHTMLHelper::IntToString(m_Width), "0", "5", 1, 1);
	form->AppendChild(table);
    
	table->InsertInTable(0, 0, new CHTML_input("text", m_TermName, "",CHTMLHelper::IntToString((int)(m_Width*0.075)))); // todo: the width calculation
	table->InsertInTable(0, 0, new CHTMLText("&nbsp;"));
	table->InsertInTable(0, 0, new CHTML_input("submit", "Search"));
    
	table->InsertInTable(0, 0, new CHTML_br);

	CHTML_select * selectpage = new CHTML_select(m_DispMax);
	table->InsertInTable(0, 0, selectpage);
	for(iList = m_Disp.begin(); iList != m_Disp.end(); iList++)
	    selectpage->AppendOption(*iList, *iList == m_DefaultDispMax);

	table->InsertTextInTable(0, 0, "documents per page");

	AppendChild(form);

	if(!(style & kNoCOMMENTS)) AppendChild(m_Comments);
    }
    catch (...) {
        delete form;
        throw;
    }
}


// pager

void CButtonList::Finish(int Style)
{
    CHTML_select * Select = new CHTML_select(m_Select);
    map < string, string >::iterator iMap;

    try {
	AppendChild(new CHTML_input("submit", m_Name));
	AppendChild(Select);
	for(iMap = m_List.begin(); iMap != m_List.end(); iMap++)
	    Select->AppendOption((*iMap).second, iMap == m_List.begin(), (*iMap).first);
    }
    catch (...) {
	delete Select;
    }
}


void CPageList::Finish(int Style)
{
    map < int, string >::iterator iMap;

    AppendChild(new CHTML_a(m_Backward, "&lt;&lt;"));
    for(iMap = m_Pages.begin(); iMap != m_Pages.end(); iMap++)
	AppendChild(new CHTML_a((*iMap).second, CHTMLHelper::IntToString((*iMap).first)));
    AppendChild(new CHTML_a(m_Forward, "&gt;&gt;"));
}


CPagerBox::CPagerBox(): m_NumResults(0), m_Width(460), m_TopButton(0), m_LeftButton(0), m_RightButton(0),  m_PageList(0)  {}


void CPagerBox::InitMembers(int)
{
    try {
	if(!m_TopButton)  m_TopButton = new CButtonList;
	if(!m_LeftButton) m_LeftButton = new CButtonList;
	if(!m_RightButton) m_RightButton = new CButtonList;
	if(!m_PageList) m_PageList = new CPageList;
    }
    catch(...) {
	delete m_TopButton;
	delete m_LeftButton;
	delete m_RightButton;
	delete m_PageList;
    }
}


void CPagerBox::Finish(int)
{
    CHTML_table * Table;
    CHTML_table * TableTop;
    CHTML_table * TableBot;
    try {
	Table = new CHTML_table("#CCCCCC", CHTMLHelper::IntToString(m_Width), "0", "0", 2, 1);
	Table->SetAttributes("border", "0");
	AppendChild(Table);
	TableTop = new CHTML_table("#CCCCCC", CHTMLHelper::IntToString(m_Width), "0", "0", 1, 2);
	TableBot = new CHTML_table("#CCCCCC", CHTMLHelper::IntToString(m_Width), "0", "0", 1, 3);
	if(m_TopButton)  m_TopButton->Create();
	if(m_LeftButton) m_LeftButton->Create();
	if(m_RightButton) m_RightButton->Create();
	if(m_PageList) m_PageList->Create();
	Table->InsertInTable(0, 0, TableTop);
	Table->InsertInTable(1, 0, TableBot);
	TableTop->InsertInTable(0, 0, m_TopButton);
	TableTop->InsertInTable(0, 1, m_PageList);
	TableBot->InsertInTable(0, 0, m_LeftButton);
	TableBot->InsertInTable(0, 1, m_RightButton);
	TableBot->InsertInTable(0, 2, new CHTMLText(CHTMLHelper::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
    }
    catch (...) {
	delete Table;
	delete TableTop;
	delete TableBot;
	delete m_TopButton;
	delete m_LeftButton;
	delete m_RightButton;
	delete m_PageList;
    }
}

CSmallPagerBox::CSmallPagerBox(): m_NumResults(0), m_Width(460), m_PageList(0)  {}


void CSmallPagerBox::InitMembers(int)
{
    try {
	if(!m_PageList) m_PageList = new CPageList;
    }
    catch(...) {
	delete m_PageList;
    }
}


void CSmallPagerBox::Finish(int)
{
    CHTML_table * Table;

    try {
	Table = new CHTML_table("#CCCCCC", CHTMLHelper::IntToString(m_Width), "0", "0", 1, 2);
	Table->SetAttributes("border", "0");
	AppendChild(Table);

	if(m_PageList) m_PageList->Create();

	Table->InsertInTable(0, 0, m_PageList);
	Table->InsertInTable(0, 1, new CHTMLText(CHTMLHelper::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
    }
    catch (...) {
	delete Table;
	delete m_PageList;
    }
}


END_NCBI_SCOPE
