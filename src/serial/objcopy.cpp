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
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>

#include <serial/objcopy.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objectinfo.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/impl/objistrimpl.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/serialimpl.hpp>

#undef _TRACE
#define _TRACE(arg) ((void)0)

BEGIN_NCBI_SCOPE

CObjectStreamCopier::CObjectStreamCopier(CObjectIStream& in,
                                         CObjectOStream& out)
    : m_In(in), m_Out(out)
{
}

CObjectStreamCopier::~CObjectStreamCopier(void)
{
    ResetLocalHooks();
}

void CObjectStreamCopier::ResetLocalHooks(void)
{
    XSERIAL_TYPEINFO_WRITELOCK;
    m_ObjectHookKey.Clear();
    m_ClassMemberHookKey.Clear();
    m_ChoiceVariantHookKey.Clear();
}

void CObjectStreamCopier::Copy(const CObjectTypeInfo& objectType)
{
    TTypeInfo type = objectType.GetTypeInfo();
    // root object
    BEGIN_OBJECT_2FRAMES2(eFrameNamed, type);
    In().SkipFileHeader(type);
    Out().WriteFileHeader(type);

    CopyObject(type);

    Separator(Out());

    Out().EndOfWrite();
    In().EndOfRead();
    END_OBJECT_2FRAMES();
}

void CObjectStreamCopier::Copy(TTypeInfo type, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_2FRAMES2(eFrameNamed, type);
    Out().WriteFileHeader(type);

    CopyObject(type);
    
    Separator(Out());

    Out().EndOfWrite();
    In().EndOfRead();
    END_OBJECT_2FRAMES();
}

void CObjectStreamCopier::CopyPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::CopyPointer("<<declaredType->GetName()<<")");
    CObjectIStream::EPointerType ptype = In().ReadPointerType();
    if ( ptype != CObjectIStream::eNullPointer && !In().DetectLoops() ) {
        CopyObject(declaredType);
        return;
    }
    TTypeInfo typeInfo;
    switch ( ptype ) {
    case CObjectIStream::eNullPointer:
        _TRACE("CObjectIStream::CopyPointer: null");
        Out().WriteNullPointer();
        return;
    case CObjectIStream::eObjectPointer:
        {
            _TRACE("CObjectIStream::CopyPointer: @...");
            CObjectIStream::TObjectIndex index = In().ReadObjectPointer();
            _TRACE("CObjectIStream::CopyPointer: @" << index);
            typeInfo = In().GetRegisteredObject(index).GetTypeInfo();
            Out().WriteObjectReference(index);
            break;
        }
    case CObjectIStream::eThisPointer:
        {
            _TRACE("CObjectIStream::CopyPointer: new");
            In().RegisterObject(declaredType);
            Out().RegisterObject(declaredType);
            CopyObject(declaredType);
            return;
        }
    case CObjectIStream::eOtherPointer:
        {
            _TRACE("CObjectIStream::CopyPointer: new...");
            string className = In().ReadOtherPointer();
            _TRACE("CObjectIStream::CopyPointer: new " << className);
            typeInfo = CClassTypeInfoBase::GetClassInfoByName(className);

            BEGIN_OBJECT_2FRAMES2(eFrameNamed, typeInfo);

            In().RegisterObject(typeInfo);
            Out().RegisterObject(typeInfo);

            Out().WriteOtherBegin(typeInfo);

            CopyObject(typeInfo);

            Out().WriteOtherEnd(typeInfo);
                
            END_OBJECT_2FRAMES();

            In().ReadOtherPointerEnd();
            break;
        }
    default:
        ThrowError(CObjectIStream::fFormatError,"illegal pointer type");
        return;
    }
    while ( typeInfo != declaredType ) {
        // try to check parent class pointer
        if ( typeInfo->GetTypeFamily() != eTypeFamilyClass ) {
            ThrowError(CObjectIStream::fFormatError,"incompatible member type");
        }
        const CClassTypeInfo* parentClass =
            CTypeConverter<CClassTypeInfo>::SafeCast(typeInfo)->GetParentClassInfo();
        if ( parentClass ) {
            typeInfo = parentClass;
        }
        else {
            ThrowError(CObjectIStream::fFormatError,"incompatible member type");
        }
    }
    return;
}

bool CObjectStreamCopier::CopyNullPointer(void)
{
    CObjectIStream::EPointerType ptype = In().ReadPointerType();
    if ( ptype == CObjectIStream::eNullPointer ) {
        Out().WriteNullPointer();
        return true;
    }
    return false;
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
        ob.End();
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
            ob.End();
        }
    }
    ib.End();
}

void CObjectStreamCopier::SetPathCopyObjectHook(const string& path,
                                               CCopyObjectHook*   hook)
{
    m_PathCopyObjectHooks.SetHook(path,hook);
    In().WatchPathHooks();
    Out().WatchPathHooks();
}
void CObjectStreamCopier::SetPathCopyMemberHook(const string& path,
                                                 CCopyClassMemberHook*   hook)
{
    m_PathCopyMemberHooks.SetHook(path,hook);
    In().WatchPathHooks();
    Out().WatchPathHooks();
}
void CObjectStreamCopier::SetPathCopyVariantHook(const string& path,
                                                 CCopyChoiceVariantHook* hook)
{
    m_PathCopyVariantHooks.SetHook(path,hook);
    In().WatchPathHooks();
    Out().WatchPathHooks();
}

void CObjectStreamCopier::SetPathHooks(CObjectStack& stk, bool set)
{
    if (!m_PathCopyObjectHooks.IsEmpty()) {
        CCopyObjectHook* hook = m_PathCopyObjectHooks.GetHook(stk);
        if (hook) {
            CTypeInfo* item = m_PathCopyObjectHooks.FindType(stk);
            if (item) {
                item->SetPathCopyHook(this,stk.GetStackPath(),set ? hook : NULL);
            }
        }
    }
    if (!m_PathCopyMemberHooks.IsEmpty()) {
        CCopyClassMemberHook* hook = m_PathCopyMemberHooks.GetHook(stk);
        if (hook) {
            CMemberInfo* item = m_PathCopyMemberHooks.FindItem(stk);
            if (item) {
                item->SetPathCopyHook(this,stk.GetStackPath(),set ? hook : NULL);
            }
        }
    }
    if (!m_PathCopyVariantHooks.IsEmpty()) {
        CCopyChoiceVariantHook* hook = m_PathCopyVariantHooks.GetHook(stk);
        if (hook) {
            CVariantInfo* item = m_PathCopyVariantHooks.FindItem(stk);
            if (item) {
                item->SetPathCopyHook(this,stk.GetStackPath(),set ? hook : NULL);
            }
        }
    }
}

END_NCBI_SCOPE
