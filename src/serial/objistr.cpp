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
* Revision 1.13  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.12  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.11  1999/07/02 21:31:54  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:29  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:53  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:54  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:30  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:19:48  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:14:57  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:52  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

// root reader
void CObjectIStream::Read(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

void CObjectIStream::ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CObjectIStream::Read(" << unsigned(object) << ", "
           << typeInfo->GetName() << ")");
    TIndex index = RegisterObject(object, typeInfo);
    _TRACE("CObjectIStream::ReadData(" << unsigned(object) << ", "
           << typeInfo->GetName() << ") @" << index);
    ReadData(object, typeInfo);
}

void CObjectIStream::SkipValue(void)
{
    THROW1_TRACE(runtime_error, "cannot skip value");
}

void CObjectIStream::FBegin(Block& block)
{
    SetNonFixed(block);
    VBegin(block);
}

void CObjectIStream::VBegin(Block& )
{
}

void CObjectIStream::FNext(const Block& )
{
}

bool CObjectIStream::VNext(const Block& )
{
    return false;
}

void CObjectIStream::FEnd(const Block& )
{
}

void CObjectIStream::VEnd(const Block& )
{
}

void CObjectIStream::EndMember(const Member& )
{
}

CObjectIStream::Block::Block(CObjectIStream& in)
    : m_In(in), m_Fixed(false), m_RandomOrder(false),
      m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, EFixed )
    : m_In(in), m_Fixed(true), m_RandomOrder(false),
      m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.FBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder)
    : m_In(in), m_Fixed(false), m_RandomOrder(randomOrder),
      m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.VBegin(*this);
}

CObjectIStream::Block::Block(CObjectIStream& in, bool randomOrder , EFixed )
    : m_In(in), m_Fixed(true), m_RandomOrder(randomOrder),
      m_Finished(false), m_NextIndex(0), m_Size(0)
{
    in.FBegin(*this);
}

bool CObjectIStream::Block::Next(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() >= GetSize() ) {
            return false;
        }
        m_In.FNext(*this);
    }
    else {
        if ( Finished() ) {
            return false;
        }
        if ( !m_In.VNext(*this) ) {
            m_Finished = true;
            return false;
        }
    }
    IncIndex();
    return true;
}

CObjectIStream::Block::~Block(void)
{
    if ( Fixed() ) {
        if ( GetNextIndex() != GetSize() ) {
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.FEnd(*this);
    }
    else {
        if ( !Finished() ) {
            THROW1_TRACE(runtime_error, "not all elements read");
        }
        m_In.VEnd(*this);
    }
}

void CObjectIStream::ReadStr(string& data)
{
	data = ReadString();
}

void CObjectIStream::ReadStr(const char*& data)
{
	data = ReadCString();
}

void CObjectIStream::ReadStr(char*& data)
{
	data = ReadCString();
}

char* CObjectIStream::ReadCString(void)
{
	return strdup(ReadString().c_str());
}

CObjectIStream::TIndex CObjectIStream::RegisterObject(TObjectPtr object,
                                                      TTypeInfo typeInfo)
{
    TIndex index = m_Objects.size();
    m_Objects.push_back(CIObjectInfo(object, typeInfo));
    return index;
}

const CIObjectInfo& CObjectIStream::GetRegisteredObject(TIndex index) const
{
    if ( index >= m_Objects.size() ) {
        THROW1_TRACE(runtime_error, "invalid object index");
    }
    return m_Objects[index];
}

END_NCBI_SCOPE

