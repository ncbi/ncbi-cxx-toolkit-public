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
// NOTE: For successful work menu in HTML pages file "menu.js" need.  
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
    eHTML_PM_fontFamily,             // arial,helvetica,espy,sans-serif
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


class CHTMLPopupMenu : public CObject
{
    friend class CHTML_head;
    friend class CHTML_body;

public:
    // Construct menu with name "name" (JavaScript variable name)
    CHTMLPopupMenu(const string& name);
    virtual ~CHTMLPopupMenu(void);

    // Get menu name
    string GetName(void);

    // Add new item to current menu
    void AddItem(const string& title, 
                 const string& action    = kEmptyStr,
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,
                 const string& mouseout  = kEmptyStr);

    // Add item's separator
    void AddSeparator(void); 

    // Set menu attribute
    void SetAttribute(const EHTML_PM_Attribute attribute, const string& value);

    // Get JavaScript code for call menu
    string ShowMenu(void);

private:
    // Get HTML code for insert in the HEAD and BODY tags
    static string GetCodeHead(const string& menu_lib_url,
                              const string& menu_items_code,
                              const string& menu_any_name);
    static string GetCodeBody(void);

    // Get code for menu items in text format (JS function inside code)
    string GetCodeMenuItems(void);

    // Menu item type
    struct SItem {
        string title;      // menu item title
        string action;     // JS action on press item
        string color;      // ? (not used in JSMenu script)
        string mouseover;  // JS action on mouse over event for item
        string mouseout;   // JS action on mouse out event for item
    };
    typedef list<SItem> TItems;

    // Menu attribute type
    struct SAttribute {
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
