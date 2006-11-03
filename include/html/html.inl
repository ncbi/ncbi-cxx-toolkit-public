#if defined(HTML___HTML__HPP)  &&  !defined(HTML___HTML__INL)
#define HTML___HTML__INL

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
 * Authors:  Lewis Geer, Eugene Vasilchenko, Vladimir Ivanov
 *
 */



// CHTMLNode

inline
CHTMLNode* CHTMLNode::SetClass(const string& class_name)
{
    SetOptionalAttribute("class", class_name);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetId(const string& class_name)
{
    SetOptionalAttribute("id", class_name);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetWidth(int width)
{
    SetAttribute("width", width);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetHeight(int height)
{
    SetAttribute("height", height);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetWidth(const string& width)
{
    SetOptionalAttribute("width", width);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetHeight(const string& height)
{
    SetOptionalAttribute("height", height);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetSize(int size)
{
    SetAttribute("size", size);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetAlign(const string& align)
{
    SetOptionalAttribute("align", align);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetVAlign(const string& align)
{
    SetOptionalAttribute("valign", align);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetColor(const string& color)
{
    SetOptionalAttribute("color", color);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetBgColor(const string& color)
{
    SetOptionalAttribute("bgcolor", color);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetNameAttribute(const string& name)
{
    SetAttribute("name", name);
    return this;
}

inline
const string& CHTMLNode::GetNameAttribute(void) const
{
    return GetAttribute("name");
}

inline
CHTMLNode* CHTMLNode::SetAccessKey(char key)
{
    SetAttribute("accesskey", string(1, key));
    return this;
}

inline
CHTMLNode* CHTMLNode::SetTitle(const string& title)
{
    SetAttribute("title", title);
    return this;
}

inline
CHTMLNode* CHTMLNode::SetStyle(const string& style)
{
    SetAttribute("style", style);
    return this;
}

inline
void CHTMLNode::AppendPlainText(const string& appendstring, bool noEncode)
{
    if ( !appendstring.empty() ) {
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
    }
}

inline
void CHTMLNode::AppendPlainText(const char* appendstring, bool noEncode)
{
    if ( appendstring && *appendstring ) {
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
    }
}

inline
void CHTMLNode::AppendHTMLText(const string& appendstring)
{
    if ( !appendstring.empty() ) {
        AppendChild(new CHTMLText(appendstring));
    }
}

inline
void CHTMLNode::AppendHTMLText(const char* appendstring)
{
    if ( appendstring && *appendstring ) {
        AppendChild(new CHTMLText(appendstring));
    }
}



inline
const string& CHTMLPlainText::GetText(void) const
{
    return m_Text;
}

inline
void CHTMLPlainText::SetText(const string& text)
{
    m_Text = text;
}

inline
const string& CHTMLText::GetText(void) const
{
    return m_Text;
}

inline
void CHTMLText::SetText(const string& text)
{
    m_Text = text;
}

inline
CHTMLListElement* CHTMLListElement::AppendItem(const char* text)
{
    AppendChild(new CHTML_li(text));
    return this;
}

inline
CHTMLListElement* CHTMLListElement::AppendItem(const string& text)
{
    AppendChild(new CHTML_li(text));
    return this;
}

inline
CHTMLListElement* CHTMLListElement::AppendItem(CNCBINode* node)
{
    AppendChild(new CHTML_li(node));
    return this;
}

inline
CHTML_tc* CHTML_table::NextCell(ECellType type)
{
    if ( m_CurrentRow == TIndex(-1) ) {
        m_CurrentRow = 0;
    }
    return Cell(m_CurrentRow, m_CurrentCol + 1, type);
}

inline
CHTML_tc* CHTML_table::NextRowCell(ECellType type)
{
    return Cell(m_CurrentRow + 1, 0, type);
}

inline
CHTML_tc* CHTML_table::InsertAt(TIndex row, TIndex column, CNCBINode* node)
{
    CHTML_tc* cell = Cell(row, column);
    cell->AppendChild(node);
    return cell;
}

inline
CHTML_tc* CHTML_table::InsertAt(TIndex row, TIndex column, const string& text)
{
    return InsertAt(row, column, new CHTMLPlainText(text));
}

inline
CHTML_tc* CHTML_table::InsertTextAt(TIndex row, TIndex column,
                                    const string& text)
{
    return InsertAt(row, column, text);
}

inline
CHTML_tc* CHTML_table::InsertNextCell(CNCBINode* node)
{
    CHTML_tc* cell = NextCell();
    cell->AppendChild(node);
    return cell;
}

inline
CHTML_tc* CHTML_table::InsertNextCell(const string& text)
{
    return InsertNextCell(new CHTMLPlainText(text));
}

inline
CHTML_tc* CHTML_table::InsertNextRowCell(CNCBINode* node)
{
    CHTML_tc* cell = NextRowCell();
    cell->AppendChild(node);
    return cell;
}

inline
CHTML_tc* CHTML_table::InsertNextRowCell(const string& text)
{
    return InsertNextRowCell(new CHTMLPlainText(text));
}

inline
CHTML_table* CHTML_table::SetCellSpacing(int spacing)
{
    SetAttribute("cellspacing", spacing);
    return this;
}

inline
CHTML_table* CHTML_table::SetCellPadding(int padding)
{
    SetAttribute("cellpadding", padding);
    return this;
}

inline
void CHTML_table::ResetTableCache(void)
{
    m_Cache.reset(0);
}

inline
CHTML_table* CHTML_table::SetColumnWidth(TIndex column, int width)
{
    return SetColumnWidth(column, NStr::IntToString(width));
}

inline
CHTML_table* CHTML_table::SetColumnWidth(TIndex column, const string& width)
{
    m_ColWidths[column] = width;
    return this;
}

inline
CHTML_ol::CHTML_ol(bool compact)
    : CParent(sm_TagName, compact)
{
    return;
}

inline
CHTML_ol::CHTML_ol(const char* type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_ol::CHTML_ol(const string& type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_ol::CHTML_ol(int start, bool compact)
    : CParent(sm_TagName, compact)
{
    SetStart(start);
}

inline
CHTML_ol::CHTML_ol(int start, const char* type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    SetStart(start);
}

inline
CHTML_ol::CHTML_ol(int start, const string& type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    SetStart(start);
}

inline
CHTML_ul::CHTML_ul(bool compact)
    : CParent(sm_TagName, compact)
{
    return;
}

inline
CHTML_ul::CHTML_ul(const char* type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_ul::CHTML_ul(const string& type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_dir::CHTML_dir(bool compact)
    : CParent(sm_TagName, compact)
{
    return;
}

inline
CHTML_dir::CHTML_dir(const char* type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_dir::CHTML_dir(const string& type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_menu::CHTML_menu(bool compact)
    : CParent(sm_TagName, compact)
{
    return;
}

inline
CHTML_menu::CHTML_menu(const char* type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_menu::CHTML_menu(const string& type, bool compact)
    : CParent(sm_TagName, type, compact)
{
    return;
}

inline
CHTML_a::CHTML_a(void)
    : CParent(sm_TagName)
{
    return;
}

inline
CHTML_a::CHTML_a(const string& href)
    : CParent(sm_TagName)
{
    SetHref(href);
}

inline
CHTML_a::CHTML_a(const string& href, CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetHref(href);
}

inline
CHTML_a::CHTML_a(const string& href, const char* text)
    : CParent(sm_TagName, text)
{
    SetHref(href);
}

inline
CHTML_a::CHTML_a(const string& href, const string& text)
    : CParent(sm_TagName, text)
{
    SetHref(href);
}

inline
CHTML_a* CHTML_a::SetHref(const string& href)
{
    SetAttribute("href", href);
    return this;
}

inline
CHTML_select::CHTML_select(const string& name, bool multiple)
    : CParent(sm_TagName)
{
    SetNameAttribute(name);
    if ( multiple ) {
        SetMultiple();
    }
}

inline
CHTML_select::CHTML_select(const string& name, int size, bool multiple)
    : CParent(sm_TagName)
{
    SetNameAttribute(name);
    SetSize(size);
    if ( multiple ) {
        SetMultiple();
    }
}

inline
CHTML_select* CHTML_select::SetMultiple(void)
{
    SetAttribute("multiple");
    return this;
}

inline CHTML_select*
CHTML_select::AppendOption(const string& value,
                           bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, selected, disabled));
    return this;
}

inline CHTML_select*
CHTML_select::AppendOption(const string& value,
                           const string& label,
                           bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, label, selected, disabled));
    return this;
}

inline CHTML_select*
CHTML_select::AppendOption(const string& value,
                           const char* label,
                           bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, label, selected, disabled));
    return this;
}

inline CHTML_select*
CHTML_select::AppendGroup(CHTML_optgroup* group)
{
    AppendChild(group);
    return this;
}

inline
CHTML_optgroup::CHTML_optgroup(const string& label, bool disabled)
    : CParent(sm_TagName)
{
    SetAttribute("label", label);
    if ( disabled ) {
        SetDisabled();
    }
}

inline CHTML_optgroup*
CHTML_optgroup::AppendOption(const string& value,
                             bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, selected, disabled));
    return this;
}

inline CHTML_optgroup*
CHTML_optgroup::AppendOption(const string& value,
                             const string& label,
                             bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, label, selected, disabled));
    return this;
}

inline CHTML_optgroup*
CHTML_optgroup::AppendOption(const string& value,
                             const char* label,
                             bool selected, bool disabled)
{
    AppendChild(new CHTML_option(value, label, selected, disabled));
    return this;
}

inline
CHTML_optgroup* CHTML_optgroup::SetDisabled(void)
{
    SetAttribute("disabled");
    return this;
}

inline
CHTML_option::CHTML_option(const string& value, bool selected, bool disabled)
    : CParent(sm_TagName, value)
{
    if ( selected ) {
        SetSelected();
    }
    if ( disabled ) {
        SetDisabled();
    }
}

inline
CHTML_option::CHTML_option(const string& value, const string& label,
                           bool selected, bool disabled)
    : CParent(sm_TagName, label)
{
    SetValue(value);
    if ( selected ) {
        SetSelected();
    }
    if ( disabled ) {
        SetDisabled();
    }
}

inline
CHTML_option::CHTML_option(const string& value, const char* label,
                           bool selected, bool disabled)
    : CParent(sm_TagName, label)
{
    SetValue(value);
    if ( selected ) {
        SetSelected();
    }
    if ( disabled ) {
        SetDisabled();
    }
}

inline
CHTML_option* CHTML_option::SetValue(const string& value)
{
    SetAttribute("value", value);
    return this;
}

inline
CHTML_option* CHTML_option::SetSelected(void)
{
    SetAttribute("selected");
    return this;
}

inline
CHTML_option* CHTML_option::SetDisabled(void)
{
    SetAttribute("disabled");
    return this;
}

inline
CHTML_br::CHTML_br(void)
    : CParent(sm_TagName)
{
    return;
}

inline CHTML_map*
CHTML_map::AddRect(const string& href, int x1, int y1, int x2, int y2,
                   const string& alt)
{
    AppendChild(new CHTML_area(href, x1, y1, x2, y2, alt));
    return this;
}

inline CHTML_map*
CHTML_map::AddCircle(const string& href, int x, int y, int radius,
                     const string& alt)
{
    AppendChild(new CHTML_area(href, x, y, radius, alt));
    return this;
}

inline CHTML_map*
CHTML_map::AddPolygon(const string& href, int coords[], int count,
                      const string& alt)
{
    AppendChild(new CHTML_area(href, coords, count, alt));
    return this;
}

inline CHTML_map*
CHTML_map::AddPolygon(const string& href, vector<int> coords,
                      const string& alt)
{
    AppendChild(new CHTML_area(href, coords, alt));
    return this;
}

inline CHTML_map*
CHTML_map::AddPolygon(const string& href, list<int> coords,
                      const string& alt)
{
    AppendChild(new CHTML_area(href, coords, alt));
    return this;
}

inline
CHTML_map* CHTML_map::AddArea(CHTML_area* area)
{
    AppendChild(area);
    return this;
}

inline
CHTML_map* CHTML_map::AddArea(CNodeRef& area)
{
    AppendChild(area);
    return this;
}


inline
CHTML_area::CHTML_area(void)
    : CParent(sm_TagName)
{
    return;
}

inline
CHTML_area::CHTML_area(const string& href, int x1, int y1, int x2, int y2,
                       const string& alt)
    : CParent(sm_TagName)
{
    SetHref(href);
    DefineRect(x1, y1, x2, y2);
    SetOptionalAttribute("alt", alt);
}

inline
CHTML_area::CHTML_area(const string& href, int x, int y, int radius,
                       const string& alt)
    : CParent(sm_TagName)
{
    SetHref(href);
    DefineCircle(x, y, radius);
    SetOptionalAttribute("alt", alt);
}

inline
CHTML_area::CHTML_area(const string& href, int coords[], int count,
                       const string& alt)
    : CParent(sm_TagName)
{
    SetHref(href);
    DefinePolygon(coords, count);
    SetOptionalAttribute("alt", alt);
}

inline
CHTML_area::CHTML_area(const string& href, vector<int> coords,
                       const string& alt)
    : CParent(sm_TagName)
{
    SetHref(href);
    DefinePolygon(coords);
    SetOptionalAttribute("alt", alt);
}

inline
CHTML_area::CHTML_area(const string& href, list<int> coords,
                       const string& alt)
    : CParent(sm_TagName)
{
    SetHref(href);
    DefinePolygon(coords);
    SetOptionalAttribute("alt", alt);
}

inline
CHTML_area* CHTML_area::SetHref(const string& href)
{
    SetAttribute("href", href);
    return this;
}

inline
CHTML_dl::CHTML_dl(bool compact)
    : CParent(sm_TagName)
{
    if ( compact )
        SetCompact();
}

inline
CHTML_basefont::CHTML_basefont(int size)
    : CParent(sm_TagName)
{
    SetSize(size);
}

inline
CHTML_basefont::CHTML_basefont(const string& typeface)
    : CParent(sm_TagName)
{
    SetTypeFace(typeface);
}

inline
CHTML_basefont::CHTML_basefont(const string& typeface, int size)
    : CParent(sm_TagName)
{
    SetTypeFace(typeface);
    SetSize(size);
}

inline
CHTML_font::CHTML_font(void)
    : CParent(sm_TagName)
{
    return;
}

inline
CHTML_font* CHTML_font::SetFontSize(int size, bool absolute)
{
    if ( absolute ) {
        SetSize(size);
    } else {
        SetRelativeSize(size);
    }
    return this;
}

inline
CHTML_font::CHTML_font(int size,
                       CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(int size,
                       const string& text)
    : CParent(sm_TagName, text)
{
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(int size,
                       const char* text)
    : CParent(sm_TagName, text)
{
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(int size, bool absolute,
                       CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetFontSize(size, absolute);
}

inline
CHTML_font::CHTML_font(int size, bool absolute,
                       const string& text)
    : CParent(sm_TagName, text)
{
    SetFontSize(size, absolute);
}

inline
CHTML_font::CHTML_font(int size, bool absolute,
                       const char* text)
    : CParent(sm_TagName, text)
{
    SetFontSize(size, absolute);
}

inline
CHTML_font::CHTML_font(const string& typeface,
                       CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetTypeFace(typeface);
}

inline
CHTML_font::CHTML_font(const string& typeface,
                       const string& text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
}

inline
CHTML_font::CHTML_font(const string& typeface,
                       const char* text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size,
                       CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetTypeFace(typeface);
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size,
                       const string& text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size,
                       const char* text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
    SetRelativeSize(size);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size, bool absolute,
                       CNCBINode* node)
    : CParent(sm_TagName, node)
{
    SetTypeFace(typeface);
    SetFontSize(size, absolute);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size, bool absolute,
                       const string& text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
    SetFontSize(size, absolute);
}

inline
CHTML_font::CHTML_font(const string& typeface, int size, bool absolute,
                       const char* text)
    : CParent(sm_TagName, text)
{
    SetTypeFace(typeface);
    SetFontSize(size, absolute);
}

inline
CHTML_color::CHTML_color(const string& color, const string& text)
{
    SetColor(color);
    AppendPlainText(text);
}

inline
CHTML_color::CHTML_color(const string& color, CNCBINode* node)
{
    SetColor(color);
    AppendChild(node);
}

inline
CHTML_hr* CHTML_hr::SetNoShade(bool noShade)
{
    if ( noShade ) {
        SetNoShade();
    }
    return this;
}

inline
CHTML_hr::CHTML_hr(bool noShade)
    : CParent(sm_TagName)
{
    SetNoShade(noShade);
}

inline
CHTML_hr::CHTML_hr(int size, bool noShade)
    : CParent(sm_TagName)
{
    SetSize(size);
    SetNoShade(noShade);
}

inline
CHTML_hr::CHTML_hr(int size, int width, bool noShade)
    : CParent(sm_TagName)
{
    SetSize(size);
    SetWidth(width);
    SetNoShade(noShade);
}

inline
CHTML_hr::CHTML_hr(int size, const string& width, bool noShade)
    : CParent(sm_TagName)
{
    SetSize(size);
    SetWidth(width);
    SetNoShade(noShade);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.41  2006/11/03 15:06:21  ivanov
 * Added OPTGROUP tag support
 *
 * Revision 1.40  2005/10/19 15:09:41  ivanov
 * CHTML_table -- init current row with -1 value, markink it as undefined
 *
 * Revision 1.39  2005/08/22 12:12:32  ivanov
 * Move some code from html.cpp to html.inl
 *
 * Revision 1.38  2004/12/27 14:27:32  ivanov
 * CHTML_map:: added AddArea() method
 *
 * Revision 1.37  2004/12/13 13:49:40  ivanov
 * Added CHTML_map and CHTML_area classes
 *
 * Revision 1.36  2004/07/20 16:36:37  ivanov
 * + CHTML_table::SetColumnWidth
 *
 * Revision 1.35  2004/02/04 12:45:11  ivanov
 * Made CHTML[Plain]Text::SetText() inline.
 *
 * Revision 1.34  2004/02/03 19:45:42  ivanov
 * Binded dummy names for the unnamed nodes
 *
 * Revision 1.33  2003/12/31 19:00:45  ivanov
 * Added default constructor for CHTML_a
 *
 * Revision 1.32  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.31  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.30  2003/11/03 15:07:55  ucko
 * Fixed typo in include guards: there must be *three* underscores after
 * the first "HTML".
 *
 * Revision 1.29  2003/11/03 14:47:01  ivanov
 * Some formal code rearrangement
 *
 * Revision 1.28  2002/09/25 01:24:29  dicuccio
 * Added CHTMLHelper::StripTags() - strips HTML comments and tags from any
 * string
 *
 * Revision 1.27  2002/01/17 23:37:50  ivanov
 * Moved CVS Log to end of file
 *
 * Revision 1.26  2001/06/08 19:01:42  ivanov
 * Added base classes: CHTMLDualNode, CHTMLSpecialChar
 *     (and based on it: CHTML_nbsp, _gt, _lt, _quot, _amp, _copy, _reg)
 * Added realization for tags <meta> (CHTML_meta) and <script> (CHTML_script)
 * Changed base class for tags LINK, PARAM, ISINDEX -> CHTMLOpenElement 
 * Added tags: OBJECT, NOSCRIPT
 * Added attribute "alt" for CHTML_img
 * Added CHTMLComment::Print() for disable print html-comments in plaintext mode
 *
 * Revision 1.25  2001/05/17 14:55:32  lavr
 * Typos corrected
 *
 * Revision 1.24  2000/10/18 13:25:47  vasilche
 * Added missing constructors to CHTML_font.
 *
 * Revision 1.23  2000/07/18 17:21:34  vasilche
 * Added possibility to force output of empty attribute value.
 * Added caching to CHTML_table, now large tables work much faster.
 * Changed algorithm of emitting EOL symbols in html output.
 *
 * Revision 1.22  1999/12/28 21:01:03  vasilche
 * Fixed conflict on MS VC between bool and const string& arguments by
 * adding const char* argument.
 *
 * Revision 1.21  1999/12/28 18:55:28  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.20  1999/10/29 18:28:53  vakatov
 * [MSVC]  bool vs. const string& arg confusion
 *
 * Revision 1.19  1999/10/28 13:40:30  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.18  1999/07/08 18:05:12  vakatov
 * Fixed compilation warnings
 *
 * Revision 1.17  1999/05/28 18:04:06  vakatov
 * CHTMLNode::  added attribute "CLASS"
 *
 * Revision 1.16  1999/05/10 17:01:11  vasilche
 * Fixed warning on Sun by renaming CHTML_font::SetSize() -> SetFontSize().
 *
 * Revision 1.15  1999/04/22 14:20:11  vasilche
 * Now CHTML_select::AppendOption and CHTML_option constructor accept option
 * name always as first argument.
 *
 * Revision 1.14  1999/04/15 19:48:17  vasilche
 * Fixed several warnings detected by GCC
 *
 * Revision 1.13  1999/04/15 16:28:37  vasilche
 * Fixed type in definition of list HTML tags.
 *
 * Revision 1.12  1999/04/08 19:00:26  vasilche
 * Added current cell pointer to CHTML_table
 *
 * Revision 1.11  1999/02/02 17:57:47  vasilche
 * Added CHTML_table::Row(int row).
 * Linkbar now have equal image spacing.
 *
 * Revision 1.10  1999/01/28 16:58:59  vasilche
 * Added several constructors for CHTML_hr.
 * Added CHTMLNode::SetSize method.
 *
 * Revision 1.9  1999/01/25 19:34:15  vasilche
 * String arguments which are added as HTML text now treated as plain text.
 *
 * Revision 1.8  1999/01/21 21:12:54  vasilche
 * Added/used descriptions for HTML submit/select/text.
 * Fixed some bugs in paging.
 *
 * Revision 1.7  1999/01/11 15:13:33  vasilche
 * Fixed CHTML_font size.
 * CHTMLHelper extracted to separate file.
 *
 * Revision 1.6  1999/01/07 16:41:53  vasilche
 * CHTMLHelper moved to separate file.
 * TagNames of CHTML classes ara available via s_GetTagName() static
 * method.
 * Input tag types ara available via s_GetInputType() static method.
 * Initial selected database added to CQueryBox.
 * Background colors added to CPagerBax & CSmallPagerBox.
 *
 * Revision 1.5  1999/01/05 21:47:11  vasilche
 * Added 'current page' to CPageList.
 * CPageList doesn't display forward/backward if empty.
 *
 * Revision 1.4  1999/01/04 20:06:10  vasilche
 * Redesigned CHTML_table.
 * Added selection support to HTML forms (via hidden values).
 *
 * Revision 1.3  1998/12/24 16:15:36  vasilche
 * Added CHTMLComment class.
 * Added TagMappers from static functions.
 *
 * Revision 1.2  1998/12/23 21:20:57  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.1  1998/12/21 22:24:56  vasilche
 * A lot of cleaning.
 *
 * ===========================================================================
 */

#endif /* def HTML___HTML__HPP  &&  ndef HTML___HTML__INL */
