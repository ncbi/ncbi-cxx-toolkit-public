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

const int kNoCOMMENTS = 0x2;
const int kNoLIST = 0x1;

class CQueryBox: public CHTMLBasicPage {
public:

    virtual void InitMembers(int);
    virtual void InitSubPages(int);
    virtual void Draw(int);

    ////////  members

    string m_Width; // in pixels
    string m_BgColor;
    string m_DbName;  // name of the database field
    map < string, string, less<string> > m_Databases;  // the list of databases
    map < string, string, less<string> > m_HiddenValues;
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
    ~CQueryBox() { delete m_Comments; }
 
};

END_NCBI_SCOPE
#endif
