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
#include <ncbiapp.hpp>
BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.
  
class CHTMLBasicPage: public CHTMLNode {
public: 
    CHTMLBasicPage();
    virtual void Create(void) { Create(0); }
    virtual void Create(int style) { InitMembers(style); InitSubPages(style); Finish(style); }
    virtual void InitMembers(int style) {}  
    virtual void InitSubPages(int style) {}  // initialize members
    virtual void Finish(int style) = 0;  // create and aggregate sub pages + other html
    virtual CCgiApplication * GetApplication(void) { return m_CgiApplication; }
    virtual void SetApplication(CCgiApplication * App) { m_CgiApplication = App; }

private:
    CCgiApplication * m_CgiApplication;  // pointer to runtime information  
};


////////////////////////////////////
//  this is the basic 3 section NCBI page


class CHTMLPage: public CHTMLBasicPage {
public:
    ////////// 'tors

    CHTMLPage();
    static CHTMLBasicPage * New(void) { return new CHTMLPage; }  // for the page factory

    ////////// flags

    static const int kNoTITLE = 0x1;
    static const int kNoVIEW = 0x2;

    ////////// how to make the page

    virtual void InitMembers(int);
    virtual void InitSubPages(int);
    virtual void Finish(int);

    ////////// page parameters

    string m_PageName;
    string m_TemplateFile;

    ////////// the individual sub pages

    virtual CHTMLNode * CreateTemplate(void);
    CHTMLNode * m_Template;

    virtual CHTMLNode * CreateTitle(void);
    CHTMLNode * m_Title;

    virtual CHTMLNode * CreateView(void);
    CHTMLNode * m_View;


};


END_NCBI_SCOPE
#endif
