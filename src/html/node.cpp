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
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <html/node.hpp>
#include <html/html_exception.hpp>


BEGIN_NCBI_SCOPE


// Store global exception handling flags in TLS
static CSafeStaticRef< CTls<CNCBINode::TExceptionFlags> > s_TlsExceptionFlags;


CNCBINode::CNCBINode(void)
    : m_CreateSubNodesCalled(false),
      m_RepeatCount(1),
      m_RepeatTag(false)
{
    return;
}


CNCBINode::CNCBINode(const string& name)
    : m_CreateSubNodesCalled(false),
      m_Name(name),
      m_RepeatCount(1),
      m_RepeatTag(false)
{
    return;
}


CNCBINode::CNCBINode(const char* name)
    : m_CreateSubNodesCalled(false),
      m_Name(name),
      m_RepeatCount(1),
      m_RepeatTag(false)
{
    return;
}


CNCBINode::~CNCBINode(void)
{
    return;
}


static bool s_CheckEndlessRecursion(const CNCBINode* parent,
                                    const CNCBINode* child)
{
    if ( !parent  ||  !child  ||  !child->HaveChildren() ) {
        return false;
    }
    ITERATE ( CNCBINode::TChildren, i, child->Children() ) {
        const CNCBINode* cnode = parent->Node(i);
        if ( parent == cnode ) {
            return true;
        }
        if ( cnode->HaveChildren()  &&
             s_CheckEndlessRecursion(parent, cnode)) {
            return true;
        }
    }
    return false;
}


void CNCBINode::DoAppendChild(CNCBINode* child)
{
    // Check endless recursion
    TExceptionFlags flags = GetExceptionFlags();
    if ( (flags  &  CNCBINode::fDisableCheckRecursion) == 0 ) {
        if ( this == child ) {
            NCBI_THROW(CHTMLException, eEndlessRecursion,
                "Endless recursion: current and child nodes are identical");
        }
        if ( s_CheckEndlessRecursion(this, child) ) {
            NCBI_THROW(CHTMLException, eEndlessRecursion,
                "Endless recursion: appended node contains current node " \
                "in the child nodes list");
        }
    }
    GetChildren().push_back(CRef<ncbi::CNCBINode>(child));
}


void CNCBINode::RemoveAllChildren(void)
{
#if NCBI_LIGHTWEIGHT_LIST
    m_Children.clear();
#else
    m_Children.reset(0);
#endif
}


bool CNCBINode::HaveAttribute(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return true;
        }
    }
    return false;
}


const string& CNCBINode::GetAttribute(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return ptr->second;
        }
    }
    return NcbiEmptyString;
}


bool CNCBINode::AttributeIsOptional(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return ptr->second.IsOptional();
        }
    }
    return true;
}


bool CNCBINode::AttributeIsOptional(const char* name) const
{
    return AttributeIsOptional(string(name));
}


const string* CNCBINode::GetAttributeValue(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return &ptr->second.GetValue();
        }
    }
    return 0;
}


void CNCBINode::SetAttribute(const string& name, int value)
{
    SetAttribute(name, NStr::IntToString(value));
}


void CNCBINode::SetAttribute(const char* name, int value)
{
    SetAttribute(name, NStr::IntToString(value));
}


void CNCBINode::SetAttribute(const string& name)
{
    DoSetAttribute(name, NcbiEmptyString, true);
}


void CNCBINode::DoSetAttribute(const string& name,
                               const string& value, bool optional)
{
    GetAttributes()[name] = SAttributeValue(value, optional);
}


void CNCBINode::SetAttributeOptional(const string& name, bool optional)
{
    GetAttributes()[name].SetOptional(optional);
}


void CNCBINode::SetAttributeOptional(const char* name, bool optional)
{
    SetAttributeOptional(string(name), optional);
}


void CNCBINode::SetAttribute(const char* name)
{
    SetAttribute(string(name));
}


void CNCBINode::SetAttribute(const char* name, const string& value)
{
    SetAttribute(string(name), value);
}


CNCBINode* CNCBINode::MapTag(const string& /*tagname*/)
{
    return 0;
}


CNodeRef CNCBINode::MapTagAll(const string& tagname, const TMode& mode)
{
    const TMode* context = &mode;
    do {
        CNCBINode* stackNode = context->GetNode();
        if ( stackNode ) {
            CNCBINode* mapNode = stackNode->MapTag(tagname);
            if ( mapNode )
                return CNodeRef(mapNode);
        }
        context = context->GetPreviousContext();
    } while ( context );
    return CNodeRef(0);
}


CNcbiOstream& CNCBINode::Print(CNcbiOstream& out, TMode prev)
{
    Initialize();
    TMode mode(&prev, this);

    int n_count = GetRepeatCount();
    for (int i = 0; i < n_count; i++ )
    {
        try {
            PrintBegin(out, mode);
            PrintChildren(out, mode);
        }
        catch (CHTMLException& e) {
            e.AddTraceInfo(GetName());
            throw;
        }
        catch (CException& e) {
            TExceptionFlags flags = GetExceptionFlags();
            if ( (flags  &  CNCBINode::fCatchAll) == 0 ) {
                throw;
            }
            CHTMLException new_e(__FILE__, __LINE__, 0,
                                 CHTMLException::eUnknown, e.GetMsg());
            new_e.AddTraceInfo(GetName());
            throw new_e;
        }
        catch (exception& e) {
            TExceptionFlags flags = GetExceptionFlags();
            if ( (flags  &  CNCBINode::fCatchAll) == 0 ) {
                throw;
            }
            CHTMLException new_e(__FILE__, __LINE__, 0,
                                 CHTMLException::eUnknown,
                                 string("CNCBINode::Print: ") + e.what());
            new_e.AddTraceInfo(GetName());
            throw new_e;
        }
        catch (...) {
            TExceptionFlags flags = GetExceptionFlags();
            if ( (flags  &  CNCBINode::fCatchAll) == 0 ) {
                throw;
            }
            CHTMLException new_e(__FILE__, __LINE__, 0,
                                 CHTMLException::eUnknown,
                                 "CNCBINode::Print: unknown exception");
            new_e.AddTraceInfo(GetName());
            throw new_e;
        }
        PrintEnd(out, mode);
    }
    return out;
}


CNcbiOstream& CNCBINode::PrintBegin(CNcbiOstream& out, TMode)
{
    return out;
}


CNcbiOstream& CNCBINode::PrintEnd(CNcbiOstream& out, TMode)
{
    return out;
}


CNcbiOstream& CNCBINode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( HaveChildren() ) {
        NON_CONST_ITERATE ( TChildren, i, Children() ) {
            Node(i)->Print(out, mode);
        }
    }
    return out;
}


void CNCBINode::Initialize(void)
{
    if ( !m_CreateSubNodesCalled ) {
        m_CreateSubNodesCalled = true;
        CreateSubNodes();
    }
}


void CNCBINode::CreateSubNodes(void)
{
    return;
}


void CNCBINode::SetExceptionFlags(TExceptionFlags flags)
{
    s_TlsExceptionFlags->SetValue(reinterpret_cast<TExceptionFlags*> (flags));
}


CNCBINode::TExceptionFlags CNCBINode::GetExceptionFlags()
{
    // Some 64 bit compilers refuse to cast from int* to EExceptionFlags
    return EExceptionFlags(long(s_TlsExceptionFlags->GetValue()));
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.32  2004/03/10 20:12:53  ivanov
 * DoAppendChild(): added check on endless recursion
 *
 * Revision 1.31  2004/02/04 17:17:57  ivanov
 * CNCBINode::Print(). Removed PrintEnd() calls from exception catch blocks.
 *
 * Revision 1.30  2004/02/02 14:26:23  ivanov
 * CNCBINode: added ability to repeat stored context
 *
 * Revision 1.29  2003/12/23 17:58:11  ivanov
 * Added exception tracing
 *
 * Revision 1.28  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.27  2003/11/03 14:48:30  ivanov
 * Moved log to end
 *
 * Revision 1.26  2003/03/11 15:28:57  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.25  2002/11/04 21:29:07  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.24  2001/05/17 15:05:42  lavr
 * Typos corrected
 *
 * Revision 1.23  2000/12/12 14:38:45  vasilche
 * Changed the way CHTMLNode::CreateSubNodes() is called.
 *
 * Revision 1.22  2000/11/01 20:37:38  vasilche
 * Removed ECanDelete enum and related constructors.
 *
 * Revision 1.21  2000/10/17 18:00:10  vasilche
 * CNCBINode::AppendChild() can throw exception if added child was not
 * allocated on heap.
 *
 * Revision 1.20  2000/08/22 16:25:39  vasilche
 * Avoid internal error of Forte compiler.
 *
 * Revision 1.19  2000/07/18 17:21:40  vasilche
 * Added possibility to force output of empty attribute value.
 * Added caching to CHTML_table, now large tables work much faster.
 * Changed algorithm of emitting EOL symbols in html output.
 *
 * Revision 1.18  2000/03/29 15:50:43  vasilche
 * Added const version of CRef - CConstRef.
 * CRef and CConstRef now accept classes inherited from CObject.
 *
 * Revision 1.17  2000/03/07 15:26:13  vasilche
 * Removed second definition of CRef.
 *
 * Revision 1.16  1999/12/28 18:55:46  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.15  1999/11/01 14:32:04  vasilche
 * Fixed null pointer reference in MapTagAll
 *
 * Revision 1.14  1999/10/28 13:40:35  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.13  1999/05/27 21:43:30  vakatov
 * Get rid of some minor compiler warnings
 *
 * Revision 1.12  1999/05/20 16:52:34  pubmed
 * SaveAsText action for query; minor changes in filters,labels, tabletemplate
 *
 * Revision 1.11  1999/05/04 00:03:18  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.10  1999/04/30 19:21:08  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.9  1999/03/18 17:54:50  vasilche
 * CNCBINode will try to call PrintEnd if exception in PrintChildren
 * occurs
 *
 * Revision 1.8  1999/01/25 19:32:44  vasilche
 * Virtual destructors now work properly.
 *
 * Revision 1.7  1999/01/04 20:06:14  vasilche
 * Redesigned CHTML_table.
 * Added selection support to HTML forms (via hidden values).
 *
 * Revision 1.6  1998/12/28 20:29:19  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.5  1998/12/23 21:21:04  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.4  1998/12/21 22:25:04  vasilche
 * A lot of cleaning.
 *
 * Revision 1.3  1998/11/23 23:42:29  lewisg
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
