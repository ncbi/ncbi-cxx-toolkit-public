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
* Revision 1.1  1998/10/06 20:34:30  lewisg
* html library includes
*
* ===========================================================================
*/

#include <ncbistd.hpp>



#include <node.hpp>


class CHTMLNode : public CNCBINode // base class for html node
{
public:
    string NodeName() const { return m_Name; }
    void NodeName(string namein) { m_Name = namein; }
    string GetAttributes(string aname) { return m_Attributes[aname]; }  // how to make const with non-const []?
    void SetAttributes(string aname, string avalue) { m_Attributes[aname] = avalue; }
    virtual void Print(string&);   // prints the tag and children
    virtual void Rfind(string&, CNCBINode *);  // finds and replaces text with a node    

protected:
    string m_Name; // the tag name
    map<string, string, less<string> > m_Attributes; // the tag attributes, e.g. href="link.html"
};


// A text node that contain plain text
class CHTMLText: public CHTMLNode 
{
public:
    string Data() const { return m_Datamember; }
    void Data(string datain) { m_Datamember = datain; }
    CNCBINode * Split(SIZE_TYPE); // splits a node in two
    virtual void Print(string&);
    virtual void Rfind(string&, CNCBINode *);  
    CHTMLText() {};
    CHTMLText(string text) { m_Datamember = text; };
    
protected:
    string m_Datamember;  // the text
};


// An html tag
class CHTMLElement: public CHTMLNode 
{
public:
    virtual void Print(string&);
    CHTMLElement() { m_EndTag = true; }
protected:
    bool m_EndTag;  // is there a closing tag for the element?
};


// the "a" tag
class CHTML_a: public CHTMLElement
{
public:
    CHTML_a(string href);
};


// the table tag
class CHTML_table: public CHTMLElement
{
public:
    CHTML_table() { m_Name = "table"; }
    CHTML_table(string);
    CHTML_table(string, string);
    CHTML_table(int, int);
    CHTML_table(string, int, int);
    CHTML_table(string, string, int, int);
    CNCBINode * InsertInTable(int, int, CNCBINode *);
    void ColumnWidth(CHTML_table *, int, string);  
protected:
    void MakeTable(int, int);

};


// the tr tag
class CHTML_tr: public CHTMLElement
{
public:
    CHTML_tr() { m_Name = "tr"; }
    CHTML_tr(string);
    CHTML_tr(string, string);
    CHTML_tr(string, string, string);
};


// the td tag
class CHTML_td: public CHTMLElement
{
public:
    CHTML_td() { m_Name = "td"; }
    CHTML_td(string);
    CHTML_td(string, string);
    CHTML_td(string, string, string);
};


// the form tag
class CHTML_form: public CHTMLElement
{
public:
    CHTML_form() { m_Name = "form"; }
    CHTML_form(string, string);
};


// the textarea tag
class CHTML_textarea: public CHTMLElement
{
public:
    CHTML_textarea() { m_Name = "textarea"; }
	CHTML_textarea(string, string, string);
};


class CHTML_input: public CHTMLElement
{
public:
    CHTML_input() { m_Name = "input"; m_EndTag = false; };
    CHTML_input::CHTML_input(string, string, string);
    CHTML_input::CHTML_input(string, string, string, string);
};


//option tag.  rarely used alone.  see select tag
class CHTML_option: public CHTMLElement 
{
public:
    CHTML_option() { m_Name = "option"; m_EndTag = false; };
    CHTML_option(bool);
};


// select tag
class CHTML_select: public CHTMLElement 
{
public:
    CNCBINode * AppendOption(string);
    CNCBINode * AppendOption(string, bool);
    CHTML_select() { m_Name = "select"; m_EndTag = false; };
    CHTML_select(string);
};


// paragraph with end tag
class CHTML_p: public CHTMLElement 
{
public:
    CHTML_p() { m_Name = "p"; };
};


// paragraph without end tag
class CHTML_pnop: public CHTMLElement 
{
public:
    CHTML_pnop() { m_Name = "p"; m_EndTag = false; };
};

#endif
