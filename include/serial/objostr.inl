#if defined(OBJOSTR__HPP)  &&  !defined(OBJOSTR__INL)
#define OBJOSTR__INL

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
* Revision 1.5  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.4  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* Revision 1.3  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.2  2000/05/04 16:22:24  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.1  1999/05/19 19:56:27  vasilche
* Commit just in case.
*
* ===========================================================================
*/

inline
void CObjectOStream::WriteObjectNoHook(TConstObjectPtr objectPtr,
                                       TTypeInfo objectType)
{
    objectType->WriteData(*this, objectPtr);
}

inline
void CObjectOStream::WriteObjectNoHook(const CConstObjectInfo& object)
{
    object.GetTypeInfo()->WriteData(*this, object.GetObjectPtr());
}

inline
void CObjectOStream::WriteObject(TConstObjectPtr objectPtr,
                                 TTypeInfo objectType)
{
    TWriteObjectHooks* hooks = m_WriteObjectHooks.get();
    if ( hooks ) {
        CWriteObjectHook* hook = GetHook(*hooks, objectType);
        if ( hook ) {
            hook->WriteObject(*this,
                              CConstObjectInfo(objectPtr, objectType,
                                               CConstObjectInfo::eNonCObject));
            return;
        }
    }
    WriteObjectNoHook(objectPtr, objectType);
}

inline
void CObjectOStream::WriteObject(const CConstObjectInfo& object)
{
    TWriteObjectHooks* hooks = m_WriteObjectHooks.get();
    if ( hooks ) {
        CWriteObjectHook* hook = GetHook(*hooks, object.GetTypeInfo());
        if ( hook ) {
            hook->WriteObject(*this, object);
            return;
        }
    }
    WriteObjectNoHook(object);
}

inline
void CObjectOStream::WriteContainerElements(const CConstObjectInfo& container,
                                            CWriteContainerElementsHook& hook)
{
    hook.WriteContainerElements(*this, container);
}

inline
void CObjectOStream::WriteClass(const CConstObjectInfo& object,
                                CWriteClassMembersHook& hook)
{
    DoWriteClass(object, hook);
}

inline
void CObjectOStream::WriteClass(TConstObjectPtr objectPtr,
                                const CClassTypeInfo* objectType)
{
    DoWriteClass(objectPtr, objectType);
}

inline
void CObjectOStream::WriteClass(const CConstObjectInfo& object)
{
    DoWriteClass(object.GetObjectPtr(), object.GetClassTypeInfo());
}

inline
void CObjectOStream::WriteClassMembers(const CConstObjectInfo& object)
{
    WriteClassMembers(object.GetObjectPtr(), object.GetClassTypeInfo());
}

inline
void CObjectOStream::WriteChoiceVariant(const CConstObjectInfo& choice,
                                        TMemberIndex index)
{
    TWriteChoiceVariantHooks* hooks = m_WriteChoiceVariantHooks.get();
    if ( hooks ) {
        CWriteChoiceVariantHook* hook = GetHook(*hooks,
                                                choice.GetChoiceTypeInfo(),
                                                index);
        if ( hook ) {
            hook->WriteChoiceVariant(*this, choice, index);
            return;
        }
    }
    WriteChoiceVariantNoHook(choice, index);
}

inline
CObjectOStream::ByteBlock::ByteBlock(CObjectOStream& out, size_t length)
    : m_Stream(out), m_Length(length)
{
    out.BeginBytes(*this);
}

inline
CObjectOStream::ByteBlock::~ByteBlock(void)
{
    _ASSERT(m_Length == 0);
    GetStream().EndBytes(*this);
}

inline
CObjectOStream& CObjectOStream::ByteBlock::GetStream(void) const
{
    return m_Stream;
}

inline
size_t CObjectOStream::ByteBlock::GetLength(void) const
{
    return m_Length;
}

inline
void CObjectOStream::ByteBlock::Write(const void* bytes, size_t length)
{
    _ASSERT( length <= m_Length );
    GetStream().WriteBytes(*this, static_cast<const char*>(bytes), length);
    m_Length -= length;
}

#if HAVE_NCBI_C

inline
CObjectOStream& CObjectOStream::AsnIo::GetStream(void) const
{
    return m_Stream;
}

inline
CObjectOStream::AsnIo::operator asnio*(void)
{
    return m_AsnIo;
}

inline
asnio* CObjectOStream::AsnIo::operator->(void)
{
    return m_AsnIo;
}

inline
const string& CObjectOStream::AsnIo::GetRootTypeName(void) const
{
    return m_RootTypeName;
}

inline
void CObjectOStream::AsnIo::Write(const char* data, size_t length)
{
    GetStream().AsnWrite(*this, data, length);
}

#endif

#endif /* def OBJOSTR__HPP  &&  ndef OBJOSTR__INL */
