#ifndef QUERYPAGES__HPP
#define QUERYPAGES__HPP

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
*   Pages for pubmed query program
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/12/12 00:06:56  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/12/11 22:53:41  lewisg
* added docsum page
*
* Revision 1.1  1998/12/11 18:13:51  lewisg
* frontpage added
*

*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <page.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////
///// Basic Search


class CPmFrontPage: public CHTMLPage {
public:
    virtual void InitMembers(int Style = 0);
    virtual CHTMLNode * CreateView(void);

    CPmFrontPage();
    static CHTMLBasicPage * New(void) { return new CPmFrontPage;}
};


class CPmDocSumPage: public CHTMLPage
{
 public:
    virtual void InitMembers(int Style = 0);
    virtual void InitSubPages(int Style = 0);
    virtual void Finish(int Style = 0);

    static const int kNoQUERYBOX = 0x8;
    virtual CHTMLNode * CreateQueryBox();
    CHTMLNode * m_QueryBox;

    // only one pager for now until I make copy constructor.
    static const int kNoPAGER = 0x16;
    virtual CHTMLNode * CreatePager();
    CHTMLNode * m_Pager;

    virtual CHTMLNode * CreateView(void);

    CPmDocSumPage();
    static CHTMLBasicPage * New(void) { return new CPmDocSumPage;}

};

#if 0
CPmReportPage: public CPmDocSumPage
{
 public:
    virtual void InitMembers(int);
    virtual void InitSubPages(int Style);
    virtual void Finish(int Style);
    virtual CHTMLNode * CreateView(void);

    CPmReportPage();
    static CHTMLBasicPage * New(void) { return new CPmReportPage;}
};
#endif /* 0 */

END_NCBI_SCOPE
#endif
