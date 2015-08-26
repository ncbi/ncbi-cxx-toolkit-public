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
*/

#include <serial/impl/classinfob.hpp>
#include <serial/impl/variant.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CChoiceTypeInfoReader;
class CClassInfoHelperBase;
class CReadChoiceVariantHook;

class CChoiceTypeInfoFunctions;

class NCBI_XSERIAL_EXPORT CChoiceTypeInfo : public CClassTypeInfoBase
{
    typedef CClassTypeInfoBase CParent;
public:
    typedef TMemberIndex (*TWhichFunction)(const CChoiceTypeInfo* choiceType,
                                           TConstObjectPtr choicePtr);
    typedef void (*TResetFunction)(const CChoiceTypeInfo* choiceType,
                                   TObjectPtr choicePtr);
    typedef void (*TSelectFunction)(const CChoiceTypeInfo* choiceType,
                                    TObjectPtr choicePtr, TMemberIndex index,
                                    CObjectMemoryPool* memPool);
    typedef void (*TSelectDelayFunction)(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr,
                                         TMemberIndex index);
    typedef TObjectPtr (*TGetDataFunction)(const CChoiceTypeInfo* choiceType,
                                           TObjectPtr choicePtr,
                                           TMemberIndex index);

    CChoiceTypeInfo(size_t size, const char* name,
                    const void* nonCObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const char* name,
                    const CObject* cObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const string& name,
                    const void* nonCObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);
    CChoiceTypeInfo(size_t size, const string& name,
                    const CObject* cObject, TTypeCreate createFunc,
                    const type_info& ti,
                    TWhichFunction whichFunc,
                    TSelectFunction selectFunc,
                    TResetFunction resetFunc);

    const CItemsInfo& GetVariants(void) const;
    const CVariantInfo* GetVariantInfo(TMemberIndex index) const;
    const CVariantInfo* GetVariantInfo(const CIterator& i) const;
    const CVariantInfo* GetVariantInfo(const CTempString& name) const;

    CVariantInfo* AddVariant(const char* variantId,
                             const void* variantPtr,
                             const CTypeRef& variantType);
    CVariantInfo* AddVariant(const CMemberId& variantId,
                             const void* variantPtr,
                             const CTypeRef& variantType);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    // iterators interface
    TMemberIndex GetIndex(TConstObjectPtr object) const;
    void ResetIndex(TObjectPtr object) const;
    void SetIndex(TObjectPtr object, TMemberIndex index,
                  CObjectMemoryPool* pool = 0) const;
    void SetDelayIndex(TObjectPtr object, TMemberIndex index) const;

    TConstObjectPtr GetData(TConstObjectPtr object, TMemberIndex index) const;
    TObjectPtr GetData(TObjectPtr object, TMemberIndex index) const;

    void SetGlobalHook(const CTempString& variant_names,
                       CReadChoiceVariantHook* hook);

    void AdjustChoiceTypeInfoFunctions(void);
    bool MayBeEmpty(void) const {
        return m_AllowEmpty;
    }

    void SetSelectDelay(TSelectDelayFunction func);
protected:
    void CallUserOp_Assign(TObjectPtr dst, TConstObjectPtr src) const;

    friend class CChoiceTypeInfoFunctions;

private:
    void InitChoiceTypeInfoFunctions(void);

protected:
    TWhichFunction m_WhichFunction;
    TResetFunction m_ResetFunction;
    TSelectFunction m_SelectFunction;
    TSelectDelayFunction m_SelectDelayFunction;
    bool m_AllowEmpty;
};


/* @} */


#include <serial/impl/choice.inl>

END_NCBI_SCOPE

#endif  /* CHOICE__HPP */
