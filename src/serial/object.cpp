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
* Revision 1.4  2000/07/06 16:21:19  vasilche
* Added interface to primitive types in CObjectInfo & CConstObjectInfo.
*
* Revision 1.3  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.2  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.1  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/object.hpp>
#include <serial/typeinfo.hpp>
#include <serial/classinfob.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/stdtypes.hpp>

BEGIN_NCBI_SCOPE

const CPrimitiveTypeInfo* CObjectTypeInfo::GetPrimitiveTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CPrimitiveTypeInfo*>(GetTypeInfo()));
    return static_cast<const CPrimitiveTypeInfo*>(GetTypeInfo());
}

const CClassTypeInfoBase* CObjectTypeInfo::GetClassTypeInfoBase(void) const
{
    _ASSERT(dynamic_cast<const CClassTypeInfoBase*>(GetTypeInfo()));
    return static_cast<const CClassTypeInfoBase*>(GetTypeInfo());
}

const CClassTypeInfo* CObjectTypeInfo::GetClassTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CClassTypeInfo*>(GetTypeInfo()));
    return static_cast<const CClassTypeInfo*>(GetTypeInfo());
}

const CChoiceTypeInfo* CObjectTypeInfo::GetChoiceTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CChoiceTypeInfo*>(GetTypeInfo()));
    return static_cast<const CChoiceTypeInfo*>(GetTypeInfo());
}

const CContainerTypeInfo* CObjectTypeInfo::GetContainerTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CContainerTypeInfo*>(GetTypeInfo()));
    return static_cast<const CContainerTypeInfo*>(GetTypeInfo());
}

const CPointerTypeInfo* CObjectTypeInfo::GetPointerTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CPointerTypeInfo*>(GetTypeInfo()));
    return static_cast<const CPointerTypeInfo*>(GetTypeInfo());
}

// pointer interface
CConstObjectInfo CConstObjectInfo::GetPointedObject(void) const
{
    return GetPointerTypeInfo()->GetPointedObject(*this);
}

CObjectInfo CObjectInfo::GetPointedObject(void) const
{
    return GetPointerTypeInfo()->GetPointedObject(*this);
}

// class interface
CObjectTypeInfo::TMemberIndex CObjectTypeInfo::GetMembersCount(void) const
{
    return GetClassTypeInfoBase()->GetMembersCount();
}

const CMemberId& CObjectTypeInfo::GetMemberId(TMemberIndex index) const
{
    return GetClassTypeInfoBase()->GetMemberId(index);
}

CObjectTypeInfo::TMemberIndex
CObjectTypeInfo::FindMember(const string& name) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(name);
}

CObjectTypeInfo::TMemberIndex
CObjectTypeInfo::FindMemberByTag(int tag) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(tag);
}

void CObjectTypeInfo::CMemberIterator::Init(const CObjectTypeInfo& info)
{
    m_MemberIndex = m_MembersCount = 0;
    m_ClassTypeInfo = info.GetClassTypeInfo();
    m_MembersCount = GetClassTypeInfo()->GetMembersCount();
    _DEBUG_ARG(m_LastCall = eNone);
}

const CMemberId& CObjectTypeInfo::CMemberIterator::GetId(void) const
{
    _ASSERT(CheckValid());
    return GetClassTypeInfo()->GetMemberId(GetMemberIndex());
}

bool CConstObjectInfo::ClassMemberIsSet(TMemberIndex index) const
{
    const CMemberInfo* mInfo = GetClassTypeInfo()->GetMemberInfo(index);
    if ( mInfo->HaveSetFlag() )
        return mInfo->GetSetFlag(GetObjectPtr());
    
    if ( mInfo->CanBeDelayed() &&
         mInfo->GetDelayBuffer(GetObjectPtr()) )
        return true;

    if ( mInfo->Optional() ) {
        TConstObjectPtr defaultPtr = mInfo->GetDefault();
        TConstObjectPtr memberPtr = mInfo->GetMember(GetObjectPtr());
        TTypeInfo memberType = mInfo->GetTypeInfo();
        if ( !defaultPtr ) {
            if ( memberType->IsDefault(memberPtr) )
                return false; // DEFAULT
        }
        else {
            if ( memberType->Equals(memberPtr, defaultPtr) )
                return false; // OPTIONAL
        }
    }
    return true;
}

CConstObjectInfo CConstObjectInfo::GetClassMember(TMemberIndex index) const
{
    const CMemberInfo* mInfo = GetClassTypeInfo()->GetMemberInfo(index);
    if ( mInfo->CanBeDelayed() )
        const_cast<CDelayBuffer&>
            (mInfo->GetDelayBuffer(GetObjectPtr())).Update();
    return make_pair(mInfo->GetMember(GetObjectPtr()), mInfo->GetTypeInfo());
}

CObjectInfo CObjectInfo::GetClassMember(TMemberIndex index) const
{
    const CMemberInfo* mInfo = GetClassTypeInfo()->GetMemberInfo(index);
    if ( mInfo->CanBeDelayed() )
        mInfo->GetDelayBuffer(GetObjectPtr()).Update();
    return make_pair(mInfo->GetMember(GetObjectPtr()), mInfo->GetTypeInfo());
}

void CObjectInfo::EraseClassMember(TMemberIndex index)
{
    const CMemberInfo* mInfo = GetClassTypeInfo()->GetMemberInfo(index);
    if ( !mInfo->Optional() || mInfo->GetDefault() ) {
        THROW1_TRACE(runtime_error, "cannot erase non OPTIONAL member");
    }
    
    // check 'set' flag
    bool haveSetFlag = mInfo->HaveSetFlag();
    if ( haveSetFlag && !mInfo->GetSetFlag(GetObjectPtr()) ) {
        // member not set
        return;
    }

    // update delayed buffer
    if ( mInfo->CanBeDelayed() )
        mInfo->GetDelayBuffer(GetObjectPtr()).Update();

    // reset member
    mInfo->GetTypeInfo()->SetDefault(mInfo->GetMember(GetObjectPtr()));

    // update 'set' flag
    if ( haveSetFlag )
        mInfo->GetSetFlag(GetObjectPtr()) = false;
}

CObjectTypeInfo::TMemberIndex CConstObjectInfo::WhichChoice(void) const
{
    return GetChoiceTypeInfo()->GetIndex(GetObjectPtr());
}

CConstObjectInfo CConstObjectInfo::GetCurrentChoiceVariant(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = choiceInfo->GetIndex(GetObjectPtr());
    return make_pair(choiceInfo->GetData(GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

CObjectInfo CObjectInfo::GetCurrentChoiceVariant(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = choiceInfo->GetIndex(GetObjectPtr());
    return make_pair(choiceInfo->GetData(GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

CConstObjectElementIterator::CConstObjectElementIterator(const CConstObjectInfo& object)
    : m_Iterator(object.GetContainerTypeInfo()->
                 Elements(object.GetObjectPtr()))
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CConstObjectElementIterator&
CConstObjectElementIterator::operator=(const CConstObjectInfo& object)
{
    m_Iterator.reset(object.GetContainerTypeInfo()->
                     Elements(object.GetObjectPtr()));
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

CObjectElementIterator::CObjectElementIterator(const CObjectInfo& object)
    : m_Iterator(object.GetContainerTypeInfo()->
                 Elements(object.GetObjectPtr()))
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CObjectElementIterator&
CObjectElementIterator::operator=(const CObjectInfo& object)
{
    m_Iterator.reset(object.GetContainerTypeInfo()->
                     Elements(object.GetObjectPtr()));
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

END_NCBI_SCOPE
