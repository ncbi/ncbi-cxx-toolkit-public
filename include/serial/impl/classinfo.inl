#if defined(CLASSINFO__HPP)  &&  !defined(CLASSINFO__INL)
#define CLASSINFO__INL

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
const CItemsInfo& CClassTypeInfo::GetMembers(void) const
{
    return GetItems();
}

inline
const CMemberInfo* CClassTypeInfo::GetMemberInfo(TMemberIndex index) const
{
    return static_cast<const CMemberInfo*>(GetMembers().GetItemInfo(index));
}

inline
const CMemberInfo* CClassTypeInfo::GetMemberInfo(const CIterator& i) const
{
    return GetMemberInfo(*i);
}

inline
const CMemberInfo* CClassTypeInfo::GetMemberInfo(const CTempString& name) const
{
    return GetMemberInfo(GetMembers().Find(name));
}

inline
bool CClassTypeInfo::RandomOrder(void) const
{
    return m_ClassType == eRandom;
}

inline
bool CClassTypeInfo::Implicit(void) const
{
    return m_ClassType == eImplicit;
}

inline
const CClassTypeInfo::TSubClasses* CClassTypeInfo::SubClasses(void) const
{
    return m_SubClasses.get();
}

inline
const type_info*
CClassTypeInfo::GetCPlusPlusTypeInfo(TConstObjectPtr object) const
{
    return m_GetTypeIdFunction(object);
}

#endif /* def CLASSINFO__HPP  &&  ndef CLASSINFO__INL */
