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
* Revision 1.85  2001/10/17 20:41:23  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.84  2001/10/15 23:30:10  vakatov
* Get rid of extraneous "break;" after "throw"
*
* Revision 1.83  2001/07/30 14:42:46  lavr
* eDiag_Trace and eDiag_Fatal always print as much as possible
*
* Revision 1.82  2001/07/25 19:14:25  grichenk
* Implemented functions for reading/writing parent classes
*
* Revision 1.81  2001/07/17 14:52:48  kholodov
* Fixed: replaced int argument by size_t in CheckVisibleChar() and
* ReplaceVisibleChar to avoid truncation in 64 bit mode.
*
* Revision 1.80  2001/07/16 16:24:08  grichenk
* Added "case eFNP_Allow:" to ReplaceVisibleChar()
*
* Revision 1.79  2001/06/07 17:12:50  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.78  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.77  2001/05/16 17:55:39  grichenk
* Redesigned support for non-blocking stream read operations
*
* Revision 1.76  2001/05/11 20:41:17  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.75  2001/01/22 23:11:22  vakatov
* CObjectIStream::{Read,Skip}ClassSequential() -- use curr.member "pos"
*
* Revision 1.74  2001/01/05 20:10:50  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.73  2001/01/05 13:57:31  vasilche
* Fixed overloaded method ambiguity on Mac.
*
* Revision 1.72  2000/12/26 22:24:10  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.71  2000/12/26 17:26:16  vasilche
* Added one more Read() interface method.
*
* Revision 1.70  2000/12/15 21:29:01  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.69  2000/12/15 15:38:43  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.68  2000/11/01 20:38:37  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.67  2000/10/20 19:29:36  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.66  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.65  2000/10/17 18:45:34  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.64  2000/10/04 19:18:58  vasilche
* Fixed processing floating point data.
*
* Revision 1.63  2000/10/03 17:22:43  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.62  2000/09/29 16:18:22  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.61  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.60  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.59  2000/09/01 13:16:17  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.58  2000/08/15 19:44:48  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.57  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.56  2000/06/16 20:01:26  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.55  2000/06/16 18:04:11  thiessen
* avoid uncaught_exception() unimplemented on Mac/CodeWarrior
*
* Revision 1.54  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.53  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.52  2000/06/01 19:07:03  vasilche
* Added parsing of XML data.
*
* Revision 1.51  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.50  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.49  2000/05/04 16:22:19  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.48  2000/05/03 14:38:14  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.47  2000/04/28 16:58:12  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.46  2000/04/13 14:50:26  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.45  2000/04/06 16:10:59  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.44  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.43  2000/03/14 14:42:30  vasilche
* Fixed error reporting.
*
* Revision 1.42  2000/03/10 17:59:20  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.41  2000/03/07 14:06:22  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.40  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.39  2000/02/11 17:10:24  vasilche
* Optimized text parsing.
*
* Revision 1.38  2000/01/10 19:46:40  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.37  2000/01/05 19:43:53  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.36  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.35  1999/12/20 15:29:35  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.34  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.33  1999/11/18 20:18:08  vakatov
* ReadObjectInfo() -- get rid of the CodeWarrior(MAC) C++ compiler warning
*
* Revision 1.32  1999/10/18 20:21:41  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.31  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.30  1999/09/27 16:17:19  vasilche
* Fixed several incompatibilities with Windows
*
* Revision 1.29  1999/09/27 14:18:02  vasilche
* Fixed bug with overloaded constructors of Block.
*
* Revision 1.28  1999/09/26 05:19:02  vakatov
* FLT_MIN/MAX are not #define'd on some platforms
*
* Revision 1.27  1999/09/24 18:19:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.26  1999/09/23 21:16:07  vasilche
* Removed dependence on asn.h
*
* Revision 1.25  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.24  1999/09/23 18:56:58  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.23  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.22  1999/09/16 19:22:15  vasilche
* Removed some warnings under GCC.
*
* Revision 1.21  1999/09/14 18:54:17  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.20  1999/08/16 16:08:30  vasilche
* Added ENUMERATED type.
*
* Revision 1.19  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.18  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/26 18:31:34  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.16  1999/07/22 17:33:49  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.15  1999/07/19 15:50:32  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.13  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.12  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.11  1999/07/02 21:31:54  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:29  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:53  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:54  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:30  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:19:48  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:14:57  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:52  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <exception>
#include <serial/objistr.hpp>
#include <serial/typeref.hpp>
#include <serial/member.hpp>
#include <serial/variant.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <util/bytesrc.hpp>
#include <serial/delaybuf.hpp>
#include <serial/objistrimpl.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objlist.hpp>

#include <limits.h>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif

#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CRef<CByteSource> CObjectIStream::GetSource(ESerialDataFormat format,
                                            const string& fileName,
                                            unsigned openFlags)
{
    if ( (openFlags & eSerial_StdWhenEmpty) && fileName.empty() ||
         (openFlags & eSerial_StdWhenDash) && fileName == "-" ||
         (openFlags & eSerial_StdWhenStd) && fileName == "stdin" ) {
        return new CStreamByteSource(NcbiCin);
    }
    else {
        bool binary;
        switch ( format ) {
        case eSerial_AsnText:
        case eSerial_Xml:
            binary = false;
            break;
        case eSerial_AsnBinary:
            binary = true;
            break;
        default:
            THROW1_TRACE(runtime_error,
                         "CObjectIStream::Open: unsupported format");
        }
        
        if ( (openFlags & eSerial_UseFileForReread) )  {
            // use file as permanent file
            return new CFileByteSource(fileName, binary);
        }
        else {
            // open file as stream
            return new CFStreamByteSource(fileName, binary);
        }
    }
}

CRef<CByteSource> CObjectIStream::GetSource(CNcbiIstream& inStream,
                                            bool deleteInStream)
{
    if ( deleteInStream ) {
        return new CFStreamByteSource(inStream);
    }
    else {
        return new CStreamByteSource(inStream);
    }
}

CObjectIStream* CObjectIStream::Create(ESerialDataFormat format)
{
    switch ( format ) {
    case eSerial_AsnText:
        return CreateObjectIStreamAsn();
    case eSerial_AsnBinary:
        return CreateObjectIStreamAsnBinary();
    case eSerial_Xml:
        return CreateObjectIStreamXml();
    default:
        break;
    }
    THROW1_TRACE(runtime_error,
                 "CObjectIStream::Open: unsupported format");
}

CObjectIStream* CObjectIStream::Create(ESerialDataFormat format,
                                       const CRef<CByteSource>& source)
{
    AutoPtr<CObjectIStream> stream(Create(format));
    stream->Open(source);
    return stream.release();
}

CObjectIStream* CObjectIStream::Open(ESerialDataFormat format,
                                     CNcbiIstream& inStream,
                                     bool deleteInStream)
{
    return Create(format, GetSource(inStream, deleteInStream));
}

CObjectIStream* CObjectIStream::Open(ESerialDataFormat format,
                                     const string& fileName,
                                     unsigned openFlags)
{
    return Create(format, GetSource(format, fileName, openFlags));
}

CObjectIStream::CObjectIStream(void)
    : m_Fail(eNotOpen), m_Flags(eFlagNone)
{
}

CObjectIStream::~CObjectIStream(void)
{
}

void CObjectIStream::Open(const CRef<CByteSourceReader>& reader)
{
    Close();
    _ASSERT(m_Fail == eNotOpen);
    m_Input.Open(reader);
    m_Fail = 0;
}

void CObjectIStream::Open(const CRef<CByteSource>& source)
{
    Open(source.GetObject().Open());
}

void CObjectIStream::Open(CNcbiIstream& inStream, bool deleteInStream)
{
    Open(GetSource(inStream, deleteInStream));
}

void CObjectIStream::Close(void)
{
    m_Input.Close();
    if ( m_Objects )
        m_Objects->Clear();
    ClearStack();
    m_Fail = eNotOpen;
}

unsigned CObjectIStream::SetFailFlags(unsigned flags, const char* message)
{
    unsigned old = m_Fail;
    m_Fail |= flags;
    if ( !old && flags ) {
        // first fail
        ERR_POST("CObjectIStream: error at "<<
                 GetPosition()<<": "<<GetStackTrace() << ": " << message);
    }
    return old;
}

bool CObjectIStream::InGoodState(void)
{
    if ( fail() ) {
        // fail flag already set
        return false;
    }
    else if ( m_Input.fail() ) {
        // IO exception thrown without setting fail flag
        SetFailFlags(eReadError, m_Input.GetError());
        m_Input.ResetFail();
        return false;
    }
    else {
        // ok
        return true;
    }
}

void CObjectIStream::Unended(const string& msg)
{
    if ( InGoodState() )
        ThrowError(eFail, msg);
}

void CObjectIStream::UnendedFrame(void)
{
    Unended("internal error: unended object stack frame");
}

string CObjectIStream::GetStackTrace(void) const
{
    return GetStackTraceASN();
}

void CObjectIStream::ThrowError1(EFailFlags fail, const char* message)
{
    SetFailFlags(fail, message);
    THROW1_TRACE(runtime_error, message);
}

void CObjectIStream::ThrowError1(EFailFlags fail, const string& message)
{
    SetFailFlags(fail, message.c_str());
    THROW1_TRACE(runtime_error, message);
}

void CObjectIStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const char* message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    ThrowError1(fail, message);
}

void CObjectIStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const string& message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    ThrowError1(fail, message);
}

static inline
TTypeInfo MapType(const string& name)
{
    return CClassTypeInfoBase::GetClassInfoByName(name);
}

void CObjectIStream::RegisterObject(TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(typeInfo);
}

void CObjectIStream::RegisterObject(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    if ( m_Objects )
        m_Objects->RegisterObject(objectPtr, typeInfo);
}

const CReadObjectInfo&
CObjectIStream::GetRegisteredObject(CReadObjectInfo::TObjectIndex index)
{
    if ( !m_Objects ) {
        ThrowError(eFormatError,
                   "invalid object index: NO_COLLECT defined");
    }
    return m_Objects->GetRegisteredObject(index);
}

// root reader
void CObjectIStream::SkipFileHeader(TTypeInfo typeInfo)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
    
    string name = ReadFileHeader();
    if ( !name.empty() && name != typeInfo->GetName() ) {
        ThrowError(eFormatError,
                   "incompatible type "+name+"<>"+typeInfo->GetName());
    }

    END_OBJECT_FRAME();
}

void CObjectIStream::EndOfRead(void)
{
    if ( m_Objects )
        m_Objects->Clear();
}

void CObjectIStream::Read(const CObjectInfo& object, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_FRAME2(eFrameNamed, object.GetTypeInfo());
    
    ReadObject(object);

    EndOfRead();
    
    END_OBJECT_FRAME();
}

void CObjectIStream::Read(const CObjectInfo& object)
{
    // root object
    SkipFileHeader(object.GetTypeInfo());
    Read(object, eNoFileHeader);
}

void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo, ENoFileHeader)
{
    // root object
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    ReadObject(object, typeInfo);
    
    EndOfRead();

    END_OBJECT_FRAME();
}

void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    // root object
    SkipFileHeader(typeInfo);
    Read(object, typeInfo, eNoFileHeader);
}

CObjectInfo CObjectIStream::Read(TTypeInfo typeInfo)
{
    // root object
    SkipFileHeader(typeInfo);
    CObjectInfo info(typeInfo->Create(), typeInfo);
    Read(info, eNoFileHeader);
    return info;
}

CObjectInfo CObjectIStream::Read(const CObjectTypeInfo& type)
{
    return Read(type.GetTypeInfo());
}

void CObjectIStream::Skip(TTypeInfo typeInfo, ENoFileHeader)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    SkipObject(typeInfo);
    
    EndOfRead();

    END_OBJECT_FRAME();
}

void CObjectIStream::Skip(TTypeInfo typeInfo)
{
    SkipFileHeader(typeInfo);
    Skip(typeInfo, eNoFileHeader);
}

void CObjectIStream::Skip(const CObjectTypeInfo& type)
{
    Skip(type.GetTypeInfo());
}

void CObjectIStream::StartDelayBuffer(void)
{
    m_Input.StartSubSource();
}

void CObjectIStream::EndDelayBuffer(CDelayBuffer& buffer,
                                    const CItemInfo* itemInfo,
                                    TObjectPtr objectPtr)
{
    buffer.SetData(itemInfo, objectPtr,
                   GetDataFormat(), m_Input.EndSubSource());
}

void CObjectIStream::ExpectedMember(const CMemberInfo* memberInfo)
{
    ThrowError(eFormatError,
               "member "+memberInfo->GetId().ToString()+" expected");
}

void CObjectIStream::DuplicatedMember(const CMemberInfo* memberInfo)
{
    ThrowError(eFormatError,
               "duplicated member: "+memberInfo->GetId().ToString());
}

void CObjectIStream::ReadSeparateObject(const CObjectInfo& object)
{
    if ( m_Objects ) {
        size_t firstObject = m_Objects->GetObjectCount();
        ReadObject(object);
        size_t lastObject = m_Objects->GetObjectCount();
        m_Objects->ForgetObjects(firstObject, lastObject);
    }
    else {
        ReadObject(object);
    }
}

void CObjectIStream::ReadExternalObject(TObjectPtr objectPtr,
                                        TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read("<<NStr::PtrToString(objectPtr)<<", "<<
           typeInfo->GetName()<<")");
    RegisterObject(objectPtr, typeInfo);
    ReadObject(objectPtr, typeInfo);
}

CObjectInfo CObjectIStream::ReadObject(void)
{
    TTypeInfo typeInfo = MapType(ReadFileHeader());
    TObjectPtr objectPtr;
    BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);

    objectPtr = typeInfo->Create();
    RegisterObject(objectPtr, typeInfo);
    ReadObject(objectPtr, typeInfo);

    END_OBJECT_FRAME();
    return make_pair(objectPtr, typeInfo);
}

void CObjectIStream::ReadObject(const CObjectInfo& object)
{
    ReadObject(object.GetObjectPtr(), object.GetTypeInfo());
}

void CObjectIStream::SkipObject(const CObjectTypeInfo& objectType)
{
    SkipObject(objectType.GetTypeInfo());
}

void CObjectIStream::ReadClassMember(const CObjectInfo::CMemberIterator& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    ReadObject(memberInfo->GetMemberPtr(classPtr),
		       memberInfo->GetTypeInfo());
}

string CObjectIStream::ReadFileHeader(void)
{
    return NcbiEmptyString;
}

pair<TObjectPtr, TTypeInfo> CObjectIStream::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::ReadPointer("<<declaredType->GetName()<<")");
    TObjectPtr objectPtr;
    TTypeInfo objectType;
    switch ( ReadPointerType() ) {
    case eNullPointer:
        _TRACE("CObjectIStream::ReadPointer: null");
        return pair<TObjectPtr, TTypeInfo>(0, declaredType);
    case eObjectPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::ReadPointer: @" << index);
            const CReadObjectInfo& info = GetRegisteredObject(index);
            objectType = info.GetTypeInfo();
            objectPtr = info.GetObjectPtr();
            if ( !objectPtr ) {
                ThrowError(eFormatError,
                           "invalid reference to skipped object");
            }
            break;
        }
    case eThisPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            objectPtr = declaredType->Create();
            RegisterObject(objectPtr, declaredType);
            ReadObject(objectPtr, declaredType);
            return make_pair(objectPtr, declaredType);
        }
    case eOtherPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            objectType = MapType(className);

            BEGIN_OBJECT_FRAME2(eFrameNamed, objectType);
                
            objectPtr = objectType->Create();
            RegisterObject(objectPtr, objectType);
            ReadObject(objectPtr, objectType);
                
            END_OBJECT_FRAME();

            ReadOtherPointerEnd();
            break;
        }
    default:
        ThrowError(eFormatError, "illegal pointer type");
        break;
    }
    while ( objectType != declaredType ) {
        // try to check parent class pointer
        if ( objectType->GetTypeFamily() != eTypeFamilyClass ) {
            ThrowError(eFormatError, "incompatible member type");
        }
        const CClassTypeInfo* parentClass =
            CTypeConverter<CClassTypeInfo>::SafeCast(objectType)->GetParentClassInfo();
        if ( parentClass ) {
            objectType = parentClass;
        }
        else {
            ThrowError(eFormatError, "incompatible member type");
        }
    }
    return make_pair(objectPtr, objectType);
}

void CObjectIStream::ReadOtherPointerEnd(void)
{
}

void CObjectIStream::SkipExternalObject(TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::SkipExternalObject("<<typeInfo->GetName()<<")");
    RegisterObject(typeInfo);
    SkipObject(typeInfo);
}

void CObjectIStream::SkipPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::SkipPointer("<<declaredType->GetName()<<")");
    switch ( ReadPointerType() ) {
    case eNullPointer:
        _TRACE("CObjectIStream::SkipPointer: null");
        return;
    case eObjectPointer:
        {
            _TRACE("CObjectIStream::SkipPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::SkipPointer: @" << index);
            GetRegisteredObject(index);
            break;
        }
    case eThisPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            RegisterObject(declaredType);
            SkipObject(declaredType);
            break;
        }
    case eOtherPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
                
            RegisterObject(typeInfo);
            SkipObject(typeInfo);

            END_OBJECT_FRAME();
            ReadOtherPointerEnd();
            break;
        }
    default:
        ThrowError(eFormatError, "illegal pointer type");
    }
}

void CObjectIStream::BeginNamedType(TTypeInfo /*namedTypeInfo*/)
{
}

void CObjectIStream::EndNamedType(void)
{
}

void CObjectIStream::ReadNamedType(TTypeInfo
#ifndef VIRTUAL_MID_LEVEL_IO
                                   namedTypeInfo
#endif
                                   ,
                                   TTypeInfo typeInfo, TObjectPtr object)
{
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    ReadObject(object, typeInfo);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif
}

void CObjectIStream::SkipNamedType(TTypeInfo /*namedTypeInfo*/,
                                   TTypeInfo typeInfo)
{
    SkipObject(typeInfo);
}

void CObjectIStream::EndContainerElement(void)
{
}

void CObjectIStream::ReadContainer(const CContainerTypeInfo* containerType,
                                   TObjectPtr containerPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
    BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

    while ( BeginContainerElement(elementType) ) {
        containerType->AddElement(containerPtr, *this);
        EndContainerElement();
    }

    END_OBJECT_FRAME();

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipContainer(const CContainerTypeInfo* containerType)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
    BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

    while ( BeginContainerElement(elementType) ) {
        SkipObject(elementType);
        EndContainerElement();
    }

    END_OBJECT_FRAME();

    EndContainer();
    END_OBJECT_FRAME();
}

void CObjectIStream::EndClass(void)
{
}

void CObjectIStream::EndClassMember(void)
{
}

void CObjectIStream::ReadClassRandom(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);

    ReadClassRandomContentsBegin();

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {

        ReadClassRandomContentsMember();
        
        EndClassMember();
    }

    ReadClassRandomContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::ReadClassSequential(const CClassTypeInfo* classType,
                                         TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    ReadClassSequentialContentsBegin();

    TMemberIndex index;
    while ( (index = BeginClassMember(classType, *pos)) != kInvalidMember ) {

        ReadClassSequentialContentsMember();

        EndClassMember();
    }

    ReadClassSequentialContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipClassRandom(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    SkipClassRandomContentsBegin();

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {

        SkipClassRandomContentsMember();

        EndClassMember();
    }

    SkipClassRandomContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::SkipClassSequential(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    BeginClass(classType);
    
    SkipClassSequentialContentsBegin();

    TMemberIndex index;
    while ( (index = BeginClassMember(classType, *pos)) != kInvalidMember ) {

        SkipClassSequentialContentsMember();

        EndClassMember();
    }

    SkipClassSequentialContentsEnd();
    
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStream::EndChoiceVariant(void)
{
}

void CObjectIStream::ReadChoice(const CChoiceTypeInfo* choiceType,
                                TObjectPtr choicePtr)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);

    TMemberIndex index = BeginChoiceVariant(choiceType);
    _ASSERT(index != kInvalidMember);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);

    EndChoiceVariant();
    END_OBJECT_FRAME();

    END_OBJECT_FRAME();
}

void CObjectIStream::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);

    TMemberIndex index = BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember )
        ThrowError(eFormatError, "choice variant id expected");

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->SkipVariant(*this);

    EndChoiceVariant();
    END_OBJECT_FRAME();

    END_OBJECT_FRAME();
}

///////////////////////////////////////////////////////////////////////
//
// CObjectIStream::ByteBlock
//

CObjectIStream::ByteBlock::ByteBlock(CObjectIStream& in)
    : m_Stream(in), m_KnownLength(false), m_Ended(false), m_Length(1)
{
    in.BeginBytes(*this);
}

CObjectIStream::ByteBlock::~ByteBlock(void)
{
    if ( !m_Ended )
        GetStream().Unended("byte block not fully read");
}

void CObjectIStream::ByteBlock::End(void)
{
    _ASSERT(!m_Ended);
    if ( m_Length == 0 ) {
        GetStream().EndBytes(*this);
        m_Ended = true;
    }
}

size_t CObjectIStream::ByteBlock::Read(void* dst, size_t needLength,
                                       bool forceLength)
{
    size_t length;
    if ( KnownLength() ) {
        if ( m_Length < needLength )
            length = m_Length;
        else
            length = needLength;
    }
    else {
        if ( m_Length == 0 )
            length = 0;
        else
            length = needLength;
    }
    
    if ( length == 0 ) {
        if ( forceLength && needLength != 0 )
            GetStream().ThrowError(eReadError, "read fault");
        return 0;
    }

    length = GetStream().ReadBytes(*this, static_cast<char*>(dst), length);
    if ( KnownLength() )
        m_Length -= length;
    if ( forceLength && needLength != length )
        GetStream().ThrowError(eReadError, "read fault");
    return length;
}

///////////////////////////////////////////////////////////////////////
//
// CObjectIStream::CharBlock
//

CObjectIStream::CharBlock::CharBlock(CObjectIStream& in)
    : m_Stream(in), m_KnownLength(false), m_Ended(false), m_Length(1)
{
    in.BeginChars(*this);
}

CObjectIStream::CharBlock::~CharBlock(void)
{
    if ( !m_Ended )
        GetStream().Unended("char block not fully read");
}

void CObjectIStream::CharBlock::End(void)
{
    _ASSERT(!m_Ended);
    if ( m_Length == 0 ) {
        GetStream().EndChars(*this);
        m_Ended = true;
    }
}

size_t CObjectIStream::CharBlock::Read(char* dst, size_t needLength,
                                       bool forceLength)
{
    size_t length;
    if ( KnownLength() ) {
        if ( m_Length < needLength )
            length = m_Length;
        else
            length = needLength;
    }
    else {
        if ( m_Length == 0 )
            length = 0;
        else
            length = needLength;
    }
    
    if ( length == 0 ) {
        if ( forceLength && needLength != 0 )
            GetStream().ThrowError(eReadError, "read fault");
        return 0;
    }

    length = GetStream().ReadChars(*this, dst, length);
    if ( KnownLength() )
        m_Length -= length;
    if ( forceLength && needLength != length )
        GetStream().ThrowError(eReadError, "read fault");
    return length;
}


void CObjectIStream::EndBytes(const ByteBlock& /*b*/)
{
}

void CObjectIStream::EndChars(const CharBlock& /*b*/)
{
}

Int1 CObjectIStream::ReadInt1(void)
{
    Int4 data = ReadInt4();
    Int1 ret = Int1(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

Uint1 CObjectIStream::ReadUint1(void)
{
    Uint4 data = ReadUint4();
    Uint1 ret = Uint1(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

Int2 CObjectIStream::ReadInt2(void)
{
    Int4 data = ReadInt4();
    Int2 ret = Int2(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

Uint2 CObjectIStream::ReadUint2(void)
{
    Uint4 data = ReadUint4();
    Uint2 ret = Uint2(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

Int4 CObjectIStream::ReadInt4(void)
{
    Int8 data = ReadInt8();
    Int4 ret = Int4(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

Uint4 CObjectIStream::ReadUint4(void)
{
    Uint8 data = ReadUint8();
    Uint4 ret = Uint4(data);
    if ( ret != data )
        ThrowError(eOverflow, "integer overflow");
    return ret;
}

float CObjectIStream::ReadFloat(void)
{
    double data = ReadDouble();
#if defined(FLT_MIN) && defined(FLT_MAX)
    if ( data < FLT_MIN || data > FLT_MAX )
        ThrowError(eOverflow, "float overflow");
#endif
    return float(data);
}

#if SIZEOF_LONG_DOUBLE != 0
long double CObjectIStream::ReadLDouble(void)
{
    return ReadDouble();
}
#endif

char* CObjectIStream::ReadCString(void)
{
    string s;
    ReadString(s);
	return strdup(s.c_str());
}

void CObjectIStream::ReadStringStore(string& s)
{
    ReadString(s);
}

void CObjectIStream::SkipInt1(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint1(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt2(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint2(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt4(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint4(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt8(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUint8(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipFloat(void)
{
    SkipFNumber();
}

void CObjectIStream::SkipDouble(void)
{
    SkipFNumber();
}

#if SIZEOF_LONG_DOUBLE != 0
void CObjectIStream::SkipLDouble(void)
{
    SkipFNumber();
}
#endif

void CObjectIStream::SkipCString(void)
{
    SkipString();
}

void CObjectIStream::SkipStringStore(void)
{
    SkipString();
}

#if HAVE_NCBI_C
extern "C" {
    Int2 LIBCALLBACK ReadAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
        CObjectIStream::AsnIo* asnio =
            static_cast<CObjectIStream::AsnIo*>(object);
        return Uint2(asnio->Read(data, length));
    }
}

CObjectIStream::AsnIo::AsnIo(CObjectIStream& in, const string& rootTypeName)
    : m_Stream(in), m_Ended(false),
      m_RootTypeName(rootTypeName), m_Count(0)
{
    m_AsnIo = AsnIoNew(in.GetAsnFlags() | ASNIO_IN, 0, this, ReadAsn, 0);
    in.AsnOpen(*this);
}

void CObjectIStream::AsnIo::End(void)
{
    _ASSERT(!m_Ended);
    if ( GetStream().InGoodState() ) {
        AsnIoClose(*this);
        GetStream().AsnClose(*this);
        m_Ended = true;
    }
}

CObjectIStream::AsnIo::~AsnIo(void)
{
    if ( !m_Ended )
        GetStream().Unended("AsnIo read error");
}

unsigned CObjectIStream::GetAsnFlags(void)
{
    return 0;
}

void CObjectIStream::AsnOpen(AsnIo& )
{
}

void CObjectIStream::AsnClose(AsnIo& )
{
}

size_t CObjectIStream::AsnRead(AsnIo& , char* , size_t )
{
    ThrowError(eIllegalCall, "illegal call");
    return 0;
}
#endif


char& ReplaceVisibleChar(char& c, EFixNonPrint fix_method, size_t at_line)
{
    string message = "Bad char in VisibleString" +
        ((at_line > 0) ?
            " starting at line " + NStr::UIntToString(at_line) :
            string("")) + ": " + NStr::IntToString(int(c) & 0xff);
    c = '#';
    switch (fix_method) {
    case eFNP_Replace:
        break;
    case eFNP_ReplaceAndWarn:
        CNcbiDiag(eDiag_Error, eDPF_Default) << message << Endm;
        break;
    case eFNP_Throw:
        throw runtime_error(message);
    case eFNP_Abort:
        CNcbiDiag(eDiag_Fatal, eDPF_Default) << message << Endm;
        break;
    case eFNP_Allow:
        break;
    }
    return c;
}


END_NCBI_SCOPE
