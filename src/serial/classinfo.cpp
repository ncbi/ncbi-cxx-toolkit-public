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
* Revision 1.4  1999/06/07 19:59:40  vasilche
* offset_t -> size_t
*
* Revision 1.3  1999/06/07 19:30:24  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:44  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:51  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CClassInfoTmpl::CClassInfoTmpl(const type_info& ti, size_t size,
                               TObjectPtr (*creator)(void))
    : CParent(ti), m_Size(size), m_Creator(creator)
{
    _TRACE(ti.name());
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& ti, size_t size,
                               TObjectPtr (*creator)(void),
                               const CTypeRef& parent, size_t offset)
    : CParent(ti), m_Size(size), m_Creator(creator)
{
    _TRACE(ti.name());
    AddMember(CMemberInfo(offset, parent));
    _TRACE(ti.name());
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

CTypeInfo::TObjectPtr CClassInfoTmpl::Create(void) const
{
    return m_Creator();
}

CClassInfoTmpl* CClassInfoTmpl::AddMember(const CMemberInfo& member)
{
    // check for conflicts
    if ( m_Members.find(member.GetName()) != m_Members.end() )
        THROW1_TRACE(runtime_error, "duplicated members: " + member.GetName());

    m_Members[member.GetName()] = member;
    return this;
}

void CClassInfoTmpl::CollectObjects(COObjectList& list,
                                    TConstObjectPtr object) const
{
    AddObject(list, object, this);
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    out.WriteObject(object, this);
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    in.ReadObject(object, this);
}


END_NCBI_SCOPE
