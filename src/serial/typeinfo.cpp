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
* Revision 1.20  2000/05/24 20:08:50  vasilche
* Implemented XML dump.
*
* Revision 1.19  2000/03/29 15:55:30  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.18  2000/02/17 20:02:46  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.17  1999/12/17 19:05:05  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.16  1999/09/14 18:54:21  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.15  1999/08/31 17:50:09  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.14  1999/07/20 18:23:14  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.13  1999/07/19 15:50:39  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
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

#include <serial/typeinfo.hpp>
#include <serial/objlist.hpp>

BEGIN_NCBI_SCOPE

CTypeInfo::CTypeInfo(void)
{
}

CTypeInfo::CTypeInfo(const string& name)
    : m_Name(name)
{
}

CTypeInfo::CTypeInfo(const char* name)
    : m_Name(name)
{
}

CTypeInfo::~CTypeInfo(void)
{
}

TObjectPtr CTypeInfo::Create(void) const
{
    THROW1_TRACE(runtime_error, GetName() + " cannot be allocated on heap");
}

void CTypeInfo::Delete(TObjectPtr /*object*/) const
{
    THROW1_TRACE(runtime_error, GetName() + " cannot be allocated on heap");
}

void CTypeInfo::DeleteExternalObjects(TObjectPtr /*object*/) const
{
}

TTypeInfo CTypeInfo::GetRealTypeInfo(TConstObjectPtr ) const
{
    return this;
}

TTypeInfo CTypeInfo::GetParentTypeInfo(void) const
{
    return 0;
}

TTypeInfo CTypeInfo::GetPointedTypeInfo(void) const
{
    return 0;
}

void CTypeInfo::GetPointedObject(CConstObjectInfo& /*object*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::GetPointedObject(CObjectInfo& /*object*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

bool CTypeInfo::IsType(TTypeInfo typeInfo) const
{
    return this == typeInfo;
}

bool CTypeInfo::MayContainType(TTypeInfo /*typeInfo*/) const
{
    return false;
}

bool CTypeInfo::HaveChildren(TConstObjectPtr /*object*/) const
{
    return false;
}

void CTypeInfo::BeginTypes(CChildrenTypesIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::Begin(CConstChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::Begin(CChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

bool CTypeInfo::ValidTypes(const CChildrenTypesIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

bool CTypeInfo::Valid(const CConstChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

bool CTypeInfo::Valid(const CChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

TTypeInfo CTypeInfo::GetChildType(const CChildrenTypesIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::GetChild(const CConstChildrenIterator& /*cc*/,
                         CConstObjectInfo& /*child*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::GetChild(const CChildrenIterator& /*cc*/,
                         CObjectInfo& /*child*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::NextType(CChildrenTypesIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::Next(CConstChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::Next(CChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void CTypeInfo::Erase(CChildrenIterator& /*cc*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

END_NCBI_SCOPE
