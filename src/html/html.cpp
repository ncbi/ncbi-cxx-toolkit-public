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


#include <ncbistd.hpp>
#include <stl.hpp>
#include <html.hpp>
#include <string>
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
const string KHTMLAttributeName_cellpadding = "CELLPADDING";
const string KHTMLAttributeName_cellspacing = "CELLSPACING";
const string KHTMLAttributeName_checked = "CHECKED";
const string KHTMLAttributeName_color = "COLOR";
const string KHTMLAttributeName_cols = "COLS";
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
const string KHTMLAttributeName_selected = "SELECTED";
const string KHTMLAttributeName_size = "SIZE";
const string KHTMLAttributeName_src = "SRC";
const string KHTMLAttributeName_start = "START";
const string KHTMLAttributeName_type = "TYPE";
const string KHTMLAttributeName_value = "VALUE";
const string KHTMLAttributeName_width = "WIDTH";


string CHTMLHelper::HTMLEncode(const string& input)
{
    string output;
    string::size_type last = 0;

    // find first symbol to encode
    string::size_type ptr = input.find_first_of("\"&<>", last);
    while ( ptr != string::npos ) {
        // copy plain part of input
        if ( ptr != last )
            output.append(input, last, ptr);

        // append encoded symbol
        switch ( input[ptr] ) {
        case '"':
            output.append("&quot;");
            break;
        case '&':
            output.append("&amp;");
            break;
        case '<':
            output.append("&lt;");
            break;
        case '>':
            output.append("&gt;");
            break;
        }

        // skip it
        last = ptr + 1;

        // find next symbol to encode
        ptr = input.find_first_of("\"&<>", last);
    }

    // append last part of input
    if ( last != input.size() )
        output.append(input, last, input.size());

    return output;
}

/*
NcbiOstream& CHTMLHelper::PrintEncoded(NcbiOstream& out, const string& input)
{
    SIZE_TYPE last = 0;

    // find first symbol to encode
    SIZE_TYPE ptr = input.find_first_of("\"&<>", last);
    while ( ptr != NPOS ) {
        // copy plain part of input
        if ( ptr != last )
            out << input.substr(last, ptr - last);

        // append encoded symbol
        switch ( input[ptr] ) {
        case '"':
            out << "&quot;";
            break;
        case '&':
            out << "&amp;";
            break;
        case '<':
            out << "&lt;";
            break;
        case '>':
            out << "&gt;";
            break;
        }

        // skip it
        last = ptr + 1;

        // find next symbol to encode
        ptr = input.find_first_of("\"&<>", last);
    }

    // append last part of input
    if ( last != input.size() )
        out << input.substr(last);

    return out;
}
*/

CHTMLNode::CHTMLNode(const string& name)
    : CParent(name)
{
}

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

// TABLE element

CHTML_table::CHTML_table(void)
{
}


CHTML_table::CHTML_table(const string& bgcolor)
{
     SetBgColor(bgcolor);
}


CHTML_table::CHTML_table(const string& bgcolor, const string& width)
{
    SetBgColor(bgcolor);
    SetWidth(width);
}


// instantiates table rows and cells
void CHTML_table::MakeTable(int rows, int columns)  // throw(bad_alloc)
{
    for(int row = 0; row < rows; row++) {
        CHTML_tr* tr = new CHTML_tr;
        AppendChild(tr);
        for (int column = 0; column < columns; column++) {
            tr->AppendChild(new CHTML_td);
        }
    }
}


// contructors that also instantiate rows and cells
CHTML_table::CHTML_table(int row, int column)
{
     MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, int row, int column)
{
    SetBgColor(bgcolor);
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, int row, int column)
{
    SetBgColor(bgcolor);
    SetWidth(width);
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, const string & cellspacing, const string & cellpadding, int row, int column)
{
    SetBgColor(bgcolor);
    SetWidth(width);
    SetOptionalAttribute(KHTMLAttributeName_cellspacing, cellspacing);
    SetOptionalAttribute(KHTMLAttributeName_cellpadding, cellpadding);
    MakeTable(row, column);
}

/*
CNCBINode* CHTML_table::Cell(int row, int column)  // todo: exception
{
    return Column(Row(row), column);
}

// returns row in table, adding rows if nesessary
CNCBINode* CHTML_table::Row(int needRow)
{
    int row = -1; // we didn't scan any row yet
    // scan all existing rows
    for ( TChildList::iterator i = ChildBegin(); i != ChildEnd(); ++i ) {
        // if it's real row
        if ( (*i)->GetName() == "tr" ) {
            // if we row number is ok
            if ( row > needRow ) {
                // this is the result
                return *i;
            }
            row++;
        }
    }

    // we don't have enough rows in our table: allocate some more
    CHTML_tr* tr;
    while ( row < needRow ) {
        tr = new CHTML_tr;
        AppendChild(tr);
        row++;
    }
    return tr;
}

*/

CNCBINode* CHTML_table::InsertInTable(int row, int column, CNCBINode * Child)  // todo: exception
{
    int iRow, iColumn;
    list <CNCBINode *>::iterator iChildren;
    list <CNCBINode *>::iterator iRowChildren;

    if(!Child) return NULL;
    iRow = 0;
    iChildren = m_ChildNodes.begin();

    while ( iRow <= row && iChildren != m_ChildNodes.end() ) {
        if ( (*iChildren)->GetName() == KHTMLTagName_tr )
            iRow++;
        iChildren++;
    }

    if( iChildren == m_ChildNodes.end() && iRow <= row)
        return NULL;
    iChildren--;

    iColumn = 0;
    iRowChildren = (*iChildren)->ChildBegin();

    while ( iColumn <= column && iRowChildren != (*iChildren)->ChildEnd() ) {
        if ( (*iRowChildren)->GetName() == KHTMLTagName_td )
            iColumn++;
        iRowChildren++;
    }

    if ( iRowChildren == (*iChildren)->ChildEnd() && iColumn <= column )
        return NULL;
    else {
        iRowChildren--;
        return (*iRowChildren)->AppendChild(Child);
    }
}

CNCBINode * CHTML_table::InsertTextInTable(int row, int column, const string & appendstring) // throw(bad_alloc)
{
    CHTMLText * text;
    try {
        text = new CHTMLText(appendstring);
    }
    catch (.../*bad_alloc &e*/) {
        delete text;
        throw;
    }
    return InsertInTable(row, column, text);
}


void CHTML_table::ColumnWidth(CHTML_table * table, int column, const string & width)
{
    int ix, iy;
    list <CNCBINode *>::iterator iChildren;
    list <CNCBINode *>::iterator iyChildren;

    iy = 1;
    iChildren = m_ChildNodes.begin();
    
    while (iChildren != m_ChildNodes.end()) {
        if ( (*iChildren)->GetName() == KHTMLTagName_tr ) {
            ix = 1;
            iyChildren = (*iChildren)->ChildBegin();
            
            while ( ix < column && iyChildren != (*iChildren)->ChildEnd() ) {
                if ( (*iyChildren)->GetName() == KHTMLTagName_td )
                    ix++;  
                iyChildren++;
            }
            if ( iyChildren != (*iChildren)->ChildEnd() ) {
                (*iyChildren)->SetAttribute(KHTMLAttributeName_width, width);
            }
        }
        iChildren++;
    }    
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


// checkbox tag 

CHTML_checkbox::CHTML_checkbox(const string& name)
    : CParent("CHECKBOX", name)
{
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value)
    : CParent("CHECKBOX", name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_checkbox::CHTML_checkbox(const string& name, bool checked, const string& description)
    : CParent("CHECKBOX", name)
{
    SetOptionalAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value, bool checked, const string& description)
    : CParent("CHECKBOX", name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
    SetOptionalAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}


// radio tag 

CHTML_radio::CHTML_radio(const string& name, const string& value, bool checked, const string& description)
    : CParent("RADIO", name)
{
    SetAttribute(KHTMLAttributeName_value, value);
    SetAttribute(KHTMLAttributeName_checked, checked);
    AppendHTMLText(description);  // adds the description at the end
}

// hidden tag 

CHTML_hidden::CHTML_hidden(const string& name, const string& value)
    : CParent("HIDDEN", name)
{
    SetAttribute(KHTMLAttributeName_value, value);
}

CHTML_submit::CHTML_submit(const string& name)
    : CParent("SUBMIT", name)
{
}

CHTML_submit::CHTML_submit(const string& name, const string& label)
    : CParent("SUBMIT", name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, label);
}

CHTML_reset::CHTML_reset(const string& label)
    : CParent("RESET", NcbiEmptyString)
{
    SetOptionalAttribute(KHTMLAttributeName_value, label);
}

// text tag 

CHTML_text::CHTML_text(const string& name, const string& value)
    : CParent("TEXT", name)
{
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_text::CHTML_text(const string& name, int size, const string& value)
    : CParent("TEXT", name)
{
    SetAttribute(KHTMLAttributeName_size, size);
    SetOptionalAttribute(KHTMLAttributeName_value, value);
}

CHTML_text::CHTML_text(const string& name, int size, int maxlength, const string& value)
    : CParent("TEXT", name)
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

END_NCBI_SCOPE
