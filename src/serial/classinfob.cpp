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
* Revision 1.10  2000/10/20 15:51:38  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
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

#include <corelib/ncbistd.hpp>
#include <serial/classinfob.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objhook.hpp>

BEGIN_NCBI_SCOPE

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(false)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(true)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(false)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(true)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

CClassTypeInfoBase::~CClassTypeInfoBase(void)
{
    Deregister();
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
    if ( !classes )
        classes = sm_Classes = new TClasses;
    return *classes;
}

inline
CClassTypeInfoBase::TClassesById& CClassTypeInfoBase::ClassesById(void)
{
    TClassesById* classes = sm_ClassesById;
    if ( !classes ) {
        classes = sm_ClassesById = new TClassesById;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassTypeInfoBase* info = *i;
            if ( info->GetId() != typeid(void) ) {
                _TRACE("class by id: " << info->GetId().name()
                       << " : " << info->GetName());
                if ( !classes->insert(
                         TClassesById::value_type(&info->GetId(),
                                                  info)).second ) {
                    THROW1_TRACE(runtime_error, "duplicated class ids");
                }
            }
        }
    }
    return *classes;
}

inline
CClassTypeInfoBase::TClassesByName& CClassTypeInfoBase::ClassesByName(void)
{
    TClassesByName* classes = sm_ClassesByName;
    if ( !classes ) {
        classes = sm_ClassesByName = new TClassesByName;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassTypeInfoBase* info = *i;
            if ( !info->GetName().empty() ) {
                _TRACE("class by name: " << " : " <<
                       info->GetName() << info->GetId().name());
                if ( !classes->insert(
                         TClassesByName::value_type(info->GetName(),
                                                    info)).second ) {
                    THROW1_TRACE(runtime_error, "duplicated class names");
                }
            }
        }
    }
    return *classes;
}

void CClassTypeInfoBase::Register(void)
{
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().insert(this);
}

void CClassTypeInfoBase::Deregister(void)
{
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().erase(this);
}

TTypeInfo CClassTypeInfoBase::GetClassInfoById(const type_info& id)
{
    TClassesById& types = ClassesById();
    TClassesById::iterator i = types.find(&id);
    if ( i == types.end() ) {
        string msg("class not found: ");
        msg += id.name();
        THROW1_TRACE(runtime_error, msg);
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
        THROW1_TRACE(runtime_error, msg);
    }
    return i->second;
}

bool CClassTypeInfoBase::IsCObject(void) const
{
    return m_IsCObject;
}

const CObject* CClassTypeInfoBase::GetCObjectPtr(TConstObjectPtr objectPtr) const
{
    if ( m_IsCObject )
        return static_cast<const CObject*>(objectPtr);
    else
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
    : CParent(eCanDelete), m_PostRead(func)
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
    : CParent(eCanDelete), m_PreWrite(func)
{
}

void CPreWriteHook::WriteObject(CObjectOStream& out,
                                const CConstObjectInfo& object)
{
    m_PreWrite(object.GetTypeInfo(), object.GetObjectPtr());
    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
}

void CClassTypeInfoBase::SetPostReadFunction(TPostReadFunction func)
{
    SetGlobalReadHook(new CPostReadHook(func));
}

void CClassTypeInfoBase::SetPreWriteFunction(TPreWriteFunction func)
{
    SetGlobalWriteHook(new CPreWriteHook(func));
}

END_NCBI_SCOPE
