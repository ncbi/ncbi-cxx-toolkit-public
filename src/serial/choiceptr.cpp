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

BEGIN_NCBI_SCOPE

CChoicePointerTypeInfo::CChoicePointerTypeInfo(TTypeInfo typeInfo)
    : CPointerTypeInfo(typeInfo)
{
    Init();
}

CChoicePointerTypeInfo::CChoicePointerTypeInfo(const string& name,
                                               TTypeInfo typeInfo)
    : CPointerTypeInfo(name, typeInfo)
{
    Init();
}

void CChoicePointerTypeInfo::Init(void)
{
    const CClassInfoTmpl::TSubClasses* subclasses =
        dynamic_cast<const CClassInfoTmpl&>(*GetDataTypeInfo()).SubClasses();
    if ( !subclasses )
        return;
    for ( CClassInfoTmpl::TSubClasses::const_iterator i = subclasses->begin();
          i != subclasses->end(); ++i ) {
        m_Variants.AddMember(i->first);
        m_VariantTypes.push_back(i->second);
    }
}

const CChoicePointerTypeInfo::TVariantsByType&
CChoicePointerTypeInfo::VariantsByType(void) const
{    
    TVariantsByType* variants = m_VariantsByType.get();
    if ( !variants ) {
        m_VariantsByType.reset(variants = new TVariantsByType);

        for ( TIndex i = 0, size = m_VariantTypes.size();
              i != size; ++i ) {
            TTypeInfo type = m_VariantTypes[i].Get();
            const type_info* id =
                &dynamic_cast<const CClassInfoTmpl&>(*type).GetId();
            _TRACE(GetDataTypeInfo()->GetName() << ".AddSubClass: " << id->name());
            if ( !variants->insert(TVariantsByType::
                                   value_type(id, i)).second ) {
                THROW1_TRACE(runtime_error,
                             "conflict subclasses: " + type->GetName());
            }
        }
    }
    return *variants;
}

CChoicePointerTypeInfo::TIndex
CChoicePointerTypeInfo::FindVariant(TConstObjectPtr object) const
{    
    if ( !object )
        THROW1_TRACE(runtime_error, "null choice pointer");

    const TVariantsByType& variants = VariantsByType();
    const type_info* id =
        dynamic_cast<const CClassInfoTmpl*>(GetDataTypeInfo())->
        GetCPlusPlusTypeInfo(object);
    TVariantsByType::const_iterator p = variants.find(id);
    if ( p == variants.end() )
        THROW1_TRACE(runtime_error, "incompatible type: " + string(id->name()));
    return p->second;
}

void CChoicePointerTypeInfo::CollectExternalObjects(COObjectList& objectList,
                                                    TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    m_VariantTypes[FindVariant(data)].Get()->CollectObjects(objectList, data);
}

void CChoicePointerTypeInfo::WriteData(CObjectOStream& out,
                                       TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    TIndex index = FindVariant(data);
    CObjectOStream::Member m(out, m_Variants.GetCompleteMemberId(index));
    out.WriteExternalObject(data, m_VariantTypes[index].Get());
}

void CChoicePointerTypeInfo::ReadData(CObjectIStream& in,
                                      TObjectPtr object) const
{
    CObjectIStream::Member m(in);
    TIndex index = m_Variants.FindMember(m);
    if ( index < 0 )
        THROW1_TRACE(runtime_error, "incompatible type: " + m.ToString());
    TTypeInfo dataType = m_VariantTypes[index].Get();
    TObjectPtr data = dataType->Create();
    SetObjectPointer(object, data);
    in.ReadExternalObject(data, dataType);
}

END_NCBI_SCOPE
