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
* Revision 1.1  1999/09/07 20:57:57  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CTypeInfoMap<CAutoPointerTypeInfo> CAutoPointerTypeInfo::sm_Map;

void CAutoPointerTypeInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    if ( data == 0 )
        THROW1_TRACE(runtime_error, "null auto pointer");
    TTypeInfo dataType = GetDataTypeInfo();
    if ( dataType->GetRealTypeInfo(data) != dataType )
        THROW1_TRACE(runtime_error, "auto pointer have different type");
    out.WriteExternalObject(data, dataType);
}

void CAutoPointerTypeInfo::ReadData(CObjectIStream& in,
                                    TObjectPtr object) const
{
    TObjectPtr data = const_cast<TObjectPtr>(GetObjectPointer(object));
    TTypeInfo dataType = GetDataTypeInfo();
    if ( data == 0 ) {
        _TRACE("null auto pointer");
        SetObjectPointer(object, data = dataType->Create());
    }
    else if ( dataType->GetRealTypeInfo(data) != dataType )
        THROW1_TRACE(runtime_error, "auto pointer have different type");
    in.ReadExternalObject(data, dataType);
}

END_NCBI_SCOPE
