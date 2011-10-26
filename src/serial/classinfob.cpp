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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/impl/classinfob.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objhook.hpp>
#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE

DEFINE_STATIC_MUTEX(s_ClassInfoMutex);

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name)
{
    m_IsCObject = true;
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name)
{
    m_IsCObject = true;
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::~CClassTypeInfoBase(void)
{
    Deregister();
}

CMemberInfo* CClassTypeInfoBase::AddMember(const char* memberId,
                                           const void* memberPtr,
                                           const CTypeRef& memberType)
{
    CMemberInfo* memberInfo = new CMemberInfo(this, memberId,
                                              TPointerOffsetType(memberPtr),
                                              memberType);
    GetItems().AddItem(memberInfo);
    return memberInfo;
}

CMemberInfo* CClassTypeInfoBase::AddMember(const CMemberId& memberId,
                                           const void* memberPtr,
                                           const CTypeRef& memberType)
{
    CMemberInfo* memberInfo = new CMemberInfo(this, memberId,
                                              TPointerOffsetType(memberPtr),
                                              memberType);
    GetItems().AddItem(memberInfo);
    return memberInfo;
}

void CClassTypeInfoBase::InitClassTypeInfoBase(const type_info& id)
{
    m_Id = &id;
    Register();
}

CClassTypeInfoBase::TClasses* CClassTypeInfoBase::sm_Classes = 0;
CClassTypeInfoBase::TClassesById* CClassTypeInfoBase::sm_ClassesById = 0;
CClassTypeInfoBase::TClassesByName* CClassTypeInfoBase::sm_ClassesByName = 0;
set<string>* CClassTypeInfoBase::sm_Modules = 0;

inline
CClassTypeInfoBase::TClasses& CClassTypeInfoBase::Classes(void)
{
    TClasses* classes = sm_Classes;
    if ( !classes ) {
        CMutexGuard GUARD(s_ClassInfoMutex);
        classes = sm_Classes;
        if ( !classes ) {
            classes = sm_Classes = new TClasses;
        }
    }
    return *classes;
}

inline
CClassTypeInfoBase::TClassesById& CClassTypeInfoBase::ClassesById(void)
{
    TClassesById* classes = sm_ClassesById;
    if ( !classes ) {
        CMutexGuard GUARD(s_ClassInfoMutex);
        classes = sm_ClassesById;
        if ( !classes ) {
            const TClasses& cc = Classes();
            auto_ptr<TClassesById> keep(classes = new TClassesById);
            ITERATE ( TClasses, i , cc ) {
                const CClassTypeInfoBase* info = *i;
                if ( info->GetId() != typeid(bool) ) {
                    if ( !classes->insert(
                        TClassesById::value_type(&info->GetId(),
                                                 info)).second ) {
                        NCBI_THROW(CSerialException,eInvalidData,
                                   string("duplicate class id: ")+
                                   info->GetId().name());
                    }
                }
            }
            sm_ClassesById = keep.release();
        }
    }
    return *classes;
}

inline
CClassTypeInfoBase::TClassesByName& CClassTypeInfoBase::ClassesByName(void)
{
    TClassesByName* classes = sm_ClassesByName;
    if ( !classes ) {
        CMutexGuard GUARD(s_ClassInfoMutex);
        classes = sm_ClassesByName;
        if ( !classes ) {
            auto_ptr<TClassesByName> keep(classes = new TClassesByName);
            const TClasses& cc = Classes();
            ITERATE ( TClasses, i, cc ) {
                const CClassTypeInfoBase* info = *i;
                if ( !info->GetName().empty() ) {
                    classes->insert
                        (TClassesByName::value_type(info->GetName(), info));
                }
            }
            sm_ClassesByName = keep.release();
        }
    }
    return *classes;
}

void CClassTypeInfoBase::Register(void)
{
    CMutexGuard GUARD(s_ClassInfoMutex);
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().insert(this);
}

void CClassTypeInfoBase::Deregister(void)
{
    CMutexGuard GUARD(s_ClassInfoMutex);
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().erase(this);
    if (Classes().size() == 0) {
        delete sm_Classes;
        sm_Classes = 0;
    }
}

TTypeInfo CClassTypeInfoBase::GetClassInfoById(const type_info& id)
{
    TClassesById& types = ClassesById();
    TClassesById::iterator i = types.find(&id);
    if ( i == types.end() ) {
        string msg("class not found: ");
        msg += id.name();
        NCBI_THROW(CSerialException,eInvalidData, msg);
    }
    return i->second;
}

TTypeInfo CClassTypeInfoBase::GetClassInfoByName(const string& name)
{
    TClassesByName& classes = ClassesByName();
    pair<TClassesByName::iterator, TClassesByName::iterator> i =
        classes.equal_range(name);
    if (  i.first == i.second ) {
        NCBI_THROW_FMT(CSerialException, eInvalidData,
                       "class not found: "<<name);
    }
    if ( --i.second != i.first ) {
        // multiple types with the same name
        const CClassTypeInfoBase* t1 = i.first->second;
        const CClassTypeInfoBase* t2 = i.second->second;
        NCBI_THROW_FMT
            (CSerialException, eInvalidData,
             "ambiguous class name: "<<t1->GetName()<<
             " ("<<t1->GetModuleName()<<"&"<<t2->GetModuleName()<<")");
    }
    return i.first->second;
}

void CClassTypeInfoBase::GetRegisteredModuleNames(
    CClassTypeInfoBase::TRegModules& modules)
{
    modules.clear();
    CMutexGuard GUARD(s_ClassInfoMutex);
    if (sm_Modules) {
        modules.insert(sm_Modules->begin(), sm_Modules->end());
    }
}

void CClassTypeInfoBase::GetRegisteredClassNames(
    const string& module, CClassTypeInfoBase::TRegClasses& names)
{
    names.clear();
    CMutexGuard GUARD(s_ClassInfoMutex);
    TClasses& cc = Classes();
    ITERATE ( TClasses, i , cc ) {
        const CClassTypeInfoBase* info = *i;
        if (info->GetModuleName() == module) {
            names.insert( info->GetName());
        }
    }
}

void CClassTypeInfoBase::RegisterModule(const string& module)
{
    CMutexGuard GUARD(s_ClassInfoMutex);
    if (!sm_Modules) {
        sm_Modules = new set<string>;
    }
    sm_Modules->insert(module);
}

const CObject* CClassTypeInfoBase::GetCObjectPtr(TConstObjectPtr objectPtr) const
{
    if ( IsCObject() ) {
        return static_cast<const CObject*>(objectPtr);
    }
    return 0;
}

CTypeInfo::EMayContainType
CClassTypeInfoBase::GetMayContainType(TTypeInfo typeInfo) const
{
    CMutexGuard GUARD(GetTypeInfoMutex());
    TContainedTypes* cache = m_ContainedTypes.get();
    if ( !cache ) {
        m_ContainedTypes.reset(cache = new TContainedTypes);
    }
    pair<TContainedTypes::iterator, bool> ins =
        cache->insert(TContainedTypes::value_type(typeInfo,
                                                  eMayContainType_recursion));
    if ( !ins.second ) {
        return ins.first->second;
    }

    static int recursion_level = 0;
    ++recursion_level;
    EMayContainType ret;
    try {
        ret = CalcMayContainType(typeInfo);
        --recursion_level;
        if ( ret == eMayContainType_recursion ) {
            if ( recursion_level == 0 ) {
                ins.first->second = ret = eMayContainType_no;
            }
            else {
                cache->erase(ins.first);
            }
        }
        else {
            ins.first->second = ret;
        }
    }
    catch ( ... ) {
        --recursion_level;
        cache->erase(ins.first);
        throw;
    }
    return ret;
}

CTypeInfo::EMayContainType
CClassTypeInfoBase::CalcMayContainType(TTypeInfo typeInfo) const
{
    EMayContainType ret = eMayContainType_no;
    // check members
    for ( TMemberIndex i = GetItems().FirstIndex(),
              last = GetItems().LastIndex(); i <= last; ++i ) {
        EMayContainType contains = GetItems().GetItemInfo(i)->GetTypeInfo()->
            IsOrMayContainType(typeInfo);
        if ( contains == eMayContainType_yes ) {
            return contains;
        }
        if ( contains == eMayContainType_recursion ) {
            ret = contains;
        }
    }
    return ret;
}

class CPreReadHook : public CReadObjectHook
{
    typedef CReadObjectHook CParent;
public:
    typedef CClassTypeInfoBase::TPreReadFunction TPreReadFunction;

    CPreReadHook(TPreReadFunction func);

    void ReadObject(CObjectIStream& in, const CObjectInfo& object);

private:
    TPreReadFunction m_PreRead;
};

CPreReadHook::CPreReadHook(TPreReadFunction func)
    : m_PreRead(func)
{
}

void CPreReadHook::ReadObject(CObjectIStream& in,
                               const CObjectInfo& object)
{
    m_PreRead(object.GetTypeInfo(), object.GetObjectPtr());
    object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
}

class CPostReadHook : public CReadObjectHook
{
    typedef CReadObjectHook CParent;
public:
    typedef CClassTypeInfoBase::TPostReadFunction TPostReadFunction;

    CPostReadHook(TPostReadFunction func);

    void ReadObject(CObjectIStream& in, const CObjectInfo& object);

private:
    TPostReadFunction m_PostRead;
};

CPostReadHook::CPostReadHook(TPostReadFunction func)
    : m_PostRead(func)
{
}

void CPostReadHook::ReadObject(CObjectIStream& in,
                               const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
    m_PostRead(object.GetTypeInfo(), object.GetObjectPtr());
}

class CPreWriteHook : public CWriteObjectHook
{
    typedef CWriteObjectHook CParent;
public:
    typedef CClassTypeInfoBase::TPreWriteFunction TPreWriteFunction;

    CPreWriteHook(TPreWriteFunction func);

    void WriteObject(CObjectOStream& out, const CConstObjectInfo& object);

private:
    TPreWriteFunction m_PreWrite;
};

CPreWriteHook::CPreWriteHook(TPreWriteFunction func)
    : m_PreWrite(func)
{
}

void CPreWriteHook::WriteObject(CObjectOStream& out,
                                const CConstObjectInfo& object)
{
    m_PreWrite(object.GetTypeInfo(), object.GetObjectPtr());
    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
}

class CPostWriteHook : public CWriteObjectHook
{
    typedef CWriteObjectHook CParent;
public:
    typedef CClassTypeInfoBase::TPostWriteFunction TPostWriteFunction;

    CPostWriteHook(TPostWriteFunction func);

    void WriteObject(CObjectOStream& out, const CConstObjectInfo& object);

private:
    TPostWriteFunction m_PostWrite;
};

CPostWriteHook::CPostWriteHook(TPostWriteFunction func)
    : m_PostWrite(func)
{
}

void CPostWriteHook::WriteObject(CObjectOStream& out,
                                const CConstObjectInfo& object)
{
    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
    m_PostWrite(object.GetTypeInfo(), object.GetObjectPtr());
}

void CClassTypeInfoBase::SetPreReadFunction(TPreReadFunction func)
{
    SetGlobalReadHook(new CPreReadHook(func));
}

void CClassTypeInfoBase::SetPostReadFunction(TPostReadFunction func)
{
    SetGlobalReadHook(new CPostReadHook(func));
}

void CClassTypeInfoBase::SetPreWriteFunction(TPreWriteFunction func)
{
    SetGlobalWriteHook(new CPreWriteHook(func));
}

void CClassTypeInfoBase::SetPostWriteFunction(TPostWriteFunction func)
{
    SetGlobalWriteHook(new CPostWriteHook(func));
}

END_NCBI_SCOPE
