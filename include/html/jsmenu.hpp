#ifndef HTML___JSMENU__HPP
#define HTML___JSMENU__HPP

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
 */

/// @file jsmenu.hpp 
/// JavaScript popup menu support.


#include <corelib/ncbistd.hpp>
#include <html/node.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Popup menu attribute.
///
/// If attribute not define for menu with function SetAttribute(), 
/// then it have some default value dependent on menu type.
/// All attributes have effect only for specified menu type, otherwise it
/// will be ignored.
///
/// NOTE: See documentation for detail attribute description.

enum EHTML_PM_Attribute {
    //
    //  Old menu attributes
    //  (used for compatibility with previous version only).
    //

    //                               Using by       Value example
    //
    //                               S  - eSmith 
    //                               K  - eKurdin 
    //                               KC - eKurdinConf
    //                               KS - eKurdinSide
    
    eHTML_PM_enableTracker,          // S           "true"
    eHTML_PM_disableHide,            // S      KS   "false"
    eHTML_PM_menuWidth,              //        KS   "100"
    eHTML_PM_peepOffset,             //        KS   "20"
    eHTML_PM_topOffset,              //        KS   "10"

    eHTML_PM_fontSize,               // S           "14"
    eHTML_PM_fontWeigh,              // S           "plain"
    eHTML_PM_fontFamily,             // S           "arial,helvetica"
    eHTML_PM_fontColor,              // S           "black"
    eHTML_PM_fontColorHilite,        // S           "#ffffff"
    eHTML_PM_menuBorder,             // S           "1"
    eHTML_PM_menuItemBorder,         // S           "0"
    eHTML_PM_menuItemBgColor,        // S           "#cccccc"
    eHTML_PM_menuLiteBgColor,        // S           "white"
    eHTML_PM_menuBorderBgColor,      // S           "#777777"
    eHTML_PM_menuHiliteBgColor,      // S           "#000084"
    eHTML_PM_menuContainerBgColor,   // S           "#cccccc"
    eHTML_PM_childMenuIcon,          // S           "images/arrows.gif"
    eHTML_PM_childMenuIconHilite,    // S           "images/arrows2.gif"
    eHTML_PM_bgColor,                // S K         "#555555"
    eHTML_PM_titleColor,             //   K         "#FFFFFF"
    eHTML_PM_borderColor,            //   K         "black"
    eHTML_PM_alignH,                 //   K         "left" or "right"
    eHTML_PM_alignV,                 //   K         "bottom" or "top"


    //
    //  New menu attributes.
    //

    // View

    eHTML_PM_ColorTheme,               //     KC      Name of theme
    eHTML_PM_ShowTitle,                //     KC      "yes" / "no"
    eHTML_PM_ShowCloseIcon,            //             "yes" / "no"
    eHTML_PM_HelpURL,                  //     KC      URL or JS code
    eHTML_PM_HideTime,                 //     KC      Number of milliseconds
    eHTML_PM_FreeText,                 //     KC      Some text or html
/*
    eHTML_PM_DisableHide,              //             
    eHTML_PM_MenuWidth,                //             
    eHTML_PM_PeepOffset,               //             
    eHTML_PM_TopOffset,                //             
*/
    // Menu colors

    eHTML_PM_BorderColor,              //     KC      Standard web color
    eHTML_PM_BackgroundColor,          //   K KC      Standard web color

    // Position
    
    eHTML_PM_AlignLR,                  //   K KC      "left" / "right"
    eHTML_PM_AlignTB,                  //   K KC      "bottom" / "top"
    eHTML_PM_AlignCenter,              //     KC      "yes" / "no"

    // Title

    eHTML_PM_TitleText,                //     KC      Title text
    eHTML_PM_TitleColor,               //   K KC      Standard web color
    eHTML_PM_TitleSize,                //     KC      "11" / "11px" / "11em"
    eHTML_PM_TitleFont,                //     KC      Web fonts name(s)
    eHTML_PM_TitleBackgroundColor,     //             Standard web color
    eHTML_PM_TitleBackgroundImage,     //     KC      Path to image file

    // Items

    eHTML_PM_ItemColor,                //     KC      Standard web color
    eHTML_PM_ItemColorActive,          //             Standard web color
    eHTML_PM_ItemBackgroundColorActive,//     KC      Standard web color
    eHTML_PM_ItemSize,                 //     KC      "11" / "11px" / "11em"
    eHTML_PM_ItemFont,                 //     KC      Web fonts name(s)
    eHTML_PM_ItemBulletImage,          //     KC      Path to image file
    eHTML_PM_ItemBulletImageActive,    //             Path to image file
    eHTML_PM_SeparatorColor            //     KC      Standard web color
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHTMLPopupMenu --
///
/// Define for support JavaScript popup menues.
///
/// For successful work menu in HTML pages next steps required:
///    - File with popup menu JavaScript library -- "*.js".
///      By default using menu with URL, defined in constant
///      kJSMenuDefaultURL*, defined in the jsmenu.cpp.
///    - Define menues and add it to HTML page (AppendChild()).
///    - Call EnablePopupMenu() (member function of CHTML_html and CHTMLPage).
/// 
/// NOTE: We must add menues to a BODY only, otherwise menu not will be work.
/// NOTE: Menues of eKurdinSide type must be added (using AppendChild) only
///       to a HEAD node. And menu of this type must be only one on the page!

class NCBI_XHTML_EXPORT CHTMLPopupMenu : public CNCBINode
{
    typedef CNCBINode CParent;
    friend class CHTMLPage;
    friend class CHTMLNode;
public:

    /// Popup menu type.
    enum EType {
        eSmith,             ///< Smith's menu (ncbi_menu*.js)
        eKurdin,            ///< Sergey Kurdin's popup menu (popupmenu2*.js)
        eKurdinConf,        ///< Sergey Kurdin's popup menu with configurations
                            ///< (popupmenu2*.js, v2.5 and above)
        eKurdinSide,        ///< Sergey Kurdin's side menu (sidemenu*.js)

        ePMFirst = eSmith,
        ePMLast  = eKurdinSide
    };
    /// Menu attribute type.
    typedef map<EHTML_PM_Attribute, string> TAttributes;

    /// Constructor.
    ///
    /// Construct menu with name "name" (JavaScript variable name).
    CHTMLPopupMenu(const string& name, EType type = eSmith);

    /// Destructor.
    virtual ~CHTMLPopupMenu(void);

    /// Get menu name.
    string GetName(void) const;
    /// Get menu type.
    EType  GetType(void) const;


    /// Add new item to current menu.
    ///
    /// NOTE: action - can be also URL type like "http://...".
    /// NOTE: Parameters have some restrictions according to menu type:
    ///       title  - can be text or HTML-code (for eSmith menu type only);
    ///       color  - will be ignored for eKurdin menu type.

    void AddItem(const string& title,                  ///< Text or HTML-code
                 const string& action    = kEmptyStr,  ///< JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  ///< JS code
                 const string& mouseout  = kEmptyStr); ///< JS code

    void AddItem(const char*   title,                  ///< Text or HTML-code
                 const string& action    = kEmptyStr,  ///< JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  ///< JS code
                 const string& mouseout  = kEmptyStr); ///< JS code

    /// NOTE: The "node" will be convert to a string inside function, so
    ///       the node's Print() method must not change a node structure.
    void AddItem(CNCBINode& node,
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    /// Add item's separator.
    ///
    /// NOTE: parameter 'text' have effect only for eKurdinKC menu type.
    void AddSeparator(const string& text = kEmptyStr); 

    /// Set menu attribute.
    void SetAttribute(EHTML_PM_Attribute attribute, const string& value);

    /// Get attribute name.
    string GetAttributeName(EHTML_PM_Attribute attribute) const;
    static
    string GetAttributeName(EHTML_PM_Attribute attribute, EType type);

    /// Get attribute value.
    string GetAttributeValue(EHTML_PM_Attribute attribute) const;

    /// Set global menu attribute.
    /// NOTE: Works only with eKurdinConf menu type.
    static void SetAttributeGlobal(EHTML_PM_Attribute attribute,
                                   const              string& value);

    /// Get JavaScript code for menu call.
    string ShowMenu(void) const;

    /// Get JavaScript code for menu hide.
    string HideMenu(void) const;

    /// Use specified menu configuration (for eKurdinConf only).
    /// NOTE: All attributes stated by SetAttribute() will be ignored.
    void UseConfig(const string& name);
    void DisableLocalConfig(bool disable = true);

    /// Get HTML code for inserting into the end of the HEAD and BODY blocks.
    /// If "menu_lib_url" is not defined, then use default URL.
    /// NOTE: Parameters "use_dyn_menu" have effect only for eSmith menu
    static string GetCodeHead(EType         type         = eSmith,
                              const string& menu_lib_url = kEmptyStr);
    static string GetCodeBody(EType         type         = eSmith,
                              bool          use_dyn_menu = false);

    /// Get string with JavaScript code for menu items
    string GetCodeItems(void) const;

private:
    /// Print menu.
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

    /// Menu item type.
    struct SItem {
        /// Constructor for menu item.
        SItem(const string& title, const string& action, const string& color,
              const string& mouseover, const string& mouseout);
        /// Constructor for separator.
        SItem(void);

        string title;      ///< Menu item title.
        string action;     ///< JS action on press item.
        string color;      ///< ? (not used in JSMenu script).
        string mouseover;  ///< JS action on mouse over event for item.
        string mouseout;   ///< JS action on mouse out event for item.
    };
    typedef list<SItem> TItems;

    /// Get pointer to global attributes.
    static TAttributes* GetGlobalAttributesPtr(void);

private:
    string       m_Name;   ///< Menu name
    EType        m_Type;   ///< Menu type
    TItems       m_Items;  ///< Menu items
    TAttributes  m_Attrs;  ///< Menu attributes

    // Name of local configuration for eKurdinConf menu type.
    string       m_ConfigName; 
    // Enable/disable local configuration for current menu.
    bool         m_DisableLocalConfig; 
};


//=============================================================================
//
//  Inline
//
//=============================================================================


inline 
string CHTMLPopupMenu::GetName(void) const
{
    return m_Name;
}


inline 
CHTMLPopupMenu::EType CHTMLPopupMenu::GetType(void) const
{
    return m_Type;
}


inline 
void CHTMLPopupMenu::UseConfig(const string& name)
{
    m_ConfigName = name;
}


inline 
string CHTMLPopupMenu::GetAttributeName(EHTML_PM_Attribute attribute) const
{
    return GetAttributeName(attribute, m_Type);
}


inline 
void CHTMLPopupMenu::DisableLocalConfig(bool disable)
{
    m_DisableLocalConfig = disable;
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2004/05/05 13:55:40  ivanov
 * + DisableLocalConfig()
 *
 * Revision 1.18  2004/04/05 16:19:46  ivanov
 * Added support for Sergey Kurdin's popup menu with configurations
 *
 * Revision 1.17  2004/01/16 15:12:32  ivanov
 * Minor cosmetic changes
 *
 * Revision 1.16  2003/12/18 20:14:21  golikov
 * Added HideMenu
 *
 * Revision 1.15  2003/12/02 14:21:35  ivanov
 * Remjved obsolete functions GetCodeBodyTag[Handler|Action]()
 *
 * Revision 1.14  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.13  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.12  2003/10/02 18:16:46  ivanov
 * Get rid of compilation warnings; some formal code rearrangement
 *
 * Revision 1.11  2003/10/01 15:55:04  ivanov
 * Added support for Sergey Kurdin's side menu
 *
 * Revision 1.10  2003/04/25 13:45:32  siyan
 * Added doxygen groupings
 *
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
 * Added support of dynamic popup menues. Changed GetCodeBody().
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
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* HTML___JSMENU__HPP */
