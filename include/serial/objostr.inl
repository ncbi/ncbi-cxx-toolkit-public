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
void CObjectOStream::StackElement::SetName(const char* str)
{
    m_NameType = eNameCharPtr;
    m_NameCharPtr = str;
}

inline
void CObjectOStream::StackElement::SetName(const string& str)
{
    m_NameType = eNameString;
    m_NameString = &str;
}

inline
void CObjectOStream::StackElement::SetName(const CMemberId& id)
{
    m_NameType = eNameId;
    m_NameId = &id;
}

inline
CObjectOStream::StackElement::StackElement(CObjectOStream& s)
    : m_Stream(s), m_Previous(s.m_CurrentElement),
      m_NameType(eNameEmpty)
{
    s.m_CurrentElement = this;
}

inline
CObjectOStream::StackElement::StackElement(CObjectOStream& s,
                                           const CMemberId& id)
    : m_Stream(s), m_Previous(s.m_CurrentElement)
{
    SetName(id);
    s.m_CurrentElement = this;
}

inline
CObjectOStream::StackElement::StackElement(CObjectOStream& s,
                                           const string& str)
    : m_Stream(s), m_Previous(s.m_CurrentElement)
{
    SetName(str);
    s.m_CurrentElement = this;
}

inline
CObjectOStream::StackElement::StackElement(CObjectOStream& s,
                                           const char* str)
    : m_Stream(s), m_Previous(s.m_CurrentElement)
{
    SetName(str);
    s.m_CurrentElement = this;
}

inline
CObjectOStream::StackElement::~StackElement(void)
{
    m_Stream.m_CurrentElement = m_Previous;
}

inline
CObjectOStream& CObjectOStream::StackElement::GetStream(void) const
{
    return m_Stream;
}

inline
const CObjectOStream::StackElement*
CObjectOStream::StackElement::GetPrevous(void) const
{
    return m_Previous;
}

inline
CObjectOStream::Member::Member(CObjectOStream& out, const CMemberId& member)
    : StackElement(out, member)
{
    out.StartMember(*this, member);
}

inline
CObjectOStream::Member::~Member(void)
{
    GetStream().EndMember(*this);
}

inline
bool CObjectOStream::Block::RandomOrder(void) const
{
    return m_RandomOrder;
}

inline
size_t CObjectOStream::Block::GetNextIndex(void) const
{
    return m_NextIndex;
}

inline
size_t CObjectOStream::Block::GetIndex(void) const
{
    return GetNextIndex() - 1;
}

inline
bool CObjectOStream::Block::First(void) const
{
    return GetNextIndex() == 0;
}

inline
size_t CObjectOStream::Block::GetSize(void) const
{
    return m_Size;
}

inline
void CObjectOStream::Block::IncIndex(void)
{
    ++m_NextIndex;
}

inline
CObjectOStream::Block::Block(CObjectOStream& out, bool randomOrder)
    : StackElement(out, "E"), m_RandomOrder(randomOrder),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

inline
CObjectOStream::Block::Block(CObjectOStream& out, EClass /*isClass*/,
                             bool randomOrder)
    : StackElement(out), m_RandomOrder(randomOrder),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

inline
void CObjectOStream::Block::Next(void)
{
    GetStream().VNext(*this);
    IncIndex();
}

inline
CObjectOStream::Block::~Block(void)
{
    GetStream().VEnd(*this);
}

inline
size_t CObjectOStream::ByteBlock::GetLength(void) const
{
    return m_Length;
}

inline
CObjectOStream::ByteBlock::ByteBlock(CObjectOStream& out, size_t length)
    : StackElement(out), m_Length(length)
{
    out.Begin(*this);
}

inline
void CObjectOStream::ByteBlock::Write(const void* bytes, size_t length)
{
    _ASSERT( length <= m_Length );
    GetStream().WriteBytes(*this, static_cast<const char*>(bytes), length);
    m_Length -= length;
}

inline
CObjectOStream::ByteBlock::~ByteBlock(void)
{
    _ASSERT(m_Length == 0);
    GetStream().End(*this);
}

#if HAVE_NCBI_C

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
