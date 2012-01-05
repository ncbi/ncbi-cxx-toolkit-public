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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/objhook.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/impl/member.hpp>
#include <serial/impl/memberid.hpp>

BEGIN_NCBI_SCOPE

CReadObjectHook::~CReadObjectHook(void)
{
}

CReadClassMemberHook::~CReadClassMemberHook(void)
{
}

void CReadClassMemberHook::ReadMissingClassMember(CObjectIStream& in,
                                                  const CObjectInfoMI& member)
{
    member.GetMemberInfo()->
        DefaultReadMissingMember(in, member.GetClassObject().GetObjectPtr());
}

CPreReadClassMemberHook::~CPreReadClassMemberHook(void)
{
}

void CPreReadClassMemberHook::ReadClassMember(CObjectIStream& in,
                                              const CObjectInfoMI& member)
{
    PreReadClassMember(in, member);
    DefaultRead(in, member);
}

CReadChoiceVariantHook::~CReadChoiceVariantHook(void)
{
}

CPreReadChoiceVariantHook::~CPreReadChoiceVariantHook(void)
{
}

void CPreReadChoiceVariantHook::ReadChoiceVariant(CObjectIStream& in,
                                                  const CObjectInfoCV& variant)
{
    PreReadChoiceVariant(in, variant);
    DefaultRead(in, variant);
}

CReadContainerElementHook::~CReadContainerElementHook(void)
{
}

CWriteObjectHook::~CWriteObjectHook(void)
{
}

CWriteClassMemberHook::~CWriteClassMemberHook(void)
{
}

CWriteChoiceVariantHook::~CWriteChoiceVariantHook(void)
{
}

CSkipObjectHook::~CSkipObjectHook(void)
{
}

void CSkipObjectHook::DefaultRead(CObjectIStream& in,
                                  const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
}

void CSkipObjectHook::DefaultSkip(CObjectIStream& in,
                                  const CObjectTypeInfo& type)
{
    type.GetTypeInfo()->DefaultSkipData(in);
}

CSkipClassMemberHook::~CSkipClassMemberHook(void)
{
}

void CSkipClassMemberHook::SkipMissingClassMember(CObjectIStream& stream,
                                                  const CObjectTypeInfoMI& member)
{
    member.GetMemberInfo()->DefaultSkipMissingMember(stream);
}

void CSkipClassMemberHook::DefaultSkip(CObjectIStream& in,
                                       const CObjectTypeInfoMI& object)
{
    in.SkipObject(object.GetMemberType());
}

CSkipChoiceVariantHook::~CSkipChoiceVariantHook(void)
{
}

CCopyObjectHook::~CCopyObjectHook(void)
{
}

CCopyClassMemberHook::~CCopyClassMemberHook(void)
{
}

void CCopyClassMemberHook::CopyMissingClassMember(CObjectStreamCopier& copier,
                                                  const CObjectTypeInfoMI& member)
{
    member.GetMemberInfo()->DefaultCopyMissingMember(copier);
}

CCopyChoiceVariantHook::~CCopyChoiceVariantHook(void)
{
}


void CReadObjectHook::DefaultRead(CObjectIStream& in,
                                  const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
}

void CReadObjectHook::DefaultSkip(CObjectIStream& in,
                                  const CObjectInfo& object)
{
    object.GetTypeInfo()->DefaultSkipData(in);
}

void CReadClassMemberHook::DefaultRead(CObjectIStream& in,
                                       const CObjectInfoMI& object)
{
    in.ReadClassMember(object);
}

void CReadClassMemberHook::ResetMember(const CObjectInfoMI& object,
                                       CObjectInfoMI::EEraseFlag flag)
{
    const_cast<CObjectInfoMI&>(object).Erase(flag);
}

void CReadClassMemberHook::DefaultSkip(CObjectIStream& in,
                                       const CObjectInfoMI& object)
{
    in.SkipObject(object.GetMember());
}

void CReadChoiceVariantHook::DefaultRead(CObjectIStream& in,
                                         const CObjectInfoCV& object)
{
    in.ReadChoiceVariant(object);
}

void CWriteObjectHook::DefaultWrite(CObjectOStream& out,
                                    const CConstObjectInfo& object)
{
    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
}

void CWriteClassMemberHook::DefaultWrite(CObjectOStream& out,
                                         const CConstObjectInfoMI& member)
{
    out.WriteClassMember(member);
}

void CWriteChoiceVariantHook::DefaultWrite(CObjectOStream& out,
                                           const CConstObjectInfoCV& variant)
{
    out.WriteChoiceVariant(variant);
}

void CCopyObjectHook::DefaultCopy(CObjectStreamCopier& copier,
                                  const CObjectTypeInfo& type)
{
    type.GetTypeInfo()->DefaultCopyData(copier);
}

void CCopyClassMemberHook::DefaultCopy(CObjectStreamCopier& copier,
                                       const CObjectTypeInfoMI& member)
{
    member.GetMemberInfo()->DefaultCopyMember(copier);
}

void CCopyChoiceVariantHook::DefaultCopy(CObjectStreamCopier& copier,
                                         const CObjectTypeInfoCV& variant)
{
    variant.GetVariantInfo()->DefaultCopyVariant(copier);
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           CReadObjectHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Object)
{
    m_Stream.m_IStream = stream;
    if ( stream ) {
        info.SetLocalReadHook(*stream, &hook);
    }
    else {
        info.SetGlobalReadHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           CWriteObjectHook& hook,
                                           CObjectOStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Object)
{
    m_Stream.m_OStream = stream;
    if ( stream ) {
        info.SetLocalWriteHook(*stream, &hook);
    }
    else {
        info.SetGlobalWriteHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           CSkipObjectHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Skip),
      m_HookType(eHook_Object)
{
    m_Stream.m_IStream = stream;
    if ( stream ) {
        info.SetLocalSkipHook(*stream, &hook);
    }
    else {
        info.SetGlobalSkipHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           CCopyObjectHook& hook,
                                           CObjectStreamCopier* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Copy),
      m_HookType(eHook_Object)
{
    m_Stream.m_Copier = stream;
    if ( stream ) {
        info.SetLocalCopyHook(*stream, &hook);
    }
    else {
        info.SetGlobalCopyHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CReadClassMemberHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Member),
      m_Id(id)
{
    m_Stream.m_IStream = stream;
    CObjectTypeInfoMI member = info.FindMember(id);
    if ( stream ) {
        member.SetLocalReadHook(*stream, &hook);
    }
    else {
        member.SetGlobalReadHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CWriteClassMemberHook& hook,
                                           CObjectOStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Member),
      m_Id(id)
{
    m_Stream.m_OStream = stream;
    CObjectTypeInfoMI member = info.FindMember(id);
    if ( stream ) {
        member.SetLocalWriteHook(*stream, &hook);
    }
    else {
        member.SetGlobalWriteHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CSkipClassMemberHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Skip),
      m_HookType(eHook_Member),
      m_Id(id)
{
    m_Stream.m_IStream = stream;
    CObjectTypeInfoMI member = info.FindMember(id);
    if ( stream ) {
        member.SetLocalSkipHook(*stream, &hook);
    }
    else {
        member.SetGlobalSkipHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CCopyClassMemberHook& hook,
                                           CObjectStreamCopier* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Copy),
      m_HookType(eHook_Member),
      m_Id(id)
{
    m_Stream.m_Copier = stream;
    CObjectTypeInfoMI member = info.FindMember(id);
    if ( stream ) {
        member.SetLocalCopyHook(*stream, &hook);
    }
    else {
        member.SetGlobalCopyHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CReadChoiceVariantHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    m_Stream.m_IStream = stream;
    CObjectTypeInfoVI variant = info.FindVariant(id);
    if ( stream ) {
        variant.SetLocalReadHook(*stream, &hook);
    }
    else {
        variant.SetGlobalReadHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CWriteChoiceVariantHook& hook,
                                           CObjectOStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    m_Stream.m_OStream = stream;
    CObjectTypeInfoVI variant = info.FindVariant(id);
    if ( stream ) {
        variant.SetLocalWriteHook(*stream, &hook);
    }
    else {
        variant.SetGlobalWriteHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CSkipChoiceVariantHook& hook,
                                           CObjectIStream* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Skip),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    m_Stream.m_IStream = stream;
    CObjectTypeInfoVI variant = info.FindVariant(id);
    if ( stream ) {
        variant.SetLocalSkipHook(*stream, &hook);
    }
    else {
        variant.SetGlobalSkipHook(&hook);
    }
}


CObjectHookGuardBase::CObjectHookGuardBase(const CObjectTypeInfo& info,
                                           const string& id,
                                           CCopyChoiceVariantHook& hook,
                                           CObjectStreamCopier* stream)
    : m_Hook(&hook),
      m_HookMode(eHook_Copy),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    m_Stream.m_Copier = stream;
    CObjectTypeInfoVI variant = info.FindVariant(id);
    if ( stream ) {
        variant.SetLocalCopyHook(*stream, &hook);
    }
    else {
        variant.SetGlobalCopyHook(&hook);
    }
}


CObjectHookGuardBase::~CObjectHookGuardBase(void)
{
    _ASSERT(m_HookMode == eHook_None);
    _ASSERT(m_HookType == eHook_Null);
}


void CObjectHookGuardBase::ResetHook(const CObjectTypeInfo& info)
{
    switch (m_HookType) {
    case eHook_Object:
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream.m_IStream ) {
                info.ResetLocalReadHook(*m_Stream.m_IStream);
            }
            else {
                info.ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream.m_OStream ) {
                info.ResetLocalWriteHook(*m_Stream.m_OStream);
            }
            else {
                info.ResetGlobalWriteHook();
            }
            break;
        case eHook_Skip:
            if ( m_Stream.m_IStream ) {
                info.ResetLocalSkipHook(*m_Stream.m_IStream);
            }
            else {
                info.ResetGlobalSkipHook();
            }
            break;
        case eHook_Copy:
            if ( m_Stream.m_Copier ) {
                info.ResetLocalCopyHook(*m_Stream.m_Copier);
            }
            else {
                info.ResetGlobalCopyHook();
            }
            break;
        default:
            break;
        }
        break;
    case eHook_Member:
    {
        CObjectTypeInfoMI member = info.FindMember(m_Id);
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream.m_IStream ) {
                member.ResetLocalReadHook(*m_Stream.m_IStream);
            }
            else {
                member.ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream.m_OStream ) {
                member.ResetLocalWriteHook(*m_Stream.m_OStream);
            }
            else {
                member.ResetGlobalWriteHook();
            }
            break;
        case eHook_Skip:
            if ( m_Stream.m_IStream ) {
                member.ResetLocalSkipHook(*m_Stream.m_IStream);
            }
            else {
                member.ResetGlobalSkipHook();
            }
            break;
        case eHook_Copy:
            if ( m_Stream.m_Copier ) {
                member.ResetLocalCopyHook(*m_Stream.m_Copier);
            }
            else {
                member.ResetGlobalCopyHook();
            }
            break;
        default:
            break;
        }
        break;
    }
    case eHook_Variant:
    {
        CObjectTypeInfoVI variant = info.FindVariant(m_Id);
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream.m_IStream ) {
                variant.ResetLocalReadHook(*m_Stream.m_IStream);
            }
            else {
                variant.ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream.m_OStream ) {
                variant.ResetLocalWriteHook(*m_Stream.m_OStream);
            }
            else {
                variant.ResetGlobalWriteHook();
            }
            break;
        case eHook_Skip:
            if ( m_Stream.m_IStream ) {
                variant.ResetLocalSkipHook(*m_Stream.m_IStream);
            }
            else {
                variant.ResetGlobalSkipHook();
            }
            break;
        case eHook_Copy:
            if ( m_Stream.m_Copier ) {
                variant.ResetLocalCopyHook(*m_Stream.m_Copier);
            }
            else {
                variant.ResetGlobalCopyHook();
            }
            break;
        default:
            break;
        }
        break;
    }
    case eHook_Element:
    case eHook_Null:
    default:
        break;
    }
    m_HookMode = eHook_None;
    m_HookType = eHook_Null;
}

END_NCBI_SCOPE
