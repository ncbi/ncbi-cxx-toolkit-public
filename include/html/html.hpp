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
* Revision 1.2  1998/10/29 16:15:52  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:34:30  lewisg
* html library includes
*
* ===========================================================================
*/





#include <node.hpp>

// base class for html node

class CHTMLNode : public CNCBINode {
public:
    string NodeName() const { return m_Name; }
    void NodeName(string & namein) { m_Name = namein; }
    string GetAttributes(const string& aname) { return m_Attributes[aname]; }  // how to make const with non-const []?
    void SetAttributes(const string & aname, string avalue) { m_Attributes[aname] = avalue; }
    virtual void Print(string &);   // prints the tag and children
    virtual void Rfind(const string &, CNCBINode *);  // finds and replaces text with a node    
    CHTMLNode() { m_Name = ""; }
    CNCBINode * AppendText(const string &);  // convenient way to add CHTMLText

protected:
    string m_Name; // the tag name
    map<string, string, less<string> > m_Attributes; // the tag attributes, e.g. href="link.html"
};


// A text node that contain plain text
class CHTMLText: public CHTMLNode {
public:
    string Data() const { return m_Datamember; }
    void Data(const string & datain) { m_Datamember = datain; }
    CNCBINode * Split(SIZE_TYPE); // splits a node in two
    virtual void Print(string &);
    virtual void Rfind(const string &, CNCBINode *);  
    CHTMLText() {};
    CHTMLText(const string & text) { m_Datamember = text; };
    
protected:
    string m_Datamember;  // the text
};


// An html tag
class CHTMLElement: public CHTMLNode {
public:
    virtual void Print(string&);
    CHTMLElement() { m_EndTag = true; }
protected:
    bool m_EndTag;  // is there a closing tag for the element?
};


// the "a" tag
class CHTML_a: public CHTMLElement {
public:
    CHTML_a(const string & href);
};


// the table tag
class CHTML_table: public CHTMLElement {
public:
    CHTML_table() { m_Name = "table"; }
    CHTML_table(const string &);
    CHTML_table(const string &, const string &);
    CHTML_table(int, int);
    CHTML_table(const string &, int, int);
    CHTML_table(const string &, const string &, int, int);
    CHTML_table(const string &, const string &, const string &, const string &, int, int);
    CNCBINode * InsertInTable(int, int, CNCBINode *);
    CNCBINode * InsertTextInTable(int, int, const string &);
    void ColumnWidth(CHTML_table *, int, const string &);  
protected:
    void MakeTable(int, int);

};


// the tr tag
class CHTML_tr: public CHTMLElement {
public:
    CHTML_tr() { m_Name = "tr"; }
    CHTML_tr(const string &);
    CHTML_tr(const string &, const string &);
    CHTML_tr(const string &, const string &, const string &);
};


// the td tag
class CHTML_td: public CHTMLElement {
public:
    CHTML_td() { m_Name = "td"; }
    CHTML_td(const string &);
    CHTML_td(const string &, const string &);
    CHTML_td(const string &, const string &, const string &);
};


// the form tag
class CHTML_form: public CHTMLElement {
public:
    CHTML_form() { m_Name = "form"; }
    CHTML_form(const string &, const string &);
};


// the textarea tag
class CHTML_textarea: public CHTMLElement {
public:
    CHTML_textarea() { m_Name = "textarea"; }
	CHTML_textarea(const string &, const string &, const string &);
};


class CHTML_input: public CHTMLElement {
public:
    CHTML_input() { m_Name = "input"; m_EndTag = false; };
    CHTML_input::CHTML_input(const string &, const string &);
    CHTML_input::CHTML_input(const string &, const string &, const string &);
    CHTML_input::CHTML_input(const string &, const string &, const string &, const string &);
};


//option tag.  rarely used alone.  see select tag
class CHTML_option: public CHTMLElement {
public:
    CHTML_option() { m_Name = "option"; m_EndTag = false; };
    CHTML_option(bool);
    CHTML_option(bool, const string &);
};


// select tag
class CHTML_select: public CHTMLElement {
public:
    CNCBINode * AppendOption(const string &);
    CNCBINode * AppendOption(const string &, bool);
    CNCBINode * AppendOption(const string &, bool, const string &);
    CHTML_select() { m_Name = "select"; m_EndTag = false; };
    CHTML_select(const string &);
};


// paragraph with end tag
class CHTML_p: public CHTMLElement {
public:
    CHTML_p() { m_Name = "p"; };
};


// paragraph without end tag
class CHTML_pnop: public CHTMLElement {
public:
    CHTML_pnop() { m_Name = "p"; m_EndTag = false; };
};


// break
class CHTML_br: public CHTMLElement {
public:
    CHTML_br() { m_Name = "br"; m_EndTag = false; };
};

#endif
