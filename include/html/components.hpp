#ifndef COMPONENTS__HPP
#define COMPONENTS__HPP

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
*   The individual html components used on a page
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  1999/01/28 21:58:04  vasilche
* QueryBox now inherits from CHTML_table (not CHTML_form as before).
* Use 'new CHTML_form("url", queryBox)' as replacement of old QueryBox.
*
* Revision 1.18  1999/01/21 21:12:53  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* Revision 1.17  1999/01/20 18:12:42  vasilche
* Added possibility to change label of buttons.
*
* Revision 1.16  1999/01/19 21:17:37  vasilche
* Added CPager class
*
* Revision 1.15  1999/01/15 20:22:32  volodya
* syntax fix
*
* Revision 1.14  1999/01/14 21:25:16  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.13  1999/01/07 17:06:32  vasilche
* Added default query text in CQueryBox.
* Added query text width in CQueryBox.
*
* Revision 1.12  1999/01/07 16:41:52  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* Revision 1.11  1999/01/05 21:47:09  vasilche
* Added 'current page' to CPageList.
* CPageList doesn't display forward/backward if empty.
*
* Revision 1.10  1998/12/28 23:29:02  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 16:48:04  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.8  1998/12/23 21:20:56  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.7  1998/12/21 22:24:55  vasilche
* A lot of cleaning.
*
* Revision 1.6  1998/12/11 22:53:39  lewisg
* added docsum page
*
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

#include <html/html.hpp>

BEGIN_NCBI_SCOPE

class CSubmitDescription
{
public:
    string m_Name;
    string m_Label;

    CSubmitDescription(void);
    CSubmitDescription(const string& name);
    CSubmitDescription(const string& name, const string& label);

    CNCBINode* CreateComponent(void) const;
};

class COptionDescription
{
public:
    string m_Value;
    string m_Label;

    COptionDescription(void);
    COptionDescription(const string& value);
    COptionDescription(const string& value, const string& label);

    CNCBINode* CreateComponent(const string& def) const;
};

class CSelectDescription
{
public:
    string m_Name;
    list<COptionDescription> m_List;
    string m_Default;

    string m_TextBefore;
    string m_TextAfter;

    CSelectDescription(void);
    CSelectDescription(const string& value);

    void Add(const string& value);
    void Add(const string& value, const string& label);
    void Add(int value);

    CNCBINode* CreateComponent(void) const;
};

class CTextInputDescription
{
public:
    string m_Name;
    string m_Value;
    int m_Width;

    CTextInputDescription(void);
    CTextInputDescription(const string& value);

    CNCBINode* CreateComponent(void) const;
};

class CQueryBox: public CHTML_table
{
    // parent class
    typedef CHTML_form CParent;

public:

    //////// 'tors

    CQueryBox(void);

    ////////  members

    static const int kNoCOMMENTS = 0x2;
    static const int kNoLIST = 0x1;

    int m_Width; // in pixels
    string m_BgColor;

    //    map<string, string> m_HiddenValues;

    CSubmitDescription m_Submit;
    CSelectDescription m_Database;
    CTextInputDescription m_Term;
    CSelectDescription m_DispMax;

    //////// subpages

    virtual void CreateSubNodes(void);
    virtual CNCBINode* CreateComments(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};


// makes a button followed by a drop down

class CButtonList: public CNCBINode
{
    // parent class
    typedef CHTML_form CParent;

public:
    CButtonList(void);

    CSubmitDescription m_Button;
    CSelectDescription m_List;

    virtual void CreateSubNodes(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// makes a set of pagination links

class CPageList: public CHTML_table
{
    // parent class
    typedef CHTML_table CParent;
    
public:
    CPageList(void);
    
    map<int, string> m_Pages; // number, href
    string m_Forward; // forward url
    string m_Backward; // backward url
    int m_Current; // current page number
    
    virtual void CreateSubNodes(void);
    
private:
    void x_AddInactiveImageString(CNCBINode* node, const string& name, int number,
                                  const string& imageStart, const string& imageEnd);
    void x_AddImageString(CNCBINode* node, const string& name, int number,
                          const string& imageStart, const string& imageEnd);
};

class CPagerBox: public CNCBINode
{
    // parent class
    typedef CHTML_form CParent;

public:
    CPagerBox(void);

    int m_Width; // in pixels
    CButtonList* m_TopButton;  // display button
    CButtonList* m_LeftButton; // save button
    CButtonList* m_RightButton; // order button
    CPageList* m_PageList;  // the pager
    int m_NumResults;  // the number of results to display

    string m_BgColor;
    
    virtual void CreateSubNodes(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

class CSmallPagerBox: public CNCBINode
{
    // parent class
    typedef CHTML_form CParent;

public:
    CSmallPagerBox(void);

    int m_Width; // in pixels
    CPageList * m_PageList;  // the pager
    int m_NumResults;  // the number of results to display

    string m_BgColor;
    
    virtual void CreateSubNodes(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

#include <html/components.inl>

END_NCBI_SCOPE

#endif
