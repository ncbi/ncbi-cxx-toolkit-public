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
 * File Description:  code for CHTMLNode
 *
 */


#include <html/html.hpp>
#include <html/htmlhelper.hpp>
#include <html/jsmenu.hpp>


BEGIN_NCBI_SCOPE


// CHTMLNode

CHTMLNode::~CHTMLNode(void)
{
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
    if ( !appendstring.empty() )
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
}

void CHTMLNode::AppendPlainText(const char* appendstring, bool noEncode)
{
    if ( appendstring && *appendstring )
        AppendChild(new CHTMLPlainText(appendstring, noEncode));
}

void CHTMLNode::AppendHTMLText(const string& appendstring)
{
    if ( !appendstring.empty() )
        AppendChild(new CHTMLText(appendstring));
}

void CHTMLNode::AppendHTMLText(const char* appendstring)
{
    if ( appendstring && *appendstring )
        AppendChild(new CHTMLText(appendstring));
}

void CHTMLNode::SetEventHandler(const EHTML_EH_Attribute name,
                                const string& value)
{
    if ( value.empty() )
        return;

    string handler_name;

    switch (name) {

    case eHTML_EH_Blur:
        handler_name = "onBlur";
        break;
    case eHTML_EH_Change:
        handler_name = "onChange";
        break;
    case eHTML_EH_Click:
        handler_name = "onClick";
        break;
    case eHTML_EH_DblClick:
        handler_name = "onDblClick";
        break;
    case eHTML_EH_Focus:
        handler_name = "onFocus";
        break;
    case eHTML_EH_Load:
        handler_name = "onLoad";
        break;
    case eHTML_EH_Unload:
        handler_name = "onUnload";
        break;
    case eHTML_EH_MouseDown:
        handler_name = "onMouseDown";
        break;
    case eHTML_EH_MouseUp:
        handler_name = "onMouseUp";
        break;
    case eHTML_EH_MouseMove:
        handler_name = "onMouseMove";
        break;
    case eHTML_EH_MouseOver:
        handler_name = "onMouseOver";
        break;
    case eHTML_EH_MouseOut:
        handler_name = "onMouseOut";
        break;
    case eHTML_EH_Select:
        handler_name = "onSelect";
        break;
    case eHTML_EH_Submit:
        handler_name = "onSubmit";
        break;
    case eHTML_EH_KeyDown:
        handler_name = "onKeyDown";
        break;
    case eHTML_EH_KeyPress:
        handler_name = "onKeyPress";
        break;
    case eHTML_EH_KeyUp:
        handler_name = "onKeyUp";
        break;
    }

    SetAttribute(handler_name, value);
}


// <@XXX@> mapping tag node

CHTMLTagNode::CHTMLTagNode(const char* name)
    : CParent(name)
{
}

CHTMLTagNode::CHTMLTagNode(const string& name)
    : CParent(name)
{
}

CHTMLTagNode::~CHTMLTagNode(void)
{
}

CNcbiOstream& CHTMLTagNode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    CNodeRef node = MapTagAll(GetName(), mode);
    if ( node )
        node->Print(out, mode);
    return out;
}


// dual text node

CHTMLDualNode::CHTMLDualNode(const char* html, const char* plain)
{
    AppendChild(new CHTMLText(html));
    m_Plain = plain;
}

CHTMLDualNode::CHTMLDualNode(CNCBINode* child, const char* plain)
{
   AppendChild(child);
   m_Plain = plain;
}

CHTMLDualNode::~CHTMLDualNode(void)
{
}

CNcbiOstream& CHTMLDualNode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText )
        return out << m_Plain;
    else
        return CParent::PrintChildren(out, mode);
}


// plain text node

CHTMLPlainText::CHTMLPlainText(const char* text, bool noEncode)
    : CNCBINode(text), m_NoEncode(noEncode)
{
}

CHTMLPlainText::CHTMLPlainText(const string& text, bool noEncode)
    : CNCBINode(text), m_NoEncode(noEncode)
{
}

CHTMLPlainText::~CHTMLPlainText(void)
{
}

void CHTMLPlainText::SetText(const string& text)
{
    m_Name = text;
}

CNcbiOstream& CHTMLPlainText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText || NoEncode() ) {
        return out << GetText();
    }
    else {
        return out << CHTMLHelper::HTMLEncode(GetText());
    }
}


// text node

CHTMLText::CHTMLText(const string& text)
    : CParent(text)
{
}

CHTMLText::CHTMLText(const char* text)
    : CParent(text)
{
}

CHTMLText::~CHTMLText(void)
{
}

void CHTMLText::SetText(const string& text)
{
    m_Name = text;
}

static const string KTagStart = "<@";
static SIZE_TYPE KTagStart_size = 2;
static const string KTagEnd = "@>";
static SIZE_TYPE KTagEnd_size = 2;

CNcbiOstream& CHTMLText::PrintBegin(CNcbiOstream& out, TMode mode)
{
    const string& text = GetText();
    SIZE_TYPE tagStart = text.find(KTagStart);
    if ( tagStart == NPOS ) {
        return out << text;
    }

    out << text.substr(0, tagStart);
    SIZE_TYPE last = tagStart;
    do {
        SIZE_TYPE tagNameStart = tagStart + KTagStart_size;
        SIZE_TYPE tagNameEnd = text.find(KTagEnd, tagNameStart);
        if ( tagNameEnd == NPOS ) {
            // tag not closed
            THROW1_TRACE(runtime_error, "tag not closed");
        }
        else {
            // tag found
            if ( last != tagStart ) {
                out << text.substr(last, tagStart - last);
            }

            CNodeRef tag = MapTagAll(text.substr(tagNameStart,
                                                 tagNameEnd - tagNameStart),
                                     mode);
            if ( tag )
                tag->Print(out, mode);

            last = tagNameEnd + KTagEnd_size;
            tagStart = text.find(KTagStart, last);
        }
    } while ( tagStart != NPOS );

    if ( last != text.size() ) {
        out << text.substr(last);
    }
    return out;
}

CHTMLOpenElement::~CHTMLOpenElement(void)
{
}

CNcbiOstream& CHTMLOpenElement::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    }
    else {
        out << '<' << m_Name;
        if ( HaveAttributes() ) {
            for ( TAttributes::const_iterator i = Attributes().begin();
                  i != Attributes().end(); ++i ) {
                out << ' ' << i->first;
                if ( !i->second.IsOptional() ||
                     !i->second.GetValue().empty() ) {
                    out << "=\"" << CHTMLHelper::HTMLEncode(i->second) << '"';
                }
            }
        }
        return out << '>';
    }
}

CHTMLInlineElement::~CHTMLInlineElement(void)
{
}

CNcbiOstream& CHTMLInlineElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if ( mode != ePlainText )
        out << "</" << m_Name << '>';
    return out;
}

CHTMLElement::~CHTMLElement(void)
{
}

CNcbiOstream& CHTMLElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    CParent::PrintEnd(out, mode);
    if ( mode != ePlainText ) {
        const TMode* previous = mode.GetPreviousContext();
        if ( previous ) {
            CNCBINode* parent = previous->GetNode();
            if ( parent && parent->HaveChildren() &&
                 parent->Children().size() > 1 )
                out << endl; // separate child nodes by newline
        }
        else {
            out << endl;
        }
    }
    return out;
}


// HTML comment class

CHTMLComment::~CHTMLComment(void)
{
}

CNcbiOstream& CHTMLComment::Print(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    }
    else {
        return CParent::Print(out, mode);
    }
}

CNcbiOstream& CHTMLComment::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    }
    else {
        return out << "<!--";
    }
}

CNcbiOstream& CHTMLComment::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    }
    else {
        return out << "-->";
    }
}

CHTMLListElement::~CHTMLListElement(void)
{
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


// Special char

CHTMLSpecialChar::CHTMLSpecialChar(const char* html, const char* plain,
                                   int count)
    : CParent("", plain)
{
    m_Name  = html;
    m_Count = count;
}

CHTMLSpecialChar::~CHTMLSpecialChar(void)
{
}

CNcbiOstream& CHTMLSpecialChar::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText ) {
        for ( int i = 0; i < m_Count; i++ )
            out << m_Plain;
    } else {
        for ( int i = 0; i < m_Count; i++ )
            out << "&" << m_Name << ";";
    }
    return out;
}


// the <HTML> tag

const char CHTML_html::sm_TagName[] = "html";

CHTML_html::~CHTML_html(void)
{
}

void CHTML_html::Init(void)
{
    m_UsePopupMenu = false;
    m_PopupMenuLibUrl = kEmptyStr;
}


void CHTML_html::EnablePopupMenu(const string& menu_script_url)
{
    m_UsePopupMenu = true;
    m_PopupMenuLibUrl = menu_script_url;
}


static bool s_CheckUsePopupMenus(const CNCBINode* node)
{
    iterate ( CNCBINode::TChildren, i, node->Children() ) {
        const CNCBINode* cnode = node->Node(i);
        if ( dynamic_cast<const CHTMLPopupMenu*>(cnode) ) {
            return true;
        }
        if ( cnode->HaveChildren()  &&  s_CheckUsePopupMenus(cnode)) {
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
    if ( !m_UsePopupMenu ) {
        m_UsePopupMenu = s_CheckUsePopupMenus(this);
    }

    if ( !m_UsePopupMenu  ||  !HaveChildren() ) {
        return CParent::PrintChildren(out, mode);
    }

    non_const_iterate ( TChildren, i, Children() ) {

        if ( dynamic_cast<CHTML_head*>(Node(i)) ) {
             Node(i)->AppendChild(new CHTMLText(
                      CHTMLPopupMenu::GetCodeHead(m_PopupMenuLibUrl)));
        } else {
            if ( dynamic_cast<CHTML_body*>(Node(i)) ) {
                 Node(i)->AppendChild(new CHTMLText(
                          CHTMLPopupMenu::GetCodeBody()));
            }
        }
    }
    return CParent::PrintChildren(out, mode);
}



// TABLE element

class CHTML_tc_Cache
{
public:
    CHTML_tc_Cache(void)
        : m_Used(false), m_Node(0)
        {
        }

    bool IsUsed(void) const
        {
            return m_Used;
        }

    bool IsNode(void) const
        {
            return m_Node != 0;
        }
    CHTML_tc* GetCellNode(void) const
        {
            return m_Node;
        }

    void SetUsed(void);
    void SetCellNode(CHTML_tc* node);

private:
    bool m_Used;
    CHTML_tc* m_Node;
};

class CHTML_tr_Cache
{
public:
    typedef CHTML_table::TIndex TIndex;

    CHTML_tr_Cache(void)
        : m_Node(0),
          m_CellCount(0), m_CellsSize(0), m_Cells(0), m_FilledCellCount(0)
        {
        }
    ~CHTML_tr_Cache(void)
        {
            delete[] m_Cells;
        }

    CHTML_tr* GetRowNode(void) const
        {
            return m_Node;
        }
    void SetRowNode(CHTML_tr* rowNode)
        {
            _ASSERT(!m_Node && rowNode);
            m_Node = rowNode;
        }

    TIndex GetCellCount(void) const
        {
            return m_CellCount;
        }
    CHTML_tc_Cache& GetCellCache(TIndex col);

    void AppendCell(CHTML_tr* rowNode, TIndex col,
                    CHTML_tc* cellNode, TIndex colSpan);
    void SetUsedCells(TIndex colBegin, TIndex colEnd);
    void SetUsedCells(CHTML_tc* cellNode, TIndex colBegin, TIndex colEnd);

private:
    CHTML_tr_Cache(const CHTML_tr_Cache&);
    CHTML_tr_Cache& operator=(const CHTML_tr_Cache&);

    CHTML_tr* m_Node;
    TIndex m_CellCount;
    TIndex m_CellsSize;
    CHTML_tc_Cache* m_Cells;
    TIndex m_FilledCellCount;
};

class CHTML_table_Cache
{
public:
    typedef CHTML_table::TIndex TIndex;

    CHTML_table_Cache(CHTML_table* table);
    ~CHTML_table_Cache(void);

    TIndex GetRowCount(void) const
        {
            return m_RowCount;
        }

    CHTML_tr_Cache& GetRowCache(TIndex row);
    CHTML_tr* GetRowNode(TIndex row);
    CHTML_tc* GetCellNode(TIndex row, TIndex col,
                          CHTML_table::ECellType type);
    CHTML_tc* GetCellNode(TIndex row, TIndex col,
                          CHTML_table::ECellType type,
                          TIndex rowSpan, TIndex colSpan);

    void InitRow(TIndex row, CHTML_tr* rowNode);
    void SetUsedCells(TIndex rowBegin, TIndex rowEnd,
                      TIndex colBegin, TIndex colEnd);
private:
    CHTML_table* m_Node;
    TIndex m_RowCount;
    TIndex m_RowsSize;
    CHTML_tr_Cache** m_Rows;
    TIndex m_FilledRowCount;

    CHTML_table_Cache(const CHTML_table_Cache&);
    CHTML_table_Cache& operator=(const CHTML_table_Cache&);
};

CHTML_tr::CHTML_tr(void)
    : CParent("tr"), m_Parent(0)
{
}

CHTML_tr::CHTML_tr(CNCBINode* node)
    : CParent("tr", node), m_Parent(0)
{
}

CHTML_tr::CHTML_tr(const string& text)
    : CParent("tr", text), m_Parent(0)
{
}

void CHTML_tr::DoAppendChild(CNCBINode* node)
{
    CHTML_tc* cell = dynamic_cast<CHTML_tc*>(node);
    if ( cell ) {
        // adding new cell
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
        out << CHTMLHelper::GetNL();
        if (m_Parent->m_IsRowSep == CHTML_table::ePrintRowSep) {
            out << string(GetTextLength(mode), m_Parent->m_RowSepChar)
                << CHTMLHelper::GetNL();
        }
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

    non_const_iterate ( TChildren, i, Children() ) {
        if ( i != Children().begin() ) {
            out << m_Parent->m_ColSepM;
        }
        Node(i)->Print(out, mode);
    }

    out << m_Parent->m_ColSepR;

    return out;
}

size_t CHTML_tr::GetTextLength(TMode mode)
{
    if ( !HaveChildren() ) {
        return 0;
    }

    CNcbiOstrstream sout;
    size_t cols = 0;

    non_const_iterate ( TChildren, i, Children() ) {
        Node(i)->Print(sout, mode);
        cols++;
    }
    sout.put('\0');

    size_t textlen = strlen(sout.str());
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
    if ( !node->HaveAttribute(attributeName) )
        return 1;

    const string& value = node->GetAttribute(attributeName);

    try {
        CHTML_table::TIndex span = NStr::StringToUInt(value);
        if ( span > 0 )
            return span;
    }
    catch ( exception& ) {
        // error will be posted later
    }
    ERR_POST("Bad attribute: " << attributeName << "=\"" << value << "\"");
    return 1;
}

void CHTML_tc::DoSetAttribute(const string& name,
                              const string& value, bool optional)
{
    if ( name == "rowspan" || name == "colspan" ) // changing cell size
        ResetTableCache();
    CParent::DoSetAttribute(name, value, optional);
}

void CHTML_tc::ResetTableCache(void)
{
    if ( m_Parent )
        m_Parent->ResetTableCache();
}

void CHTML_tc_Cache::SetUsed()
{
    if ( IsUsed() )
        THROW1_TRACE(runtime_error, "Overlapped table cells");
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
    for ( TIndex col = colBegin; col < colEnd; ++col )
        GetCellCache(col).SetUsed();
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
    if ( colSpan != 1 )
        SetUsedCells(col + 1, col + colSpan);
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
    for ( TIndex i = 0; i < GetRowCount(); ++i )
        delete m_Rows[i];
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

    // scan all children (which should be <TH> or <TD> tags)
    if ( rowNode->HaveChildren() ) {
        // beginning with column 0
        TIndex col = 0;
        for ( CNCBINode::TChildren::iterator iCol = rowNode->ChildBegin(),
                  iColEnd = rowNode->ChildEnd();
              iCol != iColEnd; ++iCol ) {
            CHTML_tc* cellNode =
                dynamic_cast<CHTML_tc*>(rowNode->Node(iCol));

            if ( !cellNode )
                continue;

            // skip all used cells
            while ( rowCache.GetCellCache(col).IsUsed() ) {
                ++col;
            }

            // determine current cell size
            TIndex rowSpan = x_GetSpan(cellNode, "rowspan");
            TIndex colSpan = x_GetSpan(cellNode, "colspan");

            // end of new cell in columns
            rowCache.SetUsedCells(cellNode, col, col + colSpan);
            if ( rowSpan > 1 )
                SetUsedCells(row + 1, row + rowSpan, col, col + colSpan);

            // skip this cell's columns
            col += colSpan;
        }
    }
}

CHTML_table_Cache::CHTML_table_Cache(CHTML_table* table)
    : m_Node(table),
      m_RowCount(0), m_RowsSize(0), m_Rows(0), m_FilledRowCount(0)
{
    // scan all children (which should be <TR> tags)
    if ( table->HaveChildren() ) {
        // beginning with row 0
        TIndex row = 0;
        for ( CNCBINode::TChildren::iterator iRow = table->ChildBegin(),
                  iRowEnd = table->ChildEnd(); iRow != iRowEnd; ++iRow ) {
            CHTML_tr* rowNode = dynamic_cast<CHTML_tr*>(table->Node(iRow));
            if( !rowNode )
                continue;

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
                    THROW1_TRACE(runtime_error,
                                 "wrong cell type: TH expected");
                break;
            case CHTML_table::eDataCell:
                if ( !dynamic_cast<CHTML_td*>(cell) )
                    THROW1_TRACE(runtime_error,
                                 "wrong cell type: TD expected");
                break;
            default:
                break;
            }
            return cell;
        }
        if ( cellCache.IsUsed() )
            THROW1_TRACE(runtime_error, "invalid use of big table cell");
    }
    CHTML_tc* cell;
    if ( type == CHTML_table::eHeaderCell )
        cell = new CHTML_th;
    else
        cell = new CHTML_td;
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
                    THROW1_TRACE(runtime_error,
                                 "wrong cell type: TH expected");
                break;
            case CHTML_table::eDataCell:
                if ( !dynamic_cast<CHTML_td*>(cell) )
                    THROW1_TRACE(runtime_error,
                                 "wrong cell type: TD expected");
                break;
            default:
                break;
            }
            if ( x_GetSpan(cell, "rowspan") != rowSpan ||
                 x_GetSpan(cell, "colspan") != colSpan )
                THROW1_TRACE(runtime_error, "cannot change table cell size");
            return cell;
        }
        if ( cellCache.IsUsed() )
            THROW1_TRACE(runtime_error, "invalid use of big table cell");
    }

    CHTML_tc* cell;
    if ( type == CHTML_table::eHeaderCell )
        cell = new CHTML_th;
    else
        cell = new CHTML_td;

    if ( colSpan != 1 )
        cell->SetColSpan(colSpan);
    if ( rowSpan != 1 )
        cell->SetRowSpan(rowSpan);
    rowCache.AppendCell(GetRowNode(row), col, cell, colSpan);
    if ( rowSpan != 1 )
        SetUsedCells(row + 1, row + rowSpan, col, col + colSpan);
    return cell;
}

CHTML_table::CHTML_table(void)
    : CParent("table"), m_CurrentRow(0), m_CurrentCol(TIndex(-1)),
      m_ColSepL(kEmptyStr), m_ColSepM((kEmptyStr), m_ColSepR(kEmptyStr),
      m_RowSepChar('-'), m_IsRowSep(eSkipRowSep)
{
}

CHTML_table::~CHTML_table(void)
{
}

CHTML_table* CHTML_table::SetCellSpacing(int spacing)
{
    SetAttribute("cellspacing", spacing);
    return this;
}

CHTML_table* CHTML_table::SetCellPadding(int padding)
{
    SetAttribute("cellpadding", padding);
    return this;
}

void CHTML_table::ResetTableCache(void)
{
    m_Cache.reset(0);
}

CHTML_table_Cache& CHTML_table::GetCache(void) const
{
    CHTML_table_Cache* cache = m_Cache.get();
    if ( !cache )
        m_Cache.reset(cache = new CHTML_table_Cache(const_cast<CHTML_table*>(this)));
    return *cache;
}

void CHTML_table::DoAppendChild(CNCBINode* node)
{
    CHTML_tr* row = dynamic_cast<CHTML_tr*>(node);
    if ( row ) {
        // adding new row
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

CHTML_tr* CHTML_table::Row(TIndex row)  // todo: exception
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
    for ( TIndex i = 0; i < cache.GetRowCount(); ++i )
        columns = max(columns, cache.GetRowCache(i).GetCellCount());
    return columns;
}

CHTML_table::TIndex CHTML_table::CalculateNumberOfRows(void) const
{
    return GetCache().GetRowCount();
}

CNcbiOstream& CHTML_table::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if ( mode == ePlainText  &&  HaveChildren() ) {
        size_t seplen = 0;
        CHTML_tr* tr = dynamic_cast<CHTML_tr*>(&**Children().begin());
        if ( tr ) {
            seplen = tr->GetTextLength(mode);
        }
        if (seplen > m_ColSepL.length() + m_ColSepR.length() &&
            m_IsRowSep == ePrintRowSep) {
            out << string(seplen, m_RowSepChar) << CHTMLHelper::GetNL();
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


// form element

CHTML_form::CHTML_form(void)
    : CParent("form")
{
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

// legend element

CHTML_legend::CHTML_legend(const string& legend)
    : CParent("legend", legend)
{
}

CHTML_legend::CHTML_legend(CHTMLNode* legend)
    : CParent("legend", legend)
{
}

CHTML_legend::~CHTML_legend(void)
{
}

// fieldset element

CHTML_fieldset::CHTML_fieldset(void)
    : CParent("fieldset")
{
}

CHTML_fieldset::CHTML_fieldset(const string& legend)
    : CParent("fieldset", new CHTML_legend(legend))
{
}

CHTML_fieldset::CHTML_fieldset(CHTML_legend* legend)
    : CParent("fieldset", legend)
{
}

CHTML_fieldset::~CHTML_fieldset(void)
{
}

// label element

CHTML_label::CHTML_label(const string& text)
    : CParent("label", text)
{
}

CHTML_label::CHTML_label(const string& text, const string& idRef)
    : CParent("label", text)
{
    SetFor(idRef);
}

CHTML_label::~CHTML_label(void)
{
}

void CHTML_label::SetFor(const string& idRef)
{
    SetAttribute("for", idRef);
}

// textarea element

CHTML_textarea::CHTML_textarea(const string& name, int cols, int rows)
    : CParent("textarea")
{
    SetNameAttribute(name);
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
}

//input tag

CHTML_input::CHTML_input(const char* type, const string& name)
    : CParent("input")
{
    SetAttribute("type", type);
    SetNameAttribute(name);
}

CHTML_input::~CHTML_input(void)
{
}

// checkbox tag

const char CHTML_checkbox::sm_InputType[] = "checkbox";

CHTML_checkbox::CHTML_checkbox(const string& name)
    : CParent(sm_InputType, name)
{
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
    AppendPlainText(description);  // adds the description at the end
}

CHTML_checkbox::CHTML_checkbox(const string& name, const string& value,
                               bool checked, const string& description)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
    SetOptionalAttribute("checked", checked);
    AppendPlainText(description);  // adds the description at the end
}

CHTML_checkbox::~CHTML_checkbox(void)
{
}

// image tag

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
}

// radio tag

const char CHTML_radio::sm_InputType[] = "radio";

CHTML_radio::CHTML_radio(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
}

CHTML_radio::CHTML_radio(const string& name, const string& value, bool checked, const string& description)
    : CParent(sm_InputType, name)
{
    SetAttribute("value", value);
    SetOptionalAttribute("checked", checked);
    AppendPlainText(description);  // adds the description at the end
}

CHTML_radio::~CHTML_radio(void)
{
}

// hidden tag

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
}

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
}

const char CHTML_reset::sm_InputType[] = "reset";

CHTML_reset::CHTML_reset(const string& label)
    : CParent(sm_InputType, NcbiEmptyString)
{
    SetOptionalAttribute("value", label);
}

CHTML_reset::~CHTML_reset(void)
{
}

// button tag
/*
  commented out because it's not supported in most browsers
CHTML_button::CHTML_button(const string& text, EButtonType type)
    : CParent("button", text)
{
    SetType(type);
}

CHTML_button::CHTML_button(CNCBINode* contents, EButtonType type)
    : CParent("button", contents)
{
    SetType(type);
}

CHTML_button::CHTML_button(const string& text, const string& name,
                           const string& value)
    : CParent("button", text)
{
    SetSubmit(name, value);
}

CHTML_button::CHTML_button(CNCBINode* contents, const string& name,
                           const string& value)
    : CParent("button", contents)
{
    SetSubmit(name, value);
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

CHTML_button* CHTML_button::SetSubmit(const string& name,
                                      const string& value)
{
    SetNameAttribute(name);
    SetOptionalAttribute("value", value);
    return this;
}

CHTML_button::~CHTML_button(void)
{
}
*/

// text tag

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

CHTML_text::CHTML_text(const string& name, int size, int maxlength, const string& value)
    : CParent(sm_InputType, name)
{
    SetSize(size);
    SetAttribute("maxlength", maxlength);
    SetOptionalAttribute("value", value);
}

CHTML_text::~CHTML_text(void)
{
}

// text tag

const char CHTML_file::sm_InputType[] = "file";

CHTML_file::CHTML_file(const string& name, const string& value)
    : CParent(sm_InputType, name)
{
    SetOptionalAttribute("value", value);
}

CHTML_file::~CHTML_file(void)
{
}

const char CHTML_select::sm_TagName[] = "select";

CHTML_select::~CHTML_select(void)
{
}

CHTML_select* CHTML_select::SetMultiple(void)
{
    SetAttribute("multiple");
    return this;
}

const char CHTML_option::sm_TagName[] = "option";

CHTML_option::~CHTML_option(void)
{
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

const char CHTML_a::sm_TagName[] = "a";

CHTML_a::~CHTML_a(void)
{
}

CHTML_a* CHTML_a::SetHref(const string& href)
{
    SetAttribute("href", href);
    return this;
}


// br tag

const char CHTML_br::sm_TagName[] = "br";

CHTML_br::CHTML_br(int count)
    : CParent(sm_TagName)
{
    for ( int i = 1; i < count; ++i )
        AppendChild(new CHTML_br());
}

CHTML_br::~CHTML_br(void)
{
}

CNcbiOstream& CHTML_br::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out << CHTMLHelper::GetNL();
    }
    else {
        return CParent::PrintBegin(out,mode);
    }
}

// img tag

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
}


// dl tag

const char CHTML_dl::sm_TagName[] = "dl";

CHTML_dl::~CHTML_dl(void)
{
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
    if ( !definition.empty() )
        AppendChild(new CHTML_dd(definition));
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

const char CHTML_ol::sm_TagName[] = "ol";

CHTML_ol::~CHTML_ol(void)
{
}

CHTML_ol* CHTML_ol::SetStart(int start)
{
    SetAttribute("start", start);
    return this;
}

const char CHTML_ul::sm_TagName[] = "ul";

CHTML_ul::~CHTML_ul(void)
{
}

const char CHTML_dir::sm_TagName[] = "dir";

CHTML_dir::~CHTML_dir(void)
{
}

const char CHTML_menu::sm_TagName[] = "menu";

CHTML_menu::~CHTML_menu(void)
{
}

const char CHTML_font::sm_TagName[] = "font";

CHTML_font::~CHTML_font(void)
{
}

CHTML_font* CHTML_font::SetTypeFace(const string& typeface)
{
    SetAttribute("typeface", typeface);
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
}

CHTML_basefont* CHTML_basefont::SetTypeFace(const string& typeface)
{
    SetAttribute("typeface", typeface);
    return this;
}

CHTML_color::~CHTML_color(void)
{
}


// hr tag

const char CHTML_hr::sm_TagName[] = "hr";

CHTML_hr::~CHTML_hr(void)
{
}

CHTML_hr* CHTML_hr::SetNoShade(void)
{
    SetAttribute("noshade");
    return this;
}

CNcbiOstream& CHTML_hr::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out << CHTMLHelper::GetNL() << CHTMLHelper::GetNL();
    }
    else {
        return CParent::PrintBegin(out, mode);
    }
}


// meta tag

const char CHTML_meta::sm_TagName[] = "meta";

CHTML_meta::CHTML_meta(EType mtype, const string& var, const string& content)
    : CParent(sm_TagName)
{
    SetAttribute(( mtype == eName ) ? "name" : "http-equiv", var);
    SetAttribute("content", content);
}

CHTML_meta::~CHTML_meta(void)
{
}


// script tag

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
}

CHTML_script* CHTML_script::AppendScript(const string& script)
{
    AppendChild(new CHTMLPlainText("\n<!--\n" + script + "-->\n", true));
    return this;
}


// other tags

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
DEFINE_HTML_ELEMENT(map);
DEFINE_HTML_ELEMENT(area);


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
