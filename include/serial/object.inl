#if defined(OBJECT__HPP)  &&  !defined(OBJECT__INL)
#define OBJECT__INL

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
* Revision 1.1  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

inline
CObject* CObjectInfo::GetCObjectPtr(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    if ( !IsCObject(typeInfo) )
        return 0;
    CObject* object = static_cast<CObject*>(objectPtr);
    if ( object->ReferenceCount() == 0 ) {
        ERR_POST("CObjectInfo() with non referenced object");
        return 0;
    }
    return object;
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr),
      m_Ref(GetCObjectPtr(objectPtr, typeInfo))
{
}

inline
CObjectInfo::CObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(GetCObjectPtr(object.first, object.second))
{
}

inline
void CObjectInfo::Reset(void)
{
    m_ObjectPtr = 0;
    ResetTypeInfo();
    m_Ref.Reset();
}

inline
void CObjectInfo::Set(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_ObjectPtr = objectPtr;
    SetTypeInfo(typeInfo);
    m_Ref.Reset(GetCObjectPtr(objectPtr, typeInfo));
}

inline
void CObjectInfo::Set(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
}

inline
void CObjectInfo::Set(const CObjectInfo& object)
{
    *this = object;
}

inline
const CObject* CConstObjectInfo::GetCObjectPtr(TConstObjectPtr objectPtr,
                                               TTypeInfo typeInfo)
{
    if ( !IsCObject(typeInfo) )
        return 0;
    const CObject* object = static_cast<const CObject*>(objectPtr);
    if ( object->ReferenceCount() == 0 ) {
        ERR_POST("CObjectInfo() with non referenced object");
        return 0;
    }
    return object;
}

inline
CConstObjectInfo::CConstObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr),
      m_Ref(GetCObjectPtr(objectPtr, typeInfo))
{
}

inline
CConstObjectInfo::CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(GetCObjectPtr(object.first, object.second))
{
}

inline
CConstObjectInfo::CConstObjectInfo(pair<NCBI_NS_NCBI::TObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(GetCObjectPtr(object.first, object.second))
{
}

inline
CConstObjectInfo::CConstObjectInfo(const CObjectInfo& object)
    : CObjectTypeInfo(object), m_ObjectPtr(object.GetObjectPtr()),
      m_Ref(object.GetObjectRef())
{
}

inline
void CConstObjectInfo::Reset(void)
{
    m_ObjectPtr = 0;
    ResetTypeInfo();
    m_Ref.Reset();
}

inline
void CConstObjectInfo::Set(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_ObjectPtr = objectPtr;
    SetTypeInfo(typeInfo);
    m_Ref.Reset(GetCObjectPtr(objectPtr, typeInfo));
}

inline
void CConstObjectInfo::Set(NCBI_NS_NCBI::TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_ObjectPtr = objectPtr;
    SetTypeInfo(typeInfo);
    m_Ref.Reset(GetCObjectPtr(objectPtr, typeInfo));
}

inline
void CConstObjectInfo::Set(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
}

inline
void CConstObjectInfo::Set(pair<NCBI_NS_NCBI::TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
}

inline
void CConstObjectInfo::Set(const CConstObjectInfo& object)
{
    *this = object;
}

inline
void CConstObjectInfo::Set(const CObjectInfo& object)
{
    m_ObjectPtr = object.GetObjectPtr();
    SetTypeInfo(object.GetTypeInfo());
    m_Ref = object.GetObjectRef();
}

#endif /* def OBJECT__HPP  &&  ndef OBJECT__INL */
