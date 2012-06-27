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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <serial/objostrasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/error_codes.hpp>

#include <stdio.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Serial_OStream

BEGIN_NCBI_SCOPE

CObjectOStream* CObjectOStream::OpenObjectOStreamAsn(CNcbiOstream& out,
                                                     bool deleteOut)
{
    return new CObjectOStreamAsn(out, deleteOut);
}

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out,
                                     EFixNonPrint how)
    : CObjectOStream(eSerial_AsnText, out), m_FixMethod(how)
{
    m_Output.SetBackLimit(80);
    SetSeparator("\n");
    SetAutoSeparator(true);
}

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out,
                                     bool deleteOut,
                                     EFixNonPrint how)
    : CObjectOStream(eSerial_AsnText, out, deleteOut), m_FixMethod(how)
{
    m_Output.SetBackLimit(80);
    SetSeparator("\n");
    SetAutoSeparator(true);
}

CObjectOStreamAsn::~CObjectOStreamAsn(void)
{
}

string CObjectOStreamAsn::GetPosition(void) const
{
    return "line "+NStr::SizetToString(m_Output.GetLine());
}

void CObjectOStreamAsn::WriteFileHeader(TTypeInfo type)
{
    if ( true || m_Output.ZeroIndentLevel() ) {
        WriteId(type->GetName());
        m_Output.PutString(" ::= ");
    }
}

void CObjectOStreamAsn::WriteEnum(
    const CEnumeratedTypeValues& values,
    TEnumValueType value, const string& valueName)
{
    if (valueName.empty() || (m_WriteNamedIntegersByValue && values.IsInteger())) {
        m_Output.PutInt4(value);
    } else {
        m_Output.PutString(valueName);
    }    
}

void CObjectOStreamAsn::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value)
{
    WriteEnum(values,value,values.FindName(value, values.IsInteger()));
}

void CObjectOStreamAsn::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamAsn::WriteBool(bool data)
{
    if ( data )
        m_Output.PutString("TRUE");
    else
        m_Output.PutString("FALSE");
}

void CObjectOStreamAsn::WriteChar(char data)
{
    m_Output.PutChar('\'');
    m_Output.PutChar(data);
    m_Output.PutChar('\'');
}

void CObjectOStreamAsn::WriteInt4(Int4 data)
{
    m_Output.PutInt4(data);
}

void CObjectOStreamAsn::WriteUint4(Uint4 data)
{
    m_Output.PutUint4(data);
}

void CObjectOStreamAsn::WriteInt8(Int8 data)
{
    m_Output.PutInt8(data);
}

void CObjectOStreamAsn::WriteUint8(Uint8 data)
{
    m_Output.PutUint8(data);
}

void CObjectOStreamAsn::WriteDouble2(double data, size_t digits)
{
    if (isnan(data)) {
        ThrowError(fInvalidData, "invalid double: not a number");
    }
    if (!finite(data)) {
        ThrowError(fInvalidData, "invalid double: infinite");
    }
    if ( data == 0.0 ) {
        m_Output.PutString("{ 0, 10, 0 }");
        return;
    }

    char buffer[128];
    if (m_FastWriteDouble) {
        int dec, sign;
        size_t len = NStr::DoubleToString_Ecvt(
            data, digits, buffer, sizeof(buffer), &dec, &sign);
        _ASSERT(len > 0);
        m_Output.PutString("{ ");
        if (sign < 0) {
            m_Output.PutString("-");
        }
        m_Output.PutString(buffer,len);
        m_Output.PutString(", 10, ");
        m_Output.PutInt4(dec - (int)(len-1));
        m_Output.PutString(" }");
        
    } else {
        // ensure buffer is large enough to fit result
        // (additional bytes are for sign, dot and exponent)
        _ASSERT(sizeof(buffer) > digits + 16);
        int width = sprintf(buffer, "%.*e", int(digits-1), data);
        if ( width <= 0 || width >= int(sizeof(buffer) - 1) )
            ThrowError(fOverflow, "buffer overflow");
        _ASSERT(int(strlen(buffer)) == width);
        char* dotPos = strchr(buffer, '.');
        if (!dotPos) {
            dotPos = strchr(buffer, ','); // non-C locale?
        }
        _ASSERT(dotPos);
        char* ePos = strchr(dotPos, 'e');
        _ASSERT(ePos);

        // now we have:
        // mantissa with dot - buffer:ePos
        // exponent - (ePos+1):

        int exp;
        // calculate exponent
        if ( sscanf(ePos + 1, "%d", &exp) != 1 )
            ThrowError(fInvalidData, "double value conversion error");

        // remove trailing zeroes
        int fractDigits = int(ePos - dotPos - 1);
        while ( fractDigits > 0 && ePos[-1] == '0' ) {
            --ePos;
            --fractDigits;
        }

        // now we have:
        // mantissa with dot without trailing zeroes - buffer:ePos

        m_Output.PutString("{ ");
        m_Output.PutString(buffer, dotPos - buffer);
        m_Output.PutString(dotPos + 1, fractDigits);
        m_Output.PutString(", 10, ");
        m_Output.PutInt4(exp - fractDigits);
        m_Output.PutString(" }");
    }
}

void CObjectOStreamAsn::WriteDouble(double data)
{
    WriteDouble2(data, DBL_DIG);
}

void CObjectOStreamAsn::WriteFloat(float data)
{
    WriteDouble2(data, FLT_DIG);
}

void CObjectOStreamAsn::WriteNull(void)
{
    m_Output.PutString("NULL");
}

void CObjectOStreamAsn::WriteAnyContentObject(const CAnyContentObject& )
{
    ThrowError(fNotImplemented,
        "CObjectOStreamAsn::WriteAnyContentObject: "
        "unable to write AnyContent object in ASN");
}

void CObjectOStreamAsn::CopyAnyContentObject(CObjectIStream& )
{
    ThrowError(fNotImplemented,
        "CObjectOStreamAsn::CopyAnyContentObject: "
        "unable to copy AnyContent object in ASN");
}

void CObjectOStreamAsn::WriteBitString(const CBitString& obj)
{
    static const char ToHex[] = "0123456789ABCDEF";
    bool hex = obj.size()%8 == 0;
    m_Output.PutChar('\'');
#if BITSTRING_AS_VECTOR
// CBitString is vector<bool>
    if (hex) {
        Uint1 data, mask;
        for ( CBitString::const_iterator i = obj.begin(); i != obj.end(); ) {
            for (data=0, mask=0x8; mask!=0; mask >>= 1, ++i) {
                if (*i) {
                    data |= mask;
                }
            }
            m_Output.WrapAt(78, false);
            m_Output.PutChar(ToHex[data]);
        }
    } else {
        ITERATE ( CBitString, i, obj) {
            m_Output.WrapAt(78, false);
            m_Output.PutChar( *i ? '1' : '0');
        }
    }
#else
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        bm::word_t* tmp_block = obj.allocate_tempblock();
        CBitString::statistics st;
        obj.calc_stat(&st);
        char* buf = (char*)malloc(st.max_serialize_mem);
        unsigned int len = bm::serialize(obj, (unsigned char*)buf, tmp_block);
        WriteBytes(buf,len);
        free(buf);
        free(tmp_block);
        hex = true;
    } else {
        CBitString::size_type i=0;
        CBitString::size_type ilast = obj.size();
        CBitString::enumerator e = obj.first();
        if (hex) {
            Uint1 data, mask;
            while (i < ilast) {
                for (data=0, mask=0x8; mask!=0; mask >>= 1, ++i) {
                    if (i == *e) {
                        data |= mask;
                        ++e;
                    }
                }
                m_Output.WrapAt(78, false);
                m_Output.PutChar(ToHex[data]);
            }
        } else {
            for (; i < ilast; ++i) {
                m_Output.WrapAt(78, false);
                m_Output.PutChar( (i == *e) ? '1' : '0');
                if (i == *e) {
                    ++e;
                }
            }
        }
    }
#endif
    m_Output.PutChar('\'');
    m_Output.PutChar(hex ? 'H' : 'B');
}

void CObjectOStreamAsn::CopyBitString(CObjectIStream& in)
{
    CBitString obj;
    in.ReadBitString(obj);
    WriteBitString(obj);
}

void CObjectOStreamAsn::WriteString(const char* ptr, size_t length)
{
    m_Output.PutChar('"');
    while ( length > 0 ) {
        char c = *ptr++;
        if ( m_FixMethod != eFNP_Allow ) {
            if ( !GoodVisibleChar(c) ) {
                FixVisibleChar(c, m_FixMethod, this, string(ptr,length));
            }
        }
        --length;
        m_Output.WrapAt(78, true);
        m_Output.PutChar(c);
        if ( c == '"' )
            m_Output.PutChar('"');
    }
    m_Output.PutChar('"');
}

void CObjectOStreamAsn::WriteCString(const char* str)
{
    if ( str == 0 ) {
        WriteNull();
    }
    else {
        WriteString(str, strlen(str));
    }
}

void CObjectOStreamAsn::WriteString(const string& str, EStringType type)
{
    EFixNonPrint fix = m_FixMethod;
    if (type == eStringTypeUTF8) {
        m_FixMethod = eFNP_Allow;
    }
    WriteString(str.data(), str.size());
    m_FixMethod = fix;
}

void CObjectOStreamAsn::WriteStringStore(const string& str)
{
    WriteString(str.data(), str.size());
}

void CObjectOStreamAsn::CopyString(CObjectIStream& in,
                                   EStringType type)
{
    string s;
    in.ReadString(s, type);
    WriteString(s, type);
}

void CObjectOStreamAsn::CopyStringStore(CObjectIStream& in)
{
    string s;
    in.ReadStringStore(s);
    WriteString(s.data(), s.size());
}

void CObjectOStreamAsn::WriteId(const string& str)
{
    if ( str.find(' ') != NPOS || str.find('<') != NPOS ||
         str.find(':') != NPOS ) {
        m_Output.PutChar('[');
        m_Output.PutString(str);
        m_Output.PutChar(']');
    } else {
        m_Output.PutString(str);
    }
}

void CObjectOStreamAsn::WriteNullPointer(void)
{
    m_Output.PutString("NULL");
}

void CObjectOStreamAsn::WriteObjectReference(TObjectIndex index)
{
    m_Output.PutChar('@');
    if ( sizeof(TObjectIndex) == sizeof(Int4) )
        m_Output.PutInt4(Int4(index));
    else if ( sizeof(TObjectIndex) == sizeof(Int8) )
        m_Output.PutInt8(index);
    else
        ThrowError(fIllegalCall, "invalid size of TObjectIndex: "
            "must be either sizeof(Int4) or sizeof(Int8)");
}

void CObjectOStreamAsn::WriteOtherBegin(TTypeInfo typeInfo)
{
    m_Output.PutString(": ");
    WriteId(typeInfo->GetName());
    m_Output.PutChar(' ');
}

void CObjectOStreamAsn::WriteOther(TConstObjectPtr object,
                                   TTypeInfo typeInfo)
{
    m_Output.PutString(": ");
    WriteId(typeInfo->GetName());
    m_Output.PutChar(' ');
    WriteObject(object, typeInfo);
}

void CObjectOStreamAsn::StartBlock(void)
{
    m_Output.PutChar('{');
    m_Output.IncIndentLevel();
    m_BlockStart = true;
}

void CObjectOStreamAsn::EndBlock(void)
{
    m_Output.DecIndentLevel();
    m_Output.PutEol();
    m_Output.PutChar('}');
    m_BlockStart = false;
}

void CObjectOStreamAsn::NextElement(void)
{
    if ( m_BlockStart )
        m_BlockStart = false;
    else
        m_Output.PutChar(',');
    m_Output.PutEol();
}

void CObjectOStreamAsn::BeginContainer(const CContainerTypeInfo* /*cType*/)
{
    StartBlock();
}

void CObjectOStreamAsn::EndContainer(void)
{
    EndBlock();
}

void CObjectOStreamAsn::BeginContainerElement(TTypeInfo /*elementType*/)
{
    NextElement();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsn::WriteContainer(const CContainerTypeInfo* cType,
                                       TConstObjectPtr containerPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    StartBlock();
    
    CContainerTypeInfo::CConstIterator i;
    if ( cType->InitIterator(i, containerPtr) ) {
        TTypeInfo elementType = cType->GetElementType();
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        do {
            if (elementType->GetTypeFamily() == eTypeFamilyPointer) {
                const CPointerTypeInfo* pointerType =
                    CTypeConverter<CPointerTypeInfo>::SafeCast(elementType);
                _ASSERT(pointerType->GetObjectPointer(cType->GetElementPtr(i)));
                if ( !pointerType->GetObjectPointer(cType->GetElementPtr(i)) ) {
                    ERR_POST_X(8, Warning << " NULL pointer found in container: skipping");
                    continue;
                }
            }

            NextElement();

            WriteObject(cType->GetElementPtr(i), elementType);

        } while ( cType->NextElement(i) );
        
        END_OBJECT_FRAME();
    }

    EndBlock();
    END_OBJECT_FRAME();
}

void CObjectOStreamAsn::CopyContainer(const CContainerTypeInfo* cType,
                                      CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameArray, cType);
    copier.In().BeginContainer(cType);

    StartBlock();

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArrayElement, elementType);

    while ( copier.In().BeginContainerElement(elementType) ) {
        NextElement();

        CopyObject(elementType, copier);

        copier.In().EndContainerElement();
    }

    END_OBJECT_2FRAMES_OF(copier);
    
    EndBlock();

    copier.In().EndContainer();
    END_OBJECT_FRAME_OF(copier.In());
}
#endif


void CObjectOStreamAsn::WriteMemberId(const CMemberId& id)
{
    const string& name = id.GetName();
    if ( !name.empty() ) {
        if (id.HaveNoPrefix() && isupper((unsigned char)name[0])) {
            m_Output.PutChar(tolower((unsigned char)name[0]));
            m_Output.PutString(name.data()+1, name.size()-1);
        } else {
            m_Output.PutString(name);
        }
        m_Output.PutChar(' ');
    }
    else if ( id.HaveExplicitTag() ) {
        m_Output.PutString("[" + NStr::IntToString(id.GetTag()) + "] ");
    }
}

void CObjectOStreamAsn::BeginClass(const CClassTypeInfo* /*classInfo*/)
{
    StartBlock();
}

void CObjectOStreamAsn::EndClass(void)
{
    EndBlock();
}

void CObjectOStreamAsn::BeginClassMember(const CMemberId& id)
{
    NextElement();

    WriteMemberId(id);
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsn::WriteClass(const CClassTypeInfo* classType,
                                   TConstObjectPtr classPtr)
{
    StartBlock();
    
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        classType->GetMemberInfo(*i)->WriteMember(*this, classPtr);
    }
    
    EndBlock();
}

void CObjectOStreamAsn::WriteClassMember(const CMemberId& memberId,
                                         TTypeInfo memberType,
                                         TConstObjectPtr memberPtr)
{
    NextElement();

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    
    WriteMemberId(memberId);
    
    WriteObject(memberPtr, memberType);

    END_OBJECT_FRAME();
}

bool CObjectOStreamAsn::WriteClassMember(const CMemberId& memberId,
                                         const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(eSerial_AsnText) )
        return false;

    NextElement();

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    
    WriteMemberId(memberId);
    
    Write(buffer.GetSource());

    END_OBJECT_FRAME();

    return true;
}

void CObjectOStreamAsn::CopyClassRandom(const CClassTypeInfo* classType,
                                        CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);

    StartBlock();

    vector<Uint1> read(classType->GetMembers().LastIndex() + 1);

    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());

        if ( read[index] ) {
            copier.DuplicatedMember(memberInfo);
        }
        else {
            read[index] = true;

            NextElement();
            WriteMemberId(memberInfo->GetId());

            memberInfo->CopyMember(copier);
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

    EndBlock();

    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
}

void CObjectOStreamAsn::CopyClassSequential(const CClassTypeInfo* classType,
                                            CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);

    StartBlock();

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

        NextElement();
        WriteMemberId(memberInfo->GetId());
        
        memberInfo->CopyMember(copier);
        
        pos.SetIndex(index + 1);

        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( ; pos.Valid(); ++pos ) {
        classType->GetMemberInfo(*pos)->CopyMissingMember(copier);
    }

    EndBlock();

    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
}
#endif

void CObjectOStreamAsn::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    if (choiceType->GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist()) {
        TopFrame().SetNotag();
        StartBlock();
    }
    m_BlockStart = true;
}
void CObjectOStreamAsn::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        EndBlock();
    }
    m_BlockStart = false;
}

void CObjectOStreamAsn::BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                           const CMemberId& id)
{
    if ( m_BlockStart ) {
        m_BlockStart = false;
    } else {
        NextElement();
        WriteId(choiceType->GetName());
        m_Output.PutChar(' ');
    }
    WriteMemberId(id);
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsn::WriteChoice(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    WriteMemberId(variantInfo->GetId());
    
    variantInfo->WriteVariant(*this, choicePtr);

    END_OBJECT_FRAME();
}

/*
void CObjectOStreamAsn::CopyChoice(const CChoiceTypeInfo* choiceType,
                                   CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameChoice, choiceType);
    copier.In().BeginChoice(choiceType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameChoiceVariant);
    TMemberIndex index = copier.In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        copier.ThrowError(CObjectIStream::fFormatError,
                          "choice variant id expected");
    }

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    copier.In().SetTopMemberId(variantInfo->GetId());
    copier.Out().SetTopMemberId(variantInfo->GetId());
    WriteMemberId(variantInfo->GetId());

    variantInfo->CopyVariant(copier);

    copier.In().EndChoiceVariant();
    END_OBJECT_2FRAMES_OF(copier);
    copier.In().EndChoice();
    END_OBJECT_FRAME_OF(copier.In());
}
*/
#endif

void CObjectOStreamAsn::BeginBytes(const ByteBlock& )
{
    m_Output.PutChar('\'');
}

static const char HEX[] = "0123456789ABCDEF";

void CObjectOStreamAsn::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
{
    WriteBytes(bytes, length);
}

void CObjectOStreamAsn::WriteBytes(const char* bytes, size_t length)
{
    while ( length-- > 0 ) {
        char c = *bytes++;
        m_Output.WrapAt(78, false);
        m_Output.PutChar(HEX[(c >> 4) & 0xf]);
        m_Output.PutChar(HEX[c & 0xf]);
    }
}

void CObjectOStreamAsn::EndBytes(const ByteBlock& )
{
    m_Output.WrapAt(78, false);
    m_Output.PutString("\'H");
}

void CObjectOStreamAsn::BeginChars(const CharBlock& )
{
    m_Output.PutChar('"');
}

void CObjectOStreamAsn::WriteChars(const CharBlock& ,
                                   const char* chars, size_t length)
{
    while ( length > 0 ) {
        char c = *chars++;
        if ( !GoodVisibleChar(c) ) {
            FixVisibleChar(c, m_FixMethod, this, string(chars,length));
        }
        --length;
        m_Output.WrapAt(78, true);
        m_Output.PutChar(c);
        if ( c == '"' )
            m_Output.PutChar('"');
    }
}

void CObjectOStreamAsn::EndChars(const CharBlock& )
{
    m_Output.WrapAt(78, false);
    m_Output.PutChar('"');
}


void CObjectOStreamAsn::WriteSeparator(void)
{
    m_Output.PutString(GetSeparator());
    FlushBuffer();
}


END_NCBI_SCOPE
