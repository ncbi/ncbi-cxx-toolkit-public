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
* Revision 1.5  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.4  2000/09/18 20:00:21  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
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
    typedef CContainerTypeInfo::TNewIteratorResult TNewIteratorResult;

    static void Throw(const char* message)
        {
            THROW1_TRACE(runtime_error, message);
        }
    static TNewIteratorResult InitIteratorConst(const CContainerTypeInfo* ,
                                                TConstObjectPtr )
        {
            Throw("cannot create iterator");
            return TNewIteratorResult(0, false);
        }
    static TNewIteratorResult InitIterator(const CContainerTypeInfo* ,
                                           TObjectPtr )
        {
            Throw("cannot create iterator");
            return TNewIteratorResult(0, false);
        }
    static void AddElement(const CContainerTypeInfo* /*containerType*/,
                           TObjectPtr /*containerPtr*/,
                           TConstObjectPtr /*elementPtr*/)
        {
            Throw("illegal call");
        }
    
    static void AddElementIn(const CContainerTypeInfo* /*containerType*/,
                             TObjectPtr /*containerPtr*/,
                             CObjectIStream& /*in*/)
        {
            Throw("illegal call");
        }
};

void CContainerTypeInfo::InitContainerTypeInfoFunctions(void)
{
    SetReadFunction(&ReadContainer);
    SetWriteFunction(&WriteContainer);
    SetCopyFunction(&CopyContainer);
    SetSkipFunction(&SkipContainer);
    m_InitIteratorConst = &CContainerTypeInfoFunctions::InitIteratorConst;
    m_InitIterator = &CContainerTypeInfoFunctions::InitIterator;
    m_AddElement = &CContainerTypeInfoFunctions::AddElement;
    m_AddElementIn = &CContainerTypeInfoFunctions::AddElementIn;
}

void CContainerTypeInfo::SetAddElementFunctions(TAddElement addElement,
                                                TAddElementIn addElementIn)
{
    m_AddElement = addElement;
    m_AddElementIn = addElementIn;
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
                                              TEraseElement erase)
{
    m_InitIterator = init;
    m_ReleaseIterator = release;
    m_CopyIterator = copy;
    m_NextElement = next;
    m_GetElementPtr = get;
    m_EraseElement = erase;
}

bool CContainerTypeInfo::MayContainType(TTypeInfo type) const
{
    return GetElementType()->IsOrMayContainType(type);
}

void CContainerTypeInfo::Assign(TObjectPtr dst,
                                TConstObjectPtr src) const
{
    SetDefault(dst); // clear destination container
    CConstIterator i;
    if ( InitIterator(i, src) ) {
        do {
            AddElement(dst, GetElementPtr(i));
        } while ( NextElement(i) );
    }
}

bool CContainerTypeInfo::Equals(TConstObjectPtr object1,
                                TConstObjectPtr object2) const
{
    TTypeInfo elementType = GetElementType();
    CConstIterator i1, i2;
    if ( InitIterator(i1, object1) ) {
        if ( !InitIterator(i2, object2) )
            return false;
        if ( !elementType->Equals(GetElementPtr(i1),
                                  GetElementPtr(i2)) )
            return false;
        while ( NextElement(i1) ) {
            if ( !NextElement(i2) )
                return false;
            if ( !elementType->Equals(GetElementPtr(i1),
                                      GetElementPtr(i2)) )
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
