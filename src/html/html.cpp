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
*   code for CHTMLNode
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  1999/01/21 21:12:59  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* Revision 1.24  1999/01/21 16:18:05  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.23  1999/01/14 21:25:20  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.22  1999/01/11 22:05:52  vasilche
* Fixed CHTML_font size.
* Added CHTML_image input element.
*
* Revision 1.21  1999/01/11 15:13:35  vasilche
* Fixed CHTML_font size.
* CHTMLHelper extracted to separate file.
*
* Revision 1.20  1999/01/07 16:41:56  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* Revision 1.19  1999/01/05 20:23:29  vasilche
* Fixed HTMLEncode.
*
* Revision 1.18  1999/01/04 20:06:14  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.16  1998/12/28 21:48:16  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.15  1998/12/28 16:48:09  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.14  1998/12/24 16:15:41  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.13  1998/12/23 21:51:44  vasilche
* Added missing constructors to checkbox.
*
* Revision 1.12  1998/12/23 21:21:03  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.11  1998/12/23 14:28:10  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.10  1998/12/21 22:25:03  vasilche
* A lot of cleaning.
*
* Revision 1.9  1998/12/10 19:21:51  lewisg
* correct error handling in InsertInTable
*
* Revision 1.8  1998/12/10 00:17:27  lewisg
* fix index in InsertInTable
*
* Revision 1.7  1998/12/09 23:00:54  lewisg
* use new cgiapp class
*
* Revision 1.6  1998/12/08 00:33:43  lewisg
* cleanup
*
* Revision 1.5  1998/12/03 22:48:00  lewisg
* added HTMLEncode() and CHTML_img
*
* Revision 1.4  1998/12/01 19:10:38  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.3  1998/11/23 23:42:17  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:13:06  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:36:05  lewisg
* new html lib and test program
*
* ===========================================================================
*/


#include <html/html.hpp>
#include <html/htmlhelper.hpp>

BEGIN_NCBI_SCOPE

// HTML element names
const string KHTMLTagName_html = "HTML";
const string KHTMLTagName_head = "HEAD";
const string KHTMLTagName_body = "BODY";
const string KHTMLTagName_base = "BASE";
const string KHTMLTagName_isindex = "ISINDEX";
const string KHTMLTagName_link = "LINK";
const string KHTMLTagName_meta = "META";
const string KHTMLTagName_script = "SCRIPT";
const string KHTMLTagName_style = "STYLE";
const string KHTMLTagName_title = "TITLE";
const string KHTMLTagName_address = "ADDRESS";
const string KHTMLTagName_blockquote = "BLOCKQUOTE";
const string KHTMLTagName_center = "CENTER";
const string KHTMLTagName_div = "DIV";
const string KHTMLTagName_h1 = "H1";
const string KHTMLTagName_h2 = "H2";
const string KHTMLTagName_h3 = "H3";
const string KHTMLTagName_h4 = "H4";
const string KHTMLTagName_h5 = "H5";
const string KHTMLTagName_h6 = "H6";
const string KHTMLTagName_hr = "HR";
const string KHTMLTagName_p = "P";
const string KHTMLTagName_pre = "PRE";
const string KHTMLTagName_form = "FORM";
const string KHTMLTagName_input = "INPUT";
const string KHTMLTagName_select = "SELECT";
const string KHTMLTagName_option = "OPTION";
const string KHTMLTagName_textarea = "TEXTAREA";
const string KHTMLTagName_dl = "DL";
const string KHTMLTagName_dt = "DT";
const string KHTMLTagName_dd = "DD";
const string KHTMLTagName_ol = "OL";
const string KHTMLTagName_ul = "UL";
const string KHTMLTagName_dir = "DIR";
const string KHTMLTagName_menu = "MENU";
const string KHTMLTagName_li = "LI";
const string KHTMLTagName_table = "TABLE";
const string KHTMLTagName_caption = "CAPTION";
const string KHTMLTagName_col = "COL";
const string KHTMLTagName_colgroup = "COLGROUP";
const string KHTMLTagName_thead = "THEAD";
const string KHTMLTagName_tbody = "TBODY";
const string KHTMLTagName_tfoot = "TFOOT";
const string KHTMLTagName_tr = "TR";
const string KHTMLTagName_th = "TH";
const string KHTMLTagName_td = "TD";
const string KHTMLTagName_applet = "APPLET";
const string KHTMLTagName_param = "PARAM";
const string KHTMLTagName_img = "IMG";
const string KHTMLTagName_a = "A";
const string KHTMLTagName_cite = "CITE";
const string KHTMLTagName_code = "CODE";
const string KHTMLTagName_dfn = "DFN";
const string KHTMLTagName_em = "EM";
const string KHTMLTagName_kbd = "KBD";
const string KHTMLTagName_samp = "SAMP";
const string KHTMLTagName_strike = "STRIKE";
const string KHTMLTagName_strong = "STRONG";
const string KHTMLTagName_var = "VAR";
const string KHTMLTagName_b = "B";
const string KHTMLTagName_big = "BIG";
const string KHTMLTagName_font = "FONT";
const string KHTMLTagName_i = "I";
const string KHTMLTagName_s = "S";
const string KHTMLTagName_small = "SMALL";
const string KHTMLTagName_sub = "SUB";
const string KHTMLTagName_sup = "SUP";
const string KHTMLTagName_tt = "TT";
const string KHTMLTagName_u = "U";
const string KHTMLTagName_blink = "BLINK";
const string KHTMLTagName_br = "BR";
const string KHTMLTagName_basefont = "BASEFONT";
const string KHTMLTagName_map = "MAP";
const string KHTMLTagName_area = "AREA";


// HTML attribute names
const string KHTMLAttributeName_action = "ACTION";
const string KHTMLAttributeName_align = "ALIGN";
const string KHTMLAttributeName_bgcolor = "BGCOLOR";
const string KHTMLAttributeName_border = "BORDER";
const string KHTMLAttributeName_cellpadding = "CELLPADDING";
const string KHTMLAttributeName_cellspacing = "CELLSPACING";
const string KHTMLAttributeName_checked = "CHECKED";
const string KHTMLAttributeName_color = "COLOR";
const string KHTMLAttributeName_cols = "COLS";
const string KHTMLAttributeName_colspan = "COLSPAN";
const string KHTMLAttributeName_compact = "COMPACT";
const string KHTMLAttributeName_enctype = "ENCTYPE";
const string KHTMLAttributeName_face = "FACE";
const string KHTMLAttributeName_height = "HEIGHT";
const string KHTMLAttributeName_href = "HREF";
const string KHTMLAttributeName_maxlength = "MAXLENGTH";
const string KHTMLAttributeName_method = "METHOD";
const string KHTMLAttributeName_multiple = "MULTIPLE";
const string KHTMLAttributeName_name = "NAME";
const string KHTMLAttributeName_rows = "ROWS";
const string KHTMLAttributeName_rowspan = "ROWSPAN";
const string KHTMLAttributeName_selected = "SELECTED";
const string KHTMLAttributeName_size = "SIZE";
const string KHTMLAttributeName_src = "SRC";
const string KHTMLAttributeName_start = "START";
const string KHTMLAttributeName_type = "TYPE";
const string KHTMLAttributeName_value = "VALUE";
const string KHTMLAttributeName_width = "WIDTH";


// CHTMLNode

void CHTMLNode::AppendPlainText(const string& appendstring)
{
    if ( !appendstring.empty() )
        AppendChild(new CHTMLPlainText(appendstring));
}

void CHTMLNode::AppendHTMLText(const string& appendstring)
{
    if ( !appendstring.empty() )
        AppendChild(new CHTMLText(appendstring));
}

// <@XXX@> mapping tag node

CHTMLTagNode::CHTMLTagNode(const string& name)
    : CParent(name)
{
}

CNCBINode* CHTMLTagNode::CloneSelf(void) const
{
    return new CHTMLTagNode(*this);
}

void CHTMLTagNode::CreateSubNodes(void)
{
    AppendChild(MapTag(GetName()));
}

// plain text node

CHTMLPlainText::CHTMLPlainText(const string& text)
    : m_Text(text)
{
}

CNCBINode* CHTMLPlainText::CloneSelf() const
{
    return new CHTMLPlainText(*this);
}

void CHTMLPlainText::SetText(const string& text)
{
    m_Text = text;
}

CNcbiOstream& CHTMLPlainText::PrintBegin(CNcbiOstream& out)  
{
    return out << CHTMLHelper::HTMLEncode(m_Text);
}

// text node

CHTMLText::CHTMLText(const string& text)
    : m_Text(text)
{
}

CNCBINode* CHTMLText::CloneSelf() const
{
    return new CHTMLText(*this);
}

static const string KTagStart = "<@";
static const string KTagEnd = "@>";

CNcbiOstream& CHTMLText::PrintBegin(CNcbiOstream& out)  
{
    return out << m_Text;
}

// text node

void CHTMLText::CreateSubNodes()
{
    SIZE_TYPE tagStart = m_Text.find(KTagStart);
    if ( tagStart != NPOS ) {
        string text = m_Text.substr(tagStart);
        m_Text = m_Text.substr(0, tagStart);
        SIZE_TYPE last = 0;
        tagStart = 0;
        do {
            SIZE_TYPE tagNameStart = tagStart + KTagStart.size();
            SIZE_TYPE tagNameEnd = text.find(KTagEnd, tagNameStart);
            if ( tagNameEnd != NPOS ) {
                // tag found
                if ( last != tagStart ) {
                    AppendChild(new CHTMLText(text.substr(last, tagStart - last)));
                }

                AppendChild(new CHTMLTagNode(text.substr(tagNameStart, tagNameEnd - tagNameStart)));
                last = tagNameEnd + KTagEnd.size();
                tagStart = text.find(KTagStart, last);
            }
            else {
                // tag not closed
                throw runtime_error("tag not closed");
            }
        } while ( tagStart != NPOS );

        if ( last != text.size() ) {
            AppendChild(new CHTMLText(text.substr(last)));
        }
    }
}

// tag node

CHTMLOpenElement::CHTMLOpenElement(const string& name)
    : CParent(name)
{
}

CHTMLOpenElement::CHTMLOpenElement(const string& name, CNCBINode* node)
    : CParent(name)
{
    AppendChild(node);
}

CHTMLOpenElement::CHTMLOpenElement(const string& name, const string& text)
    : CParent(name)
{
    AppendHTMLText(text);
}

CNCBINode* CHTMLOpenElement::CloneSelf(void) const
{
    return new CHTMLOpenElement(*this);
}

CNcbiOstream& CHTMLOpenElement::PrintBegin(CNcbiOstream& out)
{
    out << '<' << m_Name;
    for (TAttributes::iterator i = m_Attributes.begin(); i != m_Attributes.end(); ++i ) {
        out << ' ' << i->first;
        if ( !i->second.empty() ) {
            out << "=\"" << CHTMLHelper::HTMLEncode(i->second) << '"';
        }
    }
    return out << '>';
}

CHTMLElement::CHTMLElement(const string& name)
    : CParent(name)
{
}

CHTMLElement::CHTMLElement(const string& name, CNCBINode* node)
    : CParent(name)
{
    AppendChild(node);
}

CHTMLElement::CHTMLElement(const string& name, const string& text)
    : CParent(name)
{
    AppendHTMLText(text);
}

CNCBINode* CHTMLElement::CloneSelf(void) const
{
    return new CHTMLElement(*this);
}

CNcbiOstream& CHTMLElement::PrintEnd(CNcbiOstream& out)
{
    return out << "</" << m_Name << ">\n";
}

// HTML comment class
CHTMLComment::CHTMLComment(void)
{
}

CHTMLComment::CHTMLComment(CNCBINode* node)
{
    AppendChild(node);
}

CHTMLComment::CHTMLComment(const string& text)
{
    AppendPlainText(text);
}

CNCBINode* CHTMLComment::CloneSelf(void) const
{
    return new CHTMLComment(*this);
}

CNcbiOstream& CHTMLComment::PrintBegin(CNcbiOstream& out)
{
    return out << "<!--";
}

CNcbiOstream& CHTMLComment::PrintEnd(CNcbiOstream& out)
{
    return out << "-->";
}

// TABLE element

CHTML_table::CHTML_table(void)
{
}

CNCBINode* CHTML_table::CloneSelf(void) const
{
    return new CHTML_table(*this);
}

CHTML_table* CHTML_table::SetCellSpacing(int spacing)
{
    SetAttribute(KHTMLAttributeName_cellspacing, spacing);
    return this;
}

CHTML_table* CHTML_table::SetCellPadding(int padding)
{
    SetAttribute(KHTMLAttributeName_cellpadding, padding);
    return this;
}

int CHTML_table::sx_GetSpan(const CNCBINode* node,
                            const string& attributeName, CTableInfo* info)
{
    if ( !node->HaveAttribute(attributeName) )
        return 1;

    int span;
    try {
        span = NStr::StringToInt(node->GetAttribute(attributeName));
    }
    catch ( runtime_error err ) {
        if ( info )
            info->m_BadSpan = true;
        return 1;
    }
    if ( span <= 0 ) {
        if ( info )
            info->m_BadSpan = true;
        return 1;
    }
    return span;
}

CHTMLNode* CHTML_table::Cell(int needRow, int needCol)  // todo: exception
{
    vector<int> rowSpans; // hanging down cells (with rowspan > 1)

    // beginning with row 0
    int row = 0;
    int needRowCols = 0;
    CHTMLNode* needRowNode = 0;
    // scan all children (which should be <TR> tags)
    for ( TChildList::const_iterator iRow = ChildBegin();
          iRow != ChildEnd(); ++iRow ) {
        CHTMLNode* trNode = dynamic_cast<CHTMLNode*>(*iRow);
        if ( !trNode || !sx_IsRow(trNode) ) {
            throw runtime_error("Table contains non <TR> tag");
        }

        // beginning with column 0
        int col = 0;
        // scan all children (which should be <TH> or <TD> tags)
        for ( TChildList::iterator iCol = trNode->ChildBegin();
              iCol != trNode->ChildEnd(); ++iCol ) {
            CHTMLNode* cell = dynamic_cast<CHTMLNode*>(*iCol);

            if ( !cell || !sx_IsCell(cell) ) {
                throw runtime_error("Table row contains non <TH> or <TD> tag");
            }

            // skip all used cells
            while ( col < rowSpans.size() && rowSpans[col] > 0 ) {
                --rowSpans[col++];
            }

            if ( row == needRow ) {
                if ( col == needCol )
                    return cell;
                if ( col > needCol )
                    throw runtime_error("Table cells are overlapped");
            }

            // determine current cell size
            int rowSpan = sx_GetSpan(cell, KHTMLAttributeName_rowspan, 0);
            int colSpan = sx_GetSpan(cell, KHTMLAttributeName_colspan, 0);

            // end of new cell in columns
            const int colEnd = col + colSpan;
            // check that there is enough space
            for ( int i = col; i < colEnd && i < rowSpans.size(); ++i ) {
                if ( rowSpans[i] ) {
                    throw runtime_error("Table cells are overlapped");
                }
            }

            if ( rowSpan > 1 ) {
                // we should remember space used by this cell
                // first expand rowSpans vector if needed
                if ( rowSpans.size() < colEnd )
                    rowSpans.resize(colEnd);

                // then store span number
                for ( int i = col; i < colEnd; ++i ) {
                    rowSpans[i] = max(rowSpans[i], rowSpan - 1);
                }
            }
            // skip this cell's columns
            col += colSpan;
        }

        ++row;

        if ( row > needRow ) {
            needRowCols = col;
            needRowNode = trNode;
            break;
        }
    }

    while ( row <= needRow ) {
        // add needed rows
        AppendChild(needRowNode = new CHTML_tr);
        ++row;
        // decrement hanging cell sizes
        for ( int i = 0; i < rowSpans.size(); ++i ) {
            if ( rowSpans[i] )
                --rowSpans[i];
        }
    }

    // this row doesn't have enough columns -> add some
    int addCells = 0;

    do {
        // skip hanging cells
        while ( needRowCols < rowSpans.size() && rowSpans[needRowCols] > 0 ) {
            if ( needRowCols == needCol )
                throw runtime_error("Table cells are overlapped");
            ++needRowCols;
        }
        // allocate one column cell
        ++addCells;
        ++needRowCols;
    } while ( needRowCols <= needCol );

    // add needed columns
    CHTMLNode* cell;
    for ( int i = 0; i < addCells; ++i ) {
        needRowNode->AppendChild(cell = new CHTML_td);
    }
    return cell;
}

bool CHTML_table::sx_IsRow(const CNCBINode* node)
{
    return node->GetName() == CHTML_tr::s_GetTagName();
}

bool CHTML_table::sx_IsCell(const CNCBINode* node)
{
    return node->GetName() == CHTML_td::s_GetTagName() ||
        node->GetName() == CHTML_th::s_GetTagName();
}

void CHTML_table::x_CheckTable(CTableInfo *info) const
{
    vector<int> rowSpans; // hanging down cells (with rowspan > 1)

    // beginning with row 0
    int row = 0;
    // scan all children (which should be <TR> tags)
    for ( TChildList::const_iterator iRow = ChildBegin(); iRow != ChildEnd();
          ++iRow ) {
        CNCBINode* trNode = *iRow;

        if ( !sx_IsRow(trNode) ) {
            if ( info ) {
                // we just gathering info -> skip wrong tag
                info->m_BadNode = info->m_BadRowNode = true;
                continue;
            }
            throw runtime_error("Table contains non <TR> tag");
        }

        // beginning with column 0
        int col = 0;
        // scan all children (which should be <TH> or <TD> tags)
        for ( TChildList::iterator iCol = trNode->ChildBegin();
              iCol != trNode->ChildEnd(); ++iCol ) {
            CNCBINode* cell = *iCol;

            if ( !sx_IsCell(cell) ) {
                if ( info ) {
                    // we just gathering info -> skip wrong tag
                    info->m_BadNode = info->m_BadCellNode = true;
                    continue;
                }
                throw runtime_error("Table row contains non <TH> or <TD> tag");
            }

            // skip all used cells
            while ( col < rowSpans.size() && rowSpans[col] > 0 ) {
                --rowSpans[col++];
            }

            // determine current cell size
            int rowSpan = sx_GetSpan(cell, KHTMLAttributeName_rowspan, info);
            int colSpan = sx_GetSpan(cell, KHTMLAttributeName_colspan, info);

            // end of new cell in columns
            const int colEnd = col + colSpan;
            // check that there is enough space
            for ( int i = col; i < colEnd && i < rowSpans.size(); ++i ) {
                if ( rowSpans[i] ) {
                    if ( info )
                        info->m_Overlapped = true;
                    else
                        throw runtime_error("Table cells are overlapped");
                }
            }

            if ( rowSpan > 1 ) {
                // we should remember space used by this cell
                // first expand rowSpans vector if needed
                if ( rowSpans.size() < colEnd )
                    rowSpans.resize(colEnd);

                // then store span number
                for ( int i = col; i < colEnd; ++i ) {
                    rowSpans[i] = max(rowSpans[i], rowSpan - 1);
                }
            }
            // skip this cell's columns
            col += colSpan;
        }
        if ( info )
            info->AddRowSize(col);
        ++row;
    }

    if ( info )
        info->SetFinalRowSpans(row, rowSpans);
}

CHTML_table::CTableInfo::CTableInfo(void)
    : m_Columns(0), m_Rows(0), m_FinalRow(0),
      m_BadNode(false), m_BadRowNode(false), m_BadCellNode(false),
      m_Overlapped(false), m_BadSpan(false)
{
}

void CHTML_table::CTableInfo::AddRowSize(int columns)
{
    m_RowSizes.push_back(columns);
    m_Columns = max(m_Columns, columns);
}

void CHTML_table::CTableInfo::SetFinalRowSpans(int row, const vector<int>& rowSpans)
{
    m_FinalRow = row;
    m_FinalRowSpans = rowSpans;

    // check for the rest of rowSpans
    int addRows = 0;
    for ( int i = 0; i < rowSpans.size(); ++i ) {
        addRows = max(addRows, rowSpans[i]);
    }
    m_Rows = row + addRows;
}

int CHTML_table::CalculateNumberOfColumns(void) const
{
    CTableInfo info;
    x_CheckTable(&info);
    return info.m_Columns;
}

int CHTML_table::CalculateNumberOfRows(void) const
{
    CTableInfo info;
    x_CheckTable(&info);
    return info.m_Rows;
}

CNcbiOstream& CHTML_table::PrintChildren(CNcbiOstream& out)
{
    CTableInfo info;
    x_CheckTable(&info);

    int row = 0;
    for ( TChildList::iterator iRow = ChildBegin();
          iRow != ChildEnd(); ++iRow ) {
        CNCBINode* rowNode = *iRow;
        if ( !sx_IsRow(rowNode) )
            rowNode->Print(out);
        else {
            rowNode->PrintBegin(out);
            rowNode->PrintChildren(out);
            // determine additional cells to print
            int addCells = info.m_Columns - info.m_RowSizes[row];
            // print them
            for ( int i = 0; i < addCells; ++i )
                out << "<TD></TD>";
            rowNode->PrintEnd(out);
            ++row;
        }
    }

    // print implicit rows
    for ( int i = info.m_FinalRow; i < info.m_Rows; ++i )
        out << "<TR></TR>";

    return out;
}

// form element

CHTML_form::CHTML_form (const string& action, const string& method, const string& enctype)
{
    SetOptionalAttribute(KHTMLAttributeName_action, action);
    SetOptionalAttribute(KHTMLAttributeName_method, method);
    SetOptionalAttribute(KHTMLAttributeName_enctype, enctype);
}

void CHTML_form::AddHidden(const string& name, const string& value)
{
    AppendChild(new CHTML_hidden(name, value));
}

// textarea element

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows)
{
    SetAttribute(KHTMLAttributeName_name, name);
    SetAttribute(KHTMLAttributeName_cols, cols);
    SetAttribute(KHTMLAttributeName_rows, rows);
}

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows, const string& value)
    : CParent(value)
{
    SetAttribute(KHTMLAttributeName_name, name);
    SetAttribute(KHTMLAttributeName_cols, cols);
    SetAttribute(KHTMLAttributeName_rows, rows);
}


//input tag

CHTML_input::CHTML_input(const string& type, const string& name)
{
    SetAttribute(KHTMLAttributeName_type, type);
    SetOptionalAttribute(KHTMLAttributeName_name, name);
};


// HTML input type names
const string KHTMLInputTypeName_text = "TEXT";
const string KHTMLInputTypeName_radio = "RADIO";
const string KHTMLInputTypeName_checkbox = "CHECKBOX";
const string KHTMLInputTypeName_hidden = "HIDDEN";
const string KHTMLInputTypeName_image = "IMAGE";
const string KHTMLInputTypeName_submit = "SUBMIT";
const string KHTMLInputTypeName_reset = "RESET";

const string& CHTML_text::s_GetInputType(void)
{
    return KHTMLInputTypeName_text;
}

const string& CHTML_radio::s_GetInputType(void)
{
    return KHTMLInputTypeName_radio;
}

const string& CHTML_checkbox::s_GetInputType(void)
{
    return KHTMLInputTypeName_checkbox;
}

const string& CHTML_hidden::s_GetInputType(void)
{
    return KHTMLInputTypeName_hidden;
}

const string& CHTML_image::s_GetInputType(void)
{
    return KHTMLInputTypeName_image;
}

const string& CHTML_submit::s_GetInputType(void)
{
    return KHTMLInputTypeName_submit;
}

const string& CHTML_reset::s_GetInputType(void)
{
    return KHTMLInputTypeName_reset;
}

// checkbox tag 

CHTML_checkbox::CHTML_checkbox(const string& name)
    : CParent(s_GetInputType(), name)
{
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_checkbox::CHTML_checkbox(const string& name, bool checked, const string& description)
    : CParent(s_GetInputType(), name)
{
    SetOptionalAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value, bool checked, const string& description)
    : CParent(s_GetInputType(), name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
    SetOptionalAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}


// image tag 

CHTML_image::CHTML_image(const string& name, const string& src)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_src, src);
}

CHTML_image::CHTML_image(const string& name, const string& src, int border)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_src, src);
    SetAttribute(KHTMLAttributeName_border, border);
}

// radio tag 

CHTML_radio::CHTML_radio(const string& name, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_value, value);
}

CHTML_radio::CHTML_radio(const string& name, const string& value, bool checked, const string& description)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_value, value);
    SetAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}

// hidden tag 

CHTML_hidden::CHTML_hidden(const string& name, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_value, value);
}

CHTML_hidden::CHTML_hidden(const string& name, int value)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_value, value);
}

CHTML_submit::CHTML_submit(const string& name)
    : CParent(s_GetInputType(), NcbiEmptyString)
{
    SetOptionalAttribute(KHTMLAttributeName_value, name);
}

CHTML_submit::CHTML_submit(const string& name, const string& label)
    : CParent(s_GetInputType(), name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, label);
}

CHTML_reset::CHTML_reset(const string& label)
    : CParent(s_GetInputType(), NcbiEmptyString)
{
    SetOptionalAttribute(KHTMLAttributeName_value, label);
}

// text tag 

CHTML_text::CHTML_text(const string& name, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_text::CHTML_text(const string& name, int size, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_size, size);
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_text::CHTML_text(const string& name, int size, int maxlength, const string& value)
    : CParent(s_GetInputType(), name)
{
    SetAttribute(KHTMLAttributeName_size, size);
    SetAttribute(KHTMLAttributeName_maxlength, maxlength);
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_br::CHTML_br(int count)
{
    for ( int i = 1; i < count; ++i )
        AppendChild(new CHTML_br());
}

// img tag

CHTML_img::CHTML_img(const string& url)
{
    SetAttribute(KHTMLAttributeName_src, url);
}

CHTML_img::CHTML_img(const string& url, int width, int height)
{
    SetAttribute(KHTMLAttributeName_src, url);
    SetWidth(width);
    SetHeight(height);
}

// dl tag

CHTML_dl* CHTML_dl::AppendTerm(const string& term, const string& definition)
{
    AppendChild(new CHTML_dt(term));
    if ( !definition.empty() )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(const string& term, CNCBINode* definition)
{
    AppendChild(new CHTML_dt(term));
    if ( definition )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(CNCBINode* term, const string& definition)
{
    AppendChild(new CHTML_dt(term));
    if ( !definition.empty() )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(CNCBINode* term, CNCBINode* definition)
{
    AppendChild(new CHTML_dt(term));
    if ( definition )
        AppendChild(new CHTML_dd(definition));
    return this;
}

void CHTML_font::SetRelativeSize(int size)
{
    if ( size != 0 )
        SetAttribute(KHTMLAttributeName_size, NStr::IntToString(size, true));
}

END_NCBI_SCOPE
