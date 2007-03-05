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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/impl/memberlist.hpp>

BEGIN_NCBI_SCOPE

CMemberId::CMemberId(void)
    : m_Tag(eNoExplicitTag), m_ExplicitTag(false),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
}

CMemberId::CMemberId(TTag tag, bool explicitTag)
    : m_Tag(tag), m_ExplicitTag(explicitTag),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
}

CMemberId::CMemberId(const string& name)
    : m_Name(name), m_Tag(eNoExplicitTag), m_ExplicitTag(false),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
}

CMemberId::CMemberId(const string& name, TTag tag, bool explicitTag)
    : m_Name(name), m_Tag(tag), m_ExplicitTag(explicitTag),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
}

CMemberId::CMemberId(const char* name)
    : m_Name(name), m_Tag(eNoExplicitTag), m_ExplicitTag(false),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
    _ASSERT(name);
}

CMemberId::CMemberId(const char* name, TTag tag, bool explicitTag)
    : m_Name(name), m_Tag(tag), m_ExplicitTag(explicitTag),
    m_NoPrefix(false), m_Attlist(false), m_Notag(false), m_AnyContent(false),
    m_Compressed(false)
{
    _ASSERT(name);
}

CMemberId::~CMemberId(void)
{
}

bool CMemberId::HaveParentTag(void) const
{
    return GetTag() == eParentTag && !HaveExplicitTag();
}

void CMemberId::SetParentTag(void)
{
    SetTag(eParentTag, false);
}

string CMemberId::ToString(void) const
{
    if ( !m_Name.empty() )
        return m_Name;
    else
        return '[' + NStr::IntToString(GetTag()) + ']';
}

void CMemberId::SetNoPrefix(void)
{
    m_NoPrefix = true;
}

bool CMemberId::HaveNoPrefix(void) const
{
    return m_NoPrefix;
}

void CMemberId::SetAttlist(void)
{
    m_Attlist = true;
}
bool CMemberId::IsAttlist(void) const
{
    return m_Attlist;
}

void CMemberId::SetNotag(void)
{
    m_Notag = true;
}
bool CMemberId::HasNotag(void) const
{
    return m_Notag;
}

void CMemberId::SetAnyContent(void)
{
    m_AnyContent = true;
}
bool CMemberId::HasAnyContent(void) const
{
    return m_AnyContent;
}

void CMemberId::SetCompressed(void)
{
    m_Compressed = true;
}
bool CMemberId::IsCompressed(void) const
{
    return m_Compressed;
}

END_NCBI_SCOPE
