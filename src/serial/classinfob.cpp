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

BEGIN_NCBI_SCOPE

static
string ClassName(const type_info& id)
{
    const char* s = id.name();
    if ( memcmp(s, "class ", 6) == 0 ) {
        // MSVC
        return s + 6;
    }
    else if ( isdigit(*s ) ) {
        // GCC
        while ( isdigit(*s) )
            ++s;
        return s;
    }
    else {
        return s;
    }
}

CClassTypeInfoBase::CClassTypeInfoBase(const type_info& id, size_t size)
    : CParent(ClassName(id))
{
    Init(id, size);
}

CClassTypeInfoBase::~CClassTypeInfoBase(void)
{
    Deregister();
}

void CClassTypeInfoBase::Init(const type_info& id, size_t size)
{
    m_Size = size;
    m_Id = &id;
    m_CreateFunction = 0;
    m_PostReadFunction = 0;
    m_PreWriteFunction = 0;
    m_IsCObject = false;
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

TObjectPtr CClassTypeInfoBase::Create(void) const
{
    if ( m_CreateFunction ) {
        TObjectPtr object = m_CreateFunction(this);
        if ( m_IsCObject )
            static_cast<CObject*>(object)->SetCanDelete();
        return object;
    }
    return CParent::Create();
}

size_t CClassTypeInfoBase::GetSize(void) const
{
    return m_Size;
}

void CClassTypeInfoBase::SetCreateFunction(TCreateFunction func)
{
    _ASSERT(m_CreateFunction == 0);
    _ASSERT(func != 0);
    m_CreateFunction = func;
}

void CClassTypeInfoBase::SetPostReadFunction(TPostReadFunction func)
{
    _ASSERT(m_PostReadFunction == 0);
    _ASSERT(func != 0);
    m_PostReadFunction = func;
}

void CClassTypeInfoBase::SetPreWriteFunction(TPreWriteFunction func)
{
    _ASSERT(m_PreWriteFunction == 0);
    _ASSERT(func != 0);
    m_PreWriteFunction = func;
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

void CClassTypeInfoBase::SetIsCObject(void)
{
    m_IsCObject = true;
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
    for ( TMemberIndex i = GetMembers().FirstMemberIndex(),
              last = GetMembers().LastMemberIndex();
          i <= last; ++i ) {
        if ( GetMemberInfo(i)->GetTypeInfo()->IsOrMayContainType(typeInfo) ) {
            return true;
        }
    }
    return false;
}

bool CClassTypeInfoBase::IsOrMayContainType(TTypeInfo typeInfo) const
{
    return IsType(typeInfo) || MayContainType(typeInfo);
}

END_NCBI_SCOPE
