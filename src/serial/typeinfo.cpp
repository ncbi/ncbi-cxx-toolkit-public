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
* Revision 1.1  1999/05/19 19:56:59  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CTypeInfo::CTypeInfo(const string& name)
    : m_Name(name)
{
    if ( !sm_Types.insert(TTypes::value_type(name, this)).second )
        throw runtime_error("duplicated types: " + name);
}

CTypeInfo::~CTypeInfo(void)
{
    sm_Types.erase(m_Name);
}

CTypeInfo::TTypes CTypeInfo::sm_Types;

CTypeInfo::TTypeInfo CTypeInfo::GetTypeInfo(const string& name)
{
    TTypes::iterator i = sm_Types.find(name);
    if ( i == sm_Types.end() )
        throw runtime_error("type not found: "+name);
    return i->second;
}

CTypeInfo::TTypeInfo CTypeInfo::GetPointerTypeInfo(void) const
{
    CTypeInfo* info = m_PointerTypeInfo.get();
    if ( !info ) {
        m_PointerTypeInfo.reset(info = new CPointerTypeInfo(this));
    }
    return info;
}

void CTypeInfo::AddObject(CObjectList& list,
                          TConstObjectPtr object, TTypeInfo typeInfo)
{
    if ( list.Add(object, typeInfo) ) {
        // new object
        typeInfo->CollectMembers(list, object);
    }
}

void CTypeInfo::CollectMembers(CObjectList& , TConstObjectPtr ) const
{
    // there is no members by default
}

size_t CPointerTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

CTypeInfo::TObjectPtr CPointerTypeInfo::Create(void) const
{
    return new(void*);
}

void CPointerTypeInfo::CollectMembers(CObjectList& list,
                                      TConstObjectPtr object) const
{
    AddObject(list, *static_cast<TConstObjectPtr*>(object),
              GetRealDataTypeInfo(object));
}

void CPointerTypeInfo::WriteData(CObjectOStream& out,
                                 TConstObjectPtr object) const
{
    out.WriteObject(*static_cast<TConstObjectPtr*>(object),
                    GetRealDataTypeInfo(object));
}

void CPointerTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    *static_cast<TConstObjectPtr*>(object) = in.ReadObject(GetDataTypeInfo());
}

CTypeInfo::TTypeInfo CPointerTypeInfo::GetRealDataTypeInfo(TConstObjectPtr ) const
{
    return GetDataTypeInfo();
}

/*




CClassInfo::CClassInfo(const type_info& info, const CClassInfo* parent,
                       void* (*creator)())
    : m_CTypeInfo(info), m_ParentClass(parent), m_Creator(creator)
{
    if ( !sm_Types.insert(TTypeByType::value_type(&m_CTypeInfo, this)).second )
        throw runtime_error("type alrady defined");
}

CClassInfo::~CClassInfo(void)
{
    if ( !sm_Types.erase(&m_CTypeInfo) ) {
        ERR_POST("CClassInfo not in map! " << m_CTypeInfo.name());
    }
    // TODO: delete members
    m_Members.clear();
}

void CClassInfo::Read(CArchiver& archiver, TObjectPtr object) const
{
    archiver.ReadClass(object, *this);
}

void CClassInfo::Write(CArchiver& archiver, TConstObjectPtr object) const
{
    archiver.WriteClass(object, *this);
}
*/

END_NCBI_SCOPE
