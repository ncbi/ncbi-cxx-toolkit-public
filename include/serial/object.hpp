#ifndef OBJECT__HPP
#define OBJECT__HPP

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
* Revision 1.14  2000/09/26 17:38:07  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.13  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.12  2000/09/01 13:16:00  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/18 17:22:58  vasilche
* GetTypeInfo & constructor of CObjectTypeInfo made public.
*
* Revision 1.9  2000/07/06 16:21:15  vasilche
* Added interface to primitive types in CObjectInfo & CConstObjectInfo.
*
* Revision 1.8  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.7  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.6  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.5  2000/05/04 16:23:10  vasilche
* Updated CTypesIterator and CTypesConstInterator interface.
*
* Revision 1.4  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.3  2000/03/07 14:05:30  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.2  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.1  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/memberid.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CPrimitiveTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CPointerTypeInfo;

class CMemberId;
class CItemInfo;
class CMemberInfo;
class CVariantInfo;

class CReadContainerElementHook;

class CObjectTypeInfoMI;
class CObjectTypeInfoVI;
class CConstObjectInfoMI;
class CConstObjectInfoCV;
class CConstObjectInfoEI;
class CObjectInfoMI;
class CObjectInfoCV;
class CObjectInfoEI;

class CObjectGetTypeInfo
{
public:
    static TTypeInfo GetTypeInfo(void);
};

class CObjectTypeInfo
{
public:
    typedef CObjectTypeInfoMI CMemberIterator;
    typedef CObjectTypeInfoVI CVariantIterator;

    CObjectTypeInfo(TTypeInfo typeinfo = 0);

    ETypeFamily GetTypeFamily(void) const;

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;
    
    bool operator==(const CObjectTypeInfo& type) const;
    bool operator!=(const CObjectTypeInfo& type) const;

    // primitive type interface
    // only when GetTypeFamily() == CTypeInfo::eTypePrimitive
    EPrimitiveValueType GetPrimitiveValueType(void) const;
    bool IsPrimitiveValueSigned(void) const;

    // container interface
    // only when GetTypeFamily() == CTypeInfo::eTypeContainer
    CObjectTypeInfo GetElementType(void) const;
    EContainerType GetContainerType(void) const;

    // class interface
    // only when GetTypeFamily() == CTypeInfo::eTypeClass
    CMemberIterator BeginMembers(void) const;
    CMemberIterator FindMember(const string& memberName) const;
    CMemberIterator FindMemberByTag(int memberTag) const;

    // choice interface
    // only when GetTypeFamily() == CTypeInfo::eTypeChoice
    CVariantIterator BeginVariants(void) const;
    CVariantIterator FindVariant(const string& memberName) const;
    CVariantIterator FindVariantByTag(int memberTag) const;

    // pointer interface
    // only when GetTypeFamily() == CTypeInfo::eTypePointer
    CObjectTypeInfo GetPointedType(void) const;

public: // mostly for internal use
    TTypeInfo GetTypeInfo(void) const;
    const CPrimitiveTypeInfo* GetPrimitiveTypeInfo(void) const;
    const CClassTypeInfo* GetClassTypeInfo(void) const;
    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;
    const CPointerTypeInfo* GetPointerTypeInfo(void) const;

    TMemberIndex FindMemberIndex(const string& name) const;
    TMemberIndex FindMemberIndex(int tag) const;
    TMemberIndex FindVariantIndex(const string& name) const;
    TMemberIndex FindVariantIndex(int tag) const;

    CMemberIterator GetMemberIterator(TMemberIndex index) const;
    CVariantIterator GetVariantIterator(TMemberIndex index) const;

protected:
    void ResetTypeInfo(void);
    void SetTypeInfo(TTypeInfo typeinfo);

    void CheckTypeFamily(ETypeFamily family) const;
    void WrongTypeFamily(ETypeFamily needFamily) const;

private:
    TTypeInfo m_TypeInfo;
};

class CConstObjectInfo : public CObjectTypeInfo
{
public:
    typedef TConstObjectPtr TObjectPtrType;
    typedef CConstObjectInfoEI CElementIterator;
    typedef CConstObjectInfoMI CMemberIterator;
    typedef CConstObjectInfoCV CChoiceVariant;
    
    enum ENonCObject {
        eNonCObject
    };

    // create empty CObjectInfo
    CConstObjectInfo(void);
    // initialize CObjectInfo
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    // initialize CObjectInfo when we are sure that object 
    // is not inherited from CObject (for efficiency)
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
                     ENonCObject nonCObject);

    // reset CObjectInfo to empty state
    void Reset(void);
    // set CObjectInfo
    CConstObjectInfo& operator=(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);

    // check if CObjectInfo initialized with valid object
    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void ResetObjectPtr(void);

    // get pointer to object
    TConstObjectPtr GetObjectPtr(void) const;
    pair<TConstObjectPtr, TTypeInfo> GetPair(void) const;

    // primitive type interface
    bool GetPrimitiveValueBool(void) const;
    char GetPrimitiveValueChar(void) const;
    long GetPrimitiveValueLong(void) const;
    unsigned long GetPrimitiveValueULong(void) const;
    double GetPrimitiveValueDouble(void) const;
    void GetPrimitiveValueString(string& value) const;
    string GetPrimitiveValueString(void) const;
    void GetPrimitiveValueOctetString(vector<char>& value) const;

    // class interface
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    TMemberIndex GetCurrentChoiceVariantIndex(void) const;
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CConstObjectInfo GetPointedObject(void) const;
    
    // container interface
    CElementIterator BeginElements(void) const;

protected:
    void Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    
private:
    TConstObjectPtr m_ObjectPtr; // object pointer
    CConstRef<CObject> m_Ref; // hold reference to CObject for correct removal
};

class CObjectInfo : public CConstObjectInfo
{
    typedef CConstObjectInfo CParent;
public:
    typedef TObjectPtr TObjectPtrType;
    typedef CObjectInfoEI CElementIterator;
    typedef CObjectInfoMI CMemberIterator;
    typedef CObjectInfoCV CChoiceVariant;

    // create empty CObjectInfo
    CObjectInfo(void);
    // initialize CObjectInfo
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    CObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    // initialize CObjectInfo when we are sure that object 
    // is not inherited from CObject (for efficiency)
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
                ENonCObject nonCObject);
    // create CObjectInfo with new object
    explicit CObjectInfo(TTypeInfo typeInfo);

    // set CObjectInfo to point to another object
    CObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);
    
    // get pointer to object
    TObjectPtr GetObjectPtr(void) const;
    pair<TObjectPtr, TTypeInfo> GetPair(void) const;

    // read
    void Read(CObjectIStream& in);

    // primitive type interface
    void SetPrimitiveValueBool(bool value);
    void SetPrimitiveValueChar(char value);
    void SetPrimitiveValueLong(long value);
    void SetPrimitiveValueULong(unsigned long value);
    void SetPrimitiveValueDouble(double value);
    void SetPrimitiveValueString(const string& value);
    void SetPrimitiveValueOctetString(const vector<char>& value);

    // class interface
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CObjectInfo GetPointedObject(void) const;

    // container interface
    CElementIterator BeginElements(void) const;
    void ReadContainer(CObjectIStream& in, CReadContainerElementHook& hook);
};

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

    CObjectTypeInfo GetMemberType(void) const;
    CObjectTypeInfo operator*(void) const;

public: // mostly for internal use
    const CMemberInfo* GetMemberInfo(void) const;

protected:
    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);

    const CClassTypeInfo* GetClassTypeInfo(void) const;

    bool IsSet(const CConstObjectInfo& object) const;
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

public: // mostly for internal use
    const CVariantInfo* GetVariantInfo(void) const;

protected:
    void Init(const CObjectTypeInfo& info);
    void Init(const CObjectTypeInfo& info, TMemberIndex index);

    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;
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
    CObjectTypeInfo operator->(void) const;

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

#include <serial/object.inl>

// get starting point of object hierarchy
template<class C>
inline
TTypeInfo ObjectType(const C& /*obj*/)
{
    return C::GetTypeInfo();
}

template<class C>
inline
pair<TObjectPtr, TTypeInfo> ObjectInfo(C& obj)
{
    return pair<TObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

// get starting point of non-modifiable object hierarchy
template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ConstObjectInfo(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ObjectInfo(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

END_NCBI_SCOPE

#endif  /* OBJECT__HPP */
