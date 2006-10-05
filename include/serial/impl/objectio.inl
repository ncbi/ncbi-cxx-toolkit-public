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



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.7  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.6  2003/10/24 17:50:27  vasilche
* CIStreamContainerIterator::operator++() moved to *.cpp file.
*
* Revision 1.5  2003/10/24 15:54:27  grichenk
* Removed or blocked exceptions in destructors
*
* Revision 1.4  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2001/05/17 14:58:29  lavr
* Typos corrected
*
* Revision 1.2  2001/01/22 23:23:57  vakatov
* Added   CIStreamClassMemberIterator
* Renamed CIStreamContainer --> CIStreamContainerIterator
*
* Revision 1.1  2000/10/20 15:51:25  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/
