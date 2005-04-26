#if defined(CHOICE__HPP)  &&  !defined(CHOICE__INL)
#define CHOICE__INL

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

inline
TMemberIndex CChoiceTypeInfo::GetIndex(TConstObjectPtr object) const
{
    return m_WhichFunction(this, object);
}

inline
void CChoiceTypeInfo::ResetIndex(TObjectPtr object) const
{
    m_ResetFunction(this, object);
}

inline
void CChoiceTypeInfo::SetIndex(TObjectPtr object,
                               TMemberIndex index,
                               CObjectMemoryPool* memPool) const
{
    m_SelectFunction(this, object, index, memPool);
}

inline
const CItemsInfo& CChoiceTypeInfo::GetVariants(void) const
{
    return GetItems();
}

inline
const CVariantInfo* CChoiceTypeInfo::GetVariantInfo(TMemberIndex index) const
{
    return static_cast<const CVariantInfo*>(GetVariants().GetItemInfo(index));
}

inline
const CVariantInfo* CChoiceTypeInfo::GetVariantInfo(const CIterator& i) const
{
    return GetVariantInfo(*i);
}

inline
const CVariantInfo* CChoiceTypeInfo::GetVariantInfo(const string& name) const
{
    return GetVariantInfo(GetVariants().Find(name));
}


inline
TConstObjectPtr CChoiceTypeInfo::GetData(TConstObjectPtr object,
                                         TMemberIndex index) const
{
    const CVariantInfo* variantInfo = GetVariantInfo(index);
    return variantInfo->GetVariantPtr(object);
}

inline
TObjectPtr CChoiceTypeInfo::GetData(TObjectPtr object,
                                    TMemberIndex index) const
{
    const CVariantInfo* variantInfo = GetVariantInfo(index);
    return variantInfo->GetVariantPtr(object);
}

#endif /* def CHOICE__HPP  &&  ndef CHOICE__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2005/04/26 14:18:49  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.4  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2002/12/12 21:10:25  gouriano
* implemented handling of complex XML containers
*
* Revision 1.2  2000/10/03 17:22:30  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.1  2000/09/18 19:59:59  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
