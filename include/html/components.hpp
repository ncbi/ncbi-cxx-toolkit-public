#ifndef COMPONENTS__HPP
#define COMPONENTS__HPP

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
*   The individual html components used on a page
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1998/12/11 18:13:50  lewisg
* frontpage added
*
* Revision 1.4  1998/12/09 23:02:55  lewisg
* update to new cgiapp class
*
* Revision 1.3  1998/12/08 00:34:54  lewisg
* cleanup
*
* Revision 1.2  1998/11/23 23:47:48  lewisg
* *** empty log message ***
*
* Revision 1.1  1998/10/29 16:15:51  lewisg
* version 2
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <html.hpp>
#include <page.hpp>
BEGIN_NCBI_SCOPE


class CQueryBox: public CHTMLBasicPage 
{
public:

    virtual void InitMembers(int);
    virtual void InitSubPages(int);
    virtual void Finish(int);

    ////////  members

    static const int kNoCOMMENTS = 0x2;
    static const int kNoLIST = 0x1;

    int m_Width; // in pixels
    string m_BgColor;
    string m_DbName;  // name of the database field
    map < string, string > m_Databases;  // the list of databases
    map < string, string > m_HiddenValues;
    string m_TermName;
    string m_DispMax;  // name of dispmax field
    string m_DefaultDispMax;
    list < string > m_Disp;  // the values in dispmax field
    string m_URL;

    //////// subpages

    virtual CHTMLNode * CreateComments(void);
    CHTMLNode * m_Comments;  // extra comments

    //////// 'tors

    CQueryBox();
};


// makes a button followed by a drop down

class CButtonList: public CHTMLBasicPage
{
public:
    string m_Name;  // button name
    string m_Select;  // select tag name
    map < string, string > m_List;
    virtual void Finish(int);
};


// makes a set of pagination links

class CPageList: public CHTMLBasicPage
{
public:
    map < int, string > m_Pages; // number, href
    string m_Forward; // forward url
    string m_Backward; // backward url
    virtual void Finish(int);

};


class CPagerBox: public CHTMLBasicPage
{
public:
    int m_Width; // in pixels
    CButtonList * m_TopButton;  // display button
    CButtonList * m_LeftButton; // save button
    CButtonList * m_RightButton; // order button
    CPageList * m_PageList;  // the pager
    int m_NumResults;  // the number of results to display

    virtual void InitMembers(int);
    virtual void Finish(int);

    CPagerBox();
};


class CSmallPagerBox: public CHTMLBasicPage
{
public:
    int m_Width; // in pixels
    CPageList * m_PageList;  // the pager
    int m_NumResults;  // the number of results to display

    virtual void InitMembers(int);
    virtual void Finish(int);

    CSmallPagerBox();
};


END_NCBI_SCOPE
#endif
