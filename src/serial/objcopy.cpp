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

#include <corelib/ncbistd.hpp>
#include <serial/objcopy.hpp>
#include <serial/typeinfo.hpp>
#include <serial/classinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/choice.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objistrimpl.hpp>
#include <serial/object.hpp>

BEGIN_NCBI_SCOPE

CObjectStreamCopier::CObjectStreamCopier(CObjectIStream& in,
                                         CObjectOStream& out)
    : m_In(in), m_Out(out)
{
}

void CObjectStreamCopier::Copy(const CObjectTypeInfo& objectType)
{
    TTypeInfo type = objectType.GetTypeInfo();
    // root object
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, type);
    In().SkipFileHeader(type);

    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, type);
    Out().WriteFileHeader(type);

    CopyObject(type);
    
    Out().EndOfWrite();
    END_OBJECT_FRAME_OF(Out());

    In().EndOfRead();
    END_OBJECT_FRAME_OF(In());
}

void CObjectStreamCopier::Copy(TTypeInfo type, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_FRAME_OF2(In(), eFrameNamed, type);

    BEGIN_OBJECT_FRAME_OF2(Out(), eFrameNamed, type);
    Out().WriteFileHeader(type);

    CopyObject(type);
    
    Out().EndOfWrite();
    END_OBJECT_FRAME_OF(Out());

    In().EndOfRead();
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
            if ( typeInfo->GetTypeFamily() != eTypeFamilyClass ) {
                In().SetFailFlags(CObjectIStream::eFormatError);
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
            const CClassTypeInfo* parentClass =
                CTypeConverter<CClassTypeInfo>::SafeCast(typeInfo)->GetParentClassInfo();
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

END_NCBI_SCOPE
