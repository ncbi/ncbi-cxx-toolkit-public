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
 * Revision 1.4  2001/10/15 23:16:22  vakatov
 * + AddItem(const char* title, ...) to avoid "string/CNCBINode" ambiguity
 *
 * Revision 1.3  2001/08/15 19:43:13  ivanov
 * Added AddMenuItem( node, ...)
 *
 * Revision 1.2  2001/08/14 16:53:07  ivanov
 * Changed parent class for CHTMLPopupMenu.
 * Changed mean for init JavaScript popup menu & add it to HTML document.
 *
 * Revision 1.1  2001/07/16 13:41:32  ivanov
 * Initialization
 *
 * ===========================================================================
 */


#include <html/jsmenu.hpp>


BEGIN_NCBI_SCOPE


// URL to menu library (default)
const string kJSMenuDefaultURL
  = "http://www.ncbi.nlm.nih.gov/corehtml/jscript/menu.js";


CHTMLPopupMenu::CHTMLPopupMenu(const string& name)
{
    m_Name = name;
}


CHTMLPopupMenu::~CHTMLPopupMenu(void)
{
    return;
}


CHTMLPopupMenu::SItem::SItem(const string& v_title, 
                             const string& v_action, 
                             const string& v_color,
                             const string& v_mouseover, 
                             const string& v_mouseout)
{
    title     = v_title;
    action    = v_action;
    color     = v_color;
    mouseover = v_mouseover;
    mouseout  = v_mouseout;
}

CHTMLPopupMenu::SItem::SItem()
{
    title = kEmptyStr;
}
        

void CHTMLPopupMenu::AddItem(const string& title,
                             const string& action, 
                             const string& color,
                             const string& mouseover, const string& mouseout)
{
    SItem item(title, action, color, mouseover, mouseout);
    m_Items.push_back(item);
}


void CHTMLPopupMenu::AddItem(const char*   title,
                             const string& action, 
                             const string& color,
                             const string& mouseover, const string& mouseout)
{
    if ( !title ) {
        THROW1_TRACE(runtime_error,
                     "CHTMLPopupMenu::AddItem() passed NULL title");
    }

    const string x_title(title);
    AddItem(x_title, action, color, mouseover, mouseout);
}


void CHTMLPopupMenu::AddItem(CNCBINode& node,
                             const string& action, 
                             const string& color,
                             const string& mouseover, const string& mouseout)
{
    // Convert "node" to string
    CNcbiOstrstream out;
    node.Print(out, eHTML);
    string title = CNcbiOstrstreamToString(out);
    // Replace " to '
    title = NStr::Replace(title,"\"","'");
    // Add menu item
    AddItem(title, action, color, mouseover, mouseout);
}


void CHTMLPopupMenu::AddSeparator(void)
{
    SItem item;
    m_Items.push_back(item);
} 


CHTMLPopupMenu::SAttribute::SAttribute(EHTML_PM_Attribute v_name, 
                                      const string& v_value)
{
    name   = v_name;
    value  = v_value;
}


void CHTMLPopupMenu::SetAttribute(EHTML_PM_Attribute attribute,
                                  const string&      value)
{
    SAttribute attr(attribute, value);
    m_Attrs.push_back(attr);
}


string CHTMLPopupMenu::GetName(void)
{
    return m_Name;
}


string CHTMLPopupMenu::ShowMenu(void)
{
    return "window.showMenu(window." + m_Name + ");";
}


string CHTMLPopupMenu::GetCodeMenuItems(void)
{
    string code = "window." + m_Name + " = new Menu();\n";

    // Write menu items
    iterate (TItems, i, m_Items) {
        if ( (i->title).empty() ) {
            code += m_Name + ".addMenuSeparator();\n";
        }
        else {
            code += m_Name + ".addMenuItem(\"" +
                i->title     + "\",\""  +
                i->action    + "\",\""  +
                i->color     + "\",\""  +
                i->mouseover + "\",\""  +
                i->mouseout  + "\");\n";
        }
    }

    // Write properties
    string attr; 

    iterate (TAttributes, i, m_Attrs) {
        switch ( i->name ) {
        case eHTML_PM_enableTracker:
            attr = "enableTracker"; 
            break;
        case eHTML_PM_disableHide:
            attr = "disableHide"; 
            break;
        case eHTML_PM_fontSize:
            attr = "fontSize"; 
            break;
        case eHTML_PM_fontWeigh:
            attr = "fontWeigh"; 
            break;
        case eHTML_PM_fontFamily:
            attr = "fontFamily"; 
            break;
        case eHTML_PM_fontColor:
            attr = "fontColor"; 
            break;
        case eHTML_PM_fontColorHilite:
            attr = "fontColorHilite"; 
            break;
        case eHTML_PM_bgColor:
            attr = "bgColor"; 
            break;
        case eHTML_PM_menuBorder:
            attr = "menuBorder"; 
            break;
        case eHTML_PM_menuItemBorder:
            attr = "menuItemBorder"; 
            break;
        case eHTML_PM_menuItemBgColor:
            attr = "menuItemBgColor"; 
            break;
        case eHTML_PM_menuLiteBgColor:
            attr = "menuLiteBgColor"; 
            break;
        case eHTML_PM_menuBorderBgColor:
            attr = "menuBorderBgColor"; 
            break;
        case eHTML_PM_menuHiliteBgColor:
            attr = "menuHiliteBgColor"; 
            break;
        case eHTML_PM_menuContainerBgColor:
            attr = "menuContainerBgColor"; 
            break;
        case eHTML_PM_childMenuIcon:
            attr = "childMenuIcon"; 
            break;
        case eHTML_PM_childMenuIconHilite:
            attr = "childMenuIconHilite"; 
            break;
        default:
            _TROUBLE;
        }

        code += m_Name + "." + attr + " = \"" + i->value + "\";\n";
    }

    return code;
}


CNcbiOstream& CHTMLPopupMenu::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == eHTML ) {
        out << "<script language=\"JavaScript1.2\">\n<!--\n" 
            << GetCodeMenuItems() << "//-->\n</script>\n";
    }
    return out;
}


string CHTMLPopupMenu::GetCodeHead(const string& menu_lib_url)
{
    // If URL not defined, then use default value
    string url = menu_lib_url.empty() ? kJSMenuDefaultURL : menu_lib_url;

    // Include the menu script loading
    return "<script language=\"JavaScript1.2\" src=\"" + url + "\"></script>\n";
}


string CHTMLPopupMenu::GetCodeBody(void)
{
    return "<script language=\"JavaScript1.2\">\n" \
        "<!--\nfunction onLoad() {\nwindow.defaultjsmenu = new Menu();\n" \
        "defaultjsmenu.writeMenus();\n}\n" \
        "// For IE\nif (document.all) onLoad();\n//-->\n</script>\n";
}


END_NCBI_SCOPE
