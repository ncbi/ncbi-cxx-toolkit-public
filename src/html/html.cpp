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


string & CHTMLHelper::HTMLEncode(string & input)
{
    string::size_type i = 0;
    i = input.find_first_of("\"&<>", 0);
    while(i!=string::npos) {
	switch(input[i]) {
	case '"':
	    input.replace(i, (SIZE_TYPE)1, "&quot;");
	    break;
	case '&':
	    input.replace(i, (SIZE_TYPE)1, "&amp;");
	    break;
	case '<':
	    input.replace(i, (SIZE_TYPE)1, "&lt;");
	    break;
	case '>':
	    input.replace(i, (SIZE_TYPE)1, "&gt;");
	    break;
	default:
	    break;
	}	
	i = input.find_first_of("\"&<>", ++i);
    }
    return input;
}


string CHTMLHelper::IntToString(int Input)
{
    CNcbiOstrstream ostream;
    ostream << Input << '\0';
    return ostream.str();
}




CHTMLNode::CHTMLNode(void): m_Name("") {}


// serialize the node to the given string
void CHTMLNode::Print(string & output)  
{
    list <CNCBINode *>::iterator iChildren = m_ChildNodes.begin();
    for (; iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode * ) (*iChildren))->Print(output);
}


void CHTMLNode::Print(CNcbiOstream & output)  
{
    list <CNCBINode *>::iterator iChildren = m_ChildNodes.begin();
    for (; iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode * ) (*iChildren))->Print(output);
}

// this function searches for a text string in a Text node and replaces it with a node
void CHTMLNode::Rfind(const string & tagname, CNCBINode * replacenode)
{
    list <CNCBINode *>::iterator iChildren;
    
    if(!replacenode) return;
    for (iChildren = m_ChildNodes.begin(); iChildren != m_ChildNodes.end(); iChildren++) {     
        ((CHTMLNode * )(*iChildren))->Rfind(tagname, replacenode);
    }
}


CNCBINode * CHTMLNode::AppendText(const string & appendstring)
{
    CHTMLText * text;
    try {  
	text = new CHTMLText(appendstring);
	return AppendChild(text);
    }
    catch (...) {
	delete text;
	throw;
    }
}


// text node

CHTMLText::CHTMLText(void) {}


CHTMLText::CHTMLText(const string & text): m_Datamember(text) {}


void CHTMLText::Print(string& output)  
{
    list <CNCBINode *> :: iterator iChildren = m_ChildNodes.begin();
    output.append(m_Datamember);
    for (; iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode *)(* iChildren))->Print(output);
}


void CHTMLText::Print(CNcbiOstream & output)  
{
    list <CNCBINode *> :: iterator iChildren = m_ChildNodes.begin();
    output << m_Datamember.c_str();
    for (; iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode *)(* iChildren))->Print(output);
}


// splits a text node after the iSplit position

CNCBINode * CHTMLText::Split(SIZE_TYPE iSplit) // throw(out_of_range)
{
    CHTMLText * outText;
    if(iSplit > m_Datamember.size()) throw out_of_range("CHTMLText::Split arg out of range");
    try {
	outText = new CHTMLText;
	outText->m_Datamember = m_Datamember.substr(0, iSplit); 
	m_Datamember.erase(0, iSplit);
    }
    catch(...) {
        delete outText;
    }
    return outText;
}


void CHTMLText::Rfind(const string & tagname, CNCBINode * replacenode)
{
    list <CNCBINode *>::iterator iChildren;
    SIZE_TYPE iPosition;
    CNCBINode * splittext;
    
    if(!replacenode) return;
    iPosition = m_Datamember.find(tagname, 0); // look for the tag to replace
    if (iPosition != NPOS) {
        splittext = Split(iPosition);  // split it at the found position
        if(splittext && m_ParentNode) {
            m_Datamember.erase(0, tagname.size());
            m_ParentNode->InsertBefore(splittext, this);
            m_ParentNode->InsertBefore(replacenode, this);
        }
    }
    for (iChildren = m_ChildNodes.begin(); iChildren != m_ChildNodes.end(); iChildren++) {     
        ((CHTMLNode *)(* iChildren))->Rfind(tagname, replacenode);
    }
    
}


// tag node

CHTMLElement::CHTMLElement(void): m_EndTag(true) {}


void CHTMLElement::Print(string& output)  
{
    list <CNCBINode *>::iterator iChildren;
    map <string, string, less<string> > :: iterator iAttributes = m_Attributes.begin();
    
    output.append( "<" + m_Name);
    while (iAttributes != m_Attributes.end()) {
        if ((*iAttributes).second == "") {
            output.append(" " + (*iAttributes).first);
            iAttributes++;
        }
        else {
            output.append(" " + (*iAttributes).first + "=" + (*iAttributes).second );
            iAttributes++;
        }
    }
    output.append(">");
    
    for (iChildren = m_ChildNodes.begin(); iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode * )(*iChildren))->Print(output);
    if(m_EndTag) output.append( "</" + m_Name + ">\n");
}



void CHTMLElement::Print(CNcbiOstream & output)  
{
    list <CNCBINode *>::iterator iChildren;
    map <string, string, less<string> > :: iterator iAttributes = m_Attributes.begin();
    
    output << "<" << m_Name.c_str();
    while (iAttributes != m_Attributes.end()) {
        if ((*iAttributes).second == "") {
            output << " " << (*iAttributes).first.c_str();
            iAttributes++;
        }
        else {
            output << " " << (*iAttributes).first.c_str() << "=" << (*iAttributes).second.c_str();
            iAttributes++;
        }
    }
    output << ">";
    
    for (iChildren = m_ChildNodes.begin(); iChildren != m_ChildNodes.end(); iChildren++) ((CHTMLNode * )(*iChildren))->Print(output);
    if(m_EndTag) output << "</" << m_Name.c_str() << ">\n";
}


// pre element

CHTML_pre::CHTML_pre(void) { m_Name = "pre"; }


CHTML_pre::CHTML_pre(const string & Text)
{
     m_Name = "pre";
     AppendText(Text);
}


// anchor element

CHTML_a::CHTML_a(void) { m_Name = "a"; }


CHTML_a::CHTML_a(const string & href)
{
    m_Name = "a";
    m_Attributes["href"] = href;
}


CHTML_a::CHTML_a(const string & Href, const string & Text)
{
    m_Name = "a";
    m_Attributes["href"] = Href;
    AppendText(Text);
}


// TABLE element

CHTML_table::CHTML_table(void) { m_Name = "table"; }


CHTML_table::CHTML_table(const string & bgcolor)
{
     m_Name = "table";
     m_Attributes["bgcolor"] = bgcolor;
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width)
{
    m_Name = "table";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
}


// instantiates table rows and cells
void CHTML_table::MakeTable(int row, int column)  // throw(bad_alloc)
{
    int irow, icolumn;
    CHTML_tr * tr;

    for(irow = 0; irow < row; irow++)
	{
	    try {
		tr = new CHTML_tr;
		for (icolumn = 0; icolumn < column; icolumn++) tr->AppendChild(new CHTML_td);
	    }
	    catch(...) {
		delete tr;
		throw;
	    }
	
	    this->AppendChild(tr);
	}
}


// contructors that also instantiate rows and cells
CHTML_table::CHTML_table(int row, int column)
{
     m_Name = "table";
     MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, int row, int column)
{
    m_Name = "table";
    m_Attributes["bgcolor"] = bgcolor;
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, int row, int column)
{
    m_Name = "table";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
    MakeTable(row, column);
}


CHTML_table::CHTML_table(const string & bgcolor, const string & width, const string & cellspacing, const string & cellpadding, int row, int column)
{
    m_Name = "table";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
    m_Attributes["cellspacing"] = cellspacing;
    m_Attributes["cellpadding"] = cellpadding;
    MakeTable(row, column);
}


CNCBINode * CHTML_table::InsertInTable(int row, int column, CNCBINode * Child)  // todo: exception
{
    int iRow, iColumn;
    list <CNCBINode *>::iterator iChildren;
    list <CNCBINode *>::iterator iRowChildren;

    if(!Child) return NULL;
    iRow = 0;
    iChildren = m_ChildNodes.begin();

    while (iRow <= row && iChildren != m_ChildNodes.end()) {
        if (((CHTMLNode *)(* iChildren))->GetName() == "tr") iRow++;           
        iChildren++;
    }

    if( iChildren == m_ChildNodes.end() && iRow <= row) return NULL;
    iChildren--;

    iColumn = 0;
    iRowChildren = ((CHTMLNode *)(* iChildren))->ChildBegin();

    while (iColumn <= column && iRowChildren != ((CHTMLNode *)(*iChildren))->ChildEnd()) {
        if (((CHTMLNode *)(* iRowChildren))->GetName() == "td") iColumn++;           
        iRowChildren++;
    }
    if(iRowChildren == ((CHTMLNode *)(*iChildren))->ChildEnd() && 
       iColumn <= column) return NULL;
    else {
	iRowChildren--;
	return ((CHTMLNode *)(* iRowChildren))->AppendChild(Child);
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
        if (((CHTMLNode *)(* iChildren))->GetName() == "tr") {
            ix = 1;
            iyChildren = ((CHTMLNode *)(* iChildren))->ChildBegin();
            
            while (ix < column && iyChildren != ((CHTMLNode *)(*iChildren))->ChildEnd()) {
                if (((CHTMLNode *)(* iyChildren))->GetName() == "td") ix++;  
                iyChildren++;
            }
            if(iyChildren != ((CHTMLNode *)(*iChildren))->ChildEnd()) {
                ((CHTMLNode *)(*iyChildren))->SetAttributes("width", width);
            }
        }
        iChildren++;
    }
    
}


// tr tag


CHTML_tr::CHTML_tr(void) { m_Name = "tr"; }


CHTML_tr::CHTML_tr(const string & bgcolor) 
{
    m_Name = "tr";
    m_Attributes["bgcolor"] = bgcolor;
}


CHTML_tr::CHTML_tr(const string & bgcolor, const string & width)
{
    m_Name = "tr";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
}


CHTML_tr::CHTML_tr(const string & bgcolor, const string & width, const string & align)
{ 
    m_Name = "tr";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
    m_Attributes["align"] = align;
}


// td tag

CHTML_td::CHTML_td(void) { m_Name = "td"; }


CHTML_td::CHTML_td(const string & bgcolor)
{
    m_Name = "td";
    m_Attributes["bgcolor"] = bgcolor;
}


CHTML_td::CHTML_td(const string & bgcolor, const string & width)
{
    m_Name = "td";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
}


CHTML_td::CHTML_td(const string & bgcolor, const string & width, const string & align)
{
    m_Name = "td";
    m_Attributes["bgcolor"] = bgcolor;
    m_Attributes["width"] = width;
    m_Attributes["align"] = align;
}


// form element

CHTML_form::CHTML_form(void) { m_Name = "form"; }


CHTML_form::CHTML_form(const string & action, const string & method)
{
    m_Name = "form";
    m_Attributes["action"] = action;
    m_Attributes["method"] = method;
}


CHTML_form::CHTML_form (const string & action, const string & method, const string & name)
{
    m_Name = "form";
    m_Attributes["action"] = action;
    m_Attributes["method"] = method;
    m_Attributes["name"] = name;
}


// textarea element

CHTML_textarea::CHTML_textarea(void) { m_Name = "textarea"; }


CHTML_textarea::CHTML_textarea(const string & name, const string & cols, const string & rows)
{
    m_Name = "textarea";
    m_Attributes["name"] = name;
    m_Attributes["cols"] = cols;
    m_Attributes["rows"] = rows;
}


CHTML_textarea::CHTML_textarea(const string & name, const string & cols, const string & rows, const string & value)
{
    m_Name = "textarea";
    m_Attributes["name"] = name;
    m_Attributes["cols"] = cols;
    m_Attributes["rows"] = rows;
    AppendText(value);  // an exception should propagate ok?
}


//input tag

CHTML_input::CHTML_input(void) { m_Name = "input"; m_EndTag = false; }


CHTML_input::CHTML_input(const string & type, const string & value)
{
    m_Name = "input";
    m_EndTag = false;
    m_Attributes["type"] = type;
    m_Attributes["value"] = value;
};


CHTML_input::CHTML_input(const string & type, const string & name, const string & value)
{
    m_Name = "input";
    m_EndTag = false;
    m_Attributes["type"] = type;
    m_Attributes["name"] = name;
    m_Attributes["value"] = value;
};


CHTML_input::CHTML_input(const string & type, const string & name, const string & value, const string & size)
{
    m_Name = "input";
    m_EndTag = false;
    m_Attributes["type"] = type;
    m_Attributes["name"] = name;
    m_Attributes["size"] = size;
    if(!value.empty()) m_Attributes["value"] = value;
};


// checkbox tag 

CHTML_checkbox::CHTML_checkbox(const string & name, const string & value, const string & description, bool checked)
{
    m_Name = "input";
    m_EndTag = false;
    m_Attributes["type"] = "checkbox";
    m_Attributes["name"] = name;
    if(!value.empty()) m_Attributes["value"] = value;
    if (checked) m_Attributes["checked"] = "";    
    AppendText(description);  // adds the description at the end
}


// option tag

CHTML_option::CHTML_option(void)
{
    m_Name = "option";
    m_EndTag = false;
}


CHTML_option::CHTML_option(bool selected)
{
    m_Name = "option";
    m_EndTag = false;
    if (selected) m_Attributes["selected"] = "";
};


CHTML_option::CHTML_option(bool selected, const string & value)
{
    m_Name = "option";
    m_EndTag = false;
    if (selected) m_Attributes["selected"] = "";
    m_Attributes["value"] = value;
};


// select tag

CHTML_select::CHTML_select(void) { m_Name = "select"; }


CHTML_select::CHTML_select(const string & name)
{
    m_Name = "select";
    m_Attributes["name"] = name;
}


CNCBINode * CHTML_select::AppendOption(const string & option)
{
    CNCBINode * retval;
    try {
	retval = AppendChild(new CHTML_option);
	((CHTMLNode * )(retval))->AppendChild(new CHTMLText(option));
    }
    catch (...) {
        delete retval;
        throw;
    }
    return retval;
};


CNCBINode * CHTML_select::AppendOption(const string & option, bool selected)
{
    CNCBINode * retval;
    try {
	retval = AppendChild(new CHTML_option(selected));
	((CHTMLNode * )(retval))->AppendChild(new CHTMLText(option));
    }
    catch (...) {
	delete retval;
	throw;
    }

    return retval;
};


CNCBINode * CHTML_select::AppendOption(const string & option, bool selected, const string & value)
{
    CNCBINode * retval;
    try {
	retval = AppendChild(new CHTML_option(selected, value));
	((CHTMLNode * )(retval))->AppendChild(new CHTMLText(option));
    }
    catch (...) {
	delete retval;
	throw;
    }

    return retval;
};


// p tag with close tag

CHTML_p::CHTML_p(void) { m_Name = "p"; }


// p tag without close

CHTML_pnop::CHTML_pnop(void) { m_Name = "p"; m_EndTag = false; }


// br tag

CHTML_br::CHTML_br(void) { m_Name = "br"; m_EndTag = false; }


// img tag

CHTML_img::CHTML_img(void) { m_Name = "img"; m_EndTag = false; }


CHTML_img::CHTML_img(const string & src, const string & width, const string & height, const string & border)
{
    m_Name = "img";
    m_EndTag = false; 
    m_Attributes["src"] = src;
    m_Attributes["width"] = width;
    m_Attributes["height"] = height;
    m_Attributes["border"] = border;
}


END_NCBI_SCOPE
