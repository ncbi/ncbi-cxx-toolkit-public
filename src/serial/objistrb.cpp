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
* Revision 1.5  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:02  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:53  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrb.hpp>
#include <serial/objstrb.hpp>
#include <serial/classinfo.hpp>

BEGIN_NCBI_SCOPE


CObjectIStreamBinary::CObjectIStreamBinary(CNcbiIstream& in)
    : m_Input(in)
{
}

unsigned char CObjectIStreamBinary::ReadByte(void)
{
    char c;
    if ( !m_Input.get(c) ) {
        THROW1_TRACE(runtime_error, "unexpected EOF");
    }
    return c;
}

CTypeInfo::TTypeInfo CObjectIStreamBinary::ReadTypeInfo(void)
{
    throw runtime_error("not implemented");
/*
    TByte code = ReadByte();


    switch ( code ) {
    case CObjectStreamBinaryDefs::eClassReference:
        return GetRegiteredClass(ReadId());
    case CObjectStreamBinaryDefs::eClass:
        return RegisterClass(CTypeInfo::GetTypeInfoByName(ReadString()));
    case CObjectStreamBinaryDefs::eTemplate:
        //        return RegisterClass(CTemplateResolver::Resolve(*this));
    default:
        THROW1_TRACE(runtime_error, "undefined type: ");
    }
*/
}

class CDataReader
{
public:
    typedef CObjectIStreamBinary::TByte TByte;

    static string binary(TByte byte);

    virtual void ReadStd(char& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: char");
        }
    virtual void ReadStd(unsigned char& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: unsigned char");
        }
    virtual void ReadStd(signed char& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: signed char");
        }
    virtual void ReadStd(short& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: short");
        }
    virtual void ReadStd(unsigned short& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: unsigned short");
        }
    virtual void ReadStd(int& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: int");
        }
    virtual void ReadStd(unsigned int& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: unsigned int");
        }
    virtual void ReadStd(long& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: signed long");
        }
    virtual void ReadStd(unsigned long& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: unsigned long");
        }
    virtual void ReadStd(float& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: float");
        }
    virtual void ReadStd(double& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: double");
        }
    virtual void ReadStd(string& ) const
        {
            THROW1_TRACE(runtime_error, "incompatible type: string");
        }

    TByte ReadByte(void) const
        {
            return m_Input.ReadByte();
        }

    CObjectIStreamBinary& m_Input;
};

string CDataReader::binary(TByte byte)
{
    string s = "0b";
    for ( unsigned bit = 0x80; bit; bit >>= 1 ) {
        s += (bit & byte)? '1': '0';
    }
    return s;
}

class CNumberReader : public CDataReader
{
public:
    static runtime_error overflow_error(const type_info& type, bool sign,
                                        const string& bytes);

    static runtime_error overflow_error(const type_info& type, bool sign,
                                        TByte c0)
        {
            return overflow_error(type, sign, binary(c0));
        }
    static runtime_error overflow_error(const type_info& type, bool sign,
                                        TByte c0, TByte c1)
        {
            return overflow_error(type, sign, binary(c0) + ' ' + binary(c1));
        }
    static runtime_error overflow_error(const type_info& type, bool sign,
                                       TByte c0, TByte c1, TByte c2)
        {
            return overflow_error(type, sign, binary(c0) + ' ' + binary(c1) +
                                  ' ' + binary(c2));
        }
    static runtime_error overflow_error(const type_info& type, bool sign,
                                       TByte c0, TByte c1, TByte c2, TByte c3)
        {
            return overflow_error(type, sign, binary(c0) + ' ' + binary(c1) +
                                  ' ' + binary(c2) + ' ' + binary(c3));
        }

    static bool isSignExpansion(TByte signByte, TByte highByte)
        {
            return (signByte + (highByte >> 7)) == 0;
        }
};

runtime_error CNumberReader::overflow_error(const type_info& type, bool sign, 
                                            const string& bytes)
{
    ERR_POST((sign? "": "un") << "signed number too big to fit in " <<
             type.name() << " bytes: " << bytes);
    return runtime_error((sign? "": "un") +
                         string("signed number too big to fit in ") +
                         type.name() + " bytes: " + bytes);
}

template<class TYPE>
class CStdDataReader : public CNumberReader
{
public:
    static bool IsSigned(void)
        { return TYPE(-1) < TYPE(0); }

    static TByte checkSign(TByte code, TByte highBit, bool sign)
        {
            // check if signed number is returned as unsigned
            if ( !IsSigned() &&
                 sign && (code & highBit) ) {
                throw overflow_error(typeid(TYPE), sign, code);
            }
            // mask data bits
            code &= (highBit << 1) - 1;
            if ( IsSigned() && sign ) {
                // extend sign
                return (code ^ highBit) - highBit;
            }
            else {
                return code;
            }
        }

    static TYPE ReadNumber(CObjectIStreamBinary& in, bool sign);

    static void ReadStdNumber(CObjectIStreamBinary& in, TYPE& data)
        {
            CObjectIStreamBinary::TByte code = in.ReadByte();
            switch ( code ) {
            case CObjectStreamBinaryDefs::eNull:
                data = 0;
                return;
            case CObjectStreamBinaryDefs::eStd_sbyte:
                if ( IsSigned() ) {
                    data = ReadNumber(in, true);
                }
                return;
            case CObjectStreamBinaryDefs::eStd_ubyte:
                data = ReadNumber(in, false);
                return;
            case CObjectStreamBinaryDefs::eStd_sordinal:
                data = ReadNumber(in, true);
                return;
            case CObjectStreamBinaryDefs::eStd_uordinal:
                data = ReadNumber(in, false);
                return;
            default:
                throw runtime_error("bad number code: " + binary(code));
            }
        }
};

template<class TYPE>
TYPE CStdDataReader<TYPE>::ReadNumber(CObjectIStreamBinary& in, bool sign)
{
    bool incompatibleSign = !IsSigned() && sign;
    bool expandSign = IsSigned() && sign;
    TByte code = in.ReadByte();
    if ( !(code & 0x80) ) {
        // one byte: 0xxxxxxx
        return TYPE(checkSign(code, 0x40, sign));
    }
    else if ( !(code & 0x40) ) {
        // two bytes: 10xxxxxx xxxxxxxx
        TByte c0 = in.ReadByte();
        TByte c1 = checkSign(code, 0x20, sign);
        if ( sizeof(TYPE) == 1 ) {
            // check for byte fit
            if ( !IsSigned() && (c1) ||
                 IsSigned() && !(isSignExpansion(c1, c0)) )
                throw overflow_error(typeid(TYPE), sign, code, c0);
            return TYPE(c0);
        }
        else {
            return TYPE((c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x20) ) {
        // three bytes: 110xxxxx xxxxxxxx xxxxxxxx
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        TByte c2 = checkSign(code, 0x10, sign);
        if ( sizeof(TYPE) == 1 ) {
            // check for byte fit
            if ( !IsSigned() && (c2 || c1) ||
                 IsSigned() && !(isSignExpansion(c2, c0) &&
                                 isSignExpansion(c1, c0)) )
                throw overflow_error(typeid(TYPE), sign, code, c1, c0);
            return TYPE(c0);
        }
        else if ( sizeof(TYPE) == 2 ) {
            // check for two byte fit
            if ( !IsSigned() && (c2) ||
                 IsSigned() && !(isSignExpansion(c2, c1)) )
                throw overflow_error(typeid(TYPE), sign, code, c1, c0);
            return TYPE((c1 << 8) | c0);
        }
        else {
            return TYPE((TYPE(c2) << 16) | (c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x10) ) {
        // four bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
        TByte c2 = in.ReadByte();
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        TByte c3 = checkSign(code, 0x08, sign);
        if ( sizeof(TYPE) == 1 ) {
            // check for byte fit
            if ( !IsSigned() && (c3 || c2 || c1) ||
                 IsSigned() && !(isSignExpansion(c3, c0) &&
                                 isSignExpansion(c2, c0) &&
                                 isSignExpansion(c1, c0)) )
                throw overflow_error(typeid(TYPE), sign, code, c2, c1, c0);
            return TYPE(c0);
        }
        else if ( sizeof(TYPE) == 2 ) {
            // check for two byte fit
            if ( !IsSigned() && (c3 || c2) ||
                 IsSigned() && !(isSignExpansion(c3, c1) &&
                                 isSignExpansion(c2, c1)) )
                throw overflow_error(typeid(TYPE), sign, code, c2, c1, c0);
            return TYPE((c1 << 8) | c0);
        }
        return TYPE((TYPE(c3) << 24) | (TYPE(c2) << 16) | (c1 << 8) | c0);
    }
    else {
        // 1111xxxx (xxxxxxxx)*
        size_t size = (code & 0xF) + 4;
        if ( size > 16 ) {
            THROW1_TRACE(runtime_error, "reserved number code: " + binary(code));
        }
        // skip high bytes
        while ( size > sizeof(TYPE) ) {
            TByte c = in.ReadByte();
            if ( c ) {
                throw overflow_error(typeid(TYPE), sign,
                                     binary(code) + " ... " + binary(c));
            }
            --size;
        }
        // read number
        TYPE n = 0;
        while ( size-- ) {
            n = (n << 8) | in.ReadByte();
        }
        return n;
    }
}

void CObjectIStreamBinary::ReadStd(char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_char:
        data = ReadByte();
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(signed char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        data = ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_ubyte:
        if ( (data = ReadByte()) & 0x80 ) {
            // unsigned -> signed overflow
            THROW1_TRACE(runtime_error,
                         "unsigned char doesn't fit in signed char");
        }
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(unsigned char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_ubyte:
        data = ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        if ( (data = ReadByte()) & 0x80 ) {
            // signed -> unsigned overflow
            THROW1_TRACE(runtime_error,
                         "signed char doesn't fit in unsigned char");
        }
        return;
    default:
        THROW1_TRACE(runtime_error, 
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(short& data)
{
    CStdDataReader<short>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned short& data)
{
    CStdDataReader<unsigned short>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(int& data)
{
    CStdDataReader<int>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned int& data)
{
    CStdDataReader<unsigned int>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(long& data)
{
    CStdDataReader<long>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned long& data)
{
    CStdDataReader<unsigned long>::ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(float& )
{
    throw runtime_error("float is not supported");
}

void CObjectIStreamBinary::ReadStd(double& )
{
    throw runtime_error("double is not supported");
}

void CObjectIStreamBinary::ReadStd(string& data)
{
    data = ReadString();
}

CObjectIStreamBinary::TIndex CObjectIStreamBinary::ReadIndex(void)
{
    return CStdDataReader<TIndex>::ReadNumber(*this, false);
}

unsigned CObjectIStreamBinary::ReadSize(void)
{
    return CStdDataReader<unsigned>::ReadNumber(*this, false);
}

const string& CObjectIStreamBinary::ReadString(void)
{
    TIndex index = ReadIndex();
    if ( index == m_Strings.size() ) {
        // new string
        m_Strings.push_back(string());
        string& s = m_Strings.back();
        unsigned length = ReadSize();
        s.reserve(length);
        for ( unsigned i = 0; i < length; ++i )
            s += char(ReadByte());
        return s;
    }
    else if ( index < m_Strings.size() ) {
        return m_Strings[index];
    }
    else {
        ERR_POST(index);
        THROW1_TRACE(runtime_error,
                     "invalid string index: " + NStr::UIntToString(index));
    }
}

unsigned CObjectIStreamBinary::Begin(Block& , bool )
{
    if ( ReadByte() != CObjectStreamBinaryDefs::eBlock )
        THROW1_TRACE(runtime_error, "bad block start byte");
    return ReadSize();
}

bool CObjectIStreamBinary::ReadNextMember(void)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eMember:
        return true;
    case CObjectStreamBinaryDefs::eEndOfMembers:
        return false;
    default:
        THROW1_TRACE(runtime_error, "invalid member type");
    }
}

CTypeInfo::TObjectPtr CObjectIStreamBinary::ReadPointer(TTypeInfo declaredType)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eNull:
        return 0;
    case CObjectStreamBinaryDefs::eMember:
        {
            const string& memberName = ReadId();
            CIObjectInfo info = ReadObjectPointer();
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->FindMember(memberName);
            if ( memberInfo == 0 ) {
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            if ( memberInfo->GetTypeInfo() != declaredType ) {
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
            return memberInfo->GetMember(info.GetObject());
        }
    case CObjectStreamBinaryDefs::eObjectReference:
        {
            const CIObjectInfo& info = GetRegisteredObject(ReadIndex());
            if ( info.GetTypeInfo() != declaredType ) {
                THROW1_TRACE(runtime_error, "incompatible object type");
            }
            return info.GetObject();
        }
    case CObjectStreamBinaryDefs::eThisClass:
        {
            TObjectPtr object = declaredType->Create();
            RegisterObject(object, declaredType);
            Read(object, declaredType);
            return object;
        }
    default:
        THROW1_TRACE(runtime_error, "invalid object reference code");
    }
}

CIObjectInfo CObjectIStreamBinary::ReadObjectPointer(void)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eMember:
        {
            const string& memberName = ReadId();
            CIObjectInfo info = ReadObjectPointer();
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->FindMember(memberName);
            if ( memberInfo == 0 ) {
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            return CIObjectInfo(memberInfo->GetMember(info.GetObject()),
                                memberInfo->GetTypeInfo());
        }
    case CObjectStreamBinaryDefs::eObjectReference:
        return GetRegisteredObject(ReadIndex());
    case CObjectStreamBinaryDefs::eOtherClass:
        {
            TTypeInfo typeInfo = CTypeInfo::GetTypeInfoByName(ReadId());
            TObjectPtr object = typeInfo->Create();
            RegisterObject(object, typeInfo);
            Read(object, typeInfo);
            return CIObjectInfo(object, typeInfo);
        }
    default:
        THROW1_TRACE(runtime_error, "invalid object reference code");
    }
}

void CObjectIStreamBinary::ReadObject(TObjectPtr object,
                                      const CClassInfoTmpl* classInfo)
{
    _TRACE("CObjectIStreamBinary::ReadObject(" << unsigned(object) << ", "
           << classInfo->GetName() << ')');
    while ( ReadNextMember() ) {
        const string& memberName = ReadId();
        const CMemberInfo* memberInfo = classInfo->FindMember(memberName);
        if ( memberInfo == 0 ) {
            ERR_POST("unknown member: "+memberName+", trying to skip...");
            SkipValue();
        }
        else {
            TObjectPtr member = memberInfo->GetMember(object);
            Read(member, memberInfo->GetTypeInfo());
        }
    }
}

void CObjectIStreamBinary::SkipValue()
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eNull:
        return;
    case CObjectStreamBinaryDefs::eStd_char:
    case CObjectStreamBinaryDefs::eStd_ubyte:
    case CObjectStreamBinaryDefs::eStd_sbyte:
        ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_sordinal:
        CStdDataReader<long>::ReadNumber(*this, true);
        return;
    case CObjectStreamBinaryDefs::eStd_uordinal:
        CStdDataReader<long>::ReadNumber(*this, false);
        return;
    case CObjectStreamBinaryDefs::eStd_float:
        THROW1_TRACE(runtime_error, "floating point numbers are not supported");
        return;
    case CObjectStreamBinaryDefs::eObjectReference:
        ReadIndex();
        return;
    case CObjectStreamBinaryDefs::eThisClass:
        RegisterInvalidObject();
        SkipObjectData();
        return;
    case CObjectStreamBinaryDefs::eOtherClass:
        ReadId();
        RegisterInvalidObject();
        SkipObjectData();
        return;
    case CObjectStreamBinaryDefs::eMember:
        ReadId();
        SkipObjectPointer();
        return;
    case CObjectStreamBinaryDefs::eBlock:
        SkipBlock();
        return;
    default:
        THROW1_TRACE(runtime_error, "invalid value code");
    }
}

void CObjectIStreamBinary::SkipObjectPointer(void)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eObjectReference:
        ReadIndex();
        return;
    case CObjectStreamBinaryDefs::eOtherClass:
        ReadId();
        RegisterInvalidObject();
        SkipObjectData();
        return;
    case CObjectStreamBinaryDefs::eMember:
        ReadId();
        SkipObjectPointer();
        return;
    default:
        THROW1_TRACE(runtime_error, "invalid value code");
    }
}

void CObjectIStreamBinary::SkipObjectData(void)
{
    while ( ReadNextMember() ) {
        ReadId();
        SkipValue();
    }
}

void CObjectIStreamBinary::SkipBlock(void)
{
    unsigned count = ReadSize();
    for ( unsigned i = 0; i < count; ++i )
        SkipValue();
}

END_NCBI_SCOPE
