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
* Revision 1.4  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:54  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/objlist.hpp>

BEGIN_NCBI_SCOPE

CObjectOStream::~CObjectOStream(void)
{
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::Write(" << unsigned(object) << ", "
           << typeInfo->GetName() << ')');
    typeInfo->CollectObjects(m_Objects, object);
    COObjectInfo info(m_Objects, object, typeInfo);
    if ( info.IsMember() ) {
        if ( !info.GetRootObjectInfo().IsWritten() )
            THROW1_TRACE(runtime_error,
                         "trying to write member of non written object");
    }
    else {
        if ( info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write already written object");
        }
    }
    m_Objects.RegisterObject(info.GetRootObjectInfo());
    typeInfo->WriteData(*this, object);
    m_Objects.CheckAllWritten();
}

void CObjectOStream::RegisterAndWrite(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::RegisterAndWrite(" << unsigned(object) << ", "
           << typeInfo->GetName() << ')');
    COObjectInfo info(m_Objects, object, typeInfo);
    if ( info.IsMember() ) {
        THROW1_TRACE(runtime_error,
                     "trying to register member");
    }
    else {
        if ( info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write already written object");
        }
    }
    m_Objects.RegisterObject(info.GetRootObjectInfo());
    typeInfo->WriteData(*this, object);
}

void CObjectOStream::WriteId(const string& id)
{
    WriteString(id);
}

void CObjectOStream::Begin(Block& , unsigned , bool )
{
}

void CObjectOStream::Next(Block& )
{
}

void CObjectOStream::End(Block& )
{
}

void CObjectOStream::RegisterClass(TTypeInfo )
{
    throw runtime_error("not implemented");
}

COClassInfo* CObjectOStream::GetRegisteredClass(TTypeInfo ) const
{
    throw runtime_error("not implemented");
}

CObjectOStream::Block::Block(CObjectOStream& out, unsigned count, bool tmpl)
    : m_Out(out), m_Count(count)
{
    m_Out.Begin(*this, count, tmpl);
}

void CObjectOStream::Block::Next(void)
{
    if ( !m_Count ) {
        THROW1_TRACE(runtime_error, "too many elements");
    }
    m_Out.Next(*this);
    --m_Count;
}

CObjectOStream::Block::~Block(void)
{
    if ( m_Count ) {
        THROW1_TRACE(runtime_error, "not all elements written");
    }
    m_Out.End(*this);
}

END_NCBI_SCOPE
