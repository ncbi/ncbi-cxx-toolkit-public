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
* Revision 1.28  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.27  2003/12/01 19:04:23  grichenk
* Moved Add and Sub from serialutil to ncbimisc, made them methods
* of CRawPointer class.
*
* Revision 1.26  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.25  2002/08/30 16:22:21  vasilche
* Removed excessive _TRACEs
*
* Revision 1.24  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.23  2000/10/20 15:51:41  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.22  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.21  2000/10/03 17:22:44  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.20  2000/09/18 20:00:24  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.19  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.18  2000/08/15 20:04:05  vasilche
* Fixed typo.
*
* Revision 1.17  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.16  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.15  2000/04/06 16:10:59  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.14  2000/03/29 15:55:28  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.13  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.12  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.11  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/20 18:23:11  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.8  1999/07/01 17:55:32  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.7  1999/06/30 16:04:57  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.6  1999/06/24 14:44:57  vasilche
* Added binary ASN.1 output.
*
* Revision 1.5  1999/06/16 20:35:33  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.4  1999/06/15 16:19:49  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:48  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/07 19:30:26  vasilche
* More bug fixes
*
* Revision 1.1  1999/06/04 20:51:46  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/objlist.hpp>
#include <serial/typeinfo.hpp>
#include <serial/member.hpp>
#include <serial/typeinfoimpl.hpp>

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

static inline
TConstObjectPtr EndOf(TConstObjectPtr objectPtr, TTypeInfo objectType)
{
    return CRawPointer::Add(objectPtr, TPointerOffsetType(objectType->GetSize()));
}

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
