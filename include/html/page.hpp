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

#include <corelib/cgiapp.hpp>
#include <html/html.hpp>
#include <html/nodemap.hpp>

BEGIN_NCBI_SCOPE

struct BaseTagMapper;

inline BaseTagMapper* CreateTagMapper(CNCBINode* node) {
    return new ReadyTagMapper(node);
}

inline BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(void)) {
    return new StaticTagMapper(function);
}

inline BaseTagMapper* CreateTagMapper(CNCBINode* (*function)
                                      (const string& name)) {
    return new StaticTagMapperByName(function);
}

template<class C>
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node)) {
    return new StaticTagMapperByNode<C>(function);
}

template<class C>
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)
                               (C* node, const string& name)) {
    return new StaticTagMapperByNodeAndName<C>(function);
}

template<class C>
inline BaseTagMapper* CreateTagMapper(const C*,
                                      CNCBINode* (C::*method)(void)) {
    return new TagMapper<C>(method);
}

template<class C>
inline BaseTagMapper* CreateTagMapper(const C*, CNCBINode* (C::*method)
                                      (const string& name))
{
    return new TagMapperByName<C>(method);
}


/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.

class CHTMLBasicPage: public CNCBINode
{
    typedef CNCBINode CParent; // parent class

public: 
    CHTMLBasicPage(void);
    CHTMLBasicPage(CCgiApplication* app, int style = 0);

    virtual CCgiApplication* GetApplication(void) const;
    virtual void SetApplication(CCgiApplication* App);

    int GetStyle(void) const;
    void SetStyle(int style);

    // resolve <@XXX@> tag
    virtual CNCBINode* MapTag(const string& name);

    // add tag resolver
    void AddTagMap(const string& name, BaseTagMapper* mapper);
    void AddTagMap(const string& name, CNCBINode* node);

protected:
    CCgiApplication* m_CgiApplication;  // pointer to runtime information
    int m_Style;

    // tag resolvers (as registered by AddTagMap)
    map<string, BaseTagMapper*> m_TagMap;

    // cloning
    virtual CNCBINode* CloneSelf(void) const;
};


////////////////////////////////////
//  this is the basic 3 section NCBI page


class CHTMLPage : public CHTMLBasicPage
{
    // parent class
    typedef CHTMLBasicPage CParent;

public:
    // 'tors
    CHTMLPage(const string& title         = NcbiEmptyString,
              const string& template_file = NcbiEmptyString);
    CHTMLPage(CCgiApplication* app,
              int              style         = 0,  // see "enum flags" beneath
              const string&    title         = NcbiEmptyString,
              const string&    template_file = NcbiEmptyString);
    static CHTMLBasicPage* New(void);

    // style flags
    enum flags {
        kNoTITLE    = 0x1,
        kNoVIEW     = 0x2,
        kNoTEMPLATE = 0x4
    };

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

protected:
    virtual CNCBINode* CloneSelf(void) const;

private:
    void Init(void);

    string m_Title;
    string m_TemplateFile;
};

#include <html/page.inl>

END_NCBI_SCOPE

#endif
