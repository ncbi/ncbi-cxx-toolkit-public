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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/typeinfo.hpp>
#include <serial/impl/typeinfoimpl.hpp>
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

    ENsQualifiedMode IsNsQualified(void);
    void SetNsQualified(bool qualified);

private:
    string m_NsName;
    string m_NsPrefix;
    bool   m_NsPrefixSet;
    ENsQualifiedMode   m_NsQualified;
};

typedef CTypeInfoFunctions TFunc;

CTypeInfo::CTypeInfo(ETypeFamily typeFamily, size_t size)
    : m_TypeFamily(typeFamily), m_Size(size), m_Name(),
      m_InfoItem(0),
      m_IsCObject(false),
      m_IsInternal(false),
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
      m_IsInternal(false),
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
      m_IsInternal(false),
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

const CTypeInfo* CTypeInfo::SetNamespaceName(const string& ns_name) const
{
    x_CreateInfoItemIfNeeded();
    m_InfoItem->SetNamespaceName(ns_name);
    return this;
}

const CTypeInfo* CTypeInfo::SetNsQualified(bool qualified) const
{
    _ASSERT(m_InfoItem);
    m_InfoItem->SetNsQualified(qualified);
    return this;
}

ENsQualifiedMode CTypeInfo::IsNsQualified(void) const
{
    return m_InfoItem ? m_InfoItem->IsNsQualified() : eNSQNotSet;
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

const string& CTypeInfo::GetName(void) const
{
    return IsInternal()? kEmptyStr: m_Name;
}

const string& CTypeInfo::GetModuleName(void) const
{
    return IsInternal()? kEmptyStr: m_ModuleName;
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

const string& CTypeInfo::GetInternalName(void) const
{
    return !IsInternal()? kEmptyStr: m_Name;
}

const string& CTypeInfo::GetInternalModuleName(void) const
{
    return !IsInternal()? kEmptyStr: m_ModuleName;
}

void CTypeInfo::SetInternalName(const string& name)
{
    if ( IsInternal() || !m_Name.empty() || !m_ModuleName.empty() )
        NCBI_THROW(CSerialException,eFail, "cannot change (internal) name");
    m_IsInternal = true;
    m_Name = name;
}

const string& CTypeInfo::GetAccessName(void) const
{
    return m_Name;
}

const string& CTypeInfo::GetAccessModuleName(void) const
{
    return m_ModuleName;
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

CTypeInfo::EMayContainType
CTypeInfo::GetMayContainType(TTypeInfo /*typeInfo*/) const
{
    return eMayContainType_no;
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
    stream.AddMonitorType(this);
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
    m_NsQualified = eNSQNotSet;
}

CNamespaceInfoItem::CNamespaceInfoItem(const CNamespaceInfoItem& other)
{
    m_NsName      = other.m_NsName;
    m_NsPrefix    = other.m_NsPrefix;
    m_NsPrefixSet = other.m_NsPrefixSet;
    m_NsQualified = other.m_NsQualified;
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

ENsQualifiedMode CNamespaceInfoItem::IsNsQualified(void)
{
    return m_NsQualified;
}

void CNamespaceInfoItem::SetNsQualified(bool qualified)
{
    m_NsQualified = qualified ? eNSQualified : eNSUnqualified;
}

END_NCBI_SCOPE
