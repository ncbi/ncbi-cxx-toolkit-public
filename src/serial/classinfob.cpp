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
* Revision 1.21  2005/02/24 14:39:04  gouriano
* Added PreRead/PostWrite hooks
*
* Revision 1.20  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.19  2003/03/11 20:08:07  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2003/03/10 18:54:24  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.17  2002/11/14 20:56:46  gouriano
* added AddMember method
*
* Revision 1.16  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.15  2002/09/13 15:38:42  dicuccio
* Fixed memory leak (need to clear a static prior to termination)
*
* Revision 1.14  2002/08/30 16:21:32  vasilche
* Added MT lock for cache maps
*
* Revision 1.13  2001/10/22 15:16:22  grichenk
* Optimized CTypeInfo::IsCObject()
*
* Revision 1.12  2001/05/17 15:07:05  lavr
* Typos corrected
*
* Revision 1.11  2000/11/01 20:38:37  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.10  2000/10/20 15:51:38  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.9  2000/10/13 20:22:54  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.8  2000/10/13 16:28:38  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.7  2000/10/03 17:22:42  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.6  2000/09/29 16:18:22  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.5  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.4  2000/09/18 20:00:21  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.3  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.2  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/06/16 16:31:18  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/classinfob.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objhook.hpp>

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
                if ( info->GetId() != typeid(void) ) {
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
                    if ( !classes->insert(
                        TClassesByName::value_type(info->GetName(),
                                                   info)).second ) {
                        NCBI_THROW(CSerialException,eInvalidData,
                            string("duplicate class name: ")+info->GetName());
                    }
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
    TClassesByName::iterator i = classes.find(name);
    if ( i == classes.end() ) {
        string msg("class not found: ");
        msg += name;
        NCBI_THROW(CSerialException,eInvalidData, msg);
    }
    return i->second;
}

const CObject* CClassTypeInfoBase::GetCObjectPtr(TConstObjectPtr objectPtr) const
{
    if ( IsCObject() ) {
        return static_cast<const CObject*>(objectPtr);
    }
    return 0;
}

bool CClassTypeInfoBase::MayContainType(TTypeInfo typeInfo) const
{
    TContainedTypes* cache = m_ContainedTypes.get();
    if ( cache ) {
        TContainedTypes::const_iterator found = cache->find(typeInfo);
        if ( found != cache->end() )
            return found->second;
    }
    else {
        m_ContainedTypes.reset(cache = new TContainedTypes);
    }
    bool& contains = (*cache)[typeInfo];
    // initialize by false to avoid recursion
    contains = false;
    // check parent
    return contains = CalcMayContainType(typeInfo);
}

bool CClassTypeInfoBase::CalcMayContainType(TTypeInfo typeInfo) const
{
    // check members
    for ( TMemberIndex i = GetItems().FirstIndex(),
              last = GetItems().LastIndex(); i <= last; ++i ) {
        if ( GetItems().GetItemInfo(i)->GetTypeInfo()->IsOrMayContainType(typeInfo) ) {
            return true;
        }
    }
    return false;
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
