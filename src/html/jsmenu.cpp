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
 * Revision 1.1  2001/07/16 13:41:32  ivanov
 * Initialization
 *
 * ===========================================================================
 */


#include <html/jsmenu.hpp>


BEGIN_NCBI_SCOPE


CHTMLPopupMenu::CHTMLPopupMenu(const string& name)
{
    m_Name = name;
}


CHTMLPopupMenu::~CHTMLPopupMenu(void)
{
}


void CHTMLPopupMenu::AddItem(const string& title, const string& action, 
                             const string& color,
                             const string& mouseover, const string& mouseout)
{
    SItem item;
    item.title     = title; 
    item.action    = action; 
    item.color     = color; 
    item.mouseover = mouseover; 
    item.mouseout  = mouseout; 
    m_Items.push_back(item);
}


void CHTMLPopupMenu::AddSeparator(void)
{
    SItem item;
    item.title = kEmptyStr; 
    m_Items.push_back(item);
} 


void CHTMLPopupMenu::SetAttribute(const EHTML_PM_Attribute attribute,
                                  const string& value)
{
    SAttribute attr;
    attr.name  = attribute;
    attr.value = value;
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
            code += m_Name + ".addMenuItem(\"" + \
                i->title     + "\",\""  + \
                i->action    + "\",\""  + \
                i->color     + "\",\""  + \
                i->mouseover + "\",\""  + \
                i->mouseout  + "\");\n";
        }
    }

    // Write properties
    string attr; 

    iterate (TAttributes, i, m_Attrs) {
        switch(i->name) {
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
        }
        code += m_Name + "." + attr + " = \"" + i->value + "\";\n";
    }
    return code + "\n";
}

 
string CHTMLPopupMenu::GetCodeHead(const string& menu_lib_url,
                                   const string& menu_items_code,
                                   const string& menu_any_name)
{
    // Include the menu script loading
    string script = "<script language=\"JavaScript1.2\" src=\"" + \
        menu_lib_url + "\"></script>\n";

    // Menu definition
    script += "<script language=\"JavaScript1.2\">\n<!--\n";
    script += "function onLoad() {\n\n" + menu_items_code;
    script += menu_any_name + ".writeMenus();\n}\n//-->\n</script>\n";

    // Return generated script
    return script;
}


string CHTMLPopupMenu::GetCodeBody(void)
{
    return "<script language=\"JavaScript1.2\">\n" \
        "<!--\n//For IE\nif (document.all) onLoad();\n//-->\n</script>\n";
}


END_NCBI_SCOPE
