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
 * File Description:   JavaScript popup menu support
 *
 */

#include <corelib/ncbistd.hpp>
#include <html/node.hpp>


BEGIN_NCBI_SCOPE


//===========================================================================
//
// Class for support JavaScript popup menu's
//
// NOTE: For successful work menu in HTML pages need:
//    - file with popup menu JavaScript library ("ncbi_menu*.js" or
//      "popupmenu2*.js")
//      (by default using menu with URL, defined in constant 
//      "kJSMenuDefaultURL*");
//    - define menus and add its to HTML page (AppendChild()).
//    - call EnablePopupMenu() (in CHTML_html or CHTMLPage).
// 
// NOTE: We must add menus to a BODY only, otherwise menu not will be work.
//
//===========================================================================

// Popup menu attribute   
// if attribute not define for menu with function SetAttribute(), 
// then it have some default value
//
// NOTE: All attributes have effect only for specified menu type
//       (CHTMLPopupMenu::EType). Otherwise it will be ignored.
//
enum EHTML_PM_Attribute {
    //                               Using by type     Value example
    eHTML_PM_enableTracker,          // eSmith         "true"
    eHTML_PM_disableHide,            // eSmith         "false"
    eHTML_PM_fontSize,               // eSmith         "14"
    eHTML_PM_fontWeigh,              // eSmith         "plain"
    eHTML_PM_fontFamily,             // eSmith         "arial,helvetica"
    eHTML_PM_fontColor,              // eSmith         "black"
    eHTML_PM_fontColorHilite,        // eSmith         "#ffffff"
    eHTML_PM_menuBorder,             // eSmith         "1"
    eHTML_PM_menuItemBorder,         // eSmith         "0"
    eHTML_PM_menuItemBgColor,        // eSmith         "#cccccc"
    eHTML_PM_menuLiteBgColor,        // eSmith         "white"
    eHTML_PM_menuBorderBgColor,      // eSmith         "#777777"
    eHTML_PM_menuHiliteBgColor,      // eSmith         "#000084"
    eHTML_PM_menuContainerBgColor,   // eSmith         "#cccccc"
    eHTML_PM_childMenuIcon,          // eSmith         "images/arrows.gif"
    eHTML_PM_childMenuIconHilite,    // eSmith         "images/arrows2.gif"
    eHTML_PM_bgColor,                // eSmith,eKurdin "#555555"
    eHTML_PM_titleColor,             // eKurdin        "#FFFFFF"
    eHTML_PM_borderColor,            // eKurdin        "black"
    eHTML_PM_alignH,                 // eKurdin        "left" or "right"
    eHTML_PM_alignV                  // eKurdin        "bottom" or "top"
};


class CHTMLPopupMenu : public CNCBINode
{
    typedef CNCBINode CParent;
    friend class CHTMLPage;
    friend class CHTMLNode;

public:

    // Popup menu type
    enum EType {
        eSmith,             // Smith's menu (ncbi_menu*.js)
        eKurdin,            // Sergey Kurdin's menu (popupmenu2.js)
        ePMFirst = eSmith,
        ePMLast  = eKurdin
    };

    // Construct menu with name "name" (JavaScript variable name)
    CHTMLPopupMenu(const string& name, EType type = eSmith);
    virtual ~CHTMLPopupMenu(void);

    // Get menu name
    string GetName(void) const;
    // Get menu type
    EType GetType(void) const;


    // Add new item to current menu
    //
    // NOTE: action - can be also URL type like "http://..."
    // NOTE: Parameters have some restrictions according to menu type:
    //       title  - can be text or HTML-code (for eSmith menu type only)
    //       color  - will be ignored for eKurdin menu type

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
    // NOTE: do nothing for eKurdin menu type
    void AddSeparator(void); 

    // Set menu attribute
    void SetAttribute(EHTML_PM_Attribute attribute, const string& value);

    // Get JavaScript code for menu call
    string ShowMenu(void) const;

    // Get HTML code for insert into the end of the HEAD and BODY blocks.
    // If "menu_lib_url" is not defined, then using default URL.
    static string GetCodeHead(EType type = eSmith,
                              const string& menu_lib_url = kEmptyStr);
    static string GetCodeBody(EType type = eSmith,
                              bool use_dynamic_menu = false);
    // Get HTML code for insert into the end of the <BODY ...> tag.
    static string GetCodeBodyTagHandler(EType type);
    static string GetCodeBodyTagAction(EType type);

private:
    // Get code for menu items in text format (JS function inside code)
    string GetCodeMenuItems(void) const;

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
    typedef map<EHTML_PM_Attribute, string> TAttributes;
    string GetMenuAttributeValue(EHTML_PM_Attribute attribute) const;
    string GetMenuAttributeName(EHTML_PM_Attribute attribute) const;

    string       m_Name;   // menu name
    EType        m_Type;   // menu type
    TItems       m_Items;  // menu items
    TAttributes  m_Attrs;  // menu attributes
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.9  2002/12/12 17:20:21  ivanov
 * Renamed GetAttribute() -> GetMenuAttributeValue,
 *         GetAttributeName() -> GetMenuAttributeName().
 *
 * Revision 1.8  2002/12/09 22:12:45  ivanov
 * Added support for Sergey Kurdin's popup menu.
 *
 * Revision 1.7  2002/04/29 15:48:16  ucko
 * Make CHTMLPopupMenu::GetName const for consistency with CNcbiNode (and
 * because there's no reason for it not to be const).
 *
 * Revision 1.6  2002/02/13 20:15:39  ivanov
 * Added support of dynamic popup menus. Changed GetCodeBody().
 *
 * Revision 1.5  2001/11/29 16:05:16  ivanov
 * Changed using menu script name "menu.js" -> "ncbi_menu.js"
 *
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

#endif // JSMENU__HPP
