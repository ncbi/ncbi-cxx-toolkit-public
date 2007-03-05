#ifndef HTML___COMPONENTS__HPP
#define HTML___COMPONENTS__HPP

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
 */

/// @file components.hpp
/// The HTML page.
///
/// Defines the individual html components used on a page.


#include <html/html.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XHTML_EXPORT CSubmitDescription
{
public:
    CSubmitDescription(void);
    CSubmitDescription(const string& name);
    CSubmitDescription(const string& name, const string& label);

    CNCBINode* CreateComponent(void) const;
public:
    string m_Name;
    string m_Label;
};


class NCBI_XHTML_EXPORT COptionDescription
{
public:
    COptionDescription(void);
    COptionDescription(const string& value);
    COptionDescription(const string& value, const string& label);

    CNCBINode* CreateComponent(const string& def) const;
public:
    string m_Value;
    string m_Label;
};


class NCBI_XHTML_EXPORT CSelectDescription
{
public:
    CSelectDescription(void);
    CSelectDescription(const string& value);

    void Add(const string& value);
    void Add(const string& value, const string& label);
    void Add(int value);

    CNCBINode* CreateComponent(void) const;
public:
    string                   m_Name;
    list<COptionDescription> m_List;
    string                   m_Default;
    string                   m_TextBefore;
    string                   m_TextAfter;
};


class NCBI_XHTML_EXPORT CTextInputDescription
{
public:
    CTextInputDescription(void);
    CTextInputDescription(const string& value);

    CNCBINode* CreateComponent(void) const;
public:
    string  m_Name;
    string  m_Value;
    int     m_Width;
};


class NCBI_XHTML_EXPORT CQueryBox: public CHTML_table
{
    // Parent class
    typedef CHTML_form CParent;
public:
    // 'tors
    CQueryBox(void);

    // Flags
    enum flags {
        kNoLIST     = 0x1,
        kNoCOMMENTS = 0x2
    };

    // Subpages
    virtual void       CreateSubNodes(void);
    virtual CNCBINode* CreateComments(void);

public:
    CSubmitDescription    m_Submit;
    CSelectDescription    m_Database;
    CTextInputDescription m_Term;
    CSelectDescription    m_DispMax;

    int                   m_Width;    // in pixels
    string                m_BgColor;
};


// Make a button followed by a drop down.
class NCBI_XHTML_EXPORT CButtonList: public CNCBINode
{
    // Parent class
    typedef CHTML_form CParent;
public:
    CButtonList(void);
    virtual void CreateSubNodes(void);

public:
    CSubmitDescription m_Button;
    CSelectDescription m_List;
};


// Make a set of pagination links
class NCBI_XHTML_EXPORT CPageList: public CHTML_table
{
    // Parent class
    typedef CHTML_table CParent;
    
public:
    CPageList(void);
    virtual void CreateSubNodes(void);
public:
    map<int,string> m_Pages;     // number, href
    string          m_Forward;   // forward url
    string          m_Backward;  // backward url
    int             m_Current;   // current page number
    
private:
    void x_AddInactiveImageString
    (CNCBINode* node, const string& name, int number,
     const string& imageStart, const string& imageEnd);
    void x_AddImageString
    (CNCBINode* node, const string& name, int number,
     const string& imageStart, const string& imageEnd);
};


class NCBI_XHTML_EXPORT CPagerBox: public CNCBINode
{
    // Parent class
    typedef CHTML_form CParent;
public:
    CPagerBox(void);
    virtual void CreateSubNodes(void);
public:
    int           m_Width;       // in pixels
    CButtonList*  m_TopButton;   // display button
    CButtonList*  m_LeftButton;  // save button
    CButtonList*  m_RightButton; // order button
    CPageList*    m_PageList;    // the pager
    int           m_NumResults;  // the number of results to display
    string        m_BgColor;
};


class NCBI_XHTML_EXPORT CSmallPagerBox: public CNCBINode
{
    // parent class
    typedef CHTML_form CParent;
public:
    CSmallPagerBox(void);
    virtual void CreateSubNodes(void);

public:
    int        m_Width;      // in pixels
    CPageList* m_PageList;   // the pager
    int        m_NumResults; // the number of results to display
    string     m_BgColor;
};


#include <html/components.inl>


END_NCBI_SCOPE


/* @} */

#endif  /* HTML___COMPONENTS__HPP */
