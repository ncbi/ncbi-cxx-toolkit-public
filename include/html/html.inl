#if defined(HTML__HPP)  &&  !defined(HTML__INL)
#define HTML__INL

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
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
* Fixed type in defenition of list HTML tags.
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

inline CHTMLNode::CHTMLNode(void)
{
}

inline CHTMLNode::CHTMLNode(const string& name)
    : CParent(name)
{
}

inline CHTMLNode* CHTMLNode::SetClass(const string& class_name)
{
    SetOptionalAttribute(KHTMLAttributeName_class, class_name);
    return this;
}

inline CHTMLNode* CHTMLNode::SetWidth(int width)
{
    SetAttribute(KHTMLAttributeName_width, width);
    return this;
}

inline CHTMLNode* CHTMLNode::SetHeight(int height)
{
    SetAttribute(KHTMLAttributeName_height, height);
    return this;
}

inline CHTMLNode* CHTMLNode::SetWidth(const string& width)
{
    SetOptionalAttribute(KHTMLAttributeName_width, width);
    return this;
}

inline CHTMLNode* CHTMLNode::SetHeight(const string& height)
{
    SetOptionalAttribute(KHTMLAttributeName_height, height);
    return this;
}

inline CHTMLNode* CHTMLNode::SetSize(int size)
{
    SetAttribute(KHTMLAttributeName_size, size);
    return this;
}

inline CHTMLNode* CHTMLNode::SetAlign(const string& align)
{
    SetOptionalAttribute(KHTMLAttributeName_align, align);
    return this;
}

inline CHTMLNode* CHTMLNode::SetVAlign(const string& align)
{
    SetOptionalAttribute(KHTMLAttributeName_valign, align);
    return this;
}

inline CHTMLNode* CHTMLNode::SetColor(const string& color)
{
    SetOptionalAttribute(KHTMLAttributeName_color, color);
    return this;
}

inline CHTMLNode* CHTMLNode::SetBgColor(const string& color)
{
    SetOptionalAttribute(KHTMLAttributeName_bgcolor, color);
    return this;
}

inline CHTMLNode* CHTMLNode::SetNameAttribute(const string& name)
{
    SetAttribute(KHTMLAttributeName_name, name);
    return this;
}

inline string CHTMLNode::GetNameAttribute(void) const
{
    return GetAttribute(KHTMLAttributeName_name);
}

inline const string& CHTMLPlainText::GetText(void) const
{
    return m_Text;
}

inline const string& CHTMLText::GetText(void) const
{
    return m_Text;
}
/*
template<const string* TagName>
inline const string& CHTMLElementTmpl<TagName>::s_GetTagName(void)
{
    return *TagName;
}

template<const string* TagName>
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(void)
    : CParent(s_GetTagName())
{
}

template<const string* TagName>
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(CNCBINode* node)
    : CParent(s_GetTagName(), node)
{
}

template<const string* TagName>
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(const string& text)
    : CParent(s_GetTagName(), text)
{
}

template<const string* TagName>
inline const string& CHTMLOpenElementTmpl<TagName>::s_GetTagName(void)
{
    return *TagName;
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(void)
    : CParent(s_GetTagName())
{
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(CNCBINode* node)
    : CParent(s_GetTagName(), node)
{
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(const string& text)
    : CParent(s_GetTagName(), text)
{
}
*/

template<const string* TagName>
inline const string& CHTMLListElementTmpl<TagName>::s_GetTagName(void)
{
    return *TagName;
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(void)
    : CParent(s_GetTagName())
{
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(const string& type)
    : CParent(s_GetTagName())
{
    SetAttribute(KHTMLAttributeName_type, type);
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(bool compact)
    : CParent(s_GetTagName())
{
    SetOptionalAttribute(KHTMLAttributeName_compact, compact);
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(const string& type, bool compact)
    : CParent(s_GetTagName())
{
    SetAttribute(KHTMLAttributeName_type, type);
    SetOptionalAttribute(KHTMLAttributeName_compact, compact);
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>* CHTMLListElementTmpl<TagName>::AppendItem(const string& item)
{
    AppendChild(new CHTML_li(item));
    return this;
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>* CHTMLListElementTmpl<TagName>::AppendItem(CNCBINode* item)
{
    AppendChild(new CHTML_li(item));
    return this;
}

inline CHTML_tc* CHTML_table::NextCell(ECellType type)
{
    return Cell(m_CurrentRow, m_CurrentCol + 1, type);
}

inline CHTML_tc* CHTML_table::NextRowCell(ECellType type)
{
    return Cell(m_CurrentRow + 1, 0, type);
}

inline CHTML_tc* CHTML_table::InsertAt(TIndex row, TIndex column, CNCBINode* node)
{
    CHTML_tc* cell = Cell(row, column);
    cell->AppendChild(node);
    return cell;
}

inline CHTML_tc* CHTML_table::InsertAt(TIndex row, TIndex column, const string& text)
{
    return InsertAt(row, column, new CHTMLPlainText(text));
}

inline CHTML_tc* CHTML_table::InsertTextAt(TIndex row, TIndex column,
                                           const string& text)
{
    return InsertAt(row, column, text);
}

inline CHTML_tc* CHTML_table::InsertNextCell(CNCBINode* node)
{
    CHTML_tc* cell = NextCell();
    cell->AppendChild(node);
    return cell;
}

inline CHTML_tc* CHTML_table::InsertNextCell(const string& text)
{
    return InsertNextCell(new CHTMLPlainText(text));
}

inline CHTML_tc* CHTML_table::InsertNextRowCell(CNCBINode* node)
{
    CHTML_tc* cell = NextRowCell();
    cell->AppendChild(node);
    return cell;
}

inline CHTML_tc* CHTML_table::InsertNextRowCell(const string& text)
{
    return InsertNextRowCell(new CHTMLPlainText(text));
}

inline void CHTML_table::CheckTable(void) const
{
    x_CheckTable(0);
}

inline CHTML_ol::CHTML_ol(bool compact)
    : CParent(compact)
{
}

inline CHTML_ol::CHTML_ol(const string& type, bool compact)
    : CParent(type, compact)
{
}

inline CHTML_ol::CHTML_ol(int start, bool compact)
    : CParent(compact)
{
    SetAttribute(KHTMLAttributeName_start, start);
}

inline CHTML_ol::CHTML_ol(int start, const string& type, bool compact)
    : CParent(type, compact)
{
    SetAttribute(KHTMLAttributeName_start, start);
}

inline CHTML_a::CHTML_a(const string& href, CNCBINode* node)
    : CParent(node)
{
    SetOptionalAttribute(KHTMLAttributeName_href, href);
}

inline CHTML_a::CHTML_a(const string& href, const string& text)
    : CParent(text)
{
    SetOptionalAttribute(KHTMLAttributeName_href, href);
}

inline CHTML_option::CHTML_option(const string& value, bool selected)
    : CParent(value)
{
    SetOptionalAttribute(KHTMLAttributeName_selected, selected);
};

inline CHTML_option::CHTML_option(const string& value, const string& label, bool selected)
    : CParent(label)
{
    SetAttribute(KHTMLAttributeName_value, value);
    SetOptionalAttribute(KHTMLAttributeName_selected, selected);
};

inline CHTML_select::CHTML_select(const string& name, bool multiple)
{
    SetAttribute(KHTMLAttributeName_name, name);
    SetOptionalAttribute(KHTMLAttributeName_multiple, multiple);
}

inline CHTML_select::CHTML_select(const string& name, int size, bool multiple)
{
    SetAttribute(KHTMLAttributeName_name, name);
    SetSize(size);
    SetOptionalAttribute(KHTMLAttributeName_multiple, multiple);
}

inline CHTML_select* CHTML_select::AppendOption(const string& value, bool selected)
{
    AppendChild(new CHTML_option(value, selected));
    return this;
};


inline CHTML_select* CHTML_select::AppendOption(const string& value, const string& label, bool selected)
{
    AppendChild(new CHTML_option(value, label, selected));
    return this;
};

inline CHTML_br::CHTML_br(void)
{
}

inline CHTML_dl::CHTML_dl(bool compact)
{
    SetOptionalAttribute(KHTMLAttributeName_compact, compact);
}

inline CHTML_basefont::CHTML_basefont(int size)
{
    SetSize(size);
}

inline CHTML_basefont::CHTML_basefont(const string& typeface)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_basefont::CHTML_basefont(const string& typeface, int size)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetSize(size);
}

inline CHTML_font::CHTML_font(void)
{
}

inline CHTML_font::CHTML_font(int size, CNCBINode* node)
    : CParent(node)
{
    SetRelativeSize(size);
}

inline CHTML_font::CHTML_font(int size, const string& text)
    : CParent(text)
{
    SetRelativeSize(size);
}

inline CHTML_font* CHTML_font::SetFontSize(int size, bool absolute)
{
    if ( absolute )
        SetSize(size);
    else
        SetRelativeSize(size);
    return this;
}

inline CHTML_font::CHTML_font(int size, bool absolute, CNCBINode* node)
    : CParent(node)
{
    SetFontSize(size, absolute);
}

inline CHTML_font::CHTML_font(int size, bool absolute, const string& text)
    : CParent(text)
{
    SetFontSize(size, absolute);
}

inline CHTML_font::CHTML_font(const string& typeface, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_font::CHTML_font(const string& typeface, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetRelativeSize(size);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetRelativeSize(size);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, bool absolute, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetFontSize(size, absolute);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, bool absolute, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetFontSize(size, absolute);
}

inline CHTML_color::CHTML_color(const string& color, const string& text)
{
    SetColor(color);
    AppendPlainText(text);
}

inline CHTML_color::CHTML_color(const string& color, CNCBINode* node)
{
    SetColor(color);
    AppendChild(node);
}

inline CHTML_hr* CHTML_hr::SetNoShade(void)
{
    SetAttribute(KHTMLAttributeName_noshade);
    return this;
}

inline CHTML_hr* CHTML_hr::SetNoShade(bool noShade)
{
    if ( noShade )
        SetNoShade();
    return this;
}

inline CHTML_hr::CHTML_hr(bool noShade)
{
    SetNoShade(noShade);
}

inline CHTML_hr::CHTML_hr(int size, bool noShade)
{
    SetSize(size);
    SetNoShade(noShade);
}

inline CHTML_hr::CHTML_hr(int size, int width, bool noShade)
{
    SetSize(size);
    SetWidth(width);
    SetNoShade(noShade);
}

inline CHTML_hr::CHTML_hr(int size, const string& width, bool noShade)
{
    SetSize(size);
    SetWidth(width);
    SetNoShade(noShade);
}

#endif /* def HTML__HPP  &&  ndef HTML__INL */
