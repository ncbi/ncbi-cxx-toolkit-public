#if defined(PAGE__HPP)  &&  !defined(PAGE__INL)
#define PAGE__INL

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/04/26 21:59:29  vakatov
* Cleaned and ported to build with MSVC++ 6.0 compiler
*
* Revision 1.7  1999/04/19 20:11:48  vakatov
* CreateTagMapper() template definitions moved from "page.inl" to
* "page.hpp" because MSVC++ gets confused(cannot understand what
* is declaration and what is definition).
*
* Revision 1.6  1999/04/15 19:48:18  vasilche
* Fixed several warnings detected by GCC
*
* Revision 1.5  1998/12/28 21:48:14  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.4  1998/12/24 16:15:39  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.3  1998/12/23 21:21:00  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.2  1998/12/22 16:39:13  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.1  1998/12/21 22:24:59  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

inline CHTMLBasicPage* CHTMLPage::New(void)
{
    return new CHTMLPage;
}

inline void CHTMLPage::SetTitle(const string& title)
{
    m_Title = title;
}

inline void CHTMLPage::SetTemplateFile(const string& template_file)
{
    m_TemplateFile = template_file;
}

inline CCgiApplication* CHTMLBasicPage::GetApplication(void) const
{
    return m_CgiApplication;
}

inline int CHTMLBasicPage::GetStyle(void) const
{
    return m_Style;
}

inline void CHTMLBasicPage::AddTagMap(const string& name, CNCBINode* node)
{
    AddTagMap(name, new ReadyTagMapper(node));
}

#endif /* def PAGE__HPP  &&  ndef PAGE__INL */
