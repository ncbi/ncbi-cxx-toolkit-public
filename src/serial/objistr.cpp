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
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.28  1999/09/26 05:19:02  vakatov
* FLT_MIN/MAX are not #define'd on some platforms
*
* Revision 1.27  1999/09/24 18:19:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.26  1999/09/23 21:16:07  vasilche
* Removed dependance on asn.h
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
#include <serial/objistr.hpp>
#include <serial/typeref.hpp>
#include <serial/member.hpp>
#include <serial/classinfo.hpp>
#include <serial/typemapper.hpp>
#include <serial/enumerated.hpp>
#include <serial/memberlist.hpp>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif
#if HAVE_NCBI_C
# include <asn.h>
#endif

#define ALLOW_CYCLES 1
#undef SKIP_NON_CLASS

BEGIN_NCBI_SCOPE

CObjectIStream::CObjectIStream(void)
    : m_Fail(0), m_CurrentElement(0), m_TypeMapper(0)
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

void CObjectIStream::ThrowError1(EFailFlags fail, const char* message)
{
    SetFailFlags(fail);
    throw runtime_error(message);
}

void CObjectIStream::ThrowError1(EFailFlags fail, const string& message)
{
    SetFailFlags(fail);
    throw runtime_error(message);
}

void CObjectIStream::ThrowIOError1(CNcbiIstream& in)
{
    if ( in.eof() ) {
        ThrowError1(eEOF, "unexpected EOF");
    }
    else {
        ThrowError1(eReadError, "read error");
    }
}

void CObjectIStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const char* message)
{
    CNcbiDiag(file, line, eDiag_Trace, eDPF_Trace) << message;
    ThrowError1(fail, message);
}

void CObjectIStream::ThrowError1(const char* file, int line, 
                                 EFailFlags fail, const string& message)
{
    CNcbiDiag(file, line, eDiag_Trace, eDPF_Trace) << message;
    ThrowError1(fail, message);
}

void CObjectIStream::ThrowIOError1(const char* file, int line,
                                   CNcbiIstream& in)
{
    if ( in.eof() ) {
        ThrowError1(file, line, eEOF, "unexpected EOF");
    }
    else {
        ThrowError1(file, line, eReadError, "read error");
    }
}

// root reader
void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    try {
        StackElement m(*this, typeInfo->GetName());
        _TRACE("CObjectIStream::Read(" << NStr::PtrToString(object) << ", "
               << typeInfo->GetName() << ")");
        string name = ReadTypeName();
        if ( !name.empty() && name != typeInfo->GetName() )
            THROW1_TRACE(runtime_error,
                         "incompatible type " + name + "<>" +
                         typeInfo->GetName());
#if ALLOW_CYCLES
        TIndex index = RegisterObject(object, typeInfo);
        if ( index ) {
            _TRACE("CObjectIStream::ReadData(" <<
                   NStr::PtrToString(object) << ", "
                   << typeInfo->GetName() << ") @" << index);
        }
#endif
        ReadData(object, typeInfo);
    }
    catch (...) {
        SetFailFlags(eFail);
        throw;
    }
}

void CObjectIStream::Read(TObjectPtr object, const CTypeRef& type)
{
    Read(object, type.Get());
}

void CObjectIStream::ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << NStr::PtrToString(object) << ", "
           << typeInfo->GetName() << ")");
#if ALLOW_CYCLES
    TIndex index = RegisterObject(object, typeInfo);
    if ( index ) {
        _TRACE("CObjectIStream::ReadData(" << NStr::PtrToString(object) << ", "
               << typeInfo->GetName() << ") @" << index);
    }
#endif
    ReadData(object, typeInfo);
}

CObjectInfo CObjectIStream::ReadObject(void)
{
    try {
        CObjectInfo object(MapType(ReadTypeName()));
        ReadExternalObject(object.GetObject(), object.GetTypeInfo());
        return object;
    }
    catch (...) {
        SetFailFlags(eFail);
        throw;
    }
}

string CObjectIStream::ReadTypeName(void)
{
    return NcbiEmptyString;
}

pair<long, bool> CObjectIStream::ReadEnum(const CEnumeratedTypeValues& values)
{
    if ( values.IsInteger() ) {
        // allow any integer
        return make_pair(0l, false);
    }
    
    // enum element by value
    long value = ReadLong();
    values.FindName(value, false); // check value
    return make_pair(value, true);
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
    try {
        _TRACE("CObjectIStream::ReadPointer(" <<
               declaredType->GetName() << ")");
        CObjectInfo info;
        switch ( ReadPointerType() ) {
        case eNullPointer:
            _TRACE("CObjectIStream::ReadPointer: null");
            return 0;
        case eObjectPointer:
            {
                _TRACE("CObjectIStream::ReadPointer: @...");
                TIndex index = ReadObjectPointer();
                _TRACE("CObjectIStream::ReadPointer: @" << index);
                info = GetRegisteredObject(index);
                break;
            }
        case eMemberPointer:
            {
                _TRACE("CObjectIStream::ReadPointer: member...");
                info = ReadObjectInfo();
                SelectMember(info);
                if ( info.GetTypeInfo() != declaredType ) {
                    SetFailFlags(eFormatError);
                    THROW1_TRACE(runtime_error,
                                 "incompatible member type: " +
                                 info.GetTypeInfo()->GetName() +
                                 " need: " + declaredType->GetName());
                }
                return info.GetObject();
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
                _TRACE("CObjectIStream::ReadPointer: new...");
                string className = ReadOtherPointer();
                StackElement m(*this, className);
                _TRACE("CObjectIStream::ReadPointer: new " << className);
                TTypeInfo typeInfo = MapType(className);
                TObjectPtr object = typeInfo->Create();
                ReadExternalObject(object, typeInfo);
                ReadOtherPointerEnd();
                info = CObjectInfo(object, typeInfo);
                break;
            }
        default:
            SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "illegal pointer type");
        }
        while ( HaveMemberSuffix() ) {
            _TRACE("CObjectIStream::ReadPointer: member...");
            SelectMember(info);
        }
        while ( info.GetTypeInfo() != declaredType ) {
            // try to check parent class pointer
            if ( info.GetTypeInfo()->FindMember(NcbiEmptyString) == 0 ) {
                const CMemberInfo* parent =
                    info.GetTypeInfo()->GetMemberInfo(0);
                info = CObjectInfo(parent->GetMember(info.GetObject()),
                                   parent->GetTypeInfo());
            }
            else {
                SetFailFlags(eFormatError);
                THROW1_TRACE(runtime_error, "incompatible member type: " +
                             info.GetTypeInfo()->GetName() + " need: " +
                             declaredType->GetName());
            }
        }
        return info.GetObject();
    }
    catch (...) {
        SetFailFlags(eFail);
        throw;
    }
}

CObjectInfo CObjectIStream::ReadObjectInfo(void)
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
            _TRACE("CObjectIStream::ReadPointer: member...");
            CObjectInfo info = ReadObjectInfo();
            SelectMember(info);
            return info;
        }
    case eOtherPointer:
        {
            const string& className = ReadOtherPointer();
            StackElement m(*this, className);
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            TObjectPtr object = typeInfo->Create();
#if ALLOW_CYCLES
            RegisterObject(object, typeInfo);
#endif
            Read(object, typeInfo);
            ReadOtherPointerEnd();
            return CObjectInfo(object, typeInfo);
        }
    default:
        break;  // error
    }

    SetFailFlags(eFormatError);
    THROW1_TRACE(runtime_error, "illegal pointer type");
}

void CObjectIStream::SelectMember(CObjectInfo& object)
{
    const CClassInfoTmpl* classInfo =
        dynamic_cast<const CClassInfoTmpl*>(object.GetTypeInfo());
    if ( !classInfo ) {
        SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error,
                     "member of non class: " +
                     object.GetTypeInfo()->GetName());
    }
    TMemberIndex index = ReadMemberSuffix(classInfo->GetMembers());
    const CMemberInfo* memberInfo = classInfo->GetMemberInfo(index);
    object.Set(memberInfo->GetMember(object.GetObject()),
               memberInfo->GetTypeInfo());
}

bool CObjectIStream::HaveMemberSuffix(void)
{
    return false;
}

void CObjectIStream::ReadOtherPointerEnd(void)
{
}

void CObjectIStream::ReadThisPointerEnd(void)
{
}

void CObjectIStream::Skip(TTypeInfo typeInfo)
{
    try {
        StackElement m(*this, typeInfo->GetName());
        _TRACE("CObjectIStream::Skip(" << typeInfo->GetName() << ")");
        string name = ReadTypeName();
        if ( !name.empty() && name != typeInfo->GetName() )
            THROW1_TRACE(runtime_error,
                         "incompatible type " + name + "<>" +
                         typeInfo->GetName());
        SkipData(typeInfo);
    }
    catch (...) {
        SetFailFlags(eFail);
        throw;
    }
}

void CObjectIStream::Skip(const CTypeRef& type)
{
    Skip(type.Get());
}

void CObjectIStream::SkipExternalObject(TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::SkipExternalObject(" << typeInfo->GetName() << ")");
    SkipData(typeInfo);
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

string CObjectIStream::MemberStack(void) const
{
    string stack;
    bool wasMember = false;
    for ( const StackElement* m = m_CurrentElement; m; m = m->GetPrevous() ) {
        string s = m->ToString();
        if ( s.empty() ) {
            if ( !wasMember ) {
                s = "E";
                wasMember = false;
            }
            else {
                wasMember = false;
                continue;
            }
        }
        else {
            wasMember = true;
        }
        if ( stack.empty() )
            stack = s;
        else
            stack = s + '.' + stack;
    }
    return stack;
}

string CObjectIStream::StackElement::ToString(void) const
{
    if ( m_CString )
        return m_CString;
    else if ( m_String )
        return *m_String;
    else if ( m_Id )
        return m_Id->ToString();
    else
        return NcbiEmptyString;
}

CObjectIStream::Member::Member(CObjectIStream& in, const CMembers& members)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, members);
}

CObjectIStream::Member::Member(CObjectIStream& in, LastMember& lastMember)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, lastMember);
}

CObjectIStream::Member::Member(CObjectIStream& in, const CMemberId& member)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, member);
}

CObjectIStream::Member::~Member(void)
{
    if ( GetStream().fail() ) {
        ERR_POST("Error in CObjectIStream: " << GetStream().MemberStack());
        return;
    }
    
    GetStream().EndMember(*this);
}

void CObjectIStream::EndMember(const Member& )
{
}

CObjectIStream::Block::Block(CObjectIStream& in)
    : StackElement(in), m_Fixed(false), m_RandomOrder(false),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(EFixed , CObjectIStream& in)
    : StackElement(in), m_Fixed(true), m_RandomOrder(false),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.FBegin(*this);
    _ASSERT(!Fixed());
}

CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder)
    : StackElement(in), m_Fixed(false), m_RandomOrder(randomOrder),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(EFixed , CObjectIStream& in, bool randomOrder)
    : StackElement(in), m_Fixed(true), m_RandomOrder(randomOrder),
      m_Finished(false), m_Size(0), m_NextIndex(0)
{
    in.FBegin(*this);
    _ASSERT(!Fixed());
}

bool CObjectIStream::Block::Next(void)
{
#if 0
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            return false;
        }
        GetStream().FNext(*this);
    }
    else
#endif
    {
#if 0
        if ( Finished() ) {
            return false;
        }
#endif
        if ( !GetStream().VNext(*this) ) {
#if 0
            m_Finished = true;
#endif
            return false;
        }
    }
    IncIndex();
    return true;
}

CObjectIStream::Block::~Block(void)
{
    if ( GetStream().fail() ) {
        ERR_POST("Error in CObjectIStream: " << GetStream().MemberStack());
        return;
    }
#if 0
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            GetStream().SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        GetStream().FEnd(*this);
    }
    else
#endif
    {
#if 0
        if ( !Finished() ) {
            GetStream().SetFailFlags(eFormatError);
            THROW1_TRACE(runtime_error, "not all elements read");
        }
#endif
        GetStream().VEnd(*this);
    }
}

CObjectIStream::ByteBlock::ByteBlock(CObjectIStream& in)
    : m_In(in), m_KnownLength(false), m_EndOfBlock(false), m_Length(0)
{
    in.Begin(*this);
}

CObjectIStream::ByteBlock::~ByteBlock(void)
{
    if ( m_In.fail() ) {
        ERR_POST("Error in CObjectIStream: " << m_In.MemberStack());
        return;
    }

    if ( KnownLength()? m_Length != 0: !m_EndOfBlock )
        THROW1_TRACE(runtime_error, "not all bytes read");
    m_In.End(*this);
}

size_t CObjectIStream::ByteBlock::Read(void* dst, size_t needLength,
                                       bool forceLength)
{
    size_t length;
    if ( KnownLength() && m_Length < needLength )
        length = m_Length;
    else
        length = needLength;
    
    if ( m_EndOfBlock || length == 0 ) {
        if ( forceLength && needLength != 0 )
            THROW1_TRACE(runtime_error, "read fault");
        return 0;
    }
    
    length = m_In.ReadBytes(*this, static_cast<char*>(dst), length);
    m_Length -= length;
    m_EndOfBlock = length == 0;
    if ( forceLength && needLength != length )
        THROW1_TRACE(runtime_error, "read fault");
    return length;
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
#if defined(FLT_MIN) && defined(FLT_MAX)
    if ( data < FLT_MIN || data > FLT_MAX )
        ThrowError(eOverflow, "float overflow");
#endif
    return float(data);
}

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

void CObjectIStream::SkipUNumber(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipSChar(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUChar(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipShort(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUShort(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipInt(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipUInt(void)
{
    SkipUNumber();
}

void CObjectIStream::SkipLong(void)
{
    SkipSNumber();
}

void CObjectIStream::SkipULong(void)
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

void CObjectIStream::SkipCString(void)
{
    SkipString();
}

void CObjectIStream::SkipStringStore(void)
{
    SkipString();
}

CObjectIStream::TIndex CObjectIStream::RegisterObject(TObjectPtr object,
                                                      TTypeInfo typeInfo)
{
#if SKIP_NON_CLASS
    if ( dynamic_cast<const CClassInfoTmpl*>(typeInfo) == 0 )
        return TIndex(-1);
#endif
#if ALLOW_CYCLES
    TIndex index = m_Objects.size();
    m_Objects.push_back(CObjectInfo(object, typeInfo));
    return index;
#else
    return TIndex(-1);
#endif
}

const CObjectInfo& CObjectIStream::GetRegisteredObject(TIndex index) const
{
#if ALLOW_CYCLES
    if ( index >= m_Objects.size() ) {
        const_cast<CObjectIStream*>(this)->SetFailFlags(eFormatError);
        THROW1_TRACE(runtime_error, "invalid object index");
    }
    return m_Objects[index];
#else
    THROW1_TRACE(runtime_error, "invalid object index: NO_COLLECT defined");
#endif
}

#if HAVE_NCBI_C
extern "C" {
    Int2 LIBCALLBACK ReadAsn(Pointer object, CharPtr data, Uint2 length)
    {
        if ( !object || !data )
            return -1;
    
        return static_cast<CObjectIStream::AsnIo*>(object)->Read(data, length);
    }
}

CObjectIStream::AsnIo::AsnIo(CObjectIStream& in, const string& rootTypeName)
    : m_In(in), m_RootTypeName(rootTypeName), m_Count(0)
{
    m_AsnIo = AsnIoNew(in.GetAsnFlags() | ASNIO_IN, 0, this, ReadAsn, 0);
    in.AsnOpen(*this);
}

CObjectIStream::AsnIo::~AsnIo(void)
{
    AsnIoClose(*this);
    m_In.AsnClose(*this);
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
    SetFailFlags(eIllegalCall);
    THROW1_TRACE(runtime_error, "illegal call");
}
#endif

END_NCBI_SCOPE
