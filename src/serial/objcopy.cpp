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
* Revision 1.1  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objcopy.hpp>
#include <serial/typeinfo.hpp>
#include <serial/classinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/choice.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

void CObjectStreamCopier::Copy(TTypeInfo type)
{
    // root object
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, type);
    In().SkipTypeName(type);

    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, type);
    Out().WriteTypeName(type);

    CopyObject(type);
    
    Out().EndOfWrite();
    END_OBJECT_FRAME_OF(Out());

    In().EndOfRead();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::Copy(TTypeInfo type, ENoTypeName)
{
    // root object
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, type);

    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, type);
    Out().WriteTypeName(type);

    CopyObject(type);
    
    Out().EndOfWrite();
    END_OBJECT_FRAME_OF(Out());

    In().EndOfRead();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::CopyNamedType(TTypeInfo namedTypeInfo,
                                        TTypeInfo type)
{
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, namedTypeInfo);
    In().BeginNamedType(namedTypeInfo);
    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, namedTypeInfo);
    Out().BeginNamedType(namedTypeInfo);
    
    CopyObject(type);

    Out().EndNamedType();
    END_OBJECT_FRAME_OF(Out());
    In().EndNamedType();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::CopyPointer(TTypeInfo declaredType)
{
    try {
        _TRACE("CObjectIStream::ReadPointer("<<declaredType->GetName()<<")");
        TTypeInfo typeInfo;
        switch ( In().ReadPointerType() ) {
        case CObjectIStream::eNullPointer:
            _TRACE("CObjectIStream::ReadPointer: null");
            Out().WriteNullPointer();
            return;
        case CObjectIStream::eObjectPointer:
            {
                _TRACE("CObjectIStream::ReadPointer: @...");
                CObjectIStream::TObjectIndex index = In().ReadObjectPointer();
                _TRACE("CObjectIStream::ReadPointer: @" << index);
                typeInfo = In().GetRegisteredObject(index).GetTypeInfo();
                Out().WriteObjectReference(index);
                break;
            }
        case CObjectIStream::eThisPointer:
            {
                _TRACE("CObjectIStream::ReadPointer: new");
                In().RegisterObject(declaredType);
                Out().RegisterObject(declaredType);
                CopyObject(declaredType);
                In().ReadThisPointerEnd();
                return;
            }
        case CObjectIStream::eOtherPointer:
            {
                _TRACE("CObjectIStream::ReadPointer: new...");
                string className = In().ReadOtherPointer();
                _TRACE("CObjectIStream::ReadPointer: new " << className);
                typeInfo = CClassTypeInfoBase::GetClassInfoByName(className);

                BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, typeInfo);
                BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, typeInfo);
                
                In().RegisterObject(typeInfo);
                Out().RegisterObject(typeInfo);

                Out().WriteOtherBegin(typeInfo);

                CopyObject(typeInfo);

                Out().WriteOtherEnd(typeInfo);
                
                END_OBJECT_FRAME_OF(Out());
                END_OBJECT_FRAME_OF(In());

                In().ReadOtherPointerEnd();
                break;
            }
        default:
            In().SetFailFlags(CObjectIStream::eFormatError);
            THROW1_TRACE(runtime_error, "illegal pointer type");
        }
        while ( typeInfo != declaredType ) {
            // try to check parent class pointer
            if ( typeInfo->GetTypeFamily() != CTypeInfo::eTypeClass ) {
                In().SetFailFlags(CObjectIStream::eFormatError);
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
            _ASSERT(dynamic_cast<const CClassTypeInfo*>(typeInfo));
            const CClassTypeInfo* parentClass =
                static_cast<const CClassTypeInfo*>(typeInfo)->GetParentClassInfo();
            if ( parentClass ) {
                typeInfo = parentClass;
            }
            else {
                In().SetFailFlags(CObjectIStream::eFormatError);
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
        }
        return;
    }
    catch (...) {
        In().SetFailFlags(CObjectIStream::eFail);
        throw;
    }
}

void CObjectStreamCopier::CopyString(void)
{
    string s;
    In().ReadString(s);
    Out().WriteString(s);
}

void CObjectStreamCopier::CopyStringStore(void)
{
    string s;
    In().ReadStringStore(s);
    Out().WriteStringStore(s);
}

void CObjectStreamCopier::CopyByteBlock(void)
{
    CObjectIStream::ByteBlock ib(In());
    if ( ib.KnownLength() ) {
        size_t length = ib.GetExpectedLength();
        CObjectOStream::ByteBlock ob(Out(), length);
        char buffer[4096];
        size_t count;
        while ( (count = ib.Read(buffer, sizeof(buffer))) != 0 ) {
            ob.Write(buffer, count);
        }
    }
    else {
        // length is unknown -> copy via buffer
        vector<char> o;
        {
            // load data
            char buffer[4096];
            size_t count;
            while ( (count = ib.Read(buffer, sizeof(buffer))) != 0 ) {
                o.insert(o.end(), buffer, buffer + count);
            }
        }
        {
            // store data
            size_t length = o.size();
            CObjectOStream::ByteBlock ob(Out(), length);
            if ( length > 0 )
                ob.Write(&o.front(), length);
        }
    }
    ib.End();
}

void CObjectStreamCopier::CopyContainer(const CContainerTypeInfo* containerType)
{
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameArray, containerType);
    In().BeginContainer(containerType);
    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameArray, containerType);
    Out().BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameArrayElement, elementType);
    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameArrayElement, elementType);

    while ( In().BeginContainerElement(elementType) ) {
        Out().BeginContainerElement(elementType);

        CopyObject(elementType);

        Out().EndContainerElement();
        In().EndContainerElement();
    }

    END_OBJECT_FRAME_OF(Out());
    END_OBJECT_FRAME_OF(In());
    
    Out().EndContainer();
    END_OBJECT_FRAME_OF(Out());

    In().EndContainer();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::CopyClassMembersRandom(const CClassTypeInfo* classType)
{
    const CMembersInfo& members = classType->GetMembers();
    TMemberIndex lastIndex = members.LastMemberIndex();
    vector<bool> read(lastIndex + 1);
    
    BEGIN_OBJECT_FRAME_OF(In(), eFrameClassMember);
    BEGIN_OBJECT_FRAME_OF(Out(), eFrameClassMember);

    TMemberIndex index;
    while ( (index = In().BeginClassMember(members)) != kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        const CMemberId& id = memberInfo->GetId();
        In().TopFrame().SetMemberId(id);
        Out().TopFrame().SetMemberId(id);

        if ( read[index] ) {
            In().DuplicatedMember(memberInfo);
        }
        else {
            read[index] = true;
            Out().BeginClassMember(id);

            CopyObject(memberInfo->GetTypeInfo());

            Out().EndClassMember();
        }
        
        In().EndClassMember();
    }

    END_OBJECT_FRAME_OF(Out());
    END_OBJECT_FRAME_OF(In());

    // init all absent members
    for ( TMemberIndex i = members.FirstMemberIndex();
          i <= lastIndex; ++i ) {
        if ( !read[i] ) {
            const CMemberInfo* memberInfo = classType->GetMemberInfo(i);
            if ( !memberInfo->Optional() )
                In().ExpectedMember(memberInfo);
        }
    }
}

void CObjectStreamCopier::CopyClassMembersSequential(const CClassTypeInfo* classType)
{
    const CMembersInfo& members = classType->GetMembers();
    TMemberIndex lastIndex = members.LastMemberIndex();
    TMemberIndex pos = members.FirstMemberIndex();

    BEGIN_OBJECT_FRAME_OF(In(), eFrameClassMember);
    BEGIN_OBJECT_FRAME_OF(Out(), eFrameClassMember);

    TMemberIndex index;
    while ( (index = In().BeginClassMember(members, pos)) != kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        const CMemberId& id = memberInfo->GetId();
        In().TopFrame().SetMemberId(id);
        Out().TopFrame().SetMemberId(id);

        for ( TMemberIndex i = pos; i < index; ++i ) {
            // init missing member
            const CMemberInfo* memberInfo2 = classType->GetMemberInfo(i);
            if ( !memberInfo2->Optional() )
                In().ExpectedMember(memberInfo2);
        }
        Out().BeginClassMember(id);

        CopyObject(memberInfo->GetTypeInfo());
        
        Out().EndClassMember();
        In().EndClassMember();

        pos = index + 1;
    }

    END_OBJECT_FRAME_OF(Out());
    END_OBJECT_FRAME_OF(In());

    // init all absent members
    for ( TMemberIndex i = pos; i <= lastIndex; ++i ) {
        const CMemberInfo* memberInfo2 = classType->GetMemberInfo(i);
        if ( !memberInfo2->Optional() )
            In().ExpectedMember(memberInfo2);
    }
}

void CObjectStreamCopier::CopyClass(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameClass, classType);
    In().BeginClass(classType);
    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameClass, classType);
    Out().BeginClass(classType);

    if ( classType->RandomOrder() )
        CopyClassMembersRandom(classType);
    else
        CopyClassMembersSequential(classType);

    Out().EndClass();
    END_OBJECT_FRAME_OF(Out());
    In().EndClass();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::CopyChoice(const CChoiceTypeInfo* choiceType)
{
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameChoice, choiceType);
    TMemberIndex index = In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember )
        In().ThrowError(CObjectIStream::eFormatError,
                        "choice variant id expected");
    const CMemberInfo* variantInfo = choiceType->GetMemberInfo(index);
    const CMemberId& id = variantInfo->GetId();
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameChoiceVariant, id);

    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameChoice, choiceType);
    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameChoiceVariant, id);
    Out().BeginChoiceVariant(choiceType, id);

    CopyObject(variantInfo->GetTypeInfo());

    Out().EndChoiceVariant();
    END_OBJECT_FRAME_OF(Out());
    END_OBJECT_FRAME_OF(Out());

    In().EndChoiceVariant();
    END_OBJECT_FRAME_OF(In());
    END_OBJECT_FRAME_OF(In());
}

END_NCBI_SCOPE
