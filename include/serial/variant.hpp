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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/18 20:00:12  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/item.hpp>
#include <serial/hookdata.hpp>

BEGIN_NCBI_SCOPE

class CChoiceTypeInfo;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CReadChoiceVariantHook;
class CWriteChoiceVariantHook;
class CCopyChoiceVariantHook;

class CDelayBuffer;

class CVariantInfo : public CItemInfo
{
    typedef CItemInfo CParent;
public:
    typedef TConstObjectPtr (*TVariantGetConst)(const CVariantInfo*variantInfo,
                                                TConstObjectPtr choicePtr);
    typedef TObjectPtr (*TVariantGet)(const CVariantInfo* variantInfo,
                                      TObjectPtr choicePtr);

    typedef void (*TVariantRead)(CObjectIStream& in,
                                 const CVariantInfo* variantInfo,
                                 TObjectPtr classPtr);
    typedef void (*TVariantWrite)(CObjectOStream& out,
                                  const CVariantInfo* variantInfo,
                                  TConstObjectPtr classPtr);
    typedef void (*TVariantSkip)(CObjectIStream& in,
                                 const CVariantInfo* variantInfo);
    typedef void (*TVariantCopy)(CObjectStreamCopier& copier,
                                 const CVariantInfo* variantInfo);

    enum EVariantType {
        eInlineVariant,
        ePointerVariant,
        eObjectPointerVariant
    };

    CVariantInfo(const CChoiceTypeInfo* choiceType,
                 const CMemberId& id, TOffset offset, const CTypeRef& type);
    CVariantInfo(const CChoiceTypeInfo* choiceType,
                 const CMemberId& id, TOffset offset, TTypeInfo type);
    CVariantInfo(const CChoiceTypeInfo* choiceType,
                 const char* id, TOffset offset, const CTypeRef& type);
    CVariantInfo(const CChoiceTypeInfo* choiceType,
                 const char* id, TOffset offset, TTypeInfo type);
    ~CVariantInfo(void);

    const CChoiceTypeInfo* GetChoiceType(void) const;

    EVariantType GetVariantType(void) const;

    bool IsPointer(void) const;
    CVariantInfo* SetPointer(void);
    
    bool IsObjectPointer(void) const;
    CVariantInfo* SetObjectPointer(void);

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

    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook);
    void SetLocalWriteHook(CObjectOStream& out,
                           CWriteChoiceVariantHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);

    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier,
                          CCopyChoiceVariantHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);

    // default I/O (without hooks)
    void DefaultReadVariant(CObjectIStream& in,
                            TObjectPtr choicePtr) const;
    void DefaultWriteVariant(CObjectOStream& out,
                             TConstObjectPtr choicePtr) const;
    void DefaultCopyVariant(CObjectStreamCopier& copier) const;
    
    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const;

private:
    // owning choice type info
    const CChoiceTypeInfo* m_ChoiceType;
    // type of variant implementation: inline, pointer etc.
    EVariantType m_VariantType;
    // offset of delay buffer inside object
    TOffset m_DelayOffset;

    TVariantGetConst m_GetConstFunction;
    TVariantGet m_GetFunction;

    CHookData<CObjectIStream, CReadChoiceVariantHook,
        TVariantRead> m_ReadHookData;
    CHookData<CObjectOStream, CWriteChoiceVariantHook,
        TVariantWrite> m_WriteHookData;
    CHookData<CObjectStreamCopier, CCopyChoiceVariantHook,
        TVariantCopy> m_CopyHookData;
    TVariantSkip m_SkipFunction;

    void SetReadHook(CObjectIStream* stream, CReadChoiceVariantHook* hook);
    void SetWriteHook(CObjectOStream* stream, CWriteChoiceVariantHook* hook);
    void SetCopyHook(CObjectStreamCopier* stream, CCopyChoiceVariantHook* hook);
    void ResetReadHook(CObjectIStream* stream);
    void ResetWriteHook(CObjectOStream* stream);
    void ResetCopyHook(CObjectStreamCopier* stream);

    void SetReadFunction(TVariantRead func);
    void SetWriteFunction(TVariantWrite func);
    void SetCopyFunction(TVariantCopy func);
    void SetSkipFunction(TVariantSkip func);

    void UpdateGetFunction(void);
    void UpdateReadFunction(void);
    void UpdateWriteFunction(void);
    void UpdateCopyFunction(void);
    void UpdateSkipFunction(void);

    static
    TConstObjectPtr GetConstInlineVariant(const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstPointerVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstDelayedVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static TObjectPtr GetInlineVariant(const CVariantInfo* variantInfo,
                                       TObjectPtr choicePtr);
    static TObjectPtr GetPointerVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);
    static TObjectPtr GetDelayedVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);

    static void ReadInlineVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void ReadPointerVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr);
    static void ReadDelayedVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadHookedVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void WriteInlineVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void WritePointerVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteObjectPointerVariant(CObjectOStream& out,
                                          const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static void WriteDelayedVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteHookedVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void CopyNonObjectVariant(CObjectStreamCopier& copier,
                                     const CVariantInfo* variantInfo);
    static void CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                         const CVariantInfo* variantInfo);
    static void CopyHookedVariant(CObjectStreamCopier& copier,
                                  const CVariantInfo* variantInfo);
    static void SkipNonObjectVariant(CObjectIStream& in,
                                     const CVariantInfo* variantInfo);
    static void SkipObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo);
};

#include <serial/variant.inl>

END_NCBI_SCOPE

#endif  /* VARIANT__HPP */
