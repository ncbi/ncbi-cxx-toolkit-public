#ifndef HTML__HTML__HPP
#define HTML__HTML__HPP


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
 * File Description:  CHTMLNode defines
 *
 */

#include <corelib/ncbiobj.hpp>
#include <html/node.hpp>

BEGIN_NCBI_SCOPE



// Macro for declare html elements

#define CHTML_NAME(Tag) NCBI_NAME2(CHTML_, Tag)

#define DECLARE_HTML_ELEMENT_CONSTRUCTORS(Tag, Parent) \
    CHTML_NAME(Tag)(void) \
        : CParent(sm_TagName) \
        { } \
    CHTML_NAME(Tag)(const char* text) \
        : CParent(sm_TagName, text) \
        { } \
    CHTML_NAME(Tag)(const string& text) \
        : CParent(sm_TagName, text) \
        { } \
    CHTML_NAME(Tag)(CNCBINode* node) \
        : CParent(sm_TagName, node) \
        { } \
    ~CHTML_NAME(Tag)(void)


#define DECLARE_HTML_ELEMENT_CONSTRUCTORS_WITH_INIT(Tag, Parent) \
    CHTML_NAME(Tag)(void) \
        : CParent(sm_TagName) \
        { Init(); } \
    CHTML_NAME(Tag)(const char* text) \
        : CParent(sm_TagName, text) \
        { Init(); } \
    CHTML_NAME(Tag)(const string& text) \
        : CParent(sm_TagName, text) \
        { Init(); } \
    CHTML_NAME(Tag)(CNCBINode* node) \
        : CParent(sm_TagName, node) \
        { Init(); } \
    ~CHTML_NAME(Tag)(void)


#define DECLARE_HTML_ELEMENT_TYPES(Parent) \
    typedef Parent CParent; \
    static const char sm_TagName[]


#define DECLARE_HTML_ELEMENT_COMMON(Tag, Parent) \
    DECLARE_HTML_ELEMENT_TYPES(Parent); \
public: \
    DECLARE_HTML_ELEMENT_CONSTRUCTORS(Tag, Parent)


#define DECLARE_HTML_ELEMENT_COMMON_WITH_INIT(Tag, Parent) \
    DECLARE_HTML_ELEMENT_TYPES(Parent); \
public: \
    DECLARE_HTML_ELEMENT_CONSTRUCTORS_WITH_INIT(Tag, Parent)


#define DECLARE_HTML_ELEMENT(Tag, Parent) \
class CHTML_NAME(Tag) : public Parent \
{ \
    DECLARE_HTML_ELEMENT_COMMON(Tag, Parent); \
}


// Macro for declare special chars

#define DECLARE_HTML_SPECIAL_CHAR(Tag, plain) \
class CHTML_NAME(Tag) : public CHTMLSpecialChar \
{ \
    typedef CHTMLSpecialChar CParent; \
public: \
    CHTML_NAME(Tag)(int count = 1) \
        : CParent(#Tag, plain, count) \
        { } \
    ~CHTML_NAME(Tag)(void) { }; \
}



// event tag handler type 
//
// NOTE: Availability of realization event-handlers for some tags
//       stand on from browser's type! Set of event-handlers for tags can 
//       fluctuate in different browsers.

enum EHTML_EH_Attribute {
    //                  work with next HTML-tags (tag's group):
    eHTML_EH_Blur,      //   select, text, textarea
    eHTML_EH_Change,    //   select, text, textarea 
    eHTML_EH_Click,     //   button, checkbox, radio, link, reset, submit
    eHTML_EH_DblClick,  //   
    eHTML_EH_Focus,     //   select, text, textarea  
    eHTML_EH_Load,      //   body, frameset
    eHTML_EH_Unload,    //   body, frameset
    eHTML_EH_MouseDown, //
    eHTML_EH_MouseUp,   //
    eHTML_EH_MouseMove, //
    eHTML_EH_MouseOver, //
    eHTML_EH_MouseOut,  //
    eHTML_EH_Select,    //   text, textarea
    eHTML_EH_Submit,    //   form  
    eHTML_EH_KeyDown,   //
    eHTML_EH_KeyPress,  //
    eHTML_EH_KeyUp      //
};


// base class for html node
class CHTMLNode : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLNode(void)
        { }
    CHTMLNode(const char* tagname)
        : CParent(tagname)
        { }
    CHTMLNode(const char* tagname, const char* text)
        : CParent(tagname)
        {
            AppendPlainText(text);
        }
    CHTMLNode(const char* tagname, const string& text)
        : CParent(tagname)
        {
            AppendPlainText(text);
        }
    CHTMLNode(const char* tagname, CNCBINode* node)
        : CParent(tagname)
        {
            AppendChild(node);
        }
    CHTMLNode(const string& tagname)
        : CParent(tagname)
        { }
    CHTMLNode(const string& tagname, const char* text)
        : CParent(tagname)
        {
            AppendPlainText(text);
        }
    CHTMLNode(const string& tagname, const string& text)
        : CParent(tagname)
        {
            AppendPlainText(text);
        }
    CHTMLNode(const string& tagname, CNCBINode* node)
        : CParent(tagname)
        {
            AppendChild(node);
        }
    ~CHTMLNode(void);

    // convenient way to set some common attributes
    CHTMLNode* SetClass(const string& class_name);
    CHTMLNode* SetStyle(const string& style);
    CHTMLNode* SetId(const string& id);
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
    const string& GetNameAttribute(void) const;
    CHTMLNode* SetTitle(const string& title);
    CHTMLNode* SetAccessKey(char key);

    // convenient way to add CHTMLPlainText or CHTMLText
    void AppendPlainText(const char* text, bool noEncode = false);
    void AppendPlainText(const string& text, bool noEncode = false);
    void AppendHTMLText (const char* text);
    void AppendHTMLText (const string& text);

    // set tag event handler
    void SetEventHandler(const EHTML_EH_Attribute name, const string& value);
};


// <@XXX@> mapping node
class CHTMLTagNode : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLTagNode(const char* tag);
    CHTMLTagNode(const string& tag);
    ~CHTMLTagNode(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
};


// Dual print node
class CHTMLDualNode : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLDualNode(const char* html, const char* plain);
    CHTMLDualNode(CNCBINode* child, const char* plain);
    ~CHTMLDualNode(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

protected:
    string m_Plain;
};


// A text node that contains plain text
class CHTMLPlainText : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLPlainText(const char* text, bool noEncode = false);
    CHTMLPlainText(const string& text, bool noEncode = false);
    ~CHTMLPlainText(void);
    
    const string& GetText(void) const;
    void SetText(const string& text);

    bool NoEncode(void) const
        {
            return m_NoEncode;
        }
    void SetNoEncode(bool noEncode = true)
        {
            m_NoEncode = noEncode;
        }

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

private:
    bool m_NoEncode;
};


// A text node that contains html text with tags and possibly <@TAG@>
class CHTMLText : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLText(const char* text);
    CHTMLText(const string& text);
    ~CHTMLText(void);
    
    const string& GetText(void) const;
    void SetText(const string& text);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);
};


// An <html> tag
class CHTMLOpenElement: public CHTMLNode
{
    typedef CHTMLNode CParent;
public:
    CHTMLOpenElement(const char* tagname)
        : CParent(tagname)
        { }
    CHTMLOpenElement(const char* tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLOpenElement(const char* tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLOpenElement(const char* tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    CHTMLOpenElement(const string& tagname)
        : CParent(tagname)
        { }
    CHTMLOpenElement(const string& tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLOpenElement(const string& tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLOpenElement(const string& tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    ~CHTMLOpenElement(void);

    // prints tag itself
    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);   
};


// An <html> tag
class CHTMLInlineElement: public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
public:
    CHTMLInlineElement(const char* tagname)
        : CParent(tagname)
        { }
    CHTMLInlineElement(const char* tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLInlineElement(const char* tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLInlineElement(const char* tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    CHTMLInlineElement(const string& tagname)
        : CParent(tagname)
        { }
    CHTMLInlineElement(const string& tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLInlineElement(const string& tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLInlineElement(const string& tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    ~CHTMLInlineElement(void);

    // prints tag close    
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);   
};


// An <html> tag
class CHTMLElement: public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
public:
    CHTMLElement(const char* tagname)
        : CParent(tagname)
        { }
    CHTMLElement(const char* tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLElement(const char* tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLElement(const char* tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    CHTMLElement(const string& tagname)
        : CParent(tagname)
        { }
    CHTMLElement(const string& tagname, const char* text)
        : CParent(tagname, text)
        { }
    CHTMLElement(const string& tagname, const string& text)
        : CParent(tagname, text)
        { }
    CHTMLElement(const string& tagname, CNCBINode* node)
        : CParent(tagname, node)
        { }
    ~CHTMLElement(void);

    // prints tag close    
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);   
};


class CHTMLComment : public CHTMLNode
{
    typedef CHTMLNode CParent;
public:
    CHTMLComment(void)
        {
        }
    CHTMLComment(const char* text)
        {
            AppendPlainText(text);
        }
    CHTMLComment(const string& text)
        {
            AppendPlainText(text);
        }
    CHTMLComment(CNCBINode* node)
        {
            AppendChild(node);
        }
    ~CHTMLComment(void);

    virtual CNcbiOstream& Print(CNcbiOstream& out, TMode mode = eHTML);
    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);
};


class CHTMLListElement : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTMLListElement(const char* tagname, bool compact = false)
        : CParent(tagname)
        {
            if ( compact )
                SetCompact();
        }
    CHTMLListElement(const char* tagname, const char* type,
                     bool compact = false)
        : CParent(tagname)
        {
            SetType(type);
            if ( compact )
                SetCompact();
        }
    CHTMLListElement(const char* tagname, const string& type,
                     bool compact = false)
        : CParent(tagname)
        {
            SetType(type);
            if ( compact )
                SetCompact();
        }
    ~CHTMLListElement(void);

    CHTMLListElement* AppendItem(const char* text);
    CHTMLListElement* AppendItem(const string& text);
    CHTMLListElement* AppendItem(CNCBINode* node);

    CHTMLListElement* SetType(const char* type);
    CHTMLListElement* SetType(const string& type);
    CHTMLListElement* SetCompact(void);
};


// An HTML special char
class CHTMLSpecialChar: public CHTMLDualNode
{
    typedef CHTMLDualNode CParent;
public:
    // the "html" argument will be automagically wrapped into '&...;', e.g.
    // 'amp' --> '&amp;'
    CHTMLSpecialChar(const char* html, const char* plain, int count = 1);
    ~CHTMLSpecialChar(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

private:
    int m_Count;
};


// the <html> tag
class CHTML_html : public CHTMLElement
{
    // CParent, constructors, destructor
    DECLARE_HTML_ELEMENT_COMMON_WITH_INIT(html, CHTMLElement);
   
    // Enable using popup menus, set URL for popup menu library.
    // If "menu_lib_url" is not defined, then using default URL.
    // NOTE: If we not change value "menu_script_url", namely use default
    //       value for it, then we can skip call this function.
    void EnablePopupMenu(const string& menu_script_url = kEmptyStr);


private:
    // Init members
    void Init(void);
    
    // Print all self childrens (automatic include code for support 
    // popup menus, if it need )
    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

    // Popup menu variables
    string m_PopupMenuLibUrl;
    bool   m_UsePopupMenu;
};



// table classes

class CHTML_table; // table
class CHTML_tr;    // row
class CHTML_tc;    // any cell
class CHTML_th;    // header cell
class CHTML_td;    // data cell
class CHTML_tc_Cache;
class CHTML_tr_Cache;
class CHTML_table_Cache;

class CHTML_tc : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    typedef unsigned TIndex;
    CHTML_tc(const char* tagname)
        : CParent(tagname), m_Parent(0)
        { }
    CHTML_tc(const char* tagname, const char* text)
        : CParent(tagname, text), m_Parent(0)
        { }
    CHTML_tc(const char* tagname, const string& text)
        : CParent(tagname, text), m_Parent(0)
        { }
    CHTML_tc(const char* tagname, CNCBINode* node)
        : CParent(tagname, node), m_Parent(0)
        { }
    ~CHTML_tc(void);

    // type for row and column indexing

    CHTML_tc* SetRowSpan(TIndex span);
    CHTML_tc* SetColSpan(TIndex span);

    void ResetTableCache(void);

protected:
    virtual void DoSetAttribute(const string& name,
                                const string& value, bool optional);

    friend class CHTML_tr;
    friend class CHTML_tc_Cache;

    CHTML_tr* m_Parent;
};

class CHTML_tr : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    typedef unsigned TIndex;

    CHTML_tr(void);
    CHTML_tr(CNCBINode* node);
    CHTML_tr(const string& text);

    void ResetTableCache(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out, TMode mode);

protected:
    virtual void DoAppendChild(CNCBINode* node);
    void AppendCell(CHTML_tc* cell);
    size_t GetTextLength(TMode mode);

    friend class CHTML_table;
    friend class CHTML_tr_Cache;

    CHTML_table* m_Parent;
};

// the <table> tag
class CHTML_table : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    // type for row and column indexing
    typedef unsigned TIndex;
    enum ECellType {
        eAnyCell,
        eDataCell,
        eHeaderCell
    };

    CHTML_table(void);
    ~CHTML_table(void);

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

    class CTableInfo;

    // returns cell, will add rows/columns if needed
    // throws exception if it is not left upper corner of cell
    // also sets current insertion point
    CHTML_tc* Cell(TIndex row, TIndex column, ECellType type = eAnyCell);
    CHTML_tc* Cell(TIndex row, TIndex column, ECellType type,
                   TIndex rowSpan, TIndex colSpan);
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

    void ResetTableCache(void);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);

    // If to print horizontal row separator (affects ePlainText mode only)
    enum ERowPlainSep {
        ePrintRowSep,   // print
        eSkipRowSep     // do not print
    };
    // Set rows and cols separators (affects ePlainText mode only)
    void SetPlainSeparators(const string& col_left     = kEmptyStr,
                            const string& col_middle   = "\t|\t",
                            const string& col_right    = kEmptyStr,
                            const char    row_sep_char = '-',
                            ERowPlainSep  is_row_sep   = eSkipRowSep);

protected:
    TIndex m_CurrentRow, m_CurrentCol;
    mutable auto_ptr<CHTML_table_Cache> m_Cache;
    CHTML_table_Cache& GetCache(void) const;
    friend class CHTML_table_Cache;
    friend class CHTML_tr;

    virtual void DoAppendChild(CNCBINode* node);
    void AppendRow(CHTML_tr* row);

    string       m_ColSepL, m_ColSepM, m_ColSepR;
    char         m_RowSepChar;
    ERowPlainSep m_IsRowSep;
};

// the <form> tag
class CHTML_form : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    enum EMethod {
        eGet,
        ePost,
        ePostData
    };

    CHTML_form(void);
    CHTML_form(const string& url, EMethod method = eGet);
    CHTML_form(const string& url, CNCBINode* node, EMethod method = eGet);
    ~CHTML_form(void);

    void Init(const string& url, EMethod method = eGet);

    void AddHidden(const string& name, const string& value);
    void AddHidden(const string& name, int value);
};

// the <legend> tag
class CHTML_legend : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_legend(const string& legend);
    CHTML_legend(CHTMLNode* legend);
    ~CHTML_legend(void);
};

// the <fieldset> tag
class CHTML_fieldset : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_fieldset(void);
    CHTML_fieldset(const string& legend);
    CHTML_fieldset(CHTML_legend* legend);
    ~CHTML_fieldset(void);
};

// the <label> tag
class CHTML_label : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
public:
    CHTML_label(const string& text);
    CHTML_label(const string& text, const string& idRef);
    ~CHTML_label(void);

    void SetFor(const string& idRef);
};

// the <textarea> tag
class CHTML_textarea : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_textarea(const string& name, int cols, int rows);
    CHTML_textarea(const string& name, int cols, int rows,
                   const string& value);
    ~CHTML_textarea(void);
};

// <input> tag
class CHTML_input : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
public:
    CHTML_input(const char* type, const string& name);
    ~CHTML_input(void);
};

// <input type=checkbox> tag
class CHTML_checkbox : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_checkbox(const string& name);
    CHTML_checkbox(const string& name, bool checked,
                   const string& description = kEmptyStr);
    CHTML_checkbox(const string& name, const string& value);
    CHTML_checkbox(const string& name, const string& value,
                   bool checked, const string& description = kEmptyStr);
    ~CHTML_checkbox(void);
};

// <input type=hidden> tag
class CHTML_hidden : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_hidden(const string& name, const string& value);
    CHTML_hidden(const string& name, int value);
    ~CHTML_hidden(void);
};

// <input type=image> tag
class CHTML_image : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_image(const string& name, const string& src, 
                const string& alt = kEmptyStr);
    CHTML_image(const string& name, const string& src, int border,
                const string& alt = kEmptyStr);
    ~CHTML_image(void);
};

// <input type=radio> tag
class CHTML_radio : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_radio(const string& name, const string& value);
    CHTML_radio(const string& name, const string& value,
                bool checked, const string& description = kEmptyStr);
    ~CHTML_radio(void);
};

// <input type=text> tag
class CHTML_reset : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_reset(const string& label = kEmptyStr);
    ~CHTML_reset(void);
};

// <input type=submit> tag
class CHTML_submit : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_submit(const string& name);
    CHTML_submit(const string& name, const string& label);
    ~CHTML_submit(void);
};

// the <button> tag
/*
  commented out because it's not supported in most browsers
class CHTML_button : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    enum EButtonType {
        eSubmit,
        eReset,
        eButton
    };
    CHTML_button(const string& text, EButtonType type);
    CHTML_button(CNCBINode* contents, EButtonType type);
    CHTML_button(const string& text, const string& name,
                 const string& value = kEmptyStr);
    CHTML_button(CNCBINode* contents, const string& name,
                 const string& value = kEmptyStr);
    ~CHTML_button(void);

    CHTML_button* SetType(EButtonType type);
    CHTML_button* SetSubmit(const string& name,
                            const string& value = kEmptyStr);
};
*/

// <input type=text> tag
class CHTML_text : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_text(const string& name,
               const string& value = kEmptyStr);
    CHTML_text(const string& name, int size,
               const string& value = kEmptyStr);
    CHTML_text(const string& name, int size, int maxlength,
               const string& value = kEmptyStr);
    ~CHTML_text(void);
};

// <input type=file> tag
class CHTML_file : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_file(const string& name, const string& value = kEmptyStr);
    ~CHTML_file(void);
};

// <select> tag
class CHTML_select : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_select(const string& name, bool multiple = false);
    CHTML_select(const string& name, int size, bool multiple = false);
    ~CHTML_select(void);

    // return 'this' to allow chained AppendOption
    CHTML_select* AppendOption(const string& value, bool selected = false);
    CHTML_select* AppendOption(const string& value, const char* label,
                               bool selected = false);
    CHTML_select* AppendOption(const string& value, const string& label,
                               bool selected = false);
    CHTML_select* SetMultiple(void);
};

//<option> tag.  rarely used alone.  see <select> tag
class CHTML_option : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_option(const string& value, bool selected = false);
    CHTML_option(const string& value, const char* label,
                 bool selected = false);
    CHTML_option(const string& value, const string& label,
                 bool selected = false);
    ~CHTML_option(void);

    CHTML_option* SetValue(const string& value);
    CHTML_option* SetSelected(void);
};

// the <a> tag
class CHTML_a : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
    static const char sm_TagName[];
public:
    CHTML_a(const string& href);
    CHTML_a(const string& href, const char* text);
    CHTML_a(const string& href, const string& text);
    CHTML_a(const string& href, CNCBINode* node);
    ~CHTML_a(void);

    CHTML_a* SetHref(const string& href);
};

// <br> tag (break)
class CHTML_br : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
    static const char sm_TagName[];
public:
    CHTML_br(void);
    // create "number" of <br> tags
    CHTML_br(int number);
    ~CHTML_br(void);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);
};


class CHTML_img : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
public:
    CHTML_img(const string& url, const string& alt = kEmptyStr);
    CHTML_img(const string& url, int width, int height, 
              const string& alt = kEmptyStr);
    ~CHTML_img(void);
};


// <dl> tag
class CHTML_dl : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_dl(bool compact = false);
    ~CHTML_dl(void);

    // return 'this' to allow chained AppendTerm
    CHTML_dl* AppendTerm(const string& term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(const string& term, const string& definition);
    CHTML_dl* AppendTerm(CNCBINode* term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(CNCBINode* term, const string& definition);

    CHTML_dl* SetCompact(void);
};


class CHTML_ol : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_ol(bool compact = false);
    CHTML_ol(const char* type, bool compact = false);
    CHTML_ol(const string& type, bool compact = false);
    CHTML_ol(int start, bool compact = false);
    CHTML_ol(int start, const char* type, bool compact = false);
    CHTML_ol(int start, const string& type, bool compact = false);
    ~CHTML_ol(void);

    CHTML_ol* SetStart(int start);
};


class CHTML_ul : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_ul(bool compact = false);
    CHTML_ul(const char* type, bool compact = false);
    CHTML_ul(const string& type, bool compact = false);
    ~CHTML_ul(void);
};


class CHTML_dir : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_dir(bool compact = false);
    CHTML_dir(const char* type, bool compact = false);
    CHTML_dir(const string& type, bool compact = false);
    ~CHTML_dir(void);
};


class CHTML_menu : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_menu(bool compact = false);
    CHTML_menu(const char* type, bool compact = false);
    CHTML_menu(const string& type, bool compact = false);
    ~CHTML_menu(void);
};


class CHTML_font : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
    static const char sm_TagName[];
public:
    CHTML_font(void);
    CHTML_font(int size,
               CNCBINode* node = 0);
    CHTML_font(int size,
               const char* text);
    CHTML_font(int size,
               const string& text);
    CHTML_font(int size, bool absolute,
               CNCBINode* node = 0);
    CHTML_font(int size, bool absolute,
               const string& text);
    CHTML_font(int size, bool absolute,
               const char* text);
    CHTML_font(const string& typeface,
               CNCBINode* node = 0);
    CHTML_font(const string& typeface,
               const string& text);
    CHTML_font(const string& typeface,
               const char* text);
    CHTML_font(const string& typeface, int size,
               CNCBINode* node = 0);
    CHTML_font(const string& typeface, int size,
               const string& text);
    CHTML_font(const string& typeface, int size,
               const char* text);
    CHTML_font(const string& typeface, int size, bool absolute,
               CNCBINode* node = 0);
    CHTML_font(const string& typeface, int size, bool absolute,
               const string& text);
    CHTML_font(const string& typeface, int size, bool absolute,
               const char* text);
    ~CHTML_font(void);

    CHTML_font* SetTypeFace(const string& typeface);
    CHTML_font* SetFontSize(int size, bool absolute);
    CHTML_font* SetRelativeSize(int size);
};


class CHTML_basefont : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
    static const char sm_TagName[];
public:
    CHTML_basefont(int size);
    CHTML_basefont(const string& typeface);
    CHTML_basefont(const string& typeface, int size);
    ~CHTML_basefont(void);

    CHTML_basefont* SetTypeFace(const string& typeface);
};


class CHTML_color : public CHTML_font
{
    typedef CHTML_font CParent;
public:
    CHTML_color(const string& color, CNCBINode* node = 0);
    CHTML_color(const string& color, const string& text);
    ~CHTML_color(void);
};


// hr

class CHTML_hr : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
    static const char sm_TagName[];
public:
    CHTML_hr(bool noShade = false);
    CHTML_hr(int size, bool noShade = false);
    CHTML_hr(int size, int width, bool noShade = false);
    CHTML_hr(int size, const string& width, bool noShade = false);
    ~CHTML_hr(void);

    CHTML_hr* SetNoShade(void);
    CHTML_hr* SetNoShade(bool noShade);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);
};


// <meta> tag
class CHTML_meta : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
    static const char sm_TagName[];
public:
    enum EType {
        eName,
        eHttpEquiv
    }; 
    CHTML_meta(EType mtype, const string& var, const string& content);
    ~CHTML_meta(void);
};


// <script> tag
class CHTML_script : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_script(const string& stype);
    CHTML_script(const string& stype, const string& url);
    ~CHTML_script(void);

    CHTML_script* AppendScript(const string& script);
};



DECLARE_HTML_ELEMENT( head,       CHTMLElement);
DECLARE_HTML_ELEMENT( body,       CHTMLElement);
DECLARE_HTML_ELEMENT( base,       CHTMLElement);
DECLARE_HTML_ELEMENT( isindex,    CHTMLOpenElement);
DECLARE_HTML_ELEMENT( link,       CHTMLOpenElement);
DECLARE_HTML_ELEMENT( noscript,   CHTMLElement);
DECLARE_HTML_ELEMENT( object,     CHTMLElement);
DECLARE_HTML_ELEMENT( style,      CHTMLElement);
DECLARE_HTML_ELEMENT( title,      CHTMLElement);
DECLARE_HTML_ELEMENT( address,    CHTMLElement);
DECLARE_HTML_ELEMENT( blockquote, CHTMLElement);
DECLARE_HTML_ELEMENT( center,     CHTMLElement);
DECLARE_HTML_ELEMENT( div,        CHTMLElement);
DECLARE_HTML_ELEMENT( h1,         CHTMLElement);
DECLARE_HTML_ELEMENT( h2,         CHTMLElement);
DECLARE_HTML_ELEMENT( h3,         CHTMLElement);
DECLARE_HTML_ELEMENT( h4,         CHTMLElement);
DECLARE_HTML_ELEMENT( h5,         CHTMLElement);
DECLARE_HTML_ELEMENT( h6,         CHTMLElement);
DECLARE_HTML_ELEMENT( p,          CHTMLElement);
DECLARE_HTML_ELEMENT( pnop,       CHTMLOpenElement);
DECLARE_HTML_ELEMENT( pre,        CHTMLElement);
DECLARE_HTML_ELEMENT( dt,         CHTMLElement);
DECLARE_HTML_ELEMENT( dd,         CHTMLElement);
DECLARE_HTML_ELEMENT( li,         CHTMLElement);
DECLARE_HTML_ELEMENT( caption,    CHTMLElement);
DECLARE_HTML_ELEMENT( col,        CHTMLElement);
DECLARE_HTML_ELEMENT( colgroup,   CHTMLElement);
DECLARE_HTML_ELEMENT( thead,      CHTMLElement);
DECLARE_HTML_ELEMENT( tbody,      CHTMLElement);
DECLARE_HTML_ELEMENT( tfoot,      CHTMLElement);
DECLARE_HTML_ELEMENT( th,         CHTML_tc);
DECLARE_HTML_ELEMENT( td,         CHTML_tc);
DECLARE_HTML_ELEMENT( applet,     CHTMLElement);
DECLARE_HTML_ELEMENT( param,      CHTMLOpenElement);
DECLARE_HTML_ELEMENT( cite,       CHTMLInlineElement);
DECLARE_HTML_ELEMENT( code,       CHTMLInlineElement);
DECLARE_HTML_ELEMENT( dfn,        CHTMLElement);
DECLARE_HTML_ELEMENT( em,         CHTMLInlineElement);
DECLARE_HTML_ELEMENT( kbd,        CHTMLInlineElement);
DECLARE_HTML_ELEMENT( samp,       CHTMLElement);
DECLARE_HTML_ELEMENT( strike,     CHTMLInlineElement);
DECLARE_HTML_ELEMENT( strong,     CHTMLInlineElement);
DECLARE_HTML_ELEMENT( var,        CHTMLInlineElement);
DECLARE_HTML_ELEMENT( b,          CHTMLInlineElement);
DECLARE_HTML_ELEMENT( big,        CHTMLInlineElement);
DECLARE_HTML_ELEMENT( i,          CHTMLInlineElement);
DECLARE_HTML_ELEMENT( s,          CHTMLInlineElement);
DECLARE_HTML_ELEMENT( small,      CHTMLInlineElement);
DECLARE_HTML_ELEMENT( sub,        CHTMLInlineElement);
DECLARE_HTML_ELEMENT( sup,        CHTMLInlineElement);
DECLARE_HTML_ELEMENT( tt,         CHTMLInlineElement);
DECLARE_HTML_ELEMENT( u,          CHTMLInlineElement);
DECLARE_HTML_ELEMENT( blink,      CHTMLInlineElement);
DECLARE_HTML_ELEMENT( map,        CHTMLElement);
DECLARE_HTML_ELEMENT( area,       CHTMLElement);

DECLARE_HTML_SPECIAL_CHAR( nbsp, " ");
DECLARE_HTML_SPECIAL_CHAR( gt,   ">");
DECLARE_HTML_SPECIAL_CHAR( lt,   "<");
DECLARE_HTML_SPECIAL_CHAR( quot, "\"");
DECLARE_HTML_SPECIAL_CHAR( amp,  "&");
DECLARE_HTML_SPECIAL_CHAR( copy, "(c)");
DECLARE_HTML_SPECIAL_CHAR( reg,  "(r)");


#include <html/html.inl>

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.55  2002/01/17 23:39:35  ivanov
 * Added means to print HTML tables in plain text mode
 *
 * Revision 1.54  2001/08/14 16:51:33  ivanov
 * Change mean for init JavaScript popup menu & add it to HTML document.
 * Remove early redefined classes for tags HEAD and BODY.
 *
 * Revision 1.53  2001/07/16 19:45:22  ivanov
 * Changed default value for JS menu lib path in CHTML_html::InitPopupMenus().
 *
 * Revision 1.52  2001/07/16 13:54:42  ivanov
 * Added support JavaScript popups menu (jsmenu.[ch]pp)
 *
 * Revision 1.51  2001/06/08 19:01:41  ivanov
 * Added base classes: CHTMLDualNode, CHTMLSpecialChar
 *     (and based on it: CHTML_nbsp, _gt, _lt, _quot, _amp, _copy, _reg)
 * Added realization for tags <meta> (CHTML_meta) and <script> (CHTML_script)
 * Changed base class for tags LINK, PARAM, ISINDEX -> CHTMLOpenElement
 * Added tags: OBJECT, NOSCRIPT
 * Added attribute "alt" for CHTML_img
 * Added CHTMLComment::Print() for disable print html-comments in plaintext mode
 *
 * Revision 1.50  2001/06/05 15:36:10  ivanov
 * Added attribute "alt" to CHTML_image
 *
 * Revision 1.49  2001/05/17 14:55:24  lavr
 * Typos corrected
 *
 * Revision 1.48  2000/10/18 13:25:46  vasilche
 * Added missing constructors to CHTML_font.
 *
 * Revision 1.47  2000/09/27 14:11:10  vasilche
 * Newline '\n' will not be generated after tags LABEL, A, FONT, CITE, CODE, EM,
 * KBD, STRIKE STRONG, VAR, B, BIG, I, S, SMALL, SUB, SUP, TT, U and BLINK.
 *
 * Revision 1.46  2000/08/15 19:40:16  vasilche
 * Added CHTML_label::SetFor() method for setting HTML attribute FOR.
 *
 * Revision 1.45  2000/07/25 15:26:00  vasilche
 * Added newline symbols before table and after each table row in text mode.
 *
 * Revision 1.44  2000/07/18 19:08:48  vasilche
 * Fixed uninitialized members.
 * Fixed NextCell to advance to next cell.
 *
 * Revision 1.43  2000/07/18 17:21:34  vasilche
 * Added possibility to force output of empty attribute value.
 * Added caching to CHTML_table, now large tables work much faster.
 * Changed algorithm of emitting EOL symbols in html output.
 *
 * Revision 1.42  2000/07/12 16:37:37  vasilche
 * Added new HTML4 tags: LABEL, BUTTON, FIELDSET, LEGEND.
 * Added methods for setting common attributes: STYLE, ID, TITLE, ACCESSKEY.
 *
 * Revision 1.41  1999/12/28 21:01:03  vasilche
 * Fixed conflict on MS VC between bool and const string& arguments by
 * adding const char* argument.
 *
 * Revision 1.40  1999/12/28 18:55:28  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.39  1999/10/29 18:28:53  vakatov
 * [MSVC]  bool vs. const string& arg confusion
 *
 * Revision 1.38  1999/10/28 13:40:29  vasilche
 * Added reference counters to CNCBINode.
 *
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
 * CAsnWriteNode made simple node. Use CHTML_pre explicitly.
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

#endif  /* HTML__HTNL__HPP */
