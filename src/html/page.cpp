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
* Revision 1.1  1998/10/29 16:13:11  lewisg
* version 2
*
* ===========================================================================
*/

#include <fstream.h>


#include <components.hpp>
#include <page.hpp>



void CHTMLPage::Init(int style)
{
    if (!m_Template) m_Template = CreateTemplate();

    if(!(style & kNoTITLE))
	if (!m_Title) m_Title = CreateTitle();
    else m_Title = new CHTMLNode;

    if(!(style & kNoVIEW))
        if (!m_View) m_View = CreateView();
    else m_View = new CHTMLNode;
}


void CHTMLPage::Draw(int style)
{
    if(m_Template)
	AppendChild(m_Template);

    if(!(style & kNoTITLE) && m_Title && m_Template)
	m_Template->Rfind("<@TITLE@>", m_Title);

    if(!(style & kNoVIEW) && m_View && m_Template) 
        m_Template->Rfind("<@VIEW@>", m_View);
}


CHTMLNode * CHTMLPage::CreateTitle() 
{
    return new CHTMLText("My Page");
}


CHTMLNode * CHTMLPage::CreateTemplate() 
{
    string input;  
    char ch;

    // demo hack
    ifstream inFile("frontpage.html");
    while( inFile.get(ch)) input += ch;
    CHTMLText * text = new CHTMLText;
    text->Data(input); // some text
    return text;
}


CHTMLNode * CHTMLPage::CreateView() 
{
    CQueryBox * querybox = new CQueryBox();
    querybox->Create();
    return querybox;
}

