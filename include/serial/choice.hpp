#ifndef CHOICE__HPP
#define CHOICE__HPP

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
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CChoiceTypeInfoBase : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    CChoiceTypeInfoBase(const string& name);

    void AddVariant(const CMemberId& id, const CTypeRef& type);
    void AddVariant(const string& name, const CTypeRef& type);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    TMemberIndex GetVariantsCount(void) const
        { return m_VariantTypes.size(); }

    TTypeInfo GetVariantTypeInfo(TMemberIndex index) const
        { return m_VariantTypes[index].Get(); }

protected:
    virtual TMemberIndex GetIndex(TConstObjectPtr object) const = 0;
    virtual void SetIndex(TObjectPtr object, TMemberIndex index) const = 0;
    virtual TObjectPtr x_GetData(TObjectPtr object) const = 0;
    TConstObjectPtr GetData(TConstObjectPtr object) const
        {
            return x_GetData(const_cast<TObjectPtr>(object));
        }
    TObjectPtr GetData(TObjectPtr object) const
        {
            return x_GetData(object);
        }

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    CMembers m_Variants;
    vector<CTypeRef> m_VariantTypes;
};

template<typename T>
class CChoiceTypeInfoTmpl : public CChoiceTypeInfoBase
{
    typedef CChoiceTypeInfoBase CParent;
public:
    typedef T TDataType;
    typedef struct {
        TMemberIndex index;
        TDataType data;
    } TObjectType;

    CChoiceTypeInfoTmpl(const string& name)
        : CParent(name)
        {
        }

    // object getters:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }
    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

protected:
    virtual TMemberIndex GetIndex(TConstObjectPtr object) const
        {
            return Get(object).index;
        }
    virtual void SetIndex(TObjectPtr object, TMemberIndex index) const
        {
            Get(object).index = index;
        }
    virtual TObjectPtr x_GetData(TObjectPtr object) const
        {
            return &Get(object).data;
        }
};

END_NCBI_SCOPE

#endif  /* CHOICE__HPP */
