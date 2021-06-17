#if defined(MEMBERID__HPP)  &&  !defined(MEMBERID__INL)
#define MEMBERID__INL

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
*/

inline
const string& CMemberId::GetName(void) const
{
    return m_Name;
}

inline
CMemberId::TTag CMemberId::GetTag(void) const
{
    return m_Tag;
}
inline
CAsnBinaryDefs::ETagClass CMemberId::GetTagClass(void) const
{
    return m_TagClass;
}
inline
CAsnBinaryDefs::ETagType  CMemberId::GetTagType(void) const
{
    return m_TagType;
}
inline
CAsnBinaryDefs::ETagConstructed CMemberId::GetTagConstructed(void) const {
    return m_TagConstructed;
}
inline
bool CMemberId::IsTagConstructed(void) const {
    return m_TagConstructed == CAsnBinaryDefs::eConstructed;
}

inline
bool CMemberId::IsTagImplicit(void) const {
    return m_TagType == CAsnBinaryDefs::eImplicit;
}

inline
bool CMemberId::HasTag(void) const
{
    return m_Tag != eNoExplicitTag;
}

inline
bool CMemberId::HaveExplicitTag(void) const
{
    return HasTag() && GetTagType() != CAsnBinaryDefs::eAutomatic;
}

inline
bool CMemberId::HaveParentTag(void) const
{
    return GetTag() == eParentTag && GetTagClass() == CAsnBinaryDefs::eContextSpecific;
}

inline
void CMemberId::SetName(const string& name)
{
    m_Name = name;
}

inline
void CMemberId::SetTag(TTag tag,
    CAsnBinaryDefs::ETagClass tagclass, CAsnBinaryDefs::ETagType tagtype)

{
    m_Tag = tag;
    m_TagClass = tagclass;
    m_TagType = tagtype;
}

inline
bool CMemberId::HaveNoPrefix(void) const
{
    return m_NoPrefix;
}

inline
bool CMemberId::IsAttlist(void) const
{
    return m_Attlist;
}

inline
bool CMemberId::HasNotag(void) const
{
    return m_Notag;
}

inline
bool CMemberId::HasAnyContent(void) const
{
    return m_AnyContent;
}

inline
bool CMemberId::IsCompressed(void) const
{
    return m_Compressed;
}

inline
ENsQualifiedMode CMemberId::IsNsQualified(void) const
{
    return m_NsqMode;
}

#endif /* def MEMBERID__HPP  &&  ndef MEMBERID__INL */
