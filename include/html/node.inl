#if defined(HTML___NODE__HPP)  &&  !defined(HTML___NODE__INL)
#define HTML___NODE__INL

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
 * Author:  Eugene Vasilchenko
 *
 */


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
    return &**i;
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
    return &**i;
}


inline
CNCBINode* CNCBINode::AppendChild(CNCBINode* child)
{
    if ( child ) {
        DoAppendChild(child);
    }
    return this;
}


inline
CNCBINode* CNCBINode::AppendChild(CNodeRef& ref)
{
    DoAppendChild(ref.GetPointer());
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
    if ( !attributes ) {
        m_Attributes.reset(attributes = new TAttributes);
    }
    return *attributes;
#endif
}


inline
void CNCBINode::SetAttribute(const string& name, const string& value)
{
    DoSetAttribute(name, value, false);
}


inline
void CNCBINode::SetOptionalAttribute(const string& name, const string& value)
{
    if ( !value.empty() ) {
        SetAttribute(name, value);
    }
}


inline
void CNCBINode::SetOptionalAttribute(const string& name, bool value)
{
    if ( value ) {
        SetAttribute(name);
    }
}


inline
void CNCBINode::SetOptionalAttribute(const char* name, const string& value)
{
    if ( !value.empty() ) {
        SetAttribute(name, value);
    }
}


inline
void CNCBINode::SetOptionalAttribute(const char* name, bool value)
{
    if ( value ) {
        SetAttribute(name);
    }
}


inline
size_t CNCBINode::GetRepeatCount(void)
{
    return m_RepeatCount;
}


inline
void CNCBINode::SetRepeatCount(size_t count)
{
    m_RepeatCount = count;
}


inline
void CNCBINode::RepeatTag(bool repeat)
{
    m_RepeatTag = repeat;
}


inline
bool CNCBINode::NeedRepeatTag(void)
{
    return m_RepeatTag;
}

#endif /* def HTML___NODE__HPP  &&  ndef HTML___NODE__INL */
