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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/09/18 20:00:11  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.2  2000/03/29 15:55:22  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.1  1999/05/19 19:56:33  vasilche
* Commit just in case.
*
* ===========================================================================
*/

inline
ETypeFamily CTypeInfo::GetTypeFamily(void) const
{
    return m_TypeFamily;
}

inline
const string& CTypeInfo::GetName(void) const
{
    return m_Name;
}

inline
size_t CTypeInfo::GetSize(void) const
{
    return m_Size;
}

inline
bool CTypeInfo::IsOrMayContainType(TTypeInfo typeInfo) const
{
    return IsType(typeInfo) || MayContainType(typeInfo);
}

inline
TObjectPtr CTypeInfo::Create(void) const
{
    return m_CreateFunction(this);
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
    m_SkipFunction(in, this);
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
void CTypeInfo::SetGlobalReadHook(CReadObjectHook* hook)
{
    SetReadHook(0, hook);
}

inline
void CTypeInfo::SetLocalReadHook(CObjectIStream& in,
                                 CReadObjectHook* hook)
{
    SetReadHook(&in, hook);
}

inline
void CTypeInfo::ResetGlobalReadHook(void)
{
    ResetReadHook(0);
}

inline
void CTypeInfo::ResetLocalReadHook(CObjectIStream& in)
{
    ResetReadHook(&in);
}

inline
void CTypeInfo::SetGlobalWriteHook(CWriteObjectHook* hook)
{
    SetWriteHook(0, hook);
}

inline
void CTypeInfo::SetLocalWriteHook(CObjectOStream& stream,
                                 CWriteObjectHook* hook)
{
    SetWriteHook(&stream, hook);
}

inline
void CTypeInfo::ResetGlobalWriteHook(void)
{
    ResetWriteHook(0);
}

inline
void CTypeInfo::ResetLocalWriteHook(CObjectOStream& stream)
{
    ResetWriteHook(&stream);
}

inline
void CTypeInfo::SetGlobalCopyHook(CCopyObjectHook* hook)
{
    SetCopyHook(0, hook);
}

inline
void CTypeInfo::SetLocalCopyHook(CObjectStreamCopier& stream,
                                 CCopyObjectHook* hook)
{
    SetCopyHook(&stream, hook);
}

inline
void CTypeInfo::ResetGlobalCopyHook(void)
{
    ResetCopyHook(0);
}

inline
void CTypeInfo::ResetLocalCopyHook(CObjectStreamCopier& stream)
{
    ResetCopyHook(&stream);
}

#endif /* def TYPEINFO__HPP  &&  ndef TYPEINFO__INL */
