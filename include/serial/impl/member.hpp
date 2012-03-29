#ifndef MEMBER__HPP
#define MEMBER__HPP

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

class CClassTypeInfoBase;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CReadClassMemberHook;
class CWriteClassMemberHook;
class CSkipClassMemberHook;
class CCopyClassMemberHook;

class CDelayBuffer;

class CMemberInfoFunctions;

class NCBI_XSERIAL_EXPORT CMemberInfo : public CItemInfo
{
    typedef CItemInfo CParent;
public:
    typedef TConstObjectPtr (*TMemberGetConst)(const CMemberInfo* memberInfo,
                                               TConstObjectPtr classPtr);
    typedef TObjectPtr (*TMemberGet)(const CMemberInfo* memberInfo,
                                     TObjectPtr classPtr);

    CMemberInfo(const CClassTypeInfoBase* classType, const CMemberId& id,
                TPointerOffsetType offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfoBase* classType, const CMemberId& id,
                TPointerOffsetType offset, TTypeInfo type);
    CMemberInfo(const CClassTypeInfoBase* classType, const char* id,
                TPointerOffsetType offset, const CTypeRef& type);
    CMemberInfo(const CClassTypeInfoBase* classType, const char* id,
                TPointerOffsetType offset, TTypeInfo type);

    const CClassTypeInfoBase* GetClassType(void) const;

    bool Optional(void) const;
    CMemberInfo* SetOptional(void);
    CMemberInfo* SetNoPrefix(void);
    CMemberInfo* SetAttlist(void);
    CMemberInfo* SetNotag(void);
    CMemberInfo* SetAnyContent(void);
    CMemberInfo* SetCompressed(void);
    CMemberInfo* SetNsQualified(bool qualified);

    TConstObjectPtr GetDefault(void) const;
    CMemberInfo* SetDefault(TConstObjectPtr def);
    CMemberInfo* SetElementDefault(TConstObjectPtr def);

    bool HaveSetFlag(void) const;
    CMemberInfo* SetSetFlag(const bool* setFlag);
    CMemberInfo* SetSetFlag(const Uint4* setFlag);
    CMemberInfo* SetOptional(const bool* setFlag);

    enum ESetFlag {
        eSetNo    = 0,
        eSetMaybe = 1,
        eSetYes   = 3
    };

    /// return current value of 'setFlag'
    ESetFlag GetSetFlag(TConstObjectPtr object) const;

    /// true if 'setFlag' is not eSetNo
    bool GetSetFlagYes(TConstObjectPtr object) const;
    /// true if 'setFlag' is eSetNo
    bool GetSetFlagNo(TConstObjectPtr object) const;

    /// set value of 'setFlag'
    void UpdateSetFlag(TObjectPtr object, ESetFlag value) const;
    /// set 'setFlag' to eSetYes
    void UpdateSetFlagYes(TObjectPtr object) const;
    /// set 'setFlag' to eSetMaybe
    void UpdateSetFlagMaybe(TObjectPtr object) const;
    /// set 'setFlag' to eSetNo and return true if previous value wasn't eSetNo
    bool UpdateSetFlagNo(TObjectPtr object) const;
    bool CompareSetFlags(TConstObjectPtr object1,
                         TConstObjectPtr object2) const;

    bool CanBeDelayed(void) const;
    CMemberInfo* SetDelayBuffer(CDelayBuffer* buffer);
    CDelayBuffer& GetDelayBuffer(TObjectPtr object) const;
    const CDelayBuffer& GetDelayBuffer(TConstObjectPtr object) const;

    void SetParentClass(void);

    // I/O
    void ReadMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void ReadMissingMember(CObjectIStream& in, TObjectPtr classPtr) const;
    void WriteMember(CObjectOStream& out, TConstObjectPtr classPtr) const;
    void CopyMember(CObjectStreamCopier& copier) const;
    void CopyMissingMember(CObjectStreamCopier& copier) const;
    void SkipMember(CObjectIStream& in) const;
    void SkipMissingMember(CObjectIStream& in) const;

    TObjectPtr GetMemberPtr(TObjectPtr classPtr) const;
    TConstObjectPtr GetMemberPtr(TConstObjectPtr classPtr) const;

    // hooks
    void SetGlobalReadHook(CReadClassMemberHook* hook);
    void SetLocalReadHook(CObjectIStream& in, CReadClassMemberHook* hook);
    void ResetGlobalReadHook(void);
    void ResetLocalReadHook(CObjectIStream& in);
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadClassMemberHook* hook);

    void SetGlobalWriteHook(CWriteClassMemberHook* hook);
    void SetLocalWriteHook(CObjectOStream& out, CWriteClassMemberHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);
    void SetPathWriteHook(CObjectOStream* out, const string& path,
                          CWriteClassMemberHook* hook);

    void SetLocalSkipHook(CObjectIStream& in, CSkipClassMemberHook* hook);
    void ResetLocalSkipHook(CObjectIStream& in);
    void SetPathSkipHook(CObjectIStream* in, const string& path,
                         CSkipClassMemberHook* hook);

    void SetGlobalCopyHook(CCopyClassMemberHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier,
                          CCopyClassMemberHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);
    void SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                         CCopyClassMemberHook* hook);

    // default I/O (without hooks)
    void DefaultReadMember(CObjectIStream& in,
                           TObjectPtr classPtr) const;
    void DefaultReadMissingMember(CObjectIStream& in,
                                  TObjectPtr classPtr) const;
    void DefaultWriteMember(CObjectOStream& out,
                            TConstObjectPtr classPtr) const;
    void DefaultCopyMember(CObjectStreamCopier& copier) const;
    void DefaultCopyMissingMember(CObjectStreamCopier& copier) const;
    void DefaultSkipMember(CObjectIStream& in) const;
    void DefaultSkipMissingMember(CObjectIStream& in) const;

    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const;

private:
    // Create parent class object
    TObjectPtr CreateClass(void) const;

    // containing class type info
    const CClassTypeInfoBase* m_ClassType;
    // is optional
    bool m_Optional;
    // default value
    TConstObjectPtr m_Default;
    // offset of 'SET' flag inside object
    TPointerOffsetType m_SetFlagOffset;
    bool m_BitSetFlag;
    // offset of delay buffer inside object
    TPointerOffsetType m_DelayOffset;

    TMemberGetConst m_GetConstFunction;
    TMemberGet m_GetFunction;

    CHookData<CReadClassMemberHook, SMemberReadFunctions> m_ReadHookData;
    CHookData<CWriteClassMemberHook, TMemberWriteFunction> m_WriteHookData;
    CHookData<CSkipClassMemberHook, SMemberSkipFunctions> m_SkipHookData;
    CHookData<CCopyClassMemberHook, SMemberCopyFunctions> m_CopyHookData;

    void SetReadFunction(TMemberReadFunction func);
    void SetReadMissingFunction(TMemberReadFunction func);
    void SetWriteFunction(TMemberWriteFunction func);
    void SetCopyFunction(TMemberCopyFunction func);
    void SetCopyMissingFunction(TMemberCopyFunction func);
    void SetSkipFunction(TMemberSkipFunction func);
    void SetSkipMissingFunction(TMemberSkipFunction func);

    void UpdateFunctions(void);

    friend class CMemberInfoFunctions;
};

/* @} */


#include <serial/impl/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */
