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
* Revision 1.45  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.44  2004/01/05 14:25:22  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.43  2003/11/24 14:10:05  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.42  2003/11/18 18:11:49  grichenk
* Resolve aliased type info before using it in CObjectTypeInfo
*
* Revision 1.41  2003/09/16 14:48:36  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.40  2003/08/26 19:25:58  gouriano
* added possibility to discard a member of an STL container
* (from a read hook)
*
* Revision 1.39  2003/08/25 15:59:09  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.38  2003/07/29 18:47:48  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.37  2003/06/04 17:02:13  rsmith
* Move static mutex out of function to work around CW complex initialization bug.
*
* Revision 1.36  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.35  2002/12/23 18:42:22  dicuccio
* Added #include to avoid warnings in Wn32 about deletion of incomplete types
*
* Revision 1.34  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.33  2002/08/13 13:56:06  grichenk
* Improved MT-safety in CTypeInfo and CTypeRef
*
* Revision 1.32  2001/12/09 07:38:13  vakatov
* Got rid of a minor GCC warning (fixed member's order of initialization)
*
* Revision 1.31  2001/10/22 15:16:22  grichenk
* Optimized CTypeInfo::IsCObject()
*
* Revision 1.30  2001/05/17 15:07:09  lavr
* Typos corrected
*
* Revision 1.29  2000/11/07 17:25:42  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.28  2000/10/20 15:51:44  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.27  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.26  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.25  2000/09/18 20:00:26  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.24  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.23  2000/07/03 18:42:48  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.22  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.21  2000/06/07 19:46:01  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.20  2000/05/24 20:08:50  vasilche
* Implemented XML dump.
*
* Revision 1.19  2000/03/29 15:55:30  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.18  2000/02/17 20:02:46  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.17  1999/12/17 19:05:05  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.16  1999/09/14 18:54:21  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.15  1999/08/31 17:50:09  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.14  1999/07/20 18:23:14  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.13  1999/07/19 15:50:39  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.12  1999/07/13 20:18:24  vasilche
* Changed types naming.
*
* Revision 1.11  1999/07/07 19:59:09  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.10  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.9  1999/07/01 17:55:36  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.8  1999/06/30 16:05:06  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.7  1999/06/24 14:45:02  vasilche
* Added binary ASN.1 output.
*
* Revision 1.6  1999/06/15 16:19:53  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:50  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.3  1999/06/07 19:30:28  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:51  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:59  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeinfoimpl.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialimpl.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

/* put back inside GetTypeInfoMutex when Mac CodeWarrior 9 comes out */
DEFINE_STATIC_MUTEX(s_TypeInfoMutex);

SSystemMutex& GetTypeInfoMutex(void)
{
    return s_TypeInfoMutex;
}


class CTypeInfoFunctions
{
public:
    static void ReadWithHook(CObjectIStream& in,
                             TTypeInfo objectType, TObjectPtr objectPtr);
    static void WriteWithHook(CObjectOStream& out,
                              TTypeInfo objectType, TConstObjectPtr objectPtr);
    static void SkipWithHook(CObjectIStream& stream,
                             TTypeInfo objectType);
    static void CopyWithHook(CObjectStreamCopier& copier,
                             TTypeInfo objectType);
};

class CNamespaceInfoItem
{
public:
    CNamespaceInfoItem(void);
    CNamespaceInfoItem(const CNamespaceInfoItem& other);
    virtual ~CNamespaceInfoItem(void);

    bool HasNamespaceName(void) const;
    const string& GetNamespaceName(void) const;
    void SetNamespaceName(const string& ns_name);

    bool HasNamespacePrefix(void) const;
    const string& GetNamespacePrefix(void) const;
    void SetNamespacePrefix(const string& ns_prefix);

private:
    string m_NsName;
    string m_NsPrefix;
    bool   m_NsPrefixSet;
};

typedef CTypeInfoFunctions TFunc;

CTypeInfo::CTypeInfo(ETypeFamily typeFamily, size_t size)
    : m_TypeFamily(typeFamily), m_Size(size), m_Name(),
      m_InfoItem(0),
      m_IsCObject(false),
      m_CreateFunction(&CVoidTypeFunctions::Create),
      m_ReadHookData(&CVoidTypeFunctions::Read, &TFunc::ReadWithHook),
      m_WriteHookData(&CVoidTypeFunctions::Write, &TFunc::WriteWithHook),
      m_SkipHookData(&CVoidTypeFunctions::Skip, &TFunc::SkipWithHook),
      m_CopyHookData(&CVoidTypeFunctions::Copy, &TFunc::CopyWithHook)
{
    return;
}


CTypeInfo::CTypeInfo(ETypeFamily typeFamily, size_t size, const char* name)
    : m_TypeFamily(typeFamily), m_Size(size), m_Name(name),
      m_InfoItem(0),
      m_IsCObject(false),
      m_CreateFunction(&CVoidTypeFunctions::Create),
      m_ReadHookData(&CVoidTypeFunctions::Read, &TFunc::ReadWithHook),
      m_WriteHookData(&CVoidTypeFunctions::Write, &TFunc::WriteWithHook),
      m_SkipHookData(&CVoidTypeFunctions::Skip, &TFunc::SkipWithHook),
      m_CopyHookData(&CVoidTypeFunctions::Copy, &TFunc::CopyWithHook)
{
    return;
}


CTypeInfo::CTypeInfo(ETypeFamily typeFamily, size_t size, const string& name)
    : m_TypeFamily(typeFamily), m_Size(size), m_Name(name),
      m_InfoItem(0),
      m_IsCObject(false),
      m_CreateFunction(&CVoidTypeFunctions::Create),
      m_ReadHookData(&CVoidTypeFunctions::Read, &TFunc::ReadWithHook),
      m_WriteHookData(&CVoidTypeFunctions::Write, &TFunc::WriteWithHook),
      m_SkipHookData(&CVoidTypeFunctions::Skip, &TFunc::SkipWithHook),
      m_CopyHookData(&CVoidTypeFunctions::Copy, &TFunc::CopyWithHook)
{
    return;
}


CTypeInfo::~CTypeInfo(void)
{
    if (m_InfoItem) {
        delete m_InfoItem;
    }
    return;
}

bool CTypeInfo::HasNamespaceName(void) const
{
    return m_InfoItem ? m_InfoItem->HasNamespaceName() : false;
}

const string& CTypeInfo::GetNamespaceName(void) const
{
    return m_InfoItem ? m_InfoItem->GetNamespaceName() : kEmptyStr;
}

void CTypeInfo::SetNamespaceName(const string& ns_name) const
{
    x_CreateInfoItemIfNeeded();
    m_InfoItem->SetNamespaceName(ns_name);
}

bool CTypeInfo::HasNamespacePrefix(void) const
{
    return m_InfoItem ? m_InfoItem->HasNamespacePrefix() : false;
}

const string& CTypeInfo::GetNamespacePrefix(void) const
{
    return m_InfoItem ? m_InfoItem->GetNamespacePrefix() : kEmptyStr;
}

void CTypeInfo::SetNamespacePrefix(const string& ns_prefix) const
{
    x_CreateInfoItemIfNeeded();
    m_InfoItem->SetNamespacePrefix(ns_prefix);
}

void CTypeInfo::x_CreateInfoItemIfNeeded(void) const
{
    if (!m_InfoItem) {
        m_InfoItem = new CNamespaceInfoItem;
    }
}

const string& CTypeInfo::GetModuleName(void) const
{
    return m_ModuleName;
}

void CTypeInfo::SetModuleName(const string& name)
{
    if ( !m_ModuleName.empty() )
        NCBI_THROW(CSerialException,eFail, "cannot change module name");
    m_ModuleName = name;
}

void CTypeInfo::SetModuleName(const char* name)
{
    SetModuleName(string(name));
}

void CTypeInfo::Delete(TObjectPtr /*object*/) const
{
    NCBI_THROW(CSerialException,eIllegalCall,
        "This type cannot be allocated on heap");
}

void CTypeInfo::DeleteExternalObjects(TObjectPtr /*object*/) const
{
}

TTypeInfo CTypeInfo::GetRealTypeInfo(TConstObjectPtr ) const
{
    return this;
}

bool CTypeInfo::IsType(TTypeInfo typeInfo) const
{
    return this == typeInfo;
}

bool CTypeInfo::MayContainType(TTypeInfo /*typeInfo*/) const
{
    return false;
}

const CObject* CTypeInfo::GetCObjectPtr(TConstObjectPtr /*objectPtr*/) const
{
    return 0;
}

bool CTypeInfo::IsParentClassOf(const CClassTypeInfo* /*classInfo*/) const
{
    return false;
}

void CTypeInfo::SetCreateFunction(TTypeCreate func)
{
    m_CreateFunction = func;
}

void CTypeInfo::SetLocalReadHook(CObjectIStream& stream,
                                 CReadObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetLocalHook(stream.m_ObjectHookKey, hook);
}

void CTypeInfo::SetGlobalReadHook(CReadObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetGlobalHook(hook);
}

void CTypeInfo::ResetLocalReadHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetLocalHook(stream.m_ObjectHookKey);
}

void CTypeInfo::ResetGlobalReadHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetGlobalHook();
}

void CTypeInfo::SetPathReadHook(CObjectIStream* in, const string& path,
                                CReadObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetPathHook(in,path,hook);
}

void CTypeInfo::SetGlobalWriteHook(CWriteObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetGlobalHook(hook);
}

void CTypeInfo::SetLocalWriteHook(CObjectOStream& stream,
                                 CWriteObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetLocalHook(stream.m_ObjectHookKey, hook);
}

void CTypeInfo::ResetGlobalWriteHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetGlobalHook();
}

void CTypeInfo::ResetLocalWriteHook(CObjectOStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetLocalHook(stream.m_ObjectHookKey);
}

void CTypeInfo::SetPathWriteHook(CObjectOStream* out, const string& path,
                                 CWriteObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetPathHook(out,path,hook);
}

void CTypeInfo::SetGlobalSkipHook(CSkipObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetGlobalHook(hook);
}

void CTypeInfo::SetLocalSkipHook(CObjectIStream& stream,
                                 CSkipObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetLocalHook(stream.m_ObjectSkipHookKey, hook);
}

void CTypeInfo::ResetGlobalSkipHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetGlobalHook();
}

void CTypeInfo::ResetLocalSkipHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetLocalHook(stream.m_ObjectSkipHookKey);
}

void CTypeInfo::SetPathSkipHook(CObjectIStream* in, const string& path,
                                CSkipObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetPathHook(in,path,hook);
}

void CTypeInfo::SetGlobalCopyHook(CCopyObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetGlobalHook(hook);
}

void CTypeInfo::SetLocalCopyHook(CObjectStreamCopier& stream,
                                 CCopyObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetLocalHook(stream.m_ObjectHookKey, hook);
}

void CTypeInfo::ResetGlobalCopyHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetGlobalHook();
}

void CTypeInfo::ResetLocalCopyHook(CObjectStreamCopier& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetLocalHook(stream.m_ObjectHookKey);
}

void CTypeInfo::SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                                CCopyObjectHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetPathHook(copier ? &(copier->In()) : 0,path,hook);
}

void CTypeInfo::SetReadFunction(TTypeReadFunction func)
{
    m_ReadHookData.SetDefaultFunction(func);
}

void CTypeInfo::SetWriteFunction(TTypeWriteFunction func)
{
    m_WriteHookData.SetDefaultFunction(func);
}

void CTypeInfo::SetCopyFunction(TTypeCopyFunction func)
{
    m_CopyHookData.SetDefaultFunction(func);
}

void CTypeInfo::SetSkipFunction(TTypeSkipFunction func)
{
    m_SkipHookData.SetDefaultFunction(func);
}

void CTypeInfoFunctions::ReadWithHook(CObjectIStream& stream,
                                      TTypeInfo objectType,
                                      TObjectPtr objectPtr)
{
    CReadObjectHook* hook =
        objectType->m_ReadHookData.GetHook(stream.m_ObjectHookKey);
    if (!hook) {
        hook = objectType->m_ReadHookData.GetPathHook(stream);
    }
    if ( hook )
        hook->ReadObject(stream, CObjectInfo(objectPtr, objectType));
    else
        objectType->DefaultReadData(stream, objectPtr);
}

void CTypeInfoFunctions::WriteWithHook(CObjectOStream& stream,
                                       TTypeInfo objectType,
                                       TConstObjectPtr objectPtr)
{
    CWriteObjectHook* hook =
        objectType->m_WriteHookData.GetHook(stream.m_ObjectHookKey);
    if (!hook) {
        hook = objectType->m_WriteHookData.GetPathHook(stream);
    }
    if ( hook )
        hook->WriteObject(stream, CConstObjectInfo(objectPtr, objectType));
    else
        objectType->DefaultWriteData(stream, objectPtr);
}

void CTypeInfoFunctions::SkipWithHook(CObjectIStream& stream,
                                      TTypeInfo objectType)
{
    CSkipObjectHook* hook =
        objectType->m_SkipHookData.GetHook(stream.m_ObjectSkipHookKey);
    if (!hook) {
        hook = objectType->m_SkipHookData.GetPathHook(stream);
    }
    if ( hook )
        hook->SkipObject(stream, objectType);
    else
        objectType->DefaultSkipData(stream);
}

void CTypeInfoFunctions::CopyWithHook(CObjectStreamCopier& stream,
                                      TTypeInfo objectType)
{
    CCopyObjectHook* hook =
        objectType->m_CopyHookData.GetHook(stream.m_ObjectHookKey);
    if (!hook) {
        hook = objectType->m_CopyHookData.GetPathHook(stream.In());
    }
    if ( hook )
        hook->CopyObject(stream, objectType);
    else
        objectType->DefaultCopyData(stream);
}


CNamespaceInfoItem::CNamespaceInfoItem(void)
{
    m_NsPrefixSet = false;
}

CNamespaceInfoItem::CNamespaceInfoItem(const CNamespaceInfoItem& other)
{
    m_NsName      = other.m_NsName;
    m_NsPrefix    = other.m_NsPrefix;
    m_NsPrefixSet = other.m_NsPrefixSet;
}

CNamespaceInfoItem::~CNamespaceInfoItem(void)
{
}

bool CNamespaceInfoItem::HasNamespaceName(void) const
{
    return !m_NsName.empty();
}

const string& CNamespaceInfoItem::GetNamespaceName(void) const
{
    return m_NsName;
}

void CNamespaceInfoItem::SetNamespaceName(const string& ns_name)
{
    m_NsName = ns_name;
}

bool CNamespaceInfoItem::HasNamespacePrefix(void) const
{
    return m_NsPrefixSet;
}

const string& CNamespaceInfoItem::GetNamespacePrefix(void) const
{
    return m_NsPrefix;
}

void CNamespaceInfoItem::SetNamespacePrefix(const string& ns_prefix)
{
    m_NsPrefix = ns_prefix;
    m_NsPrefixSet = !m_NsPrefix.empty();
}

END_NCBI_SCOPE
