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
* Revision 1.12  1999/07/13 20:18:24  vasilche
* Changed types naming.
*
* Revision 1.11  1999/07/07 19:59:09  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.10  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.9  1999/07/01 17:55:36  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.8  1999/06/30 16:05:06  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.7  1999/06/24 14:45:02  vasilche
* Added binary ASN.1 output.
*
* Revision 1.6  1999/06/15 16:19:53  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:50  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.3  1999/06/07 19:30:28  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:51  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:59  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CTypeInfo::CTypeInfo(void)
    : m_Default(0)
{
}

CTypeInfo::CTypeInfo(const string& name)
    : m_Name(name), m_Default(0)
{
}

CTypeInfo::~CTypeInfo(void)
{
}

void CTypeInfo::CollectObjects(COObjectList& objectList,
                               TConstObjectPtr object) const
{
    if ( objectList.Add(object, this) )
        CollectExternalObjects(objectList, object);
}

void CTypeInfo::CollectExternalObjects(COObjectList& , TConstObjectPtr ) const
{
    // there is no members by default
}

TObjectPtr CTypeInfo::Create(void) const
{
    THROW1_TRACE(runtime_error, GetName() + " cannot be allocated on heap");
}

TConstObjectPtr CTypeInfo::GetDefault(void) const
{
    TConstObjectPtr def = m_Default;
    if ( !def ) {
        def = m_Default = Create();
    }
    return def;
}

TTypeInfo CTypeInfo::GetRealTypeInfo(TConstObjectPtr ) const
{
    return this;
}

CTypeInfo::TMemberIndex CTypeInfo::FindMember(const string& ) const
{
    return -1;
}

CTypeInfo::TMemberIndex CTypeInfo::LocateMember(TConstObjectPtr ,
                                                TConstObjectPtr ,
                                                TTypeInfo ) const
{
    return -1;
}

const CMemberId* CTypeInfo::GetMemberId(TMemberIndex ) const
{
    return 0;
}

const CMemberInfo* CTypeInfo::GetMemberInfo(TMemberIndex ) const
{
    return 0;
}

END_NCBI_SCOPE
