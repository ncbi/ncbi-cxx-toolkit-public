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
* Revision 1.57  2000/12/04 19:02:41  beloslyu
* changes for FreeBSD
*
* Revision 1.56  2000/11/07 17:25:40  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.55  2000/10/20 15:51:42  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.54  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.53  2000/10/13 20:22:55  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.52  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.51  2000/10/05 15:52:50  vasilche
* Avoid useing snprintf bacause it's missing on osf1_gcc
*
* Revision 1.50  2000/10/05 13:17:17  vasilche
* Added missing #include <stdio.h>
*
* Revision 1.49  2000/10/04 19:18:59  vasilche
* Fixed processing floating point data.
*
* Revision 1.48  2000/10/03 17:22:44  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.47  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.46  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.45  2000/09/18 20:00:24  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.44  2000/09/01 13:16:19  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.43  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.42  2000/07/03 18:42:46  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.41  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.40  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.39  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.38  2000/05/24 20:08:48  vasilche
* Implemented XML dump.
*
* Revision 1.37  2000/05/09 19:04:56  vasilche
* Fixed bug in storing float point number to text ASN.1 format.
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
* Revision 1.33  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.32  2000/02/01 21:47:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.31  2000/01/11 14:16:46  vasilche
* Fixed pow ambiguity.
*
* Revision 1.30  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.29  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.28  1999/10/25 20:19:51  vasilche
* Fixed strings representation in text ASN.1 files.
*
* Revision 1.27  1999/10/04 16:22:18  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.26  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.25  1999/09/23 21:16:08  vasilche
* Removed dependance on asn.h
*
* Revision 1.24  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.23  1999/09/23 18:57:01  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.22  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.21  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.20  1999/07/22 19:48:57  vasilche
* Reversed hack with embedding old ASN.1 output to new.
*
* Revision 1.19  1999/07/22 19:40:57  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.18  1999/07/22 17:33:56  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.17  1999/07/21 14:20:07  vasilche
* Added serialization of bool.
*
* Revision 1.16  1999/07/20 18:23:12  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.15  1999/07/19 15:50:36  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/15 19:35:30  vasilche
* Implemented map<K, V>.
* Changed ASN.1 text formatting.
*
* Revision 1.13  1999/07/15 16:54:49  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.12  1999/07/14 18:58:10  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.11  1999/07/13 20:18:20  vasilche
* Changed types naming.
*
* Revision 1.10  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.9  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.8  1999/07/02 21:31:59  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.7  1999/07/01 17:55:34  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.6  1999/06/30 16:05:00  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.5  1999/06/24 14:44:58  vasilche
* Added binary ASN.1 output.
*
* Revision 1.4  1999/06/18 16:26:49  vasilche
* Fixed bug with unget() in MSVS
*
* Revision 1.3  1999/06/17 20:42:07  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.2  1999/06/16 20:35:34  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.1  1999/06/15 16:19:51  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:49  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:03  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:55  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/delaybuf.hpp>

#include <stdio.h>
#include <math.h>
#include <limits.h>
#if HAVE_WINDOWS_H || defined(__FreeBSD__)
// FreeBSD needs this to find FLT_DIG and DBL_DIG
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif

#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamAsn(CNcbiOstream& out, bool deleteOut)
{
    return new CObjectOStreamAsn(out, deleteOut);
}

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out)
    : CObjectOStream(out)
{
    m_Output.SetBackLimit(80);
}

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(out, deleteOut)
{
    m_Output.SetBackLimit(80);
}

CObjectOStreamAsn::~CObjectOStreamAsn(void)
{
}

ESerialDataFormat CObjectOStreamAsn::GetDataFormat(void) const
{
    return eSerial_AsnText;
}

string CObjectOStreamAsn::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Output.GetLine());
}

void CObjectOStreamAsn::WriteFileHeader(TTypeInfo type)
{
    if ( true || m_Output.ZeroIndentLevel() ) {
        WriteId(type->GetName());
        m_Output.PutString(" ::= ");
    }
}

inline
void CObjectOStreamAsn::WriteEnum(long value, const string& valueName)
{
    if ( !valueName.empty() )
		m_Output.PutString(valueName);
    else
        m_Output.PutLong(value);
}

void CObjectOStreamAsn::WriteEnum(const CEnumeratedTypeValues& values,
                                  long value)
{
    WriteEnum(value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamAsn::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    long value = in.ReadEnum(values);
    WriteEnum(value, values.FindName(value, values.IsInteger()));
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

void CObjectOStreamAsn::WriteInt(int data)
{
    m_Output.PutInt(data);
}

void CObjectOStreamAsn::WriteUInt(unsigned data)
{
    m_Output.PutUInt(data);
}

void CObjectOStreamAsn::WriteLong(long data)
{
    m_Output.PutLong(data);
}

void CObjectOStreamAsn::WriteULong(unsigned long data)
{
    m_Output.PutULong(data);
}

void CObjectOStreamAsn::WriteDouble2(double data, size_t digits)
{
	if ( data == 0.0 ) {
        m_Output.PutString("{ 0, 10, 0 }");
        return;
	}

    char buffer[128];
    // ensure buffer is large enough to fit result
    // (additional bytes are for sign, dot and exponent)
    _ASSERT(sizeof(buffer) > digits + 16);
    int width = sprintf(buffer, "%.*e", int(digits-1), data);
    if ( width <= 0 || width >= int(sizeof(buffer) - 1) )
        ThrowError(eOverflow, "buffer overflow");
    _ASSERT(int(strlen(buffer)) == width);
    char* dotPos = strchr(buffer, '.');
    _ASSERT(dotPos);
    char* ePos = strchr(dotPos, 'e');
    _ASSERT(ePos);

    // now we have:
    // mantissa with dot - buffer:ePos
    // exponent - (ePos+1):

    int exp;
    // calcilate exponent
    if ( sscanf(ePos + 1, "%d", &exp) != 1 )
        ThrowError(eFail, "double conversion error");

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
    m_Output.PutInt(exp - fractDigits);
    m_Output.PutString(" }");
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

void CObjectOStreamAsn::WriteString(const char* ptr, size_t length)
{
    m_Output.PutChar('"');
    while ( length > 0 ) {
        char c = *ptr++;
        --length;
        if ( c < ' ' || c > '~' ) {
            ERR_POST("bad char in string: " << int(c));
            c = '#';
        }
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

void CObjectOStreamAsn::WriteString(const string& str)
{
    WriteString(str.data(), str.size());
}

void CObjectOStreamAsn::WriteStringStore(const string& str)
{
    WriteString(str.data(), str.size());
}

void CObjectOStreamAsn::CopyString(CObjectIStream& in)
{
    string s;
    in.ReadStd(s);
    WriteString(s.data(), s.size());
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
	}
	else {
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
    if ( sizeof(TObjectIndex) == sizeof(int) )
        m_Output.PutInt(int(index));
    else if ( sizeof(TObjectIndex) == sizeof(long) )
        m_Output.PutLong(long(index));
    else
        ThrowError(eIllegalCall, "invalid size of TObjectIndex");
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

    if ( !id.GetName().empty() ) {
        m_Output.PutString(id.GetName());
        m_Output.PutChar(' ');
    }
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
    
    if ( !memberId.GetName().empty() ) {
        m_Output.PutString(memberId.GetName());
        m_Output.PutChar(' ');
    }
    
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
    
    if ( !memberId.GetName().empty() ) {
        m_Output.PutString(memberId.GetName());
        m_Output.PutChar(' ');
    }
    
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

    vector<bool> read(classType->GetMembers().LastIndex() + 1);

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
            if ( !memberInfo->GetId().GetName().empty() ) {
                m_Output.PutString(memberInfo->GetId().GetName());
                m_Output.PutChar(' ');
            }

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
        if ( !memberInfo->GetId().GetName().empty() ) {
            m_Output.PutString(memberInfo->GetId().GetName());
            m_Output.PutChar(' ');
        }
        
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

void CObjectOStreamAsn::BeginChoiceVariant(const CChoiceTypeInfo* ,
                                           const CMemberId& id)
{
    m_Output.PutString(id.GetName());
    m_Output.PutChar(' ');
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsn::WriteChoice(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    m_Output.PutString(variantInfo->GetId().GetName());
    m_Output.PutChar(' ');
    
    variantInfo->WriteVariant(*this, choicePtr);

    END_OBJECT_FRAME();
}

void CObjectOStreamAsn::CopyChoice(const CChoiceTypeInfo* choiceType,
                                   CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameChoice, choiceType);

    TMemberIndex index = copier.In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        copier.ThrowError(CObjectIStream::eFormatError,
                          "choice variant id expected");
    }

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameChoiceVariant,
                             variantInfo->GetId());
    m_Output.PutString(variantInfo->GetId().GetName());
    m_Output.PutChar(' ');

    variantInfo->CopyVariant(copier);

    copier.In().EndChoiceVariant();
    END_OBJECT_2FRAMES_OF(copier);

    END_OBJECT_FRAME_OF(copier.In());
}
#endif

void CObjectOStreamAsn::BeginBytes(const ByteBlock& )
{
	m_Output.PutChar('\'');
}

static const char HEX[] = "0123456789ABCDEF";

void CObjectOStreamAsn::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
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

#if HAVE_NCBI_C
unsigned CObjectOStreamAsn::GetAsnFlags(void)
{
    return ASNIO_TEXT;
}

void CObjectOStreamAsn::AsnOpen(AsnIo& asn)
{
    asn.m_Count = 0;
    size_t indent = m_Output.GetIndentLevel();
    asn->indent_level = Int1(indent);
    size_t max_indent = asn->max_indent;
    if ( indent >= max_indent ) {
        Boolean* tmp = asn->first;
        asn->first = (BoolPtr) MemNew((sizeof(Boolean) * (indent + 10)));
        MemCopy(asn->first, tmp, (size_t)(sizeof(Boolean) * max_indent));
        MemFree(tmp);
        asn->max_indent = Int1(indent);
    }
}

void CObjectOStreamAsn::AsnWrite(AsnIo& asn, const char* data, size_t length)
{
    if ( asn.m_Count == 0 ) {
        // dirty hack to skip structure name with '::='
        const char* p = (const char*)memchr(data, ':', length);
        if ( p && p[1] == ':' && p[2] == '=' ) {
            // check type name
            const char* beg = data;
            const char* end = p;
            while ( beg < end && isspace(beg[0]) )
                beg++;
            while ( end > beg && isspace(end[-1]) )
                end--;
            if ( string(beg, end) != asn.GetRootTypeName() ) {
                ERR_POST("AsnWrite: wrong ASN.1 type name: must be \"" <<
                         asn.GetRootTypeName() << "\"");
            }
            // skip header
            size_t skip = p + 3 - data;
            _TRACE(Warning <<
                   "AsnWrite: skipping \"" << string(data, skip) << "\"");
            data += skip;
            length -= skip;
        }
        else {
            ERR_POST("AsnWrite: no \"Asn-Type ::=\" header");
        }
        asn.m_Count = 1;
    }
    m_Output.PutString(data, length);
}
#endif

END_NCBI_SCOPE
