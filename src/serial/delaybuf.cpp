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
* Revision 1.3  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/05/03 14:38:13  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.1  2000/04/28 16:58:12  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/delaybuf.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/bytesrc.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

CDelayBuffer::~CDelayBuffer(void)
{
}

bool CDelayBuffer::DelayRead(CObjectIStream& in, TObjectPtr object,
                             TMemberIndex index, const CMemberInfo* memberInfo)
{
    if ( !memberInfo->CanBeDelayed() )
        return false;
    _ASSERT(memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->GetDelayBuffer(object));

    CRef<CByteSource> data = in.DelayRead(memberInfo->GetTypeInfo());
    if ( !data )
        return false;

    // we've got delayed buffer -> select using delay buffer
    memberInfo->GetDelayBuffer(object).m_Info.
        reset(new SInfo(object, index, memberInfo, in.GetDataFormat(), data));
    return true;
}

void CDelayBuffer::Forget(void)
{
    _ASSERT(m_Info.get() != 0);
    m_Info.reset(0);
}

void CDelayBuffer::DoUpdate(void)
{
    _ASSERT(m_Info.get() != 0);
    SInfo& info = *m_Info;
    TObjectPtr member = info.m_MemberInfo->GetMember(info.m_Object);
    TTypeInfo typeInfo = info.m_MemberInfo->GetTypeInfo();
    if ( info.m_MemberInfo->IsPointer() ) {
        // create object itself
        if ( info.m_MemberInfo->IsObjectPointer() ) {
            _TRACE("Should check for real pointer type (CRef...)");
            member = CType<TObjectPtr>::Get(member) = typeInfo->Create();
            CType<CObject>::Get(member).AddReference();
        }
        else {
            member = CType<TObjectPtr>::Get(member) = typeInfo->Create();
        }
    }
    auto_ptr<CObjectIStream> in(CObjectIStream::Create(info.m_DataFormat,
                                                       info.m_Source));
    typeInfo->ReadData(*in, member);
    Forget();
}

bool CDelayBuffer::CopyTo(CObjectOStream& out) const
{
    _ASSERT(m_Info.get() != 0);
    if ( m_Info->m_DataFormat != out.GetDataFormat() )
        return false;
    return out.Write(m_Info->m_Source);
}

CDelayBuffer::SInfo::SInfo(TObjectPtr object,
                           TMemberIndex index, const CMemberInfo* memberInfo,
                           ESerialDataFormat format,
                           const CRef<CByteSource>& source)
    : m_Object(object), m_Index(index), m_MemberInfo(memberInfo),
      m_DataFormat(format), m_Source(source)
{
}

CDelayBuffer::SInfo::~SInfo(void)
{
}

END_NCBI_SCOPE
