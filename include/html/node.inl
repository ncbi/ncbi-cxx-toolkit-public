#if defined(NODE__HPP)  &&  !defined(NODE__INL)
#define NODE__INL

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
* Revision 1.4  1999/10/28 13:40:30  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.3  1999/01/21 16:18:04  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.2  1998/12/23 21:20:58  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.1  1998/12/21 22:24:57  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

inline
void CNCBINode::Ref(void)
{
    if ( m_RefCount++ <= 0 )
        BadRef();
}

inline
void CNCBINode::UnRef(void)
{
    if ( --m_RefCount <= 0 )
        Destroy();
}

inline
CNodeRef::TObjectType* CNodeRef::Ref(TObjectType* data)
{
    if ( data )
        data->Ref();
    return data;
}

inline
void CNodeRef::UnRef() const
{
    if ( m_Data )
        m_Data->UnRef();
}

inline
CNodeRef::CNodeRef(void)
    : m_Data(0)
{
}

inline
CNodeRef::CNodeRef(TObjectType* data)
    : m_Data(Ref(data))
{
}

inline
CNodeRef::CNodeRef(const CNodeRef& ref)
    : m_Data(Ref(ref.m_Data))
{
}

inline
CNodeRef::~CNodeRef(void)
{
    UnRef();
}

inline
CNodeRef& CNodeRef::operator=(TObjectType* data)
{
    if ( data != m_Data ) {
        UnRef();
        m_Data = Ref(data);
    }
    return *this;
}

inline
CNodeRef& CNodeRef::operator=(const CNodeRef& ref)
{
    return *this = ref.m_Data;
}

inline
CNodeRef::operator bool(void) const
{
    return m_Data != 0;
}

inline
CNodeRef::operator CNodeRef::TObjectType*(void)
{
    return m_Data;
}

inline
CNodeRef::operator const CNodeRef::TObjectType*(void) const
{
    return m_Data;
}

inline
CNodeRef::TObjectType& CNodeRef::operator*(void)
{
    return *m_Data;
}

inline
const CNodeRef::TObjectType& CNodeRef::operator*(void) const
{
    return *m_Data;
}

inline
CNodeRef::TObjectType* CNodeRef::operator->(void)
{
    return m_Data;
}

inline
const CNodeRef::TObjectType* CNodeRef::operator->(void) const
{
    return m_Data;
}

inline
CNodeRef::TObjectType* CNodeRef::get(void)
{
    return m_Data;
}

inline
const CNodeRef::TObjectType* CNodeRef::get(void) const
{
    return m_Data;
}

inline
const string& CNCBINode::GetName(void) const
{
    return m_Name;
}

inline
bool CNCBINode::HaveChildren(void) const
{
#if NCBI_LIGHTWEIGHT_LIST
    return !m_Children.empty();
#else
    return m_Children.get() != 0;
#endif
}

inline
CNCBINode::TChildren& CNCBINode::Children(void)
{
#if NCBI_LIGHTWEIGHT_LIST
    return m_Children;
#else
    return *m_Children;
#endif
}

inline
const CNCBINode::TChildren& CNCBINode::Children(void) const
{
#if NCBI_LIGHTWEIGHT_LIST
    return m_Children;
#else
    return *m_Children;
#endif
}

inline
CNCBINode::TChildren& CNCBINode::GetChildren(void)
{
#if NCBI_LIGHTWEIGHT_LIST
    return m_Children;
#else
    TChildren* children = m_Children.get();
    if ( !children )
        m_Children.reset(children = new TChildren);
    return *children;
#endif
}

inline
CNCBINode::TChildren::iterator CNCBINode::ChildBegin(void)
{
    return Children().begin();
}

inline
CNCBINode::TChildren::iterator CNCBINode::ChildEnd(void)
{
    return Children().end();
}

inline
CNCBINode* CNCBINode::Node(TChildren::iterator i)
{
    return i->get();
}

inline
CNCBINode::TChildren::const_iterator CNCBINode::ChildBegin(void) const
{
    return Children().begin();
}

inline
CNCBINode::TChildren::const_iterator CNCBINode::ChildEnd(void) const
{
    return Children().end();
}

inline
const CNCBINode* CNCBINode::Node(TChildren::const_iterator i)
{
    return i->get();
}

// append a child
inline
CNCBINode* CNCBINode::AppendChild(CNCBINode* child)
{
    if ( child )
        DoAppendChild(child);
    return this;
}

inline
bool CNCBINode::HaveAttributes(void) const
{
    return m_Attributes.get() != 0;
}

inline 
CNCBINode::TAttributes& CNCBINode::Attributes(void)
{
    return *m_Attributes;
}

inline 
const CNCBINode::TAttributes& CNCBINode::Attributes(void) const
{
    return *m_Attributes;
}

inline
CNCBINode::TAttributes& CNCBINode::GetAttributes(void)
{
#if NCBI_LIGHTWEIGHT_LIST
    return m_Attributes;
#else
    TAttributes* attributes = m_Attributes.get();
    if ( !attributes )
        m_Attributes.reset(attributes = new TAttributes);
    return *attributes;
#endif
}

inline
void CNCBINode::SetAttribute(const string& name, int value)
{
    SetAttribute(name, NStr::IntToString(value));
}

inline
void CNCBINode::SetAttribute(const char* name, int value)
{
    SetAttribute(name, NStr::IntToString(value));
}

inline
void CNCBINode::SetOptionalAttribute(const string& name, const string& value)
{
    if ( !value.empty() )
        SetAttribute(name, value);
}

inline
void CNCBINode::SetOptionalAttribute(const string& name, bool value)
{
    if ( value )
        SetAttribute(name);
}

inline
void CNCBINode::SetOptionalAttribute(const char* name, const string& value)
{
    if ( !value.empty() )
        SetAttribute(name, value);
}

inline
void CNCBINode::SetOptionalAttribute(const char* name, bool value)
{
    if ( value )
        SetAttribute(name);
}

#endif /* def NODE__HPP  &&  ndef NODE__INL */
