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


CNodeRef CNCBINode::RemoveChild(CNCBINode* child)
{
    CNodeRef ref(child);

    if ( child  &&  HaveChildren() ) {
        SIZE_TYPE prev_size = Children().size();
        // Remove all child nodes from the list.
        TChildren& children = Children();
        // It is better to use Children().remove_if(...) here,
        // but WorkShop's version works only with plain functions :(
        typedef TChildren::iterator TChildrenIt;
        for (TChildrenIt it = children.begin(); it != children.end(); ) {
            if ( it->GetPointer() == child ) {
                TChildrenIt cur = it;
                ++it;
                children.erase(cur);
            } else {
                ++it;
            }
        }
#  if !NCBI_LIGHTWEIGHT_LIST
        if ( !children.size() ) {
            m_Children.release();
        }
#  endif        
        if ( children.size() != prev_size ) {
            return ref;
        }
    }
    NCBI_THROW(CHTMLException, eNotFound,
               "Specified node is not a child of the current node");
    // not reached
    return CNodeRef(0);
}


CNodeRef CNCBINode::RemoveChild(CNodeRef& child)
{
    return RemoveChild(child.GetPointer());
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

    size_t n_count = GetRepeatCount();
    for (size_t i = 0; i < n_count; i++ )
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
            CHTMLException new_e(DIAG_COMPILE_INFO, 0,
                                 CHTMLException::eUnknown, e.GetMsg());
            new_e.AddTraceInfo(GetName());
            throw new_e;
        }
        catch (exception& e) {
            TExceptionFlags flags = GetExceptionFlags();
            if ( (flags  &  CNCBINode::fCatchAll) == 0 ) {
                throw;
            }
            CHTMLException new_e(DIAG_COMPILE_INFO, 0,
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
            CHTMLException new_e(DIAG_COMPILE_INFO, 0,
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


void CNCBINode::ReInitialize(void)
{
    RemoveAllChildren();
    m_CreateSubNodesCalled = false;
}


void CNCBINode::CreateSubNodes(void)
{
    return;
}


void CNCBINode::SetExceptionFlags(TExceptionFlags flags)
{
    s_TlsExceptionFlags->SetValue(
        reinterpret_cast<TExceptionFlags*> ((intptr_t) flags));
}


CNCBINode::TExceptionFlags CNCBINode::GetExceptionFlags()
{
    // Some 64 bit compilers refuse to cast from int* to EExceptionFlags
    return EExceptionFlags((intptr_t) s_TlsExceptionFlags->GetValue());
}


END_NCBI_SCOPE
