#ifndef HTML__HPP
#define HTML__HPP

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
*   CHTMLNode defines
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.37  1999/08/20 16:14:36  golikov
* 'non-<TR> tag' bug fixed
*
* Revision 1.36  1999/05/28 18:04:05  vakatov
* CHTMLNode::  added attribute "CLASS"
*
* Revision 1.35  1999/05/20 16:49:11  pubmed
* Changes for SaveAsText: all Print() methods get mode parameter that can be HTML or PlainText
*
* Revision 1.34  1999/05/10 17:01:11  vasilche
* Fixed warning on Sun by renaming CHTML_font::SetSize() -> SetFontSize().
*
* Revision 1.33  1999/05/10 14:26:09  vakatov
* Fixes to compile and link with the "egcs" C++ compiler under Linux
*
* Revision 1.32  1999/04/27 14:49:57  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.31  1999/04/22 14:20:10  vasilche
* Now CHTML_select::AppendOption and CHTML_option constructor accept option
* name always as first argument.
*
* Revision 1.30  1999/04/15 22:05:16  vakatov
* Include NCBI C++ headers before the standard ones
*
* Revision 1.29  1999/04/15 19:48:16  vasilche
* Fixed several warnings detected by GCC
*
* Revision 1.28  1999/04/08 19:00:24  vasilche
* Added current cell pointer to CHTML_table
*
* Revision 1.27  1999/03/01 21:03:27  vasilche
* Added CHTML_file input element.
* Changed CHTML_form constructors.
*
* Revision 1.26  1999/02/26 21:03:30  vasilche
* CAsnWriteNode made simple node. Use CHTML_pre explicitely.
* Fixed bug in CHTML_table::Row.
* Added CHTML_table::HeaderCell & DataCell methods.
*
* Revision 1.25  1999/02/02 17:57:46  vasilche
* Added CHTML_table::Row(int row).
* Linkbar now have equal image spacing.
*
* Revision 1.24  1999/01/28 21:58:05  vasilche
* QueryBox now inherits from CHTML_table (not CHTML_form as before).
* Use 'new CHTML_form("url", queryBox)' as replacement of old QueryBox.
*
* Revision 1.23  1999/01/28 16:58:58  vasilche
* Added several constructors for CHTML_hr.
* Added CHTMLNode::SetSize method.
*
* Revision 1.22  1999/01/25 19:34:14  vasilche
* String arguments which are added as HTML text now treated as plain text.
*
* Revision 1.21  1999/01/21 21:12:54  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* Revision 1.20  1999/01/14 21:25:16  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.19  1999/01/11 22:05:48  vasilche
* Fixed CHTML_font size.
* Added CHTML_image input element.
*
* Revision 1.18  1999/01/11 15:13:32  vasilche
* Fixed CHTML_font size.
* CHTMLHelper extracted to separate file.
*
* Revision 1.17  1999/01/07 16:41:53  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* Revision 1.16  1999/01/05 21:47:10  vasilche
* Added 'current page' to CPageList.
* CPageList doesn't display forward/backward if empty.
*
* Revision 1.15  1999/01/04 20:06:09  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.13  1998/12/28 21:48:12  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.12  1998/12/28 16:48:05  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.11  1998/12/24 16:15:36  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.10  1998/12/23 21:20:56  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.9  1998/12/23 14:28:07  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.8  1998/12/21 22:24:56  vasilche
* A lot of cleaning.
*
* Revision 1.7  1998/12/09 23:02:55  lewisg
* update to new cgiapp class
*
* Revision 1.6  1998/12/08 00:34:55  lewisg
* cleanup
*
* Revision 1.5  1998/12/03 22:49:10  lewisg
* added HTMLEncode() and CHTML_img
*
* Revision 1.4  1998/12/01 19:09:05  lewisg
* uses CCgiApplication and new page factory
* ===========================================================================
*/

#include <html/node.hpp>
#include <map>
#include <vector>


BEGIN_NCBI_SCOPE

// base class for html node
class CHTMLNode : public CNCBINode
{
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLNode(void);
    CHTMLNode(const string& name);

    // convenient way to set some common attributes
    CHTMLNode* SetClass(const string& class_name);
    CHTMLNode* SetWidth(int width);
    CHTMLNode* SetWidth(const string& width);
    CHTMLNode* SetHeight(int height);
    CHTMLNode* SetHeight(const string& width);
    CHTMLNode* SetSize(int size);
    CHTMLNode* SetAlign(const string& align);
    CHTMLNode* SetVAlign(const string& align);
    CHTMLNode* SetBgColor(const string& color);
    CHTMLNode* SetColor(const string& color);
    CHTMLNode* SetNameAttribute(const string& name);
    string GetNameAttribute(void) const;

    // convenient way to add CHTMLPlainText or CHTMLText
    void AppendPlainText(const string &);
    void AppendHTMLText (const string &);
};

// <@XXX@> mapping node
class CHTMLTagNode : public CNCBINode
{
    typedef CNCBINode CParent;

public:
    CHTMLTagNode(const string& tagname);

    virtual void CreateSubNodes(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};


// A text node that contains plain text
class CHTMLPlainText : public CNCBINode
{
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLPlainText(const string& text);
    
    const string& GetText(void) const;
    void SetText(const string& text);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode = eHTML);

protected:
    string m_Text;  // the text

    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// A text node that contains html text with tags and possibly <@TAG@>
class CHTMLText : public CNCBINode
{
    typedef CNCBINode CParent;

public:
    CHTMLText(const string& text);
    
    const string& GetText(void) const;

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode = eHTML);
    virtual void CreateSubNodes();

protected:
    string m_Text;  // the text

    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// An html tag
class CHTMLOpenElement: public CHTMLNode
{
    typedef CHTMLNode CParent;

public:
    CHTMLOpenElement(const string& name);
    CHTMLOpenElement(const string& name, CNCBINode* node);
    CHTMLOpenElement(const string& name, const string& text);

    // prints tag itself
    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode = eHTML);   

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// An html tag
class CHTMLElement: public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;

public:
    CHTMLElement(const string& name);
    CHTMLElement(const string& name, CNCBINode* node);
    CHTMLElement(const string& name, const string& text);

    // prints tag close    
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode = eHTML);   

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

class CHTMLComment : public CHTMLNode
{
    typedef CHTMLNode CParent;

public:
    CHTMLComment();
    CHTMLComment(CNCBINode* node);
    CHTMLComment(const string& text);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode = eHTML);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode = eHTML);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// HTML element names (in order of appearence in third edition of
//    'HTML sourcebook'
extern const string KHTMLTagName_html;
extern const string KHTMLTagName_head;
extern const string KHTMLTagName_body;
extern const string KHTMLTagName_base;
extern const string KHTMLTagName_isindex;
extern const string KHTMLTagName_link;
extern const string KHTMLTagName_meta;
extern const string KHTMLTagName_script;
extern const string KHTMLTagName_style;
extern const string KHTMLTagName_title;
extern const string KHTMLTagName_address;
extern const string KHTMLTagName_blockquote;
extern const string KHTMLTagName_center;
extern const string KHTMLTagName_div;
extern const string KHTMLTagName_h1;
extern const string KHTMLTagName_h2;
extern const string KHTMLTagName_h3;
extern const string KHTMLTagName_h4;
extern const string KHTMLTagName_h5;
extern const string KHTMLTagName_h6;
extern const string KHTMLTagName_hr;
extern const string KHTMLTagName_p;
extern const string KHTMLTagName_pre;
extern const string KHTMLTagName_form;
extern const string KHTMLTagName_input;
extern const string KHTMLTagName_select;
extern const string KHTMLTagName_option;
extern const string KHTMLTagName_textarea;
extern const string KHTMLTagName_dl;
extern const string KHTMLTagName_dt;
extern const string KHTMLTagName_dd;
extern const string KHTMLTagName_ol;
extern const string KHTMLTagName_ul;
extern const string KHTMLTagName_dir;
extern const string KHTMLTagName_menu;
extern const string KHTMLTagName_li;
extern const string KHTMLTagName_table;
extern const string KHTMLTagName_caption;
extern const string KHTMLTagName_col;
extern const string KHTMLTagName_colgroup;
extern const string KHTMLTagName_thead;
extern const string KHTMLTagName_tbody;
extern const string KHTMLTagName_tfoot;
extern const string KHTMLTagName_tr;
extern const string KHTMLTagName_th;
extern const string KHTMLTagName_td;
extern const string KHTMLTagName_applet;
extern const string KHTMLTagName_param;
extern const string KHTMLTagName_img;
extern const string KHTMLTagName_a;
extern const string KHTMLTagName_cite;
extern const string KHTMLTagName_code;
extern const string KHTMLTagName_dfn;
extern const string KHTMLTagName_em;
extern const string KHTMLTagName_kbd;
extern const string KHTMLTagName_samp;
extern const string KHTMLTagName_strike;
extern const string KHTMLTagName_strong;
extern const string KHTMLTagName_var;
extern const string KHTMLTagName_b;
extern const string KHTMLTagName_big;
extern const string KHTMLTagName_font;
extern const string KHTMLTagName_i;
extern const string KHTMLTagName_s;
extern const string KHTMLTagName_small;
extern const string KHTMLTagName_sub;
extern const string KHTMLTagName_sup;
extern const string KHTMLTagName_tt;
extern const string KHTMLTagName_u;
extern const string KHTMLTagName_blink; // netscape specific
extern const string KHTMLTagName_br;
extern const string KHTMLTagName_basefont;
extern const string KHTMLTagName_map;
extern const string KHTMLTagName_area;

// HTML attribute names in alphabetical order
extern const string KHTMLAttributeName_action;
extern const string KHTMLAttributeName_align;
extern const string KHTMLAttributeName_bgcolor;
extern const string KHTMLAttributeName_cellpadding;
extern const string KHTMLAttributeName_cellspacing;
extern const string KHTMLAttributeName_checked;
extern const string KHTMLAttributeName_color;
extern const string KHTMLAttributeName_cols;
extern const string KHTMLAttributeName_compact;
extern const string KHTMLAttributeName_enctype;
extern const string KHTMLAttributeName_face;
extern const string KHTMLAttributeName_height;
extern const string KHTMLAttributeName_href;
extern const string KHTMLAttributeName_maxlength;
extern const string KHTMLAttributeName_method;
extern const string KHTMLAttributeName_multiple;
extern const string KHTMLAttributeName_name;
extern const string KHTMLAttributeName_noshade;
extern const string KHTMLAttributeName_rows;
extern const string KHTMLAttributeName_selected;
extern const string KHTMLAttributeName_size;
extern const string KHTMLAttributeName_src;
extern const string KHTMLAttributeName_start;
extern const string KHTMLAttributeName_type;
extern const string KHTMLAttributeName_valign;
extern const string KHTMLAttributeName_value;
extern const string KHTMLAttributeName_width;
extern const string KHTMLAttributeName_class;

// template for simple closed tag
template<const string* TagName>
class CHTMLElementTmpl : public CHTMLElement
{
    typedef CHTMLElement CParent;

public:
    static const string& s_GetTagName(void)
        { return *TagName; }

    CHTMLElementTmpl(void)
        : CParent(s_GetTagName()) {}
    CHTMLElementTmpl(CNCBINode* node)
        : CParent(s_GetTagName()) { AppendChild(node); }
    CHTMLElementTmpl(const string& text)
        : CParent(s_GetTagName()) { AppendPlainText(text); }

};

// template for open tag
template<const string* TagName>
class CHTMLOpenElementTmpl : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;

public:
    static const string& s_GetTagName(void)
        { return *TagName; }
 
    CHTMLOpenElementTmpl(void)
        : CParent(s_GetTagName()) {}
    CHTMLOpenElementTmpl(CNCBINode* node)
        : CParent(s_GetTagName()) { AppendChild(node); }
    CHTMLOpenElementTmpl(const string& text)
        : CParent(s_GetTagName()) { AppendPlainText(text); }
};

// template for lists (OL, UL, DIR, MENU)
template<const string* TagName>
class CHTMLListElementTmpl : public CHTMLElement
{
    typedef CHTMLElement CParent;

public:
    static const string& s_GetTagName(void);

    CHTMLListElementTmpl(void);
    CHTMLListElementTmpl(bool compact);
    CHTMLListElementTmpl(const string& type);
    CHTMLListElementTmpl(const string& type, bool compact);

    CHTMLListElementTmpl* AppendItem(const string& item);
    CHTMLListElementTmpl* AppendItem(CNCBINode* item);
};

class CHTML_tc : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    // type for row and column indexing
    typedef unsigned TIndex;

    CHTML_tc* SetRowSpan(TIndex span);
    CHTML_tc* SetColSpan(TIndex span);

protected:
    CHTML_tc(const string& name)
        : CParent(name)
        { }
};

template<const string* TagName>
class CHTML_tcTmpl : public CHTML_tc
{
    typedef CHTML_tc CParent;

public:
    static const string& s_GetTagName(void)
        { return *TagName; }

    CHTML_tcTmpl(void)
        : CParent(s_GetTagName()) {}
    CHTML_tcTmpl(CNCBINode* node)
        : CParent(s_GetTagName()) { AppendChild(node); }
    CHTML_tcTmpl(const string& text)
        : CParent(s_GetTagName()) { AppendPlainText(text); }

};

typedef CHTMLElementTmpl<&KHTMLTagName_html> CHTML_html;
typedef CHTMLElementTmpl<&KHTMLTagName_head> CHTML_head;
typedef CHTMLElementTmpl<&KHTMLTagName_body> CHTML_body;
typedef CHTMLElementTmpl<&KHTMLTagName_base> CHTML_base;
typedef CHTMLElementTmpl<&KHTMLTagName_isindex> CHTML_isindex;
typedef CHTMLElementTmpl<&KHTMLTagName_link> CHTML_link;
typedef CHTMLElementTmpl<&KHTMLTagName_meta> CHTML_meta;
typedef CHTMLElementTmpl<&KHTMLTagName_script> CHTML_script;
typedef CHTMLElementTmpl<&KHTMLTagName_style> CHTML_style;
typedef CHTMLElementTmpl<&KHTMLTagName_title> CHTML_title;
typedef CHTMLElementTmpl<&KHTMLTagName_address> CHTML_address;
typedef CHTMLElementTmpl<&KHTMLTagName_blockquote> CHTML_blockquote;
typedef CHTMLElementTmpl<&KHTMLTagName_center> CHTML_center;
typedef CHTMLElementTmpl<&KHTMLTagName_div> CHTML_div;
typedef CHTMLElementTmpl<&KHTMLTagName_h1> CHTML_h1;
typedef CHTMLElementTmpl<&KHTMLTagName_h2> CHTML_h2;
typedef CHTMLElementTmpl<&KHTMLTagName_h3> CHTML_h3;
typedef CHTMLElementTmpl<&KHTMLTagName_h4> CHTML_h4;
typedef CHTMLElementTmpl<&KHTMLTagName_h5> CHTML_h5;
typedef CHTMLElementTmpl<&KHTMLTagName_h6> CHTML_h6;
typedef CHTMLOpenElementTmpl<&KHTMLTagName_hr> CHTML_hr_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_p> CHTML_p;
typedef CHTMLOpenElementTmpl<&KHTMLTagName_p> CHTML_pnop;
typedef CHTMLElementTmpl<&KHTMLTagName_pre> CHTML_pre;
typedef CHTMLElementTmpl<&KHTMLTagName_form> CHTML_form_Base;
typedef CHTMLOpenElementTmpl<&KHTMLTagName_input> CHTML_input_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_select> CHTML_select_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_option> CHTML_option_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_textarea> CHTML_textarea_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_dl> CHTML_dl_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_dt> CHTML_dt;
typedef CHTMLElementTmpl<&KHTMLTagName_dd> CHTML_dd;
typedef CHTMLListElementTmpl<&KHTMLTagName_ol> CHTML_ol_Base;
typedef CHTMLListElementTmpl<&KHTMLTagName_ul> CHTML_ul;
typedef CHTMLListElementTmpl<&KHTMLTagName_dir> CHTML_dir;
typedef CHTMLListElementTmpl<&KHTMLTagName_menu> CHTML_menu;
typedef CHTMLElementTmpl<&KHTMLTagName_li> CHTML_li;
typedef CHTMLElementTmpl<&KHTMLTagName_table> CHTML_table_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_caption> CHTML_caption;
typedef CHTMLElementTmpl<&KHTMLTagName_col> CHTML_col;
typedef CHTMLElementTmpl<&KHTMLTagName_colgroup> CHTML_colgroup;
typedef CHTMLElementTmpl<&KHTMLTagName_thead> CHTML_thead;
typedef CHTMLElementTmpl<&KHTMLTagName_tbody> CHTML_tbody;
typedef CHTMLElementTmpl<&KHTMLTagName_tfoot> CHTML_tfoot;
typedef CHTMLElementTmpl<&KHTMLTagName_tr> CHTML_tr;
typedef CHTML_tcTmpl<&KHTMLTagName_th> CHTML_th;
typedef CHTML_tcTmpl<&KHTMLTagName_td> CHTML_td;
typedef CHTMLElementTmpl<&KHTMLTagName_applet> CHTML_applet;
typedef CHTMLElementTmpl<&KHTMLTagName_param> CHTML_param;
typedef CHTMLOpenElementTmpl<&KHTMLTagName_img> CHTML_img_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_a> CHTML_a_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_cite> CHTML_cite;
typedef CHTMLElementTmpl<&KHTMLTagName_code> CHTML_code;
typedef CHTMLElementTmpl<&KHTMLTagName_dfn> CHTML_dfn;
typedef CHTMLElementTmpl<&KHTMLTagName_em> CHTML_em;
typedef CHTMLElementTmpl<&KHTMLTagName_kbd> CHTML_kbd;
typedef CHTMLElementTmpl<&KHTMLTagName_samp> CHTML_samp;
typedef CHTMLElementTmpl<&KHTMLTagName_strike> CHTML_strike;
typedef CHTMLElementTmpl<&KHTMLTagName_strong> CHTML_strong;
typedef CHTMLElementTmpl<&KHTMLTagName_var> CHTML_var;
typedef CHTMLElementTmpl<&KHTMLTagName_b> CHTML_b;
typedef CHTMLElementTmpl<&KHTMLTagName_big> CHTML_big;
typedef CHTMLElementTmpl<&KHTMLTagName_font> CHTML_font_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_i> CHTML_i;
typedef CHTMLElementTmpl<&KHTMLTagName_s> CHTML_s;
typedef CHTMLElementTmpl<&KHTMLTagName_small> CHTML_small;
typedef CHTMLElementTmpl<&KHTMLTagName_sub> CHTML_sub;
typedef CHTMLElementTmpl<&KHTMLTagName_sup> CHTML_sup;
typedef CHTMLElementTmpl<&KHTMLTagName_tt> CHTML_tt;
typedef CHTMLElementTmpl<&KHTMLTagName_u> CHTML_u;
typedef CHTMLElementTmpl<&KHTMLTagName_blink> CHTML_blink;
typedef CHTMLOpenElementTmpl<&KHTMLTagName_br> CHTML_br_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_basefont> CHTML_basefont_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_map> CHTML_map;
typedef CHTMLElementTmpl<&KHTMLTagName_area> CHTML_area;

// the a tag
class CHTML_a : public CHTML_a_Base
{
    typedef CHTML_a_Base CParent;

public:
    CHTML_a(const string& href, const string& text);
    CHTML_a(const string& href, CNCBINode* node);
};

// the table tag
class CHTML_table : public CHTML_table_Base
{
    typedef CHTML_table_Base CParent;

public:
    // type for row and column indexing
    typedef unsigned TIndex;
    // limit of row and column index
    enum { KMaxIndex = 1024 };

    CHTML_table(void);

    // returns row, will add rows if needed
    // throws exception if it is not left upper corner of cell
    CHTML_tr* Row(TIndex row);

    // current insertion point
    void SetCurrentCell(TIndex row, TIndex col)
        { m_CurrentRow = row; m_CurrentCol = col; }
    TIndex GetCurrentRow(void) const
        { return m_CurrentRow; }
    TIndex GetCurrentCol(void) const
        { return m_CurrentCol; }

    enum ECellType {
        eAnyCell,
        eDataCell,
        eHeaderCell
    };

    // returns cell, will add rows/columns if needed
    // throws exception if it is not left upper corner of cell
    // also sets current insertion point
    CHTML_tc* Cell(TIndex row, TIndex column, ECellType type = eAnyCell);
    CHTML_tc* HeaderCell(TIndex row, TIndex column)
        { return Cell(row, column, eHeaderCell); }
    CHTML_tc* DataCell(TIndex row, TIndex column)
        { return Cell(row, column, eDataCell); }

    CHTML_tc* NextCell(ECellType type = eAnyCell);
    CHTML_tc* NextHeaderCell(void)
        { return NextCell(eHeaderCell); }
    CHTML_tc* NextDataCell(void)
        { return NextCell(eDataCell); }

    CHTML_tc* NextRowCell(ECellType type = eAnyCell);
    CHTML_tc* NextRowHeaderCell(void)
        { return NextRowCell(eHeaderCell); }
    CHTML_tc* NextRowDataCell(void)
        { return NextRowCell(eDataCell); }

    // checks table contents for validaty, throws exception if invalid
    void CheckTable(void) const;
    // returns width of table in columns. Should call CheckTable before
    TIndex CalculateNumberOfColumns(void) const;
    TIndex CalculateNumberOfRows(void) const;

    // return cell of insertion
    CHTML_tc* InsertAt(TIndex row, TIndex column, CNCBINode* node);
    CHTML_tc* InsertAt(TIndex row, TIndex column, const string& text);
    CHTML_tc* InsertTextAt(TIndex row, TIndex column, const string& text);

    CHTML_tc* InsertNextCell(CNCBINode* node);
    CHTML_tc* InsertNextCell(const string& text);

    CHTML_tc* InsertNextRowCell(CNCBINode* node);
    CHTML_tc* InsertNextRowCell(const string& text);

    void ColumnWidth(CHTML_table*, TIndex column, const string & width);

    CHTML_table* SetCellSpacing(int spacing);
    CHTML_table* SetCellPadding(int padding);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode = eHTML);

protected:
    TIndex m_CurrentRow, m_CurrentCol;

    virtual CNCBINode* CloneSelf(void) const;

    struct CTableInfo
    {
        TIndex m_Rows;
        TIndex m_Columns;
        TIndex m_FinalRow;
        vector<TIndex> m_FinalRowSpans;
        vector<TIndex> m_RowSizes;
        bool m_BadNode, m_BadRowNode, m_BadCellNode, m_Overlapped, m_BadSpan;

        CTableInfo(void);
        void AddRowSize(TIndex columns);
        void SetFinalRowSpans(TIndex rows, const vector<TIndex>& rowSpans);
    };

    void x_CheckTable(CTableInfo* info) const;
    static TIndex sx_GetSpan(const CNCBINode* node, const string& attr,
                             CTableInfo* info);
    static bool sx_IsRow(const CNCBINode* node);
    static bool sx_IsCell(const CNCBINode* node);
    static bool sx_CanContain(const CNCBINode* node);
    static CHTML_tc* sx_CheckType(CHTMLNode* node, ECellType type);
};

// the form tag
class CHTML_form : public CHTML_form_Base
{
    typedef CHTML_form_Base CParent;

public:
    enum EMethod {
        eGet,
        ePost,
        ePostData
    };

    CHTML_form(const string& url = NcbiEmptyString, EMethod method = eGet);
    CHTML_form(const string& url, CNCBINode* node, EMethod method = eGet);

    void Init(const string& url, EMethod method = eGet);

    void AddHidden(const string& name, const string& value);
    void AddHidden(const string& name, int value);
};

// the textarea tag
class CHTML_textarea : public CHTML_textarea_Base
{
    typedef CHTML_textarea_Base CParent;

public:
    CHTML_textarea(const string& name, int cols, int rows);
    CHTML_textarea(const string& name, int cols, int rows, const string& value);

};

// input tag
class CHTML_input : public CHTML_input_Base
{
    typedef CHTML_input_Base CParent;

public:
    CHTML_input(const string& type, const string& name);
};

// input type=checkbox tag
class CHTML_checkbox : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_checkbox(const string& name);
    CHTML_checkbox(const string& name, bool checked, const string& description = NcbiEmptyString);
    CHTML_checkbox(const string& name, const string& value);
    CHTML_checkbox(const string& name, const string& value, bool checked, const string& description = NcbiEmptyString);


    static const string& s_GetInputType(void);
};

// input type=hidden tag
class CHTML_hidden : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_hidden(const string& name, const string& value);
    CHTML_hidden(const string& name, int value);

    static const string& s_GetInputType(void);
};

// input type=image tag
class CHTML_image : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_image(const string& name, const string& src);
    CHTML_image(const string& name, const string& src, int border);

    static const string& s_GetInputType(void);
};

// input type=radio tag
class CHTML_radio : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_radio(const string& name, const string& value);
    CHTML_radio(const string& name, const string& value, bool checked, const string& description = NcbiEmptyString);

    static const string& s_GetInputType(void);
};

// input type=text tag
class CHTML_reset : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_reset(const string& label = NcbiEmptyString);

    static const string& s_GetInputType(void);
};

// input type=submit tag
class CHTML_submit : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_submit(const string& name);
    CHTML_submit(const string& name, const string& label);

    static const string& s_GetInputType(void);
};

// input type=text tag
class CHTML_text : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_text(const string& name, const string& value = NcbiEmptyString);
    CHTML_text(const string& name, int size, const string& value = NcbiEmptyString);
    CHTML_text(const string& name, int size, int maxlength, const string& value = NcbiEmptyString);

    static const string& s_GetInputType(void);
};

// input type=file tag
class CHTML_file : public CHTML_input
{
    typedef CHTML_input CParent;

public:
    CHTML_file(const string& name, const string& value = NcbiEmptyString);

    static const string& s_GetInputType(void);
};

// select tag
class CHTML_select : public CHTML_select_Base
{
    typedef CHTML_select_Base CParent;

public:
    CHTML_select(const string& name, bool multiple = false);
    CHTML_select(const string& name, int size, bool multiple = false);

    // return 'this' to allow chained AppendOption
    CHTML_select* AppendOption(const string& value, bool selected = false);
    CHTML_select* AppendOption(const string& value, const string& label,
                               bool selected = false);
};

//option tag.  rarely used alone.  see select tag
class CHTML_option: public CHTML_option_Base
{
    typedef CHTML_option_Base CParent;

public:
    CHTML_option(const string& value, bool selected = false);
    CHTML_option(const string& value, const string& label,
                 bool selected = false);
};

// break
class CHTML_br : public CHTML_br_Base
{
    typedef CHTML_br_Base CParent;

public:
    CHTML_br(void);
    // create <number> of <br> tags
    CHTML_br(int number);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode = eHTML);
};


class CHTML_img : public CHTML_img_Base
{
    typedef CHTML_img_Base CParent;

public:
    CHTML_img(const string& url);
    CHTML_img(const string& url, int width, int height);
};

// dl tag
class CHTML_dl : public CHTML_dl_Base
{
    typedef CHTML_dl_Base CParent;

public:
    CHTML_dl(bool compact = false);

    // return 'this' to allow chained AppendTerm
    CHTML_dl* AppendTerm(const string& term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(const string& term, const string& definition);
    CHTML_dl* AppendTerm(CNCBINode* term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(CNCBINode* term, const string& definition);

};

class CHTML_ol : public CHTML_ol_Base
{
    typedef CHTML_ol_Base CParent;

public:
    CHTML_ol(bool compact = false);
    CHTML_ol(const string& type, bool compact = false);
    CHTML_ol(int start, bool compact = false);
    CHTML_ol(int start, const string& type, bool compact = false);
};

class CHTML_font : public CHTML_font_Base
{
    typedef CHTML_font_Base CParent;

public:
    CHTML_font(void);
    CHTML_font(int size, CNCBINode* node = 0);
    CHTML_font(int size, const string& text);
    CHTML_font(int size, bool absolute, CNCBINode* node = 0);
    CHTML_font(int size, bool absolute, const string& text);
    CHTML_font(const string& typeface, CNCBINode* node = 0);
    CHTML_font(const string& typeface, const string& text);
    CHTML_font(const string& typeface, int size, CNCBINode* node = 0);
    CHTML_font(const string& typeface, int size, const string& text);
    CHTML_font(const string& typeface, int size, bool absolute, CNCBINode* node = 0);
    CHTML_font(const string& typeface, int size, bool absolute, const string& text);

    CHTML_font* SetFontSize(int size, bool absolute);
    CHTML_font* SetRelativeSize(int size);
};

class CHTML_basefont : public CHTML_basefont_Base
{
    typedef CHTML_basefont_Base CParent;

public:
    CHTML_basefont(int size);
    CHTML_basefont(const string& typeface);
    CHTML_basefont(const string& typeface, int size);
};

class CHTML_color : public CHTML_font
{
    typedef CHTML_font CParent;

public:
    CHTML_color(const string& color, CNCBINode* node = 0);
    CHTML_color(const string& color, const string& text);
};

class CHTML_hr : public CHTML_hr_Base
{
    typedef CHTML_hr_Base CParent;
public:
    CHTML_hr(bool noShade = false);
    CHTML_hr(int size, bool noShade = false);
    CHTML_hr(int size, int width, bool noShade = false);
    CHTML_hr(int size, const string& width, bool noShade = false);

    CHTML_hr* SetNoShade(void);
    CHTML_hr* SetNoShade(bool noShade);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode = eHTML);
};

#include <html/html.inl>

END_NCBI_SCOPE

#endif
