/*  $RCSfile$  $Revision$  $Date$
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
* File Description:
*   code for CHTMLNode
*
* ---------------------------------------------------------------------------
* $Log$
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
* CAsnWriteNode made simple node. Use CHTML_pre explicitely.
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


#include <html/html.hpp>
#include <html/htmlhelper.hpp>

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
                if ( !i->second.empty() ) {
                    out << "=\"" << CHTMLHelper::HTMLEncode(i->second) << '"';
                }
            }
        }
        return out << '>';
    }
}

CHTMLElement::~CHTMLElement(void)
{
}

CNcbiOstream& CHTMLElement::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    } else {
        return out << "</" << m_Name << ">\n";
    }
}

// HTML comment class
CHTMLComment::~CHTMLComment(void)
{
}

CNcbiOstream& CHTMLComment::PrintBegin(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    } else {
        return out << "<!--";
    }
}

CNcbiOstream& CHTMLComment::PrintEnd(CNcbiOstream& out, TMode mode)
{
    if( mode == ePlainText ) {
        return out;
    } else {
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

// TABLE element

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

CHTML_table::CHTML_table(void)
    : CParent("table"), m_CurrentRow(0), m_CurrentCol(TIndex(-1))
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

class CHTML_table::CTableInfo
{
public:
    TIndex m_Rows;
    TIndex m_Columns;
        TIndex m_FinalRow;
    vector<TIndex> m_FinalRowSpans;
    vector<TIndex> m_RowSizes;
    bool m_BadNode, m_BadRowNode, m_BadCellNode, m_Overlapped, m_BadSpan;
    
    CTableInfo(void);
    void AddRowSize(TIndex columns);
    void SetFinalRowSpans(TIndex rows, const vector<TIndex>& rowSpans);
};

CHTML_table::TIndex CHTML_table::sx_GetSpan(const CNCBINode* node,
                            const string& attributeName, CTableInfo* info)
{
    if ( !node->HaveAttribute(attributeName) )
        return 1;

    TIndex span;
    try {
        span = NStr::StringToUInt(node->GetAttribute(attributeName));
    }
    catch (runtime_error&) {
        if ( info )
            info->m_BadSpan = true;
        return 1;
    }
    if ( span == 0 || span > KMaxIndex ) {
        if ( info )
            info->m_BadSpan = true;
        return 1;
    }
    return span;
}

CHTML_tr* CHTML_table::Row(TIndex needRow)  // todo: exception
{
    // beginning with row 0
    TIndex row = 0;
    // scan all children (which should be <TR> tags)
    if ( HaveChildren() ) {
        for ( TChildren::iterator iRow = ChildBegin();
              iRow != ChildEnd(); ++iRow, ++row ) {
            CHTMLNode* trNode = dynamic_cast<CHTMLNode*>(Node(iRow));
            if ( !trNode ) {
                THROW1_TRACE(runtime_error,
                             "Table contains non-HTML node");
            }
            if( !sx_IsRow(trNode) ) {
                if( !sx_CanContain(trNode) ) {
                    THROW1_TRACE(runtime_error,
                                 "Table contains wrong tag '" +
                                  trNode->GetName() + "'");
                }
                continue;
            }
            
            if ( row == needRow ) {
                return static_cast<CHTML_tr*>(trNode);
            }
        }
    }

    // add needed empty rows (needRow > 0 here)
    CHTML_tr* trNode;
    do {
        // add needed rows
        AppendChild(trNode = new CHTML_tr);
    } while ( ++row <= needRow );
    return trNode;
}

CHTML_tc* CHTML_table::sx_CheckType(CHTMLNode* cell, ECellType type)
{
    CHTML_tc* ret;
    switch (type) {
    case eHeaderCell:
        ret = dynamic_cast<CHTML_th*>(cell);
        if ( !ret )
            THROW1_TRACE(runtime_error,
                         "CHTML_table: wrong cell type: TH expected");
        break;
    case eDataCell:
        ret = dynamic_cast<CHTML_td*>(cell);
        if ( !ret )
            THROW1_TRACE(runtime_error,
                         "CHTML_table: wrong cell type: TD expected");
        break;
    default:
        ret = dynamic_cast<CHTML_tc*>(cell);
        if ( !ret )
            THROW1_TRACE(runtime_error,
                         "CHTML_table: wrong cell type: TD or TH expected");
        break;
    }
    return ret;
}

CHTML_tc* CHTML_table::Cell(TIndex needRow, TIndex needCol, ECellType type)
{
    vector<TIndex> rowSpans; // hanging down cells (with rowspan > 1)

    // beginning with row 0
    TIndex row = 0;
    TIndex needRowCols = 0;
    CHTMLNode* needRowNode = 0;
    // scan all children (which should be <TR> tags)
    if ( HaveChildren() ) {
        for ( TChildren::iterator iRow = ChildBegin();
              iRow != ChildEnd(); ++iRow ) {
            CHTMLNode* trNode = dynamic_cast<CHTMLNode*>(Node(iRow));
            if( !trNode ) {
                THROW1_TRACE(runtime_error,
                             "Table contains non-HTML node");
            }
            if( !sx_IsRow(trNode) ) {
                if( !sx_CanContain(trNode) ) {
                    THROW1_TRACE(runtime_error,
                                 "Table contains wrong tag '" +
                                 trNode->GetName() + "'");
                }
                continue;
            }
            
            // beginning with column 0
            TIndex col = 0;
            // scan all children (which should be <TH> or <TD> tags)
            if ( trNode->HaveChildren() ) {
                for ( TChildren::iterator iCol = trNode->ChildBegin();
                      iCol != trNode->ChildEnd(); ++iCol ) {
                    CHTMLNode* cell = dynamic_cast<CHTMLNode*>(Node(iCol));
                    
                    if ( !cell || !sx_IsCell(cell) ) {
                        THROW1_TRACE(runtime_error,
                                     "Table row contains non- <TH> or <TD> tag");
                    }
                    
                    // skip all used cells
                    while ( col < rowSpans.size() && rowSpans[col] > 0 ) {
                        --rowSpans[col++];
                    }
                    
                    if ( row == needRow ) {
                        if ( col == needCol ) {
                            CHTML_tc* c = sx_CheckType(cell, type);
                            m_CurrentRow = needRow;
                            m_CurrentCol = needCol;
                            return c;
                        }
                        if ( col > needCol )
                            THROW1_TRACE(runtime_error,
                                         "Table cells are overlapped");
                    }
                    
                    // determine current cell size
                    TIndex rowSpan =
                        sx_GetSpan(cell, "rowspan", 0);
                    TIndex colSpan =
                        sx_GetSpan(cell, "colspan", 0);
                    
                    // end of new cell in columns
                    const TIndex colEnd = col + colSpan;
                    // check that there is enough space
                    for ( TIndex i = col; i < colEnd && i < rowSpans.size(); ++i ) {
                        if ( rowSpans[i] ) {
                            THROW1_TRACE(runtime_error,
                                         "Table cells are overlapped");
                        }
                    }
                    
                    if ( rowSpan > 1 ) {
                        // we should remember space used by this cell
                        // first expand rowSpans vector if needed
                        if ( rowSpans.size() < colEnd )
                            rowSpans.resize(colEnd);
                        
                        // then store span number
                        for ( TIndex i = col; i < colEnd; ++i ) {
                            rowSpans[i] = max(rowSpans[i], rowSpan - 1);
                        }
                    }
                    // skip this cell's columns
                    col += colSpan;
                }
            }

            ++row;
            
            if ( row > needRow ) {
                needRowCols = col;
                needRowNode = trNode;
                break;
            }
        }
    }

    while ( row <= needRow ) {
        // add needed rows
        AppendChild(needRowNode = new CHTML_tr);
        ++row;
        // decrement hanging cell sizes
        for ( TIndex i = 0; i < rowSpans.size(); ++i ) {
            if ( rowSpans[i] )
                --rowSpans[i];
        }
    }

    // this row doesn't have enough columns -> add some
    TIndex addCells = 0;

    do {
        // skip hanging cells
        while ( needRowCols < rowSpans.size() && rowSpans[needRowCols] > 0 ) {
            if ( needRowCols == needCol )
                THROW1_TRACE(runtime_error,
                             "Table cells are overlapped");
            ++needRowCols;
        }
        // allocate one column cell
        ++addCells;
        ++needRowCols;
    } while ( needRowCols <= needCol );

    // add needed columns
    CHTML_tc* cell;
    for ( TIndex i = 1; i < addCells; ++i ) {
        needRowNode->AppendChild(cell = new CHTML_td);
    }
    if ( type == eHeaderCell )
        cell = new CHTML_th;
    else
        cell = new CHTML_td;
    needRowNode->AppendChild(cell);
    m_CurrentRow = needRow;
    m_CurrentCol = needCol;
    return cell;
}

bool CHTML_table::sx_IsRow(const CNCBINode* node)
{
    return dynamic_cast<const CHTML_tr*>(node) != 0;
}

bool CHTML_table::sx_IsCell(const CNCBINode* node)
{
    return dynamic_cast<const CHTML_tc*>(node) != 0;
}

static auto_ptr< set<string, PNocase> > CHTML_table_validTags;

bool CHTML_table::sx_CanContain(const CNCBINode* node)
{
    if ( !CHTML_table_validTags.get() ) {
        CHTML_table_validTags.reset(new set<string, PNocase>);
        CHTML_table_validTags->insert("caption");
        CHTML_table_validTags->insert("col");
        CHTML_table_validTags->insert("colgroup");
        CHTML_table_validTags->insert("tbody");
        CHTML_table_validTags->insert("tfoot");
        CHTML_table_validTags->insert("thead");
        CHTML_table_validTags->insert("tr");
    }
    return CHTML_table_validTags->find(node->GetName()) !=
        CHTML_table_validTags->end();
}

void CHTML_table::x_CheckTable(CTableInfo *info) const
{
    vector<TIndex> rowSpans; // hanging down cells (with rowspan > 1)

    // beginning with row 0
    TIndex row = 0;
    // scan all children (which should be <TR> tags)
    if ( HaveChildren() ) {
        for ( TChildren::const_iterator iRow = ChildBegin();
              iRow != ChildEnd();
              ++iRow ) {
            const CNCBINode* trNode = Node(iRow);
            if( !sx_IsRow(trNode) ) {
                if( !sx_CanContain(trNode) ) {
                    if ( info ) {
                        // we just gathering info -> skip wrong tag
                        info->m_BadNode = info->m_BadRowNode = true;
                        continue;
                    }
                    THROW1_TRACE(runtime_error,
                                 "Table contains wrong tag '" +
                                 trNode->GetName() + "'");
                }
                continue;
            }
            
            // beginning with column 0
            TIndex col = 0;
            // scan all children (which should be <TH> or <TD> tags)
            if ( trNode->HaveChildren() ) {
                for ( TChildren::const_iterator iCol = trNode->ChildBegin();
                      iCol != trNode->ChildEnd(); ++iCol ) {
                    const CNCBINode* cell = Node(iCol);
                    
                    if ( !sx_IsCell(cell) ) {
                        if ( info ) {
                            // we just gathering info -> skip wrong tag
                            info->m_BadNode = info->m_BadCellNode = true;
                            continue;
                        }
                        THROW1_TRACE(runtime_error,
                                     "Table row contains non- <TH> or <TD> tag");
                    }
                    
                    // skip all used cells
                    while ( col < rowSpans.size() && rowSpans[col] > 0 ) {
                        --rowSpans[col++];
                    }
                    
                    // determine current cell size
                    TIndex rowSpan =
                        sx_GetSpan(cell, "rowspan", info);
                    TIndex colSpan =
                        sx_GetSpan(cell, "colspan", info);
                    
                    // end of new cell in columns
                    const TIndex colEnd = col + colSpan;
                    // check that there is enough space
                    for ( TIndex i = col; i < colEnd && i < rowSpans.size(); ++i ) {
                        if ( rowSpans[i] ) {
                            if ( info )
                                info->m_Overlapped = true;
                            else {
                                THROW1_TRACE(runtime_error,
                                             "Table cells are overlapped");
                            }
                        }
                    }
                    
                    if ( rowSpan > 1 ) {
                        // we should remember space used by this cell
                        // first expand rowSpans vector if needed
                        if ( rowSpans.size() < colEnd )
                            rowSpans.resize(colEnd);
                        
                        // then store span number
                        for ( TIndex i = col; i < colEnd; ++i ) {
                            rowSpans[i] = rowSpan - 1;
                        }
                    }
                    // skip this cell's columns
                    col += colSpan;
                }
            }
            
            // skip all used cells
            while ( col < rowSpans.size() && rowSpans[col] > 0 ) {
                --rowSpans[col++];
            }
            
            if ( info )
                info->AddRowSize(col);
            ++row;
        }
    }
    
    if ( info )
        info->SetFinalRowSpans(row, rowSpans);
}

CHTML_table::CTableInfo::CTableInfo(void)
    : m_Rows(0), m_Columns(0), m_FinalRow(0),
      m_BadNode(false), m_BadRowNode(false), m_BadCellNode(false),
      m_Overlapped(false), m_BadSpan(false)
{
}

void CHTML_table::CTableInfo::AddRowSize(TIndex columns)
{
    m_RowSizes.push_back(columns);
    m_Columns = max(m_Columns, columns);
}

void CHTML_table::CTableInfo::SetFinalRowSpans(TIndex row,
                                               const vector<TIndex>& rowSpans)
{
    m_FinalRow = row;
    m_FinalRowSpans = rowSpans;

    // check for the rest of rowSpans
    TIndex addRows = 0;
    for ( TIndex i = 0; i < rowSpans.size(); ++i ) {
        addRows = max(addRows, rowSpans[i]);
    }
    m_Rows = row + addRows;
}

CHTML_table::TIndex CHTML_table::CalculateNumberOfColumns(void) const
{
    CTableInfo info;
    x_CheckTable(&info);
    return info.m_Columns;
}

CHTML_table::TIndex CHTML_table::CalculateNumberOfRows(void) const
{
    CTableInfo info;
    x_CheckTable(&info);
    return info.m_Rows;
}

CNcbiOstream& CHTML_table::PrintChildren(CNcbiOstream& out, TMode mode)
{
    CTableInfo info;
    x_CheckTable(&info);

    if( mode == ePlainText ) {
        out << CHTMLHelper::GetNL();
    }
    TIndex row = 0;
    if ( HaveChildren() ) {
        for ( TChildren::iterator iRow = ChildBegin();
              iRow != ChildEnd(); ++iRow ) {
            
            CNCBINode* rowNode = Node(iRow);
            if ( !sx_IsRow(rowNode) )
                rowNode->Print(out,mode);
            else {
                rowNode->Print(out,mode); 
                ++row;
            }
            if( mode == ePlainText ) {
                out << CHTMLHelper::GetNL();
            }
        }
    }

    if( mode == eHTML ) {
        // print implicit rows
        for ( TIndex i = info.m_FinalRow; i < info.m_Rows; ++i )
            out << "<TR></TR>";
    }
    
    return out;
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

CHTML_image::CHTML_image(const string& name, const string& src)
    : CParent(sm_InputType, name)
{
    SetAttribute("src", src);
}

CHTML_image::CHTML_image(const string& name, const string& src, int border)
    : CParent(sm_InputType, name)
{
    SetAttribute("src", src);
    SetAttribute("border", border);
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
    } else {
        return CParent::PrintBegin(out,mode);
    }
}

// img tag

CHTML_img::CHTML_img(const string& url)
    : CParent("img")
{
    SetAttribute("src", url);
}

CHTML_img::CHTML_img(const string& url, int width, int height)
    : CParent("img")
{
    SetAttribute("src", url);
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
        return out << CHTMLHelper::GetNL()
                   << CHTMLHelper::GetNL();
    } else {
        return CParent::PrintBegin(out, mode);
    }
}

#define DEFINE_HTML_ELEMENT(Tag) \
CHTML_(Tag)::~CHTML_(Tag)(void) \
{ \
} \
const char CHTML_(Tag)::sm_TagName[] = #Tag

DEFINE_HTML_ELEMENT(html);
DEFINE_HTML_ELEMENT(head);
DEFINE_HTML_ELEMENT(body);
DEFINE_HTML_ELEMENT(base);
DEFINE_HTML_ELEMENT(isindex);
DEFINE_HTML_ELEMENT(link);
DEFINE_HTML_ELEMENT(meta);
DEFINE_HTML_ELEMENT(script);
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
DEFINE_HTML_ELEMENT(tr);
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
