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
* Revision 1.7  1999/07/19 15:50:39  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/15 19:35:31  vasilche
* Implemented map<K, V>.
* Changed ASN.1 text formatting.
*
* Revision 1.5  1999/07/15 16:54:50  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.4  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.3  1999/07/13 20:18:22  vasilche
* Changed types naming.
*
* Revision 1.2  1999/06/04 20:51:50  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:58  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/stltypes.hpp>

BEGIN_NCBI_SCOPE

void CStlClassInfoMapImpl::CollectKeyValuePair(COObjectList& objectList,
                                               TConstObjectPtr key,
                                               TConstObjectPtr value) const
{
    GetKeyTypeInfo()->CollectExternalObjects(objectList, key);
    GetValueTypeInfo()->CollectExternalObjects(objectList, value);
}

void CStlClassInfoMapImpl::WriteKeyValuePair(CObjectOStream& out,
                                             TConstObjectPtr key,
                                             TConstObjectPtr value) const
{
    CObjectOStream::Block block(out, size_t(2));
    block.Next();
    {
        CObjectOStream::Member m(out, CMemberId(1));
        GetKeyTypeInfo()->WriteData(out, key);
    }
    block.Next();
    {
        CObjectOStream::Member m(out, CMemberId(2));
        GetValueTypeInfo()->WriteData(out, value);
    }
}

void CStlClassInfoMapImpl::ReadKeyValuePair(CObjectIStream& in,
                                            TObjectPtr key,
                                            TObjectPtr value) const
{
    CObjectIStream::Block block(in, CObjectIStream::eFixed);
    if ( !block.Next() )
        THROW1_TRACE(runtime_error, "map key expected");
    {
        CObjectIStream::Member m(in);
        GetKeyTypeInfo()->ReadData(in, key);
    }
    if ( !block.Next() )
        THROW1_TRACE(runtime_error, "map value expected");
    {
        CObjectIStream::Member m(in);
        GetValueTypeInfo()->ReadData(in, value);
    }
    if ( block.Next() )
        THROW1_TRACE(runtime_error, "too many elements in map pair");
}

CStlClassInfoCharVector::CStlClassInfoCharVector(void)
{
}

size_t CStlClassInfoCharVector::GetSize(void) const
{
    return sizeof(TObjectType);
}

TObjectPtr CStlClassInfoCharVector::Create(void) const
{
    return new TObjectType;
}

TTypeInfo CStlClassInfoCharVector::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStlClassInfoCharVector;
    return typeInfo;
}

bool CStlClassInfoCharVector::Equals(TConstObjectPtr object1,
                                     TConstObjectPtr object2) const
{
    const TObjectType& o1 = Get(object1);
    const TObjectType& o2 = Get(object2);
    size_t length = o1.size();
    if ( length != o2.size() )
        return false;
    return memcmp(&o1.front(), &o2.front(), length) == 0;
}

void CStlClassInfoCharVector::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TObjectType& to = Get(dst);
    const TObjectType& from = Get(src);
    to = from;
}

void CStlClassInfoCharVector::WriteData(CObjectOStream& out,
                                        TConstObjectPtr object) const
{
    const TObjectType& o = Get(object);
    size_t length = o.size();
    CObjectOStream::ByteBlock block(out, length);
    if ( length > 0 )
        block.Write(&o.front(), length);
}

void CStlClassInfoCharVector::ReadData(CObjectIStream& in,
                                       TObjectPtr object) const
{
    TObjectType& o = Get(object);
    CObjectIStream::ByteBlock block(in);
    if ( block.KnownLength() ) {
        size_t length = block.GetExpectedLength();
        o.resize(length);
        if ( block.Read(&o.front(), length) != length )
            THROW1_TRACE(runtime_error, "read fault");
    }
    else {
        // length is known -> copy via buffer
        char buffer[1024];
        size_t count;
        o.clear();
        while ( (count = block.Read(buffer, sizeof(buffer))) != 0 ) {
            o.insert(o.end(), buffer, buffer + count);
        }
    }
}

END_NCBI_SCOPE
