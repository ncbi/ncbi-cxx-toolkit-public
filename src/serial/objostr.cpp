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
* Revision 1.9  1999/06/30 16:04:59  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:06  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:19:50  vasilche
* Added ASN.1 object output stream.
*
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
#include <serial/memberid.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

CObjectOStream::~CObjectOStream(void)
{
}

void CObjectOStream::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectOStream::Write(" << unsigned(object) << ", "
           << typeInfo->GetName() << ')');
    _TRACE("CTypeInfo::CollectObjects: " << unsigned(object));
    typeInfo->CollectObjects(m_Objects, object);
    COObjectInfo info(m_Objects, object, typeInfo);
    if ( info.IsMember() ) {
        if ( !info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write member of non written object");
        }
    }
    else {
        if ( info.GetRootObjectInfo().IsWritten() ) {
            THROW1_TRACE(runtime_error,
                         "trying to write already written object");
        }
        m_Objects.RegisterObject(info.GetRootObjectInfo());
        _TRACE("CTypeInfo::Write: " << unsigned(object)
               << " @" << info.GetRootObjectInfo().GetIndex());
    }
    WriteData(object, typeInfo);
    m_Objects.CheckAllWritten();
}

void CObjectOStream::WriteExternalObject(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
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
        m_Objects.RegisterObject(info.GetRootObjectInfo());
        _TRACE("CTypeInfo::Write: " << unsigned(object)
               << " @" << info.GetRootObjectInfo().GetIndex());
    }
    WriteData(object, typeInfo);
}

void CObjectOStream::WritePointer(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("WritePointer(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    if ( object == 0 ) {
        _TRACE("WritePointer: " << unsigned(object) << ": null");
        WriteNullPointer();
        return;
    }

    COObjectInfo info(m_Objects, object, typeInfo);

    // find if this object is part of another object
    if ( info.IsMember() ) {
        WriteMemberPrefix(info);
    }

    const CORootObjectInfo& root = info.GetRootObjectInfo();
    if ( root.IsWritten() ) {
        // put reference on it
        _TRACE("WritePointer: " << unsigned(object) << ": @" << root.GetIndex());
        WriteObjectReference(root.GetIndex());
    }
    else {
        // new object
        TTypeInfo realTypeInfo = root.GetTypeInfo();
        if ( typeInfo == realTypeInfo ) {
            _TRACE("WritePointer: " << unsigned(object) << ": new");
            WriteThisTypeReference(realTypeInfo);
        }
        else {
            _TRACE("WritePointer: " << unsigned(object) << ": new "
                   << realTypeInfo->GetName());
            WriteOtherTypeReference(realTypeInfo);
        }
        WriteExternalObject(info.GetRootObject(), realTypeInfo);
    }

    if ( info.IsMember() ) {
        WriteMemberSuffix(info);
    }
}

void CObjectOStream::WriteMemberPrefix(COObjectInfo& )
{
}

void CObjectOStream::WriteMemberSuffix(COObjectInfo& )
{
}

void CObjectOStream::WriteThisTypeReference(TTypeInfo )
{
}

void CObjectOStream::WriteId(const string& id)
{
    WriteString(id);
}

void CObjectOStream::WriteMember(const CMemberId& member)
{
    WriteId(member.GetName());
}

void CObjectOStream::FBegin(Block& block)
{
    SetNonFixed(block);
    VBegin(block);
}

void CObjectOStream::FNext(const Block& )
{
}

void CObjectOStream::FEnd(const Block& )
{
}

void CObjectOStream::VBegin(Block& )
{
}

void CObjectOStream::VNext(const Block& )
{
}

void CObjectOStream::VEnd(const Block& )
{
}

CObjectOStream::Block::Block(CObjectOStream& out)
    : m_Out(out), m_Fixed(false), m_Sequence(false),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, unsigned size)
    : m_Out(out), m_Fixed(true), m_Sequence(false),
      m_NextIndex(0), m_Size(size)
{
    out.FBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, ESequence )
    : m_Out(out), m_Fixed(false), m_Sequence(true),
      m_NextIndex(0), m_Size(0)
{
    out.VBegin(*this);
}

CObjectOStream::Block::Block(CObjectOStream& out, ESequence , unsigned size)
    : m_Out(out), m_Fixed(true), m_Sequence(true),
      m_NextIndex(0), m_Size(size)
{
    out.FBegin(*this);
}

void CObjectOStream::Block::Next(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            THROW1_TRACE(runtime_error, "too many elements");
        }
        m_Out.FNext(*this);
    }
    else {
        m_Out.VNext(*this);
    }
    IncIndex();
}

CObjectOStream::Block::~Block(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            THROW1_TRACE(runtime_error, "not all elements written");
        }
        m_Out.FEnd(*this);
    }
    else {
        m_Out.VEnd(*this);
    }
}

END_NCBI_SCOPE
