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
* Revision 1.57  2000/12/15 22:07:02  vasilche
* Fixed typo eNotOpen -> eNoError.
*
* Revision 1.56  2000/12/15 21:29:02  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.55  2000/12/15 15:38:44  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.54  2000/10/20 19:29:36  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.53  2000/10/20 15:51:42  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.52  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.51  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.50  2000/10/03 17:22:44  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.49  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.48  2000/09/26 19:24:57  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.47  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.46  2000/09/18 20:00:24  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.45  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.44  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.43  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.42  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.41  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.40  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* Revision 1.39  2000/05/24 20:08:48  vasilche
* Implemented XML dump.
*
* Revision 1.38  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.37  2000/05/04 16:22:20  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.36  2000/04/28 16:58:13  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.35  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.34  2000/04/06 16:11:00  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.33  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.32  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.31  2000/02/11 17:10:25  vasilche
* Optimized text parsing.
*
* Revision 1.30  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.29  2000/01/05 19:43:56  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.28  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.27  1999/12/20 15:29:35  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.26  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/10/04 16:22:18  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.24  1999/09/27 14:18:02  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.23  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.22  1999/09/23 21:16:08  vasilche
* Removed dependance on asn.h
*
* Revision 1.21  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.20  1999/09/23 18:57:00  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.19  1999/09/14 18:54:19  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.18  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.17  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.16  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.15  1999/07/22 17:33:55  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.14  1999/07/19 15:50:35  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.13  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:58  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:33  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:59  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:06  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:19:50  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:54  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/typeref.hpp>
#include <serial/objlist.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/bytesrc.hpp>
#include <serial/delaybuf.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/member.hpp>
#include <serial/variant.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objlist.hpp>

#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& out, bool deleteOut);
CObjectOStream* OpenObjectOStreamAsnBinary(CNcbiOstream& out, bool deleteOut);
CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out, bool deleteOut);

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     unsigned openFlags)
{
    CNcbiOstream* outStream = 0;
    bool deleteStream;
    if ( (openFlags & eSerial_StdWhenEmpty) && fileName.empty() ||
         (openFlags & eSerial_StdWhenDash) && fileName == "-" ||
         (openFlags & eSerial_StdWhenStd) && fileName == "stdout" ) {
        outStream = &NcbiCout;
        deleteStream = false;
    }
    else {
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
            outStream = new CNcbiOfstream(fileName.c_str());
            break;
        case eSerial_AsnBinary:
            outStream = new CNcbiOfstream(fileName.c_str(),
                                          IOS_BASE::out | IOS_BASE::binary);
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CObjectOStream::Open: unsupported format");
        }
        if ( !*outStream ) {
            delete outStream;
            THROW1_TRACE(runtime_error, "cannot open file");
        }
        deleteStream = true;
    }

    return Open(format, *outStream, deleteStream);
}

CObjectOStream* CObjectOStream::Open(ESerialDataFormat format,
                                     CNcbiOstream& outStream,
                                     bool deleteStream)
{
    switch ( format ) {
    case eSerial_AsnText:
        return OpenObjectOStreamAsn(outStream, deleteStream);
    case eSerial_AsnBinary:
        return OpenObjectOStreamAsnBinary(outStream, deleteStream);
    case eSerial_Xml:
        return OpenObjectOStreamXml(outStream, deleteStream);
    default:
        break;
    }
    THROW1_TRACE(runtime_error,
                 "CObjectOStream::Open: unsupported format");
}

CObjectOStream::CObjectOStream(CNcbiOstream& out, bool deleteOut)
    : m_Output(out, deleteOut), m_Fail(eNoError), m_Flags(eFlagNone)
{
}

CObjectOStream::~CObjectOStream(void)
{
}

void CObjectOStream::Close(void)
{
    m_Fail = eNotOpen;
    m_Output.Close();
    if ( m_Objects )
        m_Objects->Clear();
    ClearStack();
}

unsigned CObjectOStream::SetFailFlagsNoError(unsigned flags)
{
    unsigned old = m_Fail;
    m_Fail |= flags;
    return old;
}

unsigned CObjectOStream::SetFailFlags(unsigned flags, const char* message)
{
    unsigned old = m_Fail;
    m_Fail |= flags;
    if ( !old && flags ) {
        // first fail
        ERR_POST("CObjectOStream: error at "<<
                 GetPosition()<<": "<<GetStackTrace() << ": " << message);
    }
    return old;
}

bool CObjectOStream::InGoodState(void)
{
    if ( fail() ) {
        // fail flag already set
        return false;
    }
    else if ( m_Output.fail() ) {
        // IO exception thrown without setting fail flag
        SetFailFlags(eWriteError, m_Output.GetError());
        m_Output.ResetFail();
        return false;
    }
    else {
        // ok
        return true;
    }
}

void CObjectOStream::Unended(const string& msg)
{
    if ( InGoodState() )
        ThrowError(eFail, msg);
}

void CObjectOStream::UnendedFrame(void)
{
    Unended("internal error: unended object stack frame");
}

string CObjectOStream::GetStackTrace(void) const
{
    return GetStackTraceASN();
}

void CObjectOStream::ThrowError1(EFailFlags fail, const char* message)
{
    SetFailFlags(fail, message);
    THROW1_TRACE(runtime_error, message);
}

void CObjectOStream::ThrowError1(EFailFlags fail, const string& message)
{
    SetFailFlags(fail, message.c_str());
    THROW1_TRACE(runtime_error, message);
}

void CObjectOStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const char* message)
{
    CNcbiDiag(file, line, eDiag_Trace, eDPF_Trace) << message;
    ThrowError1(fail, message);
}

void CObjectOStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const string& message)
{
    CNcbiDiag(file, line, eDiag_Trace, eDPF_Trace) << message;
    ThrowError1(fail, message);
}

void CObjectOStream::EndOfWrite(void)
{
    FlushBuffer();
    if ( m_Objects )
        m_Objects->Clear();
}    

void CObjectOStream::WriteObject(const CConstObjectInfo& object)
{
    WriteObject(object.GetObjectPtr(), object.GetTypeInfo());
}

void CObjectOStream::WriteClassMember(const CConstObjectInfo::CMemberIterator& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TConstObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    WriteClassMember(memberInfo->GetId(),
                     memberInfo->GetTypeInfo(),
                     memberInfo->GetMemberPtr(classPtr));
}

void CObjectOStream::Write(const CConstObjectInfo& object)
{
    // root writer
    BEGIN_OBJECT_FRAME2(eFrameNamed, object.GetTypeInfo());
    
    WriteFileHeader(object.GetTypeInfo());

    WriteObject(object);

    EndOfWrite();
    
    END_OBJECT_FRAME();
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    // root writer
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
    
    WriteFileHeader(typeInfo);

    WriteObject(object, typeInfo);

    EndOfWrite();
    
    END_OBJECT_FRAME();
}

void CObjectOStream::Write(TConstObjectPtr object, const CTypeRef& type)
{
    Write(object, type.Get());
}

void CObjectOStream::RegisterObject(TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(typeInfo);
}

void CObjectOStream::RegisterObject(TConstObjectPtr object,
                                    TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(object, typeInfo);
}

void CObjectOStream::WriteSeparateObject(const CConstObjectInfo& object)
{
    if ( m_Objects ) {
        size_t firstObject = m_Objects->GetObjectCount();
        WriteObject(object);
        size_t lastObject = m_Objects->GetObjectCount();
        m_Objects->ForgetObjects(firstObject, lastObject);
    }
    else {
        WriteObject(object);
    }
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr objectPtr,
                                         TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::WriteExternalObject(" <<
           NStr::PtrToString(objectPtr) << ", "
           << typeInfo->GetName() << ')');
    RegisterObject(objectPtr, typeInfo);
    WriteObject(objectPtr, typeInfo);
}

bool CObjectOStream::Write(const CRef<CByteSource>& source)
{
    m_Output.Write(source.GetObject().Open());
    return true;
}

void CObjectOStream::WriteFileHeader(TTypeInfo /*type*/)
{
    // do nothing by default
}

void CObjectOStream::WritePointer(TConstObjectPtr objectPtr,
                                  TTypeInfo declaredTypeInfo)
{
    _TRACE("WritePointer("<<NStr::PtrToString(objectPtr)<<", "
           <<declaredTypeInfo->GetName()<<")");
    if ( objectPtr == 0 ) {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<": null");
        WriteNullPointer();
        return;
    }
    TTypeInfo realTypeInfo = declaredTypeInfo->GetRealTypeInfo(objectPtr);
    if ( m_Objects ) {
        const CWriteObjectInfo* info =
            m_Objects->RegisterObject(objectPtr, realTypeInfo);
        if ( info ) {
            // old object
            WriteObjectReference(info->GetIndex());
            return;
        }
    }
    if ( declaredTypeInfo == realTypeInfo ) {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<": new");
        WriteThis(objectPtr, realTypeInfo);
    }
    else {
        _TRACE("WritePointer: "<<NStr::PtrToString(objectPtr)<<
               ": new "<<realTypeInfo->GetName());
        WriteOther(objectPtr, realTypeInfo);
    }
}

void CObjectOStream::WriteThis(TConstObjectPtr object, TTypeInfo typeInfo)
{
    WriteObject(object, typeInfo);
}

void CObjectOStream::WriteFloat(float data)
{
    WriteDouble(data);
}

#if SIZEOF_LONG_DOUBLE != 0
void CObjectOStream::WriteLDouble(long double data)
{
    WriteDouble(data);
}
#endif

void CObjectOStream::BeginNamedType(TTypeInfo /*namedTypeInfo*/)
{
}

void CObjectOStream::EndNamedType(void)
{
}

void CObjectOStream::WriteNamedType(TTypeInfo
#ifndef VIRTUAL_MID_LEVEL_IO
                                    namedTypeInfo
#endif
                                    ,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    WriteObject(objectPtr, objectType);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif
}

void CObjectOStream::CopyNamedType(TTypeInfo namedTypeInfo,
                                   TTypeInfo objectType,
                                   CObjectStreamCopier& copier)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);
    BeginNamedType(namedTypeInfo);

    CopyObject(objectType, copier);

    EndNamedType();
    copier.In().EndNamedType();
    END_OBJECT_2FRAMES_OF(copier);
#else
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);

    CopyObject(objectType, copier);

    copier.In().EndNamedType();
    END_OBJECT_FRAME_OF(copier.In());
#endif
}

void CObjectOStream::WriteOther(TConstObjectPtr object,
                                TTypeInfo typeInfo)
{
    WriteOtherBegin(typeInfo);
    WriteObject(object, typeInfo);
    WriteOtherEnd(typeInfo);
}

void CObjectOStream::WriteOtherEnd(TTypeInfo /*typeInfo*/)
{
}

void CObjectOStream::EndContainer(void)
{
}

void CObjectOStream::BeginContainerElement(TTypeInfo /*elementType*/)
{
}

void CObjectOStream::EndContainerElement(void)
{
}

void CObjectOStream::WriteContainer(const CContainerTypeInfo* cType,
                                    TConstObjectPtr containerPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    BeginContainer(cType);
        
    CContainerTypeInfo::CConstIterator i;
    if ( cType->InitIterator(i, containerPtr) ) {
        TTypeInfo elementType = cType->GetElementType();
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        do {
            BeginContainerElement(elementType);
            
            WriteObject(cType->GetElementPtr(i), elementType);
            
            EndContainerElement();
        } while ( cType->NextElement(i) );

        END_OBJECT_FRAME();
    }

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectOStream::WriteContainerElement(const CConstObjectInfo& element)
{
    BeginContainerElement(element.GetTypeInfo());

    WriteObject(element);

    EndContainerElement();
}

void CObjectOStream::CopyContainer(const CContainerTypeInfo* cType,
                                   CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArray, cType);
    copier.In().BeginContainer(cType);
    BeginContainer(cType);

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArrayElement, elementType);

    while ( copier.In().BeginContainerElement(elementType) ) {
        BeginContainerElement(elementType);

        CopyObject(elementType, copier);

        EndContainerElement();
        copier.In().EndContainerElement();
    }

    END_OBJECT_2FRAMES_OF(copier);
    
    EndContainer();
    copier.In().EndContainer();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::EndClass(void)
{
}

void CObjectOStream::EndClassMember(void)
{
}

void CObjectOStream::WriteClass(const CClassTypeInfo* classType,
                                TConstObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        classType->GetMemberInfo(*i)->WriteMember(*this, classPtr);
    }
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectOStream::WriteClassMember(const CMemberId& memberId,
                                      TTypeInfo memberType,
                                      TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    BeginClassMember(memberId);

    WriteObject(memberPtr, memberType);

    EndClassMember();
    END_OBJECT_FRAME();
}

bool CObjectOStream::WriteClassMember(const CMemberId& memberId,
                                      const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(GetDataFormat()) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    BeginClassMember(memberId);

    Write(buffer.GetSource());

    EndClassMember();
    END_OBJECT_FRAME();
    return true;
}

void CObjectOStream::CopyClassRandom(const CClassTypeInfo* classType,
                                     CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    BeginClass(classType);

    vector<bool> read(classType->GetMembers().LastIndex() + 1);

    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());

        if ( read[index] ) {
            copier.In().DuplicatedMember(memberInfo);
        }
        else {
            read[index] = true;
            BeginClassMember(memberInfo->GetId());

            memberInfo->CopyMember(copier);

            EndClassMember();
        }
        
        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        if ( !read[*i] ) {
            classType->GetMemberInfo(*i)->CopyMissingMember(copier);
        }
    }

    EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::CopyClassSequential(const CClassTypeInfo* classType,
                                         CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    BeginClass(classType);

    CClassTypeInfo::CIterator pos(classType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType, *pos)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());

        for ( TMemberIndex i = *pos; i < index; ++i ) {
            // init missing member
            classType->GetMemberInfo(i)->CopyMissingMember(copier);
        }
        BeginClassMember(memberInfo->GetId());

        memberInfo->CopyMember(copier);
        
        pos.SetIndex(index + 1);

        EndClassMember();
        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( ; pos.Valid(); ++pos ) {
        classType->GetMemberInfo(*pos)->CopyMissingMember(copier);
    }

    EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::EndChoiceVariant(void)
{
}

void CObjectOStream::WriteChoice(const CChoiceTypeInfo* choiceType,
                                 TConstObjectPtr choicePtr)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());
    BeginChoiceVariant(choiceType, variantInfo->GetId());

    variantInfo->WriteVariant(*this, choicePtr);

    EndChoiceVariant();
    END_OBJECT_FRAME();

    END_OBJECT_FRAME();
}

void CObjectOStream::CopyChoice(const CChoiceTypeInfo* choiceType,
                                CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameChoice, choiceType);

    TMemberIndex index = copier.In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        copier.ThrowError(CObjectIStream::eFormatError,
                          "choice variant id expected");
    }

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameChoiceVariant,
                             variantInfo->GetId());
    BeginChoiceVariant(choiceType, variantInfo->GetId());

    variantInfo->CopyVariant(copier);

    EndChoiceVariant();
    copier.In().EndChoiceVariant();
    END_OBJECT_2FRAMES_OF(copier);

    END_OBJECT_2FRAMES_OF(copier);
}

void CObjectOStream::BeginBytes(const ByteBlock& )
{
}

void CObjectOStream::EndBytes(const ByteBlock& )
{
}

void CObjectOStream::ByteBlock::End(void)
{
    _ASSERT(!m_Ended);
    _ASSERT(m_Length == 0);
    if ( GetStream().InGoodState() ) {
        GetStream().EndBytes(*this);
        m_Ended = true;
    }
}

CObjectOStream::ByteBlock::~ByteBlock(void)
{
    if ( !m_Ended )
        GetStream().Unended("byte block not fully written");
}

#if HAVE_NCBI_C
extern "C" {
    Int2 LIBCALLBACK WriteAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
    
        static_cast<CObjectOStream::AsnIo*>(object)->Write(data, length);
        return length;
    }
}

CObjectOStream::AsnIo::AsnIo(CObjectOStream& out, const string& rootTypeName)
    : m_Stream(out), m_RootTypeName(rootTypeName), m_Ended(false), m_Count(0)
{
    m_AsnIo = AsnIoNew(out.GetAsnFlags() | ASNIO_OUT, 0, this, 0, WriteAsn);
    out.AsnOpen(*this);
}

void CObjectOStream::AsnIo::End(void)
{
    _ASSERT(!m_Ended);
    if ( GetStream().InGoodState() ) {
        AsnIoClose(*this);
        GetStream().AsnClose(*this);
        m_Ended = true;
    }
}

CObjectOStream::AsnIo::~AsnIo(void)
{
    if ( !m_Ended )
        GetStream().Unended("AsnIo write error");
}

void CObjectOStream::AsnOpen(AsnIo& )
{
}

void CObjectOStream::AsnClose(AsnIo& )
{
}

unsigned CObjectOStream::GetAsnFlags(void)
{
    return 0;
}

void CObjectOStream::AsnWrite(AsnIo& , const char* , size_t )
{
    ThrowError(eIllegalCall, "illegal call");
}
#endif

END_NCBI_SCOPE
