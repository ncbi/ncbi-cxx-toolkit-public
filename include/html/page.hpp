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
* Revision 1.1  1998/10/29 16:15:53  lewisg
* version 2
*
* ===========================================================================
*/



#include <html.hpp>

/////////////////////////////////////////////////////////////
// CHTMLBasicPage is the virtual base class.  The main functionality is
// the turning on and off of sub HTML components via style bits and a
// creation function that orders sub components on the page.  The ability
// to hold children and print HTML is inherited from CHTMLNode.
  
class CHTMLBasicPage: public CHTMLNode {
public: 
    virtual void Create() { Create(0); }
    virtual void Create(int style) { Init(style); Draw(style); }
    virtual void Init(int style) {}  // initialize members
    virtual void Draw(int) = 0;  // create and aggregate sub pages + other html

};


const int kNoTITLE = 0x1;
const int kNoVIEW = 0x2;

class CHTMLPage: public CHTMLBasicPage {
public:

    ////////// how to make the page

    virtual void Init(int);
    virtual void Draw(int);

    ////////// the individual components of the page

    virtual CHTMLNode * CreateTemplate();
    CHTMLNode * m_Template;

    virtual CHTMLNode * CreateTitle();
    CHTMLNode * m_Title;

    virtual CHTMLNode * CreateView();
    CHTMLNode * m_View;

    ////////// 'tor

    CHTMLPage() { m_Title = NULL; m_View = NULL; m_Template = NULL;}


};

#endif
