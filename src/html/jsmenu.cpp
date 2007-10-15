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

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <html/jsmenu.hpp>
#include <html/htmlhelper.hpp>
#include <html/html_exception.hpp>
#include <html/error_codes.hpp>
#include <corelib/ncbi_safe_static.hpp>


#define NCBI_USE_ERRCODE_X   Html_Lib


BEGIN_NCBI_SCOPE


// URL to menu library (default)

// Smith's menu 
const string kJSMenuDefaultURL_Smith
 = "http://www.ncbi.nlm.nih.gov/corehtml/jscript/ncbi_menu_dnd.js";

// Sergey Kurdin's popup menu
const string kJSMenuDefaultURL_Kurdin
 = "http://www.ncbi.nlm.nih.gov/coreweb/javascript/popupmenu2/popupmenu2_4.js";

// Sergey Kurdin's popup menu with configurations
const string kJSMenuDefaultURL_KurdinConf
 = "http://www.ncbi.nlm.nih.gov/coreweb/javascript/popupmenu2/popupmenu2_7loader.js";

// Sergey Kurdin's side menu
const string kJSMenuDefaultURL_KurdinSide
 = "http://www.ncbi.nlm.nih.gov/coreweb/javascript/sidemenu/sidemenu1.js";
const string kJSMenuDefaultURL_KurdinSideCSS
 = "http://www.ncbi.nlm.nih.gov/coreweb/styles/sidemenu.css"; 


// ===========================================================================

// MT Safe: Have individual copy of global attributes for each thread

// Store menu global attributes in TLS (eKurdinConf menu type only)
static CSafeStaticRef< CTls<CHTMLPopupMenu::TAttributes> > s_TlsGlobalAttrs;

static void s_TlsGlobalAttrsCleanup(CHTMLPopupMenu::TAttributes* attrs,
                                    void* /* data */)
{
    delete attrs;
}


CHTMLPopupMenu::TAttributes* CHTMLPopupMenu::GetGlobalAttributesPtr(void)
{
    CHTMLPopupMenu::TAttributes* attrs = s_TlsGlobalAttrs->GetValue();
    if ( !attrs ) {
        attrs = new CHTMLPopupMenu::TAttributes; 
        s_TlsGlobalAttrs->SetValue(attrs, s_TlsGlobalAttrsCleanup);
    }
    return attrs;
}


// ===========================================================================
 

CHTMLPopupMenu::CHTMLPopupMenu(const string& name, EType type)
{
    m_Name = name;
    m_Type = type;

    // Other menu-specific members (eKurdinConf)
    m_ConfigName = kEmptyStr;
    m_DisableLocalConfig = false;
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
    if (m_Type == eKurdinSide  &&   x_action.empty() ) {
        x_action = "none";
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


void CHTMLPopupMenu::AddSeparator(const string& text)
{
    SItem item;

    switch (m_Type) {
        case eSmith:
            break;
        case eKurdin:
            // eKurdin popup menu doesn't support separators
            return;
        case eKurdinConf:
            item.title  = text.empty() ? "-" : text;
            item.action = "-";
            break;
        case eKurdinSide:
            item.title  = "none";
            item.action = "none";
            break;
    }
    m_Items.push_back(item);
} 


void CHTMLPopupMenu::SetAttribute(EHTML_PM_Attribute attribute,
                                  const string&      value)
{
    m_Attrs[attribute] = value;
    if (m_Type == eKurdinConf  &&  m_ConfigName.empty()) {
        m_ConfigName = m_Name;
    }
}


void CHTMLPopupMenu::SetAttributeGlobal(EHTML_PM_Attribute attribute,
                                        const string&      value)
{
    CHTMLPopupMenu::TAttributes* attrs = GetGlobalAttributesPtr();
    (*attrs)[attribute] = value;
}


string CHTMLPopupMenu::GetAttributeValue(EHTML_PM_Attribute attribute) const
{
    TAttributes::const_iterator i = m_Attrs.find(attribute);
    if ( i != m_Attrs.end() ) {
        return i->second;
    }
    return kEmptyStr;
}


struct SAttributeSupport {
    EHTML_PM_Attribute attr;
    const char*        name[CHTMLPopupMenu::ePMLast+1];
};

const SAttributeSupport ksAttributeSupportTable[] = {

    //
    //  Old menu attributes
    //  (used for compatibility with previous version only).
    //
    //  S  - eSmith 
    //  K  - eKurdin 
    //  KC - eKurdinConf
    //  KS - eKurdinSide
    //
    //  0     - not sopported, 
    //  ""    - supported (name not used)
    //  "..." - supported (with name)
    //                                     S  K  KC KS            

    { eHTML_PM_enableTracker,            { "enableTracker"       , 0,  0,  0 } },
    { eHTML_PM_disableHide,              { "disableHide"         , 0,  "", 0 } },
    { eHTML_PM_menuWidth,                { 0                     , 0,  "", 0 } },
    { eHTML_PM_peepOffset,               { 0                     , 0,  "", 0 } },
    { eHTML_PM_topOffset,                { 0                     , 0,  "", 0 } },

    { eHTML_PM_fontSize,                 { "fontSize"            , 0,  0,  0 } },
    { eHTML_PM_fontWeigh,                { "fontWeigh"           , 0,  0,  0 } },
    { eHTML_PM_fontFamily,               { "fontFamily"          , 0,  0,  0 } },
    { eHTML_PM_fontColor,                { "fontColor"           , 0,  0,  0 } },
    { eHTML_PM_fontColorHilite,          { "fontColorHilite"     , 0,  0,  0 } },
    { eHTML_PM_menuBorder,               { "menuBorder"          , 0,  0,  0 } },
    { eHTML_PM_menuItemBorder,           { "menuItemBorder"      , 0,  0,  0 } },
    { eHTML_PM_menuItemBgColor,          { "menuItemBgColor"     , 0,  0,  0 } },
    { eHTML_PM_menuLiteBgColor,          { "menuLiteBgColor"     , 0,  0,  0 } },
    { eHTML_PM_menuBorderBgColor,        { "menuBorderBgColor"   , 0,  0,  0 } },
    { eHTML_PM_menuHiliteBgColor,        { "menuHiliteBgColor"   , 0,  0,  0 } },
    { eHTML_PM_menuContainerBgColor,     { "menuContainerBgColor", 0,  0,  0 } },
    { eHTML_PM_childMenuIcon,            { "childMenuIcon"       , 0,  0,  0 } },
    { eHTML_PM_childMenuIconHilite,      { "childMenuIconHilite" , 0,  0,  0 } },
    { eHTML_PM_bgColor,                  { "bgColor"             , "", 0,  0 } },
    { eHTML_PM_titleColor,               { 0                     , "", 0,  0 } },
    { eHTML_PM_borderColor,              { 0                     , "", 0,  0 } },
    { eHTML_PM_alignH,                   { 0                     , "", 0,  0 } },
    { eHTML_PM_alignV,                   { 0                     , "", 0,  0 } },

    //
    //  New menu attributes.
    //

    // View

    { eHTML_PM_ColorTheme,               { 0, 0, "ColorTheme",                0 } },
    { eHTML_PM_ShowTitle,                { 0, 0, "ShowTitle",                 0 } },
    { eHTML_PM_ShowCloseIcon,            { 0, 0, "ShowCloseIcon",             0 } },
    { eHTML_PM_HelpURL,                  { 0, 0, "Help",                      0 } },
    { eHTML_PM_HideTime,                 { 0, 0, "HideTime",                  0 } },
    { eHTML_PM_FreeText,                 { 0, 0, "FreeText",                  0 } },
    { eHTML_PM_ToolTip,                  { 0, 0, "ToolTip",                   0 } },
    { eHTML_PM_FrameTarget,              { 0, 0, "FrameTarget",               0 } },
/*
    { eHTML_PM_DisableHide,              { 0, 0, "", 0 } },
    { eHTML_PM_MenuWidth,                { 0, 0, "", 0 } },
    { eHTML_PM_PeepOffset,               { 0, 0, "", 0 } },
    { eHTML_PM_TopOffset,                { 0, 0, "", 0 } },
*/
    // Menu colors

    { eHTML_PM_BorderColor,              { 0, 0, "BorderColor",               0 } },
    { eHTML_PM_BackgroundColor,          { 0, 0, "BackgroundColor",           0 } },

    // Position
    
    { eHTML_PM_AlignLR,                  { 0, 0, "AlignLR",                   0 } },
    { eHTML_PM_AlignTB,                  { 0, 0, "AlignTB",                   0 } },
    { eHTML_PM_AlignCenter,              { 0, 0, "AlignCenter",               0 } },

    // Title

    { eHTML_PM_TitleText,                { 0, 0, "TitleText",                 0 } },
    { eHTML_PM_TitleColor,               { 0, 0, "TitleColor",                0 } },
    { eHTML_PM_TitleSize,                { 0, 0, "TitleSize",                 0 } },
    { eHTML_PM_TitleFont,                { 0, 0, "TitleFont",                 0 } },
    { eHTML_PM_TitleBackgroundColor,     { 0, 0, "TitleBackgroundColor",      0 } },
    { eHTML_PM_TitleBackgroundImage,     { 0, 0, "TitleBackgroundImage",      0 } },

    // Items

    { eHTML_PM_ItemColor,                { 0, 0, "ItemColor", 0 } },
    { eHTML_PM_ItemColorActive,          { 0, 0, "ItemColorActive",           0 } },
    { eHTML_PM_ItemBackgroundColorActive,{ 0, 0, "ItemBackgroundColorActive", 0 } },
    { eHTML_PM_ItemSize,                 { 0, 0, "ItemSize",                  0 } },
    { eHTML_PM_ItemFont,                 { 0, 0, "ItemFont",                  0 } },
    { eHTML_PM_ItemBulletImage,          { 0, 0, "ItemBulletImage",           0 } },
    { eHTML_PM_ItemBulletImageActive,    { 0, 0, "ItemBulletImageActive",     0 } },
    { eHTML_PM_SeparatorColor,           { 0, 0, "SeparatorColor",            0 } }
};


string CHTMLPopupMenu::GetAttributeName(EHTML_PM_Attribute attribute, EType type)
{
    // Find attribute
    size_t i;
    for (i = 0; i < sizeof(ksAttributeSupportTable)/sizeof(SAttributeSupport);
         i++) {
        if ( ksAttributeSupportTable[i].attr == attribute ) {
            if ( ksAttributeSupportTable[i].name[type] ) {
                return ksAttributeSupportTable[i].name[type];
            }
            break;
        }
    }
    string type_name = "This";
    switch (type) {
        case eSmith:
            type_name = "eSmith";
            break;
        case eKurdin:
            type_name = "eKurdin";
            break;
        case eKurdinConf:
            type_name = "eKurdinConf";
            break;
        case eKurdinSide:
            type_name = "eKurdinSide";
            break;
    }
    // Get attribute name approximately on the base other menu types
    string attr_name;
    for (size_t j = 0; j < ePMLast; j++) {
        const char* name = ksAttributeSupportTable[i].name[j];
        if ( name  &&  name[0] != '\0' ) {
            attr_name = name;
        }
    }
    // Name is not defined, use attribute numeric value
    if ( attr_name.empty() ) {
        attr_name = "with code " + NStr::IntToString(attribute);
    }
    ERR_POST_X(3, Warning << "CHTMLPopupMenu::GetMenuAttributeName:  " <<
                type_name << " menu type does not support attribute " <<
                attr_name);
    return kEmptyStr;
}


string CHTMLPopupMenu::ShowMenu(void) const
{
    switch (m_Type) {
        case eSmith:
            return "window.showMenu(window." + m_Name + ");";
        case eKurdin:
            {
            string align_h      = GetAttributeValue(eHTML_PM_alignH);
            string align_v      = GetAttributeValue(eHTML_PM_alignV);
            string color_border = GetAttributeValue(eHTML_PM_borderColor);
            string color_title  = GetAttributeValue(eHTML_PM_titleColor);
            string color_back   = GetAttributeValue(eHTML_PM_bgColor);
            string s = "','"; 
            return "PopUpMenu2_Set(" + m_Name + ",'" + align_h + s + align_v +
                   s + color_border + s + color_title + s +
                   color_back + "');";
            }
        case eKurdinConf:
            return "PopUpMenu2_Set(" + m_Name + ");";
        case eKurdinSide: {
            const string& nl = CHTMLHelper::GetNL();
            return "<script language=\"JavaScript1.2\">" + nl + "<!--" + nl +
                "document.write(SideMenuType == \"static\" ? " \
                "SideMenuStaticHtml : SideMenuDynamicHtml);" + nl +
                "//-->" + nl + "</script>" + nl;

            }
    }
    _TROUBLE;
    return kEmptyStr;
}


string CHTMLPopupMenu::HideMenu(void) const
{
    switch (m_Type) {
        case eKurdin:
        case eKurdinConf:
            return "PopUpMenu2_Hide();";
        default:
            ;
    }
    return kEmptyStr;
}


CNcbiOstream& CHTMLPopupMenu::PrintBegin(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            return out;
        case eHTML:
        case eXHTML:
            break;
    }
    string items = GetCodeItems();
    if ( !items.empty() ) {
        const string& nl = CHTMLHelper::GetNL();
        out << "<script language=\"JavaScript1.2\">" << nl 
            << "<!--" << nl << items << "//-->" << nl
            << "</script>" << nl;
    }
    return out;
}


string CHTMLPopupMenu::GetCodeHead(EType type, const string& menu_lib_url)
{
    string url, code;
    const string& nl = CHTMLHelper::GetNL();

    switch (type) {

    case eSmith:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_Smith : menu_lib_url;
        break;

    case eKurdin:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_Kurdin : menu_lib_url;
        break;

    case eKurdinConf:
        {
        code = "<script language=\"JavaScript1.2\">" + nl+ "<!--" + nl;
        code.append("var PopUpMenu2_GlobalConfig = [" + nl +
                    "  [\"UseThisGlobalConfig\",\"yes\"]");
        // Write properties
        CHTMLPopupMenu::TAttributes* attrs = GetGlobalAttributesPtr();
        ITERATE (TAttributes, i, *attrs) {
            string name  = GetAttributeName(i->first, eKurdinConf);
            string value = i->second;
            code.append("," + nl + "  [\"" + name + "\",\"" + value + "\"]");
        }
        code.append(nl + "]" + nl + "//-->" + nl + "</script>" + nl);
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_KurdinConf :
               menu_lib_url;
        break;
        }

    case eKurdinSide:
        url  = menu_lib_url.empty() ? kJSMenuDefaultURL_KurdinSide :
               menu_lib_url;
        code = "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
               kJSMenuDefaultURL_KurdinSideCSS + "\">" + nl; 
        break;
    }
    if ( !url.empty() ) {
        code.append("<script language=\"JavaScript1.2\" src=\"" + url +
                    "\"></script>" + nl);
    }
    return code;
}


string CHTMLPopupMenu::GetCodeBody(EType type, bool use_dyn_menu)
{
    if ( type != eSmith ) {
        return kEmptyStr;
    }
    string use_dm = use_dyn_menu ? "true" : "false";
    const string& nl = CHTMLHelper::GetNL();
    return "<script language=\"JavaScript1.2\">" + nl +
           "<!--" + nl + "function onLoad() {" + nl +
           "  window.useDynamicMenu = " + use_dm + ";" + nl + 
           "  window.defaultjsmenu = new Menu();" + nl +
           "  defaultjsmenu.addMenuSeparator();" + nl +
           "  defaultjsmenu.writeMenus();" + nl +
           "}" + nl +
           "// For IE & NS6" + nl + 
           "if (!document.layers) onLoad();"  + nl +
           "//-->" + nl +
           "</script>" + nl;
}


string CHTMLPopupMenu::GetCodeItems(void) const
{
    string code;
    const string& nl = CHTMLHelper::GetNL();

    switch (m_Type) {
    case eSmith: 
        {
            code = "window." + m_Name + " = new Menu();" + nl;
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                if ( (i->title).empty() ) {
                    code.append(m_Name + ".addMenuSeparator();" + nl);
                }
                else {
                    code.append(m_Name + ".addMenuItem(\"" +
                                i->title     + "\",\""  +
                                i->action    + "\",\""  +
                                i->color     + "\",\""  +
                                i->mouseover + "\",\""  +
                                i->mouseout  + "\");" + nl);
                }
            }
            // Write properties
            ITERATE (TAttributes, i, m_Attrs) {
                string name  = GetAttributeName(i->first);
                string value = i->second;
                code.append(m_Name + "." + name + " = \"" + value + "\";" + nl);
            }
        }
        break;

    case eKurdin: 
        {
            code = "var " + m_Name + " = [" + nl;
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                if ( i != m_Items.begin()) {
                    code.append("," + nl);
                }
                code.append("  [\"" +
                            i->title     + "\",\""  +
                            i->action    + "\",\""  +
                            i->mouseover + "\",\""  +
                            i->mouseout  + "\"]");
            }
            code.append(nl + "]" + nl);
        }
        break;

    case eKurdinConf:
        {
            if ( m_ConfigName == m_Name ) {
                code.append("var PopUpMenu2_LocalConfig_" +
                            m_Name + " = [" + nl);
                // If local config is disabled
                if ( m_DisableLocalConfig ) {
                    code.append("  [\"UseThisLocalConfig\",\"no\"]");
                }
                // Write properties
                ITERATE (TAttributes, i, m_Attrs) {
                    if ( m_DisableLocalConfig  ||  i != m_Attrs.begin() ) {
                        code.append("," + nl);
                    }
                    string name  = GetAttributeName(i->first);
                    string value = i->second;
                    code.append("  [\"" + name + "\",\"" + value + "\"]");
                }
                code.append(nl + "]" + nl);
            }
            // Write menu always, even it is empty
            code.append("var " + m_Name + " = [" + nl);
            if ( !m_ConfigName.empty() ) {
                code.append("  [\"UseLocalConfig\",\"" + 
                            m_ConfigName + "\",\"\",\"\"]");
            }
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                if ( !m_ConfigName.empty()  ||  i != m_Items.begin()) {
                    code.append("," + nl);
                }
                code.append("  [\"" +
                            i->title     + "\",\""  +
                            i->action    + "\",\""  +
                            i->mouseover + "\",\""  +
                            i->mouseout  + "\"]");
            }
            code.append(nl + "]" + nl);
        }
        break;

    case eKurdinSide:
        {
            // Menu name always is "SideMenuParams"
            code = "var SideMenuParams = [" + nl;
            // Menu configuration
            string disable_hide = GetAttributeValue(eHTML_PM_disableHide);
            string menu_type;
            if ( disable_hide == "true" ) {
                menu_type = "static";
            } else if ( disable_hide == "false" ) {  
                menu_type = "dynamic";
            // else menu_type have default value
            }
            string width       = GetAttributeValue(eHTML_PM_menuWidth);
            string peep_offset = GetAttributeValue(eHTML_PM_peepOffset);
            string top_offset  = GetAttributeValue(eHTML_PM_topOffset);
            code.append("[\"\",\"" + menu_type + "\",\"\",\"" + width +
                        "\",\"" + peep_offset + "\",\"" + top_offset +
                        "\",\"\",\"\"]");
            // Write menu items
            ITERATE (TItems, i, m_Items) {
                code.append("," + nl + "[\"" +
                            i->title + "\",\""  +
                            i->action + "\",\"\",\"\"]");
            }
            code.append(nl + "]" + nl);
        }
        break;
    }

    return code;
}


END_NCBI_SCOPE
