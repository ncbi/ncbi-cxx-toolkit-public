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
* Revision 1.3  1998/12/08 00:34:55  lewisg
* cleanup
*
* Revision 1.2  1998/12/01 19:09:06  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.1  1998/10/29 16:15:53  lewisg
* version 2
*
* ===========================================================================
*/

#include <corelib/cgiapp.hpp>
#include <html/html.hpp>
#include <html/nodemap.hpp>

BEGIN_NCBI_SCOPE

struct BaseTagMapper;

inline BaseTagMapper* CreateTagMapper(CNCBINode* node);
inline BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(void));
inline BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(const string& name));

template<class C>
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node));

template<class C>
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node, const string& name));

template<class C>
inline BaseTagMapper* CreateTagMapper(const C*, CNCBINode* (C::*method)(void))
{
    return new TagMapper<C>(method);
}

template<class C>
inline BaseTagMapper* CreateTagMapper(const C*, CNCBINode* (C::*method)(const string& name))
{
    return new TagMapperByName<C>(method);
}

/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.

class CHTMLBasicPage: public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public: 
    CHTMLBasicPage(void);
    CHTMLBasicPage(CCgiApplication* app, int style = 0);

    virtual CCgiApplication * GetApplication(void) const;
    virtual void SetApplication(CCgiApplication * App);

    int GetStyle(void) const;
    void SetStyle(int style);

    // resolve <@XXX@> tag
    virtual CNCBINode* MapTag(const string& name);
    // add tag resolver
    void AddTagMap(const string& name, BaseTagMapper* mapper);
    void AddTagMap(const string& name, CNCBINode* node);

    //    void AddTagMap(const string& name, CNCBINode* (*function)(void));
    //    void AddTagMap(const string& name, CNCBINode* (*function)(const string& name));

protected:
    CCgiApplication * m_CgiApplication;  // pointer to runtime information
    int m_Style;

    map<string, BaseTagMapper*> m_TagMap;

    // cloning
    virtual CNCBINode* CloneSelf() const;
};


////////////////////////////////////
//  this is the basic 3 section NCBI page


class CHTMLPage : public CHTMLBasicPage {
    // parent class
    typedef CHTMLBasicPage CParent;

public:
    ////////// 'tors

    CHTMLPage(void);
    CHTMLPage(CCgiApplication* app, int style = 0);
    static CHTMLBasicPage * New(void);

    ////////// flags

    enum flags {
        kNoTITLE    = 0x1,
        kNoVIEW     = 0x2,
        kNoTEMPLATE = 0x4
    };

    ////////// page parameters

    string m_PageName;
    string m_TemplateFile;

    ////////// the individual sub pages

    virtual void CreateSubNodes(void);

    virtual CNCBINode* CreateTemplate(void);
    virtual CNCBINode* CreateTitle(void);
    virtual CNCBINode* CreateView(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;

private:
    void Init(void);
};

#include <html/page.inl>

END_NCBI_SCOPE

#endif
