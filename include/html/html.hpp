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


#include <ncbistd.h>
#include <node.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// utility functions

class CHTMLHelper
{
public:
    static string HTMLEncode(const string &);  // HTML encodes a string. E.g. &lt;
};


// base class for html node

class CHTMLNode : public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLNode(const string& name);

    void AppendPlainText(const string &);  // convenient way to add CHTMLPlainText
    void AppendHTMLText(const string &);  // convenient way to add CHTMLText
};

// <@XXX@> mapping node

class CHTMLTagNode : public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLTagNode(const string& tagname);

    virtual void CreateSubNodes(void);

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};


// A text node that contains plain text
class CHTMLPlainText : public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLPlainText(const string& text);
    
    const string& GetText(void) const;
    void SetText(const string& text);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out);

protected:
    string m_Text;  // the text

    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// A text node that contains html text with tags and possibly <@TAG@>
class CHTMLText : public CNCBINode {
    // parent class
    typedef CNCBINode CParent;

public:
    CHTMLText(const string& text);
    
    const string& GetText(void) const;

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out);
    virtual void CreateSubNodes();

protected:
    string m_Text;  // the text

    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// An html tag
class CHTMLOpenElement: public CHTMLNode {
    // parent class
    typedef CHTMLNode CParent;

public:
    CHTMLOpenElement(const string& name);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &);   // prints tag itself

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

// An html tag
class CHTMLElement: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTMLElement(const string& name);
    CHTMLElement(const string& name, const string& text);
    CHTMLElement(const string& name, CNCBINode* node);

    virtual CNcbiOstream& PrintEnd(CNcbiOstream &);   // prints tag close

protected:
    // cloning
    virtual CNCBINode* CloneSelf() const;
};

template<const string* TagName>
class CHTMLElementTmpl : public CHTMLElement {
    typedef CHTMLElement CParent;

public:
    CHTMLElementTmpl(void) : CParent(*TagName)
        {}
    CHTMLElementTmpl(const string& text) : CParent(*TagName, text)
        {}
    CHTMLElementTmpl(CNCBINode* node) : CParent(*TagName, node)
        {}
};

template<const string* TagName>
class CHTMLOpenElementTmpl : public CHTMLOpenElement {
    typedef CHTMLOpenElement CParent;

public:
    CHTMLOpenElementTmpl(void) : CParent(*TagName)
        {}
    CHTMLOpenElementTmpl(const string& text) : CParent(*TagName, text)
        {}
    CHTMLOpenElementTmpl(CNCBINode* node) : CParent(*TagName, node)
        {}
};

// closed HTML element names
extern const string KHTMLTagName_html;
extern const string KHTMLTagName_head;
extern const string KHTMLTagName_title;
extern const string KHTMLTagName_body;
extern const string KHTMLTagName_address;
extern const string KHTMLTagName_blockquote;
extern const string KHTMLTagName_form;
extern const string KHTMLTagName_textarea;
extern const string KHTMLTagName_select;
extern const string KHTMLTagName_h1;
extern const string KHTMLTagName_h2;
extern const string KHTMLTagName_h3;
extern const string KHTMLTagName_h4;
extern const string KHTMLTagName_h5;
extern const string KHTMLTagName_h6;
extern const string KHTMLTagName_p;
extern const string KHTMLTagName_pre;
extern const string KHTMLTagName_a;
extern const string KHTMLTagName_cite;
extern const string KHTMLTagName_code;
extern const string KHTMLTagName_em;
extern const string KHTMLTagName_kbd;
extern const string KHTMLTagName_samp;
extern const string KHTMLTagName_strong;
extern const string KHTMLTagName_var;
extern const string KHTMLTagName_b;
extern const string KHTMLTagName_i;
extern const string KHTMLTagName_tt;
extern const string KHTMLTagName_u;
extern const string KHTMLTagName_caption;
extern const string KHTMLTagName_sub;
extern const string KHTMLTagName_sup;
extern const string KHTMLTagName_big;
extern const string KHTMLTagName_small;
// netscape specific
extern const string KHTMLTagName_center;
extern const string KHTMLTagName_font;
extern const string KHTMLTagName_blink;

typedef CHTMLElementTmpl<&KHTMLTagName_html> CHTML_html;
typedef CHTMLElementTmpl<&KHTMLTagName_head> CHTML_head;
typedef CHTMLElementTmpl<&KHTMLTagName_title> CHTML_title;
typedef CHTMLElementTmpl<&KHTMLTagName_body> CHTML_body;
typedef CHTMLElementTmpl<&KHTMLTagName_address> CHTML_address;
typedef CHTMLElementTmpl<&KHTMLTagName_blockquote> CHTML_blockquote;
typedef CHTMLElementTmpl<&KHTMLTagName_form> CHTML_form_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_textarea> CHTML_textarea_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_h1> CHTML_h1;
typedef CHTMLElementTmpl<&KHTMLTagName_h2> CHTML_h2;
typedef CHTMLElementTmpl<&KHTMLTagName_h3> CHTML_h3;
typedef CHTMLElementTmpl<&KHTMLTagName_h4> CHTML_h4;
typedef CHTMLElementTmpl<&KHTMLTagName_h5> CHTML_h5;
typedef CHTMLElementTmpl<&KHTMLTagName_h6> CHTML_h6;
typedef CHTMLElementTmpl<&KHTMLTagName_p> CHTML_p;
typedef CHTMLElementTmpl<&KHTMLTagName_pre> CHTML_pre;
typedef CHTMLElementTmpl<&KHTMLTagName_a> CHTML_a_Base;
typedef CHTMLElementTmpl<&KHTMLTagName_cite> CHTML_cite;
typedef CHTMLElementTmpl<&KHTMLTagName_code> CHTML_code;
typedef CHTMLElementTmpl<&KHTMLTagName_em> CHTML_em;
typedef CHTMLElementTmpl<&KHTMLTagName_kbd> CHTML_kbd;
typedef CHTMLElementTmpl<&KHTMLTagName_samp> CHTML_samp;
typedef CHTMLElementTmpl<&KHTMLTagName_strong> CHTML_strong;
typedef CHTMLElementTmpl<&KHTMLTagName_var> CHTML_var;
typedef CHTMLElementTmpl<&KHTMLTagName_b> CHTML_b;
typedef CHTMLElementTmpl<&KHTMLTagName_i> CHTML_i;
typedef CHTMLElementTmpl<&KHTMLTagName_tt> CHTML_tt;
typedef CHTMLElementTmpl<&KHTMLTagName_u> CHTML_u;
typedef CHTMLElementTmpl<&KHTMLTagName_caption> CHTML_caption;
typedef CHTMLElementTmpl<&KHTMLTagName_sub> CHTML_sub;
typedef CHTMLElementTmpl<&KHTMLTagName_sup> CHTML_sup;
typedef CHTMLElementTmpl<&KHTMLTagName_big> CHTML_big;
typedef CHTMLElementTmpl<&KHTMLTagName_small> CHTML_small;
typedef CHTMLElementTmpl<&KHTMLTagName_center> CHTML_center;
typedef CHTMLElementTmpl<&KHTMLTagName_font> CHTML_font;
typedef CHTMLElementTmpl<&KHTMLTagName_blink> CHTML_blink;


// the "a" tag
class CHTML_a : public CHTML_a_Base {
    typedef CHTML_a_Base CParent;

public:
    CHTML_a(const string& href);
    CHTML_a(const string& href, const string& text);
    CHTML_a(const string& href, CNCBINode* node);
};

// the table tag
class CHTML_table: public CHTMLElement {
    // parent class
    typedef CHTMLElement CParent;

public:
    CHTML_table(void);
    CHTML_table(const string & bgcolor);
    CHTML_table(const string & bgcolor, const string & width);
    CHTML_table(int row, int column);
    CHTML_table(const string & bgcolor, int row, int column);
    CHTML_table(const string & bgcolor, const string & width, int row, int column);
    CHTML_table(const string & bgcolor, const string & width, const string & cellspacing, const string & cellpadding, int row, int column);

    CNCBINode* Cell(int row, int column);
    CNCBINode* InsertInTable(int row, int column, CNCBINode *);
    CNCBINode* InsertTextInTable(int row, int column, const string &);

    void ColumnWidth(CHTML_table*, int column, const string & width);

protected:
    void MakeTable(int, int);

    // cloning
    virtual CNCBINode* CloneSelf() const;
};


// the tr tag
class CHTML_tr: public CHTMLElement {
    // parent class
    typedef CHTMLElement CParent;

public:
    CHTML_tr(void);
    CHTML_tr(const string & bgcolor);
    CHTML_tr(const string & bgcolor, const string & width);
    CHTML_tr(const string & bgcolor, const string & width, const string & align);

};


// the th tag
class CHTML_th: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_th(void);
    CHTML_th(const string & bgcolor);
    CHTML_th(const string & bgcolor, const string & width);
    CHTML_th(const string & bgcolor, const string & width, const string & align);

};

// the td tag
class CHTML_td: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_td(void);
    CHTML_td(const string & bgcolor);
    CHTML_td(const string & bgcolor, const string & width);
    CHTML_td(const string & bgcolor, const string & width, const string & align);

};


// the form tag
class CHTML_form : public CHTML_form_Base {
    // parent class
    typedef CHTML_form_Base CParent;

public:
    CHTML_form(const string& action = NcbiEmptyString, const string& method = NcbiEmptyString, const string& enctype = NcbiEmptyString);

    void AddHidden(const string& name, const string& value);
};

// the textarea tag
class CHTML_textarea: public CHTML_textarea_Base {
    // parent class
    typedef CHTML_textarea_Base CParent;

public:
    CHTML_textarea(const string& name, int cols, int rows);
    CHTML_textarea(const string& name, int cols, int rows, const string& value);

};

// input tag
class CHTML_input: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_input(const string& type, const string& name);
};

// input type=checkbox tag
class CHTML_checkbox: public CHTML_input {
    // parent class
    typedef CHTML_input CParent;

public:
    CHTML_checkbox(const string& name, bool checked = false, const string& description = NcbiEmptyString);
    CHTML_checkbox(const string& name, const string& value, bool checked = false, const string& description = NcbiEmptyString);

};

// input type=radio tag
class CHTML_radio: public CHTML_input {
    // parent class
    typedef CHTML_input CParent;

public:
    CHTML_radio(const string& name, const string& value, bool checked = false, const string& description = NcbiEmptyString);

};

// input type=hidden tag
class CHTML_hidden: public CHTML_input {
    // parent class
    typedef CHTML_input CParent;

public:
    CHTML_hidden(const string& name, const string& value);

};

// input type=text tag
class CHTML_text: public CHTML_input {
    // parent class
    typedef CHTML_input CParent;

public:
    CHTML_text(const string& name, const string& value = NcbiEmptyString);
    CHTML_text(const string& name, int size, const string& value = NcbiEmptyString);
    CHTML_text(const string& name, int size, int maxlength, const string& value = NcbiEmptyString);

};

// input type=submit tag
class CHTML_submit: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_submit(const string& name);

};

//option tag.  rarely used alone.  see select tag
class CHTML_option: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_option(const string& content, bool selected = false);
    CHTML_option(const string& content, const string& value, bool selected = false);

};


// select tag
class CHTML_select : public CHTMLElement {
    // parent class
    typedef CHTMLElement CParent;

public:
    CHTML_select(const string & name, bool multiple = false);
    CHTML_select(const string & name, int size, bool multiple = false);

    // return this to allow chained AppendOption
    CHTML_select* AppendOption(const string& option, bool selected = false);
    CHTML_select* AppendOption(const string& option, const string & value, bool selected = false);
    
};

/*
// font change tag
class CHTML_font: public CHTMLElement {
    // parent class
    typedef CHTMLElement CParent;

public:
    CHTML_font(void);

};
*/

// paragraph without end tag
class CHTML_pnop: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_pnop(void);

};


// break
class CHTML_br: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_br(void);
    // create <number> of <br> tags
    CHTML_br(int number);

};


class CHTML_img: public CHTMLOpenElement {
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_img(void);
    CHTML_img(const string & src, const string& width, const string& height, const string& border = "0");

};

// dl tag
class CHTML_dl : public CHTMLElement
{
    // parent class
    typedef CHTMLElement CParent;

public:
    CHTML_dl(bool compact = false);

    // return this to allow chained AppendTerm
    CHTML_dl* AppendTerm(const string& term, const string& definition = NcbiEmptyString);
    CHTML_dl* AppendTerm(const string& term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(CNCBINode* term, const string& definition = NcbiEmptyString);
    CHTML_dl* AppendTerm(CNCBINode* term, CNCBINode* definition = 0);

};

// dt tag
class CHTML_dt : public CHTMLOpenElement
{
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_dt(const string& term);
    CHTML_dt(CNCBINode* term = 0);

};

// dd tag
class CHTML_dd : public CHTMLOpenElement
{
    // parent class
    typedef CHTMLOpenElement CParent;

public:
    CHTML_dd(const string& term);
    CHTML_dd(CNCBINode* definition = 0);

};

// inline functions
#include <html.inl>

END_NCBI_SCOPE

#endif
