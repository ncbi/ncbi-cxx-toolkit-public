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
* Revision 1.2  1998/12/23 21:20:57  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.1  1998/12/21 22:24:56  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

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
    SetAttribute(KHTMLAttributeName_width, width);
    return this;
}

inline CHTMLNode* CHTMLNode::SetHeight(const string& height)
{
    SetAttribute(KHTMLAttributeName_height, height);
    return this;
}

inline CHTMLNode* CHTMLNode::SetAlign(const string& align)
{
    SetAttribute(KHTMLAttributeName_align, align);
    return this;
}

inline CHTMLNode* CHTMLNode::SetColor(const string& color)
{
    SetAttribute(KHTMLAttributeName_color, color);
    return this;
}

inline CHTMLNode* CHTMLNode::SetBgColor(const string& color)
{
    SetAttribute(KHTMLAttributeName_bgcolor, color);
    return this;
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
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(void)
    : CParent(*TagName)
{
}

template<const string* TagName>
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(CNCBINode* node)
    : CParent(*TagName, node)
{
}

template<const string* TagName>
inline CHTMLElementTmpl<TagName>::CHTMLElementTmpl(const string& text)
    : CParent(*TagName, text)
{
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(void)
    : CParent(*TagName)
{
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(CNCBINode* node)
    : CParent(*TagName, node)
{
}

template<const string* TagName>
inline CHTMLOpenElementTmpl<TagName>::CHTMLOpenElementTmpl(const string& text)
    : CParent(*TagName, text)
{
}
*/
template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(void)
    : CParent(*TagName)
{
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(const string& type)
    : CParent(*TagName)
{
    SetAttribute(KHTMLAttributeName_type, type);
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(bool compact)
    : CParent(*TagName)
{
    SetOptionalAttribute(KHTMLAttributeName_compact, compact);
}

template<const string* TagName>
inline CHTMLListElementTmpl<TagName>::CHTMLListElementTmpl(const string& type, bool compact)
    : CParent(*TagName)
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

inline CHTML_option::CHTML_option(const string& content, bool selected)
    : CParent(content)
{
    SetOptionalAttribute(KHTMLAttributeName_selected, selected);
};

inline CHTML_option::CHTML_option(const string& content, const string& value, bool selected)
    : CParent(content)
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
    SetAttribute(KHTMLAttributeName_size, size);
    SetOptionalAttribute(KHTMLAttributeName_multiple, multiple);
}

inline CHTML_select* CHTML_select::AppendOption(const string& option, bool selected)
{
    AppendChild(new CHTML_option(option, selected));
    return this;
};


inline CHTML_select* CHTML_select::AppendOption(const string& option, const string& value, bool selected)
{
    AppendChild(new CHTML_option(option, value, selected));
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
    SetAttribute(KHTMLAttributeName_size, size);
}

inline CHTML_basefont::CHTML_basefont(const string& typeface)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_basefont::CHTML_basefont(const string& typeface, int size)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetAttribute(KHTMLAttributeName_size, size);
}

inline CHTML_font::CHTML_font(int size, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_size, size);
}

inline CHTML_font::CHTML_font(const string& typeface, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, CNCBINode* node)
    : CParent(node)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetAttribute(KHTMLAttributeName_size, size);
}

inline CHTML_font::CHTML_font(int size, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_size, size);
}

inline CHTML_font::CHTML_font(const string& typeface, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
}

inline CHTML_font::CHTML_font(const string& typeface, int size, const string& text)
    : CParent(text)
{
    SetAttribute(KHTMLAttributeName_face, typeface);
    SetAttribute(KHTMLAttributeName_size, size);
}

#endif /* def HTML__HPP  &&  ndef HTML__INL */
