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

/////////////////////////////////////////////////////////////////////////////
///
/// CConstObjectInfoEI --
///
///   Container iterator
///   Provides read access to elements of container
///   @sa CConstObjectInfo::BeginElements

class NCBI_XSERIAL_EXPORT CConstObjectInfoEI
{
public:
    CConstObjectInfoEI(void);
    CConstObjectInfoEI(const CConstObjectInfo& object);

    CConstObjectInfoEI& operator=(const CConstObjectInfo& object);

    /// Is iterator valid
    bool Valid(void) const;
    /// Is iterator valid
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CConstObjectInfoEI& obj) const
    {
        return GetElement() == obj.GetElement();
    }
    bool operator!=(const CConstObjectInfoEI& obj) const
    {
        return GetElement() != obj.GetElement();
    }
    
    /// Get index of the element in the container
    TMemberIndex GetIndex(void) const
    {
        return m_Iterator.GetIndex();
    }

    /// Advance to next element
    void Next(void);

    /// Advance to next element
    CConstObjectInfoEI& operator++(void);

    /// Get element data and type information
    CConstObjectInfo GetElement(void) const;

    /// Get element data and type information
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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectInfoEI --
///
///   Container iterator
///   Provides read/write access to elements of container
///   @sa CObjectInfo::BeginElements

class NCBI_XSERIAL_EXPORT CObjectInfoEI
{
public:
    CObjectInfoEI(void);
    CObjectInfoEI(const CObjectInfo& object);

    CObjectInfoEI& operator=(const CObjectInfo& object);

    /// Is iterator valid
    bool Valid(void) const;
    /// Is iterator valid
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CObjectInfoEI& obj) const
    {
        return GetElement() == obj.GetElement();
    }
    bool operator!=(const CObjectInfoEI& obj) const
    {
        return GetElement() != obj.GetElement();
    }

    /// Get index of the element in the container
    TMemberIndex GetIndex(void) const
    {
        return m_Iterator.GetIndex();
    }

    /// Advance to next element
    void Next(void);

    /// Advance to next element
    CObjectInfoEI& operator++(void);

    /// Get element data and type information
    CObjectInfo GetElement(void) const;

    /// Get element data and type information
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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectTypeInfoII --
///
/// Item iterator (either class member or choice variant)
/// provides access to the data type information.

class NCBI_XSERIAL_EXPORT CObjectTypeInfoII 
{
public:
    const string& GetAlias(void) const;
    
    /// Is iterator valid
    bool Valid(void) const;
    /// Is iterator valid
    DECLARE_OPERATOR_BOOL(Valid());
    
    bool operator==(const CObjectTypeInfoII& iter) const;
    bool operator!=(const CObjectTypeInfoII& iter) const;
    
    /// Advance to next element
    void Next(void);

    const CItemInfo* GetItemInfo(void) const;

    /// Get index of the element in the container (class or choice)
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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectTypeInfoMI --
///
/// Class member iterator
/// provides access to the data type information.

class NCBI_XSERIAL_EXPORT CObjectTypeInfoMI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoMI(void);
    CObjectTypeInfoMI(const CObjectTypeInfo& info);
    CObjectTypeInfoMI(const CObjectTypeInfo& info, TMemberIndex index);

    /// Get index of the member in the class
    TMemberIndex GetMemberIndex(void) const;

    /// Advance to next element
    CObjectTypeInfoMI& operator++(void);

    CObjectTypeInfoMI& operator=(const CObjectTypeInfo& info);

    /// Get containing class type
    CObjectTypeInfo GetClassType(void) const;

    /// Get data type information
    operator CObjectTypeInfo(void) const;
    /// Get data type information
    CObjectTypeInfo GetMemberType(void) const;
    /// Get data type information
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
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalSkipHook(CSkipClassMemberHook* hook) const;
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

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectTypeInfoVI --
///
/// Choice variant iterator
/// provides access to the data type information.

class NCBI_XSERIAL_EXPORT CObjectTypeInfoVI : public CObjectTypeInfoII
{
    typedef CObjectTypeInfoII CParent;
public:
    CObjectTypeInfoVI(const CObjectTypeInfo& info);
    CObjectTypeInfoVI(const CObjectTypeInfo& info, TMemberIndex index);

    /// Get index of the variant in the choice
    TMemberIndex GetVariantIndex(void) const;

    /// Advance to next element
    CObjectTypeInfoVI& operator++(void);

    CObjectTypeInfoVI& operator=(const CObjectTypeInfo& info);

    /// Get containing choice type
    CObjectTypeInfo GetChoiceType(void) const;

    /// Get data type information
    CObjectTypeInfo GetVariantType(void) const;
    /// Get data type information
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
    ///   Use local hooks instead
    NCBI_DEPRECATED void SetGlobalSkipHook(CSkipChoiceVariantHook* hook) const;
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

/////////////////////////////////////////////////////////////////////////////
///
/// CConstObjectInfoMI --
///
/// Class member iterator
/// provides read access to class member data.

class NCBI_XSERIAL_EXPORT CConstObjectInfoMI : public CObjectTypeInfoMI
{
    typedef CObjectTypeInfoMI CParent;
public:
    CConstObjectInfoMI(void);
    CConstObjectInfoMI(const CConstObjectInfo& object);
    CConstObjectInfoMI(const CConstObjectInfo& object, TMemberIndex index);
    
    /// Get containing class data
    const CConstObjectInfo& GetClassObject(void) const;
    
    CConstObjectInfoMI& operator=(const CConstObjectInfo& object);
    
    /// Is member assigned a value
    bool IsSet(void) const;

    /// Get class member data
    CConstObjectInfo GetMember(void) const;
    /// Get class member data
    CConstObjectInfo operator*(void) const;

    bool CanGet(void) const;
private:
    pair<TConstObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CConstObjectInfo m_Object;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectInfoMI --
///
/// Class member iterator
/// provides read/write access to class member data.

class NCBI_XSERIAL_EXPORT CObjectInfoMI : public CObjectTypeInfoMI
{
    typedef CObjectTypeInfoMI CParent;
public:
    CObjectInfoMI(void);
    CObjectInfoMI(const CObjectInfo& object);
    CObjectInfoMI(const CObjectInfo& object, TMemberIndex index);
    
    /// Get containing class data
    const CObjectInfo& GetClassObject(void) const;
    
    CObjectInfoMI& operator=(const CObjectInfo& object);
    
    /// Is member assigned a value
    bool IsSet(void) const;

    /// Get class member data
    CObjectInfo GetMember(void) const;
    /// Get class member data
    CObjectInfo operator*(void) const;

    /// Erase types
    enum EEraseFlag {
        eErase_Optional, ///< default - erase optional member only
        eErase_Mandatory ///< allow erasing mandatory members, may be dangerous!
    };
    /// Erase member value
    void Erase(EEraseFlag flag = eErase_Optional);
    /// Reset value of member to default state
    void Reset(void);

    bool CanGet(void) const;
private:
    pair<TObjectPtr, TTypeInfo> GetMemberPair(void) const;

    CObjectInfo m_Object;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectTypeInfoCV --
///
/// Choice variant
/// provides access to the data type information.

class NCBI_XSERIAL_EXPORT CObjectTypeInfoCV
{
public:
    CObjectTypeInfoCV(void);
    CObjectTypeInfoCV(const CObjectTypeInfo& info);
    CObjectTypeInfoCV(const CObjectTypeInfo& info, TMemberIndex index);
    CObjectTypeInfoCV(const CConstObjectInfo& object);

    /// Get index of the variant in the choice
    TMemberIndex GetVariantIndex(void) const;

    const string& GetAlias(void) const;

    bool Valid(void) const;
    DECLARE_OPERATOR_BOOL(Valid());

    bool operator==(const CObjectTypeInfoCV& iter) const;
    bool operator!=(const CObjectTypeInfoCV& iter) const;

    CObjectTypeInfoCV& operator=(const CObjectTypeInfo& info);
    CObjectTypeInfoCV& operator=(const CConstObjectInfo& object);

    /// Get containing choice
    CObjectTypeInfo GetChoiceType(void) const;

    /// Get variant data type
    CObjectTypeInfo GetVariantType(void) const;
    /// Get variant data type
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

/////////////////////////////////////////////////////////////////////////////
///
/// CConstObjectInfoCV --
///
/// Choice variant
/// provides read access to the variant data.

class NCBI_XSERIAL_EXPORT CConstObjectInfoCV : public CObjectTypeInfoCV
{
    typedef CObjectTypeInfoCV CParent;
public:
    CConstObjectInfoCV(void);
    CConstObjectInfoCV(const CConstObjectInfo& object);
    CConstObjectInfoCV(const CConstObjectInfo& object, TMemberIndex index);

    /// Get containing choice
    const CConstObjectInfo& GetChoiceObject(void) const;
    
    CConstObjectInfoCV& operator=(const CConstObjectInfo& object);
    
    /// Get variant data
    CConstObjectInfo GetVariant(void) const;
    /// Get variant data
    CConstObjectInfo operator*(void) const;

private:
    pair<TConstObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CConstObjectInfo m_Object;
    TMemberIndex m_VariantIndex;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectInfoCV --
///
/// Choice variant
/// provides read/write access to the variant data.

class NCBI_XSERIAL_EXPORT CObjectInfoCV : public CObjectTypeInfoCV
{
    typedef CObjectTypeInfoCV CParent;
public:
    CObjectInfoCV(void);
    CObjectInfoCV(const CObjectInfo& object);
    CObjectInfoCV(const CObjectInfo& object, TMemberIndex index);

    /// Get containing choice
    const CObjectInfo& GetChoiceObject(void) const;
    
    CObjectInfoCV& operator=(const CObjectInfo& object);
    
    /// Get variant data
    CObjectInfo GetVariant(void) const;
    /// Get variant data
    CObjectInfo operator*(void) const;

private:
    pair<TObjectPtr, TTypeInfo> GetVariantPair(void) const;

    CObjectInfo m_Object;
};


/* @} */


#include <serial/impl/objectiter.inl>

END_NCBI_SCOPE

#endif  /* OBJECTITER__HPP */
