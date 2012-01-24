#if defined(OBJLIST__HPP)  &&  !defined(OBJLIST__INL)
#define OBJLIST__INL

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
CReadObjectInfo::CReadObjectInfo(void)
    : m_TypeInfo(0), m_ObjectPtr(0)
{
}

inline
CReadObjectInfo::CReadObjectInfo(TTypeInfo typeInfo)
    : m_TypeInfo(typeInfo), m_ObjectPtr(0)
{
}

inline
CReadObjectInfo::CReadObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo)
    : m_TypeInfo(typeInfo),
      m_ObjectPtr(objectPtr), m_ObjectRef(typeInfo->GetCObjectPtr(objectPtr))
{
}

inline
TTypeInfo CReadObjectInfo::GetTypeInfo(void) const
{
    return m_TypeInfo;
}

inline
TObjectPtr CReadObjectInfo::GetObjectPtr(void) const
{
    return m_ObjectPtr;
}

inline
void CReadObjectInfo::ResetObjectPtr(void)
{
    m_ObjectPtr = 0;
    m_ObjectRef.Reset();
    m_TypeInfo = 0;
}

inline
void CReadObjectInfo::Assign(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_TypeInfo = typeInfo;
    m_ObjectPtr = objectPtr;
    m_ObjectRef.Reset(typeInfo->GetCObjectPtr(objectPtr));
}

inline
CReadObjectList::TObjectIndex CReadObjectList::GetObjectCount(void) const
{
    return m_Objects.size();
}

inline
CWriteObjectInfo::CWriteObjectInfo(void)
    : m_TypeInfo(0), m_ObjectPtr(0), m_Index(TObjectIndex(-1))
{
}

inline
CWriteObjectInfo::CWriteObjectInfo(TTypeInfo typeInfo, TObjectIndex index)
    : m_TypeInfo(typeInfo), m_ObjectPtr(0),
      m_Index(index)
{
}

inline
CWriteObjectInfo::CWriteObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo, TObjectIndex index)
    : m_TypeInfo(typeInfo), m_ObjectPtr(objectPtr),
      m_ObjectRef(typeInfo->GetCObjectPtr(objectPtr)),
      m_Index(index)
{
}

inline
CWriteObjectInfo::TObjectIndex CWriteObjectInfo::GetIndex(void) const
{
    _ASSERT(m_Index != TObjectIndex(-1));
    return m_Index;
}

inline
TTypeInfo CWriteObjectInfo::GetTypeInfo(void) const
{
    return m_TypeInfo;
}

inline
TConstObjectPtr CWriteObjectInfo::GetObjectPtr(void) const
{
    return m_ObjectPtr;
}

inline
const CConstRef<CObject>& CWriteObjectInfo::GetObjectRef(void) const
{
    return m_ObjectRef;
}

inline
void CWriteObjectInfo::ResetObjectPtr(void)
{
    m_ObjectPtr = 0;
    m_ObjectRef.Reset();
    m_TypeInfo = 0;
}

inline
CWriteObjectList::TObjectIndex CWriteObjectList::GetObjectCount(void) const
{
    return m_Objects.size();
}

inline
CWriteObjectList::TObjectIndex CWriteObjectList::NextObjectIndex(void) const
{
    return GetObjectCount();
}

#endif /* def OBJLIST__HPP  &&  ndef OBJLIST__INL */
