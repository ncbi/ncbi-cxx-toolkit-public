#ifndef HTML___HTML__HPP
#define HTML___HTML__HPP

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
 */

/// @file html.hpp 
/// HTML classes.
///
/// Defines classes to generate HTML code.


#include <corelib/ncbiobj.hpp>
#include <html/node.hpp>
#include <html/jsmenu.hpp>
#include <html/htmlhelper.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Macro for declare html elements.
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
class NCBI_XHTML_EXPORT CHTML_NAME(Tag) : public Parent \
{ \
    DECLARE_HTML_ELEMENT_COMMON(Tag, Parent); \
}


// Macro for declare special chars.

#define DECLARE_HTML_SPECIAL_CHAR(Tag, plain) \
class NCBI_XHTML_EXPORT CHTML_NAME(Tag) : public CHTMLSpecialChar \
{ \
    typedef CHTMLSpecialChar CParent; \
public: \
    CHTML_NAME(Tag)(int count = 1) \
        : CParent(#Tag, plain, count) \
        { } \
    ~CHTML_NAME(Tag)(void) { }; \
}


// Event tag handler type.
//
// NOTE: Availability of realization event-handlers for some tags
//       stand on from browser's type! Set of event-handlers for tags can 
//       fluctuate in different browsers.

enum EHTML_EH_Attribute {
    //                    work with next HTML-tags (tag's group):
    eHTML_EH_Blur,        //   select, text, textarea
    eHTML_EH_Change,      //   select, text, textarea 
    eHTML_EH_Click,       //   button, checkbox, radio, link, reset, submit
    eHTML_EH_DblClick,    //   
    eHTML_EH_Focus,       //   select, text, textarea  
    eHTML_EH_Load,        //   body, frameset
    eHTML_EH_Unload,      //   body, frameset
    eHTML_EH_MouseDown,   //
    eHTML_EH_MouseUp,     //
    eHTML_EH_MouseMove,   //
    eHTML_EH_MouseOver,   //
    eHTML_EH_MouseOut,    //
    eHTML_EH_Select,      //   text, textarea
    eHTML_EH_Submit,      //   form  
    eHTML_EH_KeyDown,     //
    eHTML_EH_KeyPress,    //
    eHTML_EH_KeyUp        //
};


// Base class for html node.
class NCBI_XHTML_EXPORT CHTMLNode : public CNCBINode
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

    // Convenient way to set some common attributes.
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

    // Convenient way to add CHTMLPlainText or CHTMLText.
    void AppendPlainText(const char*   text, bool noEncode = false);
    void AppendPlainText(const string& text, bool noEncode = false);
    void AppendHTMLText (const char*   text);
    void AppendHTMLText (const string& text);

    // Get event handler name.
    string GetEventHandlerName(const EHTML_EH_Attribute event) const;
    // Set tag event handler.
    void SetEventHandler(const EHTML_EH_Attribute event, const string& value);

    // Attach the specified popup menu to HTML node.
    // Popup menu will be shown when the "event" occurs.
    // NOTES:
    //   2) For eKurdin menu type the parameter "event" cannot be an
    //      eHTML_EH_MouseOut, because it is used to hide the menu.
    //   3) For eKurdinSide menu type the event parameters are not used.
    void AttachPopupMenu(const CHTMLPopupMenu*  menu,
                         EHTML_EH_Attribute     event = eHTML_EH_MouseOver);
};


// <@XXX@> mapping node.
class NCBI_XHTML_EXPORT CHTMLTagNode : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLTagNode(const char* tag);
    CHTMLTagNode(const string& tag);
    ~CHTMLTagNode(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
};


// Dual print node.
class NCBI_XHTML_EXPORT CHTMLDualNode : public CNCBINode
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


// A text node that contains plain text.
class NCBI_XHTML_EXPORT CHTMLPlainText : public CNCBINode
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
    bool   m_NoEncode;
    string m_Text;
};


// A text node that contains html text with tags and possibly <@TAG@>.
class NCBI_XHTML_EXPORT CHTMLText : public CNCBINode
{
    typedef CNCBINode CParent;

public:
    /// Which conversions make before printout a stored text
    ///
    /// NOTE: fDisableBuffering flag can slightly improve output speed,
    /// but it disable some functionality. In this mode tag mapping
    /// don't works in ePlainText mode, also tag stripping works incorrect
    /// if other HTML/XML or similar tags presents inside HTML tags.
    /// By default this flag is temporary enabled for compatibility with
    /// old code using CHTMLText class. In the future it will be disabled
    /// by default.
    enum EFlags {
        fStripHtmlMode    = 1 << 1,   //< Strip text in html mode
        fStripTextMode    = 1 << 2,   //< Strip text in plain text mode
        fStrip            = fStripHtmlMode  | fStripTextMode,
        fNoStrip          = 0,
        fEncodeHtmlMode   = 1 << 3,   //< Encode text in html mode
        fEncodeTextMode   = 1 << 4,   //< Encode text in plain text mode
        fEncode           = fEncodeHtmlMode | fEncodeTextMode,
        fNoEncode         = 0,
        fEnableBuffering  = 0,        //< Enable printout buffering
        fDisableBuffering = 1 << 5,   //< Disable printout buffering

        // Presets
        fCode             = fStripTextMode  | fNoEncode,
        fCodeCompatible   = fStripTextMode  | fNoEncode  |  fDisableBuffering,
        fExample          = fNoStrip        | fEncodeHtmlMode,
        fDefault          = fCodeCompatible  //< Default value
    };
    typedef int TFlags;               //< Bitwise OR of "EFlags"

    CHTMLText(const char*   text, TFlags flags = fDefault);
    CHTMLText(const string& text, TFlags flags = fDefault);
    ~CHTMLText(void);
    
    const string& GetText(void) const;
    void SetText(const string& text);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

private:
    CNcbiOstream& x_PrintBegin(CNcbiOstream& out, TMode mode,
                               const string& s) const;
private:
    string  m_Text;
    TFlags  m_Flags;
};


// HTML tag base class.
class NCBI_XHTML_EXPORT CHTMLOpenElement: public CHTMLNode
{
    typedef CHTMLNode CParent;
public:
    CHTMLOpenElement(const char* tagname)
        : CParent(tagname), m_NoWrap(false)
    { }
    CHTMLOpenElement(const char* tagname, const char* text)
        : CParent(tagname, text), m_NoWrap(false)
    { }
    CHTMLOpenElement(const char* tagname, const string& text)
        : CParent(tagname, text), m_NoWrap(false)
    { }
    CHTMLOpenElement(const char* tagname, CNCBINode* node)
        : CParent(tagname, node), m_NoWrap(false)
    { }
    CHTMLOpenElement(const string& tagname)
        : CParent(tagname), m_NoWrap(false)
    { }
    CHTMLOpenElement(const string& tagname, const char* text)
        : CParent(tagname, text), m_NoWrap(false)
    { }
    CHTMLOpenElement(const string& tagname, const string& text)
        : CParent(tagname, text), m_NoWrap(false)
    { }
    CHTMLOpenElement(const string& tagname, CNCBINode* node)
        : CParent(tagname, node), m_NoWrap(false)
    { }
    ~CHTMLOpenElement(void);

    // Print tag itself.
    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);

    // Set NOWRAP attribute (NOTE: it is depricated in HTML 4.0)
    virtual void SetNoWrap(bool noWrap = true)
        { m_NoWrap = noWrap; }

protected:
    bool m_NoWrap;


};


// HTML inline tag
class NCBI_XHTML_EXPORT CHTMLInlineElement: public CHTMLOpenElement
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

    // Print tag close.
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);   
};


// HTML tag
class NCBI_XHTML_EXPORT CHTMLElement: public CHTMLInlineElement
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

    // Print tag close.
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);   
};


// HTML block element.
class NCBI_XHTML_EXPORT CHTMLBlockElement: public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTMLBlockElement(const char* tagname)
        : CParent(tagname)
    { }
    CHTMLBlockElement(const char* tagname, const char* text)
        : CParent(tagname, text)
    { }
    CHTMLBlockElement(const char* tagname, const string& text)
        : CParent(tagname, text)
    { }
    CHTMLBlockElement(const char* tagname, CNCBINode* node)
        : CParent(tagname, node)
    { }
    CHTMLBlockElement(const string& tagname)
        : CParent(tagname)
    { }
    CHTMLBlockElement(const string& tagname, const char* text)
        : CParent(tagname, text)
    { }
    CHTMLBlockElement(const string& tagname, const string& text)
        : CParent(tagname, text)
    { }
    CHTMLBlockElement(const string& tagname, CNCBINode* node)
        : CParent(tagname, node)
    { }
    ~CHTMLBlockElement(void);

    // Close tag.
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);   
};


// HTML comment.

class NCBI_XHTML_EXPORT CHTMLComment : public CHTMLNode
{
    typedef CHTMLNode CParent;
    static const char sm_TagName[];
public:
    CHTMLComment(void)
        : CParent(sm_TagName)
    { }
    CHTMLComment(const char* text)
        : CParent(sm_TagName)
    {
        AppendPlainText(text);
    }
    CHTMLComment(const string& text)
        : CParent(sm_TagName)
    {
        AppendPlainText(text);
    }
    CHTMLComment(CNCBINode* node)
        : CParent(sm_TagName)
    {
        AppendChild(node);
    }
    ~CHTMLComment(void);

    virtual CNcbiOstream& Print(CNcbiOstream& out, TMode mode = eHTML);
    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream &, TMode mode);
};


// <list> tag.
class NCBI_XHTML_EXPORT CHTMLListElement : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTMLListElement(const char* tagname, bool compact = false)
        : CParent(tagname)
    {
        if ( compact ) {
            SetCompact();
        }
    }
    CHTMLListElement(const char* tagname, const char* type,
                     bool compact = false)
        : CParent(tagname)
    {
        SetType(type);
        if ( compact ) {
            SetCompact();
        }
    }
    CHTMLListElement(const char* tagname, const string& type,
                     bool compact = false)
        : CParent(tagname)
    {
        SetType(type);
        if ( compact ) {
            SetCompact();
        }
    }
    ~CHTMLListElement(void);

    CHTMLListElement* AppendItem(const char* text);
    CHTMLListElement* AppendItem(const string& text);
    CHTMLListElement* AppendItem(CNCBINode* node);

    CHTMLListElement* SetType(const char* type);
    CHTMLListElement* SetType(const string& type);
    CHTMLListElement* SetCompact(void);

    CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
};


// HTML special char.
class NCBI_XHTML_EXPORT CHTMLSpecialChar: public CHTMLDualNode
{
    typedef CHTMLDualNode CParent;
public:
    // The "html" argument will be automagically wrapped into '&...;',
    // e.g. 'amp' --> '&amp;'
    CHTMLSpecialChar(const char* html, const char* plain, int count = 1);
    ~CHTMLSpecialChar(void);

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
private:
    string m_Html;
    int    m_Count;
};


// <html> tag
class NCBI_XHTML_EXPORT CHTML_html : public CHTMLElement
{
    // CParent, constructors, destructor.
    DECLARE_HTML_ELEMENT_COMMON_WITH_INIT(html, CHTMLElement);
   
    // Enable using popup menus, set URL for popup menu library.
    // If "menu_lib_url" is not defined, then using default URL.
    // use_dynamic_menu - enable/disable using dynamic popup menus 
    // (default it is disabled).
    // NOTE: 1) If we not change value "menu_script_url", namely use default
    //          value for it, then we can skip call this function.
    //       2) Dynamic menu work only in new browsers. They use one container
    //          for all menus instead of separately container for each menu in 
    //          nondynamic mode. This parameter have effect only with eSmith
    //          menu type.
    void EnablePopupMenu(CHTMLPopupMenu::EType type = CHTMLPopupMenu::eSmith,
                         const string&         menu_script_url  = kEmptyStr,
                         bool                  use_dynamic_menu = false);
private:
    // Init members.
    void Init(void);
    
    // Print all self childrens (automatic include code for support 
    // popup menus, if it needed).
    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

    // The popup menu info structure.
    struct SPopupMenuInfo {
        SPopupMenuInfo() 
            : m_UseDynamicMenu(false)
        { };
        SPopupMenuInfo(const string& url, bool use_dynamic_menu)
            : m_Url(url), m_UseDynamicMenu(use_dynamic_menu)
        { }
        string m_Url;
        bool   m_UseDynamicMenu; // Only for eSmith menu type.
    };
    // The popup menus usage info.
    typedef map<CHTMLPopupMenu::EType, SPopupMenuInfo> TPopupMenus;
    TPopupMenus m_PopupMenus;
};



// Table classes.

class CHTML_table;   // Table
class CHTML_tr;      // Row
class CHTML_tc;      // Any cell
class CHTML_th;      // Header cell
class CHTML_td;      // Data cell
class CHTML_tc_Cache;
class CHTML_tr_Cache;
class CHTML_table_Cache;


class NCBI_XHTML_EXPORT CHTML_tc : public CHTMLElement
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

    // Type for row and column indexing.
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


class NCBI_XHTML_EXPORT CHTML_tr : public CHTMLElement
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


// <table> tag.

class NCBI_XHTML_EXPORT CHTML_table : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    // Type for row and column indexing.
    typedef unsigned TIndex;
    enum ECellType {
        eAnyCell,
        eDataCell,
        eHeaderCell
    };

    CHTML_table(void);
    ~CHTML_table(void);

    // Return row, will add rows if needed.
    // Throws exception if it is not left upper corner of cell.
    CHTML_tr* Row(TIndex row);

    // Get/set current insertion point.
    void SetCurrentCell(TIndex row, TIndex col)
        { m_CurrentRow = row; m_CurrentCol = col; }
    TIndex GetCurrentRow(void) const
        { return m_CurrentRow; }
    TIndex GetCurrentCol(void) const
        { return m_CurrentCol; }

    class CTableInfo;

    // Return cell, will add rows/columns if needed.
    // Throws exception if it is not left upper corner of cell
    // also sets current insertion point.
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

    // Check table contents for validaty, throws exception if invalid.
    void CheckTable(void) const;
    // Return width of table in columns. Should call CheckTable before.
    TIndex CalculateNumberOfColumns(void) const;
    TIndex CalculateNumberOfRows(void) const;

    // Return cell of insertion.
    CHTML_tc* InsertAt(TIndex row, TIndex column, CNCBINode* node);
    CHTML_tc* InsertAt(TIndex row, TIndex column, const string& text);
    CHTML_tc* InsertTextAt(TIndex row, TIndex column, const string& text);

    CHTML_tc* InsertNextCell(CNCBINode* node);
    CHTML_tc* InsertNextCell(const string& text);

    CHTML_tc* InsertNextRowCell(CNCBINode* node);
    CHTML_tc* InsertNextRowCell(const string& text);

    CHTML_table* SetCellSpacing(int spacing);
    CHTML_table* SetCellPadding(int padding);

    CHTML_table* SetColumnWidth(TIndex column, int width) ;
    CHTML_table* SetColumnWidth(TIndex column, const string& width);

    void ResetTableCache(void);

    virtual CNcbiOstream& PrintBegin(CNcbiOstream &, TMode mode);

    // If to print horizontal row separator (affects ePlainText mode only).
    enum ERowPlainSep {
        ePrintRowSep,   // print
        eSkipRowSep     // do not print
    };
    // Set rows and cols separators (affects ePlainText mode only).
    void SetPlainSeparators(const string& col_left     = kEmptyStr,
                            const string& col_middle   = " ",
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

    typedef map<size_t,string> TColWidths;
    TColWidths m_ColWidths;
};


class NCBI_XHTML_EXPORT CHTML_tc_Cache
{
public:
    CHTML_tc_Cache(void)
        : m_Used(false), m_Node(0)
    {
        return;
    }

    bool IsUsed(void) const
    {
        return m_Used;
    }

    bool IsNode(void) const
    {
        return m_Node != 0;
    }
    CHTML_tc* GetCellNode(void) const
    {
        return m_Node;
    }

    void SetUsed(void);
    void SetCellNode(CHTML_tc* node);

private:
    bool m_Used;
    CHTML_tc* m_Node;
};


class NCBI_XHTML_EXPORT CHTML_tr_Cache
{
public:
    typedef CHTML_table::TIndex TIndex;

    CHTML_tr_Cache(void)
        : m_Node(0),
          m_CellCount(0), m_CellsSize(0), m_Cells(0), m_FilledCellCount(0)
    {
        return;
    }

    ~CHTML_tr_Cache(void)
    {
        delete[] m_Cells;
    }

    CHTML_tr* GetRowNode(void) const
    {
        return m_Node;
    }

    void SetRowNode(CHTML_tr* rowNode)
    {
        _ASSERT(!m_Node && rowNode);
        m_Node = rowNode;
    }

    TIndex GetCellCount(void) const
    {
        return m_CellCount;
    }

    CHTML_tc_Cache& GetCellCache(TIndex col);

    void AppendCell(CHTML_tr* rowNode, TIndex col,
                    CHTML_tc* cellNode, TIndex colSpan);
    void SetUsedCells(TIndex colBegin, TIndex colEnd);
    void SetUsedCells(CHTML_tc* cellNode, TIndex colBegin, TIndex colEnd);

private:
    CHTML_tr_Cache(const CHTML_tr_Cache&);
    CHTML_tr_Cache& operator=(const CHTML_tr_Cache&);

    CHTML_tr* m_Node;
    TIndex m_CellCount;
    TIndex m_CellsSize;
    CHTML_tc_Cache* m_Cells;
    TIndex m_FilledCellCount;
};


class NCBI_XHTML_EXPORT CHTML_table_Cache
{
public:
    typedef CHTML_table::TIndex TIndex;

    CHTML_table_Cache(CHTML_table* table);
    ~CHTML_table_Cache(void);

    TIndex GetRowCount(void) const
    {
        return m_RowCount;
    }

    CHTML_tr_Cache& GetRowCache(TIndex row);
    CHTML_tr* GetRowNode(TIndex row);
    CHTML_tc* GetCellNode(TIndex row, TIndex col,
                          CHTML_table::ECellType type);
    CHTML_tc* GetCellNode(TIndex row, TIndex col,
                          CHTML_table::ECellType type,
                          TIndex rowSpan, TIndex colSpan);

    void InitRow(TIndex row, CHTML_tr* rowNode);
    void SetUsedCells(TIndex rowBegin, TIndex rowEnd,
                      TIndex colBegin, TIndex colEnd);
private:
    CHTML_table*     m_Node;
    TIndex           m_RowCount;
    TIndex           m_RowsSize;
    CHTML_tr_Cache** m_Rows;
    TIndex           m_FilledRowCount;

    CHTML_table_Cache(const CHTML_table_Cache&);
    CHTML_table_Cache& operator=(const CHTML_table_Cache&);
};


// <form> tag
class NCBI_XHTML_EXPORT CHTML_form : public CHTMLElement
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


// <legend> tag.
class NCBI_XHTML_EXPORT CHTML_legend : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_legend(const string& legend);
    CHTML_legend(CHTMLNode* legend);
    ~CHTML_legend(void);
};


// <fieldset> tag.
class NCBI_XHTML_EXPORT CHTML_fieldset : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_fieldset(void);
    CHTML_fieldset(const string& legend);
    CHTML_fieldset(CHTML_legend* legend);
    ~CHTML_fieldset(void);
};


// <label> tag.
class NCBI_XHTML_EXPORT CHTML_label : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
public:
    CHTML_label(const string& text);
    CHTML_label(const string& text, const string& idRef);
    ~CHTML_label(void);

    void SetFor(const string& idRef);
};


// <textarea> tag.
class NCBI_XHTML_EXPORT CHTML_textarea : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_textarea(const string& name, int cols, int rows);
    CHTML_textarea(const string& name, int cols, int rows,
                   const string& value);
    ~CHTML_textarea(void);
};


// <input> tag.
class NCBI_XHTML_EXPORT CHTML_input : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
public:
    CHTML_input(const char* type, const string& name);
    ~CHTML_input(void);
};


// <input type=checkbox> tag.
class NCBI_XHTML_EXPORT CHTML_checkbox : public CHTML_input
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


// <input type=hidden> tag.
class NCBI_XHTML_EXPORT CHTML_hidden : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_hidden(const string& name, const string& value);
    CHTML_hidden(const string& name, int value);
    ~CHTML_hidden(void);
};


// <input type=image> tag.
class NCBI_XHTML_EXPORT CHTML_image : public CHTML_input
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


// <input type=password> tag.
class NCBI_XHTML_EXPORT CHTML_password : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_password(const string& name,
                   const string& value = kEmptyStr);
    CHTML_password(const string& name, int size,
                   const string& value = kEmptyStr);
    CHTML_password(const string& name, int size, int maxlength,
                   const string& value = kEmptyStr);
    ~CHTML_password(void);
};


// <input type=radio> tag.
class NCBI_XHTML_EXPORT CHTML_radio : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_radio(const string& name, const string& value);
    CHTML_radio(const string& name, const string& value,
                bool checked, const string& description = kEmptyStr);
    ~CHTML_radio(void);
};


// <input type=reset> tag.
class NCBI_XHTML_EXPORT CHTML_reset : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_reset(const string& label = kEmptyStr);
    ~CHTML_reset(void);
};


// <input type=submit> tag.
class NCBI_XHTML_EXPORT CHTML_submit : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_submit(const string& name);
    CHTML_submit(const string& name, const string& label);
    ~CHTML_submit(void);
};


// <button> tag.
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


// <input type=text> tag.
class NCBI_XHTML_EXPORT CHTML_text : public CHTML_input
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


// <input type=file> tag.
class NCBI_XHTML_EXPORT CHTML_file : public CHTML_input
{
    typedef CHTML_input CParent;
    static const char sm_InputType[];
public:
    CHTML_file(const string& name, const string& value = kEmptyStr);
    ~CHTML_file(void);
};


// <select> tag.
class NCBI_XHTML_EXPORT CHTML_select : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_select(const string& name, bool multiple = false);
    CHTML_select(const string& name, int size, bool multiple = false);
    ~CHTML_select(void);

    // Return 'this' to allow chained AppendOption().
    CHTML_select* AppendOption(const string& value, bool selected = false);
    CHTML_select* AppendOption(const string& value, const char* label,
                               bool selected = false);
    CHTML_select* AppendOption(const string& value, const string& label,
                               bool selected = false);
    CHTML_select* SetMultiple(void);
};


// <option> tag. Rarely used alone. See <select> tag.
class NCBI_XHTML_EXPORT CHTML_option : public CHTMLElement
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


// <a> tag.
class NCBI_XHTML_EXPORT CHTML_a : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
    static const char sm_TagName[];
public:
    CHTML_a(void);
    CHTML_a(const string& href);
    CHTML_a(const string& href, const char* text);
    CHTML_a(const string& href, const string& text);
    CHTML_a(const string& href, CNCBINode* node);
    ~CHTML_a(void);

    CHTML_a* SetHref(const string& href);
};


// <br> tag (break).
class NCBI_XHTML_EXPORT CHTML_br : public CHTMLOpenElement
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


// <area> tag.
class NCBI_XHTML_EXPORT CHTML_area : public CHTMLInlineElement
{
    typedef CHTMLInlineElement CParent;
    static const char sm_TagName[];
public:
    // Shape of area region
    enum EShape {
        eRect,      // rectangular region
        eCircle,    // circular region
        ePoly       // polygonal region
    };

    CHTML_area(void);
    CHTML_area(const string& href, int x1, int y1, int x2, int y2,
               const string& alt = kEmptyStr);
    CHTML_area(const string& href, int x, int y, int radius,
               const string& alt = kEmptyStr);
    CHTML_area(const string& href, int coords[], int count,
               const string& alt = kEmptyStr);
    CHTML_area(const string& href, vector<int> coords,
               const string& alt = kEmptyStr);
    CHTML_area(const string& href, list<int> coords,
               const string& alt = kEmptyStr);

    // Destructor
    ~CHTML_area(void);

    CHTML_area* SetHref(const string& href);

    // Define a rectangular region
    CHTML_area* DefineRect(int x1, int y1, int x2, int y2);
    // Define a circular region
    CHTML_area* DefineCircle(int x, int y, int radius);
    // Define a polygonal region
    // Array should contains pairs x1,y1,x2,y2,...
    CHTML_area* DefinePolygon(int coords[], int count);
    CHTML_area* DefinePolygon(vector<int> coords);
    CHTML_area* DefinePolygon(list<int> coords);
};


// <map> tag.
class NCBI_XHTML_EXPORT CHTML_map : public CHTMLElement
{
    typedef CHTMLElement CParent;
public:
    CHTML_map(const string& name);
    ~CHTML_map(void);

    CHTML_map* AddRect   (const string& href, int x1, int y1, int x2, int y2,
                          const string& alt = kEmptyStr);
    CHTML_map* AddCircle (const string& href, int x, int y, int radius,
                          const string& alt = kEmptyStr);
    CHTML_map* AddPolygon(const string& href, int coords[], int count,
                          const string& alt = kEmptyStr);
    CHTML_map* AddPolygon(const string& href, vector<int> coords,
                          const string& alt = kEmptyStr);
    CHTML_map* AddPolygon(const string& href, list<int> coords,
                          const string& alt = kEmptyStr);

    // Add area
    CHTML_map* AddArea(CHTML_area* area);
    CHTML_map* AddArea(CNodeRef& area);
};


// <img> tag.
class NCBI_XHTML_EXPORT CHTML_img : public CHTMLOpenElement
{
    typedef CHTMLOpenElement CParent;
public:
    CHTML_img(const string& url, const string& alt = kEmptyStr);
    CHTML_img(const string& url, int width, int height, 
              const string& alt = kEmptyStr);
    ~CHTML_img(void);
    void UseMap(const string& mapname);
    void UseMap(const CHTML_map* const mapnode);
};


// <dl> tag.
class NCBI_XHTML_EXPORT CHTML_dl : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_dl(bool compact = false);
    ~CHTML_dl(void);

    // Return 'this' to allow chained AppendTerm().
    CHTML_dl* AppendTerm(const string& term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(const string& term, const string& definition);
    CHTML_dl* AppendTerm(CNCBINode* term, CNCBINode* definition = 0);
    CHTML_dl* AppendTerm(CNCBINode* term, const string& definition);

    CHTML_dl* SetCompact(void);
};


// <ol> tag.
class NCBI_XHTML_EXPORT CHTML_ol : public CHTMLListElement
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


// <ul> tag.
class NCBI_XHTML_EXPORT CHTML_ul : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_ul(bool compact = false);
    CHTML_ul(const char* type, bool compact = false);
    CHTML_ul(const string& type, bool compact = false);
    ~CHTML_ul(void);
};


// <dir> tag.
class NCBI_XHTML_EXPORT CHTML_dir : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_dir(bool compact = false);
    CHTML_dir(const char* type, bool compact = false);
    CHTML_dir(const string& type, bool compact = false);
    ~CHTML_dir(void);
};


// <menu> tag.
class NCBI_XHTML_EXPORT CHTML_menu : public CHTMLListElement
{
    typedef CHTMLListElement CParent;
    static const char sm_TagName[];
public:
    CHTML_menu(bool compact = false);
    CHTML_menu(const char* type, bool compact = false);
    CHTML_menu(const string& type, bool compact = false);
    ~CHTML_menu(void);
};


// <font> tag.
class NCBI_XHTML_EXPORT CHTML_font : public CHTMLInlineElement
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


class NCBI_XHTML_EXPORT CHTML_basefont : public CHTMLOpenElement
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


class NCBI_XHTML_EXPORT CHTML_color : public CHTML_font
{
    typedef CHTML_font CParent;
public:
    CHTML_color(const string& color, CNCBINode* node = 0);
    CHTML_color(const string& color, const string& text);
    ~CHTML_color(void);
};


// <hr> tag.
class NCBI_XHTML_EXPORT CHTML_hr : public CHTMLOpenElement
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


// <meta> tag.
class NCBI_XHTML_EXPORT CHTML_meta : public CHTMLOpenElement
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


// <scrpt> tag.
class NCBI_XHTML_EXPORT CHTML_script : public CHTMLElement
{
    typedef CHTMLElement CParent;
    static const char sm_TagName[];
public:
    CHTML_script(const string& stype);
    CHTML_script(const string& stype, const string& url);
    ~CHTML_script(void);

    CHTML_script* AppendScript(const string& script);
};


// Other tags (default implementation).

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
DECLARE_HTML_ELEMENT( blockquote, CHTMLBlockElement);
DECLARE_HTML_ELEMENT( center,     CHTMLElement);
DECLARE_HTML_ELEMENT( div,        CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h1,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h2,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h3,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h4,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h5,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( h6,         CHTMLBlockElement);
DECLARE_HTML_ELEMENT( p,          CHTMLBlockElement);
DECLARE_HTML_ELEMENT( pnop,       CHTMLOpenElement);
DECLARE_HTML_ELEMENT( pre,        CHTMLBlockElement);
DECLARE_HTML_ELEMENT( dt,         CHTMLElement);
DECLARE_HTML_ELEMENT( dd,         CHTMLElement);
DECLARE_HTML_ELEMENT( li,         CHTMLBlockElement);
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
DECLARE_HTML_ELEMENT( span,       CHTMLInlineElement);

DECLARE_HTML_SPECIAL_CHAR( nbsp, " ");
DECLARE_HTML_SPECIAL_CHAR( gt,   ">");
DECLARE_HTML_SPECIAL_CHAR( lt,   "<");
DECLARE_HTML_SPECIAL_CHAR( quot, "\"");
DECLARE_HTML_SPECIAL_CHAR( amp,  "&");
DECLARE_HTML_SPECIAL_CHAR( copy, "(c)");
DECLARE_HTML_SPECIAL_CHAR( reg,  "(r)");


#include <html/html.inl>


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.83  2005/03/28 18:29:07  jcherry
 * Added export specifiers
 *
 * Revision 1.82  2004/12/27 14:27:32  ivanov
 * CHTML_map:: added AddArea() method
 *
 * Revision 1.81  2004/12/13 13:49:40  ivanov
 * Added CHTML_map and CHTML_area classes
 *
 * Revision 1.80  2004/10/27 18:25:17  ivanov
 * CHTMLText : Added flag fDisableBuffering to disable internal buffering
 * at the cost of loss some functionality relative to tag mapping.
 * By default buffering is disabled now. But default can be changed
 * in the future.
 *
 * Revision 1.79  2004/10/21 17:43:32  ivanov
 * CHTMLText: added EFlag type and flag parameter to constructors
 *
 * Revision 1.78  2004/08/13 16:48:27  ivanov
 * Added class CHTML_password (HTML tag <input type=password>)
 *
 * Revision 1.77  2004/07/20 20:12:10  ucko
 * Move declarations of CHTML_t*_Cache from html.cpp, as required by
 * some auto_ptr<> implementations.
 *
 * Revision 1.76  2004/07/20 16:36:37  ivanov
 * + CHTML_table::SetColumnWidth
 *
 * Revision 1.75  2004/04/05 15:50:49  ivanov
 * Cosmetic changes
 *
 * Revision 1.74  2004/04/01 14:14:01  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.73  2004/02/03 19:45:41  ivanov
 * Binded dummy names for the unnamed nodes
 *
 * Revision 1.72  2004/02/02 14:08:06  ivanov
 * Added export specifier to the macro for declare HTML classes
 *
 * Revision 1.71  2004/01/26 16:26:42  ivanov
 * Added NOWRAP attribute support
 *
 * Revision 1.70  2003/12/31 19:00:45  ivanov
 * Added default constructor for CHTML_a
 *
 * Revision 1.69  2003/12/02 14:18:52  ivanov
 * AttachPopupMenu() comment changes
 *
 * Revision 1.68  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.67  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.66  2003/11/03 14:47:39  ivanov
 * Some formal code rearrangement
 *
 * Revision 1.65  2003/10/01 15:54:31  ivanov
 * Added comments for AttachPopupMenu
 *
 * Revision 1.64  2003/04/25 13:45:26  siyan
 * Added doxygen groupings
 *
 * Revision 1.63  2003/02/14 16:18:29  ucko
 * Override CHTMLListItem::PrintChildren (so we can indent them in text
 * mode); make <li> a block element for proper text display.
 *
 * Revision 1.62  2002/12/24 14:56:20  ivanov
 * Fix for R1.76:  HTML classes for tags <h1-6>, <p>, <div>. <pre>, <blockquote>
 * now inherits from CHTMLBlockElement (not CHTMElement as before)
 *
 * Revision 1.61  2002/12/20 19:19:30  ivanov
 * Added SPAN tag support
 *
 * Revision 1.60  2002/12/09 22:12:25  ivanov
 * Added support for Sergey Kurdin's popup menu.
 * Added CHTMLNode::AttachPopupMenu().
 *
 * Revision 1.59  2002/09/25 01:24:29  dicuccio
 * Added CHTMLHelper::StripTags() - strips HTML comments and tags from any
 * string
 *
 * Revision 1.58  2002/02/13 20:17:06  ivanov
 * Added support of dynamic popup menus. Changed EnablePopupMenu().
 *
 * Revision 1.57  2002/01/29 20:00:18  ivanov
 * (plain text) CHTML_table:: set def. medium sep. to " " instead of "\t"
 *
 * Revision 1.56  2002/01/28 17:56:43  vakatov
 * (plain text) CHTML_table:: set def. medium sep. to "\t" instead of "\t|\t"
 *
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
 * Changes for SaveAsText: all Print() methods get mode parameter that can be 
 * HTML or PlainText
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

#endif  /* HTML___HTML__HPP */
