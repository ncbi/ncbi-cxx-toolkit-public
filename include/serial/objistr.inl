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
CObjectIStream::StackElement::StackElement(CObjectIStream& s)
    : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false),
      m_NameType(eNameEmpty)
{
    s.m_CurrentElement = this;
}

inline
CObjectIStream::StackElement::StackElement(CObjectIStream& s,
                                           const string& str)
    : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false)
{
    SetName(str);
    s.m_CurrentElement = this;
}

inline
CObjectIStream::StackElement::StackElement(CObjectIStream& s,
                                           const char* str)
    : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false)
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
bool CObjectIStream::StackElement::Ended(void) const
{
    return m_Ended;
}

inline
void CObjectIStream::StackElement::End(void)
{
    _ASSERT(!Ended());
    m_Ended = true;
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, const CMembers& members)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, members);
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, LastMember& lastMember)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, lastMember);
}

inline
CObjectIStream::Member::Member(CObjectIStream& in, const CMemberId& member)
    : StackElement(in), m_Index(-1)
{
    in.StartMember(*this, member);
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
CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder)
    : StackElement(in, "E"), m_RandomOrder(randomOrder),
      m_Size(0), m_NextIndex(0)
{
    in.VBegin(*this);
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
size_t CObjectIStream::Block::GetSize(void) const
{
    return m_Size;
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
void CObjectIStream::SetBlockLength(ByteBlock& block, size_t length)
{
    block.m_Length = length;
    block.m_KnownLength = true;
}

#endif /* def OBJISTR__HPP  &&  ndef OBJISTR__INL */
