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



CQueryBox::CQueryBox()
{
    m_Comments = NULL;
}


CQueryBox::~CQueryBox() { delete m_Comments; }


CHTMLNode * CQueryBox::CreateComments(void)
{
    return new CHTMLNode; // new CHTMLNode;
}


void CQueryBox::InitMembers(int style)
{  
    m_Width = "400";
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
     
	form->AppendText("Search ");

	CHTML_select * select = new CHTML_select(m_DbName);
	form->AppendChild(select);
	for(iMap = m_Databases.begin(); iMap != m_Databases.end(); iMap++)
	    select->AppendOption((*iMap).second, iMap == m_Databases.begin(), (*iMap).first);
	 

	form->AppendText(" for<p>");

   

	CHTML_table * table = new CHTML_table(m_BgColor, m_Width, "0", "5", 1, 1);
	form->AppendChild(table);
    
	table->InsertInTable(1, 1, new CHTML_input("text", m_TermName, "", "30")); // todo: the width calculation

	table->InsertInTable(1, 1, new CHTML_input("submit", "Search"));
    
	table->InsertInTable(1, 1, new CHTML_br);

	CHTML_select * selectpage = new CHTML_select(m_DispMax);
	table->InsertInTable(1, 1, selectpage);
	for(iList = m_Disp.begin(); iList != m_Disp.end(); iList++)
	    selectpage->AppendOption(*iList, *iList == m_DefaultDispMax);

	table->InsertTextInTable(1, 1, "documents per page");

	AppendChild(form);

	AppendChild(m_Comments);
    }
    catch (...) {
        delete form;
        throw;
    }
}

END_NCBI_SCOPE
