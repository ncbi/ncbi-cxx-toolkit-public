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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/impl/objlist.hpp>
#include <serial/typeinfo.hpp>
#include <serial/impl/member.hpp>
#include <serial/impl/typeinfoimpl.hpp>

#undef _TRACE
#define _TRACE(arg) ((void)0)

BEGIN_NCBI_SCOPE

CWriteObjectList::CWriteObjectList(void)
{
}

CWriteObjectList::~CWriteObjectList(void)
{
}

void CWriteObjectList::Clear(void)
{
    m_Objects.clear();
    m_ObjectsByPtr.clear();
}

void CWriteObjectList::RegisterObject(TTypeInfo typeInfo)
{
    m_Objects.push_back(CWriteObjectInfo(typeInfo, NextObjectIndex()));
}

#if _DEBUG
static inline
TConstObjectPtr EndOf(TConstObjectPtr objectPtr, TTypeInfo objectType)
{
    return CRawPointer::Add(objectPtr, TPointerOffsetType(objectType->GetSize()));
}
#endif

const CWriteObjectInfo*
CWriteObjectList::RegisterObject(TConstObjectPtr object, TTypeInfo typeInfo)
{
    _TRACE("CWriteObjectList::RegisterObject("<<NStr::PtrToString(object)<<
           ", "<<typeInfo->GetName()<<") size: "<<typeInfo->GetSize()<<
           ", end: "<<NStr::PtrToString(EndOf(object, typeInfo)));
    TObjectIndex index = NextObjectIndex();
    CWriteObjectInfo info(object, typeInfo, index);
    
    if ( info.GetObjectRef() ) {
        // special case with cObjects
        if ( info.GetObjectRef()->ReferencedOnlyOnce() ) {
            // unique reference -> do not remember pointer
            // in debug mode check for absence of other references
#if _DEBUG
            pair<TObjectsByPtr::iterator, bool> ins =
                m_ObjectsByPtr.insert(TObjectsByPtr::value_type(object, index));
            if ( !ins.second ) {
                // not inserted -> already have the same object pointer
                // as reference counter is one -> error
                NCBI_THROW(CSerialException,eIllegalCall,
                             "double write of CObject with counter == 1");
            }
#endif
            // new object
            m_Objects.push_back(info);
            return 0;
        }
        else if ( info.GetObjectRef()->Referenced() ) {
            // multiple reference -> normal processing
        }
        else {
            // not referenced -> error
            NCBI_THROW(CSerialException,eIllegalCall,
                         "registering non referenced CObject");
        }
    }

    pair<TObjectsByPtr::iterator, bool> ins =
        m_ObjectsByPtr.insert(TObjectsByPtr::value_type(object, index));

    if ( !ins.second ) {
        // not inserted -> already have the same object pointer
        TObjectIndex oldIndex = ins.first->second;
        CWriteObjectInfo& objectInfo = m_Objects[oldIndex];
        _ASSERT(objectInfo.GetTypeInfo() == typeInfo);
        return &objectInfo;
    }

    // new object
    m_Objects.push_back(info);

#if _DEBUG
    // check for overlapping with previous object
    TObjectsByPtr::iterator check = ins.first;
    if ( check != m_ObjectsByPtr.begin() ) {
        --check;
        if ( EndOf(check->first,
                   m_Objects[check->second].GetTypeInfo()) > object )
            NCBI_THROW(CSerialException,eFail, "overlapping objects");
    }

    // check for overlapping with next object
    check = ins.first;
    if ( ++check != m_ObjectsByPtr.end() ) {
        if ( EndOf(object, typeInfo) > check->first )
            NCBI_THROW(CSerialException,eFail, "overlapping objects");
    }
#endif

    return 0;
}

void CWriteObjectList::ForgetObjects(TObjectIndex from, TObjectIndex to)
{
    _ASSERT(from <= to);
    _ASSERT(to <= GetObjectCount());
    for ( TObjectIndex i = from; i < to; ++i ) {
        CWriteObjectInfo& info = m_Objects[i];
        TConstObjectPtr objectPtr = info.GetObjectPtr();
        if ( objectPtr ) {
            m_ObjectsByPtr.erase(objectPtr);
            info.ResetObjectPtr();
        }
    }
}

CReadObjectList::CReadObjectList(void)
{
}

CReadObjectList::~CReadObjectList(void)
{
}

void CReadObjectList::Clear(void)
{
    m_Objects.clear();
}

void CReadObjectList::RegisterObject(TTypeInfo typeInfo)
{
    m_Objects.push_back(CReadObjectInfo(typeInfo));
}

void CReadObjectList::RegisterObject(TObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_Objects.push_back(CReadObjectInfo(objectPtr, typeInfo));
}

const CReadObjectInfo&
CReadObjectList::GetRegisteredObject(TObjectIndex index) const
{
    if ( index >= GetObjectCount() )
        NCBI_THROW(CSerialException,eFail, "invalid object index");
    return m_Objects[index];
}

void CReadObjectList::ForgetObjects(TObjectIndex from, TObjectIndex to)
{
    _ASSERT(from <= to);
    _ASSERT(to <= GetObjectCount());
    for ( TObjectIndex i = from; i < to; ++i ) {
        m_Objects[i].ResetObjectPtr();
    }
}

END_NCBI_SCOPE
