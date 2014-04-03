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

#include <ncbi_pch.hpp>
#include <html/html.hpp>
#include <html/htmlhelper.hpp>
#include <html/indentstream.hpp>
#include <html/html_exception.hpp>
#include <html/error_codes.hpp>
#include <errno.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X   Html_Lib


BEGIN_NCBI_SCOPE


/// Tag delimiters
const char* kTagStart = "<@";   ///< Tag start
const char* kTagEnd   = "@>";   ///< Tag end
const SIZE_TYPE kTagStartLen = 2;


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


// CHTMLNode

CHTMLNode::~CHTMLNode(void)
{
    return;
}

string CHTMLNode::GetEventHandlerName(const EHTML_EH_Attribute name) const
{
    switch (name) {
        case eHTML_EH_Blur:
            return "onblur";
        case eHTML_EH_Change:
            return "onchange";
        case eHTML_EH_Click:
            return "onclick";
        case eHTML_EH_DblClick:
            return "ondblclick";
        case eHTML_EH_Focus:
            return "onfocus";
        case eHTML_EH_Load:
            return "onload";
        case eHTML_EH_Unload:
            return "onunload";
        case eHTML_EH_MouseDown:
            return "onmousedown";
        case eHTML_EH_MouseUp:
            return "onmouseup";
        case eHTML_EH_MouseMove:
            return "onmousemove";
        case eHTML_EH_MouseOver:
            return "onmouseover";
        case eHTML_EH_MouseOut:
            return "onmouseout";
        case eHTML_EH_Select:
            return "onselect";
        case eHTML_EH_Submit:
            return "onsubmit";
        case eHTML_EH_KeyDown:
            return "onkeydown";
        case eHTML_EH_KeyPress:
            return "onkeypress";
        case eHTML_EH_KeyUp:
            return "onkeyup";
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
    switch (mode) {
        case ePlainText:
            INIT_STREAM_WRITE;
            out << m_Plain;
            CHECK_STREAM_WRITE(out);
            break;
        case eHTML:
        case eXHTML:
            CParent::PrintChildren(out, mode);
            break;
    }
    return out;
}


// plain text node

CHTMLPlainText::CHTMLPlainText(EEncodeMode encode_mode, const string& text) :
        CNCBINode(s_GenerateNodeInternalName("plaintext", text)),
    m_Text(text), m_EncodeMode(encode_mode)
{
}

CHTMLPlainText::CHTMLPlainText(const char* text, bool noEncode)
    : CNCBINode(s_GenerateNodeInternalName("plaintext", text)),
        m_Text(text)
{
    SetNoEncode(noEncode);
}

CHTMLPlainText::CHTMLPlainText(const string& text, bool noEncode)
    : CNCBINode(s_GenerateNodeInternalName("plaintext", text)),
        m_Text(text)
{
    SetNoEncode(noEncode);
}

CHTMLPlainText::~CHTMLPlainText(void)
{
}


CNcbiOstream& CHTMLPlainText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    string str(GetText());
    switch (mode) {
        case ePlainText:
            if (m_EncodeMode == eJSONEncode)
                str = NStr::JsonEncode(str);
            break;
        case eHTML:
        case eXHTML:
            switch (m_EncodeMode) {
            case eJSONEncode:
                str = NStr::JsonEncode(str);
            case eNoEncode:
                break;
            case eHTMLEncode:
                str = CHTMLHelper::HTMLEncode(str);
            }
            break;
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

CNcbiOstream& CHTMLText::PrintString(CNcbiOstream& out, TMode mode,
                                     const string& s) const
{
    TFlags flags = 0;
    switch (mode) {
        case ePlainText:
            if ( m_Flags & fStripTextMode )
                flags |= fStrip;
            if ( m_Flags & fEncodeTextMode )
                flags |= fEncode;
            break;
        case eHTML:
        case eXHTML:
            if ( m_Flags & fStripHtmlMode ) 
                flags |= fStrip;
            if ( m_Flags & fEncodeHtmlMode )
                flags |= fEncode;
            break;
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
        PrintString(out, mode, tmp); \
    }

CNcbiOstream& CHTMLText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    const string& text = GetText();
    SIZE_TYPE tagStart = s_Find(text, kTagStart);
    if ( tagStart == NPOS ) {
        return PrintString(out, mode, text);
    }

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
        SIZE_TYPE tagNameStart = tagStart + kTagStartLen;
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
            last = tagNameEnd + kTagStartLen;
            tagStart = s_Find(text, kTagStart, last);
        }
    } while ( tagStart != NPOS );

    if ( last != text.size() ) {
        tmp = text.substr(last);
        PRINT_TMP_STR;
    }
    if ( enable_buffering ) {
        PrintString(out, mode, CNcbiOstrstreamToString(*pstr));
        delete pstr;
    }
    return out;
}


CHTMLOpenElement::~CHTMLOpenElement(void)
{
    return;
}

CNcbiOstream& CHTMLOpenElement::x_PrintBegin(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            out << '<' << m_Name;
            if ( HaveAttributes() ) {
                for ( TAttributes::const_iterator i = Attributes().begin();
                    i != Attributes().end(); ++i ) {
                    INIT_STREAM_WRITE;
                    out << ' ' << i->first;
                    CHECK_STREAM_WRITE(out);

                    if ( (mode == eXHTML)  ||
                         !i->second.IsOptional()  ||
                         !i->second.GetValue().empty() ) {
                        string attr = i->second.GetValue();
                        out << "=\"";
                        if ( attr.empty() ) {
                            // For XHTML attribute minimization is forbidden
                            if ((mode == eXHTML) &&  i->second.IsOptional()) {
                                out << i->first;
                            }
                        } else {
                            if ( attr.find_first_of("\"&") != NPOS ) {
                                // Escape attributes
                                attr = CHTMLHelper::HTMLAttributeEncode(attr, 
                                            CHTMLHelper::fSkipEntities |
    /* NOTE: 
        temporary use fCheckPreencoded flag here to clean up redundant
        HTMLEncode() calls from NCBI code.
    */
                                            CHTMLHelper::fCheckPreencoded);
                            }
                            // Check tags inside attribute value
                            if (s_Find(attr, kTagStart) == NPOS) {
                                out << attr;
                            } else {
                                CHTMLText tmp(attr);
                                tmp.Print(out, mode);
                            }
                        }
                        out << '"';
                    }
                }
            }
            break;
    }
    return out;
}


CNcbiOstream& CHTMLOpenElement::PrintBegin(CNcbiOstream& out, TMode mode)
{
    x_PrintBegin(out, mode);
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            INIT_STREAM_WRITE;
            out << '>';
            CHECK_STREAM_WRITE(out);
            break;
    }
    return out;
}


CHTMLSingleElement::~CHTMLSingleElement(void)
{
    return;
}


CNcbiOstream& CHTMLSingleElement::PrintBegin(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
        case eHTML:
            CParent::PrintBegin(out, mode);
            break;
        case eXHTML:
            x_PrintBegin(out, mode);
            INIT_STREAM_WRITE;
            out << " />";
            CHECK_STREAM_WRITE(out);
            break;
    }
    return out;
}


CHTMLInlineElement::~CHTMLInlineElement(void)
{
    return;
}


CNcbiOstream& CHTMLInlineElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            INIT_STREAM_WRITE;
            out << "</" << m_Name << '>';
            CHECK_STREAM_WRITE(out);
            break;
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
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            const TMode* previous = mode.GetPreviousContext();
            INIT_STREAM_WRITE;
            if ( previous ) {
                CNCBINode* parent = previous->GetNode();
                if ( parent && parent->HaveChildren() &&
                    parent->Children().size() > 1 )
                    // separate child nodes by newline
                    out << CHTMLHelper::GetNL();
            } else {
                out << CHTMLHelper::GetNL();
            }
            CHECK_STREAM_WRITE(out);
            break;
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
    switch (mode) {
        case ePlainText:
            {{
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
            out << CHTMLHelper::GetNL();
            CHECK_STREAM_WRITE(out);
            }}
            break;
        case eHTML:
        case eXHTML:
            break;
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
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            CParent::Print(out, mode);
            break;
    }
    return out;
}

CNcbiOstream& CHTMLComment::PrintBegin(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            INIT_STREAM_WRITE;
            out << "<!--";
            CHECK_STREAM_WRITE(out);
            break;
    }
    return out;
}

CNcbiOstream& CHTMLComment::PrintEnd(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
            INIT_STREAM_WRITE;
            out << "-->";
            CHECK_STREAM_WRITE(out);
            break;
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
    SetAttribute("compact");
    return this;
}

CNcbiOstream& CHTMLListElement::PrintChildren(CNcbiOstream& out, TMode mode)
{
    switch (mode) {
        case ePlainText:
            {{
                CIndentingOstream out2(out);
                CHTMLElement::PrintChildren(out2, mode);
            }}
            break;
        case eHTML:
        case eXHTML:
            CHTMLElement::PrintChildren(out, mode);
            break;
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
    switch (mode) {
        case ePlainText:
            for ( int i = 0; i < m_Count; i++ ) {
                INIT_STREAM_WRITE;
                out << m_Plain;
                CHECK_STREAM_WRITE(out);
            }
            break;
        case eHTML:
        case eXHTML:
            for ( int i = 0; i < m_Count; i++ ) {
                INIT_STREAM_WRITE;
                out << "&" << m_Html << ";";
                CHECK_STREAM_WRITE(out);
            }
            break;
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
    switch (mode) {
        case ePlainText:
            if ( m_Parent ) {
                INIT_STREAM_WRITE;
                out << CHTMLHelper::GetNL();
                if (m_Parent->m_IsRowSep == CHTML_table::ePrintRowSep) {
                    out << string(GetTextLength(mode), m_Parent->m_RowSepChar)
                        << CHTMLHelper::GetNL();
                }
                CHECK_STREAM_WRITE(out);
            }
            break;
        case eHTML:
        case eXHTML:
            break;
    }
    return out;
}

CNcbiOstream& CHTML_tr::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( !HaveChildren() ) {
        return out;
    }
    switch (mode) {
        case ePlainText:
            break;
        case eHTML:
        case eXHTML:
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
    SIZE_TYPE textlen = (SIZE_TYPE)sout.pcount();

    switch (mode) {
        case ePlainText:
            textlen += m_Parent->m_ColSepL.length() +
                m_Parent->m_ColSepR.length();
            if ( cols ) {
                textlen += m_Parent->m_ColSepM.length() * (cols - 1);
            }
            break;
        case eHTML:
        case eXHTML:
            break;
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
    ERR_POST_X(1, "Bad attribute: " << attributeName << "=\"" << value << "\"");
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
    : CParent("table"),
      m_CurrentRow(TIndex(-1)), m_CurrentCol(TIndex(-1)),
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
    m_CurrentRow = (row == TIndex(-1)) ? 0 : row;
    m_CurrentCol = (col == TIndex(-1)) ? 0 : col;
    return GetCache().GetCellNode(m_CurrentRow, m_CurrentCol, type);
}

CHTML_tc* CHTML_table::Cell(TIndex row, TIndex col, ECellType type,
                            TIndex rowSpan, TIndex colSpan)
{
    m_CurrentRow = (row == TIndex(-1)) ? 0 : row;
    m_CurrentCol = (col == TIndex(-1)) ? 0 : col;
    return GetCache().GetCellNode(m_CurrentRow, m_CurrentCol,
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
    switch (mode) {
    case ePlainText:
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
        break;
    case eHTML:
    case eXHTML:
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
        break;
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

CHTML_submit::CHTML_submit(const string& label)
    : CParent(sm_InputType, NcbiEmptyString)
{
    SetOptionalAttribute("value", label);
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


// <optgroup> tag.

const char CHTML_optgroup::sm_TagName[] = "optgroup";

CHTML_optgroup::~CHTML_optgroup(void)
{
    return;
}


// <option> tag.

const char CHTML_option::sm_TagName[] = "option";

CHTML_option::~CHTML_option(void)
{
    return;
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
    switch (mode) {
        case ePlainText:
            INIT_STREAM_WRITE;
            out << CHTMLHelper::GetNL();
            CHECK_STREAM_WRITE(out);
            break;
        case eHTML:
        case eXHTML:
            CParent::PrintBegin(out, mode);
            break;
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
        SetAttribute("size", NStr::IntToString(size, NStr::fWithSign));
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
    switch (mode) {
        case ePlainText:
            INIT_STREAM_WRITE;
            out << CHTMLHelper::GetNL() << CHTMLHelper::GetNL();
            CHECK_STREAM_WRITE(out);
            break;
        case eHTML:
        case eXHTML:
            CParent::PrintBegin(out, mode);
            break;
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
    const string& nl = CHTMLHelper::GetNL();
    AppendChild(
        new CHTMLPlainText(nl + "<!--" + nl + script + "-->" + nl, true));
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
