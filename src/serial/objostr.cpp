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
#include <serial/typeref.hpp>
#include <serial/objlist.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/bytesrc.hpp>
#include <serial/delaybuf.hpp>
#include <serial/classinfo.hpp>
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& in, bool deleteOut);
CObjectOStream* OpenObjectOStreamAsnBinary(CNcbiOstream& in, bool deleteOut);
CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& in, bool deleteOut);

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
    : m_Output(out, deleteOut)
{
}

CObjectOStream::~CObjectOStream(void)
{
    _TRACE("~CObjectOStream:"<<m_Objects.GetObjectCount()<<" objects written");
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::Write(" << NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ')');
#if NCBISER_ALLOW_CYCLES
    CWriteObjectInfo& info = m_Objects.RegisterObject(object, typeInfo);
    if ( info.IsWritten() ) {
        THROW1_TRACE(runtime_error,
                     "trying to write already written object");
    }
#endif
    WriteTypeName(typeInfo->GetName());
#if NCBISER_ALLOW_CYCLES
    WriteObject(object, info);
    m_Objects.CheckAllWritten();
#else
    WriteData(object, typeInfo);
#endif
    FlushBuffer();
}

void CObjectOStream::Write(TConstObjectPtr object, const CTypeRef& type)
{
    Write(object, type.Get());
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::RegisterAndWrite(" <<
           NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ')');
#if NCBISER_ALLOW_CYCLES
    _ASSERT(object != 0);
    CWriteObjectInfo& info = m_Objects.RegisterObject(object, typeInfo);
    if ( info.IsWritten() ) {
        THROW1_TRACE(runtime_error,
                     "trying to write already written object");
    }
    WriteObject(object, info);
#else
    WriteData(object, typeInfo);
#endif
}

bool CObjectOStream::Write(const CRef<CByteSource>& source)
{
    m_Output.Write(source.GetObject().Open());
    return true;
}

void CObjectOStream::WriteTypeName(const string& )
{
    // do nothing by default
}

bool CObjectOStream::WriteEnum(const CEnumeratedTypeValues& values, long value)
{
    if ( values.IsInteger() ) {
        return false;
    }
    values.FindName(value, false);
    WriteLong(value);
    return true;
}

void CObjectOStream::WritePointer(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("WritePointer(" << NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ")");
    if ( object == 0 ) {
        _TRACE("WritePointer: " << NStr::PtrToString(object) << ": null");
        WriteNullPointer();
        return;
    }
#if NCBISER_ALLOW_CYCLES
    CWriteObjectInfo& info = m_Objects.RegisterObject(object, typeInfo);
    WritePointer(object, info, typeInfo);
#else
    ...;
    TTypeInfo realTypeInfo = typeInfo->GetRealTypeInfo(object);
    if ( typeInfo == realTypeInfo ) {
        _TRACE("WritePointer: " <<
               NStr::PtrToString(object) << ": new");
        WriteThis(object, realTypeInfo);
    }
    else {
        _TRACE("WritePointer: " <<
               NStr::PtrToString(object)
               << ": new " << realTypeInfo->GetName());
        WriteOther(object, realTypeInfo);
    }
#endif
}

void CObjectOStream::WritePointer(TConstObjectPtr object,
                                  CWriteObjectInfo& info,
                                  TTypeInfo declaredTypeInfo)
{
    if ( info.IsWritten() ) {
        // put reference on it
        _TRACE("WritePointer: " << NStr::PtrToString(object) <<
               ": @" << info.GetIndex());
        WriteObjectReference(info.GetIndex());
    }
    else {
        // new object
        TTypeInfo realTypeInfo = info.GetTypeInfo();
        if ( declaredTypeInfo == realTypeInfo ) {
            _TRACE("WritePointer: " <<
                   NStr::PtrToString(object) << ": new");
            WriteThis(object, info);
        }
        else {
            _TRACE("WritePointer: " <<
                   NStr::PtrToString(object)
                   << ": new " << realTypeInfo->GetName());
            WriteOther(object, info);
        }
    }
}

void CObjectOStream::WriteSChar(signed char data)
{
    WriteInt(data);
}

void CObjectOStream::WriteUChar(unsigned char data)
{
    WriteUInt(data);
}

void CObjectOStream::WriteShort(short data)
{
    WriteInt(data);
}

void CObjectOStream::WriteUShort(unsigned short data)
{
    WriteUInt(data);
}

void CObjectOStream::WriteInt(int data)
{
    WriteLong(data);
}

void CObjectOStream::WriteUInt(unsigned data)
{
    WriteULong(data);
}

void CObjectOStream::WriteFloat(float data)
{
    WriteDouble(data);
}

void CObjectOStream::WriteCString(const char* str)
{
	WriteString(str);
}

void CObjectOStream::WriteStringStore(const string& str)
{
	WriteString(str);
}

void CObjectOStream::WriteThis(TConstObjectPtr object, CWriteObjectInfo& info)
{
    WriteObject(object, info);
}

void CObjectOStream::WriteNamedType(TTypeInfo /*namedTypeInfo*/,
                                    TTypeInfo typeInfo,
                                    TConstObjectPtr object)
{
    typeInfo->WriteData(*this, object);
}

void CObjectOStream::EndClass(CObjectStackClass& cls)
{
    cls.End();
}

void CObjectOStream::EndClassMember(CObjectStackClassMember& m)
{
    m.End();
}

void CObjectOStream::WriteClass(CObjectClassWriter& writer,
                                const CClassTypeInfo* classInfo,
                                const CMembersInfo& members,
                                bool randomOrder)
{
    CObjectStackClass cls(*this, classInfo, randomOrder);
    BeginClass(cls);
    
    writer.WriteMembers(*this, members);
    
    EndClass(cls);
}

void CObjectOStream::WriteClassMember(CObjectClassWriter& ,
                                      const CMemberId& id,
                                      TTypeInfo memberTypeInfo,
                                      TConstObjectPtr memberPtr)
{
    CObjectStackClassMember m(*this, id);
    BeginClassMember(m, id);

    memberTypeInfo->WriteData(*this, memberPtr);
    
    EndClassMember(m);
}

void CObjectOStream::WriteDelayedClassMember(CObjectClassWriter& ,
                                             const CMemberId& id,
                                             const CDelayBuffer& buffer)
{
    CObjectStackClassMember m(*this, id);
    BeginClassMember(m, id);

    if ( !buffer.Write(*this) )
        THROW1_TRACE(runtime_error, "internal error");
    
    EndClassMember(m);
}

void CObjectOStream::BeginBytes(const ByteBlock& )
{
}

void CObjectOStream::EndBytes(const ByteBlock& )
{
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
    : m_Stream(out), m_RootTypeName(rootTypeName), m_Count(0)
{
    m_AsnIo = AsnIoNew(out.GetAsnFlags() | ASNIO_OUT, 0, this, 0, WriteAsn);
    out.AsnOpen(*this);
}

CObjectOStream::AsnIo::~AsnIo(void)
{
    AsnIoClose(*this);
    GetStream().AsnClose(*this);
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
    THROW1_TRACE(runtime_error, "illegal call");
}
#endif

CObjectArrayWriter::~CObjectArrayWriter(void)
{
}

CObjectClassWriter::~CObjectClassWriter(void)
{
}

END_NCBI_SCOPE
