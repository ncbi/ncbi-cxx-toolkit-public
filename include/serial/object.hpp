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
class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CPointerTypeInfo;
class CMemberId;
class CMemberInfo;
class CContainerElementReadHook;

class CObjectGetTypeInfo
{
public:
    static TTypeInfo GetTypeInfo(void);
};

class CObjectTypeInfo
{
public:
    CObjectTypeInfo(TTypeInfo typeinfo = 0)
        : m_TypeInfo(typeinfo)
        {
        }

    TTypeInfo GetTypeInfo(void) const
        {
            return m_TypeInfo;
        }
    CTypeInfo::ETypeFamily GetTypeFamily(void) const
        {
            return GetTypeInfo()->GetTypeFamily();
        }
    const CPrimitiveTypeInfo* GetPrimitiveTypeInfo(void) const;
    const CClassTypeInfoBase* GetClassTypeInfoBase(void) const;
    const CClassTypeInfo* GetClassTypeInfo(void) const;
    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;
    const CPointerTypeInfo* GetPointerTypeInfo(void) const;
    
    // primitive type interface
    CPrimitiveTypeInfo::EValueType GetPrimitiveValueType(void) const;
    bool IsPrimitiveValueSigned(void) const;
    // class/choice interface
    const CMemberInfo* GetMemberInfo(TMemberIndex index) const;
    TMemberIndex FindMember(const string& memberName) const; // -1 if not found
    TMemberIndex FindMemberByTag(int memberTag) const;       // -1 if not found
    const CMemberInfo* GetMemberInfo(const string& memberName) const;

    // class interface
    class CMemberIterator
    {
    public:
        CMemberIterator(void)
            : m_ClassTypeInfo(0),
              m_MemberIndex(kFirstMemberIndex),
              m_LastMemberIndex(kFirstMemberIndex - 1)
            {
                _DEBUG_ARG(m_LastCall = eNone);
            }
        CMemberIterator(const CObjectTypeInfo& info)
            {
                Init(info);
            }

        const CClassTypeInfo* GetClassTypeInfo(void) const
            {
                return m_ClassTypeInfo;
            }
        TMemberIndex GetMemberIndex(void) const
            {
                _ASSERT(CheckValid());
                return m_MemberIndex;
            }

        operator bool(void) const
            {
                _DEBUG_ARG(m_LastCall = eValid);
                return CheckValid();
            }

        void Next(void)
            {
                _ASSERT(CheckValid());
                _DEBUG_ARG(m_LastCall = eNext);
                ++m_MemberIndex;
            }

    protected:
        void Init(const CObjectTypeInfo& info);

    protected:
        bool IsSet(const CConstObjectInfo& object) const;

    private:
        const CClassTypeInfo* m_ClassTypeInfo;
        TMemberIndex m_MemberIndex;
        TMemberIndex m_LastMemberIndex;

    protected:
#if _DEBUG
        mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif

        bool CheckValid(void) const
            {
#if _DEBUG
                if ( m_LastCall != eValid)
                    ERR_POST("CMemberIterator was used without checking its validity");
#endif
                return m_MemberIndex >= kFirstMemberIndex &&
                    m_MemberIndex <= m_LastMemberIndex;
            }
    };

protected:
    void ResetTypeInfo(void)
        {
            m_TypeInfo = 0;
        }
    void SetTypeInfo(TTypeInfo typeinfo)
        {
            m_TypeInfo = typeinfo;
        }
    
private:
    TTypeInfo m_TypeInfo;
};

class CConstObjectMemberIterator;
class CConstObjectElementIterator;

class CConstObjectInfo : public CObjectTypeInfo
{
public:
    typedef TConstObjectPtr TObjectPtrType;
    typedef CConstObjectMemberIterator CMemberIterator;
    typedef CConstObjectElementIterator CElementIterator;
    
    enum ENonCObject {
        eNonCObject
    };

    // create empty CObjectInfo
    CConstObjectInfo(void);
    // initialize CObjectInfo
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
                     ENonCObject nonCObject);
    CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object);

    // reset CObjectInfo to empty state
    void Reset(void);
    // set CObjectInfo
    CConstObjectInfo& operator=(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);

    // check if CObjectInfo initialized with valid object
    operator bool(void) const
        {
            return m_ObjectPtr != 0;
        }
    void ResetObjectPtr(void)
        {
            m_ObjectPtr = 0;
        }

    // get pointer to object
    TConstObjectPtr GetObjectPtr(void) const
        {
            return m_ObjectPtr;
        }

    pair<TConstObjectPtr, TTypeInfo> GetPair(void) const
        {
            return make_pair(GetObjectPtr(), GetTypeInfo());
        }

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
    bool ClassMemberIsSet(TMemberIndex index) const;
    CConstObjectInfo GetClassMember(TMemberIndex index) const;
    // choice interface
    TMemberIndex WhichChoice(void) const;
    CConstObjectInfo GetCurrentChoiceVariant(void) const;
    // pointer interface
    CConstObjectInfo GetPointedObject(void) const;
    
protected:
    void Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    
private:
    TConstObjectPtr m_ObjectPtr; // object pointer
    CConstRef<CObject> m_Ref; // hold reference to CObject for correct removal
};

class CObjectMemberIterator;
class CObjectElementIterator;

class CObjectInfo : public CConstObjectInfo {
public:
    typedef TObjectPtr TObjectPtrType;
    typedef CObjectMemberIterator CMemberIterator;
    typedef CObjectElementIterator CElementIterator;

    // create empty CObjectInfo
    CObjectInfo(void);
    // create CObjectInfo with new object
    CObjectInfo(TTypeInfo typeInfo);
    // initialize CObjectInfo
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
                ENonCObject nonCObject);
    CObjectInfo(pair<TObjectPtr, TTypeInfo> object);


    // set CObjectInfo
    CObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);
    
    // get pointer to object
    TObjectPtr GetObjectPtr(void) const
        {
            return const_cast<TObjectPtr>(CConstObjectInfo::GetObjectPtr());
        }

    pair<TObjectPtr, TTypeInfo> GetPair(void) const
        {
            return make_pair(GetObjectPtr(), GetTypeInfo());
        }

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
    CObjectInfo GetClassMember(TMemberIndex index) const;
    void EraseClassMember(TMemberIndex index);
    // choice interface
    CObjectInfo GetCurrentChoiceVariant(void) const;
    // pointer interface
    CObjectInfo GetPointedObject(void) const;
    // container interface
    void ReadContainer(CObjectIStream& in, CContainerElementReadHook& hook);
};

class CConstObjectMemberIterator : public CObjectTypeInfo::CMemberIterator
{
    typedef CObjectTypeInfo::CMemberIterator CParent;
public:
    CConstObjectMemberIterator(void)
        {
        }
    CConstObjectMemberIterator(const CConstObjectInfo& object)
        : CParent(object), m_Object(object)
        {
            _ASSERT(object);
        }
    
    const CConstObjectInfo& GetClassObject(void) const
        {
            return m_Object;
        }
    
    CConstObjectMemberIterator& operator=(const CConstObjectInfo& object)
        {
            _ASSERT(object);
            CParent::Init(object);
            m_Object = object;
            return *this;
        }
    
    bool IsSet(void) const
        {
            return GetClassObject().ClassMemberIsSet(GetMemberIndex());
        }
    
    CConstObjectInfo operator*(void) const
        {
            return GetClassObject().GetClassMember(GetMemberIndex());
        }

private:
    CConstObjectInfo m_Object;
};

class CObjectMemberIterator : public CObjectTypeInfo::CMemberIterator
{
    typedef CObjectTypeInfo::CMemberIterator CParent;
public:
    CObjectMemberIterator(void)
        {
        }
    CObjectMemberIterator(const CObjectInfo& object)
        : CParent(object), m_Object(object)
        {
            _ASSERT(object);
        }

    const CObjectInfo& GetClassObject(void) const
        {
            return m_Object;
        }

    CObjectMemberIterator& operator=(const CObjectInfo& object)
        {
            _ASSERT(object);
            CParent::Init(object);
            m_Object = object;
            return *this;
        }

    bool IsSet(void) const
        {
            return m_Object.ClassMemberIsSet(GetMemberIndex());
        }

    CObjectInfo operator*(void) const
        {
            return m_Object.GetClassMember(GetMemberIndex());
        }
    
    void Erase(void)
        {
            m_Object.EraseClassMember(GetMemberIndex());
            Next();
        }
    
private:
    CObjectInfo m_Object;
};

class CConstObjectElementIterator
{
public:
    CConstObjectElementIterator(void)
        {
            _DEBUG_ARG(m_LastCall = eNone);
        }
    CConstObjectElementIterator(const CConstObjectElementIterator& iterator)
        : m_Iterator(iterator.CloneIterator())
        {
            _DEBUG_ARG(m_LastCall = iterator.m_LastCall);
        }
    CConstObjectElementIterator(const CConstObjectInfo& object);

    CConstObjectElementIterator& operator=(const CConstObjectElementIterator& iterator)
        {
            m_Iterator.reset(iterator.CloneIterator());
            _DEBUG_ARG(m_LastCall = iterator.m_LastCall);
            return *this;
        }
    CConstObjectElementIterator& operator=(const CConstObjectInfo& object);

    operator bool(void) const
        {
            _DEBUG_ARG(m_LastCall = eValid);
            return CheckValid();
        }

    void Next(void)
        {
            _ASSERT(CheckValid());
            m_Iterator->Next();
        }

    CConstObjectInfo operator*(void) const
        {
            _ASSERT(CheckValid());
            return m_Iterator->Get();
        }

private:
    CConstContainerElementIterator* CloneIterator(void) const
        {
            const CConstContainerElementIterator* i = m_Iterator.get();
            if ( i )
                return i->Clone();
            else
                return 0;
        }
    bool CheckValid(void) const
        {
#if _DEBUG
            if ( m_LastCall != eValid)
                ERR_POST("CElementIterator was used without checking its validity");
#endif
            const CConstContainerElementIterator* i = m_Iterator.get();
            return i && i->Valid();
        }

    auto_ptr<CConstContainerElementIterator> m_Iterator;
#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
};

class CObjectElementIterator
{
public:
    CObjectElementIterator(void)
        {
            _DEBUG_ARG(m_LastCall = eNone);
        }
    CObjectElementIterator(const CObjectElementIterator& iterator)
        : m_Iterator(iterator.CloneIterator())
        {
            _DEBUG_ARG(m_LastCall = iterator.m_LastCall);
        }
    CObjectElementIterator(const CObjectInfo& object);

    CObjectElementIterator& operator=(const CObjectElementIterator& iterator)
        {
            m_Iterator.reset(iterator.CloneIterator());
            _DEBUG_ARG(m_LastCall = iterator.m_LastCall);
            return *this;
        }
    CObjectElementIterator& operator=(const CObjectInfo& object);

    operator bool(void) const
        {
            _DEBUG_ARG(m_LastCall = eValid);
            return CheckValid();
        }

    void Next(void)
        {
            _ASSERT(CheckValid());
            _DEBUG_ARG(m_LastCall = eNext);
            m_Iterator->Next();
        }

    CObjectInfo operator*(void) const
        {
            _ASSERT(CheckValid());
            return m_Iterator->Get();
        }

    void Erase(void)
        {
            _ASSERT(CheckValid());
            _DEBUG_ARG(m_LastCall = eErase);
            m_Iterator->Erase();
        }

private:
    CContainerElementIterator* CloneIterator(void) const
        {
            const CContainerElementIterator* i = m_Iterator.get();
            if ( i )
                return i->Clone();
            else
                return 0;
        }
    bool CheckValid(void) const
        {
#if _DEBUG
            if ( m_LastCall != eValid)
                ERR_POST("CElementIterator was used without checking its validity");
#endif
            const CContainerElementIterator* i = m_Iterator.get();
            return i && i->Valid();
        }

    auto_ptr<CContainerElementIterator> m_Iterator;
#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
};

#include <serial/object.inl>

// get starting point of object hierarchy
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
