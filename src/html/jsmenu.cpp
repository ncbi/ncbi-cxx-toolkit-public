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

#include <html/jsmenu.hpp>
#include <html/html_exception.hpp>


BEGIN_NCBI_SCOPE


// URL to menu library (default)

// Smith's menu 
const string kJSMenuDefaultURL_Smith
 = "http://www.ncbi.nlm.nih.gov/corehtml/jscript/ncbi_menu_dnd.js";

// Sergey Kurdin's popup menu
const string kJSMenuDefaultURL_Kurdin
 = "http://www.ncbi.nlm.nih.gov/coreweb/javascript/popupmenu2/popupmenu2_4.js";

// Sergey Kurdin's side menu
const string kJSMenuDefaultURL_KurdinSide
 = "http://www.ncbi.nlm.nih.gov/coreweb/javascript/sidemenu/sidemenu1.js";
const string kJSMenuDefaultURL_KurdinSideCSS
 = "http://www.ncbi.nlm.nih.gov/coreweb/styles/sidemenu.css"; 
 

CHTMLPopupMenu::CHTMLPopupMenu(const string& name, EType type)
{
    m_Name = name;
    m_Type = type;
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
    string x_action = action;
    if (m_Type == eKurdinSide) {
        if ( x_action.empty() ) {
            x_action = "none";
        }
    } else {
        if ( NStr::StartsWith(action, "http:", NStr::eNocase) ) {
            x_action = "window.location='" + action + "'";
        }
    }
    SItem item(title, x_action, color, mouseover, mouseout);
    m_Items.push_back(item);
}


void CHTMLPopupMenu::AddItem(const char*   title,
                             const string& action, 
                             const string& color,
                             const string& mouseover, const string& mouseout)
{
    if ( !title ) {
        NCBI_THROW(CHTMLException,eNullPtr,
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
    // Shield double quotes
    title = NStr::Replace(title,"\"","'");
    // Add menu item
    AddItem(title, action, color, mouseover, mouseout);
}


void CHTMLPopupMenu::AddSeparator(void)
{
    // Only eKurdin popup menu don't support separators
    if ( m_Type == eKurdin ) {
        return;
    }
    SItem item;
    if (m_Type == eKurdinSide) {
        item.title  = "none";
        item.action = "none";
    }
    m_Items.push_back(item);
} 


void CHTMLPopupMenu::SetAttribute(EHTML_PM_Attribute attribute,
                                  const string&      value)
{
    m_Attrs[attribute] = value;
}


string CHTMLPopupMenu::GetMenuAttributeValue(EHTML_PM_Attribute attribute) const
{
    TAttributes::const_iterator i = m_Attrs.find(attribute);
    if ( i != m_Attrs.end() ) {
        return i->second;
    }
    return kEmptyStr;
}


string CHTMLPopupMenu::GetMenuAttributeName(EHTML_PM_Attribute attribute) const
{
    switch (attribute) {
    case eHTML_PM_enableTracker:
        return "enableTracker"; 
    case eHTML_PM_disableHide:
        return "disableHide"; 
    case eHTML_PM_menuWidth:
        return "menuWidth"; 
    case eHTML_PM_peepOffset:
        return "peepOffset"; 
    case eHTML_PM_topOffset:
        return "topOffset"; 
    case eHTML_PM_fontSize:
        return "fontSize"; 
    case eHTML_PM_fontWeigh:
        return "fontWeigh"; 
    case eHTML_PM_fontFamily:
        return "fontFamily"; 
    case eHTML_PM_fontColor:
        return "fontColor"; 
    case eHTML_PM_fontColorHilite:
        return "fontColorHilite"; 
    case eHTML_PM_menuBorder:
        return "menuBorder"; 
    case eHTML_PM_menuItemBorder:
        return "menuItemBorder"; 
    case eHTML_PM_menuItemBgColor:
        return "menuItemBgColor"; 
    case eHTML_PM_menuLiteBgColor:
        return "menuLiteBgColor"; 
    case eHTML_PM_menuBorderBgColor:
        return "menuBorderBgColor"; 
    case eHTML_PM_menuHiliteBgColor:
        return "menuHiliteBgColor"; 
    case eHTML_PM_menuContainerBgColor:
        return "menuContainerBgColor"; 
    case eHTML_PM_childMenuIcon:
        return "childMenuIcon"; 
    case eHTML_PM_childMenuIconHilite:
        return "childMenuIconHilite"; 
    case eHTML_PM_bgColor:
        return "bgColor"; 
    case eHTML_PM_titleColor:
        return "titleColor";
    case eHTML_PM_borderColor:
        return "borderColor";
    case eHTML_PM_alignH:
        return "alignH";
    case eHTML_PM_alignV:
        return "alignV";
    }
    _TROUBLE;
    return kEmptyStr;
}


string CHTMLPopupMenu::GetName(void) const
{
    return m_Name;
}


CHTMLPopupMenu::EType CHTMLPopupMenu::GetType(void) const
{
    return m_Type;
}


string CHTMLPopupMenu::ShowMenu(void) const
{
    switch (m_Type) {
    case eSmith:
        return "window.showMenu(window." + m_Name + ");";
    case eKurdin:
        {
        string align_h      = GetMenuAttributeValue(eHTML_PM_alignH);
        string align_v      = GetMenuAttributeValue(eHTML_PM_alignV);
        string color_border = GetMenuAttributeValue(eHTML_PM_borderColor);
        string color_title  = GetMenuAttributeValue(eHTML_PM_titleColor);
        string color_back   = GetMenuAttributeValue(eHTML_PM_bgColor);
        string s = "','"; 
        return "PopUpMenu2_Set(" + m_Name + ",'" + align_h + s + align_v + s + 
                color_border + s + color_title + s + color_back + "');";
        }
    case eKurdinSide:
        return "<script language=\"JavaScript1.2\">\n<!--\n" \
               "document.write(SideMenuType == \"static\" ? " \
               "SideMenuStaticHtml : SideMenuDynamicHtml);" \
               "\n//-->\n</script>\n";
    }
    _TROUBLE;
    return kEmptyStr;
}


string CHTMLPopupMenu::GetCodeMenuItems(void) const
{
    string code;
    switch (m_Type) {
    case eSmith: 
        {
            code = "window." + m_Name + " = new Menu();\n";
            // Write menu items
            ITERATE (TItems, i, m_Items) {
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
            ITERATE (TAttributes, i, m_Attrs) {
                string name  = GetMenuAttributeName(i->first);
                string value = i->second;
                code += m_Name + "." + name + " = \"" + value + "\";\n";
            }
        }
        break;

    case eKurdin: 
        {
            code = "var " + m_Name + " = [\n";
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                if ( i != m_Items.begin()) {
                    code += ",\n";
                }
                code += "[\"" +
                    i->title     + "\",\""  +
                    i->action    + "\",\""  +
                    i->mouseover + "\",\""  +
                    i->mouseout  + "\"]";
            }
            code += "\n]\n";
        }
        break;

    case eKurdinSide:
        {
            // Menu name always is "SideMenuParams"
            code = "var SideMenuParams = [\n";
            // Menu configuration
            string disable_hide = GetMenuAttributeValue(eHTML_PM_disableHide);
            string menu_type;
            if ( disable_hide == "true" ) {
                menu_type = "static";
            } else if ( disable_hide == "false" ) {  
                menu_type = "dynamic";
            // else menu_type have default value
            }
            string width       = GetMenuAttributeValue(eHTML_PM_menuWidth);
            string peep_offset = GetMenuAttributeValue(eHTML_PM_peepOffset);
            string top_offset  = GetMenuAttributeValue(eHTML_PM_topOffset);
            code += "[\"\",\"" + menu_type + "\",\"\",\"" + width + "\",\"" +
                peep_offset + "\",\"" + top_offset + "\",\"\",\"\"]";
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                code += ",\n[\"" +
                    i->title     + "\",\""  +
                    i->action    + "\",\"\",\"\"]";
            }
            code += "\n]\n";
        }
        break;
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


string CHTMLPopupMenu::GetCodeHead(EType type, const string& menu_lib_url)
{
    string url, code;

    switch (type) {

    case eSmith:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_Smith : menu_lib_url;
        break;

    case eKurdin:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_Kurdin : menu_lib_url;
        break;

    case eKurdinSide:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_KurdinSide :
            menu_lib_url;
        code = "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
            kJSMenuDefaultURL_KurdinSideCSS + "\">\n"; 
        break;
    }
    if ( !url.empty() ) {
        code += "<script language=\"JavaScript1.2\" src=\"" + url +
            "\"></script>\n";
    }
    return code;
}


string CHTMLPopupMenu::GetCodeBody(EType type, bool use_dynamic_menu)
{
    if ( type != eSmith ) {
        return kEmptyStr;
    }
    string use_dm = use_dynamic_menu ? "true" : "false";
    return "<script language=\"JavaScript1.2\">\n"
        "<!--\nfunction onLoad() {\n"
        "window.useDynamicMenu = " + use_dm + ";\n"
        "window.defaultjsmenu = new Menu();\n"
        "defaultjsmenu.addMenuSeparator();\n"
        "defaultjsmenu.writeMenus();\n"
        "}\n"
        "// For IE & NS6\nif (!document.layers) onLoad();\n//-->\n</script>\n";
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.24  2003/12/12 12:10:38  ivanov
 * Updated Sergey Kurdin's popup menu to v2.4
 *
 * Revision 1.23  2003/12/10 19:14:16  ivanov
 * Move adding a string "return false;" to menues JS code call from ShowMenu()
 * to AttachPopupMenu()
 *
 * Revision 1.22  2003/12/03 12:39:24  ivanov
 * ShowMenu(): finalize JS code for eKurdin type with "return false;"
 *
 * Revision 1.21  2003/12/02 14:27:06  ivanov
 * Removed obsolete functions GetCodeBodyTag[Handler|Action]().
 *
 * Revision 1.20  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.19  2003/10/02 18:24:38  ivanov
 * Get rid of compilation warnings; some formal code rearrangement
 *
 * Revision 1.18  2003/10/01 15:56:44  ivanov
 * Added support for Sergey Kurdin's side menu
 *
 * Revision 1.17  2003/09/03 20:21:35  ivanov
 * Updated Sergey Kurdin's popup menu to v2.3
 *
 * Revision 1.16  2003/07/08 17:13:53  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * Revision 1.15  2003/06/30 21:16:50  ivanov
 * Updated Sergey Kurdin's popup menu to v2.2
 *
 * Revision 1.14  2003/04/29 18:42:14  ivanov
 * Fix for previous commit
 *
 * Revision 1.13  2003/04/29 17:45:43  ivanov
 * Changed array with file names for Kurdin's menu to const definitions
 *
 * Revision 1.12  2003/04/29 16:28:13  ivanov
 * Use one JS Script for Kurdin's menu
 *
 * Revision 1.11  2003/04/01 16:35:29  ivanov
 * Changed path for the Kurdin's popup menu
 *
 * Revision 1.10  2003/03/11 15:28:57  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.9  2002/12/12 17:20:46  ivanov
 * Renamed GetAttribute() -> GetMenuAttributeValue,
 *         GetAttributeName() -> GetMenuAttributeName().
 *
 * Revision 1.8  2002/12/09 22:11:59  ivanov
 * Added support for Sergey Kurdin's popup menu
 *
 * Revision 1.7  2002/04/29 18:07:21  ucko
 * Make GetName const.
 *
 * Revision 1.6  2002/02/13 20:16:09  ivanov
 * Added support of dynamic popup menues. Changed GetCodeBody().
 *
 * Revision 1.5  2001/11/29 16:06:31  ivanov
 * Changed using menu script name "menu.js" -> "ncbi_menu.js".
 * Fixed error in using menu script without menu definitions.
 *
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
