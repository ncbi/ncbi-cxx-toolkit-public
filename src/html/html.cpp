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

    return input;
}

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
            out << '=' << i->second;
        }
    }
    return out << '>';
}

CHTMLElement::CHTMLElement(const string& name)
    : CParent(name)
{
}

CHTMLElement::CHTMLElement(const string& name, const string& text)
    : CParent(name)
{
    AppendHTMLText(text);
}

CHTMLElement::CHTMLElement(const string& name, CNCBINode* node)
    : CParent(name)
{
    AppendChild(node);
}

CNCBINode* CHTMLElement::CloneSelf(void) const
{
    return new CHTMLElement(*this);
}

CNcbiOstream& CHTMLElement::PrintEnd(CNcbiOstream& out)
{
    return out << "</" << m_Name << ">\n";
}


// anchor element

CHTML_a::CHTML_a(const string& href)
    : CParent()
{
    SetOptionalAttribute("href", href);
}

CHTML_a::CHTML_a(const string& href, const string& text)
    : CParent(text)
{
    SetOptionalAttribute("href", href);
}


CHTML_a::CHTML_a(const string& href, CNCBINode* node)
    : CParent(node)
{
    SetOptionalAttribute("href", href);
}


// TABLE element

CHTML_table::CHTML_table(void)
    : CParent("table")
{
}


CHTML_table::CHTML_table(const string& bgcolor)
    : CParent("table")
{
     SetAttribute("bgcolor", bgcolor);
}


CHTML_table::CHTML_table(const string& bgcolor, const string& width)
    : CParent("table")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
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
    : CParent("table")
{
     MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, int row, int column)
    : CParent("table")
{
    SetAttribute("bgcolor", bgcolor);
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, int row, int column)
    : CParent("table")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, const string & cellspacing, const string & cellpadding, int row, int column)
    : CParent("table")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
    SetAttribute("cellspacing", cellspacing);
    SetAttribute("cellpadding", cellpadding);
    MakeTable(row, column);
}

CNCBINode* CHTML_table::CloneSelf(void) const
{
    return new CHTML_table(*this);
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
        if ( (*iChildren)->GetName() == "tr" )
            iRow++;
        iChildren++;
    }

    if( iChildren == m_ChildNodes.end() && iRow <= row)
        return NULL;
    iChildren--;

    iColumn = 0;
    iRowChildren = (*iChildren)->ChildBegin();

    while ( iColumn <= column && iRowChildren != (*iChildren)->ChildEnd() ) {
        if ( (*iRowChildren)->GetName() == "td" )
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
        if ( (*iChildren)->GetName() == "tr" ) {
            ix = 1;
            iyChildren = (*iChildren)->ChildBegin();
            
            while ( ix < column && iyChildren != (*iChildren)->ChildEnd() ) {
                if ( (*iyChildren)->GetName() == "td" )
                    ix++;  
                iyChildren++;
            }
            if ( iyChildren != (*iChildren)->ChildEnd() ) {
                (*iyChildren)->SetAttribute("width", width);
            }
        }
        iChildren++;
    }    
}


// tr tag


CHTML_tr::CHTML_tr(void)
    : CParent("tr")
{
}


CHTML_tr::CHTML_tr(const string & bgcolor) 
    : CParent("tr")
{
    SetAttribute("bgcolor", bgcolor);
}


CHTML_tr::CHTML_tr(const string & bgcolor, const string & width)
    : CParent("tr")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
}


CHTML_tr::CHTML_tr(const string & bgcolor, const string & width, const string & align)
    : CParent("tr")
{ 
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
    SetAttribute("align", align);
}


// td tag

CHTML_td::CHTML_td(void)
    : CParent("td")
{
}


CHTML_td::CHTML_td(const string & bgcolor)
    : CParent("td")
{
    SetAttribute("bgcolor", bgcolor);
}


CHTML_td::CHTML_td(const string & bgcolor, const string & width)
    : CParent("td")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
}


CHTML_td::CHTML_td(const string & bgcolor, const string & width, const string & align)
    : CParent("td")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
    SetAttribute("align", align);
}


// td tag

CHTML_th::CHTML_th(void)
    : CParent("th")
{
}


CHTML_th::CHTML_th(const string & bgcolor)
    : CParent("th")
{
    SetAttribute("bgcolor", bgcolor);
}


CHTML_th::CHTML_th(const string & bgcolor, const string & width)
    : CParent("th")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
}


CHTML_th::CHTML_th(const string & bgcolor, const string & width, const string & align)
    : CParent("th")
{
    SetAttribute("bgcolor", bgcolor);
    SetAttribute("width", width);
    SetAttribute("align", align);
}


// form element

CHTML_form::CHTML_form (const string& action, const string& method, const string& enctype)
{
    SetOptionalAttribute("action", action);
    SetOptionalAttribute("method", method);
    SetOptionalAttribute("enctype", enctype);
}

void CHTML_form::AddHidden(const string& name, const string& value)
{
    AppendChild(new CHTML_hidden(name, value));
}

// textarea element

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows)
{
    SetAttribute("name", name);
    SetAttribute("cols", cols);
    SetAttribute("rows", rows);
}

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows, const string& value)
    : CParent(value)
{
    SetAttribute("name", name);
    SetAttribute("cols", cols);
    SetAttribute("rows", rows);
}


//input tag

CHTML_input::CHTML_input(const string& type, const string& name)
    : CParent("input")
{
    SetAttribute("type", type);
    SetAttribute("name", name);
};


// checkbox tag 

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value, bool checked, const string& description)
    : CParent("checkbox", name)
{
    SetOptionalAttribute("value", value);
    SetOptionalAttribute("checked", checked);
    AppendHTMLText(description);  // adds the description at the end
}

CHTML_checkbox::CHTML_checkbox(const string& name, bool checked, const string& description)
    : CParent("checkbox", name)
{
    SetOptionalAttribute("checked", checked);
    AppendHTMLText(description);  // adds the description at the end
}


// radio tag 

CHTML_radio::CHTML_radio(const string& name, const string& value, bool checked, const string& description)
    : CParent("radio", name)
{
    SetAttribute("value", value);
    SetAttribute("checked", checked);
    AppendHTMLText(description);  // adds the description at the end
}

// hidden tag 

CHTML_hidden::CHTML_hidden(const string& name, const string& value)
    : CParent("hidden", name)
{
    SetAttribute("value", value);
}

// hidden tag 

CHTML_submit::CHTML_submit(const string& name)
    : CParent("input")
{
    SetAttribute("type", "submit");
    SetOptionalAttribute("value", name);
}

// text tag 

CHTML_text::CHTML_text(const string& name, const string& value)
    : CParent("text", name)
{
    SetOptionalAttribute("value", value);
}

CHTML_text::CHTML_text(const string& name, int size, const string& value)
    : CParent("text", name)
{
    SetAttribute("size", size);
    SetOptionalAttribute("value", value);
}

CHTML_text::CHTML_text(const string& name, int size, int maxlength, const string& value)
    : CParent("text", name)
{
    SetAttribute("size", size);
    SetAttribute("maxlength", maxlength);
    SetOptionalAttribute("value", value);
}

// option tag

CHTML_option::CHTML_option(const string& content, bool selected)
    : CParent("option")
{
    SetOptionalAttribute("selected", selected);
    AppendPlainText(content);
};


CHTML_option::CHTML_option(const string& content, const string& value, bool selected)
    : CParent("option")
{
    SetAttribute("value", value);
    SetOptionalAttribute("selected", selected);
    AppendPlainText(content);
};


// select tag

CHTML_select::CHTML_select(const string& name, bool multiple)
    : CParent("select")
{
    SetAttribute("name", name);
    SetOptionalAttribute("multiple", multiple);
}

CHTML_select::CHTML_select(const string& name, int size, bool multiple)
    : CParent("select")
{
    SetAttribute("name", name);
    SetAttribute("size", size);
    SetOptionalAttribute("multiple", multiple);
}


CHTML_select* CHTML_select::AppendOption(const string& option, bool selected)
{
    AppendChild(new CHTML_option(option, selected));
    return this;
};


CHTML_select* CHTML_select::AppendOption(const string& option, const string& value, bool selected)
{
    AppendChild(new CHTML_option(option, value, selected));
    return this;
};


const string KHTMLTagName_html = "html";
const string KHTMLTagName_head = "head";
const string KHTMLTagName_title = "title";
const string KHTMLTagName_body = "body";
const string KHTMLTagName_address = "address";
const string KHTMLTagName_blockquote = "blockquote";
const string KHTMLTagName_form = "form";
const string KHTMLTagName_textarea = "textarea";
const string KHTMLTagName_select = "select";
const string KHTMLTagName_h1 = "h1";
const string KHTMLTagName_h2 = "h2";
const string KHTMLTagName_h3 = "h3";
const string KHTMLTagName_h4 = "h4";
const string KHTMLTagName_h5 = "h5";
const string KHTMLTagName_h6 = "h6";
const string KHTMLTagName_p = "p";
const string KHTMLTagName_pre = "pre";
const string KHTMLTagName_a = "a";
const string KHTMLTagName_cite = "cite";
const string KHTMLTagName_code = "code";
const string KHTMLTagName_em = "em";
const string KHTMLTagName_kbd = "kbd";
const string KHTMLTagName_samp = "samp";
const string KHTMLTagName_strong = "strong";
const string KHTMLTagName_var = "var";
const string KHTMLTagName_b = "b";
const string KHTMLTagName_i = "i";
const string KHTMLTagName_tt = "tt";
const string KHTMLTagName_u = "u";
const string KHTMLTagName_caption = "caption";
const string KHTMLTagName_sub = "sub";
const string KHTMLTagName_sup = "sup";
const string KHTMLTagName_big = "big";
const string KHTMLTagName_small = "small";
const string KHTMLTagName_center = "center";
const string KHTMLTagName_font = "font";
const string KHTMLTagName_blink = "blink";

// p tag without close

CHTML_pnop::CHTML_pnop(void)
    : CParent("p")
{
}

// br tag

CHTML_br::CHTML_br(void)
    : CParent("br")
{
}

CHTML_br::CHTML_br(int count)
    : CParent("br")
{
    for ( int i = 1; i < count; ++i )
        AppendChild(new CHTML_br());
}

// img tag

CHTML_img::CHTML_img(void)
    : CParent("img")
{
}

CHTML_img::CHTML_img(const string & src, const string & width, const string & height, const string & border)
    : CParent("img")
{
    SetAttribute("src", src);
    SetAttribute("width", width);
    SetAttribute("height", height);
    SetAttribute("border", border);
}

// dl tag

CHTML_dl::CHTML_dl(bool compact)
    : CParent("dl")
{
    SetOptionalAttribute("compact", compact);
}

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

// dt tag

CHTML_dt::CHTML_dt(CNCBINode* term)
    : CParent("dt")
{
    if ( term )
        AppendChild(term);
}

CHTML_dt::CHTML_dt(const string& term)
    : CParent("dt")
{
    AppendHTMLText(term);
}

// dd tag

CHTML_dd::CHTML_dd(CNCBINode* term)
    : CParent("dd")
{
    if ( term )
        AppendChild(term);
}

CHTML_dd::CHTML_dd(const string& term)
    : CParent("dd")
{
    AppendHTMLText(term);
}



END_NCBI_SCOPE
