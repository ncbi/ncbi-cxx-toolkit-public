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
* Revision 1.1  1999/05/19 19:56:52  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

// root reader
void CObjectIStream::Read(TObjectPtr data, TTypeInfo typeInfo)
{
    if ( ReadTypeInfo() != typeInfo )
        throw runtime_error("wrong type");
    ReadObjectData(data, typeInfo);
}

void CObjectIStream::ReadObjectData(TObjectPtr data, TTypeInfo typeInfo)
{

    typeInfo->Read(*this, data);
}

TObjectPtr CObjectIStream::ReadObject(TTypeInfo typeInfo)
{
    CObjectInfo oInfo;
    switch ( ReadReferenceType() ) {
    case NULL_OBJECT:
        return 0;
    case NEW_THIS_OBJECT:
        oInfo = CreateAndRegister(dataTypeInfo);
        ReadObjectData(oInfo);
        return oInfo.m_Object;
    case OLD_OBJECT:
        oInfo = m_Objects[ReadUInt()];
        break;
    case NEW_ANY_OBJECT:
        oInfo = CreateAndRegister(ReadTypeInfo());
        ReadObjectData(oInfo);
        break;
    case MEMBER_OF:
        {
            const string& mName = ReadId();
            oInfo = ReadObjectPointer();
            const CMemberInfo* mInfo = oInfo.m_TypeInfo->GetMember(mName);
            if ( !mInfo ) {
                throw runtime_error("member " + mName + " not found in class " +
                                    oInfo.m_TypeInfo->GetName());
            }
            oInfo.m_Object = mInfo->Get(oInfo.m_Object);
            oInfo.m_TypeInfo = mInfo->GetTypeInfo();
            return ToParentClass(oInfo, dataTypeInfo);
        }
        break;
    default:
        throw runtime_error("invalid object code");
    }
    return ToParentClass(SelectMember(oInfo), dataTypeInfo).m_Object;
}

CObjectInfo CObjectIStream::ReadObjectPointer(void)
{
    switch ( ReadReferenceType() ) {
    case OLD_OBJECT:
        {
            unsigned id = ReadUInt();
            if ( id >= m_Objects.size() )
                throw runtime_error("invalid back reference");
            CObjectInfo oInfo = m_Objects[id];
            SelectMember(oInfo);
            return oInfo;
        }
        break;
    case NEW_ANY_OBJECT:
        {
            TTypeInfo objectTypeInfo = ReadTypeInfo();
            CObjectInfo oInfo = CreateAndRegister(objectTypeInfo);
            ReadObjectData(oInfo.m_Object, oInfo.m_TypeInfo);
            SelectMember(oInfo);
            return oInfo;
        }
    case MEMBER_OF:
        {
            const string& mName = ReadId();
            CObjectInfo oInfo = ReadObjectPointer();
            const CMemberInfo* mInfo = oInfo.m_TypeInfo->GetMember(mName);
            if ( !mInfo ) {
                throw runtime_error("member " + mName + " not found in class " +
                                    oInfo.m_TypeInfo->GetName());
            }
            oInfo.m_Object = mInfo->Get(oInfo.m_Object);
            oInfo.m_TypeInfo = mInfo->GetTypeInfo();
            return oInfo;
        }
    default:
        throw runtime_error("invalid object code");
    }
}

CObjectInfo CObjectIStream::SelectMember(CObjectInfo oInfo)
{
    while ( ReadMemberReference() ) {
        const string& mName = ReadId();
        const CMemberInfo* mInfo = oInfo.m_TypeInfo->GetMember(mName);
        if ( !mInfo ) {
            throw runtime_error("member " + mName + " not found in class " +
                                oInfo.m_TypeInfo->GetName());
        }
        oInfo.m_Object = mInfo->Get(oInfo.m_Object);
        oInfo.m_TypeInfo = mInfo->GetTypeInfo();
    }
    return oInfo;
}

bool CObjectIStream::ReadMemberReference(void)
{
    return false;
}

const string& ReadId(void)
{
    return ReadString();
}

CObjectInfo CObjectIStream::CreateAndRegister(TTypeInfo typeInfo)
{
    CObjectInfo oInfo;
    oInfo.m_TypeInfo = typeInfo;
    oInfo.m_Object = typeInfo->Create();
    m_Objects.push_back(oInfo);
    return oInfo;
}

CObjectIStream::TTypeInfo CObjectIStream::ReadTypeInfo(void)
{
    unsigned id = ReadUInt();
    if ( id != unsigned(-1) )
        return m_Classes[id].m_TypeInfo;
    id = m_Classes.size();
    m_Classes.push_back(CClassInfo());
    TTypeInfo typeInfo;
    switch ( ReadClassType() ) {
    case TEMPLATE:
        typeInfo = CTemplateResolver::ResolveTemplate(*this);
        break;
    case CLASS:
        typeInfo = CTypeInfo::GetTypeInfo(ReadString());
        break;
    default:
        throw runtime_error("bad class code");
    }
    m_Classes[id].m_TypeInfo = typeInfo;
    m_ClassIds[typeInfo] = id;
    return typeInfo;
}

CObjectIStream::Block::Block(CObjectIStream& in, bool templ)
    : m_In(in), m_Count(0)
{
    in.BeginBlock(*this, templ);
}

bool CObjectIStream::Block::NextElement(void)
{
    return m_In.NextElement(*this);
}

CObjectIStream::Block::~Block(void)
{
    m_In.EndBlock(*this);
}
