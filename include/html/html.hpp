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
* Revision 1.4  1998/12/01 19:09:05  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.3  1998/11/23 23:47:49  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:15:52  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:34:30  lewisg
* html library includes
*
* ===========================================================================
*/


//#include <ncbistd.h>
#include <node.hpp>
BEGIN_NCBI_SCOPE

// base class for html node

class CHTMLNode : public CNCBINode {
public:
    string NodeName(void) const { return m_Name; }
    void NodeName(string & namein) { m_Name = namein; }
    string GetAttributes(const string& aname) { return m_Attributes[aname]; }  // how to make const with non-const []?
    void SetAttributes(const string & aname, string avalue) { m_Attributes[aname] = avalue; }
    virtual void Print(string &);   // prints the tag and children
    virtual void Print(CNcbiOstream &);   // prints the tag and children
    virtual void Rfind(const string & tagname, CNCBINode * replacenode);  // finds and replaces text with a node    
    CHTMLNode(void) { m_Name = ""; }
    CNCBINode * AppendText(const string &);  // convenient way to add CHTMLText

protected:
    string m_Name; // the tag name
    map<string, string, less<string> > m_Attributes; // the tag attributes, e.g. href="link.html"
};


// A text node that contain plain text
class CHTMLText: public CHTMLNode {
public:
    string Data(void) const { return m_Datamember; }
    void Data(const string & datain) { m_Datamember = datain; }
    CNCBINode * Split(SIZE_TYPE); // splits a node in two
    virtual void Print(string &);
    virtual void Print(CNcbiOstream &);   // prints the tag and children
    virtual void Rfind(const string & tagname, CNCBINode * replacenode);  
    CHTMLText(void) {};
    CHTMLText(const string & text) { m_Datamember = text; };
    
protected:
    string m_Datamember;  // the text
};


// An html tag
class CHTMLElement: public CHTMLNode {
public:
    virtual void Print(string&);
    virtual void Print(CNcbiOstream &);   // prints the tag and children
    CHTMLElement(void) { m_EndTag = true; }
protected:
    bool m_EndTag;  // is there a closing tag for the element?
};


// the "pre" tag
class CHTML_pre: public CHTMLElement {
public:
    CHTML_pre(void) { m_Name = "pre"; }
    CHTML_pre(const string & Text);
};


// the "a" tag
class CHTML_a: public CHTMLElement {
public:
    CHTML_a(void) { m_Name = "a"; }
    CHTML_a(const string & href);
    CHTML_a(const string & Href, const string & Text);
};


// the table tag
class CHTML_table: public CHTMLElement {
public:
    CHTML_table(void) { m_Name = "table"; }
    CHTML_table(const string & bgcolor);
    CHTML_table(const string & bgcolor, const string & width);
    CHTML_table(int row, int column);
    CHTML_table(const string & bgcolor, int row, int column);
    CHTML_table(const string & bgcolor, const string & width, int row, int column);
    CHTML_table(const string & bgcolor, const string & width, const string & cellspacing, const string & cellpadding, int row, int column);
    CNCBINode * InsertInTable(int row, int column, CNCBINode *);
    CNCBINode * InsertTextInTable(int row, int column, const string &);
    void ColumnWidth(CHTML_table *, int column, const string & width);  
protected:
    void MakeTable(int, int);

};


// the tr tag
class CHTML_tr: public CHTMLElement {
public:
    CHTML_tr(void) { m_Name = "tr"; }
    CHTML_tr(const string &);
    CHTML_tr(const string &, const string &);
    CHTML_tr(const string &, const string &, const string &);
};


// the td tag
class CHTML_td: public CHTMLElement {
public:
    CHTML_td(void) { m_Name = "td"; }
    CHTML_td(const string & bgcolor);
    CHTML_td(const string & bgcolor, const string & width);
    CHTML_td(const string & bgcolor, const string & width, const string & align);
};


// the form tag
class CHTML_form: public CHTMLElement {
public:
    CHTML_form(void) {m_Name = "form";}
    CHTML_form(const string & action, const string & method);
    CHTML_form(const string & action, const string & method, const string & name);
};


// the textarea tag
class CHTML_textarea: public CHTMLElement {
public:
    CHTML_textarea(void) { m_Name = "textarea"; }
    CHTML_textarea(const string & name, const string & cols, const string & rows);
    CHTML_textarea(const string & name, const string & cols, const string & rows, const string & value);
};


class CHTML_input: public CHTMLElement {
public:
    CHTML_input(void) { m_Name = "input"; m_EndTag = false; };
    CHTML_input::CHTML_input(const string & type, const string & value);
    CHTML_input::CHTML_input(const string & type, const string & name, const string & value);
    CHTML_input::CHTML_input(const string & type, const string & name, const string & value, const string & size);
};


class CHTML_checkbox: public CHTMLElement {
public:
    CHTML_checkbox(const string & name, const string & value, const string & description, bool checked);
};
  
//option tag.  rarely used alone.  see select tag
class CHTML_option: public CHTMLElement {
public:
    CHTML_option(void) { m_Name = "option"; m_EndTag = false; };
    CHTML_option(bool selected);
    CHTML_option(bool selected, const string & value);
};


// select tag
class CHTML_select: public CHTMLElement {
public:
    CNCBINode * AppendOption(const string & option);
    CNCBINode * AppendOption(const string & option, bool selected);
    CNCBINode * AppendOption(const string & option, bool selected, const string & value);
    CHTML_select(void) { m_Name = "select"; m_EndTag = false; };
    CHTML_select(const string & name);
};


// paragraph with end tag
class CHTML_p: public CHTMLElement {
public:
    CHTML_p(void) { m_Name = "p"; };
};


// paragraph without end tag
class CHTML_pnop: public CHTMLElement {
public:
    CHTML_pnop(void) { m_Name = "p"; m_EndTag = false; };
};


// break
class CHTML_br: public CHTMLElement {
public:
    CHTML_br(void) { m_Name = "br"; m_EndTag = false; };
};

END_NCBI_SCOPE
#endif
