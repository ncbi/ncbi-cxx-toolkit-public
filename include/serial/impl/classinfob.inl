#if defined(CLASSINFOB__HPP)  &&  !defined(CLASSINFOB__INL)
#define CLASSINFOB__INL

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
const CItemsInfo& CClassTypeInfoBase::GetItems(void) const
{
    return m_Items;
}

inline
CItemsInfo& CClassTypeInfoBase::GetItems(void)
{
    return m_Items;
}

inline
const CItemInfo* CClassTypeInfoBase::GetItemInfo(const string& name) const
{
    return GetItems().GetItemInfo(GetItems().Find(name));
}

inline
const type_info& CClassTypeInfoBase::GetId(void) const
{
    _ASSERT(m_Id);
    return *m_Id;
}

inline
CClassTypeInfoBase::CIterator::CIterator(const CClassTypeInfoBase* type)
    : CParent(type->GetItems())
{
}

inline
CClassTypeInfoBase::CIterator::CIterator(const CClassTypeInfoBase* type,
                                         TMemberIndex index)
    : CParent(type->GetItems(), index)
{
}

#endif /* def CLASSINFOB__HPP  &&  ndef CLASSINFOB__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.5  2002/12/26 21:36:27  gouriano
* corrected handling choice's XML attributes
*
* Revision 1.4  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2000/10/03 17:22:31  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.2  2000/09/29 16:18:12  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.1  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
