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
* Revision 1.41  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.40  2004/04/02 16:57:09  gouriano
* made it possible to create named CTypeInfo for containers
*
* Revision 1.39  2003/07/22 21:46:42  vasilche
* Added SET OF implemented as vector<>.
*
* Revision 1.38  2003/05/08 15:57:28  grichenk
* Declared static maps as CSafeStaticPtr<>-s
*
* Revision 1.37  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.36  2001/09/04 14:08:28  ucko
* Handle CConstRef analogously to CRef in type macros
*
* Revision 1.35  2001/05/17 15:07:09  lavr
* Typos corrected
*
* Revision 1.34  2000/11/07 17:25:42  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.33  2000/10/13 20:22:56  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.32  2000/10/13 16:28:41  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.31  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.30  2000/09/18 20:00:26  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.29  2000/09/01 13:16:21  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.28  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.27  2000/07/11 20:36:19  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.26  2000/07/03 18:42:47  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
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
* Fixed bug with overloaded constructors of Block.
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

#include <ncbi_pch.hpp>
#include <serial/stltypesimpl.hpp>
#include <serial/exception.hpp>
#include <serial/classinfo.hpp>
#include <serial/classinfohelper.hpp>
#include <serial/typemap.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE

static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_auto_ptr;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_CRef;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_CConstRef;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_AutoPtr;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_list;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMapSet_list;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_vector;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMapSet_vector;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_set;
static CSafeStaticPtr<CTypeInfoMap> s_TypeMap_multiset;

TTypeInfo CStlClassInfoUtil::Get_auto_ptr(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_auto_ptr->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_CRef(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_CRef->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_CConstRef(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_CConstRef->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_AutoPtr(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_AutoPtr->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_list(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_list->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::GetSet_list(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMapSet_list->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_vector(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_vector->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::GetSet_vector(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMapSet_vector->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_set(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_set->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_multiset(TTypeInfo arg, TTypeInfoCreator1 f)
{
    return s_TypeMap_multiset->GetTypeInfo(arg, f);
}

TTypeInfo CStlClassInfoUtil::Get_map(TTypeInfo arg1, TTypeInfo arg2,
                                     TTypeInfoCreator2 f)
{
    return f(arg1, arg2);
}

TTypeInfo CStlClassInfoUtil::Get_multimap(TTypeInfo arg1, TTypeInfo arg2,
                                          TTypeInfoCreator2 f)
{
    return f(arg1, arg2);
}

void CStlClassInfoUtil::CannotGetElementOfSet(void)
{
    NCBI_THROW(CSerialException,eFail, "cannot get pointer to element of set");
}

void CStlClassInfoUtil::ThrowDuplicateElementError(void)
{
    NCBI_THROW(CSerialException,eFail, "duplicate element of unique container");
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       TTypeInfo type, bool randomOrder,
                                       const string& name)
    : CParent(size, name, type, randomOrder)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       TTypeInfo type, bool randomOrder)
    : CParent(size, type, randomOrder)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(size_t size,
                                       const CTypeRef& type, bool randomOrder)
    : CParent(size, type, randomOrder)
{
}

void CStlOneArgTemplate::SetDataId(const CMemberId& id)
{
    m_DataId = id;
}

bool CStlOneArgTemplate::IsDefault(TConstObjectPtr objectPtr) const
{
    return m_IsDefault(objectPtr);
}

void CStlOneArgTemplate::SetDefault(TObjectPtr objectPtr) const
{
    m_SetDefault(objectPtr);
}

void CStlOneArgTemplate::SetMemFunctions(TTypeCreate create,
                                         TIsDefaultFunction isDefault,
                                         TSetDefaultFunction setDefault)
{
    SetCreateFunction(create);
    m_IsDefault = isDefault;
    m_SetDefault = setDefault;
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(size_t size,
                                         TTypeInfo keyType,
                                         TPointerOffsetType keyOffset,
                                         TTypeInfo valueType,
                                         TPointerOffsetType valueOffset,
                                         bool randomOrder)
    : CParent(size, CTypeRef(&CreateElementTypeInfo, this), randomOrder),
      m_KeyType(keyType), m_KeyOffset(keyOffset),
      m_ValueType(valueType), m_ValueOffset(valueOffset)
{
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(size_t size,
                                         const CTypeRef& keyType,
                                         TPointerOffsetType keyOffset,
                                         const CTypeRef& valueType,
                                         TPointerOffsetType valueOffset,
                                         bool randomOrder)
    : CParent(size, CTypeRef(&CreateElementTypeInfo, this), randomOrder),
      m_KeyType(keyType), m_KeyOffset(keyOffset),
      m_ValueType(valueType), m_ValueOffset(valueOffset)
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

TTypeInfo CStlTwoArgsTemplate::CreateElementTypeInfo(TTypeInfo argType)
{
    const CStlTwoArgsTemplate* mapType = 
        CTypeConverter<CStlTwoArgsTemplate>::SafeCast(argType);
    CClassTypeInfo* classInfo =
        CClassInfoHelper<bool>::CreateAbstractClassInfo("");
    classInfo->SetRandomOrder(false);
    classInfo->AddMember(mapType->GetKeyId(),
                         TConstObjectPtr(mapType->m_KeyOffset),
                         mapType->m_KeyType.Get());
    classInfo->AddMember(mapType->GetValueId(),
                         TConstObjectPtr(mapType->m_ValueOffset),
                         mapType->m_ValueType.Get());
    return classInfo;
}

END_NCBI_SCOPE
