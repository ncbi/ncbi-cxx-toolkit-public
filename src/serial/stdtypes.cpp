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
* Revision 1.7  1999/07/07 21:56:35  vasilche
* Fixed problem with static variables in templates on SunPro
*
* Revision 1.6  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.5  1999/07/07 19:59:08  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.4  1999/06/30 16:05:05  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:45:01  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/stdtypes.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

size_t CStdTypeInfo<void>::GetSize(void) const
{
    return 0;
}

TTypeInfo CStdTypeInfo<void>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

bool CStdTypeInfo<void>::Equals(TConstObjectPtr , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be compared");
}

void CStdTypeInfo<void>::Assign(TObjectPtr , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be assigned");
}

void CStdTypeInfo<void>::ReadData(CObjectIStream& , TObjectPtr ) const
{
    throw runtime_error("void cannot be read");
}
    
void CStdTypeInfo<void>::WriteData(CObjectOStream& , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be written");
}

TObjectPtr CStdTypeInfo<string>::Create(void) const
{
    return new TObjectType();
}

TConstObjectPtr CStdTypeInfo<string>::GetDefault(void) const
{
    return &NcbiEmptyString;
}

TTypeInfo CStdTypeInfo<string>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

void CStdTypeInfo<string>::ReadData(CObjectIStream& in, TObjectPtr object) const
{
	in.ReadStr(Get(object));
}

void CStdTypeInfo<string>::WriteData(CObjectOStream& out, TConstObjectPtr object) const
{
	out.WriteStr(Get(object));
}

static const char* const zeroPointer = 0;

TTypeInfo CStdTypeInfo<const char*>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

TConstObjectPtr CStdTypeInfo<const char*>::GetDefault(void) const
{
    return &zeroPointer;
}

void CStdTypeInfo<const char*>::ReadData(CObjectIStream& in,
                                         TObjectPtr object) const
{
    in.ReadStr(Get(object));
}

void CStdTypeInfo<const char*>::WriteData(CObjectOStream& out,
                                          TConstObjectPtr object) const
{
    out.WriteStr(Get(object));
}

TTypeInfo CStdTypeInfo<char*>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

TConstObjectPtr CStdTypeInfo<char*>::GetDefault(void) const
{
    return &zeroPointer;
}

void CStdTypeInfo<char*>::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    in.ReadStr(Get(object));
}

void CStdTypeInfo<char*>::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    out.WriteStr(Get(object));
}

END_NCBI_SCOPE
