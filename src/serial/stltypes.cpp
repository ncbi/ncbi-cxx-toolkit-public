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
* Revision 1.25  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.24  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.23  2000/05/24 20:08:50  vasilche
* Implemented XML dump.
*
* Revision 1.22  2000/05/09 16:38:40  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.21  2000/05/04 16:22:20  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.20  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.19  2000/03/14 14:42:32  vasilche
* Fixed error reporting.
*
* Revision 1.18  2000/03/07 14:06:24  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.17  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.16  2000/01/10 19:46:42  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.15  2000/01/05 19:43:57  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.14  1999/12/28 21:04:27  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.13  1999/12/17 19:05:05  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.12  1999/09/27 14:18:03  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.11  1999/09/23 20:38:01  vasilche
* Fixed ambiguity.
*
* Revision 1.10  1999/09/14 18:54:21  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.9  1999/09/01 17:38:13  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.8  1999/07/20 18:23:14  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.7  1999/07/19 15:50:39  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/15 19:35:31  vasilche
* Implemented map<K, V>.
* Changed ASN.1 text formatting.
*
* Revision 1.5  1999/07/15 16:54:50  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.4  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.3  1999/07/13 20:18:22  vasilche
* Changed types naming.
*
* Revision 1.2  1999/06/04 20:51:50  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:58  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <serial/stltypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

CStlOneArgTemplate::CStlOneArgTemplate(TTypeInfo type)
    : m_DataType(type)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(const CTypeRef& type)
    : m_DataType(type)
{
}

CStlOneArgTemplate::~CStlOneArgTemplate(void)
{
}

void CStlOneArgTemplate::SetDataId(const CMemberId& id)
{
    m_DataId = id;
}

bool CStlOneArgTemplate::MayContainType(TTypeInfo typeInfo) const
{
    return GetDataTypeInfo()->IsOrMayContainType(typeInfo);
}

bool CStlOneArgTemplate::HaveChildren(TConstObjectPtr object) const
{
    return !IsDefault(object);
}

void CStlOneArgTemplate::BeginTypes(CChildrenTypesIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

bool CStlOneArgTemplate::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return cc.GetIndex().m_Index == 0;
}

TTypeInfo CStlOneArgTemplate::GetChildType(const CChildrenTypesIterator& /*cc*/) const
{
    return GetDataTypeInfo();
}

void CStlOneArgTemplate::NextType(CChildrenTypesIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(TTypeInfo keyType,
                                         TTypeInfo dataType)
    : m_KeyId(1), m_ValueId(2),
      m_KeyType(keyType), m_ValueType(dataType)
{
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(const CTypeRef& keyType,
                                         const CTypeRef& dataType)
    : m_KeyId(1), m_ValueId(2),
      m_KeyType(keyType), m_ValueType(dataType)
{
}

CStlTwoArgsTemplate::~CStlTwoArgsTemplate(void)
{
}

void CStlTwoArgsTemplate::SetKeyId(const CMemberId& id)
{
    m_KeyId = id;
}

void CStlTwoArgsTemplate::SetValueId(const CMemberId& id)
{
    m_ValueId = id;
}

bool CStlTwoArgsTemplate::MayContainType(TTypeInfo typeInfo) const
{
    return
        GetKeyTypeInfo()->IsOrMayContainType(typeInfo) ||
        GetValueTypeInfo()->IsOrMayContainType(typeInfo);
}

bool CStlTwoArgsTemplate::HaveChildren(TConstObjectPtr object) const
{
    return !IsDefault(object);
}

void CStlTwoArgsTemplate::BeginTypes(CChildrenTypesIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

bool CStlTwoArgsTemplate::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return cc.GetIndex().m_Index < 2;
}

TTypeInfo CStlTwoArgsTemplate::GetChildType(const CChildrenTypesIterator& cc) const
{
    return cc.GetIndex().m_Index == 0? GetKeyTypeInfo(): GetValueTypeInfo();
}

void CStlTwoArgsTemplate::NextType(CChildrenTypesIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

CStlClassInfoMapImpl::CStlClassInfoMapImpl(TTypeInfo keyType,
                                           TTypeInfo valueType)
    : CParent(keyType, valueType)
{
}

CStlClassInfoMapImpl::CStlClassInfoMapImpl(const CTypeRef& keyType,
                                           const CTypeRef& valueType)
    : CParent(keyType, valueType)
{
}

CStlClassInfoMapImpl::~CStlClassInfoMapImpl(void)
{
}

bool CStlClassInfoMapImpl::EqualsKeyValuePair(TConstObjectPtr key1,
                                              TConstObjectPtr key2,
                                              TConstObjectPtr value1,
                                              TConstObjectPtr value2) const
{
    return GetKeyTypeInfo()->Equals(key1, key2) &&
        GetValueTypeInfo()->Equals(value1, value2);
}

const CClassTypeInfo* CStlClassInfoMapImpl::GetElementType(void) const
{
    if ( !m_ElementType ) {
        CClassTypeInfo* classInfo =
            CClassInfoHelper<bool>::CreateAbstractClassInfo("");
        m_ElementType.reset(classInfo);
        classInfo->GetMembers().AddMember(GetKeyId(), 0);
        classInfo->GetMembers().AddMember(GetValueId(), 0);
    }
    return m_ElementType.get();
}

void CMapArrayReaderBase::ReadElement(CObjectIStream& in)
{
    CObjectStackClass cls(in, m_MapTypeInfo->GetElementType(), false);
    in.BeginClass(cls);
    ReadClassElement(in, cls);
    in.EndClass(cls);
}

void CMapArrayWriterBase::WriteElement(CObjectOStream& out)
{
    CObjectStackClass cls(out, m_MapTypeInfo->GetElementType(), false);
    out.BeginClass(cls);
    WriteClassElement(out, cls);
    out.EndClass(cls);
}

void CStlClassInfoMapImpl::ReadKey(CObjectIStream& in,
                                   CObjectStackClass& cls,
                                   TObjectPtr keyPtr) const
{
    CObjectStackClassMember m(cls);
    if ( in.BeginClassMember(m, GetElementType()->GetMembers()) != 0 ) {
        in.EndClass(cls);
        in.ThrowError(CObjectIStream::eFormatError, "map key expected");
    }
    GetKeyTypeInfo()->ReadData(in, keyPtr);
    in.EndClassMember(m);
}

void CStlClassInfoMapImpl::ReadValue(CObjectIStream& in,
                                     CObjectStackClass& cls,
                                     TObjectPtr valuePtr) const
{
    CObjectStackClassMember m(cls);
    if ( in.BeginClassMember(m, GetElementType()->GetMembers()) != 1 ) {
        in.EndClass(cls);
        in.ThrowError(CObjectIStream::eFormatError, "map value expected");
    }
    GetValueTypeInfo()->ReadData(in, valuePtr);
    in.EndClassMember(m);
}

void CStlClassInfoMapImpl::WriteKeyAndValue(CObjectOStream& out,
                                            CObjectStackClass& cls,
                                            TConstObjectPtr keyPtr,
                                            TConstObjectPtr valuePtr) const
{
    {
        CObjectStackClassMember m(cls, GetKeyId());
        out.BeginClassMember(m, GetKeyId());
        
        GetKeyTypeInfo()->WriteData(out, keyPtr);
        
        out.EndClassMember(m);
    }
    {
        CObjectStackClassMember m(cls, GetValueId());
        out.BeginClassMember(m, GetValueId());

        GetValueTypeInfo()->WriteData(out, valuePtr);

        out.EndClassMember(m);
    }
}

template<>
TTypeInfo CStlClassInfoChar_vector<char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStlClassInfoChar_vector<TChar>;
    return typeInfo;
}

template<>
TTypeInfo CStlClassInfoChar_vector<signed char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStlClassInfoChar_vector<TChar>;
    return typeInfo;
}

template<>
TTypeInfo CStlClassInfoChar_vector<unsigned char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStlClassInfoChar_vector<TChar>;
    return typeInfo;
}

END_NCBI_SCOPE
