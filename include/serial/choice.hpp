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
* Revision 1.4  1999/12/28 18:55:39  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/10/08 21:00:39  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
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
    CChoiceTypeInfoBase(const char* name);
    ~CChoiceTypeInfoBase(void);

    void AddVariant(const CMemberId& id, const CTypeRef& type);
    void AddVariant(const string& name, const CTypeRef& type);
    void AddVariant(const char* name, const CTypeRef& type);

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

END_NCBI_SCOPE

#endif  /* CHOICE__HPP */
