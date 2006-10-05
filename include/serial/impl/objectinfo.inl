#if defined(OBJECTINFO__HPP)  &&  !defined(OBJECTINFO__INL)
#define OBJECTINFO__INL

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

/////////////////////////////////////////////////////////////////////////////
// CObjectTypeInfo
/////////////////////////////////////////////////////////////////////////////

inline
CObjectTypeInfo::CObjectTypeInfo(TTypeInfo typeinfo)
    : m_TypeInfo(typeinfo)
{
}

inline
TTypeInfo CObjectTypeInfo::GetTypeInfo(void) const
{
    return m_TypeInfo;
}

inline
CTypeInfo* CObjectTypeInfo::GetNCTypeInfo(void) const
{
    return const_cast<CTypeInfo*>(GetTypeInfo());
}

inline
ETypeFamily CObjectTypeInfo::GetTypeFamily(void) const
{
    return GetTypeInfo()->GetTypeFamily();
}

inline
void CObjectTypeInfo::CheckTypeFamily(ETypeFamily family) const
{
    if ( GetTypeInfo()->GetTypeFamily() != family )
        WrongTypeFamily(family);
}

inline
void CObjectTypeInfo::ResetTypeInfo(void)
{
    m_TypeInfo = 0;
}

inline
void CObjectTypeInfo::SetTypeInfo(TTypeInfo typeinfo)
{
    m_TypeInfo = typeinfo;
}

inline
bool CObjectTypeInfo::operator==(const CObjectTypeInfo& type) const
{
    return GetTypeInfo() == type.GetTypeInfo();
}

inline
bool CObjectTypeInfo::operator!=(const CObjectTypeInfo& type) const
{
    return GetTypeInfo() != type.GetTypeInfo();
}

/////////////////////////////////////////////////////////////////////////////
// CConstObjectInfo
/////////////////////////////////////////////////////////////////////////////

inline
CConstObjectInfo::CConstObjectInfo(void)
    : m_ObjectPtr(0)
{
}

inline
CConstObjectInfo::CConstObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr),
      m_Ref(typeInfo->GetCObjectPtr(objectPtr))
{
}

inline
CConstObjectInfo::CConstObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo,
                                   ENonCObject)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr)
{
    _ASSERT(!typeInfo->IsCObject() ||
            static_cast<const CObject*>(objectPtr)->Referenced() ||
            !static_cast<const CObject*>(objectPtr)->CanBeDeleted());
}

inline
CConstObjectInfo::CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(object.second->GetCObjectPtr(object.first))
{
}

inline
CConstObjectInfo::CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(object.second->GetCObjectPtr(object.first))
{
}

inline
void CConstObjectInfo::ResetObjectPtr(void)
{
    m_ObjectPtr = 0;
    m_Ref.Reset();
}

inline
TConstObjectPtr CConstObjectInfo::GetObjectPtr(void) const
{
    return m_ObjectPtr;
}

inline
pair<TConstObjectPtr, TTypeInfo> CConstObjectInfo::GetPair(void) const
{
    return make_pair(GetObjectPtr(), GetTypeInfo());
}

inline
void CConstObjectInfo::Reset(void)
{
    ResetObjectPtr();
    ResetTypeInfo();
}

inline
void CConstObjectInfo::Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_ObjectPtr = objectPtr;
    SetTypeInfo(typeInfo);
    m_Ref.Reset(typeInfo->GetCObjectPtr(objectPtr));
}

inline
CConstObjectInfo&
CConstObjectInfo::operator=(pair<TConstObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

inline
CConstObjectInfo&
CConstObjectInfo::operator=(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

/////////////////////////////////////////////////////////////////////////////
// CObjectInfo
/////////////////////////////////////////////////////////////////////////////

inline
CObjectInfo::CObjectInfo(void)
{
}

inline
CObjectInfo::CObjectInfo(TTypeInfo typeInfo)
    : CParent(typeInfo->Create(), typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo)
    : CParent(objectPtr, typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr,
                         TTypeInfo typeInfo,
                         ENonCObject nonCObject)
    : CParent(objectPtr, typeInfo, nonCObject)
{
}

inline
CObjectInfo::CObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CParent(object)
{
}

inline
TObjectPtr CObjectInfo::GetObjectPtr(void) const
{
    return const_cast<TObjectPtr>(CParent::GetObjectPtr());
}

inline
pair<TObjectPtr, TTypeInfo> CObjectInfo::GetPair(void) const
{
    return make_pair(GetObjectPtr(), GetTypeInfo());
}

inline
CObjectInfo&
CObjectInfo::operator=(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

#endif /* def OBJECTINFO__HPP  &&  ndef OBJECTINFO__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.6  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.5  2003/11/24 14:10:04  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.4  2003/11/18 18:11:47  grichenk
* Resolve aliased type info before using it in CObjectTypeInfo
*
* Revision 1.3  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.2  2001/05/17 14:57:22  lavr
* Typos corrected
*
* Revision 1.1  2000/10/20 15:51:24  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/
