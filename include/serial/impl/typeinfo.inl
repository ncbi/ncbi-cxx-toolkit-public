#if defined(TYPEINFO__HPP)  &&  !defined(TYPEINFO__INL)
#define TYPEINFO__INL

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
ETypeFamily CTypeInfo::GetTypeFamily(void) const
{
    return m_TypeFamily;
}

inline
size_t CTypeInfo::GetSize(void) const
{
    return m_Size;
}

inline
bool CTypeInfo::MayContainType(TTypeInfo typeInfo) const
{
    return GetMayContainType(typeInfo) == eMayContainType_yes;
}

inline
CTypeInfo::EMayContainType
CTypeInfo::IsOrMayContainType(TTypeInfo typeInfo) const
{
    return IsType(typeInfo)? eMayContainType_yes: GetMayContainType(typeInfo);
}

inline
TObjectPtr CTypeInfo::Create(CObjectMemoryPool* memoryPool) const
{
    return m_CreateFunction(this, memoryPool);
}

inline
void CTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    m_ReadHookData.GetCurrentFunction()(in, this, object);
}

inline
void CTypeInfo::WriteData(CObjectOStream& out, TConstObjectPtr object) const
{
    m_WriteHookData.GetCurrentFunction()(out, this, object);
}

inline
void CTypeInfo::CopyData(CObjectStreamCopier& copier) const
{
    m_CopyHookData.GetCurrentFunction()(copier, this);
}

inline
void CTypeInfo::SkipData(CObjectIStream& in) const
{
    m_SkipHookData.GetCurrentFunction()(in, this);
}

inline
void CTypeInfo::DefaultReadData(CObjectIStream& in,
                                TObjectPtr objectPtr) const
{
    m_ReadHookData.GetDefaultFunction()(in, this, objectPtr);
}

inline
void CTypeInfo::DefaultWriteData(CObjectOStream& out,
                                 TConstObjectPtr objectPtr) const
{
    m_WriteHookData.GetDefaultFunction()(out, this, objectPtr);
}

inline
void CTypeInfo::DefaultCopyData(CObjectStreamCopier& copier) const
{
    m_CopyHookData.GetDefaultFunction()(copier, this);
}

inline
void CTypeInfo::DefaultSkipData(CObjectIStream& in) const
{
    m_SkipHookData.GetDefaultFunction()(in, this);
}

inline
bool CTypeInfo::IsCObject(void) const
{
    return m_IsCObject;
}

inline
bool CTypeInfo::IsInternal(void) const
{
    return m_IsInternal;
}

#endif /* def TYPEINFO__HPP  &&  ndef TYPEINFO__INL */
