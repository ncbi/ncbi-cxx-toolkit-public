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
*   Serialization classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/03/25 19:12:04  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>

BEGIN_NCBI_SCOPE



CTypeInfo::~CTypeInfo(void)
{
}

const CTypeInfo& CTypeInfo::GetPointerTypeInfo(void) const
{
    CTypeInfo* info = m_PointerTypeInfo.get();
    if ( !info ) {
        m_PointerTypeInfo.reset(info = new CPointerTypeInfo(*this));
    }
    return *info;
}



void* CPointerTypeInfo::Create(void) const
{
    return new void*;
}

void CPointerTypeInfo::Read(CArchiver& archiver, TObjectPtr object) const
{
    Get(object) = m_ObjectTypeInfo.Create();
    m_ObjectTypeInfo.Read(archiver, Get(object));
}

void CPointerTypeInfo::Write(CArchiver& archiver, TConstObjectPtr object) const
{
    if ( Get(object) == 0 )
        throw runtime_error("cannot write null pointer");

    m_ObjectTypeInfo.Write(archiver, Get(object));
}

bool CPointerTypeInfo::HasDefaultValue(TConstObjectPtr object) const
{
    return Get(object) == 0;
}



CClassInfo::TTypeByType CClassInfo::sm_Types;

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



void CArchiver::WriteString(const string& str)
{
    WriteCString(str.c_str());
}

void CArchiver::WriteCString(const char* const& str)
{
    WriteString(str);
}

void CArchiver::ReadString(string& str)
{
    char* cstr;
    ReadCString(cstr);
    str = cstr;
    delete[] cstr;
}

void CArchiver::ReadCString(char*& cstr)
{
    string str;
    ReadString(str);
    cstr = new char[str.length() + 1];
    str.copy(cstr, NPOS);
    cstr[str.length()] = 0;
}

void CArchiver::ReadClass(TObjectPtr object, const CClassInfo& info)
{
    string memberName;
    while ( !(memberName = ReadMemberName()).empty() ) {
        const CMemberInfo* m = info.FindMember(memberName);
        if ( !m )
            SkipMemberValue(info, memberName);
        else
            m->GetTypeInfo().Read(*this, m->GetMemeberPtr(object));
    }
}

void CArchiver::WriteClass(TConstObjectPtr object, const CClassInfo& info)
{
    for ( CClassInfo::TMemberIterator i = info.MemberBegin();
          i != info.MemberEnd();
          ++i ) {
        const CMemberInfo* m = i->second;
        const CTypeInfo& mInfo = m->GetTypeInfo();
        TConstObjectPtr mPtr = m->GetMemberPtr(object);
        if ( !mInfo.HasDefaultValue(mPtr) ) {
            WriteMemberName(i->first);
            mInfo.Write(*this, mPtr);
        }
    }
}

END_NCBI_SCOPE
