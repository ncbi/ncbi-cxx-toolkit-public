#ifndef PAGE__HPP
#define PAGE__HPP

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
*   The HTML page
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2002/02/13 20:17:06  ivanov
* Added support of dynamic popup menus. Changed EnablePopupMenu().
*
* Revision 1.21  2001/08/14 16:57:14  ivanov
* Added support for work HTML templates with JavaScript popup menu.
* Renamed type Flags -> ETypes. Moved all code from "page.inl" to header file.
*
* Revision 1.20  1999/10/28 13:40:31  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.19  1999/05/28 16:32:11  vasilche
* Fixed memory leak in page tag mappers.
*
* Revision 1.18  1999/04/27 14:49:59  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.17  1999/04/26 21:59:28  vakatov
* Cleaned and ported to build with MSVC++ 6.0 compiler
*
* Revision 1.16  1999/04/20 13:51:59  vasilche
* Removed unused parameter name to avoid warning.
*
* Revision 1.15  1999/04/19 20:11:47  vakatov
* CreateTagMapper() template definitions moved from "page.inl" to
* "page.hpp" because MSVC++ gets confused(cannot understand what
* is declaration and what is definition).
*
* Revision 1.14  1999/04/15 22:06:46  vakatov
* CQueryBox:: use "enum { kNo..., };" rather than "static const int kNo...;"
* Fixed "class BaseTagMapper" to "struct ..."
*
* Revision 1.13  1998/12/28 23:29:03  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.12  1998/12/28 21:48:13  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.11  1998/12/24 16:15:38  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.10  1998/12/23 21:21:00  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.9  1998/12/22 16:39:12  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.8  1998/12/21 22:24:59  vasilche
* A lot of cleaning.
*
* Revision 1.7  1998/12/11 22:53:41  lewisg
* added docsum page
*
* Revision 1.6  1998/12/11 18:13:51  lewisg
* frontpage added
*
* Revision 1.5  1998/12/09 23:02:56  lewisg
* update to new cgiapp class
*
* Revision 1.4  1998/12/09 17:27:44  sandomir
* tool should be changed to work with the new CCgiApplication
*
* Revision 1.2  1998/12/01 19:09:06  lewisg
* uses CCgiApplication and new page factory
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/html.hpp>
#include <html/nodemap.hpp>

BEGIN_NCBI_SCOPE

class CCgiApplication;

/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.

class CHTMLBasicPage: public CNCBINode
{
    typedef CNCBINode CParent; // parent class
    typedef map<string, BaseTagMapper*> TTagMap;

public: 
    CHTMLBasicPage(void);
    CHTMLBasicPage(CCgiApplication* app, int style = 0);
    virtual ~CHTMLBasicPage(void);

    virtual CCgiApplication* GetApplication(void) const;
    virtual void SetApplication(CCgiApplication* App);

    int GetStyle(void) const;
    void SetStyle(int style);

    // resolve <@XXX@> tag
    virtual CNCBINode* MapTag(const string& name);

    // add tag resolver
    virtual void AddTagMap(const string& name, BaseTagMapper* mapper);
    virtual void AddTagMap(const string& name, CNCBINode*     node);

protected:
    CCgiApplication* m_CgiApplication;  // pointer to runtime information
    int m_Style;

    // tag resolvers (as registered by AddTagMap)
    TTagMap m_TagMap;
};


////////////////////////////////////
//  this is the basic 3 section NCBI page


class CHTMLPage : public CHTMLBasicPage
{
    // parent class
    typedef CHTMLBasicPage CParent;

public:
    // style flags
    enum EFlags {
        fNoTITLE      = 0x1,
        fNoVIEW       = 0x2,
        fNoTEMPLATE   = 0x4
    };
    typedef int TFlags;  // binary AND of "EFlags"

    // 'tors
    CHTMLPage(const string& title         = NcbiEmptyString,
              const string& template_file = NcbiEmptyString);
    CHTMLPage(CCgiApplication* app,
              TFlags           style         = 0,
              const string&    title         = NcbiEmptyString,
              const string&    template_file = NcbiEmptyString);
    static CHTMLBasicPage* New(void);

    // create the individual sub pages
    virtual void CreateSubNodes(void);

    // create the static part of the page(here - read it from <m_TemplateFile>)
    virtual CNCBINode* CreateTemplate(void);

    // tag substitution callbacks
    virtual CNCBINode* CreateTitle(void);  // def for tag "@TITLE@" - <m_Title>
    virtual CNCBINode* CreateView(void);   // def for tag "@VIEW@"  - none

    // to set title or template file outside(after) the constructor
    void SetTitle       (const string& title);
    void SetTemplateFile(const string& template_file);

    // Enable using popup menus, set URL for popup menu library.
    // If "menu_lib_url" is not defined, then using default URL.
    // use_dynamic_menu - enable/disable using dynamic popup menus 
    // (default it is disabled).
    // NOTE: 1) If we not change value "menu_script_url", namely use default
    //          value for it, then we can skip call this function.
    //       2) Dynamic menu work only in new browsers. They use one container
    //          for all menus instead of separately container for each menu in 
    //          nondynamic mode.
    // In most cases (except if popup menus are defined only in the page
    // template or printed by non-CNCBINode tag mapper), you can omit this
    // function call.
    void EnablePopupMenu(const string& menu_script_url = kEmptyStr,
                         bool use_dynamic_menu = false);

    virtual void AddTagMap(const string& name, BaseTagMapper* mapper);
    virtual void AddTagMap(const string& name, CNCBINode* node);

private:
    void Init(void);

    string m_Title;
    string m_TemplateFile;

    // Popup menu variables
    string m_PopupMenuLibUrl;
    bool   m_UsePopupMenu;
    bool   m_UsePopupMenuDynamic;
};



/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////
//  CHTMLBasicPage::
//


inline CCgiApplication* CHTMLBasicPage::GetApplication(void) const
{
    return m_CgiApplication;
}

inline int CHTMLBasicPage::GetStyle(void) const
{
    return m_Style;
}



///////////////////////////////////////////////////////
//  CHTMLPage::
//


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


END_NCBI_SCOPE

#endif  /* PAGE__HPP */
