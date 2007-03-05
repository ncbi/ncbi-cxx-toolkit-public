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

#include <ncbi_pch.hpp>
#include <html/components.hpp>
#include <html/nodemap.hpp>


BEGIN_NCBI_SCOPE


CSubmitDescription::CSubmitDescription(void)
{
    return;
}


CSubmitDescription::CSubmitDescription(const string& name)
    : m_Name(name)
{
    return;
}


CSubmitDescription::CSubmitDescription(const string& name, const string& label)
    : m_Name(name), m_Label(label)
{
    return;
}


CNCBINode* CSubmitDescription::CreateComponent(void) const
{
    if ( m_Name.empty() ) {
        return 0;
    }
    if ( m_Label.empty() ) {
        return new CHTML_submit(m_Name);
    }else {
        return new CHTML_submit(m_Name, m_Label);
    }
}


CNCBINode* COptionDescription::CreateComponent(const string& def) const
{
    if ( m_Value.empty() ) {
        return new CHTML_option(m_Label, m_Label == def);
    } else if ( m_Label.empty() ) {
        return new CHTML_option(m_Value, m_Value == def);
    } else {
        return new CHTML_option(m_Value, m_Label, m_Value == def);
    }
}


CSelectDescription::CSelectDescription(void)
{
    return;
}


CSelectDescription::CSelectDescription(const string& name)
    : m_Name(name)
{
    return;
}


void CSelectDescription::Add(const string& value)
{
    m_List.push_back(COptionDescription(value));
}


void CSelectDescription::Add(const string& value, const string& label)
{
    m_List.push_back(COptionDescription(value, label));
}


CNCBINode* CSelectDescription::CreateComponent(void) const
{
    if ( m_Name.empty() || m_List.empty() ) {
        return 0;
    }
    CNCBINode* select = new CHTML_select(m_Name);
    for ( list<COptionDescription>::const_iterator i = m_List.begin();
          i != m_List.end(); ++i ) {
        select->AppendChild(i->CreateComponent(m_Default));
    }
    if ( !m_TextBefore.empty() || !m_TextAfter.empty() ) {
        CNCBINode* combine = new CNCBINode;
        if ( !m_TextBefore.empty() ) {
            combine->AppendChild(new CHTMLPlainText(m_TextBefore));
        }
        combine->AppendChild(select);
        if ( !m_TextAfter.empty() ) {
            combine->AppendChild(new CHTMLPlainText(m_TextAfter));
        }
        select = combine;
    }
    return select;
}


CTextInputDescription::CTextInputDescription(void)
    : m_Width(0)
{
    return;
}


CTextInputDescription::CTextInputDescription(const string& name)
    : m_Name(name), m_Width(0)
{
    return;
}


CNCBINode* CTextInputDescription::CreateComponent(void) const
{
    if ( m_Name.empty() ) {
        return 0;
    }
    if ( m_Width ) {
        return new CHTML_text(m_Name, m_Width, m_Value);
    } else {
        return new CHTML_text(m_Name, m_Value);
    }
}


CQueryBox::CQueryBox(void)
    : m_Submit("cmd", "Search"), m_Database("db"),
      m_Term("term"), m_DispMax("dispmax"),
      m_Width(-1)
{
    SetCellSpacing(0);
    SetCellPadding(5);
    m_Database.m_TextBefore = "Search ";
    m_Database.m_TextAfter = "for";
    m_DispMax.m_TextBefore = "Show ";
    m_DispMax.m_TextAfter = "documents per page";
}


void CQueryBox::CreateSubNodes()
{
    SetBgColor(m_BgColor);
    if ( m_Width >= 0 ) {
        SetWidth(m_Width);
    }
    CheckTable();
    int row = CalculateNumberOfRows();

    InsertAt(row, 0, m_Database.CreateComponent())->SetColSpan(2);
    InsertAt(row + 1, 0, m_Term.CreateComponent());
    InsertAt(row + 1, 0, m_Submit.CreateComponent());
    InsertAt(row + 2, 0, m_DispMax.CreateComponent()); 
}


CNCBINode* CQueryBox::CreateComments(void)
{
    return 0;
}


// Pager

CButtonList::CButtonList(void)
{
    return;
}

void CButtonList::CreateSubNodes()
{
    CNCBINode* select = m_List.CreateComponent();
    if ( select ) {
        AppendChild(m_Button.CreateComponent());
        AppendChild(select);
    }
}


CPageList::CPageList(void)
    : m_Current(-1)
{
    SetCellSpacing(2);
}


void CPageList::x_AddImageString(CNCBINode* node, const string& name, int number,
                                 const string& imageStart, const string& imageEnd)
{
    string s = NStr::IntToString(number);

    for ( size_t i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_image(name, imageStart + s[i] + imageEnd, 0));
    }
}


void CPageList::x_AddInactiveImageString(CNCBINode* node, const string&,
                                         int number,
                                         const string& imageStart,
                                         const string& imageEnd)
{
    string s = NStr::IntToString(number);
   
    for ( size_t i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_img(imageStart + s[i] + imageEnd));
    }
}


void CPageList::CreateSubNodes()
{
    int column = 0;
    if ( !m_Backward.empty() ) {
        InsertAt(0, column++,
                 new CHTML_image(m_Backward, "/images/prev.gif", 0));
    }
    for (map<int, string>::iterator i = m_Pages.begin();
         i != m_Pages.end(); ++i ) {
        if ( i->first == m_Current ) {
            // Current link
            x_AddInactiveImageString(Cell(0, column++), i->second, i->first, "/images/black_", ".gif");
        }
        else {
            // Normal link
            x_AddImageString(Cell(0, column++), i->second, i->first, "/images/", ".gif");
        }
    }
    if ( !m_Forward.empty() ) {
        InsertAt(0, column++, new CHTML_image(m_Forward, "/images/next.gif", 0));
    }
}


// Pager box

CPagerBox::CPagerBox(void)
    : m_Width(460),
      m_TopButton(new CButtonList),
      m_LeftButton(new CButtonList),
      m_RightButton(new CButtonList),
      m_PageList(new CPageList),
      m_NumResults(0),
      m_BgColor("#c0c0c0")
{
    return;
}


void CPagerBox::CreateSubNodes(void)
{
    CHTML_table* table;
    CHTML_table* tableTop;
    CHTML_table* tableBot;

    table = new CHTML_table();
    table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)->SetWidth(m_Width)->SetAttribute("border", "0");
    AppendChild(table);

    tableTop = new CHTML_table();
    tableTop->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);
    tableBot = new CHTML_table();
    tableBot->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);

    table->InsertAt(0, 0, tableTop);
    table->InsertAt(1, 0, tableBot);
    tableTop->InsertAt(0, 0, m_TopButton);
    tableTop->InsertAt(0, 1, m_PageList);
    tableBot->InsertAt(0, 0, m_LeftButton);
    tableBot->InsertAt(0, 1, m_RightButton);
    tableBot->InsertAt(0, 2, new CHTMLText(NStr::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
}


CSmallPagerBox::CSmallPagerBox()
    : m_Width(460), m_PageList(0), m_NumResults(0)
{
    return;
}


void CSmallPagerBox::CreateSubNodes()
{
    CHTML_table* Table = new CHTML_table();
    AppendChild(Table);
    Table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)
        ->SetWidth(m_Width)->SetAttribute("border", 0);
    
    Table->InsertAt(0, 0, new CPageList);
    Table->InsertAt(0, 1, new CHTMLText(NStr::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
}


END_NCBI_SCOPE
