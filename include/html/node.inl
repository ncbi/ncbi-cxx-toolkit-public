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
* Revision 1.2  1998/12/23 21:20:58  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.1  1998/12/21 22:24:57  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/


//  !!!!!!!!!!!!!!!!!!!!!!!!!!
//  !!! PUT YOUR CODE HERE !!!
//  !!!!!!!!!!!!!!!!!!!!!!!!!!

inline CNCBINode::TChildList::iterator CNCBINode::ChildBegin(void)
{
    return m_ChildNodes.begin();
}

inline CNCBINode::TChildList::iterator CNCBINode::ChildEnd(void)
{
    return m_ChildNodes.end();
}

inline CNCBINode::TChildList::const_iterator CNCBINode::ChildBegin(void) const
{
    return m_ChildNodes.begin();
}

inline CNCBINode::TChildList::const_iterator CNCBINode::ChildEnd(void) const
{
    return m_ChildNodes.end();
}

inline const string& CNCBINode::GetName(void) const
{
    return m_Name;
}

inline void CNCBINode::SetName(const string& name)
{
    m_Name = name;
}

inline void CNCBINode::SetAttribute(const string& name, int value)
{
    SetAttribute(name, IntToString(value));
}

inline void CNCBINode::SetAttribute(const char* name, int value)
{
    SetAttribute(name, IntToString(value));
}

inline void CNCBINode::SetOptionalAttribute(const string& name, const string& value)
{
    if ( !value.empty() )
        SetAttribute(name, value);
}

inline void CNCBINode::SetOptionalAttribute(const string& name, bool value)
{
    if ( value )
        SetAttribute(name);
}

inline void CNCBINode::SetOptionalAttribute(const char* name, const string& value)
{
    if ( !value.empty() )
        SetAttribute(name, value);
}

inline void CNCBINode::SetOptionalAttribute(const char* name, bool value)
{
    if ( value )
        SetAttribute(name);
}

// append a child
inline CNCBINode* CNCBINode::AppendChild(CNCBINode* child)
{
    if ( child )
        DoAppendChild(child);

    return this;
}

#endif /* def NODE__HPP  &&  ndef NODE__INL */
