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
* Revision 1.7  2000/09/01 13:16:17  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.6  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.5  2000/07/27 14:59:29  thiessen
* minor fix to avoid Mac compiler error
*
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
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

// container iterators

CConstObjectInfoEI::CConstObjectInfoEI(const CConstObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CConstObjectInfoEI& CConstObjectInfoEI::operator=(const CConstObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

CObjectInfoEI::CObjectInfoEI(const CObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CObjectInfoEI& CObjectInfoEI::operator=(const CObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

// class iterators

bool CObjectTypeInfoMI::IsSet(const CConstObjectInfo& object) const
{
    const CMemberInfo* mInfo = GetMemberInfo();
    if ( mInfo->HaveSetFlag() )
        return mInfo->GetSetFlag(object.GetObjectPtr());
    
    if ( mInfo->CanBeDelayed() &&
         mInfo->GetDelayBuffer(object.GetObjectPtr()).Delayed() )
        return true;

    if ( mInfo->Optional() ) {
        TConstObjectPtr defaultPtr = mInfo->GetDefault();
        TConstObjectPtr memberPtr = mInfo->GetMember(object.GetObjectPtr());
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

CConstObjectInfo CConstObjectInfoMI::GetMember(void) const
{
    const CMemberInfo* mInfo = GetMemberInfo();
    return make_pair(mInfo->GetMember(m_Object.GetObjectPtr()),
                     mInfo->GetTypeInfo());
}

CConstObjectInfo CConstObjectInfoMI::operator*(void) const
{
    const CMemberInfo* mInfo = GetMemberInfo();
    return make_pair(mInfo->GetMember(m_Object.GetObjectPtr()),
                     mInfo->GetTypeInfo());
}

CObjectInfo CObjectInfoMI::GetMember(void) const
{
    const CMemberInfo* mInfo = GetMemberInfo();
    mInfo->UpdateSetFlag(mInfo->GetMember(m_Object.GetObjectPtr()), true);
    return make_pair(mInfo->GetMember(m_Object.GetObjectPtr()),
                     mInfo->GetTypeInfo());
}

CObjectInfo CObjectInfoMI::operator*(void) const
{
    const CMemberInfo* mInfo = GetMemberInfo();
    mInfo->UpdateSetFlag(mInfo->GetMember(m_Object.GetObjectPtr()), true);
    return make_pair(mInfo->GetMember(m_Object.GetObjectPtr()),
                     mInfo->GetTypeInfo());
}

void CObjectInfoMI::Erase(void)
{
    const CMemberInfo* mInfo = GetMemberInfo();
    if ( !mInfo->Optional() || mInfo->GetDefault() )
        THROW1_TRACE(runtime_error, "cannot reset non OPTIONAL member");
    
    TObjectPtr objectPtr = m_Object.GetObjectPtr();
    // check 'set' flag
    bool haveSetFlag = mInfo->HaveSetFlag();
    if ( haveSetFlag && !mInfo->GetSetFlag(objectPtr) ) {
        // member not set
        return;
    }

    // reset member
    mInfo->GetTypeInfo()->SetDefault(mInfo->GetMember(objectPtr));

    // update 'set' flag
    if ( haveSetFlag )
        mInfo->GetSetFlag(objectPtr) = false;
}

// choice iterators

CConstObjectInfo CConstObjectInfoCV::GetVariant(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = GetVariantIndex();
    return make_pair(choiceInfo->GetData(m_Object.GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

CConstObjectInfo CConstObjectInfoCV::operator*(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = GetVariantIndex();
    return make_pair(choiceInfo->GetData(m_Object.GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

CObjectInfo CObjectInfoCV::GetVariant(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = GetVariantIndex();
    return make_pair(choiceInfo->GetData(m_Object.GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

CObjectInfo CObjectInfoCV::operator*(void) const
{
    const CChoiceTypeInfo* choiceInfo = GetChoiceTypeInfo();
    TMemberIndex index = GetVariantIndex();
    return make_pair(choiceInfo->GetData(m_Object.GetObjectPtr(), index),
                     choiceInfo->GetMemberInfo(index)->GetTypeInfo());
}

END_NCBI_SCOPE
