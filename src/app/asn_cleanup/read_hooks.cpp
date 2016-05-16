/*===========================================================================
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
* Author:  Alexey Dobronadezhdin, NCBI
*
* File Description:
*   hooks reading Bioseq_set from ASN.1 files containing Seq-submit
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>

#include <objects/submit/Seq_submit.hpp>

#include "read_hooks.hpp"

BEGIN_NCBI_SCOPE

CReadSubmitBlockHook::CReadSubmitBlockHook(CObjectOStream& out) :
    m_out(out)
{
}

void CReadSubmitBlockHook::ReadClassMember(CObjectIStream &in, const CObjectInfoMI &member)
{
    ncbi::TTypeInfo seqSubmitType = CType<CSeq_submit>::GetTypeInfo();
    m_out.WriteFileHeader(seqSubmitType);

    const CClassTypeInfo* classTypeInfo = CTypeConverter<CClassTypeInfo>::SafeCast(seqSubmitType);
    m_out.BeginClass(classTypeInfo);

    // writing 'sub' member
    const CMemberInfo* memInfo = classTypeInfo->GetMemberInfo("sub");
    m_out.BeginClassMember(memInfo->GetId());
    in.ReadClassMember(member);
    m_out.WriteObject(member.GetMember().GetObjectPtr(), member.GetMemberInfo()->GetTypeInfo());
    m_out.EndClassMember();

    // start writing 'data' member
    memInfo = classTypeInfo->GetMemberInfo("data");
    m_out.BeginClassMember(memInfo->GetId());

    const CPointerTypeInfo* pointerType = CTypeConverter<CPointerTypeInfo>::SafeCast(memInfo->GetTypeInfo());
    const CChoiceTypeInfo* choiceType = CTypeConverter<CChoiceTypeInfo>::SafeCast(pointerType->GetPointedType());

    m_out.PushFrame(CObjectStackFrame::eFrameChoice, choiceType, &memInfo->GetId());
    m_out.BeginChoice(choiceType);

    // start writing 'entrys'
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo("entrys");

    m_out.PushFrame(CObjectStackFrame::eFrameChoiceVariant, variantInfo->GetId());
    m_out.BeginChoiceVariant(choiceType, variantInfo->GetId());

    const CContainerTypeInfo* containerType = CTypeConverter<CContainerTypeInfo>::SafeCast(variantInfo->GetTypeInfo());
    m_out.BeginContainer(containerType);
}

CReadEntryHook::CReadEntryHook(CGBReleaseFile::ISeqEntryHandler& handler, CObjectOStream& out) :
    m_handler(handler),
    m_out(out)
{
}

void CReadEntryHook::ReadObject(CObjectIStream &in, const CObjectInfo& obj)
{
    for (CIStreamContainerIterator i(in, obj); i; ++i)
    {
        CRef<CSeq_entry> entry(new CSeq_entry);
        i >> *entry;
        m_handler.HandleSeqEntry(entry);

        m_out.BeginContainerElement(entry->GetThisTypeInfo());
        m_out.WriteObject(entry, entry->GetThisTypeInfo());
        m_out.EndContainerElement();
    }
}
END_NCBI_SCOPE