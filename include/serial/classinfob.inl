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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

inline
CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(false)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

inline
CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const char* name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(true)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

inline
CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const void* /*nonCObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(false)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

inline
CClassTypeInfoBase::CClassTypeInfoBase(ETypeFamily typeFamily,
                                       size_t size, const string& name,
                                       const CObject* /*cObject*/,
                                       TTypeCreate createFunc,
                                       const type_info& ti)
    : CParent(typeFamily, size, name), m_IsCObject(true)
{
    InitClassTypeInfoBase(ti);
    SetCreateFunction(createFunc);
}

inline
const CMembersInfo& CClassTypeInfoBase::GetItems(void) const
{
    return m_Items;
}

inline
const type_info& CClassTypeInfoBase::GetId(void) const
{
    _ASSERT(m_Id);
    return *m_Id;
}

inline
CMembersInfo& CClassTypeInfoBase::GetItems(void)
{
    return m_Items;
}

inline
CClassTypeInfoBase::CIterator::CIterator(const CClassTypeInfoBase* type)
    : m_CurrentIndex(type->GetItems().FirstIndex()),
      m_LastIndex(type->GetItems().LastIndex())
{
}

inline
CClassTypeInfoBase::CIterator::CIterator(const CClassTypeInfoBase* type,
                                         TMemberIndex index)
    : m_CurrentIndex(index),
      m_LastIndex(type->GetItems().LastIndex())
{
    _ASSERT(index >= kFirstMemberIndex);
    _ASSERT(index <= (m_LastIndex + 1));
}

inline
CClassTypeInfoBase::CIterator&
CClassTypeInfoBase::CIterator::operator=(TMemberIndex index)
{
    _ASSERT(index >= kFirstMemberIndex);
    _ASSERT(index <= (m_LastIndex + 1));
    m_CurrentIndex = index;
    return *this;
}

inline
bool CClassTypeInfoBase::CIterator::Valid(void) const
{
    return m_CurrentIndex <= m_LastIndex;
}

inline
CClassTypeInfoBase::CIterator::operator bool(void) const
{
    return Valid();
}

inline
bool CClassTypeInfoBase::CIterator::operator!(void) const
{
    return !Valid();
}

inline
void CClassTypeInfoBase::CIterator::Next(void)
{
    ++m_CurrentIndex;
}

inline
void CClassTypeInfoBase::CIterator::operator++(void)
{
    Next();
}

inline
TMemberIndex CClassTypeInfoBase::CIterator::GetIndex(void) const
{
    return m_CurrentIndex;
}

inline
TMemberIndex CClassTypeInfoBase::CIterator::operator*(void) const
{
    return GetIndex();
}

#endif /* def CLASSINFOB__HPP  &&  ndef CLASSINFOB__INL */
