#if defined(OBJISTR__HPP)  &&  !defined(OBJISTR__INL)
#define OBJISTR__INL

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
* Revision 1.4  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.3  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* ===========================================================================
*/

inline
CObjectIStream::LastMember::LastMember(const CMembers& members)
    : m_Members(members), m_Index(-1)
{
}

inline
const CMembers& CObjectIStream::LastMember::GetMembers(void) const
{
    return m_Members;
}

inline
CObjectIStream::TMemberIndex CObjectIStream::LastMember::GetIndex(void) const
{
    return m_Index;
}

inline
void CObjectIStream::StackElement::SetName(const char* str)
{
    m_NameType = eNameCharPtr;
    m_NameCharPtr = str;
}

inline
void CObjectIStream::StackElement::SetName(const string& str)
{
    m_NameType = eNameString;
    m_NameString = &str;
}

inline
void CObjectIStream::StackElement::SetName(const CMemberId& id)
{
    m_NameType = eNameId;
    m_NameId = &id;
}

inline
bool CObjectIStream::StackElement::Named(void) const
{
    return m_NameType != eNameEmpty;
}

inline
CObjectIStream::StackElement::StackElement(CObjectIStream& s)
    : m_Stream(s), m_Previous(s.m_CurrentElement),
      m_NameType(eNameEmpty)
{
    s.m_CurrentElement = this;
}

inline
CObjectIStream::StackElement::StackElement(CObjectIStream& s,
                                           const string& str)
    : m_Stream(s), m_Previous(s.m_CurrentElement)
{
    SetName(str);
    s.m_CurrentElement = this;
}

inline
CObjectIStream::StackElement::StackElement(CObjectIStream& s,
                                           const char* str)
    : m_Stream(s), m_Previous(s.m_CurrentElement)
{
    SetName(str);
    s.m_CurrentElement = this;
}

inline
CObjectIStream::StackElement::~StackElement(void)
{
    m_Stream.m_CurrentElement = m_Previous;
}

inline
CObjectIStream& CObjectIStream::StackElement::GetStream(void) const
{
    return m_Stream;
}

inline
const CObjectIStream::StackElement*
CObjectIStream::StackElement::GetPrevous(void) const
{
    return m_Previous;
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, const CMembers& members)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, members);
    _TRACE("Member: "<<in.MemberStack());
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, LastMember& lastMember)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, lastMember);
    _TRACE("Member: "<<in.MemberStack());
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, const CMemberId& member)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, member);
    _TRACE("Member: "<<in.MemberStack());
}

inline
CObjectIStream::Member::~Member(void)
{
    if ( CanClose() )
        GetStream().EndMember(*this);
}

inline
CObjectIStream::TMemberIndex CObjectIStream::Member::GetIndex(void) const
{
    return m_Index;
}

inline
void CObjectIStream::SetIndex(LastMember& lastMember, TMemberIndex index)
{
    lastMember.m_Index = index;
}

inline
void CObjectIStream::SetIndex(Member& member,
                              TMemberIndex index, const CMemberId& id)
{
    member.m_Index = index;
    member.SetName(id);
}

inline
CObjectIStream::Block::Block(CObjectIStream& in,
                             bool randomOrder)
    : StackElement(in, "E"), m_RandomOrder(randomOrder), m_NextIndex(0)
{
    in.VBegin(*this);
    _TRACE("Block: "<<in.MemberStack());
}

inline
CObjectIStream::Block::Block(CObjectIStream& in, EClass /*class*/,
                             bool randomOrder)
    : StackElement(in), m_RandomOrder(randomOrder), m_NextIndex(0)
{
    in.VBegin(*this);
    _TRACE("Block: "<<in.MemberStack());
}

inline
bool CObjectIStream::Block::Next(void)
{
    if ( !GetStream().VNext(*this) ) {
        return false;
    }
    IncIndex();
    return true;
}

inline
CObjectIStream::Block::~Block(void)
{
    if ( CanClose() ) {
        GetStream().VEnd(*this);
    }
}

inline
bool CObjectIStream::Block::RandomOrder(void) const
{
    return m_RandomOrder;
}

inline
size_t CObjectIStream::Block::GetNextIndex(void) const
{
    return m_NextIndex;
}

inline
size_t CObjectIStream::Block::GetIndex(void) const
{
    return GetNextIndex() - 1;
}

inline
bool CObjectIStream::Block::First(void) const
{
    return GetNextIndex() == 0;
}

inline
void CObjectIStream::Block::IncIndex(void)
{
    ++m_NextIndex;
}

inline
bool CObjectIStream::ByteBlock::KnownLength(void) const
{
    return m_KnownLength;
}

inline
size_t CObjectIStream::ByteBlock::GetExpectedLength(void) const
{
    return m_Length;
}

inline
CObjectIStream::ByteBlock::ByteBlock(CObjectIStream& in)
    : StackElement(in), m_KnownLength(false), m_Length(1)
{
    in.Begin(*this);
    _TRACE("ByteBlock: "<<in.MemberStack());
}

inline
CObjectIStream::ByteBlock::~ByteBlock(void)
{
    if ( CanClose() ) {
        GetStream().End(*this);
    }
}

inline
void CObjectIStream::SetBlockLength(ByteBlock& block, size_t length)
{
    block.m_Length = length;
    block.m_KnownLength = true;
}

inline
void CObjectIStream::EndOfBlock(ByteBlock& block)
{
    _ASSERT(!block.KnownLength());
    block.m_Length = 0;
}

#if HAVE_NCBI_C
inline
CObjectIStream::AsnIo::operator asnio*(void)
{
    return m_AsnIo;
}

inline
asnio* CObjectIStream::AsnIo::operator->(void)
{
    return m_AsnIo;
}

inline
const string& CObjectIStream::AsnIo::GetRootTypeName(void) const
{
    return m_RootTypeName;
}

inline
size_t CObjectIStream::AsnIo::Read(char* data, size_t length)
{
    return GetStream().AsnRead(*this, data, length);
}
#endif

#endif /* def OBJISTR__HPP  &&  ndef OBJISTR__INL */
