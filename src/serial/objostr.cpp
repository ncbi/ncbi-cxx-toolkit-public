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
#include <serial/objlist.hpp>
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream::~CObjectOStream(void)
{
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::Write(" << unsigned(object) << ", "
           << typeInfo->GetName() << ')');
    _TRACE("CTypeInfo::CollectObjects: " << unsigned(object));
    typeInfo->CollectObjects(m_Objects, object);
    COObjectInfo info(m_Objects, object, typeInfo);
    if ( info.IsMember() ) {
        if ( !info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write member of non written object");
        }
    }
    else {
        if ( info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write already written object");
        }
        m_Objects.RegisterObject(info.GetRootObjectInfo());
        _TRACE("CTypeInfo::Write: " << unsigned(object)
               << " @" << info.GetRootObjectInfo().GetIndex());
    }
    WriteTypeName(typeInfo->GetName());
    WriteData(object, typeInfo);
    m_Objects.CheckAllWritten();
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::RegisterAndWrite(" << unsigned(object) << ", "
           << typeInfo->GetName() << ')');
    COObjectInfo info(m_Objects, object, typeInfo);
    if ( info.IsMember() ) {
        THROW1_TRACE(runtime_error,
                     "trying to register member");
    }
    else {
        if ( info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write already written object");
        }
        m_Objects.RegisterObject(info.GetRootObjectInfo());
        _TRACE("CTypeInfo::Write: " << unsigned(object)
               << " @" << info.GetRootObjectInfo().GetIndex());
    }
    WriteData(object, typeInfo);
}

void CObjectOStream::WriteTypeName(const string& )
{
    // do nothing by default
}

bool CObjectOStream::WriteEnumName(const string& )
{
    return false;
}

void CObjectOStream::WriteEnumValue(int value)
{
    WriteStd(value);
}

void CObjectOStream::WritePointer(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("WritePointer(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    if ( object == 0 ) {
        _TRACE("WritePointer: " << unsigned(object) << ": null");
        WriteNullPointer();
        return;
    }

    COObjectInfo info(m_Objects, object, typeInfo);
    WritePointer(info, typeInfo);
    _ASSERT(!info.IsMember());
}

void CObjectOStream::WritePointer(COObjectInfo& info, TTypeInfo typeInfo)
{
    if ( info.IsMember() ) {
        CMemberId memberId = info.GetMemberId();
        info.ToContainerObject();
        WriteMemberPrefix(memberId);
        WritePointer(info, 0);
        WriteMemberSuffix(memberId);
    }
    else {
        const CORootObjectInfo& root = info.GetRootObjectInfo();
        if ( root.IsWritten() ) {
            // put reference on it
            _TRACE("WritePointer: " << unsigned(info.GetRootObject()) <<
                   ": @" << root.GetIndex());
            WriteObjectReference(root.GetIndex());
        }
        else {
            // new object
            TTypeInfo realTypeInfo = root.GetTypeInfo();
            if ( typeInfo == realTypeInfo ) {
                _TRACE("WritePointer: " << unsigned(info.GetRootObject())
                       << ": new");
                WriteThis(info.GetRootObject(), realTypeInfo);
            }
            else {
                _TRACE("WritePointer: " << unsigned(info.GetRootObject())
                       << ": new " << realTypeInfo->GetName());
                WriteOther(info.GetRootObject(), realTypeInfo);
            }
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

void CObjectOStream::WriteMemberPrefix(const CMemberId& )
{
}

void CObjectOStream::WriteMemberSuffix(const CMemberId& )
{
}

void CObjectOStream::WriteThis(TConstObjectPtr object,
                               TTypeInfo typeInfo)
{
    WriteExternalObject(object, typeInfo);
}

void CObjectOStream::EndMember(const Member& )
{
}

void CObjectOStream::FBegin(Block& block)
{
    SetNonFixed(block);
    VBegin(block);
}

void CObjectOStream::FNext(const Block& )
{
}

void CObjectOStream::FEnd(const Block& )
{
}

void CObjectOStream::VBegin(Block& )
{
}

void CObjectOStream::VNext(const Block& )
{
}

void CObjectOStream::VEnd(const Block& )
{
}

CObjectOStream::Block::Block(CObjectOStream& out)
    : m_Out(out), m_Fixed(false), m_RandomOrder(false),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, unsigned size)
    : m_Out(out), m_Fixed(true), m_RandomOrder(false),
      m_NextIndex(0), m_Size(size)
{
    out.FBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, bool randomOrder)
    : m_Out(out), m_Fixed(false), m_RandomOrder(randomOrder),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, bool randomOrder, unsigned size)
    : m_Out(out), m_Fixed(true), m_RandomOrder(randomOrder),
      m_NextIndex(0), m_Size(size)
{
    out.FBegin(*this);
}

void CObjectOStream::Block::Next(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            THROW1_TRACE(runtime_error, "too many elements");
        }
        m_Out.FNext(*this);
    }
    else {
        m_Out.VNext(*this);
    }
    IncIndex();
}

CObjectOStream::Block::~Block(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            THROW1_TRACE(runtime_error, "not all elements written");
        }
        m_Out.FEnd(*this);
    }
    else {
        m_Out.VEnd(*this);
    }
}

void CObjectOStream::Begin(const ByteBlock& )
{
}

void CObjectOStream::End(const ByteBlock& )
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
#endif

CObjectOStream::AsnIo::AsnIo(CObjectOStream& out)
    : m_Out(out)
{
#if !HAVE_NCBI_C
	THROW1_TRACE(runtime_error, "ASN.1 toolkit is not accessible");
#else
    m_AsnIo = AsnIoNew(out.GetAsnFlags() | ASNIO_OUT, 0, this, 0, WriteAsn);
    out.AsnOpen(*this);
#endif
}

CObjectOStream::AsnIo::~AsnIo(void)
{
#if !HAVE_NCBI_C
	THROW1_TRACE(runtime_error, "ASN.1 toolkit is not accessible");
#else
    AsnIoClose(*this);
    m_Out.AsnClose(*this);
#endif
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

END_NCBI_SCOPE
