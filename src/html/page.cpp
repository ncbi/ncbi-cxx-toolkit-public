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
*   Page Classes
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  1999/04/26 21:59:31  vakatov
* Cleaned and ported to build with MSVC++ 6.0 compiler
*
* Revision 1.11  1999/04/19 16:51:36  vasilche
* Fixed error with member pointers detected by GCC.
*
* Revision 1.10  1999/04/15 22:10:43  vakatov
* Fixed "class TagMapper<>" to "struct ..."
*
* Revision 1.9  1998/12/28 23:29:10  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.8  1998/12/28 21:48:17  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.7  1998/12/28 16:48:09  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.6  1998/12/22 16:39:15  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.3  1998/12/01 19:10:39  lewisg
* uses CCgiApplication and new page factory
* ===========================================================================
*/

#include <html/components.hpp>
#include <html/page.hpp>

BEGIN_NCBI_SCOPE
 

/////////////////////////////////////////////////////////////////////////////
// CHTMLBasicPage

CHTMLBasicPage::CHTMLBasicPage(void)
    : m_CgiApplication(0),
      m_Style(0)
{
}

CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application),
      m_Style(style)
{
}

CNCBINode* CHTMLBasicPage::CloneSelf(void) const
{
    return new CHTMLBasicPage(*this);
}

void CHTMLBasicPage::SetApplication(CCgiApplication* App)
{
    m_CgiApplication = App;
}

void CHTMLBasicPage::SetStyle(int style)
{
    m_Style = style;
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


/////////////////////////////////////////////////////////////////////////////
// CHTMLPage

template struct TagMapper<CHTMLPage>;

CHTMLPage::CHTMLPage(const string& title, const string& template_file)
    : m_Title(title),
      m_TemplateFile(template_file)
{
    Init();
}

CHTMLPage::CHTMLPage(CCgiApplication* application, int style,
                     const string& title, const string& template_file)
    : CParent(application, style),
      m_Title(title),
      m_TemplateFile(template_file)
{
    Init();
}

void CHTMLPage::Init(void)
{
    AddTagMap("TITLE", CreateTagMapper(this, &CHTMLPage::CreateTitle));
    AddTagMap("VIEW",  CreateTagMapper(this, &CHTMLPage::CreateView));
}

CNCBINode* CHTMLPage::CloneSelf(void) const
{
    return new CHTMLPage(*this);
}

void CHTMLPage::CreateSubNodes(void)
{
    AppendChild(CreateTemplate());
}

CNCBINode* CHTMLPage::CreateTemplate(void) 
{
    string        str;  
    char          buf[1024];
    CNcbiIfstream ifstr(m_TemplateFile.c_str());

    while ( ifstr.read(buf, sizeof(buf)) )
        str.append(buf, ifstr.gcount());
    
    return new CHTMLText(str);
}

CNCBINode* CHTMLPage::CreateTitle(void) 
{
    if ( GetStyle() & kNoTITLE )
        return 0;

    return new CHTMLText(m_Title);
}

CNCBINode* CHTMLPage::CreateView(void) 
{
    return 0;
}

END_NCBI_SCOPE
