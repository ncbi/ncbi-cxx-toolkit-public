#ifndef PAGE__HPP
#define PAGE__HPP

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
*   The HTML page
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <ncbistd.hpp>
#include <html.hpp>
#include <cgiapp.hpp>
#include <nodemap.hpp>

BEGIN_NCBI_SCOPE

class BaseTagMapper;

template<class C>
BaseTagMapper* CreateTagMapper(const C* _this, CNCBINode* (C::*method)(void));

template<class C>
BaseTagMapper* CreateTagMapper(const C* _this, CNCBINode* (C::*method)(const string& name));

/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.

class CHTMLBasicPage: public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public: 
    CHTMLBasicPage(CCgiApplication* application = 0, int style = 0);

    virtual CCgiApplication * GetApplication(void);
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

/*
    template<class C>
    void AddTagMap(const string& name, CNCBINode* (C::*method)(void));

    template<class C>
    void AddTagMap(const string& name, CNCBINode* (C::*method)(const string& name));
*/

protected:
    CCgiApplication * m_CgiApplication;  // pointer to runtime information
    int m_Style;

    map<string, BaseTagMapper*> m_TagMap;

    // cloning
    virtual CNCBINode* CloneSelf() const;
    //    CHTMLBasicPage(const CHTMLBasicPage& origin);
};


////////////////////////////////////
//  this is the basic 3 section NCBI page


class CHTMLPage : public CHTMLBasicPage {
    // parent class
    typedef CHTMLBasicPage CParent;

public:
    ////////// 'tors

    CHTMLPage(CCgiApplication* application = 0, int style = 0);
    static CHTMLBasicPage * New(void);

    ////////// flags

    static const int kNoTITLE = 0x1;
    static const int kNoVIEW = 0x2;
    static const int kNoTEMPLATE = 0x4;

    ////////// page parameters

    string m_PageName;
    string m_TemplateFile;

    ////////// the individual sub pages

    virtual void CreateSubNodes(void);

    virtual CNCBINode* CreateTemplate(void);
    virtual CNCBINode* CreateTitle(void);
    virtual CNCBINode* CreateView(void);

    // cloning
    virtual CNCBINode* CloneSelf() const;
    //    CHTMLPage(const CHTMLPage& origin);
};


// inline functions here:
#include <page.inl>

END_NCBI_SCOPE

#endif
