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
#include <serial/classinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/choice.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objistrimpl.hpp>
#include <serial/objlist.hpp>
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
    CMutexGuard guard(GetTypeInfoMutex());
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
    if ( !In().DetectLoops() ) {
        CopyObject(declaredType);
        return;
    }
    TTypeInfo typeInfo;
    switch ( In().ReadPointerType() ) {
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
                if (set) {
                    item->SetLocalCopyHook(*this, hook);
                } else {
                    item->ResetLocalCopyHook(*this);
                }
            }
        }
    }
    if (!m_PathCopyMemberHooks.IsEmpty()) {
        CCopyClassMemberHook* hook = m_PathCopyMemberHooks.GetHook(stk);
        if (hook) {
            CMemberInfo* item = m_PathCopyMemberHooks.FindItem(stk);
            if (item) {
                if (set) {
                    item->SetLocalCopyHook(*this, hook);
                } else {
                    item->ResetLocalCopyHook(*this);
                }
            }
        }
    }
    if (!m_PathCopyVariantHooks.IsEmpty()) {
        CCopyChoiceVariantHook* hook = m_PathCopyVariantHooks.GetHook(stk);
        if (hook) {
            CVariantInfo* item = m_PathCopyVariantHooks.FindItem(stk);
            if (item) {
                if (set) {
                    item->SetLocalCopyHook(*this, hook);
                } else {
                    item->ResetLocalCopyHook(*this);
                }
            }
        }
    }
}

END_NCBI_SCOPE

/*
* $Log$
* Revision 1.13  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.12  2004/01/05 14:25:20  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.11  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.10  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.9  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.8  2002/08/30 16:22:21  vasilche
* Removed excessive _TRACEs
*
* Revision 1.7  2002/08/27 17:44:14  grichenk
* Added separator output to object copier
*
* Revision 1.6  2001/05/17 15:07:06  lavr
* Typos corrected
*
* Revision 1.5  2000/10/20 15:51:39  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.4  2000/10/17 18:45:33  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.3  2000/09/29 16:18:22  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.2  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* ===========================================================================
*/
