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
* Author: Eugene Vasilchenko
*
* File Description:
*   Image link bar.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/05/13 15:22:44  vakatov
* Added default constructor to CLinkDefinition:: -- to let the Mac
* CodeWarrior C++ compiler to instantiate "list<CLinkDefinition>"
*
* Revision 1.4  1999/02/22 22:45:43  vasilche
* Fixed map::insert(value_type) usage.
*
* Revision 1.3  1999/02/03 15:03:58  vasilche
* Added check for wrong link names.
*
* Revision 1.2  1999/02/02 17:57:50  vasilche
* Added CHTML_table::Row(int row).
* Linkbar now have equal image spacing.
*
* Revision 1.1  1999/01/29 20:48:04  vasilche
* Added class CLinkBar.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/linkbar.hpp>

BEGIN_NCBI_SCOPE

CLinkDefinition::CLinkDefinition(void)
    : m_Width(0), m_Height(0)
{
}

CLinkDefinition::CLinkDefinition(const string& name,
                                 const string& linkImage,
                                 const string& noLinkImage)
    : m_LinkName(name),
      m_LinkImage(linkImage), m_NoLinkImage(noLinkImage),
      m_Width(0), m_Height(0)
{
}

CLinkDefinition::CLinkDefinition(const string& name,
                                 int width, int height,
                                 const string& linkImage,
                                 const string& noLinkImage)
    : m_LinkName(name),
      m_LinkImage(linkImage), m_NoLinkImage(noLinkImage),
      m_Width(width), m_Height(height)
{
}

CLinkBar::CLinkBar(const TLinkBarTemplate* templ)
    : m_Template(templ)
{
/*
    SetCellSpacing(0);
    SetCellPadding(0);
*/
    Row(0)->SetVAlign("baseline");
}

CLinkBar::~CLinkBar(void)
{
}

void CLinkBar::AddLink(const string& name, const string& linkUrl)
{
    m_Links[name] = linkUrl;
    
    // check that this link is in template
    for ( TLinkBarTemplate::const_iterator i = m_Template->begin();
          i != m_Template->end(); ++i ) {
        if ( i->m_LinkName == name )
            return;
    }
    _TRACE("CLinkBar::AddLink: name " << name << " is not in template");
}

void CLinkBar::CreateSubNodes(void)
{
    int col = 0;
    for ( TLinkBarTemplate::const_iterator i = m_Template->begin();
          i != m_Template->end(); ++i ) {
        CHTMLNode* cell = Cell(0, col++);
        cell->AppendChild(CreateLink(*i));
        if ( i->m_Width > 0 )
            cell->SetWidth(i->m_Width);
        if ( i->m_Height > 0 )
            cell->SetHeight(i->m_Height);
    }
}

CHTMLNode* CLinkBar::CreateLink(const CLinkDefinition& def)
{
    CHTMLNode* node;
    CHTML_img* img;

    map<string, string>::iterator ptr = m_Links.find(def.m_LinkName);
    if ( ptr == m_Links.end() || ptr->second.empty() ) {
        node = img = new CHTML_img(def.m_NoLinkImage);
    }
    else {
        img = new CHTML_img(def.m_LinkImage);
        img->SetAttribute("BORDER", 0);
        node = new CHTML_a(ptr->second, img);
    }
/*
    if ( def.m_Width > 0 )
        img->SetWidth(def.m_Width);
    if ( def.m_Height > 0 )
        img->SetHeight(def.m_Height);
*/
    return node;
}

END_NCBI_SCOPE
