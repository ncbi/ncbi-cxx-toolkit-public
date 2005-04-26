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


#endif /* def TYPEINFO__HPP  &&  ndef TYPEINFO__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.11  2003/12/01 19:04:22  grichenk
* Moved Add and Sub from serialutil to ncbimisc, made them methods
* of CRawPointer class.
*
* Revision 1.10  2003/11/24 14:10:04  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.9  2003/11/18 18:11:48  grichenk
* Resolve aliased type info before using it in CObjectTypeInfo
*
* Revision 1.8  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.7  2002/12/23 18:38:52  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.6  2002/09/09 18:14:00  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.5  2001/10/22 15:16:20  grichenk
* Optimized CTypeInfo::IsCObject()
*
* Revision 1.4  2000/09/29 16:18:15  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
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
