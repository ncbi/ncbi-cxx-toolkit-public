#ifndef CONTINFO__HPP
#define CONTINFO__HPP

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
* Revision 1.2  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.1  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class CConstContainerElementIterator;
class CContainerElementIterator;

class CContainerTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CContainerTypeInfo(bool randomOrder)
        : m_RandomOrder(randomOrder)
        {
        }
    CContainerTypeInfo(const string& name, bool randomOrder)
        : CParent(name), m_RandomOrder(randomOrder)
        {
        }
    CContainerTypeInfo(const char* name, bool randomOrder)
        : CParent(name), m_RandomOrder(randomOrder)
        {
        }
    virtual ETypeFamily GetTypeFamily(void) const;

    virtual TTypeInfo GetElementType(void) const = 0;

    bool RandomElementsOrder(void) const
        {
            return m_RandomOrder;
        }

    virtual
    CConstContainerElementIterator* Elements(TConstObjectPtr object) const = 0;
    virtual
    CContainerElementIterator* Elements(TObjectPtr object) const = 0;

private:
    bool m_RandomOrder;
};

class CConstContainerElementIterator
{
public:
    CConstContainerElementIterator(TTypeInfo elementTypeInfo)
        : m_ElementTypeInfo(elementTypeInfo)
        {
        }
    virtual ~CConstContainerElementIterator(void);
    virtual CConstContainerElementIterator* Clone(void) const = 0;

    TTypeInfo GetElementTypeInfo(void) const
        {
            return m_ElementTypeInfo;
        }

    virtual bool Valid(void) const = 0;
    pair<TConstObjectPtr, TTypeInfo> Get(void) const
        {
            return make_pair(GetElementPtr(), GetElementTypeInfo());
        }
    virtual void Next(void) = 0;

protected:
    virtual TConstObjectPtr GetElementPtr(void) const = 0;

private:
    TTypeInfo m_ElementTypeInfo;
};

class CContainerElementIterator
{
public:
    CContainerElementIterator(TTypeInfo elementTypeInfo)
        : m_ElementTypeInfo(elementTypeInfo)
        {
        }
    virtual ~CContainerElementIterator(void);
    virtual CContainerElementIterator* Clone(void) const = 0;

    TTypeInfo GetElementTypeInfo(void) const
        {
            return m_ElementTypeInfo;
        }

    virtual bool Valid(void) const = 0;
    pair<TObjectPtr, TTypeInfo> Get(void) const
        {
            return make_pair(GetElementPtr(), GetElementTypeInfo());
        }
    virtual void Next(void) = 0;
    virtual void Erase(void) = 0;

protected:
    virtual TObjectPtr GetElementPtr(void) const = 0;

private:
    TTypeInfo m_ElementTypeInfo;
};

//#include <serial/continfo.inl>

END_NCBI_SCOPE

#endif  /* CONTINFO__HPP */
