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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/05/17 14:58:35  lavr
* Typos corrected
*
* Revision 1.1  2000/10/20 15:51:25  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>

BEGIN_NCBI_SCOPE

class CConstObjectInfoEI
{
public:
    CConstObjectInfoEI(void);
    CConstObjectInfoEI(const CConstObjectInfo& object);

    CConstObjectInfoEI& operator=(const CConstObjectInfo& object);

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void Next(void);
    CConstObjectInfoEI& operator++(void);

    CConstObjectInfo GetElement(void) const;
    CConstObjectInfo operator*(void) const;

private:
    bool CheckValid(void) const;

    void ReportNonValid(void) const;
    
    CConstContainerElementIterator m_Iterator;
#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
};

class CObjectInfoEI
{
public:
    CObjectInfoEI(void);
    CObjectInfoEI(const CObjectInfo& object);

    CObjectInfoEI& operator=(const CObjectInfo& object);

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void Next(void);
    CObjectInfoEI& operator++(void);

    CObjectInfo GetElement(void) const;
    CObjectInfo operator*(void) const;

    void Erase(void);

private:
    bool CheckValid(void) const;

    void ReportNonValid(void) const;
    
    CContainerElementIterator m_Iterator;
#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
};

class CObjectTypeInfoII // item iterator (either member or variant)
{
public:
    const string& GetAlias(void) const;
    
    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;
    
    bool operator==(const CObjectTypeInfoII& iter) const;
    bool operator!=(const CObjectTypeInfoII& iter) const;
    
    void Next(void);

protected:
    CObjectTypeInfoII(void);
    CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo);
    CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo, TMemberIndex index);
    
    const CObjectTypeInfo& GetOwnerType(void) const;
    const CClassTypeInfoBase* GetClassTypeInfoBase(void) const;
    TMemberIndex GetItemIndex(void) const;
    const CItemInfo* GetItemInfo(void) const;

    void Init(const CClassTypeInfoBase* typeInfo);
    void Init(const CClassTypeInfoBase* typeInfo, TMemberIndex index);

private:
    CObjectTypeInfo m_OwnerType;
    TMemberIndex m_ItemIndex;
    TMemberIndex m_LastItemIndex;
    
    void ReportNonValid(void) const;
    
protected:
#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
    bool CheckValid(void) const;
};

class CObjectTypeInfoMI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoMI(void);
    CObjectTypeInfoMI(const CObjectTypeInfo& info);
    CObjectTypeInfoMI(const CObjectTypeInfo& info, TMemberIndex index);

    TMemberIndex GetMemberIndex(void) const;

    bool Optional(void) const;

    CObjectTypeInfoMI& operator++(void);
    CObjectTypeInfoMI& operator=(const CObjectTypeInfo& info);

    CObjectTypeInfo GetClassType(void) const;

    operator CObjectTypeInfo(void) const;
    CObjectTypeInfo GetMemberType(void) const;
    CObjectTypeInfo operator*(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadClassMemberHook* hook) const;
    void SetGlobalReadHook(CReadClassMemberHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteClassMemberHook* hook) const;
    void SetGlobalWriteHook(CWriteClassMemberHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyClassMemberHook* hook) const;
    void SetGlobalCopyHook(CCopyClassMemberHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;

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

class CObjectTypeInfoVI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoVI(void);
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
    void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;

public: // mostly for internal use
    const CVariantInfo* GetVariantInfo(void) const;

protected:
    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);

    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;

private:
    CVariantInfo* GetNCVariantInfo(void) const;
};

class CConstObjectInfoMI : public CObjectTypeInfoMI
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

private:
    pair<TConstObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CConstObjectInfo m_Object;
};

class CObjectInfoMI : public CObjectTypeInfoMI
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
    void Erase(void);
    void Reset(void);

private:
    pair<TObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CObjectInfo m_Object;
};

class CObjectTypeInfoCV
{
public:
    CObjectTypeInfoCV(void);
    CObjectTypeInfoCV(const CObjectTypeInfo& info);
    CObjectTypeInfoCV(const CObjectTypeInfo& info, TMemberIndex index);
    CObjectTypeInfoCV(const CConstObjectInfo& object);

    TMemberIndex GetVariantIndex(void) const;

    const string& GetAlias(void) const;

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    bool operator==(const CObjectTypeInfoCV& iter) const;
    bool operator!=(const CObjectTypeInfoCV& iter) const;

    CObjectTypeInfoCV& operator=(const CObjectTypeInfo& info);
    CObjectTypeInfoCV& operator=(const CConstObjectInfo& object);

    CObjectTypeInfo GetChoiceType(void) const;

    CObjectTypeInfo GetVariantType(void) const;
    CObjectTypeInfo operator*(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadChoiceVariantHook* hook) const;
    void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;

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

class CConstObjectInfoCV : public CObjectTypeInfoCV
{
    typedef CObjectTypeInfoCV CParent;
public:
    CConstObjectInfoCV(void);
    CConstObjectInfoCV(const CConstObjectInfo& object);
    CConstObjectInfoCV(const CConstObjectInfo& object, TMemberIndex index);

    bool operator==(const CConstObjectInfoCV& var) const;
    bool operator!=(const CConstObjectInfoCV& var) const;

    const CConstObjectInfo& GetChoiceObject(void) const;
    
    CConstObjectInfoCV& operator=(const CConstObjectInfo& object);
    
    CConstObjectInfo GetVariant(void) const;
    CConstObjectInfo operator*(void) const;

private:
    pair<TConstObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CConstObjectInfo m_Object;
    TMemberIndex m_VariantIndex;
};

class CObjectInfoCV : public CObjectTypeInfoCV
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

    void Erase(void);

private:
    pair<TObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CObjectInfo m_Object;
};

#include <serial/objectiter.inl>

END_NCBI_SCOPE

#endif  /* OBJECTITER__HPP */
