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
* Revision 1.5  1998/12/21 22:25:04  vasilche
* A lot of cleaning.
*
* Revision 1.4  1998/12/08 00:33:44  lewisg
* cleanup
*
* Revision 1.3  1998/12/01 19:10:39  lewisg
* uses CCgiApplication and new page factory
*
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
#include <nodemap.hpp>
BEGIN_NCBI_SCOPE
 
// CHTMLBasicPage

CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application), m_Style(style)
{
}

/*
CHTMLBasicPage::CHTMLBasicPage(const CHTMLBasicPage* origin)
    : CParent(origin)
{
    m_CgiApplication = origin->m_CgiApplication;
    m_Style = origin->m_Style;
    m_TagMap = origin->m_TagMap;
}
*/

CNCBINode* CHTMLBasicPage::CloneSelf() const
{
    return new CHTMLBasicPage(*this);
}

void CHTMLBasicPage::SetApplication(CCgiApplication * App)
{
    m_CgiApplication = App;
}

void CHTMLBasicPage::SetStyle(int style)
{
    m_Style = style;
}

BaseTagMapper::~BaseTagMapper(void)
{
}

CNCBINode* CHTMLBasicPage::MapTag(const string& name)
{
    map<string, BaseTagMapper*>::iterator i = m_TagMap.find(name);
    if ( i != m_TagMap.end() ) {
        return (i->second)->MapTag(this, name);
    }

    return CParent::MapTag(name);
}

void CHTMLBasicPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    m_TagMap[name] = mapper;
}

void CHTMLBasicPage::AddTagMap(const string& name, CNCBINode*(CHTMLBasicPage::*method)(void))
{
    AddTagMap(name, new TagMapper<CHTMLBasicPage>(method));
}

void CHTMLBasicPage::AddTagMap(const string& name, CNCBINode*(CHTMLBasicPage::*method)(const string& name))
{
    AddTagMap(name, new TagMapper<CHTMLBasicPage>(method));
}

// CHTMLPage

CHTMLPage::CHTMLPage(CCgiApplication* application, int style)
    : CParent(application, style),  m_PageName("PubMed"), m_TemplateFile("frontpage.html")
{
    AddTagMap("TEMPLATE", new TagMapper<CHTMLPage>(&CreateTemplate));
    AddTagMap("TITLE", new TagMapper<CHTMLPage>(&CreateTitle));
    AddTagMap("VIEW", new TagMapper<CHTMLPage>(&CreateView));
}

/*
CHTMLPage::CHTMLPage(const CHTMLPage* origin)
    : CParent(origin)
{
    m_PageName = origin->m_PageName;
    m_TemplateFile = origin->m_TemplateFile;
}
*/

CNCBINode* CHTMLPage::CloneSelf() const
{
    return new CHTMLPage(*this);
}

void CHTMLPage::CreateSubNodes(void)
{
    AppendChild(new CHTMLTagNode("TEMPLATE"));
}

CNCBINode* CHTMLPage::CreateTemplate(void) 
{
    string input;  
    char ch;

    CNcbiIfstream inFile(m_TemplateFile.c_str());

    while( inFile.get(ch))
        input += ch;
    
    return new CHTMLText(input);
}

CNCBINode* CHTMLPage::CreateTitle(void) 
{
    if ( GetStyle() & kNoTITLE )
        return 0;

    return new CHTMLText(m_PageName);
}

CNCBINode* CHTMLPage::CreateView(void) 
{
    if ( GetStyle() & kNoVIEW)
        return 0;

    return new CQueryBox();
}

END_NCBI_SCOPE
