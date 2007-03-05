#if defined(ITEM__HPP)  &&  !defined(ITEM__INL)
#define ITEM__INL

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
const CMemberId& CItemInfo::GetId(void) const
{
    return m_Id;
}

inline
CMemberId& CItemInfo::GetId(void)
{
    return m_Id;
}

inline
TMemberIndex CItemInfo::GetIndex(void) const
{
    return m_Index;
}

inline
TPointerOffsetType CItemInfo::GetOffset(void) const
{
    return m_Offset;
}

inline
TTypeInfo CItemInfo::GetTypeInfo(void) const
{
    return m_Type.Get();
}

inline
TObjectPtr CItemInfo::GetItemPtr(TObjectPtr classPtr) const
{
    return CRawPointer::Add(classPtr, GetOffset());
}

inline
TConstObjectPtr CItemInfo::GetItemPtr(TConstObjectPtr classPtr) const
{
    return CRawPointer::Add(classPtr, GetOffset());
}

inline
bool CItemInfo::NonEmpty(void) const
{
    return m_NonEmpty;
}


#endif /* def ITEM__HPP  &&  ndef ITEM__INL */
