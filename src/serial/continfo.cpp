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
* Revision 1.3  2000/09/13 15:10:15  vasilche
* Fixed type detection in type iterators.
*
* Revision 1.2  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.1  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* ===========================================================================
*/

#include <serial/continfo.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

CTypeInfo::ETypeFamily CContainerTypeInfo::GetTypeFamily(void) const
{
    return eTypeContainer;
}

CContainerTypeInfo::CIterator* CContainerTypeInfo::NewIterator(void) const
{
    _ASSERT(RandomElementsOrder());
    THROW1_TRACE(runtime_error,
                 "cannot use non const iterator for unique container");
}

bool CContainerTypeInfo::MayContainType(TTypeInfo type) const
{
    return GetElementType()->IsOrMayContainType(type);
}

bool CContainerTypeInfo::IsOrMayContainType(TTypeInfo type) const
{
    return this == type || GetElementType()->IsOrMayContainType(type);
}

void CContainerTypeInfo::Assign(TObjectPtr dst,
                                TConstObjectPtr src) const
{
    SetDefault(dst); // clear destination container
    AutoPtr<CConstIterator> i(NewConstIterator());
    if ( i->Init(src) ) {
        do {
            AddElement(dst, i->GetElementPtr());
        } while ( i->Next() );
    }
}

bool CContainerTypeInfo::Equals(TConstObjectPtr object1,
                                TConstObjectPtr object2) const
{
    TTypeInfo elementType = GetElementType();
    AutoPtr<CConstIterator> i1(NewConstIterator());
    AutoPtr<CConstIterator> i2(NewConstIterator());
    if ( i1->Init(object1) ) {
        if ( !i2->Init(object2) )
            return false;
        if ( !elementType->Equals(i1->GetElementPtr(),
                                  i2->GetElementPtr()) )
            return false;
        while ( i1->Next() ) {
            if ( !i2->Next() )
                return false;
            if ( !elementType->Equals(i1->GetElementPtr(),
                                      i2->GetElementPtr()) )
                return false;
        }
        return !i2->Next();
    }
    else {
        return !i2->Init(object2);
    }
}

void CContainerTypeInfo::ReadData(CObjectIStream& in,
                                  TObjectPtr container) const
{
    in.ReadContainer(container, this);
}

void CContainerTypeInfo::SkipData(CObjectIStream& in) const
{
    in.SkipContainer(this);
}

void CContainerTypeInfo::WriteData(CObjectOStream& out,
                                   TConstObjectPtr container) const
{
    out.WriteContainer(container, this);
}

void CContainerTypeInfo::CopyData(CObjectStreamCopier& copier) const
{
    copier.CopyContainer(this);
}

void CContainerTypeInfo::ThrowDuplicateElementError(void) const
{
    _ASSERT(RandomElementsOrder());
    THROW1_TRACE(runtime_error,
                 "duplicate element of unique container");
}

CContainerTypeInfo::CConstIterator::~CConstIterator(void)
{
}

CContainerTypeInfo::CIterator::~CIterator(void)
{
}

END_NCBI_SCOPE
