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
#include <serial/objistr.hpp>
#include <serial/member.hpp>
#include <serial/classinfo.hpp>
#include <serial/typemapper.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

CObjectIStream::CObjectIStream(void)
    : m_Fail(0), m_CurrentMember(0), m_TypeMapper(0)
{
}

CObjectIStream::~CObjectIStream(void)
{
}

unsigned CObjectIStream::SetFailFlags(unsigned flags)
{
    unsigned old = m_Fail;
    m_Fail |= flags;
    return old;
}

void CObjectIStream::ThrowError(EFailFlags fail, const char* message)
{
    SetFailFlags(fail);
    THROW1_TRACE(runtime_error, message);
}

void CObjectIStream::ThrowError(EFailFlags fail, const string& message)
{
    SetFailFlags(fail);
    THROW1_TRACE(runtime_error, message);
}

void CObjectIStream::ThrowError(CNcbiIstream& in)
{
    if ( in.eof() ) {
        ThrowError(eEOF, "unexpected EOF");
    }
    else {
        ThrowError(eReadError, "read error");
    }
}

// root reader
void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    string name = ReadTypeName();
    if ( !name.empty() && name != typeInfo->GetName() )
        THROW1_TRACE(runtime_error, "incompatible type " + name + "<>" + typeInfo->GetName());
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

void CObjectIStream::ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

CObject CObjectIStream::ReadObject(void)
{
    CObject object(MapType(ReadTypeName()));
    ReadExternalObject(object.GetObject(), object.GetTypeInfo());
    return object;
}

string CObjectIStream::ReadTypeName(void)
{
    return NcbiEmptyString;
}

string CObjectIStream::ReadEnumName(void)
{
    return NcbiEmptyString;
}

int CObjectIStream::ReadEnumValue(void)
{
    int value;
    ReadStd(value);
    return value;
}

CTypeMapper::~CTypeMapper(void)
{
}

void CObjectIStream::SetTypeMapper(CTypeMapper* typeMapper)
{
    m_TypeMapper = typeMapper;
}

TTypeInfo CObjectIStream::MapType(const string& name)
{
    if ( m_TypeMapper != 0 )
        return m_TypeMapper->MapType(name);
    return CClassInfoTmpl::GetClassInfoByName(name);
}

TObjectPtr CObjectIStream::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::ReadPointer(" << declaredType->GetName() << ")");
    CObject info;
    switch ( ReadPointerType() ) {
    case eNullPointer:
        _TRACE("CObjectIStreamAsn::ReadPointer: null");
        return 0;
    case eObjectPointer:
        {
            TIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::ReadPointer: @" << index);
            info = GetRegisteredObject(index);
            break;
        }
    case eMemberPointer:
        {
            string memberName = ReadMemberPointer();
            _TRACE("CObjectIStream::ReadPointer: member " << memberName);
            info = ReadObjectInfo();
            ReadMemberPointerEnd();
            CTypeInfo::TMemberIndex index =
                info.GetTypeInfo()->FindMember(memberName);
            if ( index < 0 ) {
                SetFailFlags(eFormatError);
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->GetMemberInfo(index);
            if ( memberInfo->GetTypeInfo() != declaredType ) {
                SetFailFlags(eFormatError);
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
            return memberInfo->GetMember(info.GetObject());
        }
    case eThisPointer:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            TObjectPtr object = declaredType->Create();
            ReadExternalObject(object, declaredType);
            ReadThisPointerEnd();
            return object;
        }
    case eOtherPointer:
        {
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            TObjectPtr object = typeInfo->Create();
            ReadExternalObject(object, typeInfo);
            ReadOtherPointerEnd();
            info = CObject(object, typeInfo);
            break;
        }
    default:
        SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "illegal pointer type");
    }
    while ( HaveMemberSuffix() ) {
        string memberName = ReadMemberSuffix();
        _TRACE("CObjectIStream::ReadPointer: member " << memberName);
        CTypeInfo::TMemberIndex index =
            info.GetTypeInfo()->FindMember(memberName);
        if ( index < 0 ) {
            SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "member not found: " +
                         info.GetTypeInfo()->GetName() + "." + memberName);
        }
        const CMemberInfo* memberInfo =
            info.GetTypeInfo()->GetMemberInfo(index);
        info = CObject(memberInfo->GetMember(info.GetObject()),
                            memberInfo->GetTypeInfo());
    }
    if ( info.GetTypeInfo() != declaredType ) {
        SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "incompatible member type");
    }
    return info.GetObject();
}

CObject CObjectIStream::ReadObjectInfo(void)
{
    _TRACE("CObjectIStream::ReadObjectInfo()");
    switch ( ReadPointerType() ) {
    case eObjectPointer:
        {
            TIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::ReadPointer: @" << index);
            return GetRegisteredObject(index);
        }
    case eMemberPointer:
        {
            string memberName = ReadMemberPointer();
            _TRACE("CObjectIStream::ReadPointer: member " << memberName);
            CObject info = ReadObjectInfo();
            ReadMemberPointerEnd();
            CTypeInfo::TMemberIndex index =
                info.GetTypeInfo()->FindMember(memberName);
            if ( index < 0 ) {
                SetFailFlags(eFormatError);
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->GetMemberInfo(index);
            return CObject(memberInfo->GetMember(info.GetObject()),
                                memberInfo->GetTypeInfo());
        }
    case eOtherPointer:
        {
            const string& className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            TObjectPtr object = typeInfo->Create();
            RegisterObject(object, typeInfo);
            Read(object, typeInfo);
            ReadOtherPointerEnd();
            return CObject(object, typeInfo);
        }
    default:
        SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "illegal pointer type");
    }
}

string CObjectIStream::ReadMemberPointer(void)
{
    SetFailFlags(eIllegalCall);
    THROW1_TRACE(runtime_error, "illegal call");
}

void CObjectIStream::ReadMemberPointerEnd(void)
{
}

void CObjectIStream::ReadThisPointerEnd(void)
{
}

void CObjectIStream::ReadOtherPointerEnd(void)
{
}

bool CObjectIStream::HaveMemberSuffix(void)
{
    return false;
}

string CObjectIStream::ReadMemberSuffix(void)
{
    SetFailFlags(eIllegalCall);
    THROW1_TRACE(runtime_error, "illegal call");
}

void CObjectIStream::SkipValue(void)
{
    SetFailFlags(eIllegalCall);
    THROW1_TRACE(runtime_error, "cannot skip value");
}

void CObjectIStream::FBegin(Block& block)
{
    SetNonFixed(block);
    VBegin(block);
}

void CObjectIStream::VBegin(Block& )
{
}

void CObjectIStream::FNext(const Block& )
{
}

bool CObjectIStream::VNext(const Block& )
{
    return false;
}

void CObjectIStream::FEnd(const Block& )
{
}

void CObjectIStream::VEnd(const Block& )
{
}

CObjectIStream::Member::Member(CObjectIStream& in)
    : m_In(in), m_Previous(in.m_CurrentMember)
{
    in.m_CurrentMember = this;
    in.StartMember(*this);
}

CObjectIStream::Member::Member(CObjectIStream& in, const CMemberId& id)
    : m_In(in), m_Id(id), m_Previous(in.m_CurrentMember)
{
    in.m_CurrentMember = this;
}

CObjectIStream::Member::~Member(void)
{
    if ( m_In.fail() ) {
        string stack = m_Id.ToString();
        for ( const Member* m = m_Previous; m; m = m->m_Previous ) {
            stack = m->m_Id.ToString() + '.' + stack;
        }
        ERR_POST("Error in CObjectIStream: " << stack);
    }
    
    m_In.m_CurrentMember = m_Previous;
    m_In.EndMember(*this);
}

void CObjectIStream::EndMember(const Member& )
{
}

CObjectIStream::Block::Block(CObjectIStream& in)
    : m_In(in), m_Fixed(false), m_RandomOrder(false),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, EFixed )
    : m_In(in), m_Fixed(true), m_RandomOrder(false),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.FBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder)
    : m_In(in), m_Fixed(false), m_RandomOrder(randomOrder),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder , EFixed )
    : m_In(in), m_Fixed(true), m_RandomOrder(randomOrder),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.FBegin(*this);
}

bool CObjectIStream::Block::Next(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            return false;
        }
        m_In.FNext(*this);
    }
    else {
        if ( Finished() ) {
            return false;
        }
        if ( !m_In.VNext(*this) ) {
            m_Finished = true;
            return false;
        }
    }
    IncIndex();
    return true;
}

CObjectIStream::Block::~Block(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            m_In.SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.FEnd(*this);
    }
    else {
        if ( !Finished() ) {
            m_In.SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.VEnd(*this);
    }
}

void CObjectIStream::Begin(ByteBlock& )
{
}

void CObjectIStream::End(const ByteBlock& )
{
}

signed char CObjectIStream::ReadSChar(void)
{
    int data = ReadInt();
    if ( data < SCHAR_MIN || data > SCHAR_MAX )
        ThrowError(eOverflow, "integer overflow");
    return (signed char)data;
}

unsigned char CObjectIStream::ReadUChar(void)
{
    unsigned data = ReadUInt();
    if ( data > UCHAR_MAX )
        ThrowError(eOverflow, "integer overflow");
    return (unsigned char)data;
}

short CObjectIStream::ReadShort(void)
{
    int data = ReadInt();
    if ( data < SHRT_MIN || data > SHRT_MAX )
        ThrowError(eOverflow, "integer overflow");
    return short(data);
}

unsigned short CObjectIStream::ReadUShort(void)
{
    unsigned data = ReadUInt();
    if ( data > USHRT_MAX )
        ThrowError(eOverflow, "integer overflow");
    return (unsigned short)data;
}

int CObjectIStream::ReadInt(void)
{
    long data = ReadLong();
    if ( data < INT_MIN || data > INT_MAX )
        ThrowError(eOverflow, "integer overflow");
    return int(data);
}

unsigned CObjectIStream::ReadUInt(void)
{
    unsigned long data = ReadULong();
    if ( data > UINT_MAX )
        ThrowError(eOverflow, "integer overflow");
    return unsigned(data);
}

float CObjectIStream::ReadFloat(void)
{
    double data = ReadDouble();
    if ( data < FLT_MIN || data > FLT_MAX )
        ThrowError(eOverflow, "float overflow");
    return float(data);
}

char* CObjectIStream::ReadCString(void)
{
	return strdup(ReadString().c_str());
}

CObjectIStream::TIndex CObjectIStream::RegisterObject(TObjectPtr object,
                                                      TTypeInfo typeInfo)
{
    TIndex index = m_Objects.size();
    m_Objects.push_back(CObject(object, typeInfo));
    return index;
}

const CObject& CObjectIStream::GetRegisteredObject(TIndex index) const
{
    if ( index >= m_Objects.size() ) {
        const_cast<CObjectIStream*>(this)->SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "invalid object index");
    }
    return m_Objects[index];
}

extern "C" {
    Int2 LIBCALLBACK ReadAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
    
        return static_cast<CObjectIStream::AsnIo*>(object)->Read(data, length);
    }
}

CObjectIStream::AsnIo::AsnIo(CObjectIStream& in)
    : m_In(in), m_Count(0)
{
#ifdef HAVE_NO_NCBI_LIB
	THROW1_TRACE(runtime_error, "ASN.1 toolkit is not accessible");
#else
    m_AsnIo = AsnIoNew(in.GetAsnFlags() | ASNIO_IN, 0, this, ReadAsn, 0);
    in.AsnOpen(*this);
#endif
}

CObjectIStream::AsnIo::~AsnIo(void)
{
#ifdef HAVE_NO_NCBI_LIB
	THROW1_TRACE(runtime_error, "ASN.1 toolkit is not accessible");
#else
    AsnIoClose(*this);
    m_In.AsnClose(*this);
#endif
}

void CObjectIStream::AsnOpen(AsnIo& )
{
}

void CObjectIStream::AsnClose(AsnIo& asn)
{
    if ( asn.m_Count != 0 ) {
        SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "not all bytes read");
    }
}

unsigned CObjectIStream::GetAsnFlags(void)
{
    return 0;
}

size_t CObjectIStream::AsnRead(AsnIo& , char* , size_t )
{
    SetFailFlags(eIllegalCall);
    THROW1_TRACE(runtime_error, "illegal call");
}

END_NCBI_SCOPE
