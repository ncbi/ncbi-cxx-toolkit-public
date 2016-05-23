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
#include <objects/seqset/Bioseq_set.hpp>

#include "read_hooks.hpp"

BEGIN_NCBI_SCOPE

static void WriteClassMember(CObjectOStream& out, const CObjectInfoMI &member)
{
    out.BeginClassMember(member.GetMemberInfo()->GetId());
    out.WriteObject(member.GetMember().GetObjectPtr(), member.GetMemberInfo()->GetTypeInfo());
    out.EndClassMember();
}

////////////////////////////////////////////////
// class CReadSubmitBlockHook
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
    in.ReadClassMember(member);
    WriteClassMember(m_out, member);

    // start writing 'data' member
    const CMemberInfo* memInfo = classTypeInfo->GetMemberInfo("data");
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


static CObjectTypeInfoMI GetBioseqsetClassTypeInfo()
{
    CObjectTypeInfo bioseqTypeInfo = CType<CBioseq_set>();
    return bioseqTypeInfo.FindMember("class");
}

static void MidWritingSet(CObjectOStream& out, const CObjectInfo& obj)
{
    // Skip all members before 'class' member - they have been already written by 'class' hook function
    CObjectInfoMI item = obj.BeginMembers();
    for (; item; ++item)
    {
        if (item.GetItemInfo()->GetId().GetName() == "class")
            break;
    }

    // Write all members after ''class' member and before 'seq-set' member
    for (++item; item; ++item)
    {
        if (item.GetItemInfo()->GetId().GetName() == "seq-set")
            break;

        if (item.IsSet())
            WriteClassMember(out, item);
    }
}

////////////////////////////////////////////////
// class CReadSetHook
class CReadSetHook : public CReadClassMemberHook
{
public:
    CReadSetHook(CGBReleaseFile::ISeqEntryHandler& handler, CObjectOStream& out) :
        m_handler(handler),
        m_out(out),
        m_level(0)
    {}

    virtual void ReadClassMember(CObjectIStream &in, const CObjectInfoMI &member)
    {
        ++m_level;
        if (m_level == 1)
        {
            MidWritingSet(m_out, member.GetClassObject());

            // Start writing 'set'
            m_out.BeginClassMember(member.GetMemberInfo()->GetId());

            const CContainerTypeInfo* containerType = CTypeConverter<CContainerTypeInfo>::SafeCast(member.GetMemberInfo()->GetTypeInfo());
            m_out.BeginContainer(containerType);

            // Read each element separately to a local TSeqEntry,
            // process it somehow, and... not store it in the container.
            for (CIStreamContainerIterator i(in, member); i; ++i) {

                CRef<CSeq_entry> entry(new CSeq_entry);
                i >> *entry;

                m_handler.HandleSeqEntry(entry);

                m_out.BeginContainerElement(entry->GetThisTypeInfo());
                m_out.WriteObject(entry, entry->GetThisTypeInfo());
                m_out.EndContainerElement();
            }

            // Complete writing 'set'
            m_out.EndContainer();
            m_out.EndClassMember();
        }
        else
        {
            // standard read
            in.ReadClassMember(member);
        }

        --m_level;
    }

private:
    CGBReleaseFile::ISeqEntryHandler& m_handler;
    CObjectOStream& m_out;
    size_t m_level;
};


////////////////////////////////////////////////
// class CReadBioseqsetClassHook
// needs to determine 'class' of the next bioseq_set

static void StartWritingSet(CObjectOStream& out)
{
    CObjectTypeInfo entryTypeInfo = CType<CSeq_entry>();

    out.BeginContainerElement(entryTypeInfo.GetTypeInfo()); // begins the next seq-entry

    const CChoiceTypeInfo* choiceType = entryTypeInfo.GetChoiceTypeInfo();
    out.BeginChoice(choiceType);

    // start writing entries
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo("set");
    out.BeginChoiceVariant(choiceType, variantInfo->GetId());

    const CClassTypeInfo* classType = CTypeConverter<CClassTypeInfo>::SafeCast(variantInfo->GetTypeInfo());
    out.BeginClass(classType);
}

static void EndWritingSet(CObjectOStream& out, const CObjectInfo& obj)
{
    // Skip all members before 'seq-set' member - they have been already written by 'class' hook function
    CObjectInfoMI item = obj.BeginMembers();
    for (; item; ++item)
    {
        if (item.GetItemInfo()->GetId().GetName() == "seq-set")
            break;
    }

    // Write all members after 'seq-set' member
    for (++item; item; ++item)
    {
        if (item.IsSet())
            WriteClassMember(out, item);
    }

    out.EndClass();
    out.EndChoiceVariant();
    out.EndChoice();
    out.EndContainerElement();
}

class CReadBioseqsetClassHook : public CReadClassMemberHook
{
public:
    CReadBioseqsetClassHook(CReadEntryHook& entryHook, CObjectOStream& out) :
        m_inside(false),
        m_entryHook(entryHook),
        m_out(out)
    {}

    virtual void ReadClassMember(CObjectIStream &in, const CObjectInfoMI &member)
    {
        in.ReadClassMember(member);

        if (!m_inside)
        {
            m_inside = true;

            int val = member.GetMember().GetPrimitiveValueInt();
            if ((val == CBioseq_set::eClass_genbank))
            {
                StartWritingSet(m_out);

                const CObjectInfo& oi = member.GetClassObject();
                for (CObjectInfoMI item = oi.BeginMembers(); item; ++item)
                {
                    if (item.GetItemInfo()->GetId().GetName() == "class")
                        break;

                    if (item.IsSet())
                        WriteClassMember(m_out, item);
                }

                m_entryHook.x_SetBioseqsetHook(in, true);

                WriteClassMember(m_out, member);
            }
        }
    }

private:
    bool m_inside;
    CReadEntryHook& m_entryHook;
    CObjectOStream& m_out;
};

CReadEntryHook::CReadEntryHook(CGBReleaseFile::ISeqEntryHandler& handler, CObjectOStream& out) :
    m_handler(handler),
    m_out(out),
    m_isGenbank(false)
{
}

void CReadEntryHook::ReadObject(CObjectIStream &in, const CObjectInfo& obj)
{
    CObjectTypeInfoMI bioseqsetClassInfo = GetBioseqsetClassTypeInfo();

    for (CIStreamContainerIterator i(in, obj); i; ++i)
    {
        m_isGenbank = false;
        bioseqsetClassInfo.SetLocalReadHook(in, new CReadBioseqsetClassHook(*this, m_out));

        CRef<CSeq_entry> entry(new CSeq_entry);
        i >> *entry;

        if (m_isGenbank)
        {
            CBioseq_set& bio_set = entry->SetSet();
            EndWritingSet(m_out, CObjectInfo(&bio_set, bio_set.GetThisTypeInfo()));
            x_SetBioseqsetHook(in, false);
        }
        else
        {
            m_handler.HandleSeqEntry(entry);

            m_out.BeginContainerElement(entry->GetThisTypeInfo());
            m_out.WriteObject(entry, entry->GetThisTypeInfo());
            m_out.EndContainerElement();
        }

        bioseqsetClassInfo.ResetLocalReadHook(in);
    }
}

void CReadEntryHook::x_SetBioseqsetHook(CObjectIStream &in, bool isSet)
{
    CObjectTypeInfo bioseqsetTypeInfo = CType<CBioseq_set>();

    CObjectTypeInfoMI setMember = bioseqsetTypeInfo.FindMember("seq-set");

    if (isSet)
        setMember.SetLocalReadHook(in, new CReadSetHook(m_handler, m_out));
    else
        setMember.ResetLocalReadHook(in);

    m_isGenbank = true;
}

END_NCBI_SCOPE