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
* Revision 1.2  2000/05/04 16:22:24  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.1  1999/05/19 19:56:27  vasilche
* Commit just in case.
*
* ===========================================================================
*/

inline
CObjectOStream::Member::Member(CObjectOStream& out, const CMemberId& member)
    : m_Out(out)
{
    out.StartMember(*this, member);
}

inline
CObjectOStream::Member::Member(CObjectOStream& out,
                               const CMembers& members, TMemberIndex index)
    : m_Out(out)
{
    out.StartMember(*this, members, index);
}

inline
CObjectOStream::Member::~Member(void)
{
    m_Out.EndMember(*this);
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
size_t CObjectOStream::ByteBlock::GetLength(void) const
{
    return m_Length;
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
    m_Out.AsnWrite(*this, data, length);
}

#endif

inline
CObjectOStream::Block::Block(CObjectOStream& out, bool randomOrder)
    : m_Out(out), m_RandomOrder(randomOrder),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

inline
void CObjectOStream::Block::Next(void)
{
    m_Out.VNext(*this);
    IncIndex();
}

inline
CObjectOStream::Block::~Block(void)
{
    m_Out.VEnd(*this);
}

inline
CObjectOStream::ByteBlock::ByteBlock(CObjectOStream& out, size_t length)
    : m_Out(out), m_Length(length)
{
    out.Begin(*this);
}

#endif /* def OBJOSTR__HPP  &&  ndef OBJOSTR__INL */
