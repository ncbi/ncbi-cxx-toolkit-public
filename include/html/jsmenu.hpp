#ifndef JSMENU__HPP
#define JSMENU__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *   JavaScript menu support (Smith's menu)
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.4  2001/10/15 23:16:19  vakatov
 * + AddItem(const char* title, ...) to avoid "string/CNCBINode" ambiguity
 *
 * Revision 1.3  2001/08/15 19:43:30  ivanov
 * Added AddMenuItem( node, ...)
 *
 * Revision 1.2  2001/08/14 16:52:47  ivanov
 * Changed parent class for CHTMLPopupMenu.
 * Changed means for init JavaScript popup menu & add it to HTML document.
 *
 * Revision 1.1  2001/07/16 13:45:33  ivanov
 * Initialization
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <html/node.hpp>


BEGIN_NCBI_SCOPE


//===========================================================================
//
// Class for support JavaScript popup menu (Smith's menu)
//
// NOTE: For successful work menu in HTML pages need:
//       - file with popup menu JavaScript library "menu.js" (by default 
//         using menu with URL, defined in constant "kJSMenuDefaultURL");
//       - define menus and add its to HTML page (AppendChild()).
//       - call EnablePopupMenu() (in CHTML_html or CHTMLPage).
// 
// NODE 1: We must add menus to BODY only, otherwise menu not will be work.
//
//===========================================================================

// Popup menu attribute   
// if attribute not define for menu with function SetAttribute(), 
// then it have some default value
//
enum EHTML_PM_Attribute {
    //                               Value example
    eHTML_PM_enableTracker,          // true
    eHTML_PM_disableHide,            // false
    eHTML_PM_fontSize,               // 14
    eHTML_PM_fontWeigh,              // plain
    eHTML_PM_fontFamily,             // arial,helvetica,sans-serif
    eHTML_PM_fontColor,              // black
    eHTML_PM_fontColorHilite,        // #ffffff
    eHTML_PM_bgColor,                // #555555
    eHTML_PM_menuBorder,             // 1
    eHTML_PM_menuItemBorder,         // 0
    eHTML_PM_menuItemBgColor,        // #cccccc
    eHTML_PM_menuLiteBgColor,        // white
    eHTML_PM_menuBorderBgColor,      // #777777
    eHTML_PM_menuHiliteBgColor,      // #000084
    eHTML_PM_menuContainerBgColor,   // #cccccc
    eHTML_PM_childMenuIcon,          // images/arrows.gif
    eHTML_PM_childMenuIconHilite     // images/arrows2.gif
};



class CHTMLPopupMenu : public CNCBINode
{
    typedef CNCBINode CParent;
    friend class CHTMLPage;

public:
    // Construct menu with name "name" (JavaScript variable name)
    CHTMLPopupMenu(const string& name);
    virtual ~CHTMLPopupMenu(void);

    // Get menu name
    string GetName(void);

    // Add new item to current menu
    void AddItem(const string& title,                  // Text or HTML-code
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    void AddItem(const char*   title,                  // Text or HTML-code
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    // NOTE: The "node" will convert to string inside this function, so
    //       the "node"'s Print() method for must not change "node" structure.
    void AddItem(CNCBINode& node,
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    // Add item's separator
    void AddSeparator(void); 

    // Set menu attribute
    void SetAttribute(EHTML_PM_Attribute attribute, const string& value);

    // Get JavaScript code for menu call
    string ShowMenu(void);

    // Get HTML code for insert in the HEAD and BODY tags.
    // If "menu_lib_url" is not defined, then using default URL.
    static string GetCodeHead(const string& menu_lib_url = kEmptyStr);
    static string GetCodeBody(void);

private:
    // Get code for menu items in text format (JS function inside code)
    string GetCodeMenuItems(void);
    
    // Print menu
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

    // Menu item type
    struct SItem {
        // menu item
        SItem(const string& title, const string& action, const string& color,
              const string& mouseover, const string& mouseout);
        // separator
        SItem(void);
        string title;      // menu item title
        string action;     // JS action on press item
        string color;      // ? (not used in JSMenu script)
        string mouseover;  // JS action on mouse over event for item
        string mouseout;   // JS action on mouse out event for item
    };
    typedef list<SItem> TItems;

    // Menu attribute type
    struct SAttribute {
        SAttribute(EHTML_PM_Attribute name, const string& value);
        EHTML_PM_Attribute name;   // attribute name
        string             value;  // attribute value
    };
    typedef list<SAttribute> TAttributes;

    string       m_Name;   // menu name
    TItems       m_Items;  // menu items
    TAttributes  m_Attrs;  // menu attributes
};


END_NCBI_SCOPE

#endif // JSMENU__HPP
