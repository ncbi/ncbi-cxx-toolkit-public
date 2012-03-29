#ifndef VARIANT__HPP
#define VARIANT__HPP

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

#include <corelib/ncbistd.hpp>
#include <serial/serialutil.hpp>
#include <serial/impl/item.hpp>
#include <serial/impl/hookdata.hpp>
#include <serial/impl/hookfunc.hpp>
#include <serial/typeinfo.hpp>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CChoiceTypeInfo;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CReadChoiceVariantHook;
class CWriteChoiceVariantHook;
class CSkipChoiceVariantHook;
class CCopyChoiceVariantHook;

class CDelayBuffer;

class CVariantInfoFunctions;
class CObjectInfoCV;

class NCBI_XSERIAL_EXPORT CVariantInfo : public CItemInfo
{
    typedef CItemInfo CParent;
public:
    typedef TConstObjectPtr (*TVariantGetConst)(const CVariantInfo*variantInfo,
                                                TConstObjectPtr choicePtr);
    typedef TObjectPtr (*TVariantGet)(const CVariantInfo* variantInfo,
                                      TObjectPtr choicePtr);

    enum EVariantType {
        ePointerFlag = 1 << 0,
        eObjectFlag = 1 << 1,
        eInlineVariant = 0,
        eNonObjectPointerVariant = ePointerFlag,
        eObjectPointerVariant = ePointerFlag | eObjectFlag,
        eSubClassVariant = eObjectFlag
    };

    CVariantInfo(const CChoiceTypeInfo* choiceType, const CMemberId& id,
                 TPointerOffsetType offset, const CTypeRef& type);
    CVariantInfo(const CChoiceTypeInfo* choiceType, const CMemberId& id,
                 TPointerOffsetType offset, TTypeInfo type);
    CVariantInfo(const CChoiceTypeInfo* choiceType, const char* id,
                 TPointerOffsetType offset, const CTypeRef& type);
    CVariantInfo(const CChoiceTypeInfo* choiceType, const char* id,
                 TPointerOffsetType offset, TTypeInfo type);

    const CChoiceTypeInfo* GetChoiceType(void) const;

    EVariantType GetVariantType(void) const;

    CVariantInfo* SetNoPrefix(void);
    CVariantInfo* SetNotag(void);
    CVariantInfo* SetCompressed(void);
    CVariantInfo* SetNsQualified(bool qualified);

    bool IsInline(void) const;
    bool IsNonObjectPointer(void) const;
    bool IsObjectPointer(void) const;
    bool IsSubClass(void) const;

    bool IsPointer(void) const;
    bool IsNotPointer(void) const;
    bool IsObject(void) const;
    bool IsNotObject(void) const;

    CVariantInfo* SetPointer(void);
    CVariantInfo* SetObjectPointer(void);
    CVariantInfo* SetSubClass(void);

    bool CanBeDelayed(void) const;
    CVariantInfo* SetDelayBuffer(CDelayBuffer* buffer);
    CDelayBuffer& GetDelayBuffer(TObjectPtr object) const;
    const CDelayBuffer& GetDelayBuffer(TConstObjectPtr object) const;

    TConstObjectPtr GetVariantPtr(TConstObjectPtr choicePtr) const;
    TObjectPtr GetVariantPtr(TObjectPtr choicePtr) const;

    // I/O
    void ReadVariant(CObjectIStream& in, TObjectPtr choicePtr) const;
    void WriteVariant(CObjectOStream& out, TConstObjectPtr choicePtr) const;
    void CopyVariant(CObjectStreamCopier& copier) const;
    void SkipVariant(CObjectIStream& in) const;

    // hooks
    void SetGlobalReadHook(CReadChoiceVariantHook* hook);
    void SetLocalReadHook(CObjectIStream& in,
                          CReadChoiceVariantHook* hook);
    void ResetGlobalReadHook(void);
    void ResetLocalReadHook(CObjectIStream& in);
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadChoiceVariantHook* hook);

    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook);
    void SetLocalWriteHook(CObjectOStream& out,
                           CWriteChoiceVariantHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);
    void SetPathWriteHook(CObjectOStream* out, const string& path,
                          CWriteChoiceVariantHook* hook);

    void SetLocalSkipHook(CObjectIStream& in, CSkipChoiceVariantHook* hook);
    void ResetLocalSkipHook(CObjectIStream& in);
    void SetPathSkipHook(CObjectIStream* in, const string& path,
                         CSkipChoiceVariantHook* hook);

    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier,
                          CCopyChoiceVariantHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);
    void SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                         CCopyChoiceVariantHook* hook);

    // default I/O (without hooks)
    void DefaultReadVariant(CObjectIStream& in,
                            TObjectPtr choicePtr) const;
    void DefaultWriteVariant(CObjectOStream& out,
                             TConstObjectPtr choicePtr) const;
    void DefaultCopyVariant(CObjectStreamCopier& copier) const;
    void DefaultSkipVariant(CObjectIStream& in) const;
    
    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const;

private:
    // Create choice object
    TObjectPtr CreateChoice(void) const;

    // owning choice type info
    const CChoiceTypeInfo* m_ChoiceType;
    // type of variant implementation: inline, pointer etc.
    EVariantType m_VariantType;
    // offset of delay buffer inside object
    TPointerOffsetType m_DelayOffset;

    TVariantGetConst m_GetConstFunction;
    TVariantGet m_GetFunction;

    CHookData<CReadChoiceVariantHook, TVariantReadFunction> m_ReadHookData;
    CHookData<CWriteChoiceVariantHook, TVariantWriteFunction> m_WriteHookData;
    CHookData<CSkipChoiceVariantHook, TVariantSkipFunction> m_SkipHookData;
    CHookData<CCopyChoiceVariantHook, TVariantCopyFunction> m_CopyHookData;

    void SetReadFunction(TVariantReadFunction func);
    void SetWriteFunction(TVariantWriteFunction func);
    void SetCopyFunction(TVariantCopyFunction func);
    void SetSkipFunction(TVariantSkipFunction func);

    void UpdateFunctions(void);

    friend class CVariantInfoFunctions;
};


/* @} */


#include <serial/impl/variant.inl>

END_NCBI_SCOPE

#endif  /* VARIANT__HPP */
