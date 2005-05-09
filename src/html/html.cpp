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
 * Author:  Lewis Geer
 *
 */


#include <ncbi_pch.hpp>
#include <html/html.hpp>
#include <html/htmlhelper.hpp>
#include <html/indentstream.hpp>
#include <html/html_exception.hpp>
#include <errno.h>
#include <string.h>


BEGIN_NCBI_SCOPE


/// Tag delimiters
const char* kTagStart    = "<@";   ///< Tag start.
const char* kTagEnd      = "@>";   ///< Tag end.
const char* kTagStartEnd = "</@";  ///< Tag start in the end of
                                   ///< block definition.

#define INIT_STREAM_WRITE  \
    errno = 0

#define CHECK_STREAM_WRITE(out)                                               \
    if ( !out ) {                                                             \
        int x_errno = errno;                                                  \
        string x_err("write to stream failed");                               \
        if (x_errno != 0) {                                                   \
            const char* x_strerror = strerror(x_errno);                       \
            if ( !x_strerror ) {                                              \
                x_strerror = "Error code is out of range";                    \
            }                                                                 \
            string x_strerrno = NStr::IntToString(x_errno);                   \
            x_err += " {errno=" + x_strerrno + ',' + x_strerror + '}';        \
        }                                                                     \
        NCBI_THROW(CHTMLException, eWrite, x_err);                            \
    }


static string s_GenerateNodeInternalName(const string& basename,
                                         const string& v1,
                                         const string& v2 = kEmptyStr)
{
    string name(basename);
    if ( !v1.empty() ) {
        name += "(\"" + v1.substr(0,10) + "\"";
        if ( !v2.empty() ) {
            name += "|\"" + v2.substr(0,10) + "\"";
        }
        name += ")";
    }
    return name;
}


// CHTMLNode

CHTMLNode::~CHTMLNode(void)
{
    return;
}

CHTMLNode* CHTMLNode::SetClass(const string& class_name)
{
    SetOptionalAttribute("class", class_name);
    return this;
}

CHTMLNode* CHTMLNode::SetId(const string& class_name)
{
    SetOptionalAttribute("id", class_name);
    return this;
}

CHTMLNode* CHTMLNode::SetWidth(int width)
{
    SetAttribute("width", width);
    return this;
}

CHTMLNode* CHTMLNode::SetHeight(int height)
{
    SetAttribute("height", height);
    return this;
}

CHTMLNode* CHTMLNode::SetWidth(const string& width)
{
    SetOptionalAttribute("width", width);
    return this;
}

CHTMLNode* CHTMLNode::SetHeight(const string& height)
{
    SetOptionalAttribute("height", height);
    return this;
}

CHTMLNode* CHTMLNode::SetSize(int size)
{
    SetAttribute("size", size);
    return this;
}

CHTMLNode* CHTMLNode::SetAlign(const string& align)
{
    SetOptionalAttribute("align", align);
    return this;
}

CHTMLNode* CHTMLNode::SetVAlign(const string& align)
{
    SetOptionalAttribute("valign", align);
    return this;
}

CHTMLNode* CHTMLNode::SetColor(const string& color)
{
    SetOptionalAttribute("color", color);
    return this;
}

CHTMLNode* CHTMLNode::SetBgColor(const string& color)
{
    SetOptionalAttribute("bgcolor", color);
    return this;
}

CHTMLNode* CHTMLNode::SetNameAttribute(const string& name)
{
    SetAttribute("name", name);
    return this;
}

const string& CHTMLNode::GetNameAttribute(void) const
{
    return GetAttribute("name");
}

CHTMLNode* CHTMLNode::SetAccessKey(char key)
{
    SetAttribute("accesskey", string(1, key));
    return this;
}

CHTMLNode* CHTMLNode::SetTitle(const string& title)
{
    SetAttribute("title", title);
    return this;
}

CHTMLNode* CHTMLNode::SetStyle(const string& style)
{
    SetAttribute("style", style);
    return this;
}

void CHTMLNode::AppendPlainText(const string& appendstring, bool noEncode)
{
    if ( !appendstring.empty() ) {
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
    }
}

void CHTMLNode::AppendPlainText(const char* appendstring, bool noEncode)
{
    if ( appendstring && *appendstring ) {
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
    }
}

void CHTMLNode::AppendHTMLText(const string& appendstring)
{
    if ( !appendstring.empty() ) {
        AppendChild(new CHTMLText(appendstring));
    }
}

void CHTMLNode::AppendHTMLText(const char* appendstring)
{
    if ( appendstring && *appendstring ) {
        AppendChild(new CHTMLText(appendstring));
    }
}

string CHTMLNode::GetEventHandlerName(const EHTML_EH_Attribute name) const
{
    switch (name) {

    case eHTML_EH_Blur:
        return "onBlur";
    case eHTML_EH_Change:
        return "onChange";
    case eHTML_EH_Click:
        return "onClick";
    case eHTML_EH_DblClick:
        return "onDblClick";
    case eHTML_EH_Focus:
        return "onFocus";
    case eHTML_EH_Load:
        return "onLoad";
    case eHTML_EH_Unload:
        return "onUnload";
    case eHTML_EH_MouseDown:
        return "onMouseDown";
    case eHTML_EH_MouseUp:
        return "onMouseUp";
    case eHTML_EH_MouseMove:
        return "onMouseMove";
    case eHTML_EH_MouseOver:
        return "onMouseOver";
    case eHTML_EH_MouseOut:
        return "onMouseOut";
    case eHTML_EH_Select:
        return "onSelect";
    case eHTML_EH_Submit:
        return "onSubmit";
    case eHTML_EH_KeyDown:
        return "onKeyDown";
    case eHTML_EH_KeyPress:
        return "onKeyPress";
    case eHTML_EH_KeyUp:
        return "onKeyUp";
    }
    _TROUBLE;
    return kEmptyStr;
}


void CHTMLNode::SetEventHandler(const EHTML_EH_Attribute event,
                                const string& value)
{
    if ( value.empty() ) {
        return;
    }
    SetAttribute(GetEventHandlerName(event), value);
}


void CHTMLNode::AttachPopupMenu(const CHTMLPopupMenu* menu,
                                EHTML_EH_Attribute    event,
                                bool                  cancel_default_event)
{
    if ( !menu ) {
        return;
    }
    const string kStopEvent = " return false;";
    string show;
    string hide;

    switch (menu->GetType()) {
    case CHTMLPopupMenu::eSmith: 
        show = menu->ShowMenu();
        if ( cancel_default_event ) {
            show += kStopEvent;
        }
        SetEventHandler(event, show);
        return;
    case CHTMLPopupMenu::eKurdin: 
    case CHTMLPopupMenu::eKurdinConf:
        {
            show = menu->ShowMenu();
            hide = menu->HideMenu();
            if ( cancel_default_event ) {
                show += kStopEvent;
                hide += kStopEvent;
            }
            SetEventHandler(event, show);
            SetEventHandler(eHTML_EH_MouseOut, hide);
        }
        return;
    case CHTMLPopupMenu::eKurdinSide:
        AppendHTMLText(menu->ShowMenu());
        return;
    }
    _TROUBLE;
}


// <@XXX@> mapping tag node

CHTMLTagNode::CHTMLTagNode(const char* name)
    : CParent(name)
{
    return;
}

CHTMLTagNode::CHTMLTagNode(const string& name)
    : CParent(name)
{
    return;
}

CHTMLTagNode::~CHTMLTagNode(void)
{
    return;
}

CNcbiOstream& CHTMLTagNode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    CNodeRef node = MapTagAll(GetName(), mode);
    if ( node ) {
        node->Print(out, mode);
    }
    return out;
}


// Dual text node.

CHTMLDualNode::CHTMLDualNode(const char* html, const char* plain)
    : CParent(s_GenerateNodeInternalName("dualnode", html, plain))
{
    AppendChild(new CHTMLText(html));
    m_Plain = plain;
}

CHTMLDualNode::CHTMLDualNode(CNCBINode* child, const char* plain)
    : CParent(s_GenerateNodeInternalName("dualnode", "[node]", plain))
{
    AppendChild(child);
    m_Plain = plain;
}

CHTMLDualNode::~CHTMLDualNode(void)
{
    return;
}

CNcbiOstream& CHTMLDualNode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        INIT_STREAM_WRITE;
        out << m_Plain;
        CHECK_STREAM_WRITE(out);
        return out;
    } else {
        return CParent::PrintChildren(out, mode);
    }
}


// plain text node

CHTMLPlainText::CHTMLPlainText(const char* text, bool noEncode)
    : CNCBINode(s_GenerateNodeInternalName("plaintext", text)),
      m_NoEncode(noEncode), m_Text(text)
{
    return;
}

CHTMLPlainText::CHTMLPlainText(const string& text, bool noEncode)
    : CNCBINode(s_GenerateNodeInternalName("plaintext", text)),
      m_NoEncode(noEncode), m_Text(text)
{
    return;
}

CHTMLPlainText::~CHTMLPlainText(void)
{
    return;
}


CNcbiOstream& CHTMLPlainText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    string str;
    if (mode == ePlainText  ||  NoEncode()) {
        str = GetText();
    } else {
        str = CHTMLHelper::HTMLEncode(GetText());
    }
    INIT_STREAM_WRITE;
    out << str;
    CHECK_STREAM_WRITE(out);
    return out;
}


// text node

CHTMLText::CHTMLText(const string& text, TFlags flags)
    : CParent(s_GenerateNodeInternalName("htmltext", text)),
      m_Text(text), m_Flags(flags)
{
    return;
}

CHTMLText::CHTMLText(const char* text, TFlags flags)
    : CParent(s_GenerateNodeInternalName("htmltext", text)),
      m_Text(text), m_Flags(flags)
{
    return;
}

CHTMLText::~CHTMLText(void)
{
    return;
}


static SIZE_TYPE s_Find(const string& s, const char* target,
                        SIZE_TYPE start = 0)
{
    // Return s.find(target);
    // Some implementations of string::find call memcmp at every
    // possible position, which is way too slow.
    if ( start >= s.size() ) {
        return NPOS;
    }
    const char* cstr = s.c_str();
    const char* p    = strstr(cstr + start, target);
    return p ? p - cstr : NPOS;
}


CNcbiOstream& CHTMLText::x_PrintBegin(CNcbiOstream& out, TMode mode,
                                      const string& s) const
{
    TFlags flags = 0;
    if ( mode == ePlainText ) {
        if ( m_Flags & fStripTextMode )
            flags |= fStrip;
        if ( m_Flags & fEncodeTextMode )
            flags |= fEncode;
    } else {
        if ( m_Flags & fStripHtmlMode ) 
            flags |= fStrip;
        if ( m_Flags & fEncodeHtmlMode )
            flags |= fEncode;
    }
    string str;
    const string *pstr = &str;
    switch (flags) {
        case fStrip:
            str = CHTMLHelper::StripHTML(s);
            break;
        case fEncode:
            str = CHTMLHelper::HTMLEncode(s);
            break;
        case fStrip | fEncode:
            str = CHTMLHelper::HTMLEncode(CHTMLHelper::StripHTML(s));
            break;
        default:
            pstr = &s;
    }
    INIT_STREAM_WRITE;
    out.write(pstr->data(), pstr->size());
    CHECK_STREAM_WRITE(out);
    return out;
}


#define PRINT_TMP_STR \
    if ( enable_buffering ) { \
        pstr->write(tmp.data(), tmp.size()); \
    } else { \
        x_PrintBegin(out, mode, tmp); \
    }

CNcbiOstream& CHTMLText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    const string& text = GetText();
    SIZE_TYPE tagStart = s_Find(text, kTagStart);
    if ( tagStart == NPOS ) {
        return x_PrintBegin(out, mode, text);
    }

    SIZE_TYPE tag_start_size = strlen(kTagStart);
    SIZE_TYPE tag_end_size   = strlen(kTagEnd);

    // Check flag to enabe output buffering
    bool enable_buffering = !(m_Flags & fDisableBuffering);
    CNcbiOstrstream *pstr = 0;
    if ( enable_buffering ) {
        pstr = new CNcbiOstrstream;
    }

    // Printout string before the first start mapping tag
    string tmp = text.substr(0, tagStart);
    PRINT_TMP_STR;

    // Map all tags
    SIZE_TYPE last = tagStart;
    do {
        SIZE_TYPE tagNameStart = tagStart + tag_start_size;
        SIZE_TYPE tagNameEnd = s_Find(text, kTagEnd, tagNameStart);
        if ( tagNameEnd == NPOS ) {
            // tag not closed
            NCBI_THROW(CHTMLException, eTextUnclosedTag, "tag not closed");
        }
        else {
            // tag found
            if ( last != tagStart ) {
                tmp = text.substr(last, tagStart - last);
                PRINT_TMP_STR;
            }
            string name = text.substr(tagNameStart,tagNameEnd-tagNameStart);
            // Resolve and repeat tag
            for (;;) {
                CNodeRef tag = MapTagAll(name, mode);
                if ( tag ) {
                    if ( enable_buffering ) {
                        tag->Print(*pstr, mode);
                    } else {
                        tag->Print(out, mode);
                    }
                    if ( tag->NeedRepeatTag() ) {
                        RepeatTag(false);
                        continue;
                    }
                }
                break;
            }
            last = tagNameEnd + tag_end_size;
            tagStart = s_Find(text, kTagStart, last);
        }
    } while ( tagStart != NPOS );

    if ( last != text.size() ) {
        tmp = text.substr(last);
        PRINT_TMP_STR;
    }
    if ( enable_buffering ) {
        x_PrintBegin(out, mode, CNcbiOstrstreamToString(*pstr));
        delete pstr;
    }
    return out;
}


CHTMLOpenElement::~CHTMLOpenElement(void)
{
    return;
}


CNcbiOstream& CHTMLOpenElement::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode != ePlainText ) {
        out << '<' << m_Name;
        if ( HaveAttributes() ) {
            for ( TAttributes::const_iterator i = Attributes().begin();
                  i != Attributes().end(); ++i ) {
                INIT_STREAM_WRITE;
                out << ' ' << i->first;
                CHECK_STREAM_WRITE(out);
                if ( !i->second.IsOptional() ||
                     !i->second.GetValue().empty() ) {
                    out << "=\"" << i->second.GetValue() << '"';
                }
            }
        }
        if ( m_NoWrap ) {
            out << " nowrap";
        }
        INIT_STREAM_WRITE;
        out << '>';
        CHECK_STREAM_WRITE(out);
    }
    return out;
}


CHTMLInlineElement::~CHTMLInlineElement(void)
{
    return;
}


CNcbiOstream& CHTMLInlineElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if ( mode != ePlainText ) {
        INIT_STREAM_WRITE;
        out << "</" << m_Name << '>';
        CHECK_STREAM_WRITE(out);
    }
    return out;
}


CHTMLElement::~CHTMLElement(void)
{
    return;
}


CNcbiOstream& CHTMLElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    CParent::PrintEnd(out, mode);
    if ( mode != ePlainText ) {
        const TMode* previous = mode.GetPreviousContext();
        INIT_STREAM_WRITE;
        if ( previous ) {
            CNCBINode* parent = previous->GetNode();
            if ( parent && parent->HaveChildren() &&
                 parent->Children().size() > 1 )
                out << '\n'; // separate child nodes by newline
        } else {
            out << '\n';
        }
        CHECK_STREAM_WRITE(out);
    }
    return out;
}


CHTMLBlockElement::~CHTMLBlockElement(void)
{
    return;
}


CNcbiOstream& CHTMLBlockElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    CParent::PrintEnd(out, mode);
    if ( mode == ePlainText ) {
        // Add a newline iff no node on the path to the last descendant
        // is also a block element. We only need one break.
        CNCBINode* node = this;
        while (node->HaveChildren()) {
            node = node->Children().back();
            if (dynamic_cast<CHTMLBlockElement*>(node)) {
                return out;
            }
        }
        INIT_STREAM_WRITE;
        out << '\n';
        CHECK_STREAM_WRITE(out);
    }
    return out;
}


// HTML comment.

const char CHTMLComment::sm_TagName[] = "comment";

CHTMLComment::~CHTMLComment(void)
{
    return;
}

CNcbiOstream& CHTMLComment::Print(CNcbiOstream& out, TMode mode)
{
    if (mode == ePlainText) {
        return out;
    }
    return CParent::Print(out, mode);
}

CNcbiOstream& CHTMLComment::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if (mode == eHTML) {
        INIT_STREAM_WRITE;
        out << "<!--";
        CHECK_STREAM_WRITE(out);
    }
    return out;
}

CNcbiOstream& CHTMLComment::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if (mode == eHTML) {
        INIT_STREAM_WRITE;
        out << "-->";
        CHECK_STREAM_WRITE(out);
    }
    return out;
}

CHTMLListElement::~CHTMLListElement(void)
{
    return;
}

CHTMLListElement* CHTMLListElement::SetType(const char* type)
{
    SetAttribute("type", type);
    return this;
}

CHTMLListElement* CHTMLListElement::SetType(const string& type)
{
    SetAttribute("type", type);
    return this;
}

CHTMLListElement* CHTMLListElement::SetCompact(void)
{
    SetOptionalAttribute("compact", true);
    return this;
}

CNcbiOstream& CHTMLListElement::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if (mode == ePlainText) {
        CIndentingOstream out2(out);
        CHTMLElement::PrintChildren(out2, mode);
    } else {
        CHTMLElement::PrintChildren(out, mode);
    }
    return out;
}


// Special char.

CHTMLSpecialChar::CHTMLSpecialChar(const char* html, const char* plain,
                                   int count)
    : CParent("", plain)
{
    m_Name  = s_GenerateNodeInternalName("specialchar", html);
    m_Html  = html;
    m_Count = count;
}


CHTMLSpecialChar::~CHTMLSpecialChar(void)
{
    return;
}


CNcbiOstream& CHTMLSpecialChar::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        for ( int i = 0; i < m_Count; i++ ) {
            INIT_STREAM_WRITE;
            out << m_Plain;
            CHECK_STREAM_WRITE(out);
        }
    } else {
        for ( int i = 0; i < m_Count; i++ ) {
            INIT_STREAM_WRITE;
            out << "&" << m_Html << ";";
            CHECK_STREAM_WRITE(out);
        }
    }
    return out;
}


// <html> tag.

const char CHTML_html::sm_TagName[] = "html";

CHTML_html::~CHTML_html(void)
{
    return;
}

void CHTML_html::Init(void)
{
    return;
}

void CHTML_html::EnablePopupMenu(CHTMLPopupMenu::EType type,
                                 const string& menu_script_url,
                                 bool use_dynamic_menu)
{
    SPopupMenuInfo info(menu_script_url, use_dynamic_menu);
    m_PopupMenus[type] = info;
}

static bool s_CheckUsePopupMenus(const CNCBINode* node, 
                                 CHTMLPopupMenu::EType type)
{
    if ( !node  ||  !node->HaveChildren() ) {
        return false;
    }
    ITERATE ( CNCBINode::TChildren, i, node->Children() ) {
        const CNCBINode* cnode = node->Node(i);
        if ( dynamic_cast<const CHTMLPopupMenu*>(cnode) ) {
            const CHTMLPopupMenu* menu = 
                dynamic_cast<const CHTMLPopupMenu*>(cnode);
            if ( menu->GetType() == type )
                return true;
        }
        if ( cnode->HaveChildren()  &&  s_CheckUsePopupMenus(cnode, type)) {
            return true;
        }
    }
    return false;
}

CNcbiOstream& CHTML_html::PrintChildren(CNcbiOstream& out, TMode mode)
{
    // Check mode
    if ( mode != eHTML ) {
        return CParent::PrintChildren(out, mode);
    }
    
    // Check for use popup menus
    bool use_popup_menus = false;
    for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ )    {
        CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
        if ( m_PopupMenus.find(type) == m_PopupMenus.end() ) {
            if ( s_CheckUsePopupMenus(this, type) ) {
                EnablePopupMenu(type);
                use_popup_menus = true;
            }
        } else {
            use_popup_menus = true;
        }
    }

    if ( !use_popup_menus  ||  !HaveChildren() ) {
        return CParent::PrintChildren(out, mode);
    }

    NON_CONST_ITERATE ( TChildren, i, Children() ) {
        if ( dynamic_cast<CHTML_head*>(Node(i)) ) {
            for (int t = CHTMLPopupMenu::ePMFirst;
                 t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    Node(i)->AppendChild(new CHTMLText(
                        CHTMLPopupMenu::GetCodeHead(type,info->second.m_Url)));
                }
            }
        } else if ( dynamic_cast<CHTML_body*>(Node(i)) ) {
            for (int t = CHTMLPopupMenu::ePMFirst;
                 t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    Node(i)->AppendChild(new CHTMLText(
                        CHTMLPopupMenu::GetCodeBody(type, info->second.m_UseDynamicMenu)));
                }
            }
        }
    }
    return CParent::PrintChildren(out, mode);
}


CHTML_tr::CHTML_tr(void)
    : CParent("tr"), m_Parent(0)
{
    return;
}

CHTML_tr::CHTML_tr(CNCBINode* node)
    : CParent("tr", node), m_Parent(0)
{
    return;
}

CHTML_tr::CHTML_tr(const string& text)
    : CParent("tr", text), m_Parent(0)
{
    return;
}

void CHTML_tr::DoAppendChild(CNCBINode* node)
{
    CHTML_tc* cell = dynamic_cast<CHTML_tc*>(node);
    if ( cell ) {
        // Adding new cell
        _ASSERT(!cell->m_Parent);
        ResetTableCache();
        cell->m_Parent = this;
    }
    CParent::DoAppendChild(node);
}

void CHTML_tr::AppendCell(CHTML_tc* cell)
{
    _ASSERT(!cell->m_Parent);
    cell->m_Parent = this;
    CParent::DoAppendChild(cell);
}

void CHTML_tr::ResetTableCache(void)
{
    if ( m_Parent )
        m_Parent->ResetTableCache();
}

CNcbiOstream& CHTML_tr::PrintEnd(CNcbiOstream& out, TMode mode)
{
    CParent::PrintEnd(out, mode);
    if ( mode == ePlainText ) {
        INIT_STREAM_WRITE;
        out << CHTMLHelper::GetNL();
        if (m_Parent->m_IsRowSep == CHTML_table::ePrintRowSep) {
            out << string(GetTextLength(mode), m_Parent->m_RowSepChar)
                << CHTMLHelper::GetNL();
        }
        CHECK_STREAM_WRITE(out);
    }
    return out;
}

CNcbiOstream& CHTML_tr::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( !HaveChildren() ) {
        return out;
    }
    if ( mode != ePlainText ) {
        return CParent::PrintChildren(out, mode);
    }
    out << m_Parent->m_ColSepL;

    NON_CONST_ITERATE ( TChildren, i, Children() ) {
        if ( i != Children().begin() ) {
            INIT_STREAM_WRITE;
            out << m_Parent->m_ColSepM;
            CHECK_STREAM_WRITE(out);
        }
        Node(i)->Print(out, mode);
    }
    INIT_STREAM_WRITE;
    out << m_Parent->m_ColSepR;
    CHECK_STREAM_WRITE(out);

    return out;
}

SIZE_TYPE CHTML_tr::GetTextLength(TMode mode)
{
    if ( !HaveChildren() ) {
        return 0;
    }

    CNcbiOstrstream sout;
    SIZE_TYPE cols = 0;

    NON_CONST_ITERATE ( TChildren, i, Children() ) {
        Node(i)->Print(sout, mode);
        cols++;
    }
    SIZE_TYPE textlen = sout.pcount();
    if ( mode == ePlainText ) {
        textlen += m_Parent->m_ColSepL.length() +
            m_Parent->m_ColSepR.length();
        if ( cols ) {
            textlen += m_Parent->m_ColSepM.length() * (cols - 1);
        }
    }
    return textlen;
}

CHTML_tc::~CHTML_tc(void)
{
    return;
}

CHTML_tc* CHTML_tc::SetRowSpan(TIndex span)
{
    SetAttribute("rowspan", span);
    return this;
}

CHTML_tc* CHTML_tc::SetColSpan(TIndex span)
{
    SetAttribute("colspan", span);
    return this;
}

static 
CHTML_table::TIndex x_GetSpan(const CHTML_tc* node,
                              const string& attributeName)
{
    if ( !node->HaveAttribute(attributeName) ) {
        return 1;
    }
    const string& value = node->GetAttribute(attributeName);

    try {
        CHTML_table::TIndex span = NStr::StringToUInt(value);
        if ( span > 0 ) {
            return span;
        }
    }
    catch ( exception& ) {
        // Error will be posted later
    }
    ERR_POST("Bad attribute: " << attributeName << "=\"" << value << "\"");
    return 1;
}

void CHTML_tc::DoSetAttribute(const string& name,
                              const string& value, bool optional)
{
    if (name == "rowspan"  ||  name == "colspan") {
        // Changing cell size
        ResetTableCache();
    }
    CParent::DoSetAttribute(name, value, optional);
}

void CHTML_tc::ResetTableCache(void)
{
    if ( m_Parent ) {
        m_Parent->ResetTableCache();
    }
}

void CHTML_tc_Cache::SetUsed()
{
    if ( IsUsed() ) {
        NCBI_THROW(CHTMLException, eTableCellUse, "overlapped table cells");
    }
    m_Used = true;
}

void CHTML_tc_Cache::SetCellNode(CHTML_tc* cellNode)
{
    SetUsed();
    m_Node = cellNode;
}

static
CHTML_table::TIndex x_NextSize(CHTML_table::TIndex size,
                               CHTML_table::TIndex limit)
{
    do {
        if ( size == 0 )
            size = 2;
        else
            size *= 2;
    } while ( size < limit );
    return size;
}

CHTML_tc_Cache& CHTML_tr_Cache::GetCellCache(TIndex col)
{
    TIndex count = GetCellCount();
    if ( col >= count ) {
        TIndex newCount = col + 1;
        TIndex size = m_CellsSize;
        if ( newCount > size ) {
            TIndex newSize = x_NextSize(size, newCount);
            CHTML_tc_Cache* newCells = new CHTML_tc_Cache[newSize];
            for ( TIndex i = 0; i < count; ++i )
                newCells[i] = m_Cells[i];
            delete[] m_Cells;
            m_Cells = newCells;
            m_CellsSize = newSize;
        }
        m_CellCount = newCount;
    }
    return m_Cells[col];
}

void CHTML_tr_Cache::SetUsedCells(TIndex colBegin, TIndex colEnd)
{
    for ( TIndex col = colBegin; col < colEnd; ++col ) {
        GetCellCache(col).SetUsed();
    }
}

void CHTML_tr_Cache::AppendCell(CHTML_tr* rowNode, TIndex col,
                                CHTML_tc* cellNode, TIndex colSpan)
{
    _ASSERT(m_FilledCellCount <= col);
    for ( TIndex i = m_FilledCellCount; i < col; ++i ) {
        CHTML_tc_Cache& cellCache = GetCellCache(i);
        if ( !cellCache.IsUsed() ) {
            CHTML_tc* cell;
            cell = new CHTML_td;
            rowNode->AppendCell(cell);
            cellCache.SetCellNode(cell);
        }
    }
    CHTML_tc_Cache& cellCache = GetCellCache(col);
    _ASSERT(!cellCache.IsUsed());
    _ASSERT(x_GetSpan(cellNode, "colspan") == colSpan);
    rowNode->AppendCell(cellNode);
    cellCache.SetCellNode(cellNode);
    if ( colSpan != 1 ) {
        SetUsedCells(col + 1, col + colSpan);
    }
    m_FilledCellCount = col + colSpan;
}

void CHTML_tr_Cache::SetUsedCells(CHTML_tc* cellNode,
                                  TIndex colBegin, TIndex colEnd)
{
    GetCellCache(colBegin).SetCellNode(cellNode);
    SetUsedCells(colBegin + 1, colEnd);
    m_FilledCellCount = colEnd;
}


CHTML_table_Cache::~CHTML_table_Cache(void)
{
    for ( TIndex i = 0; i < GetRowCount(); ++i ) {
        delete m_Rows[i];
    }
    delete[] m_Rows;
}


CHTML_tr_Cache& CHTML_table_Cache::GetRowCache(TIndex row)
{
    TIndex count = GetRowCount();
    if ( row >= count ) {
        TIndex newCount = row + 1;
        TIndex size = m_RowsSize;
        if ( newCount > size ) {
            TIndex newSize = x_NextSize(size, newCount);
            CHTML_tr_Cache** newRows = new CHTML_tr_Cache*[newSize];
            for ( TIndex i = 0; i < count; ++i )
                newRows[i] = m_Rows[i];
            delete[] m_Rows;
            m_Rows = newRows;
            m_RowsSize = newSize;
        }
        for ( TIndex i = count; i < newCount; ++i )
            m_Rows[i] = new CHTML_tr_Cache;
        m_RowCount = newCount;
    }
    return *m_Rows[row];
}

void CHTML_table_Cache::InitRow(TIndex row, CHTML_tr* rowNode)
{
    CHTML_tr_Cache& rowCache = GetRowCache(row);
    m_Rows[row]->SetRowNode(rowNode);
    m_FilledRowCount = row + 1;

    // Scan all children (which should be <TH> or <TD> tags)
    if ( rowNode->HaveChildren() ) {
        // Beginning with column 0
        TIndex col = 0;
        for ( CNCBINode::TChildren::iterator iCol = rowNode->ChildBegin(),
              iColEnd = rowNode->ChildEnd();
              iCol != iColEnd; ++iCol ) {
            CHTML_tc* cellNode =
                dynamic_cast<CHTML_tc*>(rowNode->Node(iCol));

            if ( !cellNode ) {
                continue;
            }

            // Skip all used cells
            while ( rowCache.GetCellCache(col).IsUsed() ) {
                ++col;
            }

            // Determine current cell size
            TIndex rowSpan = x_GetSpan(cellNode, "rowspan");
            TIndex colSpan = x_GetSpan(cellNode, "colspan");

            // End of new cell in columns
            rowCache.SetUsedCells(cellNode, col, col + colSpan);
            if ( rowSpan > 1 ) {
                SetUsedCells(row + 1, row + rowSpan, col, col + colSpan);
            }

            // Skip this cell's columns
            col += colSpan;
        }
    }
}

CHTML_table_Cache::CHTML_table_Cache(CHTML_table* table)
    : m_Node(table),
      m_RowCount(0), m_RowsSize(0), m_Rows(0), m_FilledRowCount(0)
{
    // Scan all children (which should be <TR> tags)
    if ( table->HaveChildren() ) {
        // Beginning with row 0
        TIndex row = 0;
        for ( CNCBINode::TChildren::iterator iRow = table->ChildBegin(),
              iRowEnd = table->ChildEnd(); iRow != iRowEnd; ++iRow ) {
            CHTML_tr* rowNode = dynamic_cast<CHTML_tr*>(table->Node(iRow));
            if ( !rowNode ) {
                continue;
            }
            InitRow(row, rowNode);
            ++row;
        }
    }
}

CHTML_tr* CHTML_table_Cache::GetRowNode(TIndex row)
{
    GetRowCache(row);
    while ( row >= m_FilledRowCount ) {
        CHTML_tr* rowNode = new CHTML_tr;
        m_Node->AppendRow(rowNode);
        m_Rows[m_FilledRowCount++]->SetRowNode(rowNode);
    }
    return m_Rows[row]->GetRowNode();
}

void CHTML_table_Cache::SetUsedCells(TIndex rowBegin, TIndex rowEnd,
                                     TIndex colBegin, TIndex colEnd)
{
    for ( TIndex row = rowBegin; row < rowEnd; ++row ) {
        GetRowCache(row).SetUsedCells(colBegin, colEnd);
    }
}

CHTML_tc* CHTML_table_Cache::GetCellNode(TIndex row, TIndex col,
                                         CHTML_table::ECellType type)
{
    CHTML_tr_Cache& rowCache = GetRowCache(row);
    if ( col < rowCache.GetCellCount() ) {
        CHTML_tc_Cache& cellCache = rowCache.GetCellCache(col);
        if ( cellCache.IsNode() ) {
            CHTML_tc* cell = cellCache.GetCellNode();
            switch ( type ) {
            case CHTML_table::eHeaderCell:
                if ( !dynamic_cast<CHTML_th*>(cell) )
                    NCBI_THROW(CHTMLException, eTableCellType,
                               "wrong cell type: TH expected");
                break;
            case CHTML_table::eDataCell:
                if ( !dynamic_cast<CHTML_td*>(cell) )
                    NCBI_THROW(CHTMLException, eTableCellType,
                               "wrong cell type: TD expected");
                break;
            default:
                break;
            }
            return cell;
        }
        if ( cellCache.IsUsed() )
            NCBI_THROW(CHTMLException, eTableCellUse,
                       "invalid use of big table cell");
    }
    CHTML_tc* cell;
    if ( type == CHTML_table::eHeaderCell ) {
        cell = new CHTML_th;
    } else {
        cell = new CHTML_td;
    }
    rowCache.AppendCell(GetRowNode(row), col, cell, 1);
    return cell;
}

CHTML_tc* CHTML_table_Cache::GetCellNode(TIndex row, TIndex col,
                                         CHTML_table::ECellType type,
                                         TIndex rowSpan, TIndex colSpan)
{
    CHTML_tr_Cache& rowCache = GetRowCache(row);
    if ( col < rowCache.GetCellCount() ) {
        CHTML_tc_Cache& cellCache = rowCache.GetCellCache(col);
        if ( cellCache.IsNode() ) {
            CHTML_tc* cell = cellCache.GetCellNode();
            switch ( type ) {
            case CHTML_table::eHeaderCell:
                if ( !dynamic_cast<CHTML_th*>(cell) )
                    NCBI_THROW(CHTMLException, eTableCellType,
                               "wrong cell type: TH expected");
                break;
            case CHTML_table::eDataCell:
                if ( !dynamic_cast<CHTML_td*>(cell) )
                    NCBI_THROW(CHTMLException, eTableCellType,
                               "wrong cell type: TD expected");
                break;
            default:
                break;
            }
            if ( x_GetSpan(cell, "rowspan") != rowSpan ||
                 x_GetSpan(cell, "colspan") != colSpan )
                NCBI_THROW(CHTMLException, eTableCellUse,
                           "cannot change table cell size");
            return cell;
        }
        if ( cellCache.IsUsed() )
            NCBI_THROW(CHTMLException, eTableCellUse,
                       "invalid use of big table cell");
    }

    CHTML_tc* cell;
    if ( type == CHTML_table::eHeaderCell ) {
        cell = new CHTML_th;
    } else {
        cell = new CHTML_td;
    }
    if ( colSpan != 1 ) {
        cell->SetColSpan(colSpan);
    }
    if ( rowSpan != 1 ) {
        cell->SetRowSpan(rowSpan);
    }
    rowCache.AppendCell(GetRowNode(row), col, cell, colSpan);
    if ( rowSpan != 1 ) {
        SetUsedCells(row + 1, row + rowSpan, col, col + colSpan);
    }
    return cell;
}

CHTML_table::CHTML_table(void)
    : CParent("table"), m_CurrentRow(0), m_CurrentCol(TIndex(-1)),
      m_ColSepL(kEmptyStr), m_ColSepM(" "), m_ColSepR(kEmptyStr),
      m_RowSepChar('-'), m_IsRowSep(eSkipRowSep)
{
    return;
}

CHTML_table::~CHTML_table(void)
{
    return;
}

CHTML_table_Cache& CHTML_table::GetCache(void) const
{
    CHTML_table_Cache* cache = m_Cache.get();
    if ( !cache ) {
        m_Cache.reset(cache =
                      new CHTML_table_Cache(const_cast<CHTML_table*>(this)));
    }
    return *cache;
}

void CHTML_table::DoAppendChild(CNCBINode* node)
{
    CHTML_tr* row = dynamic_cast<CHTML_tr*>(node);
    if ( row ) {
        // Adding new row
        _ASSERT(!row->m_Parent);
        ResetTableCache();
        row->m_Parent = this;
    }
    CParent::DoAppendChild(node);
}

void CHTML_table::AppendRow(CHTML_tr* row)
{
    _ASSERT(!row->m_Parent);
    row->m_Parent = this;
    CParent::DoAppendChild(row);
}

CHTML_tr* CHTML_table::Row(TIndex row)
{
    return GetCache().GetRowNode(row);
}

CHTML_tc* CHTML_table::Cell(TIndex row, TIndex col, ECellType type)
{
    return GetCache().GetCellNode(m_CurrentRow = row, m_CurrentCol = col,
                                  type);
}

CHTML_tc* CHTML_table::Cell(TIndex row, TIndex col, ECellType type,
                            TIndex rowSpan, TIndex colSpan)
{
    return GetCache().GetCellNode(m_CurrentRow = row, m_CurrentCol = col,
                                  type, rowSpan, colSpan);
}

void CHTML_table::CheckTable(void) const
{
    GetCache();
}

CHTML_table::TIndex CHTML_table::CalculateNumberOfColumns(void) const
{
    CHTML_table_Cache& cache = GetCache();
    TIndex columns = 0;
    for ( TIndex i = 0; i < cache.GetRowCount(); ++i ) {
        columns = max(columns, cache.GetRowCache(i).GetCellCount());
    }
    return columns;
}

CHTML_table::TIndex CHTML_table::CalculateNumberOfRows(void) const
{
    return GetCache().GetRowCount();
}

CNcbiOstream& CHTML_table::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        INIT_STREAM_WRITE;
        out << CHTMLHelper::GetNL();
        CHECK_STREAM_WRITE(out);
        if ( m_IsRowSep == ePrintRowSep ) {
            SIZE_TYPE seplen = 0;
            // Find length of first non-empty row
            NON_CONST_ITERATE ( TChildren, i, Children() ) {
                if ( (seplen =
                      dynamic_cast<CHTML_tr*>(&**i)->GetTextLength(mode)) >0) {
                    break;
                }
            }
            if ( !seplen ) {
                seplen = 1;
            }
            INIT_STREAM_WRITE;
            out << string(seplen, m_RowSepChar) << CHTMLHelper::GetNL();
            CHECK_STREAM_WRITE(out);
        }
    } else {
        // Set column widths.
        if ( HaveChildren() ) {
            ITERATE ( TColWidths, w, m_ColWidths ) {
                // Scan all children (which should be <TR> tags)
                // Beginning with row 0
                TIndex row = 0;
                for ( CNCBINode::TChildren::iterator iRow = ChildBegin(),
                    iRowEnd = ChildEnd(); iRow != iRowEnd; ++iRow ) {
                    try {
                        CHTML_tc* cell = Cell(row, (TIndex)w->first);
                        if ( cell ) {
                        cell->SetWidth(w->second);
                        }
                    }
                    catch (CHTMLException&) {
                        // catch exception with messages about absent cells
                    }
                    ++row;
                }
            }
        }
    }
    return CParent::PrintBegin(out, mode);
}

void CHTML_table::SetPlainSeparators(const string& col_left,
                                     const string& col_middle,
                                     const string& col_right,
                                     const char    row_sep_char,
                                     ERowPlainSep  is_row_sep)
{
    m_ColSepL    = col_left;
    m_ColSepM    = col_middle;
    m_ColSepR    = col_right;
    m_RowSepChar = row_sep_char;
    m_IsRowSep   = is_row_sep;
}


// <form> tag.

CHTML_form::CHTML_form(void)
    : CParent("form")
{
    return;
}

CHTML_form::CHTML_form(const string& url, EMethod method)
    : CParent("form")
{
    Init(url, method);
}

CHTML_form::CHTML_form(const string& url, CNCBINode* node, EMethod method)
    : CParent("form", node)
{
    Init(url, method);
}

CHTML_form::~CHTML_form(void)
{
    return;
}

void CHTML_form::Init(const string& url, EMethod method)
{
    SetOptionalAttribute("action", url);
    switch ( method ) {
    case eGet:
        SetAttribute("method", "GET");
        break;
    case ePost:
        SetAttribute("enctype",
                     "application/x-www-form-urlencoded");
        SetAttribute("method", "POST");
        break;
    case ePostData:
        SetAttribute("enctype",
                     "multipart/form-data");
        SetAttribute("method", "POST");
        break;
    }
}

void CHTML_form::AddHidden(const string& name, const string& value)
{
    AppendChild(new CHTML_hidden(name, value));
}

void CHTML_form::AddHidden(const string& name, int value)
{
    AppendChild(new CHTML_hidden(name, value));
}


// <input> tag.

CHTML_input::CHTML_input(const char* type, const string& name)
    : CParent("input")
{
    SetAttribute("type", type);
    if ( !name.empty() ) {
        SetNameAttribute(name);
    }
}

CHTML_input::~CHTML_input(void)
{
    return;
}


// <legend> tag.

CHTML_legend::CHTML_legend(const string& legend)
    : CParent("legend", legend)
{
    return;
}

CHTML_legend::CHTML_legend(CHTMLNode* legend)
    : CParent("legend", legend)
{
    return;
}

CHTML_legend::~CHTML_legend(void)
{
    return;
}

// <fieldset> tag.

CHTML_fieldset::CHTML_fieldset(void)
    : CParent("fieldset")
{
    return;
}

CHTML_fieldset::CHTML_fieldset(const string& legend)
    : CParent("fieldset", new CHTML_legend(legend))
{
    return;
}

CHTML_fieldset::CHTML_fieldset(CHTML_legend* legend)
    : CParent("fieldset", legend)
{
    return;
}

CHTML_fieldset::~CHTML_fieldset(void)
{
    return;
}


// <label> tag.

CHTML_label::CHTML_label(const string& text)
    : CParent("label", text)
{
    return;
}

CHTML_label::CHTML_label(const string& text, const string& idRef)
    : CParent("label", text)
{
    SetFor(idRef);
}

CHTML_label::~CHTML_label(void)
{
    return;
}

void CHTML_label::SetFor(const string& idRef)
{
    SetAttribute("for", idRef);
}


// <textarea> tag.

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows)
    : CParent("textarea")
{
    if ( !name.empty() ) {
        SetNameAttribute(name);
    }
    SetAttribute("cols", cols);
    SetAttribute("rows", rows);
}

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows,
                               const string& value)
    : CParent("textarea", value)
{
    SetNameAttribute(name);
    SetAttribute("cols", cols);
    SetAttribute("rows", rows);
}

CHTML_textarea::~CHTML_textarea(void)
{
    return;
}


// <input type=checkbox> tag.

const char CHTML_checkbox::sm_InputType[] = "checkbox";

CHTML_checkbox::CHTML_checkbox(const string& name)
    : CParent(sm_InputType, name)
{
    return;
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
}

CHTML_checkbox::CHTML_checkbox(const string& name, bool checked,
                               const string& description)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("checked", checked);
    // Add the description at the end
    AppendPlainText(description);
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value,
                               bool checked, const string& description)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
    SetOptionalAttribute("checked", checked);
    // Add the description at the end
    AppendPlainText(description);  
}

CHTML_checkbox::~CHTML_checkbox(void)
{
    return;
}


// <input type=image> tag.

const char CHTML_image::sm_InputType[] = "image";

CHTML_image::CHTML_image(const string& name, const string& src,
                         const string& alt)
    : CParent(sm_InputType, name)
{
    SetAttribute("src", src);
    SetOptionalAttribute("alt", alt);
}

CHTML_image::CHTML_image(const string& name, const string& src, int border,
                         const string& alt)
    : CParent(sm_InputType, name)
{
    SetAttribute("src", src);
    SetAttribute("border", border);
    SetOptionalAttribute("alt", alt);
}

CHTML_image::~CHTML_image(void)
{
    return;
}


// <input type=radio> tag.

const char CHTML_radio::sm_InputType[] = "radio";

CHTML_radio::CHTML_radio(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
}

CHTML_radio::CHTML_radio(const string& name, const string& value,
                         bool checked, const string& description)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
    SetOptionalAttribute("checked", checked);
    AppendPlainText(description);  // adds the description at the end
}

CHTML_radio::~CHTML_radio(void)
{
    return;
}


// <input type=hidden> tag.

const char CHTML_hidden::sm_InputType[] = "hidden";

CHTML_hidden::CHTML_hidden(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
}

CHTML_hidden::CHTML_hidden(const string& name, int value)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
}

CHTML_hidden::~CHTML_hidden(void)
{
    return;
}


// <input type=password> tag.

const char CHTML_password::sm_InputType[] = "password";

CHTML_password::CHTML_password(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
}

CHTML_password::CHTML_password(const string& name, int size, 
                               const string& value)
    : CParent(sm_InputType, name)
{
    SetSize(size);
    SetOptionalAttribute("value", value);
}

CHTML_password::CHTML_password(const string& name, int size, int maxlength,
                           const string& value)
    : CParent(sm_InputType, name)
{
    SetSize(size);
    SetAttribute("maxlength", maxlength);
    SetOptionalAttribute("value", value);
}

CHTML_password::~CHTML_password(void)
{
    return;
}


// <input type=submit> tag.

const char CHTML_submit::sm_InputType[] = "submit";

CHTML_submit::CHTML_submit(const string& name)
    : CParent(sm_InputType, NcbiEmptyString)
{
    SetOptionalAttribute("value", name);
}

CHTML_submit::CHTML_submit(const string& name, const string& label)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", label);
}

CHTML_submit::~CHTML_submit(void)
{
    return;
}


// <input type=reset> tag.

const char CHTML_reset::sm_InputType[] = "reset";

CHTML_reset::CHTML_reset(const string& label)
    : CParent(sm_InputType, NcbiEmptyString)
{
    SetOptionalAttribute("value", label);
}

CHTML_reset::~CHTML_reset(void)
{
    return;
}


// <input type=button> tag

const char CHTML_input_button::sm_InputType[] = "button";

CHTML_input_button::CHTML_input_button(const string& label)
    : CParent(sm_InputType, NcbiEmptyString)
{
    SetOptionalAttribute("value", label);
}

CHTML_input_button::CHTML_input_button(const string& name, const string& label)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", label);
}

CHTML_input_button::~CHTML_input_button(void)
{
    return;
}


// <input type=text> tag.

const char CHTML_text::sm_InputType[] = "text";

CHTML_text::CHTML_text(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
}

CHTML_text::CHTML_text(const string& name, int size, const string& value)
    : CParent(sm_InputType, name)
{
    SetSize(size);
    SetOptionalAttribute("value", value);
}

CHTML_text::CHTML_text(const string& name, int size, int maxlength,
                       const string& value)
    : CParent(sm_InputType, name)
{
    SetSize(size);
    SetAttribute("maxlength", maxlength);
    SetOptionalAttribute("value", value);
}

CHTML_text::~CHTML_text(void)
{
    return;
}


// <input type=file> tag.

const char CHTML_file::sm_InputType[] = "file";

CHTML_file::CHTML_file(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
}

CHTML_file::~CHTML_file(void)
{
    return;
}


// <button> tag

const char CHTML_button::sm_TagName[] = "button";

CHTML_button::CHTML_button(const string& text, EButtonType type,
                           const string& name, const string& value)
    : CParent(sm_TagName, text)
{
    SetType(type);
    SetSubmitData(name, value);
}

CHTML_button::CHTML_button(CNCBINode* contents, EButtonType type,
                           const string& name, const string& value)
    : CParent(sm_TagName, contents)
{
    SetType(type);
    SetSubmitData(name, value);
}

CHTML_button* CHTML_button::SetType(EButtonType type)
{
    switch ( type ) {
        case eSubmit:
            SetAttribute("type", "submit");
            break;
        case eReset:
            SetAttribute("type", "reset");
            break;
        case eButton:
            SetAttribute("type", "button");
            break;
    }
    return this;
}

CHTML_button* CHTML_button::SetSubmitData(const string& name,
                                          const string& value)
{
    SetOptionalAttribute("name", name);
    SetOptionalAttribute("value", value);
    return this;
}

CHTML_button::~CHTML_button(void)
{
    return;
}


// <select> tag.

const char CHTML_select::sm_TagName[] = "select";

CHTML_select::~CHTML_select(void)
{
    return;
}

CHTML_select* CHTML_select::SetMultiple(void)
{
    SetAttribute("multiple");
    return this;
}


// <option> tag.

const char CHTML_option::sm_TagName[] = "option";

CHTML_option::~CHTML_option(void)
{
    return;
}

CHTML_option* CHTML_option::SetValue(const string& value)
{
    SetAttribute("value", value);
    return this;
}

CHTML_option* CHTML_option::SetSelected(void)
{
    SetAttribute("selected");
    return this;
}


// <a> tag.

const char CHTML_a::sm_TagName[] = "a";

CHTML_a::~CHTML_a(void)
{
    return;
}


// <br> tag.

const char CHTML_br::sm_TagName[] = "br";

CHTML_br::CHTML_br(int count)
    : CParent(sm_TagName)
{
    for ( int i = 1; i < count; ++i ) {
        AppendChild(new CHTML_br());
    }
}

CHTML_br::~CHTML_br(void)
{
    return;
}

CNcbiOstream& CHTML_br::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        INIT_STREAM_WRITE;
        out << CHTMLHelper::GetNL();
        CHECK_STREAM_WRITE(out);
    }
    else {
        CParent::PrintBegin(out, mode);
    }
    return out;

}


// <img> tag.

CHTML_img::CHTML_img(const string& url, const string& alt)
    : CParent("img")
{
    SetAttribute("src", url);
    SetOptionalAttribute("alt", alt);
}

CHTML_img::CHTML_img(const string& url, int width, int height,
                     const string& alt)
    : CParent("img")
{
    SetAttribute("src", url);
    SetOptionalAttribute("alt", alt);
    SetWidth(width);
    SetHeight(height);
}

CHTML_img::~CHTML_img(void)
{
    return;
}

void CHTML_img::UseMap(const string& mapname)
{
    if ( mapname.find("#") == NPOS ) {
        SetAttribute("usemap", "#" + mapname);
    } else {
        SetAttribute("usemap", mapname);
    }
}

void CHTML_img::UseMap(const CHTML_map* const mapnode)
{
    UseMap(mapnode->GetNameAttribute());
}



// <map> tag.

CHTML_map::CHTML_map(const string& name)
    : CParent("map")
{
    SetNameAttribute(name);
}

CHTML_map::~CHTML_map(void)
{
    return;
}


// <area> tag.

const char CHTML_area::sm_TagName[] = "area";

CHTML_area::~CHTML_area(void)
{
    return;
}

CHTML_area* CHTML_area::DefineRect(int x1, int y1, int x2, int y2)
{
    vector<string> c;
    c.push_back(NStr::IntToString(x1));
    c.push_back(NStr::IntToString(y1));
    c.push_back(NStr::IntToString(x2));
    c.push_back(NStr::IntToString(y2));
    SetAttribute("shape", "rect");
    SetAttribute("coords", NStr::Join(c, ","));
    return this;
}

CHTML_area* CHTML_area::DefineCircle(int x, int y, int radius)
{
    vector<string> c;
    c.push_back(NStr::IntToString(x));
    c.push_back(NStr::IntToString(y));
    c.push_back(NStr::IntToString(radius));
    SetAttribute("shape", "circle");
    SetAttribute("coords", NStr::Join(c, ","));
    return this;
}

CHTML_area* CHTML_area::DefinePolygon(int coords[], int count)
{
    string c;
    for(int i = 0; i<count; i++) {
        if ( i ) {
            c += ",";
        }
        c += NStr::IntToString(coords[i]);
    }
    SetAttribute("shape", "poly");
    SetAttribute("coords", c);
    return this;
}

CHTML_area* CHTML_area::DefinePolygon(vector<int> coords)
{
    string c;
    ITERATE(vector<int>, it, coords) {
        if ( it != coords.begin() ) {
            c += ",";
        }
        c += NStr::IntToString(*it);
    }
    SetAttribute("shape", "poly");
    SetAttribute("coords", c);
    return this;
}

CHTML_area* CHTML_area::DefinePolygon(list<int> coords)
{
    string c;
    ITERATE(list<int>, it, coords) {
        if ( it != coords.begin() ) {
            c += ",";
        }
        c += NStr::IntToString(*it);
    }
    SetAttribute("shape", "poly");
    SetAttribute("coords", c);
    return this;
}


// <dl> tag.

const char CHTML_dl::sm_TagName[] = "dl";

CHTML_dl::~CHTML_dl(void)
{
    return;
}

CHTML_dl* CHTML_dl::AppendTerm(const string& term, const string& definition)
{
    AppendChild(new CHTML_dt(term));
    if ( !definition.empty() )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(const string& term, CNCBINode* definition)
{
    AppendChild(new CHTML_dt(term));
    if ( definition )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(CNCBINode* term, const string& definition)
{
    AppendChild(new CHTML_dt(term));
    if ( !definition.empty() ) {
        AppendChild(new CHTML_dd(definition));
    }
    return this;
}

CHTML_dl* CHTML_dl::AppendTerm(CNCBINode* term, CNCBINode* definition)
{
    AppendChild(new CHTML_dt(term));
    if ( definition )
        AppendChild(new CHTML_dd(definition));
    return this;
}

CHTML_dl* CHTML_dl::SetCompact(void)
{
    SetAttribute("compact");
    return this;
}


// <ol> tag.

const char CHTML_ol::sm_TagName[] = "ol";

CHTML_ol::~CHTML_ol(void)
{
}

CHTML_ol* CHTML_ol::SetStart(int start)
{
    SetAttribute("start", start);
    return this;
}


// <ul> tag.

const char CHTML_ul::sm_TagName[] = "ul";

CHTML_ul::~CHTML_ul(void)
{
    return;
}


// <dir> tag.

const char CHTML_dir::sm_TagName[] = "dir";

CHTML_dir::~CHTML_dir(void)
{
    return;
}


// <menu> tag.

const char CHTML_menu::sm_TagName[] = "menu";

CHTML_menu::~CHTML_menu(void)
{
    return;
}

const char CHTML_font::sm_TagName[] = "font";

CHTML_font::~CHTML_font(void)
{
    return;
}


// <font> tag.

CHTML_font* CHTML_font::SetTypeFace(const string& typeface)
{
    SetAttribute("face", typeface);
    return this;
}

CHTML_font* CHTML_font::SetRelativeSize(int size)
{
    if ( size != 0 )
        SetAttribute("size", NStr::IntToString(size, true));
    return this;
}

const char CHTML_basefont::sm_TagName[] = "basefont";

CHTML_basefont::~CHTML_basefont(void)
{
    return;
}

CHTML_basefont* CHTML_basefont::SetTypeFace(const string& typeface)
{
    SetAttribute("typeface", typeface);
    return this;
}

CHTML_color::~CHTML_color(void)
{
    return;
}


// <hr> tag.

const char CHTML_hr::sm_TagName[] = "hr";

CHTML_hr::~CHTML_hr(void)
{
    return;
}

CHTML_hr* CHTML_hr::SetNoShade(void)
{
    SetAttribute("noshade");
    return this;
}

CNcbiOstream& CHTML_hr::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        INIT_STREAM_WRITE;
        out << CHTMLHelper::GetNL() << CHTMLHelper::GetNL();
        CHECK_STREAM_WRITE(out);
    }
    else {
        CParent::PrintBegin(out, mode);
    }
    return out;
}


// <meta> tag.

const char CHTML_meta::sm_TagName[] = "meta";

CHTML_meta::CHTML_meta(EType mtype, const string& var, const string& content)
    : CParent(sm_TagName)
{
    SetAttribute(( mtype == eName ) ? "name" : "http-equiv", var);
    SetAttribute("content", content);
}


CHTML_meta::~CHTML_meta(void)
{
    return;
}


// <script> tag.

const char CHTML_script::sm_TagName[] = "script";

CHTML_script::CHTML_script(const string& stype)
    : CParent(sm_TagName)
{
    SetAttribute("type", stype);
}


CHTML_script::CHTML_script(const string& stype, const string& url)
    : CParent(sm_TagName)
{
    SetAttribute("type", stype);
    SetAttribute("src", url);
}


CHTML_script::~CHTML_script(void)
{
    return;
}

CHTML_script* CHTML_script::AppendScript(const string& script)
{
    AppendChild(new CHTMLPlainText("\n<!--\n" + script + "-->\n", true));
    return this;
}


// Other tags.

#define DEFINE_HTML_ELEMENT(Tag) \
CHTML_NAME(Tag)::~CHTML_NAME(Tag)(void) \
{ \
} \
const char CHTML_NAME(Tag)::sm_TagName[] = #Tag


DEFINE_HTML_ELEMENT(head);
DEFINE_HTML_ELEMENT(body);
DEFINE_HTML_ELEMENT(base);
DEFINE_HTML_ELEMENT(isindex);
DEFINE_HTML_ELEMENT(link);
DEFINE_HTML_ELEMENT(noscript);
DEFINE_HTML_ELEMENT(object);
DEFINE_HTML_ELEMENT(style);
DEFINE_HTML_ELEMENT(title);
DEFINE_HTML_ELEMENT(address);
DEFINE_HTML_ELEMENT(blockquote);
DEFINE_HTML_ELEMENT(center);
DEFINE_HTML_ELEMENT(div);
DEFINE_HTML_ELEMENT(h1);
DEFINE_HTML_ELEMENT(h2);
DEFINE_HTML_ELEMENT(h3);
DEFINE_HTML_ELEMENT(h4);
DEFINE_HTML_ELEMENT(h5);
DEFINE_HTML_ELEMENT(h6);
DEFINE_HTML_ELEMENT(p);
DEFINE_HTML_ELEMENT(pnop);
DEFINE_HTML_ELEMENT(pre);
DEFINE_HTML_ELEMENT(dt);
DEFINE_HTML_ELEMENT(dd);
DEFINE_HTML_ELEMENT(li);
DEFINE_HTML_ELEMENT(caption);
DEFINE_HTML_ELEMENT(col);
DEFINE_HTML_ELEMENT(colgroup);
DEFINE_HTML_ELEMENT(thead);
DEFINE_HTML_ELEMENT(tbody);
DEFINE_HTML_ELEMENT(tfoot);
DEFINE_HTML_ELEMENT(th);
DEFINE_HTML_ELEMENT(td);
DEFINE_HTML_ELEMENT(applet);
DEFINE_HTML_ELEMENT(param);
DEFINE_HTML_ELEMENT(cite);
DEFINE_HTML_ELEMENT(code);
DEFINE_HTML_ELEMENT(dfn);
DEFINE_HTML_ELEMENT(em);
DEFINE_HTML_ELEMENT(kbd);
DEFINE_HTML_ELEMENT(samp);
DEFINE_HTML_ELEMENT(strike);
DEFINE_HTML_ELEMENT(strong);
DEFINE_HTML_ELEMENT(var);
DEFINE_HTML_ELEMENT(b);
DEFINE_HTML_ELEMENT(big);
DEFINE_HTML_ELEMENT(i);
DEFINE_HTML_ELEMENT(s);
DEFINE_HTML_ELEMENT(small);
DEFINE_HTML_ELEMENT(sub);
DEFINE_HTML_ELEMENT(sup);
DEFINE_HTML_ELEMENT(tt);
DEFINE_HTML_ELEMENT(u);
DEFINE_HTML_ELEMENT(blink);
DEFINE_HTML_ELEMENT(span);


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.116  2005/05/09 11:28:39  ivanov
 * CHTMLNode::AttachPopupMenu: Added parameter for canceling default
 * event processing (default is true).
 *
 * Revision 1.115  2005/04/25 19:29:16  ivanov
 * Fixed compilation warnings on 64-bit Workshop compiler
 *
 * Revision 1.114  2005/04/11 19:16:12  ivanov
 * CHTML_input:: made name parameter in the constructor optional.
 * Added CHTML_input_button class for <input type=button>.
 * Revival of CHTML_button class, many browsers already support <button> tag,
 * specified in the HTML 4.0.
 *
 * Revision 1.113  2004/12/13 13:49:41  ivanov
 * Added CHTML_map and CHTML_area classes
 *
 * Revision 1.112  2004/11/12 13:25:54  ivanov
 * Added macro INIT_STREAM_WRITE to init the errno variable.
 *
 * Revision 1.111  2004/10/27 18:25:00  ivanov
 * CHTMLText : Added flag fDisableBuffering to disable internal buffering
 * at the cost of loss some functionality relative to tag mapping.
 * By default buffering is disabled now. But default can be changed
 * in the future.
 *
 * Revision 1.110  2004/10/27 14:40:25  ivanov
 * CHTML_tr::GetTextLength() - use pcount() to determine ostrstream data size
 *
 * Revision 1.109  2004/10/26 20:17:25  ivanov
 * Removed debug code
 *
 * Revision 1.108  2004/10/26 20:12:30  ivanov
 * CHTMLText::PrintBegin() -- using CNcbiOstrstreamToString() to convert
 * strstream to string. Fixed CHTMLText::x_PrintBegin to accept '\0'.
 *
 * Revision 1.107  2004/10/21 17:44:04  ivanov
 * CHTMLText: added EFlag type and flag parameter to constructors
 *
 * Revision 1.106  2004/08/13 16:47:53  ivanov
 * Added class CHTML_password (HTML tag <input type=password>)
 *
 * Revision 1.105  2004/07/20 20:12:22  ucko
 * Move declarations of CHTML_t*_Cache to html.hpp, as required by
 * some auto_ptr<> implementations.
 *
 * Revision 1.104  2004/07/20 16:36:55  ivanov
 * + CHTML_table::SetColumnWidth
 *
 * Revision 1.103  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.102  2004/04/05 16:19:57  ivanov
 * Added support for Sergey Kurdin's popup menu with configurations
 *
 * Revision 1.101  2004/03/10 20:11:35  ivanov
 * Changed message text
 *
 * Revision 1.100  2004/02/04 17:20:10  ivanov
 * Added s_GenerateNodeInternalName() function.
 * Use it insteed dummy names in the meta-tags node classes.
 *
 * Revision 1.99  2004/02/04 12:43:47  ivanov
 * Fixed CHTMLText::SetText() -- do not use m_Name for store HTML code.
 * Bind internal name for objects of the CHTMLSpecialChar class.
 *
 * Revision 1.98  2004/02/03 19:45:13  ivanov
 * Binded dummy names for the unnamed nodes
 *
 * Revision 1.97  2004/02/02 15:48:16  ivanov
 * CHTMLText::x_PrintBegin: using CHTMLHelper::StripHTML insteed StripTags
 *
 * Revision 1.96  2004/02/02 14:30:24  ivanov
 * CHTMLText::PrintBegin - added repeat tag support. Some cosmetic changes.
 *
 * Revision 1.95  2004/01/30 14:03:50  lavr
 * Print last errno along with "write failure" message
 *
 * Revision 1.94  2004/01/26 16:26:47  ivanov
 * Added NOWRAP attribute support
 *
 * Revision 1.93  2003/12/18 20:15:42  golikov
 * use HideMenu() call, CHTMLComment::PrintBegin fix
 *
 * Revision 1.92  2003/12/16 19:08:49  ivanov
 * CHTML_font::SetTypeFace: replaced attribute name "typeface" to "face".
 *
 * Revision 1.91  2003/12/10 19:15:14  ivanov
 * Move adding a string "return false;" to menues JS code call from ShowMenu()
 * to AttachPopupMenu()
 *
 * Revision 1.90  2003/12/02 14:25:58  ivanov
 * Removed obsolete functions GetCodeBodyTag[Handler|Action]().
 * AttachPopupMenu(): use event parameter for eKurdin menu also.
 *
 * Revision 1.89  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.88  2003/11/03 14:49:16  ivanov
 * Always check if the write was successful, throw otherwise
 *
 * Revision 1.87  2003/10/01 15:57:13  ivanov
 * Added support for Sergey Kurdin's side menu
 *
 * Revision 1.86  2003/07/08 17:13:53  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * Revision 1.85  2003/05/30 18:39:33  lavr
 * Replace endl's with explicit '\n' to avoid premature flushing
 *
 * Revision 1.84  2003/05/15 12:33:54  ucko
 * s_Find: drop assertion, and just return NPOS at/past the end of s.
 *
 * Revision 1.83  2003/05/14 21:55:40  ucko
 * CHTMLText::PrintBegin: Use strstr() instead of string::find() when
 * looking for tags to replace, as the latter is much slower on some
 * systems.
 *
 * Revision 1.82  2003/04/22 15:04:05  ivanov
 * Removed HTMLEncode() for tags attributes in CHTMLOpenElement::PrintBegin()
 *
 * Revision 1.81  2003/03/11 15:28:57  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.80  2003/02/14 16:19:32  ucko
 * Indent the children of CHTMLListElement in plain-text mode.
 * Avoid redundant newlines in CHTMLBlockElement::PrintEnd.
 *
 * Revision 1.79  2002/12/24 14:56:03  ivanov
 * Fix for R1.76:  HTML classes for tags <h1-6>, <p>, <div>. <pre>, <blockquote>
 * now inherits from CHTMLBlockElement (not CHTMElement as before)
 *
 * Revision 1.78  2002/12/20 19:20:19  ivanov
 * Added SPAN tag support
 *
 * Revision 1.77  2002/12/18 19:46:18  ivanov
 * Added line break after elements of the class CHTMLElement in plain text mode
 *
 * Revision 1.76  2002/12/09 22:11:33  ivanov
 * Added support for Sergey Kurdin's popup menu.
 * Added CHTMLNode::AttachPopupMenu().
 *
 * Revision 1.75  2002/09/25 01:24:56  dicuccio
 * Added CHTMLHelper::StripTags() - strips HTML comments and tags from any
 * string.  Implemented CHTMLText::PrintBegin() for mode = ePlainText
 *
 * Revision 1.74  2002/02/13 20:16:44  ivanov
 * Added support of dynamic popup menus. Changed EnablePopupMenu().
 *
 * Revision 1.73  2002/01/30 19:54:51  ivanov
 * CHTML_table::PrintBegin - determination length of prior table's separator
 * as length of first none empty table's row. Added new line at printing
 * table in plain text mode.
 *
 * Revision 1.72  2002/01/29 20:01:46  ivanov
 * (plain text) CHTML_table:: set def. medium sep. to " " instead of "\t"
 *
 * Revision 1.71  2002/01/29 19:25:55  ivanov
 * Typo fixed
 *
 * Revision 1.70  2002/01/29 19:20:47  ivanov
 * (plain text) CHTML_table:: set def. medium sep. to "" instead of "\t".
 * Restored functionality of CHTML_tr::PrintEnd().
 * Fixed CHTML_tr::GetTextLength() -- ".put('\0')" instead of "<< '\0'".
 *
 * Revision 1.69  2002/01/28 17:54:50  vakatov
 * (plain text) CHTML_table:: set def. medium sep. to "\t" instead of "\t|\t"
 *
 * Revision 1.68  2002/01/17 23:40:01  ivanov
 * Added means to print HTML tables in plain text mode
 *
 * Revision 1.67  2001/08/14 16:51:05  ivanov
 * Change means for init JavaScript popup menu & add it to HTML document.
 * Remove early redefined classes for tags HEAD and BODY.
 *
 * Revision 1.66  2001/07/16 13:54:09  ivanov
 * Added support JavaScript popups menu (jsmenu.[ch]pp)
 *
 * Revision 1.65  2001/06/08 19:00:22  ivanov
 * Added base classes: CHTMLDualNode, CHTMLSpecialChar
 *     (and based on it: CHTML_nbsp, _gt, _lt, _quot, _amp, _copy, _reg)
 * Added realization for tags <meta> (CHTML_meta) and <script> (CHTML_script)
 * Changed base class for tags LINK, PARAM, ISINDEX -> CHTMLOpenElement
 * Added tags: OBJECT, NOSCRIPT
 * Added attribute "alt" for CHTML_img
 * Added CHTMLComment::Print() for disable print html-comments in plaintext mode
 *
 * Revision 1.64  2001/06/05 15:35:48  ivanov
 * Added attribute "alt" to CHTML_image
 *
 * Revision 1.63  2001/05/17 15:05:42  lavr
 * Typos corrected
 *
 * Revision 1.62  2000/10/13 19:55:15  vasilche
 * Fixed error with static html node object.
 *
 * Revision 1.61  2000/09/27 14:11:17  vasilche
 * Newline '\n' will not be generated after tags LABEL, A, FONT, CITE, CODE, EM,
 * KBD, STRIKE STRONG, VAR, B, BIG, I, S, SMALL, SUB, SUP, TT, U and BLINK.
 *
 * Revision 1.60  2000/08/15 19:40:48  vasilche
 * Added CHTML_label::SetFor() method for setting HTML attribute FOR.
 *
 * Revision 1.59  2000/08/01 20:05:11  golikov
 * Removed _TRACE
 *
 * Revision 1.58  2000/07/25 15:27:38  vasilche
 * Added newline symbols before table and after each table row in text mode.
 *
 * Revision 1.57  2000/07/20 20:37:19  vasilche
 * Fixed null pointer dereference.
 *
 * Revision 1.56  2000/07/18 19:08:55  vasilche
 * Fixed uninitialized members.
 * Fixed NextCell to advance to next cell.
 *
 * Revision 1.55  2000/07/18 17:21:39  vasilche
 * Added possibility to force output of empty attribute value.
 * Added caching to CHTML_table, now large tables work much faster.
 * Changed algorithm of emitting EOL symbols in html output.
 *
 * Revision 1.54  2000/07/12 16:37:42  vasilche
 * Added new HTML4 tags: LABEL, BUTTON, FIELDSET, LEGEND.
 * Added methods for setting common attributes: STYLE, ID, TITLE, ACCESSKEY.
 *
 * Revision 1.53  2000/03/07 15:26:12  vasilche
 * Removed second definition of CRef.
 *
 * Revision 1.52  1999/12/28 21:01:08  vasilche
 * Fixed conflict on MS VC between bool and const string& arguments by
 * adding const char* argument.
 *
 * Revision 1.51  1999/12/28 18:55:45  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.50  1999/10/28 13:40:35  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.49  1999/08/20 16:14:54  golikov
 * 'non-<TR> tag' bug fixed
 *
 * Revision 1.48  1999/08/09 16:20:07  golikov
 * Table output in plaintext fixed
 *
 * Revision 1.47  1999/07/08 18:05:15  vakatov
 * Fixed compilation warnings
 *
 * Revision 1.46  1999/06/18 20:42:50  vakatov
 * Fixed tiny compilation warnings
 *
 * Revision 1.45  1999/06/11 20:30:29  vasilche
 * We should catch exception by reference, because catching by value
 * doesn't preserve comment string.
 *
 * Revision 1.44  1999/06/09 20:57:58  golikov
 * RowSpan fixed by Vasilche
 *
 * Revision 1.43  1999/06/07 15:21:05  vasilche
 * Fixed some warnings.
 *
 * Revision 1.42  1999/05/28 18:03:51  vakatov
 * CHTMLNode::  added attribute "CLASS"
 *
 * Revision 1.41  1999/05/27 21:43:02  vakatov
 * Get rid of some minor compiler warnings
 *
 * Revision 1.40  1999/05/24 13:57:55  pubmed
 * Save Command; MEDLINE, FASTA format changes
 *
 * Revision 1.39  1999/05/20 16:52:31  pubmed
 * SaveAsText action for query; minor changes in filters,labels, tabletemplate
 *
 * Revision 1.38  1999/05/17 20:09:58  vasilche
 * Removed generation of implicit table cells.
 *
 * Revision 1.37  1999/04/16 17:45:35  vakatov
 * [MSVC++] Replace the <windef.h>'s min/max macros by the hand-made templates.
 *
 * Revision 1.36  1999/04/15 22:09:26  vakatov
 * "max" --> "NcbiMax"
 *
 * Revision 1.35  1999/04/15 19:56:24  vasilche
 * More warnings fixed
 *
 * Revision 1.34  1999/04/15 19:48:23  vasilche
 * Fixed several warnings detected by GCC
 *
 * Revision 1.33  1999/04/08 19:00:31  vasilche
 * Added current cell pointer to CHTML_table
 *
 * Revision 1.32  1999/03/26 22:00:01  sandomir
 * checked option in Radio button fixed; minor fixes in Selection
 *
 * Revision 1.31  1999/03/01 21:03:09  vasilche
 * Added CHTML_file input element.
 * Changed CHTML_form constructors.
 *
 * Revision 1.30  1999/02/26 21:03:33  vasilche
 * CAsnWriteNode made simple node. Use CHTML_pre explicitly.
 * Fixed bug in CHTML_table::Row.
 * Added CHTML_table::HeaderCell & DataCell methods.
 *
 * Revision 1.29  1999/02/02 17:57:49  vasilche
 * Added CHTML_table::Row(int row).
 * Linkbar now have equal image spacing.
 *
 * Revision 1.28  1999/01/28 21:58:08  vasilche
 * QueryBox now inherits from CHTML_table (not CHTML_form as before).
 * Use 'new CHTML_form("url", queryBox)' as replacement of old QueryBox.
 *
 * Revision 1.27  1999/01/28 16:59:01  vasilche
 * Added several constructors for CHTML_hr.
 * Added CHTMLNode::SetSize method.
 *
 * Revision 1.26  1999/01/25 19:34:18  vasilche
 * String arguments which are added as HTML text now treated as plain text.
 *
 * Revision 1.25  1999/01/21 21:12:59  vasilche
 * Added/used descriptions for HTML submit/select/text.
 * Fixed some bugs in paging.
 *
 * Revision 1.24  1999/01/21 16:18:05  sandomir
 * minor changes due to NStr namespace to contain string utility functions
 *
 * Revision 1.23  1999/01/14 21:25:20  vasilche
 * Changed CPageList to work via form image input elements.
 *
 * Revision 1.22  1999/01/11 22:05:52  vasilche
 * Fixed CHTML_font size.
 * Added CHTML_image input element.
 *
 * Revision 1.21  1999/01/11 15:13:35  vasilche
 * Fixed CHTML_font size.
 * CHTMLHelper extracted to separate file.
 *
 * Revision 1.20  1999/01/07 16:41:56  vasilche
 * CHTMLHelper moved to separate file.
 * TagNames of CHTML classes ara available via s_GetTagName() static
 * method.
 * Input tag types ara available via s_GetInputType() static method.
 * Initial selected database added to CQueryBox.
 * Background colors added to CPagerBax & CSmallPagerBox.
 *
 * Revision 1.19  1999/01/05 20:23:29  vasilche
 * Fixed HTMLEncode.
 *
 * Revision 1.18  1999/01/04 20:06:14  vasilche
 * Redesigned CHTML_table.
 * Added selection support to HTML forms (via hidden values).
 *
 * Revision 1.16  1998/12/28 21:48:16  vasilche
 * Made Lewis's 'tool' compilable
 *
 * Revision 1.15  1998/12/28 16:48:09  vasilche
 * Removed creation of QueryBox in CHTMLPage::CreateView()
 * CQueryBox extends from CHTML_form
 * CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
 *
 * Revision 1.14  1998/12/24 16:15:41  vasilche
 * Added CHTMLComment class.
 * Added TagMappers from static functions.
 *
 * Revision 1.13  1998/12/23 21:51:44  vasilche
 * Added missing constructors to checkbox.
 *
 * Revision 1.12  1998/12/23 21:21:03  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.11  1998/12/23 14:28:10  vasilche
 * Most of closed HTML tags made via template.
 *
 * Revision 1.10  1998/12/21 22:25:03  vasilche
 * A lot of cleaning.
 *
 * Revision 1.9  1998/12/10 19:21:51  lewisg
 * correct error handling in InsertInTable
 *
 * Revision 1.8  1998/12/10 00:17:27  lewisg
 * fix index in InsertInTable
 *
 * Revision 1.7  1998/12/09 23:00:54  lewisg
 * use new cgiapp class
 *
 * Revision 1.6  1998/12/08 00:33:43  lewisg
 * cleanup
 *
 * Revision 1.5  1998/12/03 22:48:00  lewisg
 * added HTMLEncode() and CHTML_img
 *
 * Revision 1.4  1998/12/01 19:10:38  lewisg
 * uses CCgiApplication and new page factory
 *
 * Revision 1.3  1998/11/23 23:42:17  lewisg
 * *** empty log message ***
 *
 * Revision 1.2  1998/10/29 16:13:06  lewisg
 * version 2
 *
 * Revision 1.1  1998/10/06 20:36:05  lewisg
 * new html lib and test program
 *
 * ===========================================================================
 */
