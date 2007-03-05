#if defined(OBJECTIO__HPP)  &&  !defined(OBJECTIO__INL)
#define OBJECTIO__INL

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

// read/write class

inline
CObjectOStream& COStreamFrame::GetStream(void) const
{
    return m_Stream;
}

// read/write container
inline
CObjectIStream& CIStreamFrame::GetStream(void) const
{
    return m_Stream;
}

inline
const CObjectTypeInfo& CIStreamContainerIterator::GetContainerType(void) const
{
    return m_ContainerType;
}

inline
const CObjectTypeInfo& COStreamContainer::GetContainerType(void) const
{
    return m_ContainerType;
}

inline
bool CIStreamClassMemberIterator::HaveMore(void) const
{
    return m_MemberIndex != kInvalidMember;
}

inline
CIStreamClassMemberIterator& CIStreamClassMemberIterator::operator++(void)
{
    NextClassMember();
    return *this;
}

inline
CObjectTypeInfoMI CIStreamClassMemberIterator::operator*(void) const
{
    return CObjectTypeInfoMI(m_ClassType, m_MemberIndex);
}

inline
bool CIStreamContainerIterator::HaveMore(void) const
{
    return m_State == eElementBegin;
}

#endif /* def OBJECTIO__HPP  &&  ndef OBJECTIO__INL */
