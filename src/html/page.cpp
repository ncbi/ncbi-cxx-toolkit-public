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
*   Page Classes
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1998/11/23 23:42:37  lewisg
* *** empty log message ***
*
* Revision 1.1  1998/10/29 16:13:11  lewisg
* version 2
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <components.hpp>
#include <page.hpp>
BEGIN_NCBI_SCOPE

CHTMLBasicPage * CHTMLPageFactory::Create( multimap < string, string > & Cgi)
{
    list <CHTMLBasicPage *>::iterator iPage;
    multimap < string, string >:: iterator iCgi, iRange;
    map < string, string >:: iterator iPageCgi;
    pair <multimap < string, string >:: iterator, multimap < string, string >:: iterator > Range;

    for ( iPage = m_Pages.begin(); iPage != m_Pages.end(); iPage++) {
	bool ThisPage = true;
	for (iPageCgi = ((*iPage)->m_Cgi).begin(); iPageCgi != ((*iPage)->m_Cgi).end(); iPageCgi++) { 
	    Range = Cgi.equal_range(iPageCgi->first);
	    for(iRange = Range.first; iRange != Range.second; iRange++) {
		if( iRange->second == ((*iPage)->m_Cgi)[iRange->first]) goto equality;
		if(((*iPage)->m_Cgi)[iRange->first] == "") goto equality;
	    }
	    ThisPage = false;
	equality:  // ugh
	    ;
	}
	if(ThisPage) break;
    }
    if( iPage != m_Pages.end()) return (*iPage)->New();
    else return m_DefaultPage->New();
}


	    
CHTMLPageFactory::~CHTMLPageFactory()
{
    list <CHTMLBasicPage *>::iterator iPage;
    for ( iPage = m_Pages.begin(); iPage != m_Pages.end(); iPage++) {
	delete *iPage;
    }
    delete m_DefaultPage;
}	    
	


void CHTMLPage::InitMembers(int style)
{
    ////////// section for initializing members

    m_PageName = "PubMed";
    m_TemplateFile = "frontpage.html";
}


void CHTMLPage::InitSubPages(int style)
{
  ////////// section for initializing sub windows
    try {
	if (!m_Template) m_Template = CreateTemplate();

	if(!(style & kNoTITLE)) {
	    if (!m_Title) m_Title = CreateTitle();
	    if (!m_Title) m_Title = new CHTMLNode;
	}

	if(!(style & kNoVIEW)) {
	    if (!m_View) m_View = CreateView();
	    if (!m_View) m_View = new CHTMLNode;
	}
    }
    catch (...) {
	delete m_Template;
	delete m_Title;
	delete m_View;
	throw;
    }
}


void CHTMLPage::Draw(int style)
{
    if(m_Template)
	AppendChild(m_Template);

    if(!(style & kNoTITLE) && m_Title)
	Rfind("<@TITLE@>", m_Title);

    if(!(style & kNoVIEW) && m_View) 
        Rfind("<@VIEW@>", m_View);
}


CHTMLNode * CHTMLPage::CreateTitle(void) 
{
    return new CHTMLText(m_PageName);
}


CHTMLNode * CHTMLPage::CreateTemplate(void) 
{
    string input;  
    char ch;
    CHTMLText * text;

    try {
	CNcbiIfstream inFile(m_TemplateFile.c_str());
	while( inFile.get(ch)) input += ch;
	text = new CHTMLText;
	text->Data(input); // some text
	return text;
    }
    catch(...) {
        delete text;
        throw;
    }
}


CHTMLNode * CHTMLPage::CreateView(void) 
{
    CQueryBox * querybox;
    try {
	querybox = new CQueryBox();
	querybox->Create();
	return querybox;
    }
    catch(...) {
	delete querybox;
	throw;
    }
}

END_NCBI_SCOPE
