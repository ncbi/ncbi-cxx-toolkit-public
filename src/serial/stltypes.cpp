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
    CObjectOStream::Block block(out, 2);
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
}

END_NCBI_SCOPE
