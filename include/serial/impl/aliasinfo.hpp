#ifndef ALIASINFO__HPP
#define ALIASINFO__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Alias type info
*/

#include <serial/impl/ptrinfo.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */

BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CAliasTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    CAliasTypeInfo(const string& name, TTypeInfo type);

    virtual bool IsDefault(TConstObjectPtr object) const override;
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                ESerialRecursionMode how = eRecursive) const override;
    virtual void SetDefault(TObjectPtr dst) const override;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                ESerialRecursionMode how = eRecursive) const override;

    virtual void Delete(TObjectPtr object) const override;
    virtual void DeleteExternalObjects(TObjectPtr object) const override;

    virtual const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const override;
    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const override;

    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const override;

    void SetDataOffset(TPointerOffsetType offset);
    TObjectPtr GetDataPtr(TObjectPtr objectPtr) const;
    TConstObjectPtr GetDataPtr(TConstObjectPtr objectPtr) const;

    void SetCreateFunction(TTypeCreate func)
        {
            CParent::SetCreateFunction(func);
        }

    // used in serialization
    void SetFullAlias(bool set=true) {
        m_FullAlias = set;
    }
    bool IsFullAlias(void) const {
        return m_FullAlias;
    }
protected:
    static TObjectPtr GetDataPointer(const CPointerTypeInfo* objectType,
                                     TObjectPtr objectPtr);
    static void SetDataPointer(const CPointerTypeInfo* objectType,
                               TObjectPtr objectPtr,
                               TObjectPtr dataPtr);

    TPointerOffsetType m_DataOffset;
    bool m_FullAlias;

    friend class CAliasTypeInfoFunctions;
private:
    void InitAliasTypeInfoFunctions(void);
};


END_NCBI_SCOPE

/* @} */

#endif  /* TYPEDEFINFO__HPP */
