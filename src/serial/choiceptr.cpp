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
* Revision 1.22  2000/09/26 17:38:20  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.21  2000/09/18 20:00:20  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.20  2000/09/01 13:16:14  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.19  2000/08/15 19:44:46  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.18  2000/06/16 16:31:18  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.17  2000/06/07 19:45:57  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.16  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* Revision 1.15  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.14  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.13  2000/03/14 14:42:29  vasilche
* Fixed error reporting.
*
* Revision 1.12  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.11  2000/02/17 20:02:42  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.10  2000/01/10 20:12:36  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.9  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.8  2000/01/05 19:43:51  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.7  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.6  1999/11/16 15:40:19  vasilche
* Added plain pointer choice.
*
* Revision 1.5  1999/10/28 15:37:40  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.4  1999/10/25 19:07:15  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.3  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.2  1999/09/14 18:54:15  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:58  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <serial/choiceptr.hpp>
#include <serial/typeref.hpp>
#include <serial/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/typemap.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/typeinfoimpl.hpp>

BEGIN_NCBI_SCOPE

CChoicePointerTypeInfo::CChoicePointerTypeInfo(TTypeInfo pointerType)
    : CParent(pointerType->GetSize(),
              CTypeConverter<CPointerTypeInfo>::SafeCast(pointerType)->GetPointedType()->GetName(),
              TConstObjectPtr(0), &CVoidTypeFunctions::Create, typeid(void),
              &GetPtrIndex, &SetPtrIndex, &ResetPtrIndex)
{
    SetPointerType(pointerType);
}

static CTypeInfoMap<CChoicePointerTypeInfo> CChoicePointerTypeInfo_map;

TTypeInfo CChoicePointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return CChoicePointerTypeInfo_map.GetTypeInfo(base);
}

void CChoicePointerTypeInfo::SetPointerType(TTypeInfo base)
{
    m_NullPointerIndex = kEmptyChoice;

    if ( base->GetTypeFamily() != eTypeFamilyPointer )
        THROW1_TRACE(runtime_error,
                     "invalid argument: must be CPointerTypeInfo");
    const CPointerTypeInfo* ptrType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(base);
    m_PointerTypeInfo = ptrType;

    if ( ptrType->GetPointedType()->GetTypeFamily() != eTypeFamilyClass )
        THROW1_TRACE(runtime_error,
                     "invalid argument: data must be CClassTypeInfo");
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(ptrType->GetPointedType());
    if ( !classType->IsCObject() )
        THROW1_TRACE(runtime_error,
                     "invalid argument:: choice ptr type must be CObject");
    const CClassTypeInfoBase::TSubClasses* subclasses =
        classType->SubClasses();
    if ( !subclasses )
        return;

    TTypeInfo nullTypeInfo = CNullTypeInfo::GetTypeInfo();

    for ( CClassTypeInfo::TSubClasses::const_iterator i = subclasses->begin();
          i != subclasses->end(); ++i ) {
        TTypeInfo variantType = i->second.Get();
        if ( !variantType ) {
            // null
            variantType = nullTypeInfo;
        }
        AddVariant(i->first, 0, variantType)->SetSubClass();
        TMemberIndex index = GetVariants().LastIndex();
        if ( variantType == nullTypeInfo ) {
            if ( m_NullPointerIndex == kEmptyChoice )
                m_NullPointerIndex = index;
            else {
                ERR_POST("double null");
            }
        }
        else {
            const type_info* id = &CTypeConverter<CClassTypeInfo>::SafeCast(variantType)->GetId();
            if ( !m_VariantsByType.insert(TVariantsByType::value_type(id, index)).second ) {
                THROW1_TRACE(runtime_error,
                             "conflict subclasses: "+variantType->GetName());
            }
        }
    }
}

TMemberIndex
CChoicePointerTypeInfo::GetPtrIndex(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    TConstObjectPtr classPtr = ptrType->GetObjectPointer(choicePtr);
    if ( !classPtr )
        return choicePtrType->m_NullPointerIndex;
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(ptrType->GetPointedType());
    const TVariantsByType& variants = choicePtrType->m_VariantsByType;
    TVariantsByType::const_iterator v =
        variants.find(classType->GetCPlusPlusTypeInfo(classPtr));
    if ( v == variants.end() )
        THROW1_TRACE(runtime_error,
                     "incompatible CChoicePointerTypeInfo type");
    return v->second;
}

void CChoicePointerTypeInfo::ResetPtrIndex(const CChoiceTypeInfo* choiceType,
                                           TObjectPtr choicePtr)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    ptrType->SetObjectPointer(choicePtr, 0);
}

void CChoicePointerTypeInfo::SetPtrIndex(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr,
                                         TMemberIndex index)
{
    const CChoicePointerTypeInfo* choicePtrType = 
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);

    const CPointerTypeInfo* ptrType = choicePtrType->m_PointerTypeInfo;
    _ASSERT(!ptrType->GetObjectPointer(choicePtr));
    const CVariantInfo* variantInfo = choicePtrType->GetVariantInfo(index);
    ptrType->SetObjectPointer(choicePtr,
                              variantInfo->GetTypeInfo()->Create());
}

class CNullFunctions
{
public:
    static TObjectPtr Create(TTypeInfo )
        {
            return 0;
        }
    static void Read(CObjectIStream& in, TTypeInfo ,
                     TObjectPtr objectPtr)
        {
            if ( objectPtr != 0 ) {
                THROW1_TRACE(runtime_error, "non null value");
            }
            in.ReadNull();
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr objectPtr)
        {
            if ( objectPtr != 0 ) {
                THROW1_TRACE(runtime_error, "non null value");
            }
            out.WriteNull();
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.In().ReadNull();
            copier.Out().WriteNull();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipNull();
        }
};

CNullTypeInfo::CNullTypeInfo(void)
{
    SetCreateFunction(&CNullFunctions::Create);
    SetReadFunction(&CNullFunctions::Read);
    SetWriteFunction(&CNullFunctions::Write);
    SetCopyFunction(&CNullFunctions::Copy);
    SetSkipFunction(&CNullFunctions::Skip);
}

TTypeInfo CNullTypeInfo::GetTypeInfo(void)
{
    TTypeInfo typeInfo = new CNullTypeInfo();
    return typeInfo;
}

END_NCBI_SCOPE
