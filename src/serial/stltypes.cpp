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
* Revision 1.11  1999/09/23 20:38:01  vasilche
* Fixed ambiguity.
*
* Revision 1.10  1999/09/14 18:54:21  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.9  1999/09/01 17:38:13  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.8  1999/07/20 18:23:14  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
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
    CObjectOStream::Block block(out, unsigned(2));
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

END_NCBI_SCOPE
