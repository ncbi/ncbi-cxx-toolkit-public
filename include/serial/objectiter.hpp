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
*/

#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

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

class NCBI_XSERIAL_EXPORT CObjectTypeInfoII // item iterator (either member or variant)
{
public:
    const string& GetAlias(void) const;
    
    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());
    
    bool operator==(const CObjectTypeInfoII& iter) const;
    bool operator!=(const CObjectTypeInfoII& iter) const;
    
    void Next(void);

    const CItemInfo* GetItemInfo(void) const;

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
    void SetGlobalReadHook(CReadClassMemberHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadClassMemberHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteClassMemberHook* hook) const;
    void SetGlobalWriteHook(CWriteClassMemberHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteClassMemberHook* hook) const;

    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipClassMemberHook* hook) const;
    void SetGlobalSkipHook(CSkipClassMemberHook* hook) const;
    void ResetLocalSkipHook(CObjectIStream& stream) const;
    void ResetGlobalSkipHook(void) const;
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipClassMemberHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyClassMemberHook* hook) const;
    void SetGlobalCopyHook(CCopyClassMemberHook* hook) const;
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
    void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadChoiceVariantHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteChoiceVariantHook* hook) const;

    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipChoiceVariantHook* hook) const;
    void SetGlobalSkipHook(CSkipChoiceVariantHook* hook) const;
    void ResetLocalSkipHook(CObjectIStream& stream) const;
    void ResetGlobalSkipHook(void) const;
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipChoiceVariantHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
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
    void SetGlobalReadHook(CReadChoiceVariantHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadChoiceVariantHook* hook) const;

    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteChoiceVariantHook* hook) const;
    void SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteChoiceVariantHook* hook) const;

    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyChoiceVariantHook* hook) const;
    void SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const;
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


#include <serial/objectiter.inl>

END_NCBI_SCOPE

#endif  /* OBJECTITER__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2005/03/17 21:07:42  vasilche
* Removed enforced check for call of IsValid().
*
* Revision 1.13  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.12  2004/07/27 17:11:48  gouriano
* Give access to the context of tree iterator
*
* Revision 1.11  2004/04/30 13:28:39  gouriano
* Remove obsolete function declarations
*
* Revision 1.10  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.9  2003/09/30 17:12:30  gouriano
* Modified TypeIterators to skip unset optional members
*
* Revision 1.8  2003/08/14 20:03:57  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.7  2003/08/11 15:25:50  grichenk
* Added possibility to reset an object member from
* a read hook (including non-optional members).
*
* Revision 1.6  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.5  2003/07/17 22:49:31  vasilche
* Added export specifiers.
* Added missing methods.
*
* Revision 1.4  2003/04/15 16:18:15  siyan
* Added doxygen support
*
* Revision 1.3  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
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
