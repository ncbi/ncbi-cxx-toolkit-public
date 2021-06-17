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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_TypeInfo

BEGIN_NCBI_SCOPE

CContainerTypeInfo::CContainerTypeInfo(size_t size,
                                       TTypeInfo elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

CContainerTypeInfo::CContainerTypeInfo(size_t size,
                                       const CTypeRef& elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

CContainerTypeInfo::CContainerTypeInfo(size_t size, const char* name,
                                       TTypeInfo elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size, name),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

CContainerTypeInfo::CContainerTypeInfo(size_t size, const char* name,
                                       const CTypeRef& elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size, name),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

CContainerTypeInfo::CContainerTypeInfo(size_t size, const string& name,
                                       TTypeInfo elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size, name),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

CContainerTypeInfo::CContainerTypeInfo(size_t size, const string& name,
                                       const CTypeRef& elementType,
                                       bool randomOrder)
    : CParent(eTypeFamilyContainer, size, name),
      m_ElementType(elementType), m_RandomOrder(randomOrder)
{
    InitContainerTypeInfoFunctions();
}

class CContainerTypeInfoFunctions
{
public:
    NCBI_NORETURN
    static void Throw(const char* message)
        {
            NCBI_THROW(CSerialException,eFail, message);
        }
    static bool InitIteratorConst(CContainerTypeInfo::CConstIterator&)
        {
            Throw("cannot create iterator");
            return false;
        }
    static bool InitIterator(CContainerTypeInfo::CIterator&)
        {
            Throw("cannot create iterator");
            return false;
        }
    static TObjectPtr AddElement(const CContainerTypeInfo* /*containerType*/,
                                 TObjectPtr /*containerPtr*/,
                                 TConstObjectPtr /*elementPtr*/,
                                 ESerialRecursionMode)
        {
            Throw("illegal call");
            return 0;
        }
    
    static TObjectPtr AddElementIn(const CContainerTypeInfo* /*containerType*/,
                                   TObjectPtr /*containerPtr*/,
                                   CObjectIStream& /*in*/)
        {
            Throw("illegal call");
            return 0;
        }

    static size_t GetElementCount(const CContainerTypeInfo*, TConstObjectPtr)
        {
            Throw("illegal call");
            return 0;
        }
};

void CContainerTypeInfo::InitContainerTypeInfoFunctions(void)
{
    SetTag(m_RandomOrder ? CAsnBinaryDefs::eSetOf : CAsnBinaryDefs::eSequenceOf);
    SetReadFunction(&ReadContainer);
    SetWriteFunction(&WriteContainer);
    SetCopyFunction(&CopyContainer);
    SetSkipFunction(&SkipContainer);
    m_InitIteratorConst = &CContainerTypeInfoFunctions::InitIteratorConst;
    m_InitIterator = &CContainerTypeInfoFunctions::InitIterator;
    m_AddElement = &CContainerTypeInfoFunctions::AddElement;
    m_AddElementIn = &CContainerTypeInfoFunctions::AddElementIn;
    m_GetElementCount = &CContainerTypeInfoFunctions::GetElementCount;
}

void CContainerTypeInfo::SetAddElementFunctions(TAddElement addElement,
                                                TAddElementIn addElementIn)
{
    m_AddElement = addElement;
    m_AddElementIn = addElementIn;
}

void CContainerTypeInfo::SetCountFunctions(TGetElementCount getElementCount,
                                           TReserveElements reserveElements)
{
    m_GetElementCount = getElementCount;
    m_ReserveElements = reserveElements;
}

void CContainerTypeInfo::SetConstIteratorFunctions(TInitIteratorConst init,
                                                   TReleaseIteratorConst release,
                                                   TCopyIteratorConst copy,
                                                   TNextElementConst next,
                                                   TGetElementPtrConst get)
{
    m_InitIteratorConst = init;
    m_ReleaseIteratorConst = release;
    m_CopyIteratorConst = copy;
    m_NextElementConst = next;
    m_GetElementPtrConst = get;
}

void CContainerTypeInfo::SetIteratorFunctions(TInitIterator init,
                                              TReleaseIterator release,
                                              TCopyIterator copy,
                                              TNextElement next,
                                              TGetElementPtr get,
                                              TEraseElement erase,
                                              TEraseAllElements erase_all)
{
    m_InitIterator = init;
    m_ReleaseIterator = release;
    m_CopyIterator = copy;
    m_NextElement = next;
    m_GetElementPtr = get;
    m_EraseElement = erase;
    m_EraseAllElements = erase_all;
}

CTypeInfo::EMayContainType
CContainerTypeInfo::GetMayContainType(TTypeInfo type) const
{
    return GetElementType()->IsOrMayContainType(type);
}

void CContainerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                                ESerialRecursionMode how) const
{
    if (how == eShallowChildless) {
        SetDefault(dst); // clear destination container
        return;
    }
    CIterator idst;
    CConstIterator isrc;
    bool old_element = InitIterator(idst,dst);
    if ( InitIterator(isrc, src) ) {
        TTypeInfo elementType = GetElementType();
        do {
            TConstObjectPtr elementPtr = GetElementPtr(isrc);
            if (old_element) {
                elementType->Assign(GetElementPtr(idst), elementPtr, how);
                old_element = NextElement(idst);
            } else {
                AddElement(dst, elementPtr, how);
            }
        } while ( NextElement(isrc) );
    }
    if (old_element) {
        EraseAllElements(idst);
    }
}

bool CContainerTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                                ESerialRecursionMode how) const
{
    if (how == eShallowChildless) {
        return true;
    }
    TTypeInfo elementType = GetElementType();
    CConstIterator i1, i2;
    if ( InitIterator(i1, object1) ) {
        if ( !InitIterator(i2, object2) )
            return false;
        if ( !elementType->Equals(GetElementPtr(i1),
                                  GetElementPtr(i2), how) )
            return false;
        while ( NextElement(i1) ) {
            if ( !NextElement(i2) )
                return false;
            if ( !elementType->Equals(GetElementPtr(i1),
                                      GetElementPtr(i2), how) )
                return false;
        }
        return !NextElement(i2);
    }
    else {
        return !InitIterator(i2, object2);
    }
}

void CContainerTypeInfo::ReadContainer(CObjectIStream& in,
                                       TTypeInfo objectType,
                                       TObjectPtr objectPtr)
{
    const CContainerTypeInfo* containerType =
        CTypeConverter<CContainerTypeInfo>::SafeCast(objectType);

    in.ReadContainer(containerType, objectPtr);
}

void CContainerTypeInfo::WriteContainer(CObjectOStream& out,
                                        TTypeInfo objectType,
                                        TConstObjectPtr objectPtr)
{
    const CContainerTypeInfo* containerType =
        CTypeConverter<CContainerTypeInfo>::SafeCast(objectType);

    out.WriteContainer(containerType, objectPtr);
}

void CContainerTypeInfo::CopyContainer(CObjectStreamCopier& copier,
                                       TTypeInfo objectType)
{
    const CContainerTypeInfo* containerType =
        CTypeConverter<CContainerTypeInfo>::SafeCast(objectType);

    copier.CopyContainer(containerType);
}

void CContainerTypeInfo::SkipContainer(CObjectIStream& in,
                                       TTypeInfo objectType)
{
    const CContainerTypeInfo* containerType =
        CTypeConverter<CContainerTypeInfo>::SafeCast(objectType);

    in.SkipContainer(containerType);
}

END_NCBI_SCOPE
