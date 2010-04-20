#ifndef OBJECTITER__HPP
#define OBJECTITER__HPP

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
*   Iterators, which work on object information data
*/

#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Container iterator
class NCBI_XSERIAL_EXPORT CConstObjectInfoEI
{
public:
    CConstObjectInfoEI(void);
    CConstObjectInfoEI(const CConstObjectInfo& object);

    CConstObjectInfoEI& operator=(const CConstObjectInfo& object);

    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CConstObjectInfoEI& obj) const
    {
        return GetElement() == obj.GetElement();
    }
    bool operator!=(const CConstObjectInfoEI& obj) const
    {
        return GetElement() != obj.GetElement();
    }
    TMemberIndex GetIndex(void) const
    {
        return m_Iterator.GetIndex();
    }

    void Next(void);
    CConstObjectInfoEI& operator++(void);

    CConstObjectInfo GetElement(void) const;
    CConstObjectInfo operator*(void) const;

    bool CanGet(void) const
    {
        return true;
    }
    const CItemInfo* GetItemInfo(void) const
    {
        return 0;
    }

protected:
    bool CheckValid(void) const;

private:
    CConstContainerElementIterator m_Iterator;
};

/// Container iterator
class NCBI_XSERIAL_EXPORT CObjectInfoEI
{
public:
    CObjectInfoEI(void);
    CObjectInfoEI(const CObjectInfo& object);

    CObjectInfoEI& operator=(const CObjectInfo& object);

    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CObjectInfoEI& obj) const
    {
        return GetElement() == obj.GetElement();
    }
    bool operator!=(const CObjectInfoEI& obj) const
    {
        return GetElement() != obj.GetElement();
    }
    TMemberIndex GetIndex(void) const
    {
        return m_Iterator.GetIndex();
    }

    void Next(void);
    CObjectInfoEI& operator++(void);

    CObjectInfo GetElement(void) const;
    CObjectInfo operator*(void) const;

    void Erase(void);

    bool CanGet(void) const
    {
        return true;
    }
    const CItemInfo* GetItemInfo(void) const
    {
        return 0;
    }

protected:
    bool CheckValid(void) const;

private:
    CContainerElementIterator m_Iterator;
};

/// Item iterator (either member or variant)
class NCBI_XSERIAL_EXPORT CObjectTypeInfoII 
{
public:
    const string& GetAlias(void) const;
    
    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());
    
    bool operator==(const CObjectTypeInfoII& iter) const;
    bool operator!=(const CObjectTypeInfoII& iter) const;
    
    void Next(void);

    const CItemInfo* GetItemInfo(void) const;
    TMemberIndex GetIndex(void) const
    {
        return GetItemIndex();
    }

protected:
    CObjectTypeInfoII(void);
    CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo);
    CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo, TMemberIndex index);
    
    const CObjectTypeInfo& GetOwnerType(void) const;
    const CClassTypeInfoBase* GetClassTypeInfoBase(void) const;
    TMemberIndex GetItemIndex(void) const;

    void Init(const CClassTypeInfoBase* typeInfo);
    void Init(const CClassTypeInfoBase* typeInfo, TMemberIndex index);

    bool CanGet(void) const
    {
        return true;
    }

    bool CheckValid(void) const;

private:
    CObjectTypeInfo m_OwnerType;
    TMemberIndex m_ItemIndex;
    TMemberIndex m_LastItemIndex;
};

/// Class member iterator
class NCBI_XSERIAL_EXPORT CObjectTypeInfoMI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoMI(void);
    CObjectTypeInfoMI(const CObjectTypeInfo& info);
    CObjectTypeInfoMI(const CObjectTypeInfo& info, TMemberIndex index);

    TMemberIndex GetMemberIndex(void) const;

    CObjectTypeInfoMI& operator++(void);
    CObjectTypeInfoMI& operator=(const CObjectTypeInfo& info);

    CObjectTypeInfo GetClassType(void) const;

    operator CObjectTypeInfo(void) const;
    CObjectTypeInfo GetMemberType(void) const;
    CObjectTypeInfo operator*(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadClassMemberHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalReadHook(CReadClassMemberHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadClassMemberHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteClassMemberHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalWriteHook(CWriteClassMemberHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteClassMemberHook* hook) const;

    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipClassMemberHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalSkipHook(CSkipClassMemberHook* hook) const;
    void ResetLocalSkipHook(CObjectIStream& stream) const;
    void ResetGlobalSkipHook(void) const;
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipClassMemberHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyClassMemberHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalCopyHook(CCopyClassMemberHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;
    void SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                         CCopyClassMemberHook* hook) const;

public: // mostly for internal use
    const CMemberInfo* GetMemberInfo(void) const;

protected:
    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);

    const CClassTypeInfo* GetClassTypeInfo(void) const;

    bool IsSet(const CConstObjectInfo& object) const;

private:
    CMemberInfo* GetNCMemberInfo(void) const;
};

/// Choice variant iterator
class NCBI_XSERIAL_EXPORT CObjectTypeInfoVI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoVI(const CObjectTypeInfo& info);
    CObjectTypeInfoVI(const CObjectTypeInfo& info, TMemberIndex index);

    TMemberIndex GetVariantIndex(void) const;

    CObjectTypeInfoVI& operator++(void);
    CObjectTypeInfoVI& operator=(const CObjectTypeInfo& info);

    CObjectTypeInfo GetChoiceType(void) const;

    CObjectTypeInfo GetVariantType(void) const;
    CObjectTypeInfo operator*(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadChoiceVariantHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteChoiceVariantHook* hook) const;

    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalSkipHook(CSkipChoiceVariantHook* hook) const;
    void ResetLocalSkipHook(CObjectIStream& stream) const;
    void ResetGlobalSkipHook(void) const;
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipChoiceVariantHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;
    void SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                         CCopyChoiceVariantHook* hook) const;

public: // mostly for internal use
    const CVariantInfo* GetVariantInfo(void) const;

protected:
    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);

    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;

private:
    CVariantInfo* GetNCVariantInfo(void) const;
};

/// Class member iterator
class NCBI_XSERIAL_EXPORT CConstObjectInfoMI : public CObjectTypeInfoMI
{
    typedef CObjectTypeInfoMI CParent;
public:
    CConstObjectInfoMI(void);
    CConstObjectInfoMI(const CConstObjectInfo& object);
    CConstObjectInfoMI(const CConstObjectInfo& object, TMemberIndex index);
    
    const CConstObjectInfo& GetClassObject(void) const;
    
    CConstObjectInfoMI& operator=(const CConstObjectInfo& object);
    
    bool IsSet(void) const;

    CConstObjectInfo GetMember(void) const;
    CConstObjectInfo operator*(void) const;

    bool CanGet(void) const;
private:
    pair<TConstObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CConstObjectInfo m_Object;
};

/// Class member iterator
class NCBI_XSERIAL_EXPORT CObjectInfoMI : public CObjectTypeInfoMI
{
    typedef CObjectTypeInfoMI CParent;
public:
    CObjectInfoMI(void);
    CObjectInfoMI(const CObjectInfo& object);
    CObjectInfoMI(const CObjectInfo& object, TMemberIndex index);
    
    const CObjectInfo& GetClassObject(void) const;
    
    CObjectInfoMI& operator=(const CObjectInfo& object);
    
    bool IsSet(void) const;

    CObjectInfo GetMember(void) const;
    CObjectInfo operator*(void) const;

    // reset value of member to default state
    enum EEraseFlag {
        eErase_Optional, // default - erase optional member only
        eErase_Mandatory // allow erasing mandatory members, may be dangerous!
    };
    void Erase(EEraseFlag flag = eErase_Optional);
    void Reset(void);

    bool CanGet(void) const;
private:
    pair<TObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CObjectInfo m_Object;
};

/// Choice variant iterator
class NCBI_XSERIAL_EXPORT CObjectTypeInfoCV
{
public:
    CObjectTypeInfoCV(void);
    CObjectTypeInfoCV(const CObjectTypeInfo& info);
    CObjectTypeInfoCV(const CObjectTypeInfo& info, TMemberIndex index);
    CObjectTypeInfoCV(const CConstObjectInfo& object);

    TMemberIndex GetVariantIndex(void) const;

    const string& GetAlias(void) const;

    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CObjectTypeInfoCV& iter) const;
    bool operator!=(const CObjectTypeInfoCV& iter) const;

    CObjectTypeInfoCV& operator=(const CObjectTypeInfo& info);
    CObjectTypeInfoCV& operator=(const CConstObjectInfo& object);

    CObjectTypeInfo GetChoiceType(void) const;

    CObjectTypeInfo GetVariantType(void) const;
    CObjectTypeInfo operator*(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadChoiceVariantHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteChoiceVariantHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;
    void SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                          CCopyChoiceVariantHook* hook) const;

public: // mostly for internal use
    const CVariantInfo* GetVariantInfo(void) const;

protected:
    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;

    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);
    void Init(const CConstObjectInfo& object);

private:
    const CChoiceTypeInfo* m_ChoiceTypeInfo;
    TMemberIndex m_VariantIndex;

private:
    CVariantInfo* GetNCVariantInfo(void) const;
};

/// Choice variant iterator
class NCBI_XSERIAL_EXPORT CConstObjectInfoCV : public CObjectTypeInfoCV
{
    typedef CObjectTypeInfoCV CParent;
public:
    CConstObjectInfoCV(void);
    CConstObjectInfoCV(const CConstObjectInfo& object);
    CConstObjectInfoCV(const CConstObjectInfo& object, TMemberIndex index);

    const CConstObjectInfo& GetChoiceObject(void) const;
    
    CConstObjectInfoCV& operator=(const CConstObjectInfo& object);
    
    CConstObjectInfo GetVariant(void) const;
    CConstObjectInfo operator*(void) const;

private:
    pair<TConstObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CConstObjectInfo m_Object;
    TMemberIndex m_VariantIndex;
};

/// Choice variant iterator
class NCBI_XSERIAL_EXPORT CObjectInfoCV : public CObjectTypeInfoCV
{
    typedef CObjectTypeInfoCV CParent;
public:
    CObjectInfoCV(void);
    CObjectInfoCV(const CObjectInfo& object);
    CObjectInfoCV(const CObjectInfo& object, TMemberIndex index);

    const CObjectInfo& GetChoiceObject(void) const;
    
    CObjectInfoCV& operator=(const CObjectInfo& object);
    
    CObjectInfo GetVariant(void) const;
    CObjectInfo operator*(void) const;

private:
    pair<TObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CObjectInfo m_Object;
};


/* @} */


#include <serial/impl/objectiter.inl>

END_NCBI_SCOPE

#endif  /* OBJECTITER__HPP */
